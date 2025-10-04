#include "AbilitySystemSimulationEditor.h"

#include "KismetCompiler.h"
#include "KismetCompilerModule.h"
#include "Abilities/NpGameplayAbility.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Compilation/NpGameplayAbilityBlueprint.h"
#include "Compilation/PredictionAbilityBlueprintCompilerContext.h"
#include "UObject/ObjectSaveContext.h"

#define LOCTEXT_NAMESPACE "FAbilitySystemSimulationEditorModule"

void FAbilitySystemSimulationEditorModule::StartupModule()
{
	FKismetCompilerContext::RegisterCompilerForBP(UNpGameplayAbilityBlueprint::StaticClass(), [](UBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
	{
		return MakeShared<FPredictionAbilityBlueprintCompilerContext>(CastChecked<UNpGameplayAbilityBlueprint>(InBlueprint), InMessageLog, InCompileOptions);
	});

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.OverrideBPTypeForClass(UNpGameplayAbility::StaticClass(), UNpGameplayAbilityBlueprint::StaticClass());

	// Register it in your editor module's StartupModule()
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(
		UNpGameplayAbility::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FPredictionAbilityDetailsCustomization::MakeInstance));

	
	
	PropertyModule.RegisterCustomClassLayout(
		UK2Node_PredictionTask::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FPredictionNodeNodeDetails::MakeInstance));
	
}





void FAbilitySystemSimulationEditorModule::ShutdownModule()
{
	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomClassLayout(UNpGameplayAbility::StaticClass()->GetFName());
		PropertyModule.UnregisterCustomClassLayout(UK2Node_PredictionTask::StaticClass()->GetFName());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FAbilitySystemSimulationEditorModule, AbilitySystemSimulationEditor)
