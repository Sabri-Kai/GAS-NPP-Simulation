
#pragma once

#include "CoreMinimal.h"
#include "NetworkPredictionReplicationProxy.h"
#include "UObject/Object.h"
#include "BaseTaskData.generated.h"

class UNpGameplayAbility;
// Base struct for all Ability Tasks Data
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityTaskDataBase
{
	GENERATED_BODY()

	virtual ~FAbilityTaskDataBase() {}

	virtual TSharedPtr<FAbilityTaskDataBase> CloneShared() const;

	virtual bool NetSerialize(const FNetSerializeParams& Params)
	{
		return true;
	}
	// by default, we just serialize full data (in case user doesn't implement NetDeltaSerialize)
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params)
	{
		return NetSerialize(Params);
	}
	virtual UScriptStruct* GetScriptStruct() const;
	virtual void ToString(FAnsiStringBuilderBase& Out) const {}
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) {}
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) {}
};
template<>
struct TStructOpsTypeTraits< FAbilityTaskDataBase > : public TStructOpsTypeTraitsBase2< FAbilityTaskDataBase >
{
	enum
	{
		WithCopy = true
	};
};

// this struct exist to make interfacing with blueprints easier since you can't have TSharedPtr as blueprint variable
// And To Hold Per Task Specific Data + Common Active State bit.
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityTaskDataContainer
{
	GENERATED_BODY()

	FAbilityTaskDataContainer() {};

	explicit FAbilityTaskDataContainer(const TSharedPtr<FAbilityTaskDataBase>& InPointer)
	{
		TaskDataPointer = InPointer;
		IsActive = false;
	};

	FAbilityTaskDataContainer(const TSharedPtr<FAbilityTaskDataBase>& InPointer, const bool InIsActive)
	{
		TaskDataPointer = InPointer;
		IsActive = InIsActive;
	};

	FAbilityTaskDataContainer& operator=(const FAbilityTaskDataContainer& Other);

	UPROPERTY(Transient)
	bool IsActive = false;

	TSharedPtr<FAbilityTaskDataBase> TaskDataPointer = nullptr;
};

template<>
struct TStructOpsTypeTraits<FAbilityTaskDataContainer> : public TStructOpsTypeTraitsBase2<FAbilityTaskDataContainer>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FAbilityTaskDataBase> Data is copied around
	};
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityTaskDataArray
{
	GENERATED_BODY()

	virtual ~FAbilityTaskDataArray() {}

	FORCEINLINE bool IsValidStateIndex(const int32& Index) const
	{
		return AbilityTasksData.IsValidIndex(Index);
	}

	FORCEINLINE void Empty()
	{
		AbilityTasksData.Empty();
	}

	static TSharedPtr<FAbilityTaskDataBase> CreateDataByType(const UScriptStruct* DataStructType);

	/** Serialize all moves and their states for this group */
	void NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);
	void NetDeltaSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO);

	bool ShouldReconcile(const FAbilityTaskDataArray& AuthorityState) const;

	/** Comparison operator - needs matching LayeredMoves along with identical states in those structs */
	bool operator==(const FAbilityTaskDataArray& Other) const;

	/** Comparison operator */
	FORCEINLINE bool operator!=(const FAbilityTaskDataArray& Other) const { return !(*this == Other); }

	const FAbilityTaskDataContainer& operator[](int32 Index) const;

	FAbilityTaskDataContainer& operator[](int32 Index);
	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector) const;
	/** Get a simplified string representation of this group. Typically for debugging. */
	void ToString(FAnsiStringBuilderBase& Out, const TArray<FName>& TasksNames) const;
	// Clears out any finished or invalid active moves and adds any queued moves to the active moves
	/** Make this collection a piece-wise interpolation between 2 collections */
	void Interpolate(const FAbilityTaskDataArray& From, const FAbilityTaskDataArray& To, float Pct);

	FORCEINLINE void Clear() { AbilityTasksData.Empty(); }

	FORCEINLINE int32 Num() const { return AbilityTasksData.Num(); }

	/** Layered moves currently active in this group */
	UPROPERTY(Transient)
	TArray<FAbilityTaskDataContainer> AbilityTasksData;

protected:
	/** Helper function for serializing array of root motion sources */
	static void NetSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO, TArray< FAbilityTaskDataContainer >& AbilityTasksDataArray);
	static void NetDeltaSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO, TArray< FAbilityTaskDataContainer >& AbilityTasksDataArray);
};

inline const FAbilityTaskDataContainer& FAbilityTaskDataArray::operator[](int32 Index) const
{
	check(IsValidStateIndex(Index));
	return AbilityTasksData[Index];
}

inline FAbilityTaskDataContainer& FAbilityTaskDataArray::operator[](int32 Index)
{
	check(IsValidStateIndex(Index));
	return AbilityTasksData[Index];
}

template<>
struct TStructOpsTypeTraits<FAbilityTaskDataArray> : public TStructOpsTypeTraitsBase2<FAbilityTaskDataArray>
{
	enum
	{
		WithCopy = true,		// Necessary so that TSharedPtr<FAbilityTaskDataBase> Data is copied around
		//WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};
