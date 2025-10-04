// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "EffectsDataTypes.generated.h"

struct FNetSerializeParams;
class UGameplayEffect;
/**
 * 
 */
#pragma region Effects Data
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FCapturedAttributesSyncData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<float> CapturedSourceAttributeValues;

	UPROPERTY()
	TArray<float> CapturedTargetAttributeValues;

	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
	bool ShouldReconcile(const FCapturedAttributesSyncData& AuthorityState) const;
};

/**
 * Using Attribute Index here and the attribute set sync state means during a replay where the attribute set class changes,
 * things go wrong. unsure how to deal with it now. can try using the name of the property, but FName is expensive.
 * FSyncedModifiedAttribute and FSyncedModifiedAttributes are not currently in use. we are only syncing the modified attributes
 * coming from the GE modifiers only , that's static data so we only replicate the values not the attribute also
 */

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedModifiedAttribute
{
	GENERATED_USTRUCT_BODY()
	
	FSyncedModifiedAttribute(){};
	FSyncedModifiedAttribute(const FGameplayEffectModifiedAttribute& ModifiedAttribute);

	UPROPERTY()
	TSubclassOf<UAttributeSet> AttributeSetClass;
	UPROPERTY()
	uint8 AttributeIndex = 0;
	UPROPERTY()
	float TotalMagnitude = 0.f;

	static void MakeModifiedAttribute(const FSyncedModifiedAttribute& SyncedAttribute,FGameplayEffectModifiedAttribute& OutModifiedAttribute);
	
	void NetSerialize(FNetSerializeParams& P);
	
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedModifiedAttributes
{
	GENERATED_USTRUCT_BODY()
	
	FSyncedModifiedAttributes(){};
	FSyncedModifiedAttributes(const TArray<FGameplayEffectModifiedAttribute>& InModifiedAttributes)
	{
		ModifiedAttributes.Reserve(InModifiedAttributes.Num());
		for (int i = 0; i < InModifiedAttributes.Num(); i++)
		{
			ModifiedAttributes.Add(FSyncedModifiedAttribute(InModifiedAttributes[i]));
		}
	}

	static void MakeModifiedAttributes(const FSyncedModifiedAttributes& SyncedModifiedAttributes,TArray<FGameplayEffectModifiedAttribute>& OutModifiedAttributes);
	
	TArray<FSyncedModifiedAttribute> ModifiedAttributes;
	void NetSerialize(FNetSerializeParams& P);
	
};
/**
 * the mirror data to gameplay effect spec used in the sync state.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FEffectSpecSyncData
{
	GENERATED_USTRUCT_BODY()
	FEffectSpecSyncData();
	FEffectSpecSyncData(const FGameplayEffectSpec& Spec);

	UPROPERTY()
	TObjectPtr<const UGameplayEffect> Def;
	
	UPROPERTY()
	uint32 StackCount;

	UPROPERTY()
	bool bDurationLocked;
	
	UPROPERTY()
	FCapturedAttributesSyncData CapturedRelevantAttributes;
	
	UPROPERTY()
	FTagContainerAggregator	CapturedSourceTags;

	/**
	 * Target Tags Are not needed because they get capture every execution, in the case of a correction , the effect would already
	 * be executed and attribute values corrected , so we can capture them again in next execution normally.
	 */
	//FTagContainerAggregator	CapturedTargetTags;
	
	UPROPERTY()
	FGameplayTagContainer DynamicGrantedTags;
	
	UPROPERTY()
	TArray<float> ModifiedAttributesValues;
	
	UPROPERTY()
	TMap<FGameplayTag, float>	SetByCallerTagMagnitudes;
	
	UPROPERTY()
	FGameplayEffectContextHandle EffectContext; 
	
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FEffectSpecSyncData& AuthorityState) const;
	void Interpolate(const FEffectSpecSyncData& From, const FEffectSpecSyncData& To, float Pct);

	float GetDuration() const {return (DurationMS / 1000.f) - 1;}
	float GetPeriod() const {return (PeriodMS / 1000.f) - 1;}
	float GetLevel() const {return Level - 1;}

	uint32 GetPeriodMS() const {return FMath::FloorToInt32(GetPeriod() * 1000.f);}
private:
	UPROPERTY()
	uint32 Level;

	UPROPERTY()
	uint32 DurationMS;
	
	UPROPERTY()
	uint32 PeriodMS;
};
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveEffectSyncData
{
	GENERATED_USTRUCT_BODY()
	FActiveEffectSyncData();
	FActiveEffectSyncData(const FActiveGameplayEffect& ActiveEffect);

	UPROPERTY()
	int32 EffectHandle;

	UPROPERTY()
	FEffectSpecSyncData EffectSpecData;
	
	UPROPERTY()
	bool bIsInhibited;
	
	
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FActiveEffectSyncData& AuthorityState) const;
	void Interpolate(const FActiveEffectSyncData& From, const FActiveEffectSyncData& To, float Pct);
	float GetPeriodTime() const {return PeriodTimeMS / 1000.f;}
	uint32 GetPeriodTimeMS() const {return PeriodTimeMS;}
	float GetStartTime() const {return StartTimeMS / 1000.f;}

private:
	UPROPERTY()
	uint32 PeriodTimeMS;

	UPROPERTY()
	uint32 StartTimeMS;
};
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveEffectSyncDataContainer
{
	GENERATED_USTRUCT_BODY()
	FActiveEffectSyncDataContainer();
	FActiveEffectSyncDataContainer(const FActiveGameplayEffectsContainer& EffectsContainer);

	UPROPERTY()
	uint32 ActiveEffectsHandleCount = 0;
	
	UPROPERTY()
	TArray<FActiveEffectSyncData> ActiveEffects;
	
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FActiveEffectSyncDataContainer& AuthorityState) const;
	void Interpolate(const FActiveEffectSyncDataContainer& From, const FActiveEffectSyncDataContainer& To, float Pct);

	const FActiveEffectSyncData* GetActiveEffectByHandle(const int32& Handle) const;
	
};
#pragma endregion

#pragma region Attributes Data
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAttributeSyncData
{
	GENERATED_USTRUCT_BODY()
	FAttributeSyncData();

	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FAttributeSyncData& AuthorityState) const;
	void Interpolate(const FAttributeSyncData& From, const FAttributeSyncData& To, float Pct);

	FORCEINLINE void SetBaseValue(const float& Value)
	{
		BaseValue = Value;
	};
	FORCEINLINE void SetCurrentValue(const float& Value)
	{
		CurrentValue = Value;
	};
	FORCEINLINE float GetBaseValue() const {return BaseValue;}
	FORCEINLINE float GetCurrentValue() const {return CurrentValue;}
private:
	UPROPERTY()
	float BaseValue;

	UPROPERTY()
	float CurrentValue;
};
/**
 * Using Attribute Index here means during a replay where the attribute set class changes
 * (remove attributes , additions should be ok, not 100% sure if property array order is maintained),
* things go wrong. unsure how to deal with it now. can try using the name of the property, but FName is expensive.
* Having attributes as properties in attribute set class is actually not needed at all with an ability system using
* state based replication like NPP. if i make my own , they'll just be a map of tags and its data in a struct.
*/
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAttributeSetSyncData
{
	GENERATED_USTRUCT_BODY()
	FAttributeSetSyncData();
	FAttributeSetSyncData(UAttributeSet* AttributeSet);

	UPROPERTY()
	TArray<FAttributeSyncData> AttributeValues;

	UPROPERTY()
	TSubclassOf<UAttributeSet> AttributeSetClass;
	
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FAttributeSetSyncData& AuthorityState) const;
	void Interpolate(const FAttributeSetSyncData& From, const FAttributeSetSyncData& To, float Pct);
};
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAttributeSetSyncDataCollection
{
	GENERATED_USTRUCT_BODY()
	FAttributeSetSyncDataCollection(){};
	FAttributeSetSyncDataCollection(const TArray<UAttributeSet*>& AttributeSets);

	UPROPERTY()
	TArray<FAttributeSetSyncData> AttributeSetsData;
	
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FAttributeSetSyncDataCollection& AuthorityState) const;
	void Interpolate(const FAttributeSetSyncDataCollection& From, const FAttributeSetSyncDataCollection& To, float Pct);
};


#pragma endregion 