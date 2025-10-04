// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitAttributeChangePredictionTask.generated.h"

struct FOnAttributeChangeData;
/**
 * 
 */
UENUM(BlueprintType)
enum  EWaitAttributeChangedComparison : uint8
{
	None,
	GreaterThan,
	LessThan,
	GreaterThanOrEqualTo,
	LessThanOrEqualTo,
	NotEqualTo,
	ExactlyEqualTo,
	MAX UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FWaitAttributeChangeDelegate,const float&,OldValue,const float&,CurrentValue);

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitOwnerAttributeChangePredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()


	UPROPERTY(BlueprintAssignable)
	FWaitAttributeChangeDelegate	OnChange;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings)
	TEnumAsByte<EWaitAttributeChangedComparison> ComparisonType = None;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings)
	FGameplayTag WithTag;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings)
	FGameplayTag WithoutTag;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings)
	FGameplayAttribute	Attribute;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings,meta=(EditCondition = "ComparisonType != EWaitAttributeChangedComparison::None"))
	float ComparisonValue = 0.f;
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category= Settings)
	bool bTriggerOnce = true;
	
	FDelegateHandle OnAttributeChangeDelegateHandle;

	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask();
	
	UFUNCTION()
	virtual void OnAttributeChange(const FOnAttributeChangeData& CallbackData);

	virtual UAbilitySystemComponent* GetFocusedASC() const;

	virtual void OnPreDeactivate(const bool& bWasCancelled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
};

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitTargetAttributeChangePredictionTask : public UWaitOwnerAttributeChangePredictionTask
{
	GENERATED_UCLASS_BODY()
	

	// new execution/start function
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* TargetActor = nullptr);
	
	virtual UAbilitySystemComponent* GetFocusedASC() const override;
	// Synced Data API
	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
};



