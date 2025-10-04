// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "PlayMontageAndWaitPredictionTask.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FPlayMontageTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FPlayMontageTaskData() override {}

	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToPlay = nullptr;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};
// the simplest form of this task , plays the montage and listens for the montage events
// it listens for all events while it is active and get deactivated when canceled or completed is triggered
// or if EndOnBlendOut is set to true. this uses events coming from montage player in NP ASC , which is in the simulation tick
//NOTE: BlendOut Doesn't Get Called when a montage is canceled/interrupted, Currently canceled/interrupted does not get called if already blending out.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMontageWaitDelegate);
UCLASS(BlueprintType)
class ABILITYSYSTEMSIMULATION_API UPlayMontageAndWaitPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool StopMontageIfAbilityEnds = true;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool StopMontageIfAbilityCancels = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool EndTaskOnBlendOut = false;
	
	// called when montage starts blending out, is also called when montage gets Interrupted or canceled
	UPROPERTY(BlueprintAssignable)
	FMontageWaitDelegate OnMontageBlendOut;

	UPROPERTY(BlueprintAssignable)
	FMontageWaitDelegate OnMontageCanceled;

	UPROPERTY(BlueprintAssignable)
	FMontageWaitDelegate OnMontageCompleted;

	// This Is Called If Task Is Active (Montage did not blend out or complete) and ability is canceled, or task is canceled from ability directly.
	UPROPERTY(BlueprintAssignable)
	FMontageWaitDelegate OnTaskCanceled;

	UFUNCTION(BlueprintPure,Category=PlayMontageTask)
	UAnimMontage* GetMontage() const;

	UFUNCTION(BlueprintCallable,Category=PlayMontageTask,meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask(UAnimMontage* AnimMontage,float StartTime,float PlayRate = 1.f,FName SectionName = NAME_None,float RootMotionScale = 1.f);

	virtual void SetInitialData(UAnimMontage* AnimMontage);
	UFUNCTION()
	virtual void OnBlendOut(UAnimMontage* Montage);

	UFUNCTION()
	virtual void OnCanceled(UAnimMontage* Montage);

	UFUNCTION()
	virtual void OnCompleted(UAnimMontage* Montage);
	

	FDelegateHandle OnBlendOutHandle;
	FDelegateHandle OnCanceledHandle;
	FDelegateHandle OnCompletedHandle;

#pragma region Prediction Task API

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void OnPreDeactivate(const bool& bWasCanceled) override;
	virtual void OnOwningAbilityEnded(const bool& bWasCanceled) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
#pragma endregion

protected:
	virtual void UnBindDelegates();
	virtual void BindDelegates();
	UPROPERTY()
	TObjectPtr<UAnimMontage> CachedMontageToPlay = nullptr;
	
};
