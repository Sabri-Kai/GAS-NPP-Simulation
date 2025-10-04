// 2025 Yohoho Productions /  Sirkai


#include "Tasks/GASTasks/DynamicBindPredictionTask.h"


#define LOCTEXT_NAMESPACE "Dynamic Bind Task"

#pragma region Binding Task Data

bool FBindingTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	Ar.SerializeBits(&bIsBound,1);
	return true;
}

bool FBindingTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	Ar.SerializeBits(&bIsBound,1);
	return true;
}

UScriptStruct* FBindingTaskData::GetScriptStruct() const
{
	return FBindingTaskData::StaticStruct();
}

void FBindingTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Is Bound : %s" , bIsBound ? TEXT("true") :  TEXT("false"));
}

bool FBindingTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FBindingTaskData* AuthState = static_cast<const FBindingTaskData*>(&AuthorityState);
	const bool DiffBinding = bIsBound != AuthState->bIsBound;
	return DiffBinding;
}

void FBindingTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FBindingTaskData* ToState = static_cast<const FBindingTaskData*>(&To);
	bIsBound = ToState->bIsBound;
}

#pragma endregion

#pragma region Binding Task Class

UDynamicBindPredictionTask::UDynamicBindPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FBindingTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UDynamicBindPredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}

void UDynamicBindPredictionTask::ExecuteTask()
{
	if (TriggerBindOnExecution)
	{
		TriggerBind();
	}
	ActivateTask();
}

void UDynamicBindPredictionTask::TriggerBind()
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (!bIsBound)
	{
		Bind.Broadcast();
		bIsBound = true;
	}
}

void UDynamicBindPredictionTask::TriggerUnBind()
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (bIsBound)
	{
		UnBind.Broadcast();
		bIsBound = false;
	}
}

void UDynamicBindPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	const FBindingTaskData* AuthorityBindingTaskData = static_cast<const FBindingTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
	if (AuthorityBindingTaskData->bIsBound != bIsBound)
	{
		if (AuthorityBindingTaskData->bIsBound)
		{
			Bind.Broadcast();
		}
		else
		{
			UnBind.Broadcast();
		}
	}
}

void UDynamicBindPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FBindingTaskData* BindingTaskData = static_cast<const FBindingTaskData*>(DataToRead.Get());
	bIsBound = BindingTaskData->bIsBound;
}

void UDynamicBindPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FBindingTaskData* BindingTaskData = static_cast< FBindingTaskData*>(DataToWrite.Get());
	BindingTaskData->bIsBound = bIsBound ;
}

FText UDynamicBindPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("DynamicBindPredictionTask", "Dynamic Bind Prediction Task");
}

FLinearColor UDynamicBindPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.086f, 0.086f, 0.329f, 1.0f);;
}
#pragma endregion
#undef LOCTEXT_NAMESPACE