// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/TickForDurationPredictionTask.h"

#include "DataTypes/AbilitySimulationDataTypes.h"

#define LOCTEXT_NAMESPACE "TickForDurationPredictionTask"

void UTickForDurationPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (IsActive())
	{
		OnTick.Broadcast(TimeStep.StepMs * 0.001f,GetCurrentWaitDuration());
	}
}

FText UTickForDurationPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("TickForDurationPredictionTask", "Tick For Duration Prediction Task");
}


void UTickForDynamicDurationPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	if (IsActive())
	{
		OnTick.Broadcast(TimeStep.StepMs * 0.001f,GetCurrentWaitDuration());
	}
}

FText UTickForDynamicDurationPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("TickForDynamicDurationPredictionTask", "Tick For Dynamic Duration Prediction Task");
}
#undef LOCTEXT_NAMESPACE