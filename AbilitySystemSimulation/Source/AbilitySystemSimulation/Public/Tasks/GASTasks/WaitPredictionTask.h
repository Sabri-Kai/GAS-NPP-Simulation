// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitPredictionTask.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWaitEnded);
// Duration For This Class Can Be Changed At Runtime
UCLASS(BlueprintType)
class ABILITYSYSTEMSIMULATION_API UWaitForDurationPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()
	

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WaitTask",meta=(Units="Seconds"))
	float WaitDuration = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="WaitTask")
	bool ShouldTriggerOnce = true;

	UPROPERTY(BlueprintAssignable)
	FOnWaitEnded OnWaitEnded;
	

	UFUNCTION(BlueprintCallable, Category="WaitTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual uint32 GetTotalWaitDurationMS() const;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetTotalWaitDuration() const ;
	
	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetCurrentWaitDuration() const ;

	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetCurrentAlpha() const ;

private:

	UPROPERTY()
	uint32 StartTimeMS = 0;
};




// Duration For This Class Can Be Changed At Runtime
UCLASS(Blueprintable, BlueprintType)
class ABILITYSYSTEMSIMULATION_API UWaitForDynamicDurationPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="WaitTask")
	bool ShouldTriggerOnce = true;

	UPROPERTY(BlueprintAssignable)
	FOnWaitEnded OnWaitEnded;
	
	
	UFUNCTION(BlueprintCallable, Category="WaitTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskDynamicDuration(float InWaitDuration);

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void ReadFromSyncedData(TSharedPtr< const FAbilityTaskDataBase> DataToRead) override;
	// this exists so you easily subclass from this task and override the duration
	// should use a synced value like a calculation based on tags or attributes.
	virtual uint32 GetTotalWaitDurationMS() const;

	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetTotalWaitDuration() const ;
	
	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetCurrentWaitDuration() const ;

	UFUNCTION(BlueprintPure,category="WaitTask")
	float GetCurrentAlpha() const ;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	UPROPERTY()
	uint32 WaitDurationMS = 0;

	UPROPERTY()
	uint32 StartTimeMS = 0;
};


