// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AbilityTargetingFilters.generated.h"

class UNpAbilitySystemComponent;
/**
 * 
 */
UCLASS(BlueprintType,Blueprintable,EditInlineNew,DefaultToInstanced)
class ABILITYSYSTEMSIMULATION_API UAbilityTargetingFilter : public UObject
{
	GENERATED_BODY()
public:

	/**
	 * return true if hit result is filtered out should be discarded
	 */
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,category=Targeting)
	bool ValidHitResult(const FHitResult& Hit, const UNpAbilitySystemComponent* OwningASC) const;
	/**
	 * return true if Actor is filtered out should be discarded
	 */
	UFUNCTION(BlueprintNativeEvent,BlueprintCallable,category=Targeting)
	bool ValidHitActor(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC) const;
};
UCLASS(Blueprintable)
class ABILITYSYSTEMSIMULATION_API UAbilityTargetingFilterByClass : public UAbilityTargetingFilter
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=Filter)
	TSubclassOf<AActor> AllowedClass = nullptr;
	
	UFUNCTION()
	virtual bool ValidHitResult_Implementation(const FHitResult& Hit, const UNpAbilitySystemComponent* OwningASC) const override;
	
	UFUNCTION()
	virtual bool ValidHitActor_Implementation(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC) const override;
};