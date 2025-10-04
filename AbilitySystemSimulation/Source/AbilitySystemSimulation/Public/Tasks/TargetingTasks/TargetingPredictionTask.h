// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Tasks/BasePredictionTask.h"
#include "TargetingPredictionTask.generated.h"

/**
 * 
 */

class UTargetingProcessor;

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FBaseTargetingTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FBaseTargetingTaskData() override {}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	FVector_NetQuantize10 Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	FVector_NetQuantize Direction = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	TArray<AActor*> SavedHitActors;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	float StartSimTime = 0.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Targeting)
	float LastConfirmationTime = 0.f;
	

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSuccess,const FGameplayAbilityTargetDataHandle&,TargetData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOngoing,const FGameplayAbilityTargetDataHandle&,TargetData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEnded,bool, Canceled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPreTargeting,ETargetingEvent, Event,UTargetingPredictionTask*, Task);

UCLASS()
class ABILITYSYSTEMSIMULATION_API UTargetingPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	
	UPROPERTY(EditDefaultsOnly,Category=Targeting,Instanced)
	TObjectPtr<UTargetingProcessor> TargetingProcessor = nullptr;

	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|LagCompensation")
	bool bEnableLagCompensation = true;
	
	// Making this virtual , so children can add logic to it if they want.
	// if they just want to override it, they can just use their own StartFunctionName 
	// with different inputs if desired.
	UFUNCTION(BlueprintCallable, Category="Targeting",meta=(BlueprintInternalUseOnly = "TRUE"))
	virtual void ExecuteTask(FVector InLocation,FVector InDirection , FRotator InRotation);
	/*
	 * Called by Task input execution pins or children of this task calling it manually
	 * such as UWaitInputTargetingPredictionTask
	 * Will not necessarily end the task if processor doesn't.
	 */
	UFUNCTION(BlueprintCallable, Category="Targeting",meta=(BlueprintInternalUseOnly = "TRUE"))
	virtual void ConfirmTargeting();
	/*
	 * Called by Task input execution pins or children of this task calling it manually
	 * such as UWaitInputTargetingPredictionTask
	 * Will not necessarily end the task if processor doesn't.
	 */
	UFUNCTION(BlueprintCallable, Category="Targeting",meta=(BlueprintInternalUseOnly = "TRUE"))
	virtual void CancelTargeting();
	

	UFUNCTION(BlueprintPure,category=Targeting)
	const TArray<AActor*>& GetSavedHitActors() const;

	UFUNCTION(BlueprintPure,category=Targeting)
	float GetCurrentTaskDurationMS() const;

	/*
	 * This return a negative value if confirmation did not yet happen.
	 */
	UFUNCTION(BlueprintPure,category=Targeting)
	float GetTimeSinceLastConfirmMS() const;

	UFUNCTION(BlueprintPure,category=Targeting)
	bool DidConfirmationTrigger() const;

	UFUNCTION(BlueprintPure,category=Targeting)
	float GetProcessorMaxDurationMS() const;

	/*
	 * Intended to be called on Pre Targeting, but you can call if from anywhere in the ability
	 */
	UFUNCTION(BlueprintCallable,category=Targeting)
	void UpdateLocation(FVector NewLocation);
	/*
	* Intended to be called on Pre Targeting, but you can call if from anywhere in the ability
	 */
	UFUNCTION(BlueprintCallable,category=Targeting)
	void UpdateRotation(FRotator NewRotation);
	/*
	 * Called everytime before targeting happens passing in the event that will occur
	 * and the task instance object.
	 * this is mainly to provide an easy way of updating the targeting location and rotation
	 * to arbitrary values from withing the ability.
	 */
	UPROPERTY(BlueprintAssignable)
	FPreTargeting OnPreTargeting;
	/*
	 * Call back triggered whenever targeting is successful
	 * IsConfirmed is true if this callback is from a ConfirmTargeting call.
	 */
	UPROPERTY(BlueprintAssignable)
	FSuccess OnConfirmedSuccess;
	/*
	 * Triggered callback when CancelTargeting is called. 
	 */
	UPROPERTY(BlueprintAssignable)
	FOngoing OnOngoingSuccess;
	
	UPROPERTY(BlueprintAssignable)
	FEnded OnEnded;

	
	void HandleTargetingResult(const ETargetingResult& Result,const FGameplayAbilityTargetDataHandle& TargetHandle);
	// this is runtime data that can change , it is synced.
	// maybe we can extend this to be a handle that hold a shared ptr of targeting synced data
	// so it can be easily extended?

	UPROPERTY(BlueprintReadWrite,Category="Targeting")
	FTargetingData TargetingData;

	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep) override;
	
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};





