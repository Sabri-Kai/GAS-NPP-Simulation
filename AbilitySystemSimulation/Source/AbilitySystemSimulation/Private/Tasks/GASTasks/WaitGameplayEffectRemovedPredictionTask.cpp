// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitGameplayEffectRemovedPredictionTask.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"
#include "Tasks/GASTasks/CommonDataTypes.h"

#define LOCTEXT_NAMESPACE "WaitGameplayEffectRemovedTask"

#pragma region Wait Gameplay Effect Removed Task

bool FWaitGameplayEffectRemovedTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	bool ValidHandle = Params.Ar.IsSaving() ? Handle >= 0 : false;
	Params.Ar.SerializeBits(&ValidHandle, 1);
	if (ValidHandle)
	{
		uint32 UnsignedHandle = Handle;
		Params.Ar.SerializeIntPacked(UnsignedHandle);
		Handle = UnsignedHandle;
	}
	else
	{
		Handle = INDEX_NONE;
	}
	Params.Ar.SerializeBits(&bIsSelf, 1);
	if (!bIsSelf)
	{
		bool bIsValid = Params.Ar.IsSaving() ?  IsValid(TargetASC) : false;
		Params.Ar.SerializeBits(&bIsValid, 1);

		if (bIsValid)
		{
			Params.Ar << TargetASC;
		}
	}
	

	return true;
}

bool FWaitGameplayEffectRemovedTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FWaitGameplayEffectRemovedTaskData* BaseDelta = Params.GetBaseDeltaState<FWaitGameplayEffectRemovedTaskData>();
	check(BaseDelta);
	// Handle
	bool HandleSameAsDelta =  Params.Ar.IsSaving() ? Handle == BaseDelta->Handle : false;
	Params.Ar.SerializeBits(&HandleSameAsDelta, 1);
	if (HandleSameAsDelta)
	{
		Handle = BaseDelta->Handle;
	}
	else
	{
		bool ValidHandle = Params.Ar.IsSaving() ? Handle > 0 : false;
		Params.Ar.SerializeBits(&ValidHandle, 1);
		if (ValidHandle)
		{
			uint32 UnsignedHandle = Handle;
			Params.Ar.SerializeIntPacked(UnsignedHandle);
			Handle = UnsignedHandle;
		}
		else
		{
			Handle = INDEX_NONE;
		}
	}
	// Target ASC
	Params.Ar.SerializeBits(&bIsSelf, 1);
	if (!bIsSelf)
	{
		bool ASCSameAsDelta =  Params.Ar.IsSaving() ? TargetASC == BaseDelta->TargetASC : false;
		Params.Ar.SerializeBits(&ASCSameAsDelta, 1);
		if (ASCSameAsDelta)
		{
			TargetASC = BaseDelta->TargetASC;
		}
		else
		{
			bool ValidAsc = Params.Ar.IsSaving() ? IsValid(TargetASC) : false;
			Params.Ar.SerializeBits(&ValidAsc, 1);
			if (ValidAsc)
			{
				Params.Ar << TargetASC;
			}
			else
			{
				TargetASC = nullptr;
			}
		}
	}
	
	return true;
}

UScriptStruct* FWaitGameplayEffectRemovedTaskData::GetScriptStruct() const
{
	return StaticStruct();
}

void FWaitGameplayEffectRemovedTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("      Effect Handle : %i \n", Handle);
}

bool FWaitGameplayEffectRemovedTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FWaitGameplayEffectRemovedTaskData* AuthorityData = static_cast<const FWaitGameplayEffectRemovedTaskData*>(&AuthorityState);
	return (AuthorityData->Handle != Handle) || (AuthorityData->TargetASC != TargetASC);
}

UWaitGameplayEffectRemovedPredictionTask::UWaitGameplayEffectRemovedPredictionTask(
	const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	DataType = FWaitGameplayEffectRemovedTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayEffectRemovedPredictionTask, ExecuteTask);
	ShouldTaskTick = false;
}

void UWaitGameplayEffectRemovedPredictionTask::ExecuteTask(const FActiveGameplayEffectHandle EffectHandle)
{
	if (IsInRollback())
	{
		return;
	}
	FGameplayEffectRemovalInfo EmptyGameplayEffectRemovalInfo;
	Handle = EffectHandle;
	bIsTargetAscOwner = IsValid(Handle.GetOwningAbilitySystemComponent())
	&& Handle.GetOwningAbilitySystemComponent() == GetOwningAbility()->GetAbilitySystemComponentFromActorInfo();
	
	if (Handle.IsValid() == false)
	{
		InvalidHandle.Broadcast(EmptyGameplayEffectRemovalInfo);
		return;
	}
	
	UAbilitySystemComponent* AbilitySystem = bIsTargetAscOwner ? GetOwningAbility()->GetAbilitySystemComponentFromActorInfo() : Handle.GetOwningAbilitySystemComponent();
	if (!AbilitySystem)
	{
		return;
	}

	if (FOnActiveGameplayEffectRemoved_Info* DelPtr = AbilitySystem->OnGameplayEffectRemoved_InfoDelegate(Handle))
	{
		OnGameplayEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &ThisClass::OnGameplayEffectRemoved);
		ActivateTask();
		return;
	}
	
	// GameplayEffect was already removed, treat this as a warning? Could be cases of immunity or chained gameplay rules that would instant remove something
	OnGameplayEffectRemoved(EmptyGameplayEffectRemovalInfo);
}

void UWaitGameplayEffectRemovedPredictionTask::OnGameplayEffectRemoved(const FGameplayEffectRemovalInfo& InGameplayEffectRemovalInfo)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	OnRemoved.Broadcast(InGameplayEffectRemovalInfo);
	DeactivateTask(false);
}

void UWaitGameplayEffectRemovedPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FWaitGameplayEffectRemovedTaskData* AuthorityData = static_cast<const FWaitGameplayEffectRemovedTaskData*>(DataToRead.Get());
	bIsTargetAscOwner = AuthorityData->bIsSelf;
	UAbilitySystemComponent* TargetASC = bIsTargetAscOwner ? GetOwningAbility()->GetAbilitySystemComponentFromActorInfo() : AuthorityData->TargetASC.Get(); 
	Handle.SetSyncedHandle(TargetASC, AuthorityData->Handle);
	
}

void UWaitGameplayEffectRemovedPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FWaitGameplayEffectRemovedTaskData* AuthorityData = static_cast< FWaitGameplayEffectRemovedTaskData*>(DataToWrite.Get());
	AuthorityData->bIsSelf = bIsTargetAscOwner;
	AuthorityData->Handle = Handle.GetHandle();
	AuthorityData->TargetASC = bIsTargetAscOwner ? GetOwningAbility()->GetAbilitySystemComponentFromActorInfo() : Handle.GetOwningAbilitySystemComponent();
}

void UWaitGameplayEffectRemovedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	if (OnGameplayEffectRemovedDelegateHandle.IsValid())
	{
		if (UAbilitySystemComponent* AbilitySystem = Handle.GetOwningAbilitySystemComponent())
		{
			if (FOnActiveGameplayEffectRemoved_Info* DelPtr = AbilitySystem->OnGameplayEffectRemoved_InfoDelegate(Handle))
			{
				DelPtr->Remove(OnGameplayEffectRemovedDelegateHandle);
			}
		}
		OnGameplayEffectRemovedDelegateHandle.Reset();
	}
	Super::OnPreDeactivate(false);
}

void UWaitGameplayEffectRemovedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	if (!AuthoritySyncData.IsActive)
	{
		// If authority data is not bounded, then clean up client delegate if it exists
		if (OnGameplayEffectRemovedDelegateHandle.IsValid())
		{
			if (UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent())
			{
				if (FOnActiveGameplayEffectRemoved_Info* DelPtr = ASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
				{
					DelPtr->Remove(OnGameplayEffectRemovedDelegateHandle);
				}
			}
		}
		return;
	}

	const FWaitGameplayEffectRemovedTaskData* AuthorityData = static_cast<const FWaitGameplayEffectRemovedTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
	check(AuthorityData);

	UAbilitySystemComponent* AuthorityASC = AuthorityData->bIsSelf ? GetOwningAbility()->GetAbilitySystemComponentFromActorInfo() : AuthorityData->TargetASC.Get();
	
	if (!AuthorityASC && Handle.GetOwningAbilitySystemComponent())
	{
		if (OnGameplayEffectRemovedDelegateHandle.IsValid())
		{
			if (UAbilitySystemComponent* CurrentASC = Handle.GetOwningAbilitySystemComponent())
			{
				if (FOnActiveGameplayEffectRemoved_Info* DelPtr = CurrentASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
				{
					DelPtr->Remove(OnGameplayEffectRemovedDelegateHandle);
				}
			}
		}
	}

	const bool bSameHandle = (Handle.GetHandle() == AuthorityData->Handle);
	const bool bSameASC = (Handle.GetOwningAbilitySystemComponent() == AuthorityASC);

	if (IsActive())
	{
		if (bSameHandle && bSameASC)
		{
			// Only bind if not already bound
			if (!OnGameplayEffectRemovedDelegateHandle.IsValid())
			{
				if (FOnActiveGameplayEffectRemoved_Info* DelPtr = AuthorityASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
				{
					OnGameplayEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &ThisClass::OnGameplayEffectRemoved);
				}
			}
		}
		else
		{
			// Remove old delegate
			if (OnGameplayEffectRemovedDelegateHandle.IsValid())
			{
				if (UAbilitySystemComponent* CurrentASC = Handle.GetOwningAbilitySystemComponent())
				{
					if (FOnActiveGameplayEffectRemoved_Info* DelPtr = CurrentASC->OnGameplayEffectRemoved_InfoDelegate(Handle))
					{
						DelPtr->Remove(OnGameplayEffectRemovedDelegateHandle);
					}
				}
			}

			// Register new delegate on AuthorityASC
			if (FOnActiveGameplayEffectRemoved_Info* DelPtr = AuthorityASC->OnGameplayEffectRemoved_InfoDelegate(AuthorityData->Handle))
			{
				OnGameplayEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &ThisClass::OnGameplayEffectRemoved);
			}
		}
	}
	else
	{
		// Task is not active, just bind to AuthorityASC
		if (FOnActiveGameplayEffectRemoved_Info* DelPtr = AuthorityASC->OnGameplayEffectRemoved_InfoDelegate(AuthorityData->Handle))
		{
			OnGameplayEffectRemovedDelegateHandle = DelPtr->AddUObject(this, &ThisClass::OnGameplayEffectRemoved);
		}
	}
}

FText UWaitGameplayEffectRemovedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayEffectRemoved", "Wait Gameplay Effect Removed Prediction Task");
}

FLinearColor UWaitGameplayEffectRemovedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion
#undef LOCTEXT_NAMESPACE