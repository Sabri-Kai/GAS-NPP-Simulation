// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "Compilation/NpGameplayAbilityBlueprint.h"
#include "Compilation/NpAbilityGeneratedClass.h"
#include "KismetCompiler.h"

/**
 * 
 */
class ABILITYSYSTEMSIMULATION_API PredictionAbilityBlueprintCompilerContext
{
public:
	PredictionAbilityBlueprintCompilerContext();
	~PredictionAbilityBlueprintCompilerContext();
};


class FPredictionAbilityBlueprintCompilerContext : public FKismetCompilerContext
{

protected:
	typedef FKismetCompilerContext Super;
public:
	FPredictionAbilityBlueprintCompilerContext(UNpGameplayAbilityBlueprint* SourceSketch, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);
	virtual ~FPredictionAbilityBlueprintCompilerContext();

	virtual void PreCompile() override;
	UNpAbilityGeneratedClass* GetNewNpAbilityBlueprintClass() const { return CastChecked<UNpAbilityGeneratedClass>(NewClass); };
	virtual void SpawnNewClass(const FString& NewClassName) override;
	virtual void CopyTermDefaultsToDefaultObject(UObject* DefaultObject) override;

	static void CopyTaskPropertiesIfUnmodified(UBasePredictionTask* SourceObject, UBasePredictionTask* TargetObject);
	virtual void OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& Context) override;
	static void TryAddInitializationFunction(UClass* Class,const FName& FunctionName);
	void TryRemoveInitializationFunction(UClass* Class,const FName& FunctionName);
	void CopyTasksFromParent(UNpGameplayAbility* AbilityCDO,TArray<UBasePredictionTask*>& CurrentTasks) const;
	void CopyTasksFromGraphs(UNpGameplayAbility* AbilityCDO,TArray<UBasePredictionTask*>& CurrentTasks) const;
	void CheckTasksNamesAndPrioritySort(UNpGameplayAbility* AbilityCDO) const;
	void CleanupDanglingFunctions(UNpGameplayAbility* AbilityCDO);
};


