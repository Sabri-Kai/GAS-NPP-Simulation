// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "UObject/Object.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "NetMontageSimulatorData.generated.h"

/**
 * 
 */
// Montage Simulator Synced Data

class UNetMontageSimulator;
struct FNetSerializeParams;

USTRUCT()
struct FSyncedNotifiesArray
{
	GENERATED_BODY()

	FSyncedNotifiesArray() = default;

	explicit FSyncedNotifiesArray(const UAnimMontage* InMontage);

	UPROPERTY(Transient)
	const UAnimMontage* Montage = nullptr;

	UPROPERTY(Transient)
	TArray<int32> Indexes;

	typedef TArray<int32>::TIterator TIterator;
	typedef TArray<int32>::TConstIterator TConstIterator;

	const FAnimNotifyEvent& operator[](const int32 Index) const
	{
		return Montage->Notifies[Indexes[Index]];
	}

	int32 Num() const
	{
		return Indexes.Num();
	}

	FORCEINLINE TConstIterator CreateConstIterator() const
	{
		return Indexes.CreateConstIterator();
	}

	FORCEINLINE TIterator CreateIterator()
	{
		return Indexes.CreateIterator();
	}

	bool IsValidIndex(int32 Index) const;
};

//Montage PlayBack Queue
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityMontagePlayback
{
	GENERATED_USTRUCT_BODY()

	FAbilityMontagePlayback()
	{
		MontageToPlay = nullptr;
		MontageStartTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
	};
	FAbilityMontagePlayback(UAnimMontage* AnimMontage,const float& StartTime,const float& PlayRate,const FName& SectionName,const float& InRootMotionScale)
	{
		MontageToPlay = AnimMontage;
		if (MontageToPlay)
		{
			MontagePlayRate = PlayRate;
			RootMotionScale = InRootMotionScale;
			MontageStartTime = StartTime;
			if(!SectionName.IsNone())
			{
				const int32 SectionIndex = MontageToPlay->GetSectionIndex(SectionName);
				if (SectionIndex != INDEX_NONE)
				{
					float FinalStartTime,EndTime;
					MontageToPlay->GetSectionStartAndEndTime(SectionIndex,FinalStartTime,EndTime);
					MontageStartTime = FinalStartTime + 0.001f;
				}
			}
			return;
		}
		MontageStartTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
	};

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> MontageToPlay;

	UPROPERTY(Transient)
	float MontageStartTime;

	UPROPERTY(Transient)
	float MontagePlayRate;

	UPROPERTY(Transient)
	float RootMotionScale;

	bool IsValidQueue() const ;
	void Reset()
	{
		MontageToPlay = nullptr;
		MontageStartTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
	}
};
//Montage Cancel Queue
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityMontageCancel
{
	GENERATED_USTRUCT_BODY()

	FAbilityMontageCancel()
	{
		MontageToCancel = nullptr;
	};

	explicit FAbilityMontageCancel(UAnimMontage* Montage)
	{
		MontageToCancel = Montage;
	};

	UPROPERTY()
	TObjectPtr<UAnimMontage> MontageToCancel;
	
	bool IsValidQueue() const {return IsValid(MontageToCancel);}

	void Reset()
	{
		MontageToCancel = nullptr;
	};
};
// Root Motion data Of montage That Is Currently Playing
// This data is Computed every frame from 0 , so previous value doesn't matter.
// these kind of properties do not need to be sent over the network , we don't serialize them
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedMontageRootMotion
{
	GENERATED_BODY()
	FSyncedMontageRootMotion() {}
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UAnimMontage> Montage = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	float CurrentTime = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool HasRootMotion = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector RootMotionTranslation = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FRotator RootMotionRotation = FRotator::ZeroRotator;
	FSyncedMontageRootMotion& operator=(const FSyncedMontageRootMotion& Other)
	{
		Montage = Other.Montage;
		CurrentTime = Other.CurrentTime;
		HasRootMotion = Other.HasRootMotion;
		RootMotionTranslation = Other.RootMotionTranslation;
		RootMotionRotation = Other.RootMotionRotation;
		return *this;
	}
	void ToString(FAnsiStringBuilderBase& Out) const
	{
		Out.Appendf("Playing Montage: ");
		Out.Append(TCHAR_TO_ANSI(*GetNameSafe(Montage)));
		Out.Appendf("\n");
		Out.Appendf("Current Time: %f\n", CurrentTime);
		Out.Appendf("Has RootMotion: %s\n", HasRootMotion ? "True" : "False");
		Out.Appendf("Translation: X=%.2f Y=%.2f Z=%.2f\n", RootMotionTranslation.X, RootMotionTranslation.Y, RootMotionTranslation.Z);
		Out.Appendf("Rotation: Yaw=%.2f Pitch=%.2f Roll=%.2f\n", RootMotionRotation.Yaw, RootMotionRotation.Pitch, RootMotionRotation.Roll);
	}
};
// Layered Move Responsible for Applying root motion from montage player to mover.
// The root motion data in the layered move doesn't need to be serialized because this is a 1 frame move.
// except the start time which is required to end the move if a correction happens same frame it starts.
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FLayeredMove_SyncMontageRootMotion : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()
	FLayeredMove_SyncMontageRootMotion()
	{
		DurationMs = 0.f;
		FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
		Priority = 0.f;
	}
	explicit FLayeredMove_SyncMontageRootMotion(const FSyncedMontageRootMotion& RootMotion)
	{
		ExtractionData = RootMotion;
		DurationMs = 0.f;
		FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
		Priority = 0.f;
	}
	virtual ~FLayeredMove_SyncMontageRootMotion() {}
	UPROPERTY(editAnywhere, BlueprintReadWrite, Category = Mover)
	FSyncedMontageRootMotion ExtractionData;
	// Generate a movement
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	static FTransform ConvertRootMotionToWorld(const FTransform& LocalRootMotion, const FTransform& ActorTransform, const FTransform& MeshBaseTransform);
};

#pragma region Synced Notifies Data
// Base class for all Anim Notifies
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedNotifyData
{
	GENERATED_USTRUCT_BODY()
	FSyncedNotifyData();
	virtual ~FSyncedNotifyData() {};
	// @return newly allocated copy of this FSyncedNotifyDataBase. Must be overridden by child classes
	virtual FSyncedNotifyData* Clone() const;
	FORCEINLINE TSharedPtr<FSyncedNotifyData> CloneShared() const
	{
		return TSharedPtr<FSyncedNotifyData>(Clone());
	}
	//virtual void NetSerialize(FArchive& Ar);
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
	{
		bOutSuccess = true;
		return true;
	}
	virtual UScriptStruct* GetScriptStruct() const;
	virtual void ToString(FAnsiStringBuilderBase& Out) const;
	virtual void AddReferencedObjects(class FReferenceCollector& Collector) {}
	virtual bool ShouldReconcile(const FSyncedNotifyData& AuthorityState) const;
	virtual void Interpolate(const FSyncedNotifyData& From, const FSyncedNotifyData& To, float Pct);
};
template<>
struct TStructOpsTypeTraits< FSyncedNotifyData > : public TStructOpsTypeTraitsBase2< FSyncedNotifyData >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

// this struct exist to make interfacing with blueprints easier since you can't have TSharedPtr as blueprint variable
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedNotifyDataContainer
{
	GENERATED_USTRUCT_BODY()
	FSyncedNotifyDataContainer(){}
	virtual ~FSyncedNotifyDataContainer() {}
	FSyncedNotifyDataContainer(const FSyncedNotifyDataContainer& Other)
	{
		IsActive = Other.IsActive;
		if (Other.SyncStatePointer.IsValid())
		{
			SyncStatePointer = Other.SyncStatePointer->CloneShared();
			return;
		}
		SyncStatePointer = nullptr;
		
	};
	FSyncedNotifyDataContainer& operator=(const FSyncedNotifyDataContainer& Other);
	/** Comparison operator - needs matching LayeredMoves along with identical states in those structs */
	bool operator==(const FSyncedNotifyDataContainer& Other) const;
	/** Comparison operator */
	bool operator!=(const FSyncedNotifyDataContainer& Other) const;
	
	TSharedPtr<FSyncedNotifyData> SyncStatePointer = nullptr;
	
	bool IsActive = false;
};
template<>
struct TStructOpsTypeTraits<FSyncedNotifyDataContainer> : public TStructOpsTypeTraitsBase2<FSyncedNotifyDataContainer>
{
	enum
	{
		WithCopy = true,
		WithIdenticalViaEquality = true,
	};
};
// Holds and takes care of serialization for Array Of notifies Data
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedNotifyDataArray
{
	GENERATED_USTRUCT_BODY()
	FSyncedNotifyDataArray();
	virtual ~FSyncedNotifyDataArray() {}
	FSyncedNotifyDataContainer AddNotifySyncState(const UScriptStruct* DataStructType, const uint8 InNotifyIndex);
	FSyncedNotifyDataContainer& OverrideNotifySyncState(const UScriptStruct* DataStructType, const uint8 InNotifyIndex);
	FORCEINLINE bool IsValidStateIndex(const int32& Index) const
	{
		return ActiveNotifySyncStates.IsValidIndex(Index);
	}
	static TSharedPtr<FSyncedNotifyData> CreateDataByType(const UScriptStruct* DataStructType);
	/** Serialize all moves and their states for this group */
	void NetSerialize(const FNetSerializeParams& P);
	 bool ShouldReconcile(const FSyncedNotifyDataArray& AuthorityState) const;
	/** Copy operator - deep copy so it can be used for archiving/saving off moves */
	FSyncedNotifyDataArray& operator=(const FSyncedNotifyDataArray& Other);
	/** Comparison operator - needs matching LayeredMoves along with identical states in those structs */
	bool operator==(const FSyncedNotifyDataArray& Other) const;
	/** Comparison operator */
	bool operator!=(const FSyncedNotifyDataArray& Other) const;
	const FSyncedNotifyDataContainer& operator[](int32 Index) const;
	FSyncedNotifyDataContainer& operator[](int32 Index);
	/** Exposes references to GC system */
	void AddStructReferencedObjects(FReferenceCollector& Collector) const;
	/** Get a simplified string representation of this group. Typically for debugging. */
	void ToString(FAnsiStringBuilderBase& Out) const;
	// Clears out any finished or invalid active moves and adds any queued moves to the active moves
	void Clear();
	int32 Num() const {return ActiveNotifySyncStates.Num();}
	/** Layered moves currently active in this group */
	UPROPERTY()
	TArray<FSyncedNotifyDataContainer> ActiveNotifySyncStates;

	static const FAnimNotifyEvent* GetPredictedNotifyEventFromMontage(UAnimMontage* InMontage, int32 Index);

	const FAnimNotifyEvent* GetNotifyEventAtIndex(UAnimMontage* Montage,const int32& InIndex) const;
	bool IsNotifyStateAtIndex(UAnimMontage* Montage,const int32& PredictedNotifyIndex) const;
	static bool IsNotifyState(const FAnimNotifyEvent& NotifyEvent);
	bool IsNotifyTriggered(const int32& PredictedNotifyIndex);
	bool ShouldNotifyTriggerThisFrame(UAnimMontage* Montage,const int32& InIndex, const float InCurrentTime , const float InPreviousTime);
	bool ShouldNotifyEndThisFrame(UAnimMontage* Montage,const int32& InIndex, const float InCurrentTime, const float DeltaSeconds);
	bool ShouldNotifyTickThisFrame(UAnimMontage* Montage,const int32& InIndex, const float InCurrentTime);
	bool SetupDataForNotifyAtIndex(UAnimMontage* Montage,const int32& Index);
	FORCEINLINE static UObject* GetNotifyObject(const FAnimNotifyEvent& NotifyEvent)
	{
		return NotifyEvent.NotifyStateClass
			? Cast<UObject>(NotifyEvent.NotifyStateClass.Get())
			: Cast<UObject>(NotifyEvent.Notify.Get());
	}
protected:
	/** Helper function for serializing array of root motion sources */
	static void NetSerializeNotifyStatesArray(FArchive& Ar, TArray< FSyncedNotifyDataContainer >& NotifySyncStatesArray);
};
inline const FSyncedNotifyDataContainer& FSyncedNotifyDataArray::operator[](int32 Index) const
{
	check(IsValidStateIndex(Index));
	return ActiveNotifySyncStates[Index];
}
inline FSyncedNotifyDataContainer& FSyncedNotifyDataArray::operator[](int32 Index)
{
	check(IsValidStateIndex(Index));
	return ActiveNotifySyncStates[Index];
}
template<>
struct TStructOpsTypeTraits<FSyncedNotifyDataArray> : public TStructOpsTypeTraitsBase2<FSyncedNotifyDataArray>
{
	enum
	{
		WithCopy = true,
		//WithNetSerializer = true,
		WithIdenticalViaEquality = true,
		WithAddStructReferencedObjects = true,
	};
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSimTickNotifyData
{
	GENERATED_USTRUCT_BODY()

	FSimTickNotifyData(){};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	TObjectPtr<UNetMontageSimulator> MontagePlayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	TObjectPtr<UAnimMontage> AnimMontage = nullptr;

	// Delta Seconds Here Doesn't necessarily means it's same as simulation delta seconds, since notify state can start or end in the middle of a tick
	// when this happen we only tick the notify state by the amount of time left not entire sim tick delta time.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float DeltaSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float NotifyStartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float NotifyEndTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float CurrentMontageTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float CurrentSimTimeMS = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	FTransform RootMotionTransform;

	TSharedPtr<FSyncedNotifyData> SharedNotifyDataState = nullptr;
};
template<>
struct TStructOpsTypeTraits<FSimTickNotifyData> : public TStructOpsTypeTraitsBase2<FSimTickNotifyData>
{
	enum
	{
		WithCopy = true,
	};
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSimTickNotifyEndData
{
	GENERATED_USTRUCT_BODY()

	FSimTickNotifyEndData(){};

	TSharedPtr<FSyncedNotifyData> SharedNotifyDataState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	FTransform ExtractionData;
};
template<>
struct TStructOpsTypeTraits<FSimTickNotifyEndData> : public TStructOpsTypeTraitsBase2<FSimTickNotifyEndData>
{
	enum
	{
		WithCopy = true,
	};
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FRestoreNotifyData
{
	GENERATED_USTRUCT_BODY()

	FRestoreNotifyData(){};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	TObjectPtr<UNetMontageSimulator> MontagePlayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	TObjectPtr<UAnimMontage> AnimMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float NotifyStartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float NotifyEndTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=NotifyState)
	float CurrentMontageTime = 0.f;

	TSharedPtr<FSyncedNotifyData> SharedNotifyDataState = nullptr;
};
template<>
struct TStructOpsTypeTraits<FRestoreNotifyData> : public TStructOpsTypeTraitsBase2<FRestoreNotifyData>
{
	enum
	{
		WithCopy = true,
	};
};
#pragma endregion

#pragma region // Montage Simulation Sync State
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FMontageSimSyncState
{
	GENERATED_USTRUCT_BODY()

	FMontageSimSyncState();

	UPROPERTY()
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY()
	FSyncedNotifyDataArray NotifySyncStates;
	
	FORCEINLINE UAnimMontage* GetPlayingMontage() const { return Montage; };
	FORCEINLINE float GetCurrentTime() const {return MontageTime;}
	FORCEINLINE float GetRootMotionScale() const {return RootMotionScale;}
	FORCEINLINE float GetPlayRate() const {return MontagePlayRate;}
	FORCEINLINE bool GetIsPaused() const {return bIsPaused;}

	FORCEINLINE void SetCurrentTime(const float InCurrentTime)
	{
		MontageTime = InCurrentTime;
	}

	FORCEINLINE void SetPlayRate(const float InPlayRate)
	{
		MontagePlayRate = FMath::Floor(InPlayRate * 1000) / 1000.f;
	}

	FORCEINLINE void SetRootMotionScale(const float InRootMotionScale)
	{
		RootMotionScale = FMath::Floor(InRootMotionScale * 1000) / 1000.f;
	}

	FORCEINLINE void SetIsPaused(const bool InPaused)
	{
		bIsPaused = InPaused;
	}
	
	FORCEINLINE void Reset()
	{
		Montage = nullptr;
		MontageTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
		bIsPaused = false;
		NotifySyncStates.Clear();
	}
	
	void NetSerialize(const FNetSerializeParams& P);
	void NetDeltaSerialize(const FNetSerializeParams& P);

	void ToString(FAnsiStringBuilderBase& Out) const;

	bool ShouldReconcile(const FMontageSimSyncState& AuthorityState) const;

	void Interpolate(const FMontageSimSyncState& From, const FMontageSimSyncState& To, float Pct);

	void AddStructReferencedObjects(FReferenceCollector& Collector);
private:
	UPROPERTY()
	float MontageTime;
	UPROPERTY()
	float MontagePlayRate;
	UPROPERTY()
	float RootMotionScale;
	UPROPERTY()
	bool bIsPaused;
};
template<>
struct TStructOpsTypeTraits< FMontageSimSyncState > : public TStructOpsTypeTraitsBase2< FMontageSimSyncState >
{
	enum
	{
		WithAddStructReferencedObjects = true,
	};
};
#pragma endregion
