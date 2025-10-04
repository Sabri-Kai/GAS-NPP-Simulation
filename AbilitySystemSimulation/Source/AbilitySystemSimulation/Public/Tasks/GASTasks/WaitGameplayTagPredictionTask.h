// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tasks/BasePredictionTask.h"
#include "WaitGameplayTagPredictionTask.generated.h"

class UAbilitySystemComponent;
/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FWaitGameplayTagPredictionDelegate);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayTagAddedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnce = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnStartIfPresentTag = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	FGameplayTag GameplayTag;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagPredictionDelegate Added;
	
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* ExternalTarget = nullptr);

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);

	UAbilitySystemComponent* GetFocusedASC();

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	FDelegateHandle WaitTagHandle;

private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
};

// if it's just a copy of the Added class but instead waits for removed. this exists as a copy just for the sake of having 2 separate tasks for added and removed

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayTagARemovedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnce = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnStartIfAbsentTag = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	FGameplayTag GameplayTag;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagPredictionDelegate Removed;
	
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* ExternalTarget = nullptr);

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);

	UAbilitySystemComponent* GetFocusedASC() const;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	FDelegateHandle WaitTagHandle;
private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
};


USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FWaitTagAddedThenRemovedTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FWaitTagAddedThenRemovedTaskData() override {}

	UPROPERTY()
	TObjectPtr<AActor> ExternalTarget = nullptr;
	UPROPERTY()
	bool bAlreadyAdded = false;

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
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};

UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayTagAddedThenRemovedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool TriggerOnce = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	bool WaitRemovedOnStartIfPresentTag = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	FGameplayTag GameplayTag;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagPredictionDelegate Removed;
	
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* ExternalTarget = nullptr);

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);

	UAbilitySystemComponent* GetFocusedASC() const;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	FDelegateHandle WaitTagHandle;
private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
	UPROPERTY()
	bool bAlreadyAdded = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWaitGameplayTagCountPredictionDelegate,int32 ,NewCount);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UWaitGameplayTagCountChangedPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=Settings)
	FGameplayTag GameplayTag;

	UPROPERTY(BlueprintAssignable)
	FWaitGameplayTagCountPredictionDelegate TagCountChanged;
	
	UFUNCTION(BlueprintCallable, Category="AttributeChangedTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTaskWithTarget(AActor* ExternalTarget = nullptr);

	virtual void GameplayTagCallback(const FGameplayTag Tag, int32 NewCount);

	UAbilitySystemComponent* GetFocusedASC() const;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	
	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	FDelegateHandle WaitTagHandle;
private:
	UPROPERTY()
	TObjectPtr<AActor> CachedExternalTarget = nullptr;
};
