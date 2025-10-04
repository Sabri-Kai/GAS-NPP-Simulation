// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTypes/AbilitiesDataTypes.h"

#include "NetworkPredictionTrace.h"
#include "Library/AbilitySimulationLibrary.h"
#include "Tasks/BasePredictionTask.h"

#pragma region Active Ability Instance Data

FActiveAbilityInstanceData::FActiveAbilityInstanceData():
  bIsActive(false)
 ,bIsCancelable(false)
 ,bIsBlockingOtherAbilities(false)
 , ActivatedByInput(INDEX_NONE)
 , bHasEventData(false)
{
}

bool FActiveAbilityInstanceData::NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO)
{
	FArchive& Ar = Params.Ar;
	Ar.SerializeBits(&bIsActive,1);
	// if an instance is not active we do not need to serialize any of its data
	bool HasAbilityData = Ar.IsSaving() ? AbilitySyncedVars.SyncedVars.Num() > 0 : false;
	Ar.SerializeBits(&HasAbilityData, 1);
	if (HasAbilityData)
	{
		AbilitySyncedVars.NetSerialize(Params,AbilityCDO);
	}
	NetSerializeTrackedCues(Params);
	Ar << ActivatedByInput;
	Ar.SerializeBits(&bIsCancelable,1);
	Ar.SerializeBits(&bIsBlockingOtherAbilities,1);
	TaskDataCollection.NetSerialize(Params,AbilityCDO);
	Ar.SerializeBits(&bHasEventData, 1);
	if (bHasEventData)
	{
		CurrentEventData.NetSerialize(Params);
	}
	return true;
}

bool FActiveAbilityInstanceData::NetSerializeTrackedCues(const FNetSerializeParams& Params)
{
	bool bOutSuccess = true;
	FArchive& Ar = Params.Ar;
	uint32 TrackedCuesNum = TrackedGameplayCues.Num();
	bool HasCues = Ar.IsSaving() ? TrackedCuesNum > 0 : false;
	Ar.SerializeBits(&HasCues,1);
	if (HasCues)
	{
		Ar.SerializeIntPacked(TrackedCuesNum);
		if (Ar.IsSaving())
		{
			for (FGameplayTag& Tag : TrackedGameplayCues)
			{
				Tag.NetSerialize(Ar,Params.Map,bOutSuccess);
			}
		}
		else
		{
			TrackedGameplayCues.Empty(TrackedCuesNum);
			for (uint32 i = 0; i < TrackedCuesNum; i++)
			{
				FGameplayTag Tag;
				Tag.NetSerialize(Ar,Params.Map,bOutSuccess);
				TrackedGameplayCues.Add(Tag);
			}
		}
		
	}
	else
	{
		TrackedGameplayCues.Empty();
	}
	return bOutSuccess;
}

bool FActiveAbilityInstanceData::NetDeltaSerialize(const FNetSerializeParams& Params,
                                                   const UNpGameplayAbility* AbilityCDO)
{
	const FActiveAbilityInstanceData* BaseDelta = Params.GetBaseDeltaState<FActiveAbilityInstanceData>();
	check(BaseDelta)
	FArchive& Ar = Params.Ar;
	Ar.SerializeBits(&bIsActive,1);
	if(bIsActive || BaseDelta->bIsActive)
	{
		FNetSerializeParams DeltaParams = Params;
		
		bool bHasAbilitySyncVars = Ar.IsSaving() ? AbilitySyncedVars.SyncedVars.Num() > 0 : false;
		Ar.SerializeBits(&bHasAbilitySyncVars, 1);
		if (bHasAbilitySyncVars)
		{
			DeltaParams.BaseDeltaStatePtr = &BaseDelta->AbilitySyncedVars;
			AbilitySyncedVars.NetDeltaSerialize(DeltaParams,AbilityCDO);
		}
		//ToDo @Kai: Can send only tag difference if they are not the same?
		bool SameTrackedCues = Ar.IsSaving() ? AreTrackedCuesIdentical(BaseDelta->TrackedGameplayCues) : false;
		Ar.SerializeBits(&SameTrackedCues, 1);
		if (SameTrackedCues)
		{
			TrackedGameplayCues = BaseDelta->TrackedGameplayCues;
		}
		else
		{
			NetSerializeTrackedCues(Params);
		}
		
		Ar << ActivatedByInput;
		Ar.SerializeBits(&bIsCancelable,1);
		Ar.SerializeBits(&bIsBlockingOtherAbilities,1);
		DeltaParams.BaseDeltaStatePtr = &BaseDelta->TaskDataCollection;
		TaskDataCollection.NetDeltaSerialize(DeltaParams,AbilityCDO);

		// since this event data is only set during activation, it should be safe to just copy it if base state is active
		Ar.SerializeBits(&bHasEventData,1);
		if (bHasEventData)
		{
			// Event Data is only set during activation of an ability but given it's a protected member , user can mutate it.
			// this need more, need to ensure user can't mutate the event data . making it a private member.
			// this can't be done now because UNpGameplayAbility is a child and needs access to override this.
			check(BaseDelta->bHasEventData)
			CurrentEventData = BaseDelta->CurrentEventData;
		}
		
		return true;
	}
	// they are both inactive, copy the data from the base state
	*this = *BaseDelta;
	return true;
}

void FActiveAbilityInstanceData::ToString(FAnsiStringBuilderBase& Out, const UNpGameplayAbility* AbilityCDO) const
{
	Out.Appendf("     Is Active: %s\n", bIsActive ? "True" : "False");
	if (bIsActive)
	{
		if (ActivatedByInput == INDEX_NONE || !AbilityCDO->ActivationInputs.IsValidIndex(ActivatedByInput))
		{
			Out.Appendf("     Activated By Input: -1\n");
		}
		else
		{
			Out.Appendf("     Activated By Input: %s ,%s\n",TCHAR_TO_ANSI(*GetNameSafe(AbilityCDO->ActivationInputs[ActivatedByInput].InputAction)),TCHAR_TO_ANSI(*UEnum::GetValueAsString(AbilityCDO->ActivationInputs[ActivatedByInput].TriggerEvent)));
		}
		
		Out.Appendf("     Is Cancelable: %s\n", bIsCancelable ? "True" : "False");
		Out.Appendf("     Is BlockingOtherAbilities: %s\n", bIsBlockingOtherAbilities ? "True" : "False");
		if ( AbilitySyncedVars.SyncedVars.Num() > 0)
		{
			Out.Append("     Ability Synced variables :");
			AbilitySyncedVars.ToString(Out,AbilityCDO);
			Out.Append("\n\n");
		}
		else
		{
			Out.Append("     No Ability Data\n");
		}
		Out.Append("     Tasks :\n");
		TaskDataCollection.ToString(Out,AbilityCDO->GetTasksNames());
	}
}

void FActiveAbilityInstanceData::AddReferencedObjects(FReferenceCollector& Collector) const
{
	if (AbilitySyncedVars.SyncedVars.Num() > 0)
	{
		AbilitySyncedVars.AddStructReferencedObjects(Collector);
	}
	TaskDataCollection.AddStructReferencedObjects(Collector);
}

bool FActiveAbilityInstanceData::ShouldReconcile(const FActiveAbilityInstanceData& AuthorityState) const
{
	if (bIsActive != AuthorityState.bIsActive)
	{
		UE_NP_TRACE_RECONCILE(true,"Diff Ability Activation State")
	}
	if (bIsActive)
	{
		if (ActivatedByInput != AuthorityState.ActivatedByInput)
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Activated by input")
		}
		if (bIsCancelable != AuthorityState.bIsCancelable)
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Activated is Cancelable")
		}
		if (bIsBlockingOtherAbilities != AuthorityState.bIsBlockingOtherAbilities)
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Is Blocking another abilities")
		}
		if (AbilitySyncedVars.ShouldReconcile(AuthorityState.AbilitySyncedVars))
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Ability Sync Vars")
		}
		if (TaskDataCollection.ShouldReconcile(AuthorityState.TaskDataCollection))
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Ability Tasks Data")
		}
		if (!AreTrackedCuesIdentical(AuthorityState.TrackedGameplayCues))
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Tracked Cues")
		}
		if (bHasEventData != AuthorityState.bHasEventData)
		{
			UE_NP_TRACE_RECONCILE(true,"Diff Event Data")
		}
		if (bHasEventData)
		{
			if (CurrentEventData.ShouldReconcile(AuthorityState.CurrentEventData))
			{
				UE_NP_TRACE_RECONCILE(true,"Diff Event Data")
			}
		}
	}
	
	return false;
}

void FActiveAbilityInstanceData::Interpolate(const FActiveAbilityInstanceData& From, const FActiveAbilityInstanceData& To,
                                             float Pct)
{
	AbilitySyncedVars.Interpolate(From.AbilitySyncedVars,To.AbilitySyncedVars,Pct);
	TaskDataCollection.Interpolate(From.TaskDataCollection,To.TaskDataCollection,Pct);
}

bool FActiveAbilityInstanceData::AreTrackedCuesIdentical(const TSet<FGameplayTag>& OtherTrackedCues) const
{
	bool Identical = true;
	for (const FGameplayTag& Tag : TrackedGameplayCues)
	{
		if (!OtherTrackedCues.Contains(Tag))
		{
			Identical = false;
			break;
		}
	}
	// we found all the tags 
	if (Identical)
	{
		for (const FGameplayTag& Tag : OtherTrackedCues)
		{
			if (!TrackedGameplayCues.Contains(Tag))
			{
				Identical = false;
				break;
			}
		}
	}

	return Identical;
}

FActiveAbilityInstanceData& FActiveAbilityInstanceData::operator=(const FActiveAbilityInstanceData& Other)
{
	if (this != &Other)
	{
		bIsActive = Other.bIsActive;
		ActivatedByInput = Other.ActivatedByInput;
		bIsCancelable = Other.bIsCancelable;
		bIsBlockingOtherAbilities = Other.bIsBlockingOtherAbilities;
		AbilitySyncedVars = Other.AbilitySyncedVars;
		TaskDataCollection = Other.TaskDataCollection;
		TrackedGameplayCues = Other.TrackedGameplayCues;
		bHasEventData = Other.bHasEventData;
		CurrentEventData = Other.CurrentEventData;
	}

	return *this;
}

#pragma endregion 

#pragma region Active Ability Instances Collection
FActiveAbilitiesInstancesCollection::FActiveAbilitiesInstancesCollection()
{
}

bool FActiveAbilitiesInstancesCollection::NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO)
{
	NetSerializeDataArray(Params,AbilityCDO,ActiveAbilityInstances);
	return true;
}

bool FActiveAbilitiesInstancesCollection::NetDeltaSerialize(const FNetSerializeParams& Params,
	const UNpGameplayAbility* AbilityCDO)
{
	NetDeltaSerializeDataArray(Params,AbilityCDO,ActiveAbilityInstances);
	return true;
}

FActiveAbilitiesInstancesCollection& FActiveAbilitiesInstancesCollection::operator=(
	const FActiveAbilitiesInstancesCollection& Other)
{
	if (this != &Other)
	{
		// Deep copy active data blocks
		ActiveAbilityInstances.SetNum(Other.ActiveAbilityInstances.Num());
		for (int32 i = 0; i < Other.ActiveAbilityInstances.Num(); ++i)
		{
			ActiveAbilityInstances[i] = Other.ActiveAbilityInstances[i];
		}
	}

	return *this;
}

bool FActiveAbilitiesInstancesCollection::operator==(const FActiveAbilitiesInstancesCollection& Other) const
{
	return !ShouldReconcile(Other);
}

bool FActiveAbilitiesInstancesCollection::operator!=(const FActiveAbilitiesInstancesCollection& Other) const
{
	return !(*this == Other);
}

bool FActiveAbilitiesInstancesCollection::ShouldReconcile(const FActiveAbilitiesInstancesCollection& Other) const
{
	if (ActiveAbilityInstances.Num() != Other.ActiveAbilityInstances.Num())
	{
		return true;
	}
	for (int32 i = 0; i < ActiveAbilityInstances.Num(); ++i)
	{
		const FActiveAbilityInstanceData& AuthorityData = Other.ActiveAbilityInstances[i];
		const FActiveAbilityInstanceData& LocalData = ActiveAbilityInstances[i];
		if (LocalData.ShouldReconcile(AuthorityData))
		{
			return true;
		}
	}
	return false;
}

void FActiveAbilitiesInstancesCollection::Interpolate(const FActiveAbilitiesInstancesCollection& From,
	const FActiveAbilitiesInstancesCollection& To, float Pct)
{
	//ToDo Implement Proper Interpolation of active abilities. 
	ActiveAbilityInstances = To.ActiveAbilityInstances;
}

void FActiveAbilitiesInstancesCollection::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (int32 i = 0; i < ActiveAbilityInstances.Num(); ++i)
	{
		ActiveAbilityInstances[i].AddReferencedObjects(Collector);
	}
}
void FActiveAbilitiesInstancesCollection::ToString(FAnsiStringBuilderBase& Out,const UNpGameplayAbility* AbilityCDO) const
{
	Out.Append("   Instances : \n   {");
	for (int32 i = 0; i < ActiveAbilityInstances.Num(); ++i)
	{
		Out.Append("\n");
		ActiveAbilityInstances[i].ToString(Out,AbilityCDO);
	}
	Out.Append("   }");
}
TArray<FActiveAbilityInstanceData>::TConstIterator FActiveAbilitiesInstancesCollection::GetCollectionDataIterator() const
{
	return ActiveAbilityInstances.CreateConstIterator();
}
void FActiveAbilitiesInstancesCollection::NetSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO,TArray<FActiveAbilityInstanceData>& DataArray)
{
	if (AbilityCDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		if (Params.Ar.IsSaving())
		{
			DataArray[0].NetSerialize(Params,AbilityCDO);
		}
		else
		{
			DataArray.Empty(1);
			FActiveAbilityInstanceData LocalData;
			LocalData.NetSerialize(Params,AbilityCDO);
			DataArray.Add(LocalData);
		}
	}
	else if (AbilityCDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		FArchive& Ar = Params.Ar;
		uint8 NumActiveAbilities = Ar.IsSaving() ? DataArray.Num() : 0;
		Ar << NumActiveAbilities;
		if (Ar.IsSaving())
		{
			for (int32 i = 0 ; i < NumActiveAbilities ; ++i)
			{
				DataArray[i].NetSerialize(Params,AbilityCDO);
			}
		}
		else if (Ar.IsLoading())
		{
			DataArray.Empty(NumActiveAbilities);
			for (int32 i = 0 ; i < NumActiveAbilities ; ++i)
			{
				FActiveAbilityInstanceData LocalData;
				LocalData.NetSerialize(Params,AbilityCDO);
				DataArray.Add(LocalData);
			}
		}
	}
	else
	{
		DataArray.Empty();
	}
	
	
}

void FActiveAbilitiesInstancesCollection::NetDeltaSerializeDataArray(const FNetSerializeParams& Params,
	const UNpGameplayAbility* AbilityCDO, TArray<FActiveAbilityInstanceData>& DataArray)
{
	const FActiveAbilitiesInstancesCollection* BaseDelta = Params.GetBaseDeltaState<FActiveAbilitiesInstancesCollection>();
	check(BaseDelta)
	FNetSerializeParams DeltaParams = Params;
	if (AbilityCDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		DeltaParams.BaseDeltaStatePtr = &BaseDelta->ActiveAbilityInstances[0];
		if (Params.Ar.IsLoading())
		{
			DataArray.Empty(1);
		}
		FActiveAbilityInstanceData& LocalData = Params.Ar.IsSaving() ? DataArray[0] : DataArray.AddDefaulted_GetRef();
		LocalData.NetDeltaSerialize(DeltaParams,AbilityCDO);
		return;
	}
	
	if (AbilityCDO->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		FArchive& Ar = Params.Ar;
		uint8 NumActiveAbilities = Ar.IsSaving() ? DataArray.Num() : 0;
		Ar << NumActiveAbilities;
		if (Ar.IsLoading())
		{
			DataArray.Empty(NumActiveAbilities);
		}
		
		for (int32 i = 0 ; i < NumActiveAbilities ; ++i)
		{
			FActiveAbilityInstanceData& LocalData = Params.Ar.IsSaving() ? DataArray[i] : DataArray.AddDefaulted_GetRef();
			if (BaseDelta->ActiveAbilityInstances.IsValidIndex(i))
			{
				DeltaParams.BaseDeltaStatePtr = &BaseDelta->ActiveAbilityInstances[i];
				LocalData.NetDeltaSerialize(DeltaParams,AbilityCDO);
				continue;
			}
			LocalData.NetSerialize(Params,AbilityCDO);
		}
		return;
	}
	
	DataArray.Empty();
}

#pragma endregion 

#pragma region Activatable Ability Sync State

FActivatableAbilitySyncState::FActivatableAbilitySyncState()
  :ActivatableAbilityHandle(0)
  ,Level(0)
  ,GrantingGameplayEffectHandle(0)
  ,RemoveAfterActivation(false)
  ,ActiveCount(0)
{
}

bool FActivatableAbilitySyncState::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	bool IgnoredOutSuccess;
	
	Ar << AbilityClass;
	ensure(AbilityClass && AbilityClass->IsChildOf(UNpGameplayAbility::StaticClass()));
	uint32 HandleAsUint32 = Ar.IsSaving() ? ActivatableAbilityHandle : 0;
	Params.Ar.SerializeIntPacked(HandleAsUint32);
	ActivatableAbilityHandle = HandleAsUint32;
	Ar << Level;
	Ar.SerializeBits(&RemoveAfterActivation, 1);
	//Source Object
	bool ValidSourceObject = Ar.IsSaving() ? (SourceObject != nullptr && SourceObject->IsSupportedForNetworking()) : false;
	Ar.SerializeBits(&ValidSourceObject,1);
	if (ValidSourceObject)
	{
		Ar << SourceObject;
	}
	else
	{
		SourceObject = nullptr;
	}
	DynamicAbilityTags.NetSerialize(Ar,Params.Map,IgnoredOutSuccess);
	// Granting Effect
	bool ValidGrantingEffect = Ar.IsSaving() ? GrantingGameplayEffectHandle != INDEX_NONE : false;
	Ar.SerializeBits(&ValidGrantingEffect,1);
	if (ValidGrantingEffect)
	{
		Ar << GrantingGameplayEffectHandle;
	}
	else
	{
		GrantingGameplayEffectHandle = INDEX_NONE;
	}
	//Set By Caller
	bool HasSetByCaller = Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() > 0 : false;
	Ar.SerializeBits(&HasSetByCaller,1);
	if (HasSetByCaller)
	{
		uint32 NumElements =  Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() : 0;
		Ar.SerializeIntPacked(NumElements);

		if (Ar.IsSaving())
		{
			for (TTuple<FGameplayTag, float>& Pair : SetByCallerTagMagnitudes)
			{
				Pair.Key.NetSerialize(Ar, Params.Map, IgnoredOutSuccess);
				Ar << Pair.Value;
			}
		}
		else if (Ar.IsLoading())
		{
			SetByCallerTagMagnitudes.Empty(NumElements);

			for (uint32 i = 0; i < NumElements; ++i)
			{
				FGameplayTag Tag;
				float Magnitude;
				Tag.NetSerialize(Ar, Params.Map, IgnoredOutSuccess);
				Ar << Magnitude;
				SetByCallerTagMagnitudes.Add(Tag, Magnitude);
			}
		}
	}
	// don't need to replicate Active Instance or active count for 
	UNpGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
	if (AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		ActiveInstances.Empty();
		return true;
	}
	
	Ar << ActiveCount;

	// Instances
	ActiveInstances.NetSerialize(Params, AbilityClass.GetDefaultObject());
	return true;
}

bool FActivatableAbilitySyncState::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	const FActivatableAbilitySyncState* BaseDeltaState = Params.GetBaseDeltaState<FActivatableAbilitySyncState>();
	ensure(BaseDeltaState);
	
	bool SameLevel = false;
	bool SameSource = false;
	bool SameDynamicTags = false;
	bool SameEffectHandle = false;
	bool SameSetByCaller = false;
	bool SameBaseData = false;

	if (Ar.IsSaving())
	{
		SameLevel = Level == BaseDeltaState->Level;
		SameSource = SourceObject == BaseDeltaState->SourceObject;
		SameDynamicTags = DynamicAbilityTags == BaseDeltaState->DynamicAbilityTags;
		SameEffectHandle = GrantingGameplayEffectHandle == BaseDeltaState->GrantingGameplayEffectHandle;
		SameSetByCaller = SetByCallerTagMagnitudes.Num() == BaseDeltaState->SetByCallerTagMagnitudes.Num();
		if (SameSetByCaller)
		{
			for (const TTuple<FGameplayTag, float>& Pair : SetByCallerTagMagnitudes)
			{
				const float* Found = BaseDeltaState->SetByCallerTagMagnitudes.Find(Pair.Key);
				if (!Found)
				{
					SameSetByCaller = false;
					break;
				}
				if (*Found != Pair.Value)
				{
					SameSetByCaller = false;
					break;
				}
			}
		}
		SameBaseData = SameLevel && SameSource && SameDynamicTags && SameEffectHandle && SameSetByCaller;
	}
	
	Ar.SerializeBits(&SameBaseData,1);
	if (SameBaseData)
	{
		Level = BaseDeltaState->Level;
		SourceObject = BaseDeltaState->SourceObject;
		DynamicAbilityTags = BaseDeltaState->DynamicAbilityTags;
		GrantingGameplayEffectHandle = BaseDeltaState->GrantingGameplayEffectHandle;
		SetByCallerTagMagnitudes = BaseDeltaState->SetByCallerTagMagnitudes;

		// For Now Serialize full active instance (when inactive just sends 1 bit)
		// don't need to replicate Active Instance or active count for 
		UGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
		if (AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
			|| AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
		{
			ActiveInstances.Empty();
			return true;
		}
		bool SameActiveCount = Ar.IsSaving() ? ActiveCount == BaseDeltaState->ActiveCount : false;
		Ar.SerializeBits(&SameActiveCount,1);
		if (SameActiveCount)
		{
			ActiveCount = BaseDeltaState->ActiveCount;
		}
		else
		{
			Ar << ActiveCount;
		}
		// Instances
		ActiveInstances.NetSerialize(Params, AbilityClass.GetDefaultObject());
		return true;
	}
	
	// Serialize RemoveAfterActivation as 1 bit anyways
	Ar.SerializeBits(&RemoveAfterActivation,1);
	// Serialize Level if not the same
	Ar.SerializeBits(&SameLevel,1);
	if (SameLevel)
	{
		Level = BaseDeltaState->Level;
	}
	else
	{
		uint32 LevelUnsigned = Level;
		Ar.SerializeIntPacked(LevelUnsigned);
		Level = LevelUnsigned;
	}
	// Serialize Source Object if not the same
	Ar.SerializeBits(&SameSource,1);
	if (SameSource)
	{
		SourceObject = BaseDeltaState->SourceObject;
	}
	else
	{
		bool ValidSourceObject = Ar.IsSaving() ? SourceObject != nullptr : false;
		Ar.SerializeBits(&ValidSourceObject,1);
		if (ValidSourceObject)
		{
			Ar << SourceObject;
		}
		else
		{
			SourceObject = nullptr;
		}
	}
	// Serialize Dynamic Tags As Delta If Not The same
	Ar.SerializeBits(&SameDynamicTags,1);
	if (SameDynamicTags)
	{
		DynamicAbilityTags = BaseDeltaState->DynamicAbilityTags;
	}
	else
	{
		FGameplayTagContainerDelta Delta = Ar.IsSaving() ? FGameplayTagContainerDelta(DynamicAbilityTags,BaseDeltaState->DynamicAbilityTags) : FGameplayTagContainerDelta();
		Delta.NetSerialize(Params);
		if (Ar.IsLoading())
		{
			DynamicAbilityTags = FGameplayTagContainerDelta::ReconstructContainerFromBaseAndDelta(BaseDeltaState->DynamicAbilityTags,Delta);
		}
	}
	// Serialize Effect handle if not the same
	Ar.SerializeBits(&SameEffectHandle,1);
	if (SameEffectHandle)
	{
		GrantingGameplayEffectHandle = BaseDeltaState->GrantingGameplayEffectHandle;
	}
	else
	{
		bool ValidGrantingEffect = Ar.IsSaving() ? GrantingGameplayEffectHandle != INDEX_NONE : false;
		Ar.SerializeBits(&ValidGrantingEffect,1);
		if (ValidGrantingEffect)
		{
			Ar << GrantingGameplayEffectHandle;
		}
		else
		{
			GrantingGameplayEffectHandle = INDEX_NONE;
		}
	}
	// Serialize SetByCaller if not the same //ToDo : Serialize As Delta ???
	Ar.SerializeBits(&SameSetByCaller,1);
	if (SameSetByCaller)
	{
		SetByCallerTagMagnitudes = BaseDeltaState->SetByCallerTagMagnitudes;
	}
	else
	{
		bool HasSetByCaller = Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() > 0 : false;
		Ar.SerializeBits(&HasSetByCaller,1);
		if (HasSetByCaller)
		{
			bool IgnoredOutSuccess;
			uint32 NumElements =  Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() : 0;
			Ar.SerializeIntPacked(NumElements);

			if (Ar.IsSaving())
			{
				for (TTuple<FGameplayTag, float>& Pair : SetByCallerTagMagnitudes)
				{
					Pair.Key.NetSerialize(Ar, Params.Map, IgnoredOutSuccess);
					Ar << Pair.Value;
				}
			}
			else if (Ar.IsLoading())
			{
				SetByCallerTagMagnitudes.Empty(NumElements);

				for (uint32 i = 0; i < NumElements; ++i)
				{
					FGameplayTag Tag;
					float Magnitude;
					Tag.NetSerialize(Ar, Params.Map, IgnoredOutSuccess);
					Ar << Magnitude;
					SetByCallerTagMagnitudes.Add(Tag, Magnitude);
				}
			}
		}
	}

	// don't need to replicate Active Instance or active count for  server and local only
	UGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
	if (AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| AbilityCDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		ActiveInstances.Empty();
		return true;
	}
	Ar << ActiveCount;
	// ToDo @Kai : Serialize Instances As Delta 
	ActiveInstances.NetSerialize(Params, AbilityClass.GetDefaultObject());
	return true;
}

void FActivatableAbilitySyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("  Ability Class : %s \n", TCHAR_TO_ANSI(*AbilityClass->GetFName().ToString()));
	Out.Appendf("  Activatable Ability Handle : %d \n", ActivatableAbilityHandle);
	Out.Appendf("  Level : %d \n", Level);
	Out.Appendf("  Source Object : %s \n", SourceObject ? TCHAR_TO_ANSI(*GetNameSafe(SourceObject)) : "Null");
	Out.Appendf("  Granting GameplayEffect Handle : %d \n", GrantingGameplayEffectHandle);
	//ToDo @Kai : Add Set By Caller String.
	ActiveInstances.ToString(Out,AbilityClass.GetDefaultObject());
}

void FActivatableAbilitySyncState::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(SourceObject);
	ActiveInstances.AddStructReferencedObjects(Collector);
}

bool FActivatableAbilitySyncState::ShouldReconcile(const FActivatableAbilitySyncState& AuthorityState) const
{
	bool NotSameClass = AbilityClass != AuthorityState.AbilityClass;
	//ToDo @Kai Improve this buy adding the class name to all these reconcile strings
	UE_NP_TRACE_RECONCILE(ActivatableAbilityHandle != AuthorityState.ActivatableAbilityHandle,"Different Ability Handle")
	UE_NP_TRACE_RECONCILE(Level != AuthorityState.Level,"Different Ability Level")
	UE_NP_TRACE_RECONCILE(SourceObject != AuthorityState.SourceObject,"Different Ability Source Object")
	UE_NP_TRACE_RECONCILE(DynamicAbilityTags != AuthorityState.DynamicAbilityTags,"Different Ability Dynamic Tags")
	UE_NP_TRACE_RECONCILE(GrantingGameplayEffectHandle != AuthorityState.GrantingGameplayEffectHandle,"Different Ability Granting Effect handle")
	UE_NP_TRACE_RECONCILE(RemoveAfterActivation != AuthorityState.RemoveAfterActivation,"Different Remove After Activation")
	//ToDo @Kai : Set By Caller
	//UE_NP_TRACE_RECONCILE(SetByCallerTagMagnitudes != AuthorityState.SetByCallerTagMagnitudes,"Different Ability Granting Effect handle")

	// don't check for active instances or active count if ability activation should not be synced
	UNpGameplayAbility* CDO = AbilityClass.GetDefaultObject();
	if (CDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| CDO->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		return false;
	}
	UE_NP_TRACE_RECONCILE(ActiveCount != AuthorityState.ActiveCount,"Different Active Count")
	UE_NP_TRACE_RECONCILE(ActiveInstances.ShouldReconcile(AuthorityState.ActiveInstances),"Different Active Instances")
	return NotSameClass;
}

void FActivatableAbilitySyncState::Interpolate(const FActivatableAbilitySyncState& From,
	const FActivatableAbilitySyncState& To, float Pct)
{
	check(From.AbilityClass == To.AbilityClass);
	AbilityClass = To.AbilityClass;
	ActiveInstances.Interpolate(From.ActiveInstances, To.ActiveInstances, Pct);
}

FActivatableAbilitySyncState& FActivatableAbilitySyncState::operator=(const FActivatableAbilitySyncState& Other)
{
	if (this != &Other)
	{
		ActivatableAbilityHandle = Other.ActivatableAbilityHandle;
		Level = Other.Level;
		SourceObject = Other.SourceObject;
		DynamicAbilityTags = Other.DynamicAbilityTags;
		GrantingGameplayEffectHandle = Other.GrantingGameplayEffectHandle;
		SetByCallerTagMagnitudes = Other.SetByCallerTagMagnitudes;
		RemoveAfterActivation = Other.RemoveAfterActivation;
		ActiveCount = Other.ActiveCount;
		AbilityClass = Other.AbilityClass.Get();
		ActiveInstances = Other.ActiveInstances;
	}
	return *this;
}
#pragma endregion

#pragma region Activatable Abilities Collection

FActivatableAbilitiesCollection::FActivatableAbilitiesCollection()
{
}

bool FActivatableAbilitiesCollection::NetSerialize(const FNetSerializeParams& Params)
{
	NetSerializeDataArray(Params,ActivatableAbilities);
	return true;
}

bool FActivatableAbilitiesCollection::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FActivatableAbilitiesCollection* BaseDeltaState = Params.GetBaseDeltaState<FActivatableAbilitiesCollection>();
	NetDeltaSerializeDataArray(Params,BaseDeltaState->ActivatableAbilities,ActivatableAbilities);
	return true;
}

FActivatableAbilitiesCollection& FActivatableAbilitiesCollection::operator=(
	const FActivatableAbilitiesCollection& Other)
{
	ActivatableAbilities.SetNum(Other.ActivatableAbilities.Num());
	for (int32 i = 0 ; i < Other.ActivatableAbilities.Num() ; ++i)
	{
		ActivatableAbilities[i] = Other.ActivatableAbilities[i];
	}
	return *this;
}

bool FActivatableAbilitiesCollection::operator==(const FActivatableAbilitiesCollection& Other) const
{
	return !ShouldReconcile(Other);
}

bool FActivatableAbilitiesCollection::operator!=(const FActivatableAbilitiesCollection& Other) const
{
	return !(*this == Other);
}

bool FActivatableAbilitiesCollection::ShouldReconcile(const FActivatableAbilitiesCollection& Other) const
{
	UE_NP_TRACE_RECONCILE(ActivatableAbilities.Num() != Other.ActivatableAbilities.Num(),"Different number Of Activatable Abilities")
	for (int32 i = 0 ; i < ActivatableAbilities.Num() ; ++i)
	{
		if (ActivatableAbilities[i].ShouldReconcile(Other.ActivatableAbilities[i]))
		{
			return true;
		}
	}
	return false;
}

void FActivatableAbilitiesCollection::Interpolate(const FActivatableAbilitiesCollection& From,
	const FActivatableAbilitiesCollection& To, float Pct)
{
	//Activatable Abilities Should Properly Interpolate inner values??
	ActivatableAbilities = To.ActivatableAbilities;
}

void FActivatableAbilitiesCollection::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	for (int32 i = 0 ; i < ActivatableAbilities.Num() ; ++i)
	{
		ActivatableAbilities[i].AddReferencedObjects(Collector);
	}
}

void FActivatableAbilitiesCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("\nActivatable Abilities : \n{");
	for (int32 i = 0 ; i < ActivatableAbilities.Num() ; ++i)
	{
		Out.Append("\n");
		ActivatableAbilities[i].ToString(Out);
	}
	Out.Append("\n}");
}

TArray<FActivatableAbilitySyncState>::TConstIterator FActivatableAbilitiesCollection::GetCollectionDataIterator() const
{
	return ActivatableAbilities.CreateConstIterator();
}

void FActivatableAbilitiesCollection::FillFromActivatableAbilities(FGameplayAbilitySpecContainer& ActivatableAbilitiesSpecs)
{
	UAbilitySimulationLibrary::FillAbilitiesCollectionDataFromSpecContainer(*this, ActivatableAbilitiesSpecs);
}

void FActivatableAbilitiesCollection::NetSerializeDataArray(const FNetSerializeParams& Params,TArray<FActivatableAbilitySyncState>& DataArray)
{
	FArchive& Ar = Params.Ar;
	uint32 NumActivatableAbilities = Ar.IsSaving() ? DataArray.Num() : 0;
	Ar.SerializeIntPacked(NumActivatableAbilities);

	if(Ar.IsSaving())
	{
		for (uint32 i = 0 ; i < NumActivatableAbilities ; ++i)
		{
			FNetSerializeParams SerParams = FNetSerializeParams(Params.Ar,Params.Map,Params.ReplicationTarget);
			Params.Ar << DataArray[i].ActivatableAbilityHandle;
			DataArray[i].NetSerialize(SerParams);
		}
	}
	else if(Ar.IsLoading())
	{
		DataArray.Empty(NumActivatableAbilities);

		for (uint32 i = 0 ; i < NumActivatableAbilities ; ++i)
		{
			FNetSerializeParams SerParams = FNetSerializeParams(Params.Ar,Params.Map,Params.ReplicationTarget);
			FActivatableAbilitySyncState AbilityData;
			Params.Ar << AbilityData.ActivatableAbilityHandle;
			AbilityData.NetSerialize(SerParams);
			DataArray.Add(AbilityData);
		}
	}
	
}

void FActivatableAbilitiesCollection::NetDeltaSerializeDataArray(const FNetSerializeParams& Params,const TArray<FActivatableAbilitySyncState>& BaseDeltaDataArray,
	TArray<FActivatableAbilitySyncState>& DataArray)
{
	FArchive& Ar = Params.Ar;
	uint32 NumActivatableAbilities = Ar.IsSaving() ? DataArray.Num() : 0;
	Ar.SerializeIntPacked(NumActivatableAbilities);
	// If Saving into archive create the delta state and serialize it
	if (Params.Ar.IsSaving())
	{
		for (uint32 i = 0 ; i < NumActivatableAbilities ; ++i)
		{
			
			FNetSerializeParams DeltaParams = Params;
			DeltaParams.BaseDeltaStatePtr = nullptr;
			// can probably even use int8 here, 100+ abilities given at once seems like a reasonable assumption.
			int16 DeltaIndex = INDEX_NONE;
			for (int32 j = 0 ; j < BaseDeltaDataArray.Num() ; ++j)
			{
				if (BaseDeltaDataArray[j].ActivatableAbilityHandle == DataArray[i].ActivatableAbilityHandle
					&& BaseDeltaDataArray[j].AbilityClass == DataArray[i].AbilityClass)
				{
					DeltaParams.BaseDeltaStatePtr = &BaseDeltaDataArray[j];
					DeltaIndex = j;
				}
			}
			DeltaParams.Ar << DeltaIndex;
			if (DeltaIndex == INDEX_NONE)
			{
				DataArray[i].NetSerialize(DeltaParams);
			}
			else
			{
				DataArray[i].NetDeltaSerialize(DeltaParams);
			}
		}
	}
	else // is loading
	{
		DataArray.Empty(NumActivatableAbilities);

		for (uint32 i = 0 ; i < NumActivatableAbilities ; ++i)
		{
			FActivatableAbilitySyncState AbilityData;
			FNetSerializeParams DeltaParams = Params;
			DeltaParams.BaseDeltaStatePtr = nullptr;
			int16 DeltaIndex = INDEX_NONE;
			DeltaParams.Ar << DeltaIndex;
			if (DeltaIndex == INDEX_NONE)
			{
				AbilityData.NetSerialize(DeltaParams);
			}
			else
			{
				DeltaParams.BaseDeltaStatePtr = &BaseDeltaDataArray[DeltaIndex];
				AbilityData.AbilityClass = BaseDeltaDataArray[DeltaIndex].AbilityClass;
				AbilityData.ActivatableAbilityHandle = BaseDeltaDataArray[DeltaIndex].ActivatableAbilityHandle;
				AbilityData.NetDeltaSerialize(DeltaParams);
			}
			DataArray.Add(AbilityData);
		}
	}
}
#pragma endregion
