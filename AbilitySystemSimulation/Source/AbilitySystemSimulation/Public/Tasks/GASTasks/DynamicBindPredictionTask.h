// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "Tasks/BasePredictionTask.h"
#include "DynamicBindPredictionTask.generated.h"


/**
 * - DO NOT ADD ANY LOGIC TO BIND AND UNBIND PINS EXCEPT BINDING/UNBINDING FROM DELEGATES
 * This Task is special, as it is the only task that guarantees its output pins to trigger
 * And they will trigger during a Restore Frame / Rollback if needed.
 * The purpose of this task is to be used to bind and unbind to delegate that happen async meaning :
 * 1 - Delegate binding is happening after the output pin of another task,
 * if an ability performs a rollback and this delegate should now be unbound we need to be aware of it and unbind it
 * 2 - Delegate Binding can change in the ability lifetime. E.g you bind on activation and then wait for a task trigger to unbind
 * the task has a function to be called to initiate the binding and unbinding so we don't need to use multiple of them for a single delegate
 * Usage :
 * - call the task when you want to bind , it will call the Bind execution pin , before the "first/then" pin.
 * you bind to your delegate after the Bind output pin.
 * - give the task a name , and use the function GetPredictionTaskByName in the ability to get this task then cast it to this class.
 * (i recommend creating a pure function that would get this task by name and cast it to its class for every task you want to use its functions)
 * - now you have access to the task instance , you can call Bind and Unbind from it whenever you want to change the binding for your delegate.
 * NOTES :
 * - when ability ends this task will automatically call unbind. no need to call it On End Ability
 * - If you only bind a delegate on activate ability and unbind it on end , this task is not needed
 * 
 */

#pragma region Data
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FBindingTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FBindingTaskData() override {}

	UPROPERTY()
	bool bIsBound = false;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};
#pragma endregion

#pragma region Class
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBindEvent);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UDynamicBindPredictionTask : public UBasePredictionTask
{
	GENERATED_UCLASS_BODY()
	
	/*
	 * Whether to trigger bind execution pin when the input execution pin gets triggered
	 * or let binding be fully controlled from direct TriggerBind and TriggerUnBind function calls
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="BindTask")
	bool TriggerBindOnExecution = true;

	UPROPERTY(BlueprintAssignable)
	FBindEvent Bind;

	UPROPERTY(BlueprintAssignable)
	FBindEvent UnBind;

	UFUNCTION(BlueprintCallable,BlueprintInternalUseOnly)
	void ExecuteTask();


	UFUNCTION(BlueprintCallable,Category="BindTask")
	void TriggerBind();

	UFUNCTION(BlueprintCallable,Category="BindTask")
	void TriggerUnBind();

	UFUNCTION(BlueprintCallable,Category="BindTask")
	bool IsBound() const {return bIsBound;}

	virtual void StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData) override;
	virtual void ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead) override;
	virtual void WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite) override;

	virtual FText GetNodeTitle() const override;
	virtual FLinearColor GetNodeTitleColor() const override;

private:
	bool bIsBound = false;
};
#pragma endregion