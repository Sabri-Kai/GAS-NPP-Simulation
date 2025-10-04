// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitMovementModeChangedPredictionTask.generated.h"

class UMoverComponent;
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitMovementModeChangeDelegate,FName, PreviousMode , FName , CurrentMode);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitMovementModeChangedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnce = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool MatchCurrentMovementMode = true;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings,meta=(EditCondition="MatchCurrentMovementMode",EditConditionHides))
	FName MovementModeName;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool MatchPreviousMovementMode = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings,meta=(EditCondition="MatchPreviousMovementMode",EditConditionHides))
	FName PreviousMovementModeName;

	UPROPERTY(BlueprintAssignable)
	FWaitMovementModeChangeDelegate OnModeChanged;

	UFUNCTION(BlueprintCallable,Category=WaitMoveModeChange,meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	UFUNCTION()
	void OnMovementModeChanged(const FName& PreviousMode,const FName& NewMode);

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	
	UMoverComponent* GetMoverComponent() const;
};
