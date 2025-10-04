// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySimulationSettings.h"

UAbilitySimulationSettings::UAbilitySimulationSettings(const FObjectInitializer& ObjectInitializer)
{
	CategoryName = "Project";
}

const UAbilitySimulationSettings* UAbilitySimulationSettings::Get()
{
	return GetDefault<UAbilitySimulationSettings>();
}

UAbilitySimulationSettings* UAbilitySimulationSettings::GetMutable()
{
	return GetMutableDefault<UAbilitySimulationSettings>();
}

