// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitMovementModeChangedPredictionTask.h"

#include "AbilitySystemLog.h"
#include "MoverComponent.h"
#include "Abilities/NpGameplayAbility.h"

#define LOCTEXT_NAMESPACE "WaitMovementModeChangedPredictionTask"

UWaitMovementModeChangedPredictionTask::UWaitMovementModeChangedPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitMovementModeChangedPredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}

void UWaitMovementModeChangedPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (!GetMoverComponent())
	{
		UE_LOG(LogAbilitySystem,Error,TEXT("Wait Movement Mode Changed Prediction Task Can't find Mover Component In Avatar Actor, Will Not Work!!"))
	}
	GetMoverComponent()->OnMovementModeChanged.AddUniqueDynamic(this,&UWaitMovementModeChangedPredictionTask::OnMovementModeChanged);
	ActivateTask();
}

void UWaitMovementModeChangedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (!IsValid(GetMoverComponent()))
	{
		return;
	}
	if (IsActive() != AuthoritySyncData.IsActive)
	{
		if (AuthoritySyncData.IsActive)
		{
			GetMoverComponent()->OnMovementModeChanged.AddUniqueDynamic(this,&UWaitMovementModeChangedPredictionTask::OnMovementModeChanged);
		}
		else
		{
			GetMoverComponent()->OnMovementModeChanged.RemoveAll(this);
		}
	}
}

void UWaitMovementModeChangedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (IsValid(GetMoverComponent()) && IsValid(this))
	{
		GetMoverComponent()->OnMovementModeChanged.RemoveAll(this);
	}
}

void UWaitMovementModeChangedPredictionTask::OnMovementModeChanged(const FName& PreviousMode, const FName& NewMode)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (MatchCurrentMovementMode && NewMode != MovementModeName)
	{
		return;
	}
	if (MatchPreviousMovementMode && PreviousMode != PreviousMovementModeName)
	{
		return;
	}
	OnModeChanged.Broadcast(PreviousMode,NewMode);
	if (TriggerOnce)
	{
		DeactivateTask(false);
	}
}

UMoverComponent* UWaitMovementModeChangedPredictionTask::GetMoverComponent() const
{
	AActor* Avatar = GetOwningAbility()->GetAvatarActorFromActorInfo();
	return Avatar ? Avatar->GetComponentByClass<UMoverComponent>() : nullptr;
}

FText UWaitMovementModeChangedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("K2Node_WaitMovementModeChangedPredictionTask", "Wait Movement Mode Changed Prediction Task");
}

FLinearColor UWaitMovementModeChangedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.729f, 0.204f, 0.255f, 1.0f);
}

#undef LOCTEXT_NAMESPACE
