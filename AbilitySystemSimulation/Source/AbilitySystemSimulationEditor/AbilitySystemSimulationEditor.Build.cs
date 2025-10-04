using UnrealBuildTool;

public class AbilitySystemSimulationEditor : ModuleRules
{
    public AbilitySystemSimulationEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AbilitySystemSimulation",
                "CoreUObject", 
                "Engine", 
                "Slate",
                "BlueprintGraph",
                "GameplayAbilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Slate",
                "SlateCore",
                "EditorFramework",
                "UnrealEd",
                "GraphEditor",
                "PropertyEditor",
                "KismetCompiler",
                "Kismet",
                
            }
        );
    }
}