// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputSubsystemInterface.h"
#include "Abilities/NpGameplayAbility.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilitySimulationLibrary.generated.h"

class UEnhancedInputLocalPlayerSubsystem;
struct FActiveAbilityInstanceData;
/**
 * 
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UAbilitySimulationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Use This to add mapping contexts , will add context normally to input subsystem
	 * and automatically use only the ones added to ability simulation inputs project settings
	 * and adds it to the ability system component bound inputs, to use them in the simulation.
	 */
	UFUNCTION(BlueprintCallable,Category=AbilitySimulation)
	static void AddMappingContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem,UNpAbilitySystemComponent* AbilitySimulationComponent,const UInputMappingContext* MappingContext, int32 Priority,  FModifyContextOptions Options = FModifyContextOptions());

	/*
	 * Remove the mapping context from input subsystem and ability simulation if it has been added.
	 */
	UFUNCTION(BlueprintCallable,Category=AbilitySimulation)
	static void RemoveMappingContext(UEnhancedInputLocalPlayerSubsystem* InputSubsystem,UNpAbilitySystemComponent* AbilitySimulationComponent,const UInputMappingContext* MappingContext, FModifyContextOptions Options = FModifyContextOptions());
	
	static void FillAbilityInstanceDataFromInstance(UNpGameplayAbility* AbilityInstance,FActiveAbilityInstanceData& OutData);
	static void FillAbilitiesCollectionDataFromSpecContainer(FActivatableAbilitiesCollection& Collection,FGameplayAbilitySpecContainer& SpecContainer);
	static void GetAbilitySyncedVariables(FSyncVarCollection& OutCollection,const UNpGameplayAbility* AbilityInstance);

	static void NetSerializeUniqueActorsArrays(const FNetSerializeParams& Params, TArray<AActor*>& Actors);
	static void NetDeltaSerializeUniqueActorsArrays(const FNetSerializeParams& Params,const TArray<AActor*>&  BaseArray, TArray<AActor*>& Actors);
};
