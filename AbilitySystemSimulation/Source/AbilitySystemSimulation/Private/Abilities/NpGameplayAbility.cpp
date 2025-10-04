// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/NpGameplayAbility.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "GameplayCue_Types.h"
#include "Tasks/BasePredictionTask.h"



UNpGameplayAbility::UNpGameplayAbility(const FObjectInitializer& ObjectInitializer)
  : Super(ObjectInitializer)
{
	ReplicationPolicy = EGameplayAbilityReplicationPolicy::Type::ReplicateNo;
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName OnAbilityRestoredFuncName = FName(TEXT("K2_OnAbilityRestored"));
	UFunction* OnAbilityRestoredFunction = GetClass()->FindFunctionByName(OnAbilityRestoredFuncName);
	bHasBlueprintOnAbilityRestored = IsImplementedInBlueprint(OnAbilityRestoredFunction);
}

#pragma region Synced Variables Functionality

void UNpGameplayAbility::GetSyncedVars(TArray<FSyncVarDef>& OutSyncedVars) const
{
	for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;

		if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			if (StructProp->Struct->IsChildOf(FBaseSyncVar::StaticStruct()))
			{
				FSyncVarDef NewConfig;
				NewConfig.MemberSyncVariable = StructProp;
				
				FString PreRollbackFuncName = TEXT("PreRollback") + Property->GetName();
				UFunction* PreRollbackFunc = GetClass()->FindFunctionByName(*PreRollbackFuncName);
				if (IsValid(PreRollbackFunc))
				{
					if (ValidatePreRollbackFunction(PreRollbackFunc, StructProp))
					{
						NewConfig.SyncVarPreRollbackFunction = PreRollbackFunc;
					}
				}
				OutSyncedVars.Add(NewConfig);
			}
		}
	}
}

bool UNpGameplayAbility::ValidatePreRollbackFunction(UFunction* Function, FStructProperty* SyncProperty) const
{
	if (!Function || !SyncProperty)
		return false;
    
	// Check if the function has params at all
	if (Function->ParmsSize == 0)
		return false;
        
	// Count input parameters (non-return values)
	int32 InputParamCount = 0;
	FStructProperty* MatchingProperty = nullptr;
    
	// Iterate through function properties
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Property = *It;
        
		// Only consider parameters (excluding local variables)
		if (Property->HasAnyPropertyFlags(CPF_Parm))
		{
			// Skip return value
			if (Property->HasAnyPropertyFlags(CPF_ReturnParm))
				continue;
                
			// Count this as an input parameter
			InputParamCount++;
            
			// Check if this property is a struct property
			FStructProperty* FuncStructProperty = CastField<FStructProperty>(Property);
			if (FuncStructProperty)
			{
				// Store this for type comparison
				MatchingProperty = FuncStructProperty;
			}
		}
	}
    
	// Check if there's exactly one input parameter
	if (InputParamCount != 1 || !MatchingProperty)
		return false;
        
	// Compare the struct types
	return MatchingProperty->Struct == SyncProperty->Struct;
}

void UNpGameplayAbility::RestoreSyncedVariables(const FSyncVarCollection& SyncedVars)
{
	TArray<FSyncVarDef> AbilitySyncedVars;
	GetSyncedVars(AbilitySyncedVars);
	if(SyncedVars.SyncedVars.Num() != AbilitySyncedVars.Num())
	{
		return;
	}
        
	for (int32 i = 0; i < AbilitySyncedVars.Num(); ++i)
	{
		if (const FProperty* Property = AbilitySyncedVars[i].MemberSyncVariable)
		{
			FBaseSyncVar* TargetVar = static_cast<FBaseSyncVar*>(Property->ContainerPtrToValuePtr<void>(this));
			
			if (SyncedVars.SyncedVars[i].IsValid() && TargetVar)
			{
				UFunction* Function = AbilitySyncedVars[i].SyncVarPreRollbackFunction;
				if (IsValid(Function))
				{
					ProcessEvent(Function, SyncedVars.SyncedVars[i].Get());
				}
				TargetVar->SetValue(SyncedVars.SyncedVars[i].Get());
			}
		}
	}
}

#pragma endregion


bool UNpGameplayAbility::GetActivatedByTrigger(FAbilityActivationTrigger& OutTrigger)
{
	if (ActivatedByInputActionIndex == INDEX_NONE)
	{
		return false;
	}
	if (!ActivationInputs.IsValidIndex(ActivatedByInputActionIndex))
	{
		return false;
	}
	OutTrigger = ActivationInputs[ActivatedByInputActionIndex];
	return true;
}

bool UNpGameplayAbility::CancelPredictionTaskByName(FName TaskName)
{
	UBasePredictionTask* Task = GetPredictionTaskByName(TaskName);
	if (!Task)
	{
		return false;
	}
	Task->CancelTask();
	return true;
}

UNpAbilitySystemComponent* UNpGameplayAbility::GetNpAbilitySystemComponent() const
{
	if (GetActorInfo().AbilitySystemComponent.IsValid())
	{
		return Cast<UNpAbilitySystemComponent>(GetActorInfo().AbilitySystemComponent.Get());
	}
	return nullptr;
}

void UNpGameplayAbility::RestoreFrame(const FActiveAbilityInstanceData& AuthoritySyncData)
{
	StartAbilityRollback(AuthoritySyncData);
	const bool bWasActive = bIsActive;
	bIsActive = AuthoritySyncData.bIsActive;
	bIsCancelable = AuthoritySyncData.bIsCancelable;
	bIsBlockingOtherAbilities = AuthoritySyncData.bIsBlockingOtherAbilities;
	ActivatedByInputActionIndex = AuthoritySyncData.ActivatedByInput;
	TrackedGameplayCues = AuthoritySyncData.TrackedGameplayCues;
	bHasEventData = AuthoritySyncData.bHasEventData;
	CurrentEventData = AuthoritySyncData.CurrentEventData.GameplayEventData;
	if (bIsActive)
	{
		// if we were force activated from a correction, trigger this event.
		if (!bWasActive)
		{
			FGameplayEventData* EventData = bHasEventData ? &CurrentEventData : nullptr;
			ActivateAbility(CurrentSpecHandle,GetCurrentActorInfo(),GetCurrentActivationInfo(),EventData);
		}
		// restore synced vars and task, we have to restore them before Deactivation and after Activation to ensure that user code
		// in Activate Ability events doesn't change the restored data. and the synced vars used in EndAbility are correct before using them
		// setting synced vars in EndAbility is a bit hazardous. since server can call end ability with specific value.
		// change that value in the event and send it to client. if client didn't end the ability yet, he will get the sync var value from server set
		// DURING End Ability. so client uses value after server set it.
		// if you want to "reset" sync vars, do it in activate ability not in end, otherwise don't change them.
		RestoreSyncedVariables(AuthoritySyncData.AbilitySyncedVars);
		for (int32 i = 0 ; i < PredictionTasksInstances.Num(); ++i)
		{
			UBasePredictionTask* Task = PredictionTasksInstances[i];
			if (!AuthoritySyncData.TaskDataCollection.AbilityTasksData.IsValidIndex(i))
			{
				continue;
			}
			const FAbilityTaskDataContainer& TaskData = AuthoritySyncData.TaskDataCollection.AbilityTasksData[i];
			Task->RestoreFrame(TaskData);
		}
	}
	// if Ability is not supposed to be active make sure all tasks are deactivated (which calls OnTaskDeactivated() that tasks use for clean up)
	else
	{
		
		RestoreSyncedVariables(AuthoritySyncData.AbilitySyncedVars);
		for (int32 i = 0 ; i < PredictionTasksInstances.Num(); ++i)
		{
			UBasePredictionTask* Task = PredictionTasksInstances[i];
			if (!AuthoritySyncData.TaskDataCollection.AbilityTasksData.IsValidIndex(i))
			{
				continue;
			}
			const FAbilityTaskDataContainer& TaskData = AuthoritySyncData.TaskDataCollection.AbilityTasksData[i];
			Task->RestoreFrame(TaskData);
		}
		//ToDo @Kai : This needs further work, there should be another event in the abilities for force De/activation from rollback
		// so abilities can do any needed setup or clean up for now we just call normal Activate and End Ability.
		if (bWasActive)
		{
			ForceEndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true);
		}
	}

	if (bHasBlueprintOnAbilityRestored)
	{
		K2_OnAbilityRestored();
	}
	else
	{
		OnAbilityRestored();
	}
	
}

void UNpGameplayAbility::SimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	for (UBasePredictionTask* Task : PredictionTasksInstances)
	{
		if (Task->IsActive() && Task->ShouldTaskTick)
		{
			Task->SimulationTick(TimeStep);
		}
	}
	OnSimulationTick(TimeStep);
}

void UNpGameplayAbility::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
}


void UNpGameplayAbility::OnAbilityRestored()
{
}

void UNpGameplayAbility::StartAbilityRollback(const FActiveAbilityInstanceData& AuthorityTaskData)
{
}

#pragma region UGameplay Ability Overrides
bool UNpGameplayAbility::ShouldActivateAbility(ENetRole Role) const
{
	return Role != ROLE_SimulatedProxy;
}

void UNpGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	if (IsRestoringFrame())
	{
		return;
	}
	if (CanBeCanceled())
	{
		if (ScopeLockCount > 0)
		{
			UE_LOG(LogAbilitySystem, Verbose, TEXT("Attempting to cancel Ability %s but ScopeLockCount was greater than 0, adding cancel to the WaitingToExecute Array"), *GetName());
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UGameplayAbility::CancelAbility, Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility));
			return;
		}

		// Gives the Ability BP a chance to perform custom logic/cleanup when any active ability states are active
		if (OnGameplayAbilityCancelled.IsBound())
		{
			OnGameplayAbilityCancelled.Broadcast();
		}

		// End the ability but don't replicate it, we replicate the CancelAbility call directly
		bool bReplicateEndAbility = false;
		bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
}

void UNpGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	
	if (IsEndAbilityValid(Handle, ActorInfo))
	{
		if (ScopeLockCount > 0)
		{
			UE_LOG(LogAbilitySystem, Verbose, TEXT("Attempting to end Ability %s but ScopeLockCount was greater than 0, adding end to the WaitingToExecute Array"), *GetName());
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UNpGameplayAbility::EndAbility, Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled));
			return;
		}
        
        if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
        {
            bIsAbilityEnding = true;
        	// Tell all our tasks that we are finished and they should cleanup before blueprint end ability is called
        	for (int32 TaskIdx = PredictionTasksInstances.Num() - 1; TaskIdx >= 0 && PredictionTasksInstances.Num() > 0; --TaskIdx)
        	{
        		UBasePredictionTask* Task = PredictionTasksInstances[TaskIdx];
        		if (Task && Task->IsActive())
        		{
        			Task->OnOwningAbilityEnded(bWasCancelled);
        			Task->DeactivateTask(bWasCancelled);
        		}
        	}
        }

		// Still should clean original (not predictive) ability system tasks!
		for (int32 TaskIdx = ActiveTasks.Num() - 1; TaskIdx >= 0 && ActiveTasks.Num() > 0; --TaskIdx)
		{
			if (UGameplayTask* Task = ActiveTasks[TaskIdx])
			{
				Task->TaskOwnerEnded();
			}
		}
		ActiveTasks.Reset();

		// Give blueprint a chance to react
		K2_OnEndAbility(bWasCancelled);

		// Protect against blueprint causing us to EndAbility already
		if (bIsActive == false && GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			return;
		}

		// Stop any timers or latent actions for the ability
		UWorld* MyWorld = GetWorld();
		if (MyWorld)
		{
			MyWorld->GetLatentActionManager().RemoveActionsForObject(this);
			MyWorld->GetTimerManager().ClearAllTimersForObject(this);
		}

		// Execute our delegate and unbind it, as we are no longer active and listeners can re-register when we become active again.
		OnGameplayAbilityEnded.Broadcast(this);
		OnGameplayAbilityEnded.Clear();

		OnGameplayAbilityEndedWithData.Broadcast(FAbilityEndedData(this, Handle, bReplicateEndAbility, bWasCancelled));
		OnGameplayAbilityEndedWithData.Clear();

		if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
		{
			bIsActive = false;
			bIsAbilityEnding = false;
		}
		if (UAbilitySystemComponent* const AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get())
		{
			// Remove tracked GameplayCues that we added
			for (FGameplayTag& GameplayCueTag : TrackedGameplayCues)
			{
				AbilitySystemComponent->RemoveGameplayCue(GameplayCueTag);
			}
			TrackedGameplayCues.Empty();
			// Remove tags we added to owner , use non replicated tags if activation is non replicated
			if (GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
			|| GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
			{
				AbilitySystemComponent->RemoveNonReplicatedLooseTags(ActivationOwnedTags);
			}
			else
			{
				
				AbilitySystemComponent->UpdateTagMap(ActivationOwnedTags,-1);
			}
			
			if (IsBlockingOtherAbilities())
			{
				// If we're still blocking other abilities, cancel now
				AbilitySystemComponent->ApplyAbilityBlockAndCancelTags(AbilityTags, this, false, BlockAbilitiesWithTag, false, CancelAbilitiesWithTag);
			}
			
			if (CanBeCanceled())
			{
				// If we're still cancelable, cancel it now
				AbilitySystemComponent->HandleChangeAbilityCanBeCanceled(AbilityTags, this, false);
			}

			AbilitySystemComponent->ClearAbilityReplicatedDataCache(Handle, CurrentActivationInfo);

			// Tell owning AbilitySystemComponent that we ended so it can do stuff (including MarkPendingKill us)
			AbilitySystemComponent->NotifyAbilityEnded(Handle, this, bWasCancelled);
		}

		if (IsInstantiated())
		{
			CurrentEventData = FGameplayEventData{};
			bHasEventData = false;
		}
	}
}

void UNpGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                      FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	UAbilitySystemComponent* Comp = ActorInfo->AbilitySystemComponent.Get();
	
	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		bIsActive = true;
		bIsBlockingOtherAbilities = true;
		bIsCancelable = true;
	}

	RemoteInstanceEnded = false;

	// This must be called before we start applying tags and blocking or canceling other abilities.
	// We could set off a chain that results in calling functions on this ability that rely on the current info being set.
	SetCurrentInfo(Handle, ActorInfo, ActivationInfo);
	
	
	if (TriggerEventData && IsInstantiated())
	{
		CurrentEventData = *TriggerEventData;
		bHasEventData = true;
	}

	Comp->HandleChangeAbilityCanBeCanceled(AbilityTags, this, true);
	
	// use non replicated tags if activation is non replicated
	if (GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		Comp->AddNonReplicatedLooseTags(ActivationOwnedTags);
	}
	else
	{
		Comp->UpdateTagMap(ActivationOwnedTags,1);
	}
	
	
	if (OnGameplayAbilityEndedDelegate)
	{
		OnGameplayAbilityEnded.Add(*OnGameplayAbilityEndedDelegate);
	}

	Comp->NotifyAbilityActivated(Handle, this);

	Comp->ApplyAbilityBlockAndCancelTags(AbilityTags, this, true, BlockAbilitiesWithTag, true, CancelAbilitiesWithTag);

	// Spec's active count must be incremented after applying blockor cancel tags, otherwise the ability runs the risk of cancelling itself inadvertantly before it completely activates.
	FGameplayAbilitySpec* Spec = Comp->FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("PreActivate called with a valid handle but no matching ability spec was found. Handle: %s ASC: %s. AvatarActor: %s"), *Handle.ToString(), *(Comp->GetPathName()), *GetNameSafe(Comp->GetAvatarActor_Direct()));
		return;
	}

	// make sure we do not incur a roll over if we go over the uint8 max, this will need to be updated if the var size changes
	if (LIKELY(Spec->ActiveCount < UINT8_MAX))
	{
		Spec->ActiveCount++;
	}
	else
	{
		ABILITY_LOG(Warning, TEXT("PreActivate %s called when the Spec->ActiveCount (%d) >= UINT8_MAX"), *GetName(), (int32)Spec->ActiveCount)
	}
}

void UNpGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (TriggerEventData && bHasBlueprintActivateFromEvent)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbilityFromEvent(*TriggerEventData);
	}
	else if (bHasBlueprintActivate)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbility();
	}
	else if (bHasBlueprintActivateFromEvent)
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("Ability %s expects event data but none is being supplied. Use 'Activate Ability' instead of 'Activate Ability From Event' in the Blueprint."), *GetName());
		constexpr bool bReplicateEndAbility = false;
		constexpr bool bWasCancelled = true;
		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}
	else
	{
		// Native child classes should override ActivateAbility and call CommitAbility.
		// CommitAbility is used to do one last check for spending resources.
		// Previous versions of this function called CommitAbility but that prevents the callers
		// from knowing the result. Your override should call it and check the result.
		// Here is some starter code:
		
		//	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
		//	{			
		//		constexpr bool bReplicateEndAbility = true;
		//		constexpr bool bWasCancelled = true;
		//		EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		//	}
	}
}

FActiveGameplayEffectHandle UNpGameplayAbility::ApplyGameplayEffectToOwner(const FGameplayAbilitySpecHandle Handle,
                                                                           const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
                                                                           const UGameplayEffect* GameplayEffect, float GameplayEffectLevel, int32 Stacks) const
{
	if (IsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
	if (GameplayEffect)
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffect->GetClass(), GameplayEffectLevel);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetStackCount(Stacks);
			return ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
		}
	}

	// We cannot apply GameplayEffects in this context. Return an empty handle.
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle UNpGameplayAbility::ApplyGameplayEffectSpecToOwner(
	const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle) const
{
	if (IsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (SpecHandle.IsValid())
	{
		UAbilitySystemComponent* const AbilitySystemComponent = ActorInfo->AbilitySystemComponent.Get();
		return AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get(), AbilitySystemComponent->GetPredictionKeyForNewAction());

	}
	return FActiveGameplayEffectHandle();
}

TArray<FActiveGameplayEffectHandle> UNpGameplayAbility::ApplyGameplayEffectToTarget(
	const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayAbilityTargetDataHandle& Target,
	TSubclassOf<UGameplayEffect> GameplayEffectClass, float GameplayEffectLevel, int32 Stacks) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	if (IsRestoringFrame())
	{
		return EffectHandles;
	}
	//SCOPE_CYCLE_COUNTER(STAT_ApplyGameplayEffectToTarget);
	SCOPE_CYCLE_UOBJECT(This, this);
	SCOPE_CYCLE_UOBJECT(Effect, GameplayEffectClass);
	
	// This batches all created cues together
	FScopedGameplayCueSendContext GameplayCueSendContext;

	if (GameplayEffectClass == nullptr)
	{
		ABILITY_LOG(Error, TEXT("ApplyGameplayEffectToTarget called on ability %s with no GameplayEffect."), *GetName());
	}
	else 
	{
		FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(Handle, ActorInfo, ActivationInfo, GameplayEffectClass, GameplayEffectLevel);
		if (SpecHandle.Data.IsValid())
		{
			SpecHandle.Data->SetStackCount(Stacks);

			SCOPE_CYCLE_UOBJECT(Source, SpecHandle.Data->GetContext().GetSourceObject());
			EffectHandles.Append(ApplyGameplayEffectSpecToTarget(Handle, ActorInfo, ActivationInfo, SpecHandle, Target));
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("UGameplayAbility::ApplyGameplayEffectToTarget failed to create valid spec handle. Ability: %s"), *GetPathName());
		}
	}

	return EffectHandles;
}

TArray<FActiveGameplayEffectHandle> UNpGameplayAbility::ApplyGameplayEffectSpecToTarget(
	const FGameplayAbilitySpecHandle AbilityHandle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEffectSpecHandle SpecHandle,
	const FGameplayAbilityTargetDataHandle& TargetData) const
{
	TArray<FActiveGameplayEffectHandle> EffectHandles;
	
	if (SpecHandle.IsValid())
	{
		TARGETLIST_SCOPE_LOCK(*ActorInfo->AbilitySystemComponent);
		for (TSharedPtr<FGameplayAbilityTargetData> Data : TargetData.Data)
		{
			if (Data.IsValid())
			{
				EffectHandles.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get(), ActorInfo->AbilitySystemComponent->GetPredictionKeyForNewAction()));
			}
			else
			{
				ABILITY_LOG(Warning, TEXT("UGameplayAbility::ApplyGameplayEffectSpecToTarget invalid target data passed in. Ability: %s"), *GetPathName());
			}
		}
	}
	return EffectHandles;
}

void UNpGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithAssetTags(FGameplayTagContainer WithAssetTags,
	int32 StacksToRemove)
{
	if (IsRestoringFrame())
	{
		return;
	}
	UAbilitySystemComponent* const AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo_Ensured();
	if (AbilitySystemComponent)
	{
		FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(WithAssetTags);
		AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
	}
}

void UNpGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithGrantedTags(FGameplayTagContainer WithGrantedTags,
	int32 StacksToRemove)
{
	if (IsRestoringFrame())
	{
		return;
	}
	UAbilitySystemComponent* const AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo_Ensured();
	if (AbilitySystemComponent)
	{
		FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(WithGrantedTags);
		AbilitySystemComponent->RemoveActiveEffects(Query, StacksToRemove);
	}
}

void UNpGameplayAbility::BP_RemoveGameplayEffectFromOwnerWithHandle(FActiveGameplayEffectHandle Handle,
	int32 StacksToRemove)
{
	if (IsRestoringFrame())
	{
		return;
	}
	UAbilitySystemComponent* const AbilitySystemComponent = GetAbilitySystemComponentFromActorInfo_Ensured();
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
}

bool UNpGameplayAbility::IsRestoringFrame() const
{
	if (!CurrentActorInfo)
	{
		return false;
	}
	UNpAbilitySystemComponent* NpASC = Cast<UNpAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	bool RestoringFrame = false;
	if (NpASC)
	{
		RestoringFrame = NpASC->GetIsRestoringFrame();
	}
	return RestoringFrame;
}

void UNpGameplayAbility::PreForceActivateAbilityInstance(const FGameplayAbilitySpecHandle& Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo,const FActiveAbilityInstanceData& AuthoritySyncData)
{
	SetCurrentInfo(Handle, ActorInfo, ActivationInfo);
}

void UNpGameplayAbility::ForceCancelAbilityInstance(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	if (CanBeCanceled())
	{
		if (ScopeLockCount > 0)
		{
			UE_LOG(LogAbilitySystem, Verbose, TEXT("Attempting to cancel Ability %s but ScopeLockCount was greater than 0, adding cancel to the WaitingToExecute Array"), *GetName());
			WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UNpGameplayAbility::ForceCancelAbilityInstance, Handle, ActorInfo, ActivationInfo));
			return;
		}
		bool bWasCancelled = true;
		ForceEndAbility(Handle, ActorInfo, ActivationInfo, bWasCancelled);
	}
}

void UNpGameplayAbility::ForceEndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bWasCancelled)
{
	if (ScopeLockCount > 0)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("Attempting to end Ability %s but ScopeLockCount was greater than 0, adding end to the WaitingToExecute Array"), *GetName());
		WaitingToExecute.Add(FPostLockDelegate::CreateUObject(this, &UNpGameplayAbility::ForceEndAbility, Handle, ActorInfo, ActivationInfo, bWasCancelled));
		return;
	}
    
    if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
    {
        bIsAbilityEnding = true;
    }

	// Give blueprint a chance to react
	K2_OnEndAbility(bWasCancelled);

	// Execute our delegate and unbind it, as we are no longer active and listeners can re-register when we become active again.
	OnGameplayAbilityEnded.Broadcast(this);
	OnGameplayAbilityEnded.Clear();

	OnGameplayAbilityEndedWithData.Broadcast(FAbilityEndedData(this, Handle, false, bWasCancelled));
	OnGameplayAbilityEndedWithData.Clear();

	if (GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		bIsActive = false;
		bIsAbilityEnding = false;
	}

	// Tell all our tasks that we are finished and they should cleanup
	for (int32 TaskIdx = PredictionTasksInstances.Num() - 1; TaskIdx >= 0 && PredictionTasksInstances.Num() > 0; --TaskIdx)
	{
		UBasePredictionTask* Task = PredictionTasksInstances[TaskIdx];
		if (Task && Task->IsActive())
		{
			Task->OnOwningAbilityEnded(bWasCancelled);
			Task->DeactivateTask(bWasCancelled);
		}
	}

	// Should Still Clear the normal ability task in case they get used. (they will break the prediction flow and cause corrections)
	for (int32 TaskIdx = ActiveTasks.Num() - 1; TaskIdx >= 0 && ActiveTasks.Num() > 0; --TaskIdx)
	{
		if (UGameplayTask* Task = ActiveTasks[TaskIdx])
		{
			Task->TaskOwnerEnded();
		}
	}
	ActiveTasks.Reset();

	if (UNpAbilitySystemComponent* const AbilitySystemComponent = Cast<UNpAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
	{
		// Tell owning AbilitySystemComponent that we ended so it can do stuff (including MarkPendingKill us)
		AbilitySystemComponent->NotifyAbilityForceEnded(Handle, this, bWasCancelled);
	}

	if (IsInstantiated())
	{
		CurrentEventData = FGameplayEventData{};
		bHasEventData = false;
	}
}

#pragma endregion

#pragma region Rewindable Tasks

UBasePredictionTask* UNpGameplayAbility::GetPredictionTaskByName(FName TaskName)
{
	for (UBasePredictionTask* Task : PredictionTasksInstances)
	{
		if (Task->TaskName == TaskName)
		{
			return Task;
		}
	}
	return nullptr;
}

TArray<UBasePredictionTask*> UNpGameplayAbility::GetPredictionTasks() const
{
	return PredictionTasksInstances;
}


UBasePredictionTask* UNpGameplayAbility::GetTaskFromGuid(FGuid Guid)
{
	for (UBasePredictionTask* Task : PredictionTasksInstances)
	{
		if (Task->TaskId == Guid)
		{
			return Task;
		}
	}
	return nullptr;
}

TArray<FName> UNpGameplayAbility::GetTasksNames() const
{
	TArray<FName> OutTasksNames;
	TArray<UBasePredictionTask*> Tasks = GetClass()->GetDefaultObject<UNpGameplayAbility>()->GetPredictionTasks();
	for (UBasePredictionTask* Task : Tasks)
	{
		if (Task)
		{
			FName TaskName = Task->TaskName;
			if (TaskName.IsNone())
			{
				TaskName = Task->GetClass()->GetFName();
			}
			OutTasksNames.Add(Task->TaskName);
		}
		
	}
	return OutTasksNames;
}

FGuid UNpGameplayAbility::AddNativePredictionTask(const TSubclassOf<UBasePredictionTask> TaskClass,
	const FObjectInitializer& ObjectInitializer)
{
	UBasePredictionTask* NewTask = Cast<UBasePredictionTask>(ObjectInitializer.CreateDefaultSubobject(this,NAME_None,TaskClass,TaskClass));
	if (NewTask)
	{
		NewTask->TaskId = FGuid::NewGuid();
		return NewTask->TaskId;
	}
	return FGuid();
}

void UNpGameplayAbility::InstantiateNetPredictionTasks()
{
	for (UBasePredictionTask* Task : PredictionTasksInstances)
	{
		ensureMsgf(Task,TEXT("All Prediction Task Instances Must Be Valid during Instantiation"));
		Task->OwningAbility = this;
		// Find and call the binding function
		if (UFunction* BindingFunction = FindFunction(Task->GetInitializationFunctionName(Task->TaskId)))
		{
			ProcessEvent(BindingFunction, nullptr);
		}
	}
}

#if WITH_EDITOR
TArray<FName> UNpGameplayAbility::GetValidTaskNames()
{
	TArray<FName> Keys;
	for (auto Task : PredictionTasksInstances)
	{
		if (Task && !Task->TaskName.IsNone())
		{
			Keys.Add(Task->TaskName);
		}
	}
	return Keys;
}
#endif
#pragma endregion 

