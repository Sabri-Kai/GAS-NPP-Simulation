// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitAbilityCommitPredictionTask.h"

#include "AbilitySystemComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"

#define LOCTEXT_NAMESPACE "WaitAbilityCommitTask"

UWaitAbilityCommitPredictionTask::UWaitAbilityCommitPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitAbilityCommitPredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}

void UWaitAbilityCommitPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (UAbilitySystemComponent* ASC = GetOwningAbility()->GetAbilitySystemComponentFromActorInfo())
	{
		ActivateTask();
		OnAbilityCommitDelegateHandle = ASC->AbilityActivatedCallbacks.AddUObject(this, &UWaitAbilityCommitPredictionTask::OnAbilityActivate);
	}
}

void UWaitAbilityCommitPredictionTask::OnAbilityActivate(UGameplayAbility* ActivatedAbility)
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
		OnAbilityCommit.Broadcast(ActivatedAbility);
		if (TriggerOnce)
		{
			DeactivateTask(false);
			GetAbilitySystemComponent()->AbilityActivatedCallbacks.Remove(OnAbilityCommitDelegateHandle);
			OnAbilityCommitDelegateHandle.Reset();
		}
	}
}

void UWaitAbilityCommitPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		//if we should be active make sure to bind delegate
		if (AuthoritySyncData.IsActive && !OnAbilityCommitDelegateHandle.IsValid())
		{
			OnAbilityCommitDelegateHandle = GetAbilitySystemComponent()->AbilityActivatedCallbacks.AddUObject(this, &UWaitAbilityCommitPredictionTask::OnAbilityActivate);
			return;
		}
		if (!AuthoritySyncData.IsActive && OnAbilityCommitDelegateHandle.IsValid())
		{
			GetAbilitySystemComponent()->AbilityActivatedCallbacks.Remove(OnAbilityCommitDelegateHandle);
			OnAbilityCommitDelegateHandle.Reset();
		}
	}
}


FText UWaitAbilityCommitPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitAbilityCommitTask", "Wait Ability Commit Prediction Task");
}

FLinearColor UWaitAbilityCommitPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}

void UWaitAbilityCommitPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (OnAbilityCommitDelegateHandle.IsValid())
	{
		GetAbilitySystemComponent()->AbilityCommittedCallbacks.Remove(OnAbilityCommitDelegateHandle);
		OnAbilityCommitDelegateHandle.Reset();
	}
}

#undef LOCTEXT_NAMESPACE