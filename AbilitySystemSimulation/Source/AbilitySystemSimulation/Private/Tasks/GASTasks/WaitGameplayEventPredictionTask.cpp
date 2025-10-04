// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitGameplayEventPredictionTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Tasks/GASTasks/CommonDataTypes.h"


#define LOCTEXT_NAMESPACE "WaitGameplayEventTask"

UWaitGameplayEventPredictionTask::UWaitGameplayEventPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FExternalTargetTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayEventPredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitGameplayEventPredictionTask::ExecuteTaskWithTarget(AActor* ExternalTarget)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (ExternalTarget)
	{
		CachedExternalTarget = ExternalTarget;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?  GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		if (OnlyMatchExact)
		{
			WaitEventHandle = TargetAsc->GenericGameplayEventCallbacks.FindOrAdd(EventTag).AddUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventCallback);
		}
		else
		{
			WaitEventHandle = TargetAsc->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventContainerCallback));
		}	
		ActivateTask();
	}
}

void UWaitGameplayEventPredictionTask::GameplayEventCallback(const FGameplayEventData* Payload)
{
	GameplayEventContainerCallback(EventTag, Payload);
}

void UWaitGameplayEventPredictionTask::GameplayEventContainerCallback(FGameplayTag MatchingTag,
	const FGameplayEventData* Payload)
{
	if (ShouldTriggerCallbacks())
	{
		ensureMsgf(Payload, TEXT("GameplayEventCallback expected non-null Payload"));
		FGameplayEventData TempPayload = Payload ? *Payload : FGameplayEventData{};
		TempPayload.EventTag = MatchingTag;
		EventReceived.Broadcast(MoveTemp(TempPayload));
		if (TriggerOnce)
		{
			DeactivateTask(false);
		}
	}
}

UAbilitySystemComponent* UWaitGameplayEventPredictionTask::GetFocusedASC()
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitGameplayEventPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (OnlyMatchExact)
	{
		GetFocusedASC()->GenericGameplayEventCallbacks.FindOrAdd(EventTag).Remove(WaitEventHandle);
	}
	else
	{
		GetFocusedASC()->RemoveGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), WaitEventHandle);
	}
}

void UWaitGameplayEventPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AuthorityWaitEventData = static_cast<FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AuthorityWaitEventData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AuthorityWaitEventData->ExternalTarget);
		// we are already bound
		if (WaitEventHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				if (OnlyMatchExact)
				{
					GetFocusedASC()->GenericGameplayEventCallbacks.FindOrAdd(EventTag).Remove(WaitEventHandle);
					WaitEventHandle = TargetAsc->GenericGameplayEventCallbacks.FindOrAdd(EventTag).AddUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventCallback);
				}
				else
				{
					GetFocusedASC()->RemoveGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), WaitEventHandle);
					WaitEventHandle = TargetAsc->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventContainerCallback));
				}
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			if (OnlyMatchExact)
			{
				WaitEventHandle = TargetAsc->GenericGameplayEventCallbacks.FindOrAdd(EventTag).AddUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventCallback);
			}
			else
			{
				WaitEventHandle = TargetAsc->AddGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UWaitGameplayEventPredictionTask::GameplayEventContainerCallback));
			}
		}
	}
	else // we shouldn't be active
	{
		if (WaitEventHandle.IsValid())
		{
			// we are bound , so unbind and reset.
			if (OnlyMatchExact)
			{
				GetFocusedASC()->GenericGameplayEventCallbacks.FindOrAdd(EventTag).Remove(WaitEventHandle);
			}
			else
			{
				GetFocusedASC()->RemoveGameplayEventTagContainerDelegate(FGameplayTagContainer(EventTag), WaitEventHandle);
			}
			WaitEventHandle.Reset();
		}
		
	}
}

void UWaitGameplayEventPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FExternalTargetTaskData* ExternalTargetData = static_cast<const FExternalTargetTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
}

void UWaitGameplayEventPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FExternalTargetTaskData* ExternalTargetData = static_cast< FExternalTargetTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	
}

FText UWaitGameplayEventPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayEventTask", "Wait Gameplay Event Prediction Task");
}

FLinearColor UWaitGameplayEventPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#undef LOCTEXT_NAMESPACE
