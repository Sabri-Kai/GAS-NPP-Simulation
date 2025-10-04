// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Abilities/GameplayAbility.h"
#include "DataTypes/AbilitiesDataTypes.h"
#include "DataTypes/BaseSyncedVariableData.h"
#include "StructUtils/InstancedStruct.h"
#include "NpGameplayAbility.generated.h"

struct FAbilitySystemTimeStep;
enum class ETriggerEvent : uint8;
class UInputAction;
class UBasePredictionTask;
/**
 * This is the base gameplay ability class that must be used with NpGameplayAbilitySystem. any abilities that are not children of this
 * will be rejected and won't get added.
 *
 * the reason for this class:
 * 1 - Overrides all the ability related functionality to ensure client can predict anything
 * 2 - implements Prediction tasks, each ability instances gets its tasks instances ,manages them, simulating them , rolling them back. (details in BasePredictionTask.h)
 * you can find more 
 * 3 - implements synced variables which are variables of a specific struct type in the ability that will be synced and rolled back. (Details in BasedSyncedVariableData.h)
 *
 * All rollback and simulation functionality of this ability is handled by NpGameplayAbilitySystem component. which calls restore frame and simulation tick.
 * When it comes to simulation an instance of an ability is represented by FActivatableAbilitySyncState ()
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UNpGameplayAbility : public UGameplayAbility
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Ability|Activation")
	TArray<FAbilityActivationTrigger> ActivationInputs;

	//Returns False if ability was not activated by input trigger
	UFUNCTION(BlueprintPure,Category=Inputs)
	bool GetActivatedByTrigger(FAbilityActivationTrigger& OutTrigger);

	UFUNCTION(BlueprintCallable,Category=PredictionTasks)
	bool CancelPredictionTaskByName(UPARAM(meta=(GetOptions="GetValidTaskNames"))FName TaskName);

	UFUNCTION(BlueprintPure,Category=Inputs)
	UNpAbilitySystemComponent* GetNpAbilitySystemComponent() const;
	UPROPERTY()
	int8 ActivatedByInputActionIndex = INDEX_NONE;

	/** used to check if we should sync the event data in network prediction replication or not */
	UPROPERTY()
	bool bHasEventData = false;

	// Called to Roll back an Ability Instance to a specific state, this should include variables ,
	// and delegate bindings states (Bound/Unbound) if Ability has delegate bindings outside of tasks.
	void RestoreFrame(const FActiveAbilityInstanceData& AuthoritySyncData);
	void SimulationTick(const FAbilitySystemTimeStep& TimeStep);

	// This Is tIcked After The ability prediction Tasks have been evaluated for this frame
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep);

	// This Triggers when ability activated from a restore frame after the tasks and synced vars are restored,
	// so all ability is in the correct authority state
	UFUNCTION()
	virtual void OnAbilityRestored();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable,category=AbilitySim)
	void K2_OnAbilityRestored();
	
	virtual void StartAbilityRollback(const FActiveAbilityInstanceData& AuthorityTaskData);

	void GetSyncedVars(TArray<FSyncVarDef>& OutSyncedVars) const;

	UPROPERTY()
	TArray<FName> InitializationFunctionNames;
protected:
	
	//ToDo @Kai : It Is Possible To Also Reset The Synced Vars To Their Default Values.
	// but now just like default blueprint variable, the instance of the ability if instanced per actor doesn't reset synced vars on reactivation.
	void RestoreSyncedVariables(const FSyncVarCollection& SyncedVars);
	bool ValidatePreRollbackFunction(UFunction* Function, FStructProperty* SyncProperty) const;

#pragma region UGameplayAbility Overrides
public:
	virtual bool ShouldActivateAbility(ENetRole Role) const override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo
		, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo
		, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo
		, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate
		, const FGameplayEventData* TriggerEventData) override;
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectToOwner(const FGameplayAbilitySpecHandle Handle
		, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo
		, const UGameplayEffect* GameplayEffect, float GameplayEffectLevel = 1.f, int32 Stacks = 1) const override;
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToOwner(const FGameplayAbilitySpecHandle AbilityHandle\
		, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo
		, const FGameplayEffectSpecHandle SpecHandle) const override;
	virtual TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectToTarget(const FGameplayAbilitySpecHandle Handle
		, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo
		, const FGameplayAbilityTargetDataHandle& Target, TSubclassOf<UGameplayEffect> GameplayEffectClass
		, float GameplayEffectLevel = 1.f, int32 Stacks = 1) const override;
	virtual TArray<FActiveGameplayEffectHandle> ApplyGameplayEffectSpecToTarget(const FGameplayAbilitySpecHandle AbilityHandle
		, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo
		, const FGameplayEffectSpecHandle SpecHandle, const FGameplayAbilityTargetDataHandle& TargetData) const override;
	virtual void BP_RemoveGameplayEffectFromOwnerWithAssetTags(FGameplayTagContainer WithAssetTags, int32 StacksToRemove = -1) override;
	virtual void BP_RemoveGameplayEffectFromOwnerWithGrantedTags(FGameplayTagContainer WithGrantedTags, int32 StacksToRemove = -1) override;
	virtual void BP_RemoveGameplayEffectFromOwnerWithHandle(FActiveGameplayEffectHandle Handle, int32 StacksToRemove = -1) override;


#pragma endregion

#pragma region Restore Ability Functions
	UFUNCTION(BlueprintPure, Category = "AbilitySimulation")
	bool IsRestoringFrame() const;
	void PreForceActivateAbilityInstance(const FGameplayAbilitySpecHandle& Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo& ActivationInfo,
		const FActiveAbilityInstanceData& AuthoritySyncData);
	void ForceCancelAbilityInstance(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo);
	void ForceEndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bWasCancelled);
#pragma endregion

#pragma region Rewindable Tasks

	UFUNCTION(BlueprintPure,BlueprintCallable,Category=Tasks)
	UBasePredictionTask* GetPredictionTaskByName(UPARAM(meta=(GetOptions="GetValidTaskNames"))FName TaskName);
	
	UFUNCTION(BlueprintCallable,Category=Tasks)
	TArray<UBasePredictionTask*> GetPredictionTasks() const;

	UPROPERTY(VisibleAnywhere,Instanced,EditFixedSize,category=Tasks,meta = (TitleProperty = "TaskName"))
	TArray<TObjectPtr<UBasePredictionTask>> PredictionTasksInstances;

	UFUNCTION(BlueprintPure,BlueprintInternalUseOnly)
	UBasePredictionTask* GetTaskFromGuid(FGuid Guid);

	// this gets the task names to display task in network prediction debugger better
	TArray<FName> GetTasksNames() const;

protected:
	// Add A New Native Task for c++ usage, has no node attached to it.
	// has to be called in the constructor with FObjectInitializer.
	FGuid AddNativePredictionTask(const TSubclassOf<UBasePredictionTask> TaskClass,const FObjectInitializer& ObjectInitializer);

private:

	void InstantiateNetPredictionTasks();
	
	
	friend class UNpAbilitySystemComponent;
	friend class UAbilitySimulationLibrary;

#pragma endregion

	bool bHasBlueprintOnAbilityRestored = false;
public:
	
#if WITH_EDITOR
	UFUNCTION()
	TArray<FName> GetValidTaskNames();
#endif
};

