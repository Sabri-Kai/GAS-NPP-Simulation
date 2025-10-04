// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "ProjectilesSimulator/SyncedProjectileBase.h"
#include "Tasks/BasePredictionTask.h"
#include "SpawnProjectileAndWaitPredictionTask.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSpawnProjectileTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSpawnProjectileTaskData() override {}

	UPROPERTY()
	uint32 ProjectileID = 0;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};
UCLASS(Blueprintable)
class ABILITYSYSTEMSIMULATION_API USpawnProjectileAndWaitPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()
	
	UPROPERTY()
	uint32 ProjectileID = 0;
	
	UFUNCTION(BlueprintCallable, Category="RepeatActionTask",meta=(BlueprintInternalUseOnly = "TRUE"))
	void ExecuteTask(TSubclassOf<ASyncedProjectileBase> ProjectileClass ,FVector SpawnLocation, FVector Direction);
	
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;
	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

	virtual void OnPreDeactivate(const bool& bWasCanceled) override;

	UFUNCTION(BlueprintPure, Category=RepeatTask)
	ASyncedProjectileBase* GetSpawnedProjectileInstance() const ;

	UFUNCTION()
	void OnProjectileExplode(const FHitBroadcastData& Data , const uint32& ID);
	UFUNCTION()
	void OnProjectileBounce(const FHitBroadcastData& Data , const uint32& ID);
	UFUNCTION()
	void OnProjectilePierce(const FHitBroadcastData& Data , const uint32& ID);
	UFUNCTION()
	void OnProjectileEndOfLife(const FHitBroadcastData& Data , const uint32& ID);


	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnBounce;
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnPierce;
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnExplode;
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnEndOfLife;

private:

	FDelegateHandle OnProjectileExplodeHandle;
	FDelegateHandle OnProjectileBounceHandle;
	FDelegateHandle OnProjectilePierceHandle;
	FDelegateHandle OnProjectileEndOfLifeHandle;
};

