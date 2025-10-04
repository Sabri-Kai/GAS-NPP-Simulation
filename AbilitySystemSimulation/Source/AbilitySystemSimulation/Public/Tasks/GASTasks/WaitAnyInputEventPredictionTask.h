// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/AbilitiesDataTypes.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitAnyInputEventPredictionTask.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInputEvent,const FAbilityActivationTrigger& , InputTrigger);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitAnyInputEventPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	//The task Will End When Triggered Input Event Happens
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	bool EndOnFirstTriggered = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Settings)
	TArray<FAbilityActivationTrigger> InputTriggers;
	
	UPROPERTY(BlueprintAssignable)
	FOnInputEvent OnInputEvent;
	
	UFUNCTION(BlueprintCallable,Category=WaitInputEvent,meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;
	
	UFUNCTION()
	void OnInputEventCalled(const UInputAction* TriggerInputAction,const ETriggerEvent& TriggerEvent);

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	FDelegateHandle OnInputEventDelegateHandle;
};
