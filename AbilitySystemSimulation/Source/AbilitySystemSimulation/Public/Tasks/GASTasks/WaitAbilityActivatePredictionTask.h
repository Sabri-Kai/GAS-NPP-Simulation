// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitAbilityActivatePredictionTask.generated.h"

/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitAbilityDelegate, UGameplayAbility*, ActivatedAbility);

UENUM(BlueprintType)
enum EWaitAbilityType : uint8
{
	EWithOrWithoutTag,
	EWithTagRequirements,
	EWithQuery
};

UCLASS(BlueprintType)
class ABILITYSYSTEMSIMULATION_API UWaitAbilityActivatePredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	bool TriggerOnce = true;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings")
	TEnumAsByte<EWaitAbilityType> ConditionType = EWithOrWithoutTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings",meta=(EditCondition = "ConditionType == EWaitAbilityType::EWithOrWithoutTag"))
	FGameplayTag WithTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings",meta=(EditCondition = "ConditionType == EWaitAbilityType::EWithOrWithoutTag"))
	FGameplayTag WithoutTag;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings",meta=(EditCondition = "ConditionType == EWaitAbilityType::EWithTagRequirements"))
	FGameplayTagRequirements TagRequirements;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Settings",meta=(EditCondition = "ConditionType == EWaitAbilityType::EWithQuery"))
	FGameplayTagQuery Query;

	UPROPERTY(BlueprintAssignable)
	FWaitAbilityDelegate OnAbilityActivated;
	
	UFUNCTION(BlueprintCallable, Category="WaitTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();

	UFUNCTION()
	void OnAbilityActivate(UGameplayAbility *ActivatedAbility);

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
private:
	virtual void OnPreDeactivate(const bool& bWasCancelled) override;

	FDelegateHandle OnAbilityActivateDelegateHandle;
};
