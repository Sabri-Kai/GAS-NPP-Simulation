// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/TargetingTasks/WaitInputTargetingPredictionTask.h"
#include "Abilities/NpAbilitySystemComponent.h"

#define LOCTEXT_NAMESPACE "TargetingTask"

UWaitInputTargetingPredictionTask::UWaitInputTargetingPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	// remove the input pins of confirmation they are controlled by input.
	AdditionalInputFunctions.Empty();
}
void UWaitInputTargetingPredictionTask::ExecuteTask(FVector InLocation, FVector InDirection, FRotator InRotation)
{
	Super::ExecuteTask(InLocation, InDirection, InRotation);
	// execution from the super can end if it finds a successful target, and it is set to end when doing so
	// so ensure we did activate
	if (IsActive())
	{
		if ((ConfirmInput.InputAction && ConfirmInput.TriggerEvent != ETriggerEvent::None)
			|| (CancelInput.InputAction && CancelInput.TriggerEvent != ETriggerEvent::None))
		{		
			OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitInputTargetingPredictionTask::OnInputEventCalled);
		}
	}
}

void UWaitInputTargetingPredictionTask::OnInputEventCalled(const UInputAction* TriggerInputAction,
	const ETriggerEvent& TriggerEvent)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (TriggerInputAction == ConfirmInput.InputAction && TriggerEvent == ConfirmInput.TriggerEvent)
	{
		ConfirmTargeting();
		return;
	}
	if (TriggerInputAction == CancelInput.InputAction && TriggerEvent == CancelInput.TriggerEvent)
	{
		CancelTargeting();
	}
}

void UWaitInputTargetingPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		if (AuthoritySyncData.IsActive)
		{
			if (!OnInputEventDelegateHandle.IsValid())
			{
				OnInputEventDelegateHandle = GetAbilitySystemComponent()->OnInputActionEvent.AddUObject(this, &UWaitInputTargetingPredictionTask::OnInputEventCalled);
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

void UWaitInputTargetingPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (OnInputEventDelegateHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnInputActionEvent.Remove(OnInputEventDelegateHandle);
		OnInputEventDelegateHandle.Reset();
	}
}

FText UWaitInputTargetingPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitInputTargetingPredictionTask", "Wait Input Targeting Prediction Task");
}

#undef LOCTEXT_NAMESPACE