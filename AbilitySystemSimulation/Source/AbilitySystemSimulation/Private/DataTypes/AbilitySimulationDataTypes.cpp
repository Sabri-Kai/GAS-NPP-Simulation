


#include "DataTypes/AbilitySimulationDataTypes.h"

#include "AbilitySimulationSettings.h"
#include "GameplayTagsManager.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionTrace.h"

#pragma region Full Sync State
bool FSyncedGameplayTagCount::NetSerialize(const FNetSerializeParams& P)
{
	bool bOutSuccess = true;
	uint32 MapSize = ExplicitTagCountMap.Num();
	P.Ar.SerializeIntPacked(MapSize);

	// Iterate through the map and serialize each key-value pair
	if (P.Ar.IsSaving())
	{
		for (auto& Pair : ExplicitTagCountMap)
		{
			FGameplayTag& Tag = Pair.Key;
			uint32 Count = Pair.Value;
			//ToDo : Switch Tag Serialization To Use NetSerialize_Packed,
			//This required changes to NPP to pass the package map in the FNetSerializeParams
			Tag.NetSerialize_Packed(P.Ar,P.Map,bOutSuccess);
			P.Ar.SerializeIntPacked(Count);
		}
	}
	else if (P.Ar.IsLoading())
	{
		
		ExplicitTagCountMap.Empty(); 
		for (uint32 i = 0; i < MapSize; ++i)
		{
			FGameplayTag Tag;
			uint32 Count;
			Tag.NetSerialize_Packed(P.Ar,P.Map,bOutSuccess);
			P.Ar.SerializeIntPacked(Count);
			ExplicitTagCountMap.Add(Tag, Count); 
		}
	}
	return bOutSuccess;
}

bool FSyncedGameplayTagCount::NetDeltaSerialize(const FNetSerializeParams& P)
{
	bool bOutSuccess = true;
	const FSyncedGameplayTagCount* BaseDeltaState = P.GetBaseDeltaState<FSyncedGameplayTagCount>();
	FSyncedGameplayTagCount DeltaMapCount;
	DeltaMapCount.ExplicitTagCountMap.Reserve(BaseDeltaState->ExplicitTagCountMap.Num());
	if (P.Ar.IsSaving())
	{
		// calculate delta when saving
		//Get Values in current tags add them to delta state if not found or difference if found
		for (const auto& Pair : ExplicitTagCountMap)
		{
			if (const int32* FoundInBase = BaseDeltaState->ExplicitTagCountMap.Find(Pair.Key))
			{
				if (*FoundInBase != Pair.Value)
				{
					DeltaMapCount.ExplicitTagCountMap.Add(Pair.Key,Pair.Value - *FoundInBase);
				}
			}
			else
			{
				DeltaMapCount.ExplicitTagCountMap.Add(Pair.Key,Pair.Value);
			}
		}
		// Get Values in base and not in current and add them with negative value
		// this will get them removed
		for (auto& Pair : BaseDeltaState->ExplicitTagCountMap)
		{
			const int32* FoundInBase = ExplicitTagCountMap.Find(Pair.Key);
			if (FoundInBase == nullptr)
			{
				DeltaMapCount.ExplicitTagCountMap.Add(Pair.Key,-Pair.Value);
			}
		}

		bool HasDelta = DeltaMapCount.ExplicitTagCountMap.Num() > 0;
		P.Ar.SerializeBits(&HasDelta,1);
		if (HasDelta)
		{
			uint8 DeltaTagsNum = DeltaMapCount.ExplicitTagCountMap.Num();
			P.Ar << DeltaTagsNum;
			for (auto& Pair : DeltaMapCount.ExplicitTagCountMap)
			{
				FGameplayTag& Tag = Pair.Key;
				//ToDo : Switch Tag Serialization To Use NetSerialize_Packed,
				//This required changes to NPP to pass the package map in the FNetSerializeParams
				Tag.NetSerialize_Packed(P.Ar,P.Map,bOutSuccess);
				P.Ar << Pair.Value;
			}
		}
		
	}
	else // is loading
	{
		ExplicitTagCountMap = BaseDeltaState->ExplicitTagCountMap;
		bool HasDelta = false;
		P.Ar.SerializeBits(&HasDelta,1);
		if (HasDelta)
		{
			uint8 DeltaTagsNum = 0;
			P.Ar << DeltaTagsNum;
			DeltaMapCount.ExplicitTagCountMap.Reserve(DeltaTagsNum);
			for (uint8 i = 0; i < DeltaTagsNum; ++i)
			{
				FGameplayTag Tag;
				int32 Count;
				Tag.NetSerialize_Packed(P.Ar,P.Map,bOutSuccess);
				P.Ar << Count;
				DeltaMapCount.ExplicitTagCountMap.Add(Tag, Count);
				int32& FoundInBaseDelta = ExplicitTagCountMap.FindOrAdd(Tag);
				FoundInBaseDelta = FoundInBaseDelta + Count;
				if (FoundInBaseDelta == 0)
				{
					ExplicitTagCountMap.Remove(Tag);
				}
			}
		}
		
	}
	return true;
}

void FSyncedGameplayTagCount::FillFromGameplayTagCountContainer(const FGameplayTagCountContainer& TagsCountContainer)
{
	ExplicitTagCountMap.Empty(TagsCountContainer.GetExplicitTagCountMap().Num());
	for (const auto& Pair : TagsCountContainer.GetExplicitTagCountMap())
	{
		if (Pair.Value > 0)
		{
			ExplicitTagCountMap.Add(Pair.Key, Pair.Value);
		}
	}
	ExplicitTagCountMap.Shrink();
}

void FSyncedGameplayTagCount::RemoveTags(const FGameplayTagCountContainer& TagsCountContainer)
{
	if (TagsCountContainer.GetExplicitTagCountMap().Num() <= 0)
	{
		return;
	}
	TArray<FGameplayTag> TagsToRemove;
	TagsToRemove.Reserve(TagsCountContainer.GetExplicitTagCountMap().Num());
	for (const auto& Pair : TagsCountContainer.GetExplicitTagCountMap())
	{
		if (Pair.Value > 0)
		{
			if (int32* Count = ExplicitTagCountMap.Find(Pair.Key))
			{
				*Count -= Pair.Value;
				if (*Count <= 0)
				{
					TagsToRemove.Add(Pair.Key);
				}
			}
		}
	}

	// remove any tags that their count is not 0 or negative
	if (TagsToRemove.Num() > 0)
	{
		for (const auto& Tag : TagsToRemove)
		{
			ExplicitTagCountMap.Remove(Tag);
		}
	}
}

void FSyncedGameplayTagCount::AddTags(const FGameplayTagCountContainer& TagsCountContainer)
{
	if (TagsCountContainer.GetExplicitTagCountMap().Num() <= 0)
	{
		return;
	}
	for (const auto& Pair : TagsCountContainer.GetExplicitTagCountMap())
	{
		if (Pair.Value > 0)
		{
			if (int32& Count = ExplicitTagCountMap.FindOrAdd(Pair.Key))
			{
				Count += Pair.Value;
			}
		}
	}
}

void FSyncedGameplayTagCount::ToString(FAnsiStringBuilderBase& Out) const
{
	TArray<FGameplayTag> Tags;
	ExplicitTagCountMap.GenerateKeyArray(Tags);


	UGameplayTagsManager& TagManager = UGameplayTagsManager::Get();

	// Sort the TagsArray so they are easier to compare in the debugger
	Tags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		// Get indices of tags based on their definition order
		int32 IndexA = GetTypeHash(A);
		int32 IndexB = GetTypeHash(B);

		// Compare the indices
		return IndexA < IndexB;
	});
	for (const FGameplayTag& Tag : Tags)
	{
		uint32 Count = ExplicitTagCountMap[Tag];
		Out.Appendf("Tag %s : %d \n",TCHAR_TO_ANSI(*Tag.ToString()),Count);
	}
}

bool FSyncedGameplayTagCount::operator==(const FSyncedGameplayTagCount& Other) const
{
	if(ExplicitTagCountMap.Num() != Other.ExplicitTagCountMap.Num())
	{
		return false;
	}
	for (const auto& Pair : ExplicitTagCountMap)
	{
		const int32* AuthorityTag = Other.ExplicitTagCountMap.Find(Pair.Key);
		if (!AuthorityTag)
		{
			return false;
		}
		if (*AuthorityTag != Pair.Value)
		{
			return false;
		}
	}
	return true;
}

bool FSyncedGameplayTagCount::operator!=(const FSyncedGameplayTagCount& Other) const
{
	return !(*this == Other);
}

bool FSyncedTarget::NetSerialize(const FNetSerializeParams& P)
{
	bool HasTarget = P.Ar.IsSaving() ? IsValid(Target) : false;
	P.Ar.SerializeBits(&HasTarget,1);
	if (HasTarget)
	{
		P.Ar << Target;
		bool Success = true;
		TargetType.NetSerialize(P.Ar,P.Map,Success);
	}
	else
	{
		TargetType = FGameplayTag();
		Target = nullptr;
	}
	return true;
}

bool FSyncedTarget::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FSyncedTarget* BaseDelta = P.GetBaseDeltaState<FSyncedTarget>();
	check(BaseDelta);
	bool HasTarget = P.Ar.IsSaving() ? IsValid(Target) : false;
	P.Ar.SerializeBits(&HasTarget,1);
	if (!HasTarget)
	{
		TargetType = FGameplayTag();
		Target = nullptr;
		return true;
	}
	bool DifferentTarget = P.Ar.IsSaving() ? BaseDelta->Target != Target : false;
	P.Ar.SerializeBits(&DifferentTarget,1);
	if (DifferentTarget)
	{
		P.Ar << Target;
	}
	else
	{
		Target = BaseDelta->Target;
	}
	bool DifferentType = P.Ar.IsSaving() ? BaseDelta->TargetType != TargetType : false;
	P.Ar.SerializeBits(&DifferentType,1);
	if (DifferentType)
	{
		bool Success = true;
		TargetType.NetSerialize(P.Ar,P.Map,Success);
	}
	else
	{
		TargetType = BaseDelta->TargetType;
	}
	return true;
}

void FSyncedTarget::ToString(FAnsiStringBuilderBase& Out) const
{
	if (Target)
	{
		Out.Appendf("Target :%s",TCHAR_TO_ANSI(*GetNameSafe(Target)));
		Out.Appendf("Target Type :%s",*TargetType.ToString());
	}
	else
	{
		Out.Appendf("No Target");
	}
	
}

bool FSyncedTarget::operator==(const FSyncedTarget& Other) const
{
	return  Target == Other.Target && TargetType == Other.TargetType;
}

bool FSyncedTarget::operator!=(const FSyncedTarget& Other) const
{
	return !(*this == Other);
}

FAbilitySimSyncState::FAbilitySimSyncState()
: bSuppressGrantAbility(false)
, UserAbilityActivationInhibited(false)
{
}

void FAbilitySimSyncState::NetSerialize(const FNetSerializeParams& P)
{
	if (P.BaseDeltaStatePtr)
	{
		NetDeltaSerialize(P);
		return;
	}
	bool bSuccess = true;
	if (P.ReplicationTarget == EReplicationProxyTarget::SimulatedProxy)
	{
		GameplayTagCountContainer.NetSerialize(P);
		AttributeSets.NetSerialize(P);
		MontageSimulatorData.NetSerialize(P);
		ProjectilesCollection.NetSerialize(P);
		SyncedCues.NetSerialize(P,ActiveGameplayEffects);
		if (P.Ar.IsLoading())
		{
			BlockedAbilityTags.ExplicitTagCountMap.Empty();
			bSuppressGrantAbility = false;
			UserAbilityActivationInhibited = false;
			ActivatableAbilitiesHandleCount = 0;
			Abilities.Empty();
			ActiveGameplayEffects.ActiveEffects.Empty();
			ActiveGameplayEffects.ActiveEffectsHandleCount = 0;
		}
		
		return;
	}
	//Tags
	BlockedAbilityTags.NetSerialize(P);
	GameplayTagCountContainer.NetSerialize(P);

	//Abilities
	P.Ar.SerializeBits(&bSuppressGrantAbility,1);
	P.Ar.SerializeBits(&UserAbilityActivationInhibited,1);
	P.Ar.SerializeIntPacked(ActivatableAbilitiesHandleCount);
	Abilities.NetSerialize(P);
	ActiveGameplayEffects.NetSerialize(P);
	AttributeSets.NetSerialize(P);


	//Montage Playback
	MontageSimulatorData.NetSerialize(P);
	//Synced Target
	SyncedTarget.NetSerialize(P);
	// Projectiles
	ProjectilesCollection.NetSerialize(P);
	// cues
	SyncedCues.NetSerialize(P,ActiveGameplayEffects);
}

void FAbilitySimSyncState::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FAbilitySimSyncState* BaseState = P.GetBaseDeltaState<FAbilitySimSyncState>();
	ensure(BaseState);
	
	bool bSuccess = true;
	if (P.ReplicationTarget == EReplicationProxyTarget::SimulatedProxy)
	{
		FNetSerializeParams DeltaParams = P;
		DeltaParams.BaseDeltaStatePtr = &BaseState->GameplayTagCountContainer;
		GameplayTagCountContainer.NetDeltaSerialize(DeltaParams);
		DeltaParams.BaseDeltaStatePtr = &BaseState->AttributeSets;
		AttributeSets.NetDeltaSerialize(DeltaParams);
		MontageSimulatorData.NetSerialize(P);
		DeltaParams.BaseDeltaStatePtr = &BaseState->ProjectilesCollection;
		ProjectilesCollection.NetDeltaSerialize(DeltaParams);
		DeltaParams.BaseDeltaStatePtr = &BaseState->SyncedCues;
		SyncedCues.NetDeltaSerialize(P,ActiveGameplayEffects);
		if (P.Ar.IsLoading())
		{
			BlockedAbilityTags.ExplicitTagCountMap.Empty();
			bSuppressGrantAbility = false;
			UserAbilityActivationInhibited = false;
			ActivatableAbilitiesHandleCount = 0;
			Abilities.Empty();
			ActiveGameplayEffects.ActiveEffects.Empty();
			ActiveGameplayEffects.ActiveEffectsHandleCount = 0;
		}
		
		return;
	}
	FNetSerializeParams DeltaParams = P;
	
	DeltaParams.BaseDeltaStatePtr = &BaseState->BlockedAbilityTags;
	BlockedAbilityTags.NetDeltaSerialize(DeltaParams);
	DeltaParams.BaseDeltaStatePtr = &BaseState->GameplayTagCountContainer;
	GameplayTagCountContainer.NetDeltaSerialize(DeltaParams);
	DeltaParams.BaseDeltaStatePtr = &BaseState->SyncedTarget;
	SyncedTarget.NetDeltaSerialize(DeltaParams);

	//Abilities
	P.Ar.SerializeBits(&bSuppressGrantAbility,1);
	P.Ar.SerializeBits(&UserAbilityActivationInhibited,1);
	
	// Serialize Abilities ID count as delta if changed, it will always be under 7 bits and SerializeIntPacked will only use 1 byte
	bool NoChangeInAbilitiesHandleCount = P.Ar.IsSaving() ? ActivatableAbilitiesHandleCount == BaseState->ActivatableAbilitiesHandleCount : false;
	P.Ar.SerializeBits(&NoChangeInAbilitiesHandleCount,1);
	if (NoChangeInAbilitiesHandleCount)
	{
		ActivatableAbilitiesHandleCount = BaseState->ActivatableAbilitiesHandleCount;
	}
	else
	{
		P.Ar << ActivatableAbilitiesHandleCount;
	}
	
	// These Perform Delta Serialization Inside, so we provide the "BaseState" for them
	
	DeltaParams.BaseDeltaStatePtr = &BaseState->Abilities;
	Abilities.NetDeltaSerialize(DeltaParams);

	// Most Data For Active Effects doesn't change once applied
	DeltaParams.BaseDeltaStatePtr = &BaseState->ActiveGameplayEffects;
	ActiveGameplayEffects.NetDeltaSerialize(DeltaParams);

	// Most Attributes Do not change often, it is worth sending 1 bit saying value is the same instead of current and base value.
	DeltaParams.BaseDeltaStatePtr = &BaseState->AttributeSets;
	AttributeSets.NetDeltaSerialize(DeltaParams);

	//Montage Playback
	DeltaParams.BaseDeltaStatePtr = &BaseState->MontageSimulatorData;
	MontageSimulatorData.NetDeltaSerialize(P);

	DeltaParams.BaseDeltaStatePtr = &BaseState->ProjectilesCollection;
	ProjectilesCollection.NetDeltaSerialize(DeltaParams);

	DeltaParams.BaseDeltaStatePtr = &BaseState->SyncedCues;
	SyncedCues.NetDeltaSerialize(P,ActiveGameplayEffects);
}

void FAbilitySimSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Target :\n");
	SyncedTarget.ToString(Out);
	Out.Append("\nAttributes :\n");
	AttributeSets.ToString(Out);
	Out.Append("\nBlocked Abilities\n");
	BlockedAbilityTags.ToString(Out);
	Out.Append("\nOwned Explicit tags\n");
	GameplayTagCountContainer.ToString(Out);
	Out.Appendf(" \nSuppress Grant Ability : %s\n", bSuppressGrantAbility ? "true" : "false");
	Out.Appendf("Ability Activation Inhibited : %s\n", UserAbilityActivationInhibited ? "true" : "false");
	Out.Appendf("Activatable Abilities Handle Count : %d\n", ActivatableAbilitiesHandleCount);
	Abilities.ToString(Out);
	Out.Append("\n\n");
	ActiveGameplayEffects.ToString(Out);
	Out.Append("\nSynced Montage :\n");
	MontageSimulatorData.ToString(Out);
	ProjectilesCollection.ToString(Out);
	SyncedCues.ToString(Out);
}

bool FAbilitySimSyncState::ShouldReconcile(const FAbilitySimSyncState& AuthorityState) const
{
	UE_NP_TRACE_RECONCILE(BlockedAbilityTags != AuthorityState.BlockedAbilityTags,"Different Blocked Abilities");
	UE_NP_TRACE_RECONCILE(GameplayTagCountContainer != AuthorityState.GameplayTagCountContainer,"Different GameplayTag Count Container");
	UE_NP_TRACE_RECONCILE(bSuppressGrantAbility != AuthorityState.bSuppressGrantAbility,"Different Suppress Grant Ability");
	UE_NP_TRACE_RECONCILE(UserAbilityActivationInhibited != AuthorityState.UserAbilityActivationInhibited,"Different Activation Inhibited");
	UE_NP_TRACE_RECONCILE(ActivatableAbilitiesHandleCount != AuthorityState.ActivatableAbilitiesHandleCount,"Different Abilities Handle Count");
	UE_NP_TRACE_RECONCILE(ActiveGameplayEffects.ShouldReconcile(AuthorityState.ActiveGameplayEffects),"Different Effects");
	UE_NP_TRACE_RECONCILE(AttributeSets.ShouldReconcile(AuthorityState.AttributeSets),"Different Attributes");
	UE_NP_TRACE_RECONCILE(SyncedTarget != AuthorityState.SyncedTarget,"Different Target");
	UE_NP_TRACE_RECONCILE(ProjectilesCollection.ShouldReconcile(AuthorityState.ProjectilesCollection),"Different Projectiles");
	UE_NP_TRACE_RECONCILE(SyncedCues.ShouldReconcile(AuthorityState.SyncedCues),"Different Cues");
	// montage and ability are only ones that traces reconcile inside for now
	if (Abilities.ShouldReconcile(AuthorityState.Abilities))
	{
		return true;
	}
	if (MontageSimulatorData.ShouldReconcile(AuthorityState.MontageSimulatorData))
	{
		return true;
	}
	return false;
}

void FAbilitySimSyncState::Interpolate(const FAbilitySimSyncState* From, const FAbilitySimSyncState* To, float Pct)
{
	GameplayTagCountContainer = To->GameplayTagCountContainer;
	BlockedAbilityTags = To->BlockedAbilityTags;
	bSuppressGrantAbility = To->bSuppressGrantAbility;
	UserAbilityActivationInhibited = To->UserAbilityActivationInhibited;
	ActiveGameplayEffects = To->ActiveGameplayEffects;
	AttributeSets.Interpolate(From->AttributeSets,To->AttributeSets,Pct);
	MontageSimulatorData.Interpolate(From->MontageSimulatorData,To->MontageSimulatorData,Pct);
	SyncedTarget = To->SyncedTarget;
	Abilities = To->Abilities;
	ProjectilesCollection = To->ProjectilesCollection;
	SyncedCues = To->SyncedCues;
}
#pragma endregion

#pragma region Input Command
void FAbilityInputActionState::NetSerialize(const FNetSerializeParams& P)
{
	bool bActive = P.Ar.IsSaving() ? bTriggered || bStarted || bOngoing || bCanceled || bCompleted : false;
	P.Ar.SerializeBits(&bActive,1);
	if (bActive)
	{
		P.Ar.SerializeBits(&bTriggered,1);
		P.Ar.SerializeBits(&bStarted,1);
		P.Ar.SerializeBits(&bOngoing,1);
		P.Ar.SerializeBits(&bCanceled,1);
		P.Ar.SerializeBits(&bCompleted,1);
	}
	else
	{
		bTriggered = false;
		bStarted = false;
		bOngoing = false;
		bCanceled = false;
		bCompleted = false;
	}
	
}

void FAbilityInputActionState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Action State :\n");
	if (!bTriggered && !bStarted && !bOngoing && !bCanceled && !bCompleted)
	{
		Out.Append("None\n");
		return;
	}
	if(bTriggered)
	{
		Out.Append("Triggered \n");
	}
	if(bStarted)
	{
		Out.Append("Started \n");
	}
	if(bOngoing)
	{
		Out.Append("Ongoing \n");
	}
	if(bCanceled)
	{
		Out.Append("Canceled \n");
	}
	if(bCompleted)
	{
		Out.Append("Completed \n");
	}
}

FAbilitySimInputCmd::FAbilitySimInputCmd()
{
}

void FAbilitySimInputCmd::NetSerialize(const FNetSerializeParams& P)
{
	// don't need to serialize input for sim proxy
	if (P.ReplicationTarget == EReplicationProxyTarget::SimulatedProxy)
	{
		return;
	}
	uint8 ContextsNum = P.Ar.IsSaving() ? ActiveMappingContexts.Num() : 0;
	P.Ar << ContextsNum;

	if (P.Ar.IsLoading())
	{
		ActiveMappingContexts.SetNumZeroed(ContextsNum);
	}

	for (uint8 Index = 0; Index < ContextsNum; ++Index)
	{
		if (P.Ar.IsLoading())
		{
			ActiveMappingContexts[Index] = 0;
		}
		P.Ar << ActiveMappingContexts[Index];
	}

	// we assume player would not have more than 255 input actions active at once. seems like a safe bet!!
	ensureMsgf(InputActionStates.Num() < UINT8_MAX,TEXT("Trying To Send More than 255 active inputs at once. Not Allowed"));
	uint8 StatesNum = P.Ar.IsSaving() ? InputActionStates.Num() : 0;
	P.Ar << StatesNum;
	if (P.Ar.IsLoading())
	{
		InputActionStates.SetNum(StatesNum);
	}

	for (uint8 i = 0; i < StatesNum; ++i)
	{
		InputActionStates[i].NetSerialize(P);
	}
	bool bSuccess = true;
	bool HasMouseLoc = P.Ar.IsSaving() ? !MouseScreenLocation.IsNearlyZero() : false;
	P.Ar.SerializeBits(&HasMouseLoc,1);
	if (HasMouseLoc)
	{
		P.Ar << MouseScreenLocation;
	}
	else
	{
		MouseScreenLocation = FVector2D::ZeroVector;
	}

	bool HasCameraLoc = P.Ar.IsSaving() ? !CameraLocation.IsNearlyZero() : false;
	P.Ar.SerializeBits(&HasCameraLoc,1);
	if (HasCameraLoc)
	{
		CameraLocation.NetSerialize(P.Ar,P.Map,bSuccess);
	}
	else
	{
		CameraLocation = FVector::ZeroVector;
	}

	ScreenProjectionData.NetSerialize(P.Ar,P.Map,bSuccess);
	CustomInput.NetSerialize(P.Ar,P.Map,bSuccess);
}

void FAbilitySimInputCmd::ToString(FAnsiStringBuilderBase& Out) const
{
	TArray<const UInputAction*> ActiveInputActions;
	
	Out.Append("Active Mappings :\n");
	for (uint8 Index = 0; Index < ActiveMappingContexts.Num(); ++Index)
	{
		if (const UAbilitySimulationSettings* AbilitySettings = UAbilitySimulationSettings::Get())
		{
			if (AbilitySettings->AbilitySystemMappingContexts.IsValidIndex(ActiveMappingContexts[Index]))
			{
				const UInputMappingContext* MappingContext = AbilitySettings->AbilitySystemMappingContexts[ActiveMappingContexts[Index]].LoadSynchronous();
				Out.Appendf("%s \n",TCHAR_TO_ANSI(*GetNameSafe(MappingContext)));

				for (const FEnhancedActionKeyMapping& ActionKeyMapping : MappingContext->GetMappings())
				{
					ActiveInputActions.AddUnique(ActionKeyMapping.Action);
				}
			}
		}
	}
	
	Out.Append("Input Actions :");
	for (uint8 Index = 0; Index < InputActionStates.Num(); ++Index)
	{
		Out.Append("\n");

		if (const UInputAction* Action = ActiveInputActions[Index])
		{
			Out.Appendf("%s",TCHAR_TO_ANSI(*GetNameSafe(Action)));
		}
		Out.Append("\n");
		InputActionStates[Index].ToString(Out);
	}

	Out.Append("Custom Inputs :");

	// TODO: add string for each property 
}
#pragma endregion
