// 2025 Yohoho Productions /  Sirkai


#include "DataTypes/CuesDataTypes.h"
#include "NetworkPredictionReplicationProxy.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/EffectsDataTypes.h"


bool FCueExecution_Spec::NetIdentical(const FCueExecution_Spec& Other) const
{
	if (Spec.Def != Other.Spec.Def)
	{
		return false;
	}
	if (Spec.Level != Other.Spec.Level)
	{
		return false;
	}
	if (Spec.AbilityLevel != Other.Spec.AbilityLevel)
	{
		return false;
	}
	// Temp naive check for num only.
	if (Spec.ModifiedAttributes.Num() != Other.Spec.ModifiedAttributes.Num())
	{
		return false;
	}
	if (Spec.AggregatedSourceTags != Other.Spec.AggregatedSourceTags)
	{
		return false;
	}
	if (Spec.AggregatedTargetTags != Other.Spec.AggregatedTargetTags)
	{
		return false;
	}
	if (Spec.EffectContext.IsValid() != Other.Spec.EffectContext.IsValid())
	{
		return false;
	}
	if (Spec.EffectContext.IsValid())
	{
		if (!Spec.EffectContext.Get()->NetIdentical(Other.Spec.EffectContext.Get()))
		{
			return false;
		}
	}
	
	return true;
}


bool FCueExecution_Params::NetIdentical(const FCueExecution_Params& Other) const
{
	if (CueTag != Other.CueTag)
	{
		return false;
	}
	// magnitudes are left out on purpose , we don't want the local player to mis-predict magnitude but fire the cue
	// to trigger a new one with correct magnitudes during re-simulation.
	//this seems like an arbitrary choice by me on what check if FGameplayCueParams are identical and same invocation.
	
	/*if (!FMath::IsNearlyEqual(GameplayCueParameters.NormalizedMagnitude , Other.GameplayCueParameters.NormalizedMagnitude))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(GameplayCueParameters.RawMagnitude , Other.GameplayCueParameters.RawMagnitude))
	{
		return false;
	}*/
	if (GameplayCueParameters.EffectContext.IsValid() != Other.GameplayCueParameters.EffectContext.IsValid())
	{
		return false;
	}
	if (!GameplayCueParameters.Location.Equals(Other.GameplayCueParameters.Location,5.f))
	{
		return false;
	}
	if (!GameplayCueParameters.Normal.Equals(Other.GameplayCueParameters.Normal))
	{
		return false;
	}
	if (GameplayCueParameters.Instigator != Other.GameplayCueParameters.Instigator)
	{
		return false;
	}
	if (GameplayCueParameters.TargetAttachComponent != Other.GameplayCueParameters.TargetAttachComponent)
	{
		return false;
	}
	if (GameplayCueParameters.GameplayEffectLevel != Other.GameplayCueParameters.GameplayEffectLevel)
	{
		return false;
	}
	if (GameplayCueParameters.AbilityLevel != Other.GameplayCueParameters.AbilityLevel)
	{
		return false;
	}
	if (GameplayCueParameters.EffectContext.IsValid())
	{
		if (!GameplayCueParameters.EffectContext.Get()->NetIdentical(Other.GameplayCueParameters.EffectContext.Get()))
		{
			return false;
		}
	}
	return true;
}


bool FCuesExecutionMulti_Params::NetIdentical(const FCuesExecutionMulti_Params& Other) const
{
	if (CueTags != Other.CueTags)
	{
		return false;
	}
	// magnitudes are left out on purpose , we don't want the local player to mis-predict magnitude but fire the cue
	// to trigger a new one with correct magnitudes during re-simulation.
	//this seems like an arbitrary choice by me on what check if FGameplayCueParams to be from same invocation or not.
	
	/*if (!FMath::IsNearlyEqual(GameplayCueParameters.NormalizedMagnitude , Other.GameplayCueParameters.NormalizedMagnitude))
	{
		return false;
	}
	if (!FMath::IsNearlyEqual(GameplayCueParameters.RawMagnitude , Other.GameplayCueParameters.RawMagnitude))
	{
		return false;
	}*/
	if (GameplayCueParameters.EffectContext.IsValid() != Other.GameplayCueParameters.EffectContext.IsValid())
	{
		return false;
	}
	if (!GameplayCueParameters.Location.Equals(Other.GameplayCueParameters.Location,5.f))
	{
		return false;
	}
	if (!GameplayCueParameters.Normal.Equals(Other.GameplayCueParameters.Normal))
	{
		return false;
	}
	if (GameplayCueParameters.Instigator != Other.GameplayCueParameters.Instigator)
	{
		return false;
	}
	if (GameplayCueParameters.TargetAttachComponent != Other.GameplayCueParameters.TargetAttachComponent)
	{
		return false;
	}
	if (GameplayCueParameters.GameplayEffectLevel != Other.GameplayCueParameters.GameplayEffectLevel)
	{
		return false;
	}
	if (GameplayCueParameters.AbilityLevel != Other.GameplayCueParameters.AbilityLevel)
	{
		return false;
	}
	if (GameplayCueParameters.EffectContext.IsValid())
	{
		if (!GameplayCueParameters.EffectContext.Get()->NetIdentical(Other.GameplayCueParameters.EffectContext.Get()))
		{
			return false;
		}
	}
	return true;
}


bool FCueExecution_EffectContext::NetIdentical(const FCueExecution_EffectContext& Other) const
{
	if (CueTag != Other.CueTag)
	{
		return false;
	}
	if (EffectContext.IsValid() != Other.EffectContext.IsValid())
	{
		return false;
	}
	if (EffectContext.IsValid())
	{
		if (!EffectContext.Get()->NetIdentical(Other.EffectContext.Get()))
		{
			return false;
		}
	}
	return true;
}

bool FCueExecutionMulti_EffectContext::NetIdentical(const FCueExecutionMulti_EffectContext& Other) const
{
	if (CueTags != Other.CueTags)
	{
		return false;
	}
	if (EffectContext.IsValid() != Other.EffectContext.IsValid())
	{
		return false;
	}
	if (EffectContext.IsValid())
	{
		if (!EffectContext.Get()->NetIdentical(Other.EffectContext.Get()))
		{
			return false;
		}
	}
	return true;
}

void FGameplayCueExecution::InvokeGameplayCue(UNpAbilitySystemComponent* OwningComponent)
{
	switch (ExecutionType)
	{
	case ECueExecutionType::ENone:
		{
			return;
		}
	case ECueExecutionType::ESpec:
		{
			OwningComponent->InvokeGameplayCueEvent(SpecExecution.Spec,EGameplayCueEvent::Executed);
			return;
		}
	case ECueExecutionType::EParams:
		{
			OwningComponent->InvokeGameplayCueEvent(ParamsExecution.CueTag,EGameplayCueEvent::Executed,ParamsExecution.GameplayCueParameters);
			return;
		}
	case ECueExecutionType::EMultiParams:
		{
			for (const FGameplayTag& Tag : MultiParamsExecution.CueTags)
			{
				OwningComponent->InvokeGameplayCueEvent(Tag,EGameplayCueEvent::Executed,MultiParamsExecution.GameplayCueParameters);
			}
			return;
		}
	case ECueExecutionType::EEffect:
		{
			OwningComponent->InvokeGameplayCueEvent(EffectExecution.CueTag,EGameplayCueEvent::Executed,EffectExecution.EffectContext);
			return;
		}
	case ECueExecutionType::EMultiEffect:
		{
			for (const FGameplayTag& Tag : MultiEffectExecution.CueTags)
			{
				OwningComponent->InvokeGameplayCueEvent(Tag,EGameplayCueEvent::Executed,MultiEffectExecution.EffectContext);
			}
		}
	}
}

void FGameplayCueExecutionsContainer::AddCueExecution(const FGameplayCueExecution& Execution, const ENetRole& Role)
{
	// we can check in auto proxy here if we already have this specific cue executed and don't do it,
	// but this seems pointless since server is always sending all executions, if we mispredicted and didn't trigger one as we should
	// we'll get it from server
	if (bIsLocked)
	{
		PendingCues[PendingCues.Add(Execution)].bDispatched = false;
		return;
	}
	SavedCues[SavedCues.Add(Execution)].bDispatched = false;
}

void FGameplayCueExecutionsContainer::OnExecutionReceived(const FGameplayCueExecution& Execution, const ENetRole& Role)
{
	check(Execution.ExecutionType != ECueExecutionType::ENone)
	// if auto proxy, we are receiving this from server but we could have already predicted it, check if we have it in your saved cues
	// if not add it.
	if (Role == ROLE_AutonomousProxy)
	{
		FGameplayCueExecution* FoundCue = SavedCues.FindByPredicate([Execution](const FGameplayCueExecution& ExistingCue)
		{
			if (Execution.ExecutionType != ExistingCue.ExecutionType)
			{
				return false;
			}
			switch (ExistingCue.ExecutionType)
			{
			case ECueExecutionType::ENone:
				{
					return false;
				}
			case ECueExecutionType::ESpec:
				{
					return Execution.SpecExecution.NetIdentical(ExistingCue.SpecExecution);
				}
			case ECueExecutionType::EParams:
				{
					return Execution.ParamsExecution.NetIdentical(ExistingCue.ParamsExecution);
				}
			case ECueExecutionType::EMultiParams:
				{
					return Execution.MultiParamsExecution.NetIdentical(ExistingCue.MultiParamsExecution);
				}
			case ECueExecutionType::EEffect:
				{
					return Execution.EffectExecution.NetIdentical(ExistingCue.EffectExecution);
				}
			case ECueExecutionType::EMultiEffect:
				{
					return Execution.MultiEffectExecution.NetIdentical(ExistingCue.MultiEffectExecution);
				}
			}
			return false;
		});

		// cue found on sim proxy, we predicted it, so ignore it
		if (FoundCue)
		{
			return;
		}
	}
	
	AddCueExecution(Execution,Role);
}

void FGameplayCueExecutionsContainer::DispatchCues(UNpAbilitySystemComponent* OwningComponent, const int32& Frame,
	const ENetRole& Role,const int32 PruneFrames)
{
	bIsLocked = true;
	switch (Role)
	{
		// on the server we just dispatch, send and reset
	case ROLE_Authority:
		{
			for (FGameplayCueExecution& Execution : SavedCues)
			{
				Execution.InvokeGameplayCue(OwningComponent);
				OwningComponent->SendCueRPC(Execution);
			}
			SavedCues.Reset();
			break;
		}
	case ROLE_AutonomousProxy:
		{
			// first check if a cue should be pruned, this helps protect against execution we get from server past the prune threshhold.
			// old data, we should not execute it.
			for (int32 i = SavedCues.Num() - 1; i >= 0; i--)
			{
				FGameplayCueExecution& Execution = SavedCues[i];
				const bool Pruned = Execution.GetExecutionFrame() < Frame - PruneFrames;
				if (Execution.bDispatched == false && Pruned == false)
				{
					Execution.InvokeGameplayCue(OwningComponent);
					Execution.bDispatched = true;
				}
				if (Pruned)
				{
					SavedCues.RemoveAtSwap(i);
				}
			}
			break;
		}
	case ROLE_SimulatedProxy:
		{
			// sim proxies Hold cues until it's time to dispatch them based on the frame.
			// this is passed in as the interpolation frame so if we received a cue we didn't interpolate to yet
			// hold on to it.
			for (int32 i = SavedCues.Num() - 1; i >= 0; i--)
			{
				FGameplayCueExecution& Execution = SavedCues[i];
				if (Execution.bDispatched == false && Execution.GetExecutionFrame() >= Frame)
				{
					Execution.InvokeGameplayCue(OwningComponent);
					Execution.bDispatched = true;
				}
				if (Execution.bDispatched)
				{
					SavedCues.RemoveAtSwap(i);
				}
			}
			break;
		}
	case ROLE_None:
	case ROLE_MAX:
		{
			break;
		}
	}
	// just add them to the list for now, if they need to be executed or pruned it will be next frame.
	if (PendingCues.Num() > 0)
	{
		SavedCues.Append(PendingCues);
		PendingCues.Reset();
	}
	bIsLocked = false;
}


#pragma region Active Cue Sync Data
FActiveCueSyncData::FActiveCueSyncData(const FActiveGameplayCue& ActiveCue)
{
	GameplayCueTag = ActiveCue.GameplayCueTag;
	Parameters = ActiveCue.Parameters;
	CueID = ActiveCue.CueID;
	EffectHandle = ActiveCue.ActivatingEffectHandle;
}

bool FActiveCueSyncData::NetSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects)
{
	bool Result = true;
	if (P.ReplicationTarget == EReplicationProxyTarget::SimulatedProxy)
	{
		P.Ar.SerializeIntPacked(CueID);
		GameplayCueTag.NetSerialize_Packed(P.Ar,P.Map,Result);
		Parameters.NetSerialize(P.Ar,P.Map,Result);
		EffectHandle = INDEX_NONE;
		return Result;
	}
	// if auto proxy we serialize the handle, if it's valid we will fill the params directly from the effect
	P.Ar.SerializeIntPacked(CueID);
	GameplayCueTag.NetSerialize_Packed(P.Ar,P.Map,Result);
	if (P.Ar.IsSaving())
	{
		// on server if we can't find the active effect just send index none
		if (EffectHandle != INDEX_NONE)
		{
			if (!ActiveEffects.GetActiveEffectByHandle(EffectHandle))
			{
				EffectHandle = INDEX_NONE;
			}
		}
	}
	
	P.Ar << EffectHandle;
	if (EffectHandle == INDEX_NONE)
	{
		Parameters.NetSerialize(P.Ar,P.Map,Result);
	}
	return Result;
}

bool FActiveCueSyncData::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FActiveCueSyncData* BaseState = P.GetBaseDeltaState<FActiveCueSyncData>();
	CueID = BaseState->CueID;
	GameplayCueTag = BaseState->GameplayCueTag;
	Parameters = BaseState->Parameters;
	EffectHandle = BaseState->EffectHandle;
	return true;
}

void FActiveCueSyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s \n",TCHAR_TO_ANSI(*GameplayCueTag.ToString()));
	Out.Appendf("Cue ID %d \n",CueID);
}

bool FActiveCueSyncData::ShouldReconcile(const FActiveCueSyncData& AuthorityState) const
{
	if (CueID != AuthorityState.CueID)
	{
		return true;
	}
	if (GameplayCueTag != AuthorityState.GameplayCueTag)
	{
		return true;
	}
	if (EffectHandle != AuthorityState.EffectHandle)
	{
		return true;
	}
	return false;
}

void FActiveCueSyncData::Interpolate(const FActiveCueSyncData& From, const FActiveCueSyncData& To, float Pct)
{
	*this = To;
}
#pragma endregion

#pragma region FActiveCueSyncDataContainer , Data To Reconstruct FActiveGameplayCuesContainer
FActiveCueSyncDataContainer::FActiveCueSyncDataContainer()
{
}
FActiveCueSyncDataContainer::FActiveCueSyncDataContainer(const FActiveGameplayCueContainer& CuesContainer)
{
	ActiveCues.SetNum(CuesContainer.GameplayCues.Num());
	CuesIDCounter = CuesContainer.CueIDsCounter;
	for (int32 i = 0; i < CuesContainer.GameplayCues.Num(); ++i)
	{
		ActiveCues[i] = FActiveCueSyncData(CuesContainer.GameplayCues[i]);
	}
	// sorting them by ID so we can check index by index if they match.
	ActiveCues.Sort([](const FActiveCueSyncData& A, const FActiveCueSyncData& B) {
	return A.CueID < B.CueID;});
}
bool FActiveCueSyncDataContainer::NetSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects)
{
	bool bOutSuccess = true;
	uint32 NumActiveEffects = P.Ar.IsSaving() ? ActiveCues.Num() : 0;
	P.Ar.SerializeIntPacked(NumActiveEffects);
	P.Ar.SerializeIntPacked(CuesIDCounter);
	if (P.Ar.IsLoading())
	{
		ActiveCues.SetNum(NumActiveEffects);
	}
	for (uint32 i = 0; i < NumActiveEffects; ++i)
	{
		ActiveCues[i].NetSerialize(P,ActiveEffects);
	}
	return true;
}

bool FActiveCueSyncDataContainer::NetDeltaSerialize(const FNetSerializeParams& P,const FActiveEffectSyncDataContainer& ActiveEffects)
{
	return NetSerialize(P,ActiveEffects);
	/*const FActiveCueSyncDataContainer* BaseDeltaState = P.GetBaseDeltaState<FActiveCueSyncDataContainer>();
	bool SameAsBase = P.Ar.IsSaving() ? IsIdentical(*BaseDeltaState) : false;
	P.Ar.SerializeBits(&SameAsBase,1);
	if (SameAsBase)
	{
		ActiveCues = BaseDeltaState->ActiveCues;
	}
	else
	{
		if (P.Ar.IsSaving())
		{
			uint32 NumActiveEffects = ActiveCues.Num();
			P.Ar.SerializeIntPacked(NumActiveEffects); // 1. Effects Num
			FNetSerializeParams DeltaParams = P;
			for (uint32 i = 0; i < NumActiveEffects; ++i)
			{
				FActiveCueSyncData& CueSyncData = ActiveCues[i];
				int8 BaseDeltaStateIndex = INDEX_NONE;
				DeltaParams.BaseDeltaStatePtr = nullptr;
				for (int32 j = 0; j < BaseDeltaState->ActiveCues.Num(); ++j )
				{
					// it is impossible to have a base state where effect ended, and now it's active.
					// if the opposite happens it is still safe to copy from delta, no need to start sending it now.
					if (BaseDeltaState->ActiveCues[j].CueID == CueSyncData.CueID
						&& BaseDeltaState->ActiveCues[j].GameplayCueTag == CueSyncData.GameplayCueTag)
					{
						BaseDeltaStateIndex = j;
						DeltaParams.BaseDeltaStatePtr = &BaseDeltaState->ActiveCues[j];
					}
				}
				P.Ar << BaseDeltaStateIndex; // 2. Index from Base Delta State
				if (BaseDeltaStateIndex == INDEX_NONE)
				{
					ActiveCues[i].NetSerialize(DeltaParams,ActiveEffects); // 3. Element Net Serialize
				}
				else
				{
					ActiveCues[i].NetDeltaSerialize(DeltaParams); // 3. Element Net Serialize
				}
			}
		}
		else // is Loading
		{
			uint32 NumActiveEffects = 0;
			P.Ar.SerializeIntPacked(NumActiveEffects); // 1. Effects Num
			ActiveCues.SetNum(NumActiveEffects);
			FNetSerializeParams DeltaParams = P;
			for (uint32 i = 0; i < NumActiveEffects; ++i)
			{
				DeltaParams.BaseDeltaStatePtr = nullptr;
				int8 BaseDeltaStateIndex = 0;
				P.Ar << BaseDeltaStateIndex; // 2. Index from Base Delta State
				if (BaseDeltaStateIndex == INDEX_NONE)
				{
					ActiveCues[i].NetSerialize(DeltaParams,ActiveEffects); // 3. Element Net Serialize
				}
				else
				{
					DeltaParams.BaseDeltaStatePtr = &BaseDeltaState->ActiveCues[BaseDeltaStateIndex];
					ActiveCues[i].NetDeltaSerialize(DeltaParams); // 3. Element Net Serialize
				}
			}
		}
	}
	return true;*/
}

void FActiveCueSyncDataContainer::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Active Gameplay Cues :\n");
	for (int32 i = 0; i < ActiveCues.Num(); ++i)
	{
		ActiveCues[i].ToString(Out);
		Out.Append("\n");
	}
}
bool FActiveCueSyncDataContainer::ShouldReconcile(const FActiveCueSyncDataContainer& AuthorityState) const
{
	// effects should be in same order and match
	if (ActiveCues.Num() != AuthorityState.ActiveCues.Num())
	{
		UE_LOG(LogTemp,Error,TEXT("Different Cues Num"))
		return true;
	}
	for (int32 i = 0; i < ActiveCues.Num(); ++i)
	{
		if (ActiveCues[i].ShouldReconcile(AuthorityState.ActiveCues[i]))
		{
			UE_LOG(LogTemp,Error,TEXT("Different Cue Index %d , Local Tag : %s, Server Tag %s"),i
				,*ActiveCues[i].GameplayCueTag.ToString(),*AuthorityState.ActiveCues[i].GameplayCueTag.ToString())
			return true;
		}
	}
	return false;
}
void FActiveCueSyncDataContainer::Interpolate(const FActiveCueSyncDataContainer& From,
	const FActiveCueSyncDataContainer& To, float Pct)
{
	*this = To;
}

bool FActiveCueSyncDataContainer::IsIdentical(const FActiveCueSyncDataContainer& AuthorityState) const
{
	// effects should be in same order and match
	if (ActiveCues.Num() != AuthorityState.ActiveCues.Num())
	{
		return false;
	}
	for (int32 i = 0; i < ActiveCues.Num(); ++i)
	{
		if (ActiveCues[i].ShouldReconcile(AuthorityState.ActiveCues[i]))
		{
			return false;
		}
	}
	return true;
}

bool FActiveCueSyncDataContainer::HasExactCue(const FActiveCueSyncData& SyncedCue) const
{
	for (int32 i = 0; i < ActiveCues.Num(); ++i)
	{
		if (ActiveCues[i].CueID == SyncedCue.CueID && ActiveCues[i].GameplayCueTag == SyncedCue.GameplayCueTag)
		{
			return true;
		}
	}
	return false;
}

void FActiveCueSyncDataContainer::GetCuesDifference(const FActiveCueSyncDataContainer& NewCuesContainer,
                                                    const FActiveCueSyncDataContainer& OldCuesContainer, TArray<FActiveCueSyncData>& AddedCues,
                                                    TArray<FActiveCueSyncData>& RemovedCues)
{
	AddedCues.Reserve(NewCuesContainer.ActiveCues.Num());
	RemovedCues.Reserve(OldCuesContainer.ActiveCues.Num());
	// loop through new container and check if not found in Old one means it's added
	for (const auto& SyncedCue : NewCuesContainer.ActiveCues)
	{
		if (!OldCuesContainer.HasExactCue(SyncedCue))
		{
			AddedCues.Add(SyncedCue);
		}
	}
	for (const auto& SyncedCue : OldCuesContainer.ActiveCues)
	{
		if (!NewCuesContainer.HasExactCue(SyncedCue))
		{
			RemovedCues.Add(SyncedCue);
		}
	}
}

void FActiveCueSyncDataContainer::FillGameplayCueContainer(const FActiveCueSyncDataContainer& SyncedCues,
	FActiveGameplayCueContainer& GameplayCues)
{
	GameplayCues.CueIDsCounter = SyncedCues.CuesIDCounter;
	GameplayCues.GameplayCues.Reset(SyncedCues.ActiveCues.Num());
	for (int32 i = 0; i < SyncedCues.ActiveCues.Num(); ++i)
	{
		const FActiveCueSyncData& SyncedCue = SyncedCues.ActiveCues[i];
		FActiveGameplayCue& NewCue = GameplayCues.GameplayCues[GameplayCues.GameplayCues.AddDefaulted()];
		NewCue.CueID = SyncedCue.CueID;
		NewCue.GameplayCueTag = SyncedCue.GameplayCueTag;
		NewCue.Parameters = SyncedCue.Parameters;
		NewCue.ActivatingEffectHandle = SyncedCue.EffectHandle;
	}
}

#pragma endregion
