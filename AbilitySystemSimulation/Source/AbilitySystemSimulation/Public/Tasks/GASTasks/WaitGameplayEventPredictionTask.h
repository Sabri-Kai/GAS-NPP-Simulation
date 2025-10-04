// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitGameplayEventPredictionTask.generated.h"

class UAbilitySystemComponent;
/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayEventPredictionDelegate, FGameplayEventData, Payload);

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayEventPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()


	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnce = true;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	FGameplayTag EventTag;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool OnlyMatchExact = true;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEventPredictionDelegate EventReceived;
	
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* ExternalTarget = nullptr);

	virtual void GameplayEventCallback(const FGameplayEventData* Payload);
	virtual void GameplayEventContainerCallback(FGameplayTag MatchingTag, const FGameplayEventData* Payload);

	UAbilitySystemComponent* GetFocusedASC();

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	FDelegateHandle WaitEventHandle;
	
private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
};
