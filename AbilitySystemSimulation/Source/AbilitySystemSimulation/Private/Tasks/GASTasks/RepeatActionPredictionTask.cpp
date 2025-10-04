// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/RepeatActionPredictionTask.h"

#include "DataTypes/AbilitySimulationDataTypes.h"

#define LOCTEXT_NAMESPACE "RepeatActionTask"

#pragma region Repeat Action Task Data

bool FRepeatActionTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	Params.Ar.SerializeIntPacked(CurrentCounter);
	Params.Ar.SerializeIntPacked(AccumulatedTimeMS);
	return true;
}

bool FRepeatActionTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FRepeatActionTaskData* BaseData = Params.GetBaseDeltaState<FRepeatActionTaskData>();
	check(BaseData);
	bool bSameCounter = Params.Ar.IsSaving() ? CurrentCounter == BaseData->CurrentCounter : false;
	Params.Ar.SerializeBits(&bSameCounter, 1);
	if (bSameCounter)
	{
		CurrentCounter = BaseData->CurrentCounter;
	}
	else
	{
		Params.Ar.SerializeIntPacked(CurrentCounter);
	}

	bool bSameTime = Params.Ar.IsSaving() ? AccumulatedTimeMS == BaseData->AccumulatedTimeMS : false;
	Params.Ar.SerializeBits(&bSameTime, 1);
	if (bSameTime)
	{
		AccumulatedTimeMS = BaseData->AccumulatedTimeMS;
	}
	else
	{
		Params.Ar.SerializeIntPacked(AccumulatedTimeMS);
	}
	
	return true;
}

UScriptStruct* FRepeatActionTaskData::GetScriptStruct() const
{
	return FRepeatActionTaskData::StaticStruct();
}

void FRepeatActionTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Counter : %d",CurrentCounter);
	Out.Appendf("Accumulated Time : %f",AccumulatedTimeMS / 1000.f);
}

bool FRepeatActionTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FRepeatActionTaskData* AuthState = static_cast<const FRepeatActionTaskData*>(&AuthorityState);
	const bool DiffCounter = CurrentCounter != AuthState->CurrentCounter;
	const bool DiffAccumulatedTime = AccumulatedTimeMS != AuthState->AccumulatedTimeMS;
	return DiffCounter || DiffAccumulatedTime;
}

void FRepeatActionTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FRepeatActionTaskData* FromState = static_cast<const FRepeatActionTaskData*>(&From);
	const FRepeatActionTaskData* ToState = static_cast<const FRepeatActionTaskData*>(&To);
	CurrentCounter = FMath::Lerp(FromState->CurrentCounter, ToState->CurrentCounter, Pct);
	AccumulatedTimeMS = FMath::Lerp(FromState->AccumulatedTimeMS, ToState->AccumulatedTimeMS, Pct);
}

#pragma endregion

#pragma region  Reapeat Action Task Class
URepeatActionPredictionTask::URepeatActionPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = FRepeatActionTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(URepeatActionPredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}
void URepeatActionPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	AccumulatedTimeMS = 0;
	CurrentCounter = 0;
	ActivateTask();
}

void URepeatActionPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	AccumulatedTimeMS += TimeStep.StepMs;
	
	if (bTriggerFirstActionWithoutDelay && CurrentCounter == 0)
	{
		CurrentCounter++;
		OnActionTriggered.Broadcast(CurrentCounter);
		if (CurrentCounter == TotalActionCount)
		{
			OnFinished.Broadcast();
			DeactivateTask(false);
			AccumulatedTimeMS = 0;
			CurrentCounter = 0;
		}
		return;
	}
	
	const uint32 ActionsCount = FMath::Floor(TotalActionCount);
	const uint32 TimeBetweenActionsMS = FMath::Floor(TimeBetweenActions * 1000.f);
	if (AccumulatedTimeMS >= TimeBetweenActionsMS && CurrentCounter < ActionsCount && ShouldTriggerCallbacks())
	{
		CurrentCounter++;
		OnActionTriggered.Broadcast(CurrentCounter);
		if (CurrentCounter == TotalActionCount)
		{
			OnFinished.Broadcast();
			DeactivateTask(false);
			AccumulatedTimeMS = 0;
			CurrentCounter = 0;
		}
		else
		{
			AccumulatedTimeMS -= TimeBetweenActionsMS;
		}
	}
}

void URepeatActionPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FRepeatActionTaskData* WaitTaskData = static_cast<const FRepeatActionTaskData*>(DataToRead.Get());
	CurrentCounter = WaitTaskData->CurrentCounter;
	AccumulatedTimeMS = WaitTaskData->AccumulatedTimeMS;
}

void URepeatActionPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FRepeatActionTaskData* WaitTaskData = static_cast< FRepeatActionTaskData*>(DataToWrite.Get());
	WaitTaskData->CurrentCounter = CurrentCounter ;
	WaitTaskData->AccumulatedTimeMS = AccumulatedTimeMS ;
}

FText URepeatActionPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("RepeatActionPredictionTask", "Repeat Action Prediction Task");
}

FLinearColor URepeatActionPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.694f, 0.8f, 0.325f, 1.0f);
}
#pragma endregion

#undef LOCTEXT_NAMESPACE
