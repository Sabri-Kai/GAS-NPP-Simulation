// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "BaseTaskData.h"
#include "BaseSyncedVariableData.h"
#include "InputTriggers.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitiesDataTypes.generated.h"

class UInputAction;
struct FGameplayEventData;
struct FGameplayTag;
class UGameplayAbility;
/*
 * These are the structs that represent the synced data of abilities, list of activateable abilities and with each having its active instances.
 */




class UNpGameplayAbility;
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityActivationTrigger
{
	GENERATED_USTRUCT_BODY()
	FAbilityActivationTrigger()
	{
		InputAction = nullptr;
		TriggerEvent = ETriggerEvent::None;
	};

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETriggerEvent TriggerEvent;
};

/**
 * the gameplay event data to be synced between client and server
 * when an ability is activated by an event
 */
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FSyncedGameplayEventData
{
	GENERATED_USTRUCT_BODY()
	FSyncedGameplayEventData(){}
	FSyncedGameplayEventData(const FGameplayEventData& EventData)
		: GameplayEventData(EventData)
	{
	};

	bool NetSerialize(const FNetSerializeParams& Params)
	{
		bool OutSuccess = true;
		Params.Ar << GameplayEventData.Instigator;
		Params.Ar << GameplayEventData.Target;
		Params.Ar << GameplayEventData.OptionalObject;
		Params.Ar << GameplayEventData.OptionalObject2;
		Params.Ar << GameplayEventData.EventMagnitude;
		GameplayEventData.ContextHandle.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		GameplayEventData.TargetData.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		GameplayEventData.EventTag.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		GameplayEventData.InstigatorTags.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		GameplayEventData.TargetTags.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		return OutSuccess;
	}

	 bool ShouldReconcile(const FSyncedGameplayEventData& Other) const
	{
		if (GameplayEventData.Instigator != Other.GameplayEventData.Instigator)
		{
			return true;
		}
		if (GameplayEventData.Target != Other.GameplayEventData.Target)
		{
			return true;
		}
		if (GameplayEventData.OptionalObject != Other.GameplayEventData.OptionalObject)
		{
			return true;
		}
		if (GameplayEventData.OptionalObject2 != Other.GameplayEventData.OptionalObject2)
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(GameplayEventData.EventMagnitude,Other.GameplayEventData.EventMagnitude))
		{
			return true;
		}
		if (GameplayEventData.ContextHandle.IsValid() != Other.GameplayEventData.ContextHandle.IsValid())
		{
			return true;
		}
		if (GameplayEventData.ContextHandle.IsValid())
		{
			if (!GameplayEventData.ContextHandle.Get()->NetIdentical(Other.GameplayEventData.ContextHandle.Get()))
			{
				return true;
			}
		}
		if (GameplayEventData.TargetData.Data.Num() != Other.GameplayEventData.TargetData.Data.Num())
		{
			return true;
		}
		for (int32 i = 0; i < GameplayEventData.TargetData.Data.Num(); i++)
		{
			if (GameplayEventData.TargetData.Data[i]->GetActors() != Other.GameplayEventData.TargetData.Data[i]->GetActors())
			{
				return true;
			}
			if (GameplayEventData.TargetData.Data[i]->HasHitResult() != Other.GameplayEventData.TargetData.Data[i]->HasHitResult())
			{
				return true;
			}
			if (GameplayEventData.TargetData.Data[i]->HasEndPoint() != Other.GameplayEventData.TargetData.Data[i]->HasEndPoint())
			{
				return true;
			}
			if (GameplayEventData.TargetData.Data[i]->HasEndPoint())
			{
				if (!GameplayEventData.TargetData.Data[i]->GetEndPoint().Equals(Other.GameplayEventData.TargetData.Data[i]->GetEndPoint()))
				{
					return true;
				}
			}
			if (GameplayEventData.TargetData.Data[i]->HasOrigin() != Other.GameplayEventData.TargetData.Data[i]->HasOrigin())
			{
				return true;
			}
			if (GameplayEventData.TargetData.Data[i]->HasOrigin())
			{
				if (!GameplayEventData.TargetData.Data[i]->GetOrigin().Equals(Other.GameplayEventData.TargetData.Data[i]->GetOrigin()))
				{
					return true;
				}
			}
		}
		if (GameplayEventData.EventTag != Other.GameplayEventData.EventTag)
		{
			return true;
		}
		if (GameplayEventData.TargetTags != Other.GameplayEventData.TargetTags)
		{
			return true;
		}
		if (GameplayEventData.InstigatorTags != Other.GameplayEventData.InstigatorTags)
		{
			return true;
		}
		return false;
	}

	UPROPERTY()
	FGameplayEventData GameplayEventData;
};

/**
 * The delta between two gameplay tag containers , used for delta serialization
 */
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FGameplayTagContainerDelta
{
	GENERATED_USTRUCT_BODY()
	FGameplayTagContainerDelta(){}
	FGameplayTagContainerDelta(const FGameplayTagContainer& CurrentTags,const FGameplayTagContainer& BaseTags)
	{
		// Tags in CurrentTags but not in BaseTags
		TagsAdded = CurrentTags;
		TagsAdded.RemoveTags(BaseTags);
    
		// Tags in BaseTags but not in CurrentTags
		TagsRemoved = BaseTags;
		TagsRemoved.RemoveTags(CurrentTags);
	};

	bool NetSerialize(const FNetSerializeParams& Params)
	{
		bool OutSuccess = true;
		TagsAdded.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		TagsRemoved.NetSerialize(Params.Ar,Params.Map,OutSuccess);
		return OutSuccess;
	}

	static FGameplayTagContainer ReconstructContainerFromBaseAndDelta(const FGameplayTagContainer& BaseTags,const FGameplayTagContainerDelta& DeltaTags)
	{
		FGameplayTagContainer Result = BaseTags;
    
		// Remove tags that are in B but not in A
		Result.RemoveTags(DeltaTags.TagsRemoved);
    
		// Add tags that are in A but not in B
		Result.AppendTags(DeltaTags.TagsAdded);
    
		return Result;
	}

	FGameplayTagContainer TagsAdded;
	FGameplayTagContainer TagsRemoved;
	
};
/**
 * Data for an Active Ability Instance and its Data
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveAbilityInstanceData
{
	GENERATED_USTRUCT_BODY()
	FActiveAbilityInstanceData();
	virtual ~FActiveAbilityInstanceData() {}
	
	UPROPERTY()
	bool bIsActive;
	
	UPROPERTY()
	bool bIsCancelable;
	
	UPROPERTY()
	bool bIsBlockingOtherAbilities;
	
	UPROPERTY()
	int8 ActivatedByInput;
	
	UPROPERTY()
	bool bHasEventData;
	
	UPROPERTY()
	FSyncedGameplayEventData CurrentEventData;

	UPROPERTY()
	TSet<FGameplayTag> TrackedGameplayCues;
	
	UPROPERTY()
	FAbilityTaskDataArray TaskDataCollection;

	UPROPERTY()
	FSyncVarCollection AbilitySyncedVars;
	
	bool NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);
	bool NetSerializeTrackedCues(const FNetSerializeParams& Params);
	bool NetDeltaSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);
	void ToString(FAnsiStringBuilderBase& Out, const UNpGameplayAbility* AbilityCDO) const;
	void AddReferencedObjects(FReferenceCollector& Collector) const;
	bool ShouldReconcile(const FActiveAbilityInstanceData& AuthorityState) const;
	void Interpolate(const FActiveAbilityInstanceData& From, const FActiveAbilityInstanceData& To, float Pct);
	bool AreTrackedCuesIdentical(const TSet<FGameplayTag>& OtherTrackedCues) const;

	FActiveAbilityInstanceData& operator=(const FActiveAbilityInstanceData& Other);
};
template<>
struct TStructOpsTypeTraits< FActiveAbilityInstanceData > : public TStructOpsTypeTraitsBase2< FActiveAbilityInstanceData >
{
	enum
	{
		WithCopy = true
	};
};
/**
 *	Contains a group of different FActiveAbilityInstanceData, and supports net serialization of them.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveAbilitiesInstancesCollection
{
	GENERATED_USTRUCT_BODY()
	FActiveAbilitiesInstancesCollection();
	virtual ~FActiveAbilitiesInstancesCollection() {}
	void Empty() { ActiveAbilityInstances.Empty(); }
	/** Serialize all data in this collection */
	bool NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);
	/** Delta serialize the data in this collection gives a base delta state */
	bool NetDeltaSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);
	/** Copy operator - deep copy so it can be used for archiving/saving off data */
	FActiveAbilitiesInstancesCollection& operator=(const FActiveAbilitiesInstancesCollection& Other);
	/** Comparison operator (deep) - needs matching struct types along with identical states in those structs. See also ShouldReconcile */
	bool operator==(const FActiveAbilitiesInstancesCollection& Other) const;
	/** Comparison operator */
	bool operator!=(const FActiveAbilitiesInstancesCollection& Other) const;
	/** Checks if the collections are significantly different enough (piece-wise) to need reconciliation. NOT an equality check. */
	bool ShouldReconcile(const FActiveAbilitiesInstancesCollection& Other) const;
	/** Make this collection a piece-wise interpolation between 2 collections */
	void Interpolate(const FActiveAbilitiesInstancesCollection& From, const FActiveAbilitiesInstancesCollection& To, float Pct);
	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	/** Get string representation of all elements in this collection */
	void ToString(FAnsiStringBuilderBase& Out,const UNpGameplayAbility* AbilityCDO) const;

	/** Const access to data array of collections */
	TArray<FActiveAbilityInstanceData>::TConstIterator GetCollectionDataIterator() const;

	/** All data in this collection */
	TArray<FActiveAbilityInstanceData> ActiveAbilityInstances;

protected:
	
	/** Helper function for serializing array of data */
	static void NetSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO,TArray<FActiveAbilityInstanceData>& DataArray);
	/** Helper function for serializing array of data */
	static void NetDeltaSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO,TArray<FActiveAbilityInstanceData>& DataArray);
};

template<>
struct TStructOpsTypeTraits<FActiveAbilitiesInstancesCollection> : public TStructOpsTypeTraitsBase2<FActiveAbilitiesInstancesCollection>
{
	enum
	{
		WithCopy = true,	
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};

/**
 * Activateable/Owned Ability sync data. this mirrors FGameplayAbilitySpec
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActivatableAbilitySyncState
{
	GENERATED_USTRUCT_BODY()
	FActivatableAbilitySyncState();
	virtual ~FActivatableAbilitySyncState() {}

	UPROPERTY()
	TSubclassOf<UNpGameplayAbility> AbilityClass = nullptr;

	// This Maps to FGameplayAbilitySpecHandle in FGameplayAbilitySpec
	UPROPERTY()
	int32 ActivatableAbilityHandle; 

	UPROPERTY()
	int32 Level;

	UPROPERTY()
	TObjectPtr<UObject> SourceObject;
	
	UPROPERTY()
	FGameplayTagContainer DynamicAbilityTags;

	// This Maps to FActiveGameplayEffectHandle in FGameplayAbilitySpec
	UPROPERTY()
	int32	GrantingGameplayEffectHandle;
	
	/** Passed on SetByCaller magnitudes if this ability was granted by a GE */
	TMap<FGameplayTag, float> SetByCallerTagMagnitudes;
	

	//Active Instance of this ability. most of the time it will be 1
	// always one for InstancedPerActor abilities
	UPROPERTY()
	FActiveAbilitiesInstancesCollection ActiveInstances;

	UPROPERTY()
	bool RemoveAfterActivation;

	UPROPERTY()
	uint8 ActiveCount;
	
	bool NetSerialize(const FNetSerializeParams& Params);
	bool NetDeltaSerialize(const FNetSerializeParams& Params);
	/** Get string representation of this struct instance */
	void ToString(FAnsiStringBuilderBase& Out) const;
	void AddReferencedObjects(FReferenceCollector& Collector);
	bool ShouldReconcile(const FActivatableAbilitySyncState& AuthorityState) const;
	void Interpolate(const FActivatableAbilitySyncState& From, const FActivatableAbilitySyncState& To, float Pct);

	FActivatableAbilitySyncState& operator=(const FActivatableAbilitySyncState& Other);
};
template<>
struct TStructOpsTypeTraits< FActivatableAbilitySyncState > : public TStructOpsTypeTraitsBase2< FActivatableAbilitySyncState >
{
	enum
	{
		WithCopy = true
	};
};
/**
 * Contains a group of different FActivatableAbilitySyncState data, and supports net serialization of them.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API	FActivatableAbilitiesCollection
{
	GENERATED_USTRUCT_BODY()
	FActivatableAbilitiesCollection();
	virtual ~FActivatableAbilitiesCollection() {}
	void Empty() { ActivatableAbilities.Empty(); }
	/** Serialize all data in this collection */
	bool NetSerialize(const FNetSerializeParams& Params);
	bool NetDeltaSerialize(const FNetSerializeParams& Params);
	/** Copy operator - deep copy so it can be used for archiving/saving off data */
	FActivatableAbilitiesCollection& operator=(const FActivatableAbilitiesCollection& Other);
	/** Comparison operator (deep) - needs matching struct types along with identical states in those structs. See also ShouldReconcile */
	bool operator==(const FActivatableAbilitiesCollection& Other) const;
	/** Comparison operator */
	bool operator!=(const FActivatableAbilitiesCollection& Other) const;
	/** Checks if the collections are significantly different enough (piece-wise) to need reconciliation. NOT an equality check. */
	bool ShouldReconcile(const FActivatableAbilitiesCollection& Other) const;
	/** Make this collection a piece-wise interpolation between 2 collections */
	void Interpolate(const FActivatableAbilitiesCollection& From, const FActivatableAbilitiesCollection& To, float Pct);

	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector);

	/** Get string representation of all elements in this collection */
	void ToString(FAnsiStringBuilderBase& Out) const;

	/** Const access to data array of collections */
	TArray<FActivatableAbilitySyncState>::TConstIterator GetCollectionDataIterator() const;

	TArray<FActivatableAbilitySyncState>& GetCollectionData_Mutable() {	return ActivatableAbilities; }
	const TArray<FActivatableAbilitySyncState>& GetCollectionData() const {	return ActivatableAbilities; }
	void FillFromActivatableAbilities(FGameplayAbilitySpecContainer& ActivatableAbilitiesSpecs);

	/** All data in this collection */
	TArray<FActivatableAbilitySyncState> ActivatableAbilities;
protected:
	
	/** Helper function for serializing array of data */
	static void NetSerializeDataArray(const FNetSerializeParams& Params, TArray<FActivatableAbilitySyncState>& DataArray);

	static void NetDeltaSerializeDataArray(const FNetSerializeParams& Params,const TArray<FActivatableAbilitySyncState>& BaseDeltaDataArray, TArray<FActivatableAbilitySyncState>& DataArray);
	
friend class	UNpGameplayAbility;
};


template<>
struct TStructOpsTypeTraits<FActivatableAbilitiesCollection> : public TStructOpsTypeTraitsBase2<FActivatableAbilitiesCollection>
{
	enum
	{
		WithCopy = true,	
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};
