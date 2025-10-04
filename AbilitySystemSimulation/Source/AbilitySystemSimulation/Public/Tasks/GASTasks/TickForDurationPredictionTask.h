// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WaitPredictionTask.h"
#include "TickForDurationPredictionTask.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDurationTick,  float, DeltaTime, float , CurrentDuration);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UTickForDurationPredictionTask : public UWaitForDurationPredictionTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FOnDurationTick OnTick;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual FText GetNodeTitle() const override;
};

UCLASS()
class ABILITYSYSTEMSIMULATION_API UTickForDynamicDurationPredictionTask : public UWaitForDynamicDurationPredictionTask
{
	GENERATED_BODY()

	UPROPERTY(BlueprintAssignable)
	FOnDurationTick OnTick;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual FText GetNodeTitle() const override;
};