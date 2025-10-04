// 2025 Yohoho Productions /  Sirkai


#include "Tasks/GASTasks/TickPredictionTask.h"

#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Tasks/GASTasks/CommonDataTypes.h"

#define LOCTEXT_NAMESPACE "TickTask"

UTickPredictionTask::UTickPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UTickPredictionTask,ExecuteTask);
	ShouldTaskTick = true;
}
void UTickPredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	ActivateTask();
}

void UTickPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	OnTick.Broadcast(TimeStep.StepMs/1000.f);
}

FText UTickPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("TickPredictionTask", "Tick Prediction Task");
}

FLinearColor UTickPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.694f, 0.8f, 0.325f, 1.0f);
}



#undef LOCTEXT_NAMESPACE