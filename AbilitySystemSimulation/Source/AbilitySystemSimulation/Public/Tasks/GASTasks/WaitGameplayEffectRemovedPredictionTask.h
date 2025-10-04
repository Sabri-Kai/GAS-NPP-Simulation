// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEffectRemoved.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitGameplayEffectRemovedPredictionTask.generated.h"

class UAbilitySystemComponent;
/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FWaitGameplayEffectRemovedTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FWaitGameplayEffectRemovedTaskData() override {}

	UPROPERTY()
	int32 Handle = INDEX_NONE;

	UPROPERTY()
	bool bIsSelf = true;
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> TargetASC = nullptr;
	
	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	/** Gets the type info of this FAbilityDataStructBase. MUST be overridden by derived types. */
	virtual UScriptStruct* GetScriptStruct() const override;
	/** Get string representation of this struct instance */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	/** Checks if the contained data is equal, within reason. MUST be override by types that compose STATE data (sync or aux).
	 *   AuthorityState is guaranteed to be the same concrete type as 'this'.
	 */
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	/** Interpolates contained data between a starting and ending block. MUST be override by types that compose STATE data (sync or aux).
	 * From and To are guaranteed to be the same concrete type as 'this'.
	 */
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override {};
};

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayEffectRemovedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()
	
	UFUNCTION(BlueprintCallable, Category=GameplayEffectRemovedPredictionTask,meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask(const FActiveGameplayEffectHandle EffectHandle);
	
	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectRemovedDelegate	OnRemoved;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayEffectRemovedDelegate	InvalidHandle;

	UFUNCTION()
	void OnGameplayEffectRemoved(const FGameplayEffectRemovalInfo& InGameplayEffectRemovalInfo);

	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual void OnPreDeactivate(const bool& bWasCanceled) override;
	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
private:
	
	FDelegateHandle OnGameplayEffectRemovedDelegateHandle;
	FActiveGameplayEffectHandle Handle;
	bool bIsTargetAscOwner = true;
};
