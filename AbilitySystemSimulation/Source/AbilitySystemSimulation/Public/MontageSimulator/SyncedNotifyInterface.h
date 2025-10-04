// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SyncedNotifyInterface.generated.h"

struct FRestoreNotifyData;
struct FSimTickNotifyEndData;
struct FSimTickNotifyData;
struct FAbilitySystemTimeStep;
struct FMontageSimSyncState;
struct FSyncedNotifyDataContainer;
// This class does not need to be modified.
UINTERFACE()
class USyncedNotifyInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class ABILITYSYSTEMSIMULATION_API ISyncedNotifyInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	void SimulationBegin(const bool IsReSimulating, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput);
	virtual void SimulationBegin_Implementation(const bool IsReSimulating, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput) {};

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	void SimulationTick(const FAbilitySystemTimeStep& InTimeStep, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput);
	virtual void SimulationTick_Implementation(const FAbilitySystemTimeStep& InTimeStep, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput) {};

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	void SimulationEnd(const bool IsReSimulating, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput);
	virtual void SimulationEnd_Implementation(const bool IsReSimulating, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput) {};

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	void RestoreFrame(const FRestoreNotifyData& InputData,const FSyncedNotifyDataContainer& AuthorityState, const FSyncedNotifyDataContainer& ExpungedState, const UNetMontageSimulator* MontageSimulator);
	virtual void RestoreFrame_Implementation(const FRestoreNotifyData& InputData,const FSyncedNotifyDataContainer& AuthorityState, const FSyncedNotifyDataContainer& ExpungedState, const UNetMontageSimulator* MontageSimulator) {};

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	UScriptStruct* GetRequiredType();
	virtual UScriptStruct* GetRequiredType_Implementation() {return nullptr;}
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "NotifyProcessor")
	bool CanTick() const;
	virtual bool CanTick_Implementation() const { return false; }
};
