// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputMappingContext.h"
#include "Engine/DeveloperSettings.h"
#include "AbilitySimulationSettings.generated.h"


UCLASS(config = Game, defaultconfig,meta = (DisplayName = "Ability Simulation Settings"))
class ABILITYSYSTEMSIMULATION_API UAbilitySimulationSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<TSoftObjectPtr<const UInputMappingContext>> AbilitySystemMappingContexts;

	static const UAbilitySimulationSettings* Get();
	static UAbilitySimulationSettings* GetMutable();
};
