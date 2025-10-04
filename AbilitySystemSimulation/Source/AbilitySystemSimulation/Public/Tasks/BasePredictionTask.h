
#pragma once

#include "CoreMinimal.h"
#include "DataTypes/BaseTaskData.h"
#include "UObject/Object.h"
#include "BasePredictionTask.generated.h"

/**
 * 
 */

class UNpAbilitySystemComponent;
struct FAbilitySystemTimeStep;
class UNpGameplayAbility;

/**
 * The Prediction Tasks are the most important part of this entire rework. they try to mirror the normal Ability Tasks,
 * These are their characteristics :
 *
 * 
 * 1 - Same as ability task , it's a runtime class you create, defines its execution/start function , delegates
 *  where the execution/start function inputs params become inputs of task node and delegates output pins.
 *  
 * 1 - these tasks are like pseudo-actor component, each instance of an ability owns instances of all its tasks.
 * they are added/removed through the custom blueprint nodes (each task class automatically gets his own)
 * can be added through c++ in class constructor with FObjectInitializer directly to PredictionTasks array in NpGameplayAbility
 * using function AddNativePredictionTask() which return FGuid for task ID that you can save in a variable
 * and use it to get the task instance
 * Highly recommended to use the tasks in blueprint, make any functions and logic you need in c++ , and finally ability graph in blueprint.
 * 
 * 2 - These tasks are ticked within the simulation tick if ShouldTaskTick is true.
 * 
 * 3 - each task can have synced data , which is filled inside WriteToSyncedData , from task own variables/state
 * and during a restore frame from a correction each task will call StartTaskRollback() which provides the new state and data we will rollback to
 * this gives the task the ability to react accordingly
 * for example rollback state is active, but now it's not , so make sure to bind to the delegates needed.
 * This allows and ability to be completely restore to a previous state. 
 * a simple ability that has 1 task wait for duration 1 seconds then add impulse to push me back.
 * This ability was given to me by enemy when he hit me, so it will trigger on server first (enemies can damage only on server)
 * let's say at time 0 MilliSeconds ability activated
 * when i get a correction for this ability i will be at time 100MS, the ability will be activated and task restored to being active
 * will re-simulate from time 0MS to 100MS and reaching current duration of 0.1 seconds now it will be waiting for another 0.9s to reach 1 second.
 * the ability is now completely in sync with the server at simulation time 100MS, both client and server have this ability active
 * and in 0.9 seconds they will both apply impulse.
 * 
 * IMPORTANT FOR TASK CREATORS:
 *
 * 1 - always sync any variables you use or are needed for proper task restoration
 * 2 - always make sure to properly restore your delegates bindings
 * 3 - StartTaskFunctionName MUST be set in the class constructor.
 * 4 - if the task has data it needs to sync, create the data of type FAbilityTaskDataBase and override all virtual functions
 * then set the DataType variable in constructor to YourStruct::StaticStruct().
 * 5 - ensure you override WriteToSyncedData and ReadFromSyncedData to read and write the synced data
 * 6 - SUPER IMPORTANT : ALWAYS check CanEffectSimulation(), before broadcasting the task delegates to ensure outside callbacks can't break frame restore
 * Like gameplay tag changes, those delegates might trigger during restore frame, but should not trigger task pins. 
 * 7 - 
 * 
 * Creation of these Tasks is left in c++ only, specially because their sync data (which many have their own unique data)
 * can only be created in c++. will try to add more tasks as utilities as time goes by or maybe the community will.
 */


UCLASS(Abstract,BlueprintType,DefaultToInstanced,EditInlineNew)
class ABILITYSYSTEMSIMULATION_API UBasePredictionTask : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UNpGameplayAbility;
	
	UPROPERTY(EditInstanceOnly, Category="General Settings")
	FName TaskName = NAME_None;

	// all Tasks are ordered by this priority, if a task should tick,they will tick in order based on this
	UPROPERTY(EditInstanceOnly, Category="General Settings")
	uint8 TaskTickPriority = 0;

	UPROPERTY(VisibleInstanceOnly, Category="General Settings")
	FGuid TaskId;

#pragma region Simulation API
	void RestoreFrame(const FAbilityTaskDataContainer& AuthoritySyncData);
	// Simulation Tick Event , for tasks that want to use tick such as wait tasks
	void SimulationTick(const FAbilitySystemTimeStep& TimeStep);
	// Called to Roll back a task to a specific state, this should include variables ,
	// and delegate bindings states (Bound/Unbound) if task has delegates that can changes their binding state while task is active.
	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData);
	// Overrideable Sim Tick Event
	virtual void OnSimulationTick(const FAbilitySystemTimeStep& TimeStep);
	bool IsActive() const {return bIsActive;}
	bool ShouldTriggerCallbacks() const;
	bool IsInRollback() const;
	void ActivateTask();
	/**
	 * OnOwning Ability Ended will be called even if task is not active,
	 * it is not recommended to change any synced data in this function
	 */
	virtual void OnOwningAbilityEnded(const bool& bWasCanceled);
	// OnTaskDeactivated Is Called When whenever task is deactivated.
	// is it expected to be used to clean up task , (Resetting the task data values and unbinding from delegates etc...)
	virtual void OnPreDeactivate(const bool& bWasCanceled);

	UFUNCTION(BlueprintCallable,Category= "Task")
	void CancelTask();

	UFUNCTION(BlueprintCallable,Category= "Task")
	void DeactivateTask(const bool& bWasCanceled);
	
	UFUNCTION(BlueprintCallable , Category= "Task")
	UNpGameplayAbility* GetOwningAbility() const {return OwningAbility;}
	
	UFUNCTION(BlueprintCallable , Category= "Task")
	UNpAbilitySystemComponent* GetAbilitySystemComponent() const;
	
	UFUNCTION(BlueprintCallable , Category= "Task")
	AActor* GetAvatarActor() const;
#pragma endregion

#pragma region Synced data
	// This Function Is Meant to used to set members of the task data from local variables or states
	// Example: When adding delegates, binding and unbinding them,
	// need to have booleans that represent whether each delegate is bound or not when binding changes while task is active
	// this is needed for rollback to be able to set the state of the task exactly as it was
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite);
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead);
	
	void SetSyncedData(const FAbilityTaskDataContainer& AuthoritySyncData);
#pragma endregion
	
	bool IsDefaultObject() const {return HasAllFlags(RF_ClassDefaultObject);}

	UPROPERTY()
	UScriptStruct* DataType = nullptr;

	UPROPERTY()
	FName StartTaskFunctionName = NAME_None;

	UPROPERTY()
	TArray<FName> AdditionalInputFunctions;

	UPROPERTY()
	bool ShouldTaskTick = false;

	
	
	/**
	 * these 2 functions GetNodeTitle_Internal and GetNodeTitleColor_Internal
	 * have blueprint version in the case a developer makes a task and exposes some overrideable functionality
	 * to blueprint without having to worry about color and name
	 */
	FText GetNodeTitle_Internal() const;
	FLinearColor GetNodeTitleColor_Internal() const ;

#pragma region Compilation Data
	static FName GetInitializationFunctionName(const FGuid& Guid);
	void CaptureOriginalState();
	UBasePredictionTask* GetOriginalStateObject() const;
	UPROPERTY()
	TArray<uint8> DefaultPropertiesValues;
#pragma endregion
protected:
	virtual FText GetNodeTitle() const ;
	
	virtual FLinearColor GetNodeTitleColor() const ;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Task",DisplayName= "Get Node Title")
	FText K2_GetNodeTitle() const ;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Task",DisplayName= "Get Node Title Color")
	FLinearColor K2_GetNodeTitleColor() const ;
	
	// this is only set at runtime on task instances when the owning ability activates
	UPROPERTY()
	TObjectPtr<UNpGameplayAbility> OwningAbility = nullptr;
	
	UPROPERTY()
	bool bIsActive = false;

private:

	bool bHasBlueprintGetNodeTitle = false;
	bool bHasBlueprintGetNodeTitleColor = false;
	
};
