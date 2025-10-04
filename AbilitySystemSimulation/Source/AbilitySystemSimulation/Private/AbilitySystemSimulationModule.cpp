// Copyright Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemSimulationModule.h"

#define LOCTEXT_NAMESPACE "FAbilitySystemSimulationModule"

void FAbilitySystemSimulationModule::StartupModule()
{
}

void FAbilitySystemSimulationModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FAbilitySystemSimulationModule, AbilitySystemSimulation)
