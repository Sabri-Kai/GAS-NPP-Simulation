// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TargetingPredictionTask.h"
#include "WaitInputTargetingPredictionTask.generated.h"

/**
 * 
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitInputTargetingPredictionTask : public UTargetingPredictionTask
{
	GENERATED_UCLASS_BODY()

	/*
	 * When Triggered Will call Confirm Targeting on the processor
	 */
	UPROPERTY(EditDefaultsOnly,Category="Input")
	FAbilityActivationTrigger ConfirmInput;
	/*
	* When Triggered Will call Cancel Targeting on the processor.
	* Will not necessarily end the task if processor doesn't.
	*/
	UPROPERTY(EditDefaultsOnly,Category="Input")
	FAbilityActivationTrigger CancelInput;

	
	virtual void ExecuteTask(FVector InLocation, FVector InDirection, FRotator InRotation) override;
	
	UFUNCTION()
	void OnInputEventCalled(const UInputAction* TriggerInputAction,const ETriggerEvent& TriggerEvent);

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual FText GetNodeTitle() const override;

private:
	FDelegateHandle OnInputEventDelegateHandle;
};
