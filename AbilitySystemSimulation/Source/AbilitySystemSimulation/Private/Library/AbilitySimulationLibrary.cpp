// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/AbilitySimulationLibrary.h"

#include "AbilitySystemComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Tasks/BasePredictionTask.h"



void UAbilitySimulationLibrary::AddMappingContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem
	,UNpAbilitySystemComponent* AbilitySimulationComponent,const UInputMappingContext* MappingContext
	, int32 Priority,FModifyContextOptions Options)
{
	if (!InputSubsystem)
	{
		UE_LOG(LogAbilitySystemComponent, Error, TEXT("InputSubsystem is null, Context will not be added"));
		return;
	}
	InputSubsystem->AddMappingContext(MappingContext,Priority,Options);
	if (AbilitySimulationComponent)
	{
		AbilitySimulationComponent->AddMappingContext(MappingContext);
	}
}

void UAbilitySimulationLibrary::RemoveMappingContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem,
	UNpAbilitySystemComponent* AbilitySimulationComponent, const UInputMappingContext* MappingContext,
	FModifyContextOptions Options)
{
	if (!InputSubsystem)
	{
		UE_LOG(LogAbilitySystemComponent, Error, TEXT("InputSubsystem is null, Context will not be added"));
		return;
	}
	InputSubsystem->RemoveMappingContext(MappingContext,Options);
	if (AbilitySimulationComponent)
	{
		AbilitySimulationComponent->RemoveMappingContext(MappingContext);
	}
}

void UAbilitySimulationLibrary::FillAbilityInstanceDataFromInstance(UNpGameplayAbility* AbilityInstance,
                                                                    FActiveAbilityInstanceData& OutData)
{
	if(!AbilityInstance)
	{
		return;
	}
	OutData.bIsActive = AbilityInstance->IsActive();
	OutData.ActivatedByInput = AbilityInstance->ActivatedByInputActionIndex;
	GetAbilitySyncedVariables(OutData.AbilitySyncedVars,AbilityInstance);
	OutData.bIsCancelable = AbilityInstance->CanBeCanceled();
	OutData.bIsBlockingOtherAbilities = AbilityInstance->IsBlockingOtherAbilities();
	OutData.TrackedGameplayCues = AbilityInstance->TrackedGameplayCues;
	OutData.bHasEventData = AbilityInstance->bHasEventData;
	OutData.CurrentEventData = AbilityInstance->CurrentEventData;
	TArray<FAbilityTaskDataContainer>& TasksData = OutData.TaskDataCollection.AbilityTasksData;
	TasksData.Empty(AbilityInstance->PredictionTasksInstances.Num());
	for (int32 k = 0 ; k < AbilityInstance->PredictionTasksInstances.Num() ; ++k)
	{
		UBasePredictionTask* Task = AbilityInstance->PredictionTasksInstances[k];
		FAbilityTaskDataContainer TaskData;
		TaskData.IsActive = Task->IsActive();
		if (TaskData.IsActive && Task->DataType != nullptr)
		{
			TaskData.TaskDataPointer = FAbilityTaskDataArray::CreateDataByType(Task->DataType);
			Task->WriteToSyncedData(TaskData.TaskDataPointer);
		}
		else
		{
			TaskData.TaskDataPointer.Reset();
		}
		TasksData.Add(TaskData);
	}
}



void UAbilitySimulationLibrary::FillAbilitiesCollectionDataFromSpecContainer(
	FActivatableAbilitiesCollection& Collection, FGameplayAbilitySpecContainer& SpecContainer)
{
	TArray<FActivatableAbilitySyncState>& ActivatableAbilities = Collection.GetCollectionData_Mutable();
	ActivatableAbilities.Empty(SpecContainer.Items.Num());
	
	for (int32 i = 0 ; i < SpecContainer.Items.Num() ; ++i)
	{
		FActivatableAbilitySyncState& AbilitySyncState = ActivatableAbilities.AddDefaulted_GetRef();
		FGameplayAbilitySpec& Spec = SpecContainer.Items[i];
		
		AbilitySyncState.Level = Spec.Level;
		AbilitySyncState.AbilityClass = Spec.Ability.GetClass();
		check(AbilitySyncState.AbilityClass && AbilitySyncState.AbilityClass->IsChildOf(UNpGameplayAbility::StaticClass()))
		AbilitySyncState.ActivatableAbilityHandle = Spec.Handle.GetHandle();
		AbilitySyncState.SourceObject = Spec.SourceObject.Get();
		AbilitySyncState.GrantingGameplayEffectHandle = Spec.GameplayEffectHandle.GetHandle();
		AbilitySyncState.SetByCallerTagMagnitudes = Spec.SetByCallerTagMagnitudes;
		AbilitySyncState.DynamicAbilityTags = Spec.DynamicAbilityTags;
		AbilitySyncState.ActiveCount = Spec.ActiveCount;
		AbilitySyncState.RemoveAfterActivation = Spec.RemoveAfterActivation;
		// Active Instances
		AbilitySyncState.ActiveInstances.ActiveAbilityInstances.Empty(Spec.ReplicatedInstances.Num());
		for (int32 j = 0 ; j < Spec.ReplicatedInstances.Num() ; ++j)
		{
			UNpGameplayAbility* Ability = Cast<UNpGameplayAbility>(Spec.ReplicatedInstances[j]);
			if(IsValid(Ability))
			{
				FActiveAbilityInstanceData InstanceData;
				FillAbilityInstanceDataFromInstance(Ability,InstanceData);
				AbilitySyncState.ActiveInstances.ActiveAbilityInstances.Add(InstanceData);
			}
		}
	}
	// sort the collection at the end to ensure FActivatableAbilitiesCollection should reconcile doesn't get false positive
	// if abilities array order changes
	if (ActivatableAbilities.Num() > 0)
	{
		ActivatableAbilities.Sort([](const FActivatableAbilitySyncState& A, const FActivatableAbilitySyncState& B)
		{
			return A.ActivatableAbilityHandle < B.ActivatableAbilityHandle;
		});
	}
}

void UAbilitySimulationLibrary::GetAbilitySyncedVariables(FSyncVarCollection& OutCollection,
	const UNpGameplayAbility* AbilityInstance)
{
	TArray<FSyncVarDef> AbilitySyncVars;
	AbilityInstance->GetSyncedVars(AbilitySyncVars);
	OutCollection.SyncedVars.Empty(AbilitySyncVars.Num());
	
	for (int32 i = 0 ; i < AbilitySyncVars.Num() ; ++i)
	{
		const FProperty* Property = AbilitySyncVars[i].MemberSyncVariable;
		// for debugging 
		check(Property);
		if (Property)
		{
			// Get pointer to the FSyncedVar in this ability instance
			const FBaseSyncVar* SyncedVar = static_cast<const FBaseSyncVar*>(
				Property->ContainerPtrToValuePtr<void>(AbilityInstance)
			);

			// for debugging 
			check(SyncedVar);

			// Clone it and add to output array
			if (SyncedVar)
			{
				TSharedPtr<FBaseSyncVar> NewVar = FSyncVarCollection::CreateDataByType(SyncedVar->GetScriptStruct());
				NewVar->SetValue(SyncedVar);
				OutCollection.SyncedVars.Add(NewVar);
				continue;
			}
		}
		OutCollection.SyncedVars.Add(nullptr);
	}
}

struct FActorArrayDelta
{
	TArray<uint8> RemovalIndices;    // Indices to remove from base (sorted descending)
	TArray<AActor*> ActorsToAdd;     // New actors to append
};

FActorArrayDelta ComputeDelta(const TArray<AActor*>& Base, const TArray<AActor*>& Current)
{
	FActorArrayDelta Delta;
    
	// Convert Current to set for O(1) lookups
	TSet<AActor*> CurrentSet(Current);
    
	// Find indices to remove (actors in Base but not in Current)
	for (int32 i = 0; i < Base.Num(); ++i)
	{
		if (!CurrentSet.Contains(Base[i]))
		{
			Delta.RemovalIndices.Add(static_cast<uint8>(i));
		}
	}
    
	// Sort removal indices in descending order for safe removal
	Delta.RemovalIndices.Sort([](uint8 A, uint8 B) { return A > B; });
    
	// Find actors to add (in Current but not in Base)
	TSet<AActor*> BaseSet(Base);
	for (AActor* Actor : Current)
	{
		if (!BaseSet.Contains(Actor))
		{
			Delta.ActorsToAdd.Add(Actor);
		}
	}
    
	return Delta;
}

TArray<AActor*> ApplyDelta(const TArray<AActor*>& Base, const FActorArrayDelta& Delta)
{
	TArray<AActor*> Result = Base;

	if (Delta.RemovalIndices.Num() <= 0 && Delta.ActorsToAdd.Num() <= 0)
	{
		return Result;
	}
	// Remove by indices (already sorted descending, so safe to remove)
	for (uint8 IndexToRemove : Delta.RemovalIndices)
	{
		Result.RemoveAt(IndexToRemove);
	}
    
	// Add new actors
	Result.Append(Delta.ActorsToAdd);
    
	return Result;
}
void UAbilitySimulationLibrary::NetSerializeUniqueActorsArrays(const FNetSerializeParams& Params, TArray<AActor*>& Actors)
{
	bool bHasIgnoredActors = Params.Ar.IsSaving() ? Actors.Num() > 0 : false;
	Params.Ar.SerializeBits(&bHasIgnoredActors,1);
	if (bHasIgnoredActors)
	{
		SafeNetSerializeTArray_Default<31>(Params.Ar,Actors);
	}
	else
	{
		Actors.Empty();
	}
	return;
}

void UAbilitySimulationLibrary::NetDeltaSerializeUniqueActorsArrays(const FNetSerializeParams& Params,const TArray<AActor*>&  BaseArray,
	TArray<AActor*>& Actors)
{
	bool bHasIgnoredActors = Params.Ar.IsSaving() ? Actors.Num() > 0 : false;
	Params.Ar.SerializeBits(&bHasIgnoredActors,1);
	if (!bHasIgnoredActors)
	{
		Actors.Empty();
		return;
	}
	FActorArrayDelta Delta;
	if (Params.Ar.IsSaving())
	{
		Delta = ComputeDelta(BaseArray,Actors);
	}
	bool bHasNew = Params.Ar.IsSaving() ? Delta.ActorsToAdd.Num() > 0 : false;
	Params.Ar.SerializeBits(&bHasNew,1);
	if (bHasNew)
	{
		SafeNetSerializeTArray_Default<31>(Params.Ar,Delta.ActorsToAdd);
	}
	else
	{
		Delta.ActorsToAdd.Empty();
	}
	bool bHasToRemove = Params.Ar.IsSaving() ? Delta.RemovalIndices.Num() > 0 : false;
	Params.Ar.SerializeBits(&bHasToRemove,1);
	if (bHasToRemove)
	{
		SafeNetSerializeTArray_Default<31>(Params.Ar,Delta.RemovalIndices);
	}
	else
	{
		Delta.RemovalIndices.Empty();
	}
	
	if (Params.Ar.IsLoading())
	{
		Actors = ApplyDelta(BaseArray,Delta);
	}
}




