// 2025 Yohoho Productions /  Sirkai


#include "Tasks/GASTasks/WaitAnyInputEventPredictionTask.h"

#include "AbilitySystemLog.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"


#define LOCTEXT_NAMESPACE "WaitAnyInputEventPredictionTask"

UWaitAnyInputEventPredictionTask::UWaitAnyInputEventPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitAnyInputEventPredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}
void UWaitAnyInputEventPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (InputTriggers.Num() <= 0)
	{		
		UE_LOG(LogAbilitySystem,Error,TEXT("Wait Any Input Event Prediction Task in Ability %s Does not have any input Trigger"),*GetNameSafe(GetOwningAbility()))
	}
	OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitAnyInputEventPredictionTask::OnInputEventCalled);
	ActivateTask();
}

void UWaitAnyInputEventPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		if (AuthoritySyncData.IsActive)
		{
			if (!OnInputEventDelegateHandle.IsValid())
			{
				OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitAnyInputEventPredictionTask::OnInputEventCalled);
			}
		}
		else
		{
			if (OnInputEventDelegateHandle.IsValid())
			{
				GetAbilitySystemComponent()->OnInputActionEvent.Remove(OnInputEventDelegateHandle);
				OnInputEventDelegateHandle.Reset();
			}
		}
	}
}

void UWaitAnyInputEventPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (OnInputEventDelegateHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnInputActionEvent.Remove(OnInputEventDelegateHandle);
		OnInputEventDelegateHandle.Reset();
	}
}

void UWaitAnyInputEventPredictionTask::OnInputEventCalled(const UInputAction* TriggerInputAction, const ETriggerEvent& TriggerEvent)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	for (const FAbilityActivationTrigger& Trigger : InputTriggers)
	{
		if (TriggerInputAction == Trigger.InputAction && TriggerEvent == Trigger.TriggerEvent)
		{
			OnInputEvent.Broadcast(Trigger);
			if (EndOnFirstTriggered)
			{
				DeactivateTask(false);
			}
		}
	}
}

FText UWaitAnyInputEventPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("K2Node_WaitAnyInputEventPredictionTask", "Wait Any Input Event Prediction Task");
}

FLinearColor UWaitAnyInputEventPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.09f, 0.878f, 0.875f, 1.0f);
}

#undef LOCTEXT_NAMESPACE