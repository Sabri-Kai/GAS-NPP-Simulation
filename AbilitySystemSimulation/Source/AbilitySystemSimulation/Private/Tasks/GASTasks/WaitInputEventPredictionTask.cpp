// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitInputEventPredictionTask.h"
#include "DataTypes/AbilitiesDataTypes.h"
#include "AbilitySystemLog.h"
#include "Abilities/NpAbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "WaitInputEventPredictionTask"

UWaitInputEventPredictionTask::UWaitInputEventPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitInputEventPredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}
void UWaitInputEventPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (!GetInputAction())
	{
		UE_LOG(LogAbilitySystem,Error,TEXT("Wait Input Event Prediction Task in Ability %s Does not have a valid input action"),*GetNameSafe(GetOwningAbility()))
	}
	OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitInputEventPredictionTask::OnInputEvent);
	ActivateTask();
}

void UWaitInputEventPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		if (AuthoritySyncData.IsActive)
		{
			if (!OnInputEventDelegateHandle.IsValid())
			{
				OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitInputEventPredictionTask::OnInputEvent);
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

void UWaitInputEventPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (OnInputEventDelegateHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnInputActionEvent.Remove(OnInputEventDelegateHandle);
		OnInputEventDelegateHandle.Reset();
	}
}

void UWaitInputEventPredictionTask::OnInputEvent(const UInputAction* TriggerInputAction, const ETriggerEvent& TriggerEvent)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (GetInputAction() == TriggerInputAction)
	{
		switch (TriggerEvent)
		{
		case ETriggerEvent::Triggered:
			{
				Triggered.Broadcast();
				if (EndOnTriggered)
				{
					DeactivateTask(false);
				}
				break;
			}
		case ETriggerEvent::Started:
			{
				Started.Broadcast();
				break;
			}
		case ETriggerEvent::Ongoing:
			{
				Ongoing.Broadcast();
				break;
			}
		case ETriggerEvent::Completed:
			{
				Completed.Broadcast();
				if (EndOnCompletedOrCanceled)
				{
					DeactivateTask(false);
				}
				break;
			}
		case ETriggerEvent::Canceled:
			{
				Canceled.Broadcast();
				if (EndOnCompletedOrCanceled)
				{
					DeactivateTask(true);
				}
				break;
			}
		case ETriggerEvent::None: break;
		}
	}
}

FText UWaitInputEventPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("K2Node_WaitInputEventPredictionTask", "Wait Input Event Prediction Task");
}

FLinearColor UWaitInputEventPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.09f, 0.878f, 0.875f, 1.0f);
}

UInputAction* UWaitInputEventPredictionTask::GetInputAction() const
{
	if (UseAbilityInputAction)
	{
		FAbilityActivationTrigger Trigger;
		if (GetOwningAbility()->GetActivatedByTrigger(Trigger))
		{
			return Trigger.InputAction;
		}
		return nullptr;
	}
	return InputAction;
}

#undef LOCTEXT_NAMESPACE
