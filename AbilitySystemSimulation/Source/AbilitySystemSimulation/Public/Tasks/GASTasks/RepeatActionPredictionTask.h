// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "RepeatActionPredictionTask.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FRepeatActionTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FRepeatActionTaskData() override {}

	UPROPERTY()
	uint32 CurrentCounter = 0;

	UPROPERTY()
	uint32 AccumulatedTimeMS = 0;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionTriggered,int32,CurrentCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFinished);
UCLASS(BlueprintType)
class ABILITYSYSTEMSIMULATION_API URepeatActionPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	float TimeBetweenActions = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	int32 TotalActionCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	bool bTriggerFirstActionWithoutDelay = true;

	UPROPERTY(BlueprintAssignable)
	FOnActionTriggered OnActionTriggered;
	
	UPROPERTY(BlueprintAssignable)
	FOnFinished OnFinished;
	
	UFUNCTION(BlueprintCallable, Category="RepeatActionTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();
	
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	UFUNCTION(BlueprintPure, Category=RepeatTask)
	int32 GetCurrentCounter() const {return CurrentCounter;}

	UFUNCTION(BlueprintPure, Category=RepeatTask)
	int32 GetTotalCounter() const {return TotalActionCount;}

	
private:
	UPROPERTY()
	uint32 CurrentCounter = false;

	UPROPERTY()
	uint32 AccumulatedTimeMS = false;
};
