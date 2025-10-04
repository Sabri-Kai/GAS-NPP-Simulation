// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AbilitySystemSimulation : ModuleRules
{
	public AbilitySystemSimulation(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTags",
				"NetworkPrediction",
				"EnhancedInput",
				"Mover",
				"MotionWarping",
				"NetCore"
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"GameplayAbilities",
				"NetworkPrediction",
				"DeveloperSettings",
			}
		);
	}
}
