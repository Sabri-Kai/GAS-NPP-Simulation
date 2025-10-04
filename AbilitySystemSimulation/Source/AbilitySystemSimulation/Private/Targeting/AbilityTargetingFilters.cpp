// Fill out your copyright notice in the Description page of Project Settings.


#include "Targeting/AbilityTargetingFilters.h"


// Base Filtering Class
bool UAbilityTargetingFilter::ValidHitActor_Implementation(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC) const
{
	return IsValid(Actor);
}

bool UAbilityTargetingFilter::ValidHitResult_Implementation(const FHitResult& Hit, const UNpAbilitySystemComponent* OwningASC) const
{
	if (Hit.bBlockingHit)
	{
		return true;
	}
	return false;
}

// Filter By Class
bool UAbilityTargetingFilterByClass::ValidHitResult_Implementation(const FHitResult& Hit, const UNpAbilitySystemComponent* OwningASC) const
{
	if (AllowedClass != nullptr )
	{
		if (Hit.GetActor())
		{
			return Hit.GetActor()->GetClass()->IsChildOf(AllowedClass);
		}
		return false;
	}
	return true;
}

bool UAbilityTargetingFilterByClass::ValidHitActor_Implementation(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC) const
{
	if (AllowedClass != nullptr )
	{
		if (Actor)
		{
			return Actor->GetClass()->IsChildOf(AllowedClass);
		}
		return false;
	}
	return true;
}