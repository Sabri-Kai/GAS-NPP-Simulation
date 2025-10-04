// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/NpGameplayAbility.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitInputEventPredictionTask.generated.h"

class UInputAction;
/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitInputEventDelegate);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitInputEventPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	//The task Will End When Completed Or Canceled Input Events Happen
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	bool EndOnCompletedOrCanceled = true;

	//The task Will End When Triggered Input Event Happens
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	bool EndOnTriggered = true;

	/*
	 * Use What ever input action event that activated owning ability.
	 * since ability can have multiple activation input triggers, the one that did activate
	 * will be used in this task.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	bool UseAbilityInputAction = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings,meta=(EditCondition = "UseAbilityInputAction == false"))
	TObjectPtr<UInputAction> InputAction;
	
	UPROPERTY(BlueprintAssignable)
	FWaitInputEventDelegate Triggered;

	UPROPERTY(BlueprintAssignable)
	FWaitInputEventDelegate Started;

	UPROPERTY(BlueprintAssignable)
	FWaitInputEventDelegate Ongoing;

	UPROPERTY(BlueprintAssignable)
	FWaitInputEventDelegate Completed;

	UPROPERTY(BlueprintAssignable)
	FWaitInputEventDelegate Canceled;
	
	UFUNCTION(BlueprintCallable,Category=WaitInputEvent,meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;
	
	UFUNCTION()
	void OnInputEvent(const UInputAction* TriggerInputAction,const ETriggerEvent& TriggerEvent);

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	UInputAction* GetInputAction() const;

private:
	FDelegateHandle OnInputEventDelegateHandle;
};
