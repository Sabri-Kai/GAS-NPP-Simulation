// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "TickPredictionTask.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTick, const float&, DeltaTime);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UTickPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()


	UPROPERTY(BlueprintAssignable)
	FOnTick OnTick;

	UFUNCTION(BlueprintCallable,BlueprintInternalUseOnly)
	void ExecuteTask();

	UFUNCTION()
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	
};

