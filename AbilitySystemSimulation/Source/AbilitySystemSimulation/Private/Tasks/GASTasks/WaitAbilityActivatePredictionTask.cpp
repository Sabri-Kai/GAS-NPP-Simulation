// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitAbilityActivatePredictionTask.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"

#define LOCTEXT_NAMESPACE "WaitAbilityActivateTask"

UWaitAbilityActivatePredictionTask::UWaitAbilityActivatePredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitAbilityActivatePredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}

void UWaitAbilityActivatePredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (GetAbilitySystemComponent())
	{
		ActivateTask();
		OnAbilityActivateDelegateHandle = GetAbilitySystemComponent()->AbilityActivatedCallbacks.AddUObject(this, &UWaitAbilityActivatePredictionTask::OnAbilityActivate);
	}
}

void UWaitAbilityActivatePredictionTask::OnAbilityActivate(UGameplayAbility* ActivatedAbility)
{
	switch (ConditionType)
	{
	case EWithOrWithoutTag:
		{
			if ((WithTag.IsValid() && !ActivatedAbility->AbilityTags.HasTag(WithTag)) ||
			(WithoutTag.IsValid() && ActivatedAbility->AbilityTags.HasTag(WithoutTag)))
			{
				// Failed tag check
				return;
			}
			break;
		}
	case EWithTagRequirements:
		{
			if (!TagRequirements.IsEmpty() && !TagRequirements.RequirementsMet(ActivatedAbility->AbilityTags))
			{
				// Failed tag check
				return;
			}
			break;
		}
	case EWithQuery:
		{
			if (!Query.IsEmpty() && Query.Matches(ActivatedAbility->AbilityTags) == false)
			{
				// Failed query
				return;
			}
			break;
		}
	default:
		{
			break;
		}
	}
	
	if (IsActive() && ShouldTriggerCallbacks())
	{
		OnAbilityActivated.Broadcast(ActivatedAbility);
		if (TriggerOnce)
		{
			DeactivateTask(false);
			if (UAbilitySystemComponent* ASC = GetOwningAbility()->GetAbilitySystemComponentFromActorInfo())
			{
				ASC->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
				OnAbilityActivateDelegateHandle.Reset();
			}
		}
	}
}

void UWaitAbilityActivatePredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	// rollback is changing activation state
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		//if we should be active make sure to bind delegate
		if (AuthoritySyncData.IsActive && !OnAbilityActivateDelegateHandle.IsValid())
		{
			OnAbilityActivateDelegateHandle = GetAbilitySystemComponent()->AbilityActivatedCallbacks.AddUObject(this, &UWaitAbilityActivatePredictionTask::OnAbilityActivate);
			return;
		}
		if (!AuthoritySyncData.IsActive && OnAbilityActivateDelegateHandle.IsValid())
		{
			GetAbilitySystemComponent()->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
			OnAbilityActivateDelegateHandle.Reset();
		}
	}
}

FText UWaitAbilityActivatePredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitAbilityActivateTask", "Wait Ability Activate Prediction Task");
}

FLinearColor UWaitAbilityActivatePredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}

void UWaitAbilityActivatePredictionTask::OnPreDeactivate(const bool& bWasCancelled)
{
	Super::OnPreDeactivate(bWasCancelled);
	if (OnAbilityActivateDelegateHandle.IsValid())
	{
		UAbilitySystemComponent* ASC = GetOwningAbility()->GetAbilitySystemComponentFromActorInfo();
		ASC->AbilityActivatedCallbacks.Remove(OnAbilityActivateDelegateHandle);
		OnAbilityActivateDelegateHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
