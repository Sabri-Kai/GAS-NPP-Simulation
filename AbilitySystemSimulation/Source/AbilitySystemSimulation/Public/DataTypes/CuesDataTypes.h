// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueInterface.h"
#include "GameplayEffect.h"
#include "UObject/Object.h"
#include "CuesDataTypes.generated.h"

struct FActiveEffectSyncDataContainer;
struct FActiveGameplayEffectsContainer;
struct FNetSerializeParams;

/**
 * Cues Executions: These are replicated structs in RPCs that mirror ability system component original RPCs
 * (which are now just function calls that add to the Execution container)
 * 
 * adding and removing are all triggered part of the sync state. will be predicted and corrected if needed
 * 
 * Execution however are events, this is my understanding of how previous prediction worked with them :
 * Server will send the RPC to everyone when executing a cue,
 * - if that execution happened within a prediction window that was initiated by a client sending it in a server RPC to do something (contains a prediction key)
 * - the client that sent that prediction key when receiving the multicast will just ignore it.
 * - if client mis-predicts an execution.. nothing happens, server doesn't trigger it and doesn't replicate it to everyone else.
 * - if server sent a cue without any client initiating it, everyone will trigger it.
 *
 * With the new system it's different :
 * 
 * The biggest different is now they rely on net identical function of the cue to know if predicting client triggered it or not.
 * 
 * 1 - Client :  predicts a cue , saves it in a buffer for a duration equal to his round trip time.
 * if he received an execution from the server , he compares it in his buffered cues, if any return true in NetIdentical
 * the cue will be discarded.
 * this is the trickiest part, executing same cue with exact same data multiple time in a row on server only and then sent
 * within the client round trip time (normally it won't be much above 200ms) can be discarded .
 * this usually would never happen, unless in weird cases
 * Example : if the same weapon shoots same spot multiple time and a cue execution spawns a decal at hit location that is not predicted (weird)
 * the client might not accept all of them but will at least accept 1.
 * if a weapon shoots multiple shots at once and can end up with same data , bundle them together or make each cue unique by passing a shot count
 * for example in a custom effect context with NetIdentical Override. you would probably never encounter an issue from this.
 * 
 * 2 - Server: just dispatches the cues and sends them (can be improved to send all cues from that frame in bundled RPCs)
 *
 * 3 - Simulated proxies, just receive from the server, put the cues in the buffer and invoke the execution when their interpolation frame
 * is >= than the cue execution frame (assigned on server). this makes the cues for sim proxies perfectly align with other state.
 * like jump and show effect under feet, guaranteed you will see this happen together in sync.
 * 
 * 
 * when a cue is invoked it calls normal GAS InvokeGameplayCueEvent which routes it to the CueManager
 * 
 *
 * I am not very convinced with Net Prediction Net Cues and how they work. i think instant events should be kept to RPCs
 * that is why I made this and didn't use them. this implementation can be extended to add the ability to "rollback/undo" a cue
 * if it was mis-predicted or no confirmation from server was received. this seemed unnecessary right now, execution are 1 shot events
 * they shouldn't cause a "state" change that has to be "undone", can't undo a burst VFX/SFX. if you don't want something predicted
 * just call ExecuteGameplayCueEvent or any function that executes a cue on authority only.
 * Old Multicast functions of Ability System component, are no longer RPC.
 * this is a bit of an ugly hack but oh well am shoe horning NPP into GAS , it's not going to be all pretty. 
 */
#pragma region Executions
UENUM()
enum class ECueExecutionType : int8
{
	ENone,
	ESpec,
	EParams,
	EMultiParams,
	EEffect,
	EMultiEffect,
};
USTRUCT()
struct FCueExecution_Spec
{
	GENERATED_USTRUCT_BODY()
	FCueExecution_Spec()
	{ 
	}
	
	FCueExecution_Spec(const FGameplayEffectSpecForRPC& InSpec, const int32& Frame)
		: ExecutionFrame(Frame)
		, Spec(InSpec)
	{ 
	}
	
	UPROPERTY()
	int32 ExecutionFrame = INDEX_NONE;
	UPROPERTY()
	FGameplayEffectSpecForRPC Spec;

	bool NetIdentical(const FCueExecution_Spec& Other) const;
};
USTRUCT()
struct FCueExecution_Params
{
	GENERATED_USTRUCT_BODY()
	FCueExecution_Params()
	{ 
	}
	
	FCueExecution_Params(const FGameplayTag& Tag,const FGameplayCueParameters& Parameters,const int32& Frame)
		: ExecutionFrame(Frame)
		, CueTag(Tag)
		, GameplayCueParameters(Parameters)
	{ 
	}

	bool NetIdentical(const FCueExecution_Params& Other) const;

	UPROPERTY()
	int32 ExecutionFrame = INDEX_NONE;
	UPROPERTY()
	FGameplayTag CueTag;
	UPROPERTY()
	FGameplayCueParameters GameplayCueParameters;
};
USTRUCT()
struct FCuesExecutionMulti_Params
{
	GENERATED_USTRUCT_BODY()
	
	FCuesExecutionMulti_Params()
	{ 
	}
	
	FCuesExecutionMulti_Params(const FGameplayTagContainer& Tags,const FGameplayCueParameters& Parameters,const int32& Frame)
		: ExecutionFrame(Frame)
		, CueTags(Tags)
		, GameplayCueParameters(Parameters)
	{ 
	}

	bool NetIdentical(const FCuesExecutionMulti_Params& Other) const;
	
	UPROPERTY()
	int32 ExecutionFrame = INDEX_NONE;
	UPROPERTY()
	FGameplayTagContainer CueTags;
	UPROPERTY()
	FGameplayCueParameters GameplayCueParameters;
};
USTRUCT()
struct FCueExecution_EffectContext
{
	GENERATED_USTRUCT_BODY()
	
	FCueExecution_EffectContext()
	{ 
	}
	
	FCueExecution_EffectContext(const FGameplayTag& Tag,const FGameplayEffectContextHandle& Parameters,const int32& Frame)
		: ExecutionFrame(Frame)
		, CueTag(Tag)
		, EffectContext(Parameters)
	{ 
	}

	bool NetIdentical(const FCueExecution_EffectContext& Other) const;
	
	UPROPERTY()
	int32 ExecutionFrame = INDEX_NONE;
	UPROPERTY()
	FGameplayTag CueTag;
	UPROPERTY()
	FGameplayEffectContextHandle EffectContext;
};
USTRUCT()
struct FCueExecutionMulti_EffectContext
{

	GENERATED_USTRUCT_BODY()
	FCueExecutionMulti_EffectContext()
	{ 
	}
	
	FCueExecutionMulti_EffectContext(const FGameplayTagContainer& Tags,const FGameplayEffectContextHandle& Parameters,const int32& Frame)
		: ExecutionFrame(Frame)
		, CueTags(Tags)
		, EffectContext(Parameters)
	{ 
	}

	bool NetIdentical(const FCueExecutionMulti_EffectContext& Other) const;
	
	UPROPERTY()
	int32 ExecutionFrame = INDEX_NONE;
	UPROPERTY()
	FGameplayTagContainer CueTags;
	UPROPERTY()
	FGameplayEffectContextHandle EffectContext;
};

USTRUCT()
struct FGameplayCueExecution
{

	GENERATED_USTRUCT_BODY()
	FGameplayCueExecution()
	{ 
	}
	// helper constructors
	FGameplayCueExecution(const FGameplayEffectSpecForRPC& Spec,const int32& Frame)
		: ExecutionType(ECueExecutionType::ESpec)
		, SpecExecution(Spec, Frame)
	{ 
	}

	FGameplayCueExecution(const FCueExecution_Spec& Spec)
		: ExecutionType(ECueExecutionType::ESpec)
		, SpecExecution(Spec)
	{ 
	}
	FGameplayCueExecution(const FGameplayTag& Tag,const FGameplayCueParameters& Params,const int32& Frame)
		: ExecutionType(ECueExecutionType::EParams)
		, ParamsExecution(Tag,Params,Frame)
	{ 
	}
	FGameplayCueExecution(const FCueExecution_Params& Params)
		: ExecutionType(ECueExecutionType::EParams)
		, ParamsExecution(Params)
	{ 
	}

	FGameplayCueExecution(const FGameplayTagContainer& Tags,const FGameplayCueParameters& Params,const int32& Frame)
		: ExecutionType(ECueExecutionType::EParams)
		, MultiParamsExecution(Tags,Params,Frame)
	{ 
	}

	FGameplayCueExecution(const FCuesExecutionMulti_Params& Params)
		: ExecutionType(ECueExecutionType::EParams)
		, MultiParamsExecution(Params)
	{ 
	}
	
	FGameplayCueExecution(const FGameplayTag& Tag,const FGameplayEffectContextHandle& Context,const int32& Frame)
		: ExecutionType(ECueExecutionType::EEffect)
		, EffectExecution(Tag,Context,Frame)
	{ 
	}

	FGameplayCueExecution(const FCueExecution_EffectContext& Context)
		: ExecutionType(ECueExecutionType::EEffect)
		, EffectExecution(Context)
	{ 
	}

	FGameplayCueExecution(const FGameplayTagContainer& Tags,const FGameplayEffectContextHandle& Context,const int32& Frame)
		: ExecutionType(ECueExecutionType::EMultiEffect)
		, MultiEffectExecution(Tags,Context,Frame)
	{ 
	}

	FGameplayCueExecution(const FCueExecutionMulti_EffectContext& Context)
		: ExecutionType(ECueExecutionType::EMultiEffect)
		, MultiEffectExecution(Context)
	{ 
	}
	

	UPROPERTY()
	ECueExecutionType ExecutionType = ECueExecutionType::ENone;
	
	UPROPERTY()
	bool bDispatched = false;
	
	UPROPERTY()
	FCueExecution_Spec SpecExecution;
	
	UPROPERTY()
	FCueExecution_Params ParamsExecution;
	
	UPROPERTY()
	FCuesExecutionMulti_Params MultiParamsExecution;
	
	UPROPERTY()
	FCueExecution_EffectContext EffectExecution;
	
	UPROPERTY()
	FCueExecutionMulti_EffectContext MultiEffectExecution;

	int32 GetExecutionFrame() const
	{
		switch (ExecutionType)
		{
		case ECueExecutionType::ESpec:
			{
				return SpecExecution.ExecutionFrame;
			}
		case ECueExecutionType::EParams:
			{
				return ParamsExecution.ExecutionFrame;
			}
		case ECueExecutionType::EMultiParams:
			{
				return MultiParamsExecution.ExecutionFrame;
			}
		case ECueExecutionType::EEffect:
			{
				return EffectExecution.ExecutionFrame;
			}
		case ECueExecutionType::EMultiEffect:
			{
				return MultiEffectExecution.ExecutionFrame;
			}
		case ECueExecutionType::ENone:
			{
				return INDEX_NONE;
			}
		}
		return INDEX_NONE;
	}

	void InvokeGameplayCue(UNpAbilitySystemComponent* OwningComponent);
};

USTRUCT()
struct FGameplayCueExecutionsContainer
{
	GENERATED_USTRUCT_BODY()
	FGameplayCueExecutionsContainer()
	{ 
	}
	
	void AddCueExecution(const FGameplayCueExecution& Execution, const ENetRole& Role);

	void OnExecutionReceived(const FGameplayCueExecution& Execution, const ENetRole& Role);

	void DispatchCues(UNpAbilitySystemComponent* OwningComponent,const int32& Frame, const ENetRole& Role,const int32 PruneFrames = 15);

private:
	UPROPERTY()
	TArray<FGameplayCueExecution> SavedCues;
	// if we try to add a cue while we are dispatching it will go into pending cues until we are done.
	UPROPERTY()
	TArray<FGameplayCueExecution> PendingCues;
	// the lock should not be needed we dispatch at a special time, after simulation tick,
	//but before rest of the world and actors tick. just there for peace of mind, we can't add new cues while dispatching
	
	bool bIsLocked = false; 
	
};
#pragma endregion 
/**
 * Mirror data For Active Cues in the sync state. this is needed because cues do in fact effect the sync state by adding tags.
 * having them in sync state ensure they trigger for interpolated proxies at the exact right time the action associated with it is being performed.
 * (E.g : health dropping + hit effect cue at same time)
 * this replaces Active/WhileActive , Removed events 
 * 
 */
#pragma region Active Cues
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveCueSyncData
{
	GENERATED_USTRUCT_BODY()
	FActiveCueSyncData() {};
	FActiveCueSyncData(const FActiveGameplayCue& ActiveCue);

	// The Id is needed to facilitate delta replication, if 2 cues have same tag and same ID received from server update means they are identical.
	// cues don't mutate their params after being applied so we can safely assume if it's same cue , it's same data.
	// tag alone is not enough because there could be multiple of the same cue with same tag.
	UPROPERTY()
	uint32 CueID = 0;

	// to autonomous proxy we only replicate the effect handle if it is valid and client will get the params from the effect directly.

	UPROPERTY()
	int32 EffectHandle = INDEX_NONE;
	
	UPROPERTY()
	FGameplayTag GameplayCueTag;

	UPROPERTY()
	FGameplayCueParameters Parameters;

	// this net serialization gets called after effects serialize
	bool NetSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects);
	// Cues Data Does not change once added, delta serialization just copies data from base we don't send anything.
	bool NetDeltaSerialize(const FNetSerializeParams& P);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FActiveCueSyncData& AuthorityState) const;
	void Interpolate(const FActiveCueSyncData& From, const FActiveCueSyncData& To, float Pct);

	
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FActiveCueSyncDataContainer
{
	GENERATED_USTRUCT_BODY()
	FActiveCueSyncDataContainer();
	FActiveCueSyncDataContainer(const FActiveGameplayCueContainer& CuesContainer);
	
	UPROPERTY()
	TArray<FActiveCueSyncData> ActiveCues;

	UPROPERTY()
	uint32 CuesIDCounter = 0;
	
	bool NetSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects);
	bool NetDeltaSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects);
	void ToString(FAnsiStringBuilderBase& Out) const;
	bool ShouldReconcile(const FActiveCueSyncDataContainer& AuthorityState) const;
	void Interpolate(const FActiveCueSyncDataContainer& From, const FActiveCueSyncDataContainer& To, float Pct);

	bool IsIdentical(const FActiveCueSyncDataContainer& AuthorityState) const;
	/** Does explicit check for gameplay cue tag and ID */
	bool HasExactCue(const FActiveCueSyncData& SyncedCue) const;

	static void GetCuesDifference(const FActiveCueSyncDataContainer& NewCuesContainer
		,const FActiveCueSyncDataContainer& OldCuesContainer,TArray<FActiveCueSyncData>& AddedCues,TArray<FActiveCueSyncData>& RemovedCues);

	static void FillGameplayCueContainer(const FActiveCueSyncDataContainer& SyncedCues,FActiveGameplayCueContainer& GameplayCues);
};
#pragma endregion



