// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BaseSyncedVariableData.generated.h"

class UNpGameplayAbility;
struct FNetSerializeParams;
/**
 * Sync Var Def Hold a pointer to the property and rollback function of each sync var
 * it is filled on query for sync vars from a NpGameplayAbility GetSyncedVars()
 */
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FSyncVarDef
{
	GENERATED_BODY()

	FProperty* MemberSyncVariable = nullptr;
	UFunction* SyncVarPreRollbackFunction = nullptr;
};

/**
 * Base Data Type for synced variables :
 *
 * a Sync Var :
 * 1 - a struct that holds a variable type. they are created like this to have control over the net serialization/
 * delta serialization of a value.
 * there would be a FBaseSyncVar for each type (float, vector, custom struct etc..).
 * ability instances sync and roll-back these sync variables during correction,
 * so their values are guaranteed to be synced between client and server and be freely used in the ability with async prediction tasks.
 * 
 * 2 - They can have a PreRollback function, similar to the OnRep Functions of unreal ,
 * the function name MUST be named PreRollback + Property Name.
 * These functions are called before rolling back a sync var, and allows you to react to a variable changing because of rollback.
 *  
 * 
 * normal variables are not synced, so beware when setting a variable before a task and using later in time in an async manner.
 * variables that are "acceleration" variables , like saving pointer of a component during activate ability are safe to use
 *
 * if a variable is just the result of a math function it's best to make a pure function and use it instead of a variable
 *
 * generally speaking if you can put every usage of a variable inside a single function and make that variable local variable
 * it's safe to use as it is and doesn't need to be synced.
 *
 * 
 * This is a basic implementation of synced data and i believe it can be improved
 * maybe using FInstancedStruct instead, but we lose delta serialization per member. can either say , these are equal or not
 * also ShouldReconsile might get false positives from floating point inaccuracies, it's risky even if i create structs to mirror
 * engine types that can cause issues like floats, doubles, vectors,rotators etc.. and override their Identical with nearly equal
 * the user still needs to make sure he uses those in this InstancedStruct not directly engine types.
 * nested structs in this case are unsolvable, user must use these basic types even in nested structs. so other existing structs
 * that already use float for example need to override identical as well or if they are engine types, well can't use them
*/ 

USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	
	FBaseSyncVar();
	virtual ~FBaseSyncVar() {}

	// MUST be overriden by children
	virtual UScriptStruct* GetScriptStruct() const;
	
	virtual void Serialize(const FNetSerializeParams& Params);
	// this called if the sync var is not equal to the delta state nor default state
	// but you might still want to send only delta of changes, in delta serialize the BaseDeltaState* can be found in @Params
	virtual void SerializeDelta(const FNetSerializeParams& Params);
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const;
	virtual TSharedPtr<FBaseSyncVar> CloneShared() const;
	virtual void SetValue(const FBaseSyncVar* Other);
	virtual void ToString(FAnsiStringBuilderBase& Out) const {}
	/** If derived classes hold any object references, override this function and add them to the collector. */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) {}
	/** Interpolates contained data between a starting and ending block. MUST be override by types that compose STATE data (sync or aux).
	 * From and To are guaranteed to be the same concrete type as 'this'.
	 */
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct);
private:
	void NetSerialize(const FNetSerializeParams& Params, const FBaseSyncVar* CDOVar);
	void NetDeltaSerialize(const FNetSerializeParams& Params, const FBaseSyncVar* CDOVar);
	friend struct FSyncVarCollection;
};
template<>
struct TStructOpsTypeTraits< FBaseSyncVar > : public TStructOpsTypeTraitsBase2< FBaseSyncVar >
{
	enum
	{
		WithCopy = true
	};
};

// Contains a group of different FBaseSyncVar-derived data, and supports net serialization of them. used in abilities part of their sync state
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncVarCollection
{
	GENERATED_USTRUCT_BODY()
	FSyncVarCollection();
	virtual ~FSyncVarCollection() {}
	void Empty() { SyncedVars.Empty(); }
	/** Serialize all data in this collection */
	bool NetSerialize(const FNetSerializeParams& Params, const UNpGameplayAbility* AbilityCDO);
	bool NetDeltaSerialize(const FNetSerializeParams& Params, const UNpGameplayAbility* AbilityCDO);
	/** Copy operator - deep copy so it can be used for archiving/saving off data */
	FSyncVarCollection& operator=(const FSyncVarCollection& Other);
	/** Comparison operator (deep) - needs matching struct types along with identical states in those structs. See also ShouldReconcile */
	bool operator==(const FSyncVarCollection& Other) const;
	/** Comparison operator */
	bool operator!=(const FSyncVarCollection& Other) const;
	/** Checks if the collections are significantly different enough (piece-wise) to need reconciliation. NOT an equality check. */
	bool ShouldReconcile(const FSyncVarCollection& Other) const;
	/** Make this collection a piece-wise interpolation between 2 collections */
	void Interpolate(const FSyncVarCollection& From, const FSyncVarCollection& To, float Pct);

	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector) const;

	/** Get string representation of all elements in this collection */
	void ToString(FAnsiStringBuilderBase& Out, const UNpGameplayAbility* AbilityCDO) const;

	/** Const access to data array of collections */
	TArray<TSharedPtr<FBaseSyncVar>>::TConstIterator GetCollectionDataIterator() const;

	template <typename T>
	T* GetMutableDataAtIndex(const int32& Index) const
	{
		if (const FBaseSyncVar* FoundData = GetDataAtIndex(Index))
		{
			return static_cast<T*>(FoundData);
		}

		return nullptr;
	}
	

	template <typename T>
	const T* GetDataAtIndex(const int32& Index) const
	{
		if (const FBaseSyncVar* FoundData = GetDataAtIndex(Index))
		{
			return static_cast<const T*>(FoundData);
		}

		return nullptr;
	}
	void AddData(const TSharedPtr<FBaseSyncVar> DataInstance);
	static TSharedPtr<FBaseSyncVar> CreateDataByType(const UScriptStruct* DataStructType);
	FBaseSyncVar* AddDataByType(const UScriptStruct* DataStructType);
	FBaseSyncVar* GetDataAtIndex(const int32& Index) const;
	
	/** All data in this collection */
	TArray< TSharedPtr<FBaseSyncVar> > SyncedVars;
protected:
	
	/** Helper function for serializing array of data */
	static void NetSerializeDataArray(const FNetSerializeParams& Params, TArray<TSharedPtr<FBaseSyncVar>>& DataArray,const UNpGameplayAbility* AbilityCDO);
	static void NetDeltaSerializeDataArray(const FNetSerializeParams& Params, TArray<TSharedPtr<FBaseSyncVar>>& DataArray,const UNpGameplayAbility* AbilityCDO);
};

template<>
struct TStructOpsTypeTraits<FSyncVarCollection> : public TStructOpsTypeTraitsBase2<FSyncVarCollection>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FAbilityDataStructBase> Data is copied around
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};
