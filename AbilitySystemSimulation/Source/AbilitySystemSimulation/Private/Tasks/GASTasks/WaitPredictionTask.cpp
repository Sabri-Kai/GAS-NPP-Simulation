// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitPredictionTask.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Tasks/GASTasks/CommonDataTypes.h"

#define LOCTEXT_NAMESPACE "WaitPredictionTask"



#pragma region Wait For Fixed Duration Task Class
/////////////////////////////
UWaitForDurationPredictionTask::UWaitForDurationPredictionTask(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{
	DataType = FWaitFixedDurationTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitForDurationPredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}
void UWaitForDurationPredictionTask::ExecuteTask()
{
	if (IsInRollback() || !GetAbilitySystemComponent())
	{
		return;
	}
	StartTimeMS = GetAbilitySystemComponent()->GetCurrentSimulationTimeMS();
	ActivateTask();
}

uint32 UWaitForDurationPredictionTask::GetTotalWaitDurationMS() const
{
	return FMath::RoundToInt(FMath::Max(WaitDuration,0.f) * 1000.f);
}

void UWaitForDurationPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	// we wait at least 1 single tick, if Wait Duration <= 0 we wait a tick.
	uint32 WaitDurationMS = GetTotalWaitDurationMS();
	WaitDurationMS = FMath::Max(WaitDurationMS,TimeStep.StepMs);
	const uint32 CurrentDurationMS = TimeStep.BaseSimTimeMs - StartTimeMS;
	// sanity check
	if (IsActive())
	{
		if (CurrentDurationMS >= WaitDurationMS )
		{
			OnWaitEnded.Broadcast();
			if (ShouldTriggerOnce)
			{
				DeactivateTask(false);
			}
			else
			{
				StartTimeMS = TimeStep.BaseSimTimeMs;
			}
		}
	}
	
}

void UWaitForDurationPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FWaitFixedDurationTaskData* WaitTaskData = static_cast< FWaitFixedDurationTaskData*>(DataToWrite.Get());
	WaitTaskData->StartTimeMS = StartTimeMS;
}

void UWaitForDurationPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FWaitFixedDurationTaskData* WaitTaskData = static_cast< const FWaitFixedDurationTaskData*>(DataToRead.Get());
	StartTimeMS = WaitTaskData->StartTimeMS;
}

void UWaitForDurationPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
}

FText UWaitForDurationPredictionTask::GetNodeTitle() const
{
	return  LOCTEXT("WaitPredictionTask", "Wait For Duration Prediction Task");
}

FLinearColor UWaitForDurationPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.694f, 0.8f, 0.325f, 1.0f);
}

float UWaitForDurationPredictionTask::GetTotalWaitDuration() const
{
	return GetTotalWaitDurationMS() * 0.001f;
}

float UWaitForDurationPredictionTask::GetCurrentWaitDuration() const
{
	if (!GetAbilitySystemComponent())
	{
		return 0.f;
	}
	if (!IsActive())
	{
		return 0.f;
	}
	uint32 CurrentTimeMS = FMath::RoundToInt(GetAbilitySystemComponent()->GetCurrentSimulationTimeMS());
	if (StartTimeMS > CurrentTimeMS)
	{
		return 0.f;
	}
	return (CurrentTimeMS - StartTimeMS) * 0.001f;
}

float UWaitForDurationPredictionTask::GetCurrentAlpha() const
{
	if (FMath::IsNearlyZero(GetTotalWaitDuration()))
	{
		return 0.f;
	}
	return GetCurrentWaitDuration() / GetTotalWaitDuration();
}
#pragma endregion

#pragma region Wait For Dynamic Duration Task Class
/////////////////////////////
UWaitForDynamicDurationPredictionTask::UWaitForDynamicDurationPredictionTask(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{
	DataType = FWaitDynamicDurationTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitForDynamicDurationPredictionTask,ExecuteTaskDynamicDuration);
	ShouldTaskTick = true;
}
void UWaitForDynamicDurationPredictionTask::ExecuteTaskDynamicDuration(const float InWaitDuration)
{
	if (IsInRollback())
	{
		return;
	}
	StartTimeMS = GetAbilitySystemComponent()->GetCurrentSimulationTimeMS();
	WaitDurationMS = FMath::RoundToInt(InWaitDuration * 1000.f);
	ActivateTask();
}

uint32 UWaitForDynamicDurationPredictionTask::GetTotalWaitDurationMS() const
{
	return WaitDurationMS;
}

float UWaitForDynamicDurationPredictionTask::GetTotalWaitDuration() const
{
	return GetTotalWaitDurationMS() * 0.001f;
}

float UWaitForDynamicDurationPredictionTask::GetCurrentWaitDuration() const
{
	if (!GetAbilitySystemComponent())
	{
		return 0.f;
	}
	if (!IsActive())
	{
		return 0.f;
	}
	uint32 CurrentTimeMS = FMath::RoundToInt(GetAbilitySystemComponent()->GetCurrentSimulationTimeMS());
	if (StartTimeMS > CurrentTimeMS)
	{
		return 0.f;
	}
	return (CurrentTimeMS - StartTimeMS) * 0.001f;
}

float UWaitForDynamicDurationPredictionTask::GetCurrentAlpha() const
{
	if (FMath::IsNearlyZero(GetTotalWaitDuration()))
	{
		return 0.f;
	}
	return GetCurrentWaitDuration() / GetTotalWaitDuration();
}

void UWaitForDynamicDurationPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	// we wait at least 1 single tick, if Wait Duration <= 0 we wait a tick.
	uint32 TotalWaitDurationMS = GetTotalWaitDurationMS();
	TotalWaitDurationMS = FMath::Max(TotalWaitDurationMS,TimeStep.StepMs);
	const uint32 CurrentDurationMS = TimeStep.BaseSimTimeMs - StartTimeMS;
	// sanity check
	if (IsActive())
	{
		if (CurrentDurationMS >= TotalWaitDurationMS )
		{
			OnWaitEnded.Broadcast();
			if (ShouldTriggerOnce)
			{
				DeactivateTask(false);
			}
			else
			{
				StartTimeMS = TimeStep.BaseSimTimeMs;
			}
		}
	}
	
}

void UWaitForDynamicDurationPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FWaitDynamicDurationTaskData* WaitTaskData = static_cast< FWaitDynamicDurationTaskData*>(DataToWrite.Get());
	WaitTaskData->StartTimeMS = StartTimeMS;
	WaitTaskData->TotalDurationMS = WaitDurationMS;
}

void UWaitForDynamicDurationPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FWaitDynamicDurationTaskData* WaitTaskData = static_cast< const FWaitDynamicDurationTaskData*>(DataToRead.Get());
	StartTimeMS = WaitTaskData->StartTimeMS ;
	WaitDurationMS = WaitTaskData->TotalDurationMS ;
}



FText UWaitForDynamicDurationPredictionTask::GetNodeTitle() const
{
	return  LOCTEXT("WaitPredictionTask", "Wait For Dynamic Duration Prediction Task");
}

FLinearColor UWaitForDynamicDurationPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.694f, 0.8f, 0.325f, 1.0f);
}
#pragma endregion

#undef LOCTEXT_NAMESPACE
