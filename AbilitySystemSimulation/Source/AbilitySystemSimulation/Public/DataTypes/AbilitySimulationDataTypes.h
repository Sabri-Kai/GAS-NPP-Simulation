
#pragma once

#include "CoreMinimal.h"
#include "AbilitiesDataTypes.h"
#include "CuesDataTypes.h"
#include "EffectsDataTypes.h"
#include "NetworkPredictionTickState.h"
#include "MontageSimulator/NetMontageSimulatorData.h"
#include "ProjectilesSimulator/SyncedProjectilesData.h"
#include "StructUtils/InstancedStruct.h"
#include "TargetingTypes/TargetingDataTypes.h"

#include "AbilitySimulationDataTypes.generated.h"
/**
 * 
 */

struct FNetSerializeParams;

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySystemTimeStep
{
	GENERATED_BODY()
	
	FAbilitySystemTimeStep() 
	{ 
		bIsResimulating=false; 
	}

	FAbilitySystemTimeStep(const FNetSimTimeStep& InNetSimTimeStep)
		: ServerFrame(InNetSimTimeStep.Frame)
		, BaseSimTimeMs(InNetSimTimeStep.TotalSimulationTime)
		, InterpolationTimeMS(InNetSimTimeStep.InterpolationTimeMS)
		, StepMs(InNetSimTimeStep.StepMS)
		, bIsResimulating(InNetSimTimeStep.IsResimulating)
	{
	}

	// The server simulation frame this timestep is associated with
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mover)
	int32 ServerFrame = 0;

	// Starting simulation time (in server simulation timespace)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mover)
	float BaseSimTimeMs = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mover)
	float InterpolationTimeMS = 0.f;

	// The delta time step for this tick
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Mover)
	float StepMs = 0.1f;

	// Indicates whether this time step is re-simulating based on prior inputs, such as during a correction
	bool bIsResimulating = false;

};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedGameplayTagCount
{
	GENERATED_USTRUCT_BODY()

	FSyncedGameplayTagCount() {};
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void FillFromGameplayTagCountContainer(const FGameplayTagCountContainer& TagsCountContainer);
	void RemoveTags(const FGameplayTagCountContainer& TagsCountContainer);
	void AddTags(const FGameplayTagCountContainer& TagsCountContainer);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool operator==(const FSyncedGameplayTagCount& Other) const;
	bool operator!=(const FSyncedGameplayTagCount& Other) const;
	const TMap<FGameplayTag,int32>& GetExplicitTagCountMap() const {return ExplicitTagCountMap;}
private:
	TMap<FGameplayTag,int32> ExplicitTagCountMap;
	friend struct FAbilitySimSyncState;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedTarget
{
	GENERATED_USTRUCT_BODY()

	FSyncedTarget() {};
	bool NetSerialize(const FNetSerializeParams& P);
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool operator==(const FSyncedTarget& Other) const;
	bool operator!=(const FSyncedTarget& Other) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Target)
	TObjectPtr<AActor> Target = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Target)
	FGameplayTag TargetType;
};

/** 
 *  Ability State we are evolving frame to frame and keeping in sync (frequently changing)
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySimSyncState
{
	GENERATED_USTRUCT_BODY()

public:
	FAbilitySimSyncState();
	
	UPROPERTY()
	bool bSuppressGrantAbility;
	
	UPROPERTY()
	bool UserAbilityActivationInhibited;

	//Tags
	//This Is the data required for BlockedAbilityTags in Ability System Component
	FSyncedGameplayTagCount BlockedAbilityTags;
	
	//This Is the data required for GameplayTagCountContainer in Ability System Component
	FSyncedGameplayTagCount GameplayTagCountContainer;

	// Handle Counts Uses To Have Synced Handles For Abilities
	UPROPERTY()
	uint32 ActivatableAbilitiesHandleCount = 0;
	
	UPROPERTY()
	FActivatableAbilitiesCollection Abilities;
	
	UPROPERTY()
	FActiveEffectSyncDataContainer ActiveGameplayEffects;

	UPROPERTY()
	FAttributeSetSyncDataCollection AttributeSets;

	// Synced Montage Playback
	UPROPERTY()
	FMontageSimSyncState MontageSimulatorData;

	UPROPERTY()
	FSyncedTarget SyncedTarget;

	UPROPERTY()
	FProjectilesCollection ProjectilesCollection;

	UPROPERTY()
	FActiveCueSyncDataContainer SyncedCues;

	void NetSerialize(const FNetSerializeParams& P);

	void NetDeltaSerialize(const FNetSerializeParams& P);

	void ToString(FAnsiStringBuilderBase& Out) const;

	bool ShouldReconcile(const FAbilitySimSyncState& AuthorityState) const;

	void Interpolate(const FAbilitySimSyncState* From, const FAbilitySimSyncState* To, float Pct);
};

// this can use a bit mask instead. serialization and the code checking for each would be much cleaner
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityInputActionState
{
	GENERATED_BODY()
	
	FAbilityInputActionState() 
	{
		bTriggered = false;
		bStarted = false;
		bOngoing = false;
		bCanceled = false;
		bCompleted = false;
	}

	void NetSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=InputState)
	bool bTriggered;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=InputState)
	bool bStarted;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=InputState)
	bool bOngoing;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=InputState)
	bool bCanceled;
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=InputState)
	bool bCompleted;

	void Reset()
	{
		bTriggered = false;
		bStarted = false;
		bOngoing = false;
		bCanceled = false;
		bCompleted = false;
	}

};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySimInputCmd
{
	GENERATED_USTRUCT_BODY()

public:
	
	FAbilitySimInputCmd();
	
	/**
	 * List of indexes of active mapping contexts, maps into the project settings Ability simulation mapping contexts array.
	 */
	UPROPERTY()
	TArray<uint8> ActiveMappingContexts;
	/** 
	 * the state of the input actions in the current active list of contexts
	 */
	UPROPERTY()
	TArray<FAbilityInputActionState> InputActionStates;
	/**
	 * Mouse Screen Location
	 */
	UPROPERTY()
	FVector2D MouseScreenLocation = FVector2D::ZeroVector;
	/** 
	 * Camera Relative Location To Owner Transform
	 */
	UPROPERTY()
	FVector_NetQuantize10 CameraLocation = FVector::ZeroVector;
	/**
	 * Custom user defined input data
	 */
	UPROPERTY()
	FInstancedStruct CustomInput;


	UPROPERTY()
	FSyncedScreenProjection ScreenProjectionData;

	void NetSerialize(const FNetSerializeParams& P);

	void ToString(FAnsiStringBuilderBase& Out) const;
};

// Auxiliary state that is input into the simulation (changes rarely)
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySimAuxState
{
	GENERATED_USTRUCT_BODY()

public:
	UScriptStruct* GetStruct() const { return StaticStruct(); }

	bool ShouldReconcile(const FAbilitySimAuxState& AuthorityState) const
	{ 
		return false; 
	}

	void NetSerialize(const FNetSerializeParams& P)
	{
	}

	void ToString(FAnsiStringBuilderBase& Out) const
	{
	}

	void Interpolate(const FAbilitySimAuxState* From, const FAbilitySimAuxState* To, float PCT)
	{
	}
};


/**
 * Contains all state data for the start of a simulation tick
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySystemTickStartData
{
	GENERATED_USTRUCT_BODY()

	FAbilitySystemTickStartData() {}
	FAbilitySystemTickStartData(
			const FAbilitySimInputCmd& InInputCmd,
			const FAbilitySimSyncState& InSyncState,
			const FAbilitySimAuxState& InAuxState)
		:  InputCmd(InInputCmd), SyncState(InSyncState), AuxState(InAuxState)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FAbilitySimInputCmd InputCmd;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FAbilitySimSyncState SyncState;
	UPROPERTY(BlueprintReadOnly, Category=Mover)
	FAbilitySimAuxState AuxState;
};

/**
 * Contains all state data produced by a simulation tick, including new simulation state
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilitySystemTickEndData
{
	GENERATED_USTRUCT_BODY()

	FAbilitySystemTickEndData() {}
	FAbilitySystemTickEndData(
		const FAbilitySimSyncState* SyncState,
		const FAbilitySimAuxState* AuxState)
	{
		this->SyncState = *SyncState;
		this->AuxState = *AuxState;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FAbilitySimSyncState SyncState;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	FAbilitySimAuxState AuxState;
};