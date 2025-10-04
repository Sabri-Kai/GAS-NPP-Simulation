// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTypes/EffectsDataTypes.h"

#include "GameplayEffect.h"
#include "NetworkPredictionReplicationProxy.h"

#pragma region Synced data for captured attributes
bool FCapturedAttributesSyncData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// Source
	bOutSuccess = true;
	uint8 SourceValuesNum = Ar.IsSaving() ? CapturedSourceAttributeValues.Num() : 0;
	Ar << SourceValuesNum;
	if (Ar.IsLoading())
	{
		CapturedSourceAttributeValues.SetNum(SourceValuesNum);
	}
	for (uint8 i = 0; i < SourceValuesNum; i++)
	{
		Ar << CapturedSourceAttributeValues[i];
	}
	// Target
	uint8 TargetValuesNum = Ar.IsSaving() ? CapturedTargetAttributeValues.Num() : 0;
	Ar << TargetValuesNum;
	if (Ar.IsLoading())
	{
		CapturedTargetAttributeValues.SetNum(TargetValuesNum);
	}
	for (uint8 i = 0; i < TargetValuesNum; i++)
	{
		Ar << CapturedTargetAttributeValues[i];
	}
	return true;
}
bool FCapturedAttributesSyncData::ShouldReconcile(const FCapturedAttributesSyncData& AuthorityState) const
{
	if (CapturedSourceAttributeValues.Num() != AuthorityState.CapturedSourceAttributeValues.Num())
	{
		return true;
	}
	if (CapturedTargetAttributeValues.Num() != AuthorityState.CapturedTargetAttributeValues.Num())
	{
		return true;
	}
	if (CapturedSourceAttributeValues != AuthorityState.CapturedSourceAttributeValues)
	{
		return true;
	}
	if (CapturedTargetAttributeValues != AuthorityState.CapturedTargetAttributeValues)
	{
		return true;
	}
	return false;
}
#pragma endregion 

#pragma region FEffectSpecData , Data To Reconstruct FGameplayEffectSpec
FEffectSpecSyncData::FEffectSpecSyncData()
{
	Def = nullptr;
	Level = 0;
	DurationMS = 0;
	PeriodMS = 0;
	StackCount = 0;
	bDurationLocked = false;
}
FEffectSpecSyncData::FEffectSpecSyncData(const FGameplayEffectSpec& Spec)
{
	Def = Spec.Def;
	Level = Spec.GetLevel() + 1; // level can be -1 ,but we want to use uint32 with serialize int packed , we do -1 when we restore
	DurationMS = FMath::Floor((Spec.GetDuration() + 1) * 1000.f); // Duration can be -1 ,but we want to use uint32 with serialize int packed
	PeriodMS = FMath::Floor((Spec.GetPeriod() + 1) * 1000.f); // Period can be -1 ,but we want to use uint32 with serialize int packed
	StackCount = Spec.GetStackCount();
	bDurationLocked = Spec.bDurationLocked;
	Spec.CapturedRelevantAttributes.GetCapturedAttributesValues(CapturedRelevantAttributes.CapturedSourceAttributeValues,CapturedRelevantAttributes.CapturedTargetAttributeValues);
	CapturedSourceTags = Spec.CapturedSourceTags;
	DynamicGrantedTags = Spec.DynamicGrantedTags;
	ModifiedAttributesValues.SetNum(Spec.ModifiedAttributes.Num());
	for (int32 i = 0; i < Spec.ModifiedAttributes.Num(); ++i)
	{
		ModifiedAttributesValues[i] = Spec.ModifiedAttributes[i].TotalMagnitude;
	}
	SetByCallerTagMagnitudes = Spec.SetByCallerTagMagnitudes;
	EffectContext = Spec.GetEffectContext().Duplicate();
}
bool FEffectSpecSyncData::NetSerialize(const FNetSerializeParams& P)
{
	bool bOutSuccess = true;
	P.Ar << Def;
	bool LevelIsOne = P.Ar.IsSaving() ? Level == 1 : false;
	P.Ar.SerializeBits(&LevelIsOne,1);
	if (LevelIsOne)
	{
		Level = 1;
	}
	else
	{
		P.Ar.SerializeIntPacked(Level);
	}
	
	P.Ar.SerializeIntPacked(DurationMS);
	P.Ar.SerializeIntPacked(PeriodMS);
	P.Ar.SerializeIntPacked(StackCount);
	P.Ar.SerializeBits(&bDurationLocked,1);
	CapturedRelevantAttributes.NetSerialize(P.Ar, P.Map, bOutSuccess);
	CapturedSourceTags.GetActorTags().NetSerialize(P.Ar,P.Map,bOutSuccess);
	CapturedSourceTags.GetSpecTags().NetSerialize(P.Ar,P.Map,bOutSuccess);
	DynamicGrantedTags.NetSerialize(P.Ar,P.Map,bOutSuccess);
	// Modified Attributes
	uint8 ModifiedAttributesNum = P.Ar.IsSaving() ? ModifiedAttributesValues.Num() : 0;
	P.Ar << ModifiedAttributesNum;
	if (P.Ar.IsLoading())
	{
		ModifiedAttributesValues.SetNum(ModifiedAttributesNum);
	}
	for (uint8 i = 0; i < ModifiedAttributesNum; ++i)
	{
		P.Ar << ModifiedAttributesValues[i];
	}
	// Set By Caller
	uint8 SetByCallerNum = P.Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() : 0;
	P.Ar << SetByCallerNum;
	if (P.Ar.IsSaving())
	{
		for (auto& Element : SetByCallerTagMagnitudes)
		{
			Element.Key.NetSerialize(P.Ar, P.Map, bOutSuccess);
			//Ar << Element.Key;
			P.Ar << Element.Value;
		}
	}
	else if (P.Ar.IsLoading())
	{
		SetByCallerTagMagnitudes.Reset();
		SetByCallerTagMagnitudes.Reserve(SetByCallerNum);
		for (uint8 i = 0; i < SetByCallerNum; ++i)
		{
			FGameplayTag Tag;
			float Value;
			Tag.NetSerialize(P.Ar, P.Map, bOutSuccess);
			//Ar << Tag;
			P.Ar << Value;
			SetByCallerTagMagnitudes.Add(Tag, Value);
		}
	}

	EffectContext.NetSerialize(P.Ar,P.Map,bOutSuccess);
	return true;
}
bool FEffectSpecSyncData::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FEffectSpecSyncData* BaseDeltaState =  P.GetBaseDeltaState<FEffectSpecSyncData>();
	bool bOutSuccess = true;
	Def = BaseDeltaState->Def;
	//Data That usually doesn't change while effect is active
	bool SameLevel = false;
	bool SameDuration = false;
	bool SamePeriod = false;
	bool SameCapturedRelevantAttributes = false;
	bool SameCapturedSourceTags = false;
	bool SameDynamicGrantedTags = false;
	bool SameSetByCallerTagMagnitudes = false;
	bool SameEffectContext = false;
	bool SameBaseData = false;
	// Data that can change frequently when effect is active , still serialize a single bit if it doesn't change
	bool SameModifiedAttributesValues = false;
	bool SameStackCount = false;
	if (P.Ar.IsSaving())
	{
		SameLevel = Level == BaseDeltaState->Level;
		SameDuration = DurationMS == BaseDeltaState->DurationMS;
		SamePeriod = PeriodMS == BaseDeltaState->PeriodMS;
		SameCapturedRelevantAttributes = !CapturedRelevantAttributes.ShouldReconcile(BaseDeltaState->CapturedRelevantAttributes);
		SameCapturedSourceTags = CapturedSourceTags.GetActorTags() == BaseDeltaState->CapturedSourceTags.GetActorTags()
		                       && CapturedSourceTags.GetSpecTags() == BaseDeltaState->CapturedSourceTags.GetSpecTags();
		SameDynamicGrantedTags = DynamicGrantedTags == BaseDeltaState->DynamicGrantedTags;
		SameSetByCallerTagMagnitudes = SetByCallerTagMagnitudes.Num() == BaseDeltaState->SetByCallerTagMagnitudes.Num();
		if (SameSetByCallerTagMagnitudes)
		{
			for (const auto& SetByCaller : SetByCallerTagMagnitudes)
			{
				const float* FoundSetByCallerValue = BaseDeltaState->SetByCallerTagMagnitudes.Find(SetByCaller.Key);
				if (FoundSetByCallerValue == nullptr)
				{
					SameSetByCallerTagMagnitudes = false;
					break;
				}
				if (SetByCaller.Value != *FoundSetByCallerValue)
				{
					SameSetByCallerTagMagnitudes = false;
					break;
				}
			}
		}
		SameEffectContext = EffectContext == BaseDeltaState->EffectContext;
		SameBaseData = SameLevel && SameDuration && SamePeriod && SameCapturedRelevantAttributes && SameCapturedSourceTags && SameDynamicGrantedTags
		&& SameSetByCallerTagMagnitudes && SameEffectContext;

		SameModifiedAttributesValues = ModifiedAttributesValues == BaseDeltaState->ModifiedAttributesValues;
		SameStackCount = StackCount == BaseDeltaState->StackCount;
	}
	P.Ar.SerializeBits(&SameBaseData,1);
	if (SameBaseData)
	{
		Level = BaseDeltaState->Level;
		DurationMS = BaseDeltaState->DurationMS;
		PeriodMS = BaseDeltaState->PeriodMS;
		CapturedRelevantAttributes = BaseDeltaState->CapturedRelevantAttributes;
		DynamicGrantedTags = BaseDeltaState->DynamicGrantedTags;
		SetByCallerTagMagnitudes = BaseDeltaState->SetByCallerTagMagnitudes;
		DynamicGrantedTags = BaseDeltaState->DynamicGrantedTags;
		EffectContext = BaseDeltaState->EffectContext;
		P.Ar.SerializeBits(&bDurationLocked,1);
		P.Ar.SerializeBits(&SameModifiedAttributesValues,1);
		if (SameModifiedAttributesValues)
		{
			ModifiedAttributesValues = BaseDeltaState->ModifiedAttributesValues;
		}
		else
		{
			// ToDo @Kai : this num is probably same as modifiers num in the definition
			uint8 ModifiedAttributesNum = P.Ar.IsSaving() ? ModifiedAttributesValues.Num() : 0;
			P.Ar << ModifiedAttributesNum;
			if (P.Ar.IsLoading())
			{
				ModifiedAttributesValues.SetNum(ModifiedAttributesNum);
			}
			for (uint8 i = 0; i < ModifiedAttributesNum; ++i)
			{
				P.Ar << ModifiedAttributesValues[i];
			}
		}
		P.Ar.SerializeBits(&SameStackCount,1);
		if (SameStackCount)
		{
			StackCount = BaseDeltaState->StackCount;
		}
		else
		{
			P.Ar.SerializeIntPacked(StackCount);
		}
	}
	else
	{
		P.Ar.SerializeBits(&bDurationLocked,1);
		// level
		P.Ar.SerializeBits(&SameLevel,1);
		if (SameLevel)
		{
			Level = BaseDeltaState->Level;
		}
		else
		{
			P.Ar.SerializeIntPacked(Level);
		}
		// Duration
		P.Ar.SerializeBits(&SameDuration,1);
		if (SameDuration)
		{
			DurationMS = BaseDeltaState->DurationMS;
		}
		else
		{
			P.Ar.SerializeIntPacked(DurationMS);
		}
		// Period
		P.Ar.SerializeBits(&SamePeriod,1);
		if (SamePeriod)
		{
			PeriodMS = BaseDeltaState->PeriodMS;
		}
		else
		{
			P.Ar.SerializeIntPacked(PeriodMS);
		}
		//Stack Count
		P.Ar.SerializeBits(&SameStackCount,1);
		if (SameStackCount)
		{
			StackCount = BaseDeltaState->StackCount;
		}
		else
		{
			P.Ar.SerializeIntPacked(StackCount);
		}
		//Captured Attributes
		P.Ar.SerializeBits(&SameCapturedRelevantAttributes,1);
		if (SameCapturedRelevantAttributes)
		{
			CapturedRelevantAttributes = BaseDeltaState->CapturedRelevantAttributes;
		}
		else
		{
			CapturedRelevantAttributes.NetSerialize(P.Ar, P.Map, bOutSuccess);
		}
		// Captured Source tags
		P.Ar.SerializeBits(&SameCapturedSourceTags,1);
		if (SameCapturedSourceTags)
		{
			CapturedSourceTags = BaseDeltaState->CapturedSourceTags;
		}
		else
		{
			CapturedSourceTags.GetActorTags().NetSerialize(P.Ar,P.Map,bOutSuccess);
			CapturedSourceTags.GetSpecTags().NetSerialize(P.Ar,P.Map,bOutSuccess);
		}
		// Dynamic Granted Tags
		P.Ar.SerializeBits(&SameDynamicGrantedTags,1);
		if (SameDynamicGrantedTags)
		{
			DynamicGrantedTags = BaseDeltaState->DynamicGrantedTags;
		}
		else
		{
			//ToDo @Kai : Add Delta Serialization to this
			DynamicGrantedTags.NetSerialize(P.Ar,P.Map,bOutSuccess);
		}
		// Modified Attributes
		P.Ar.SerializeBits(&SameModifiedAttributesValues,1);
		if (SameModifiedAttributesValues)
		{
			ModifiedAttributesValues = BaseDeltaState->ModifiedAttributesValues;
		}
		else
		{
			// ToDo @Kai : this num is probably same as modifiers num in the definition
			uint8 ModifiedAttributesNum = P.Ar.IsSaving() ? ModifiedAttributesValues.Num() : 0;
			P.Ar << ModifiedAttributesNum;
			if (P.Ar.IsLoading())
			{
				ModifiedAttributesValues.SetNum(ModifiedAttributesNum);
			}
			for (uint8 i = 0; i < ModifiedAttributesNum; ++i)
			{
				P.Ar << ModifiedAttributesValues[i];
			}
		}
		// Set By Caller 
		P.Ar.SerializeBits(&SameSetByCallerTagMagnitudes,1);
		if (SameSetByCallerTagMagnitudes)
		{
			SetByCallerTagMagnitudes = BaseDeltaState->SetByCallerTagMagnitudes;
		}
		else
		{
			//ToDo @Kai : Add Delta Serialization to this
			uint8 SetByCallerNum = P.Ar.IsSaving() ? SetByCallerTagMagnitudes.Num() : 0;
			P.Ar << SetByCallerNum;
			if (P.Ar.IsSaving())
			{
				for (auto& Element : SetByCallerTagMagnitudes)
				{
					Element.Key.NetSerialize(P.Ar, P.Map, bOutSuccess);
					P.Ar << Element.Value;
				}
			}
			else if (P.Ar.IsLoading())
			{
				SetByCallerTagMagnitudes.Reset();
				SetByCallerTagMagnitudes.Reserve(SetByCallerNum);
				for (uint8 i = 0; i < SetByCallerNum; ++i)
				{
					FGameplayTag Tag;
					float Value;
					Tag.NetSerialize(P.Ar, P.Map, bOutSuccess);
					P.Ar << Value;
					SetByCallerTagMagnitudes.Add(Tag, Value);
				}
			}
		}
		// Effect Context
		// (This Data inside doesn't change after giving it, so we should never have same effect context be false
		// Since this delta is for same effect with same handle)
		P.Ar.SerializeBits(&SameEffectContext,1);
		if (SameEffectContext)
		{
			EffectContext = BaseDeltaState->EffectContext;
		}
		else
		{
			EffectContext.NetSerialize(P.Ar,P.Map,bOutSuccess);
		}
	}
	return true;
}
void FEffectSpecSyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	if (IsValid(Def))
	{
		Out.Appendf("Effect :%s\n",TCHAR_TO_ANSI(*GetNameSafe(Def)));
		Out.Appendf("Level :%f\n",GetLevel());
		Out.Appendf("Duration :%f\n",GetDuration());
		Out.Appendf("Period :%f\n",GetPeriod());
		Out.Appendf("Stack Count :%d\n",StackCount);
		Out.Append("Modified Attributes :\n");
		if (ModifiedAttributesValues.Num() > 0)
		{
			for (int32 i = 0 ; i < Def->Modifiers.Num() ; ++i)
			{
				Out.Appendf("%s :%f \n",*Def->Modifiers[i].Attribute.GetName(),ModifiedAttributesValues[i]);
			}
		}
	}
	else
	{
		Out.Append("Invalid Effect :\n");
	}
	
	
	//ToDo @ Kai : Add Rest OF Debug Data
}
bool FEffectSpecSyncData::ShouldReconcile(const FEffectSpecSyncData& AuthorityState) const
{
	if (Def != AuthorityState.Def)
	{
		return true;
	}
	if (Level != AuthorityState.Level)
	{
		return true;
	}
	if (DurationMS != AuthorityState.DurationMS)
	{
		return true;
	}
	if (PeriodMS != AuthorityState.PeriodMS)
	{
		return true;
	}
	if (StackCount != AuthorityState.StackCount)
	{
		return true;
	}
	if (bDurationLocked != AuthorityState.bDurationLocked)
	{
		return true;
	}
	if (CapturedRelevantAttributes.CapturedSourceAttributeValues != AuthorityState.CapturedRelevantAttributes.CapturedSourceAttributeValues
		|| CapturedRelevantAttributes.CapturedTargetAttributeValues != AuthorityState.CapturedRelevantAttributes.CapturedTargetAttributeValues)
	{
		return true;
	}
	if (CapturedSourceTags.GetActorTags() != AuthorityState.CapturedSourceTags.GetActorTags()
		|| CapturedSourceTags.GetSpecTags() != AuthorityState.CapturedSourceTags.GetSpecTags())
	{
		return true;
	}
	if (DynamicGrantedTags != AuthorityState.DynamicGrantedTags)
	{
		return true;
	}
	if (ModifiedAttributesValues != AuthorityState.ModifiedAttributesValues)
	{
		return true;
	}
	if (SetByCallerTagMagnitudes.Num() != AuthorityState.SetByCallerTagMagnitudes.Num())
	{
		return true;
	}
	if (SetByCallerTagMagnitudes.Array() != AuthorityState.SetByCallerTagMagnitudes.Array())
	{
		return true;
	}
	//ToDo: @Kai , Fix This
	//if (EffectContext != AuthorityState.EffectContext)
	//{
	//	return true;
	//}
	return false;
}
void FEffectSpecSyncData::Interpolate(const FEffectSpecSyncData& From, const FEffectSpecSyncData& To, float Pct)
{
	*this = To;
}
#pragma endregion

#pragma region FActiveEffectData , Data To Reconstruct FActiveGameplayEffect
FActiveEffectSyncData::FActiveEffectSyncData()
{
	EffectHandle = 0;
	StartTimeMS = 0;
	PeriodTimeMS = 0;
	bIsInhibited = false;
}
FActiveEffectSyncData::FActiveEffectSyncData(const FActiveGameplayEffect& ActiveEffect)
{
	EffectHandle = ActiveEffect.Handle.GetHandle();
	StartTimeMS = FMath::Floor(ActiveEffect.StartWorldTime * 1000);
	bIsInhibited = ActiveEffect.bIsInhibited;
	PeriodTimeMS = ActiveEffect.CurrentPeriodTime;
	EffectSpecData = FEffectSpecSyncData(ActiveEffect.Spec);
}
bool FActiveEffectSyncData::NetSerialize(const FNetSerializeParams& P)
{
	P.Ar << EffectHandle;
	EffectSpecData.NetSerialize(P);
	P.Ar.SerializeIntPacked(StartTimeMS);
	P.Ar.SerializeIntPacked(PeriodTimeMS);
	P.Ar.SerializeBits(&bIsInhibited,1);
	return true;
}
bool FActiveEffectSyncData::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FActiveEffectSyncData* BaseStateDelta = P.GetBaseDeltaState<FActiveEffectSyncData>();
	EffectHandle = BaseStateDelta->EffectHandle;
	FNetSerializeParams DeltaParams = P;
	DeltaParams.BaseDeltaStatePtr = &BaseStateDelta->EffectSpecData;
	EffectSpecData.NetDeltaSerialize(DeltaParams);
	StartTimeMS = BaseStateDelta->StartTimeMS;
	P.Ar.SerializeIntPacked(PeriodTimeMS);
	P.Ar.SerializeBits(&bIsInhibited,1);
	return true;
}
void FActiveEffectSyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("\nEffect Handle : %d\n",EffectHandle);
	EffectSpecData.ToString(Out);
	Out.Appendf("Start Simulation Time MS: %d\n",StartTimeMS);
	const float CurrentPeriod = PeriodTimeMS / 1000.f;
	Out.Appendf("Current period Time MS: %f\n",CurrentPeriod);
	Out.Appendf("In Inhibited: %s\n",TCHAR_TO_ANSI(bIsInhibited ? TEXT("True") : TEXT("False")));
}
bool FActiveEffectSyncData::ShouldReconcile(const FActiveEffectSyncData& AuthorityState) const
{
	if (EffectHandle != AuthorityState.EffectHandle)
	{
		return true;
	}
	if (EffectSpecData.ShouldReconcile(AuthorityState.EffectSpecData))
	{
		return true;
	}
	if (StartTimeMS != AuthorityState.StartTimeMS)
	{
		return true;
	}
	if (PeriodTimeMS != AuthorityState.PeriodTimeMS)
	{
		return true;
	}
	if (bIsInhibited != AuthorityState.bIsInhibited)
	{
		return true;
	}
	return false;
}
void FActiveEffectSyncData::Interpolate(const FActiveEffectSyncData& From, const FActiveEffectSyncData& To, float Pct)
{
	EffectHandle = To.EffectHandle;
	EffectSpecData = To.EffectSpecData;
	StartTimeMS = To.StartTimeMS;
	PeriodTimeMS = To.PeriodTimeMS;
	bIsInhibited = From.bIsInhibited;
}
#pragma endregion

#pragma region FActiveEffectSyncDataContainer , Data To Reconstruct FActiveGameplayEffectsContainer
FActiveEffectSyncDataContainer::FActiveEffectSyncDataContainer()
{
}
FActiveEffectSyncDataContainer::FActiveEffectSyncDataContainer(const FActiveGameplayEffectsContainer& EffectsContainer)
{
	ActiveEffectsHandleCount = EffectsContainer.ActiveEffectsHandleCount;
	
	ActiveEffects.SetNum(EffectsContainer.GetNumGameplayEffects());
	
	for (int32 i = 0; i < EffectsContainer.GetNumGameplayEffects(); ++i)
	{
		ActiveEffects[i] = FActiveEffectSyncData(*EffectsContainer.GetActiveGameplayEffect(i));
	}
	ActiveEffects.Sort([](const FActiveEffectSyncData& A, const FActiveEffectSyncData& B) {
	return A.EffectHandle < B.EffectHandle;});
}
bool FActiveEffectSyncDataContainer::NetSerialize(const FNetSerializeParams& P)
{
	bool bOutSuccess = true;
	P.Ar.SerializeIntPacked(ActiveEffectsHandleCount);
	uint32 NumActiveEffects = P.Ar.IsSaving() ? ActiveEffects.Num() : 0;
	P.Ar.SerializeIntPacked(NumActiveEffects);
	if (P.Ar.IsLoading())
	{
		ActiveEffects.SetNum(NumActiveEffects);
	}
	for (uint32 i = 0; i < NumActiveEffects; ++i)
	{
		ActiveEffects[i].NetSerialize(P);
	}
	return true;
}

bool FActiveEffectSyncDataContainer::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FActiveEffectSyncDataContainer* BaseDeltaState = P.GetBaseDeltaState<FActiveEffectSyncDataContainer>();
	bool SameAsBase = P.Ar.IsSaving() ? !ShouldReconcile(*BaseDeltaState) : false;
	P.Ar.SerializeBits(&SameAsBase,1);
	if (SameAsBase)
	{
		ActiveEffects = BaseDeltaState->ActiveEffects;
		ActiveEffectsHandleCount = BaseDeltaState->ActiveEffectsHandleCount;
	}
	else
	{
		// serialize the count if state is not the same
		P.Ar.SerializeIntPacked(ActiveEffectsHandleCount);
		
		if (P.Ar.IsSaving())
		{
			uint32 NumActiveEffects = ActiveEffects.Num();
			P.Ar.SerializeIntPacked(NumActiveEffects); // 1. Effects Num
			FNetSerializeParams DeltaParams = P;
			for (uint32 i = 0; i < NumActiveEffects; ++i)
			{
				FActiveEffectSyncData& ActiveEffect = ActiveEffects[i];
				int8 BaseDeltaStateIndex = INDEX_NONE;
				DeltaParams.BaseDeltaStatePtr = nullptr;
				for (int32 j = 0; j < BaseDeltaState->ActiveEffects.Num(); ++j )
				{
					if (BaseDeltaState->ActiveEffects[j].EffectHandle == ActiveEffect.EffectHandle
						&& BaseDeltaState->ActiveEffects[j].EffectSpecData.Def == ActiveEffect.EffectSpecData.Def)
					{
						BaseDeltaStateIndex = j;
						DeltaParams.BaseDeltaStatePtr = &BaseDeltaState->ActiveEffects[j];
					}
				}
				P.Ar << BaseDeltaStateIndex; // 2. Index from Base Delta State
				if (BaseDeltaStateIndex == INDEX_NONE)
				{
					ActiveEffects[i].NetSerialize(DeltaParams); // 3. Element Net Serialize
				}
				else
				{
					ActiveEffects[i].NetDeltaSerialize(DeltaParams); // 3. Element Net Serialize
				}
			}
		}
		else // is Loading
		{
			uint32 NumActiveEffects = 0;
			P.Ar.SerializeIntPacked(NumActiveEffects); // 1. Effects Num
			ActiveEffects.SetNum(NumActiveEffects);
			FNetSerializeParams DeltaParams = P;
			for (uint32 i = 0; i < NumActiveEffects; ++i)
			{
				DeltaParams.BaseDeltaStatePtr = nullptr;
				int8 BaseDeltaStateIndex = 0;
				P.Ar << BaseDeltaStateIndex; // 2. Index from Base Delta State
				if (BaseDeltaStateIndex == INDEX_NONE)
				{
					ActiveEffects[i].NetSerialize(DeltaParams); // 3. Element Net Serialize
				}
				else
				{
					DeltaParams.BaseDeltaStatePtr = &BaseDeltaState->ActiveEffects[BaseDeltaStateIndex];
					ActiveEffects[i].NetDeltaSerialize(DeltaParams); // 3. Element Net Serialize
				}
			}
		}
	}
	return true;
}

void FActiveEffectSyncDataContainer::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("Active Gameplay Effects :\n");
	Out.Appendf("Active Effects Handle Count : %d\n", ActiveEffectsHandleCount);
	for (int32 i = 0; i < ActiveEffects.Num(); ++i)
	{
		ActiveEffects[i].ToString(Out);
		Out.Append("\n");
	}
}
bool FActiveEffectSyncDataContainer::ShouldReconcile(const FActiveEffectSyncDataContainer& AuthorityState) const
{
	// effects should be in same order and match
	if (ActiveEffectsHandleCount != AuthorityState.ActiveEffectsHandleCount)
	{
		return true;
	}
	if (ActiveEffects.Num() != AuthorityState.ActiveEffects.Num())
	{
		return true;
	}
	for (int32 i = 0; i < ActiveEffects.Num(); ++i)
	{
		if (ActiveEffects[i].ShouldReconcile(AuthorityState.ActiveEffects[i]))
		{
			return true;
		}
	}
	return false;
}
void FActiveEffectSyncDataContainer::Interpolate(const FActiveEffectSyncDataContainer& From,
	const FActiveEffectSyncDataContainer& To, float Pct)
{
	*this = To;
}

const FActiveEffectSyncData* FActiveEffectSyncDataContainer::GetActiveEffectByHandle(const int32& Handle) const
{
	for (int32 i = 0; i < ActiveEffects.Num(); ++i)
	{
		if (ActiveEffects[i].EffectHandle == Handle)
		{
			return &ActiveEffects[i];
		}
	}
	return nullptr;
}

FAttributeSyncData::FAttributeSyncData()
{
	BaseValue = 0.f;
	CurrentValue = 0.f;
}

bool FAttributeSyncData::NetSerialize(const FNetSerializeParams& P)
{

	// we don't serialize the base value to the sim proxies
	if (P.ReplicationTarget == EReplicationProxyTarget::SimulatedProxy)
	{
		P.Ar << CurrentValue;
		return true;
	}
	P.Ar << BaseValue;
	// most of the time base and current are equal, just send 1 bit in that case
	bool CurrentEqualBase = P.Ar.IsSaving() ? FMath::IsNearlyEqual(BaseValue,CurrentValue) : false;
	P.Ar.SerializeBits(&CurrentEqualBase,1);
	if (CurrentEqualBase)
	{
		CurrentValue = BaseValue;
	}
	else
	{
		P.Ar << CurrentValue;
	}
	return true;
}

bool FAttributeSyncData::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FAttributeSyncData* BaseDeltaState = P.GetBaseDeltaState<FAttributeSyncData>();
	bool SameCurrent = false;
	bool SameBase = false;
	bool SameAsBaseDelta =  false;
	if (P.Ar.IsSaving())
	{
		SameCurrent = FMath::IsNearlyEqual(BaseDeltaState->CurrentValue,CurrentValue);
		SameBase = FMath::IsNearlyEqual(BaseDeltaState->BaseValue,BaseValue);
		SameAsBaseDelta =  SameCurrent && SameBase;
	}
	P.Ar.SerializeBits(&SameAsBaseDelta,1); // 1. Same Sa Base Delta
	if (SameAsBaseDelta)
	{
		BaseValue = BaseDeltaState->BaseValue;
		CurrentValue = BaseDeltaState->CurrentValue;
		return true;
	}
	P.Ar.SerializeBits(&SameBase,1); // 2. Same Base
	if (SameBase)
	{
		BaseValue = BaseDeltaState->BaseValue;
	}
	else
	{
		//ToDo @Kai : this be optimized? we can serialize the delta , but what is best way to pack it?
		P.Ar << BaseValue; //3. Base Value
	}
	P.Ar.SerializeBits(&SameCurrent,1); //4. Same Current
	if (!SameCurrent)
	{
		bool CurrentEqualBase = P.Ar.IsSaving() ? FMath::IsNearlyEqual(BaseValue,CurrentValue) : false;
		P.Ar.SerializeBits(&CurrentEqualBase,1);//5. Current Equal Base
		if (CurrentEqualBase)
		{
			CurrentValue = BaseValue;
		}
		else
		{
			//ToDo @Kai : this be optimized? we can serialize the delta , but what is best way to pack it?
			P.Ar << CurrentValue; //6. Current Value
		}
	}
	return true;
}

void FAttributeSyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Base Value: %f.2\n",BaseValue);
	Out.Appendf("Current Value: %f.2\n",CurrentValue);
}

bool FAttributeSyncData::ShouldReconcile(const FAttributeSyncData& AuthorityState) const
{
	if (!FMath::IsNearlyEqual(BaseValue,AuthorityState.BaseValue,0.001))
	{
		return true;
	}
	if (!FMath::IsNearlyEqual(CurrentValue,AuthorityState.CurrentValue,0.001))
	{
		return true;
	}
	return false;
}

void FAttributeSyncData::Interpolate(const FAttributeSyncData& From, const FAttributeSyncData& To, float Pct)
{
	BaseValue = FMath::Lerp(From.BaseValue,To.BaseValue,Pct);
	CurrentValue = FMath::Lerp(From.CurrentValue,To.CurrentValue,Pct);
}

FAttributeSetSyncData::FAttributeSetSyncData()
{
	AttributeSetClass = nullptr;
}

FAttributeSetSyncData::FAttributeSetSyncData(UAttributeSet* AttributeSet)
{
	AttributeSetClass = AttributeSet->GetClass();
	TArray<FGameplayAttribute> Attributes;
	UAttributeSet::GetAttributesFromSetClass(AttributeSetClass, Attributes);
	AttributeValues.SetNum(Attributes.Num());
	for (int32 i = 0; i < Attributes.Num(); ++i)
	{
		FGameplayAttributeData* Data = Attributes[i].GetGameplayAttributeData(AttributeSet);
		if (Data)
		{
			AttributeValues[i].SetBaseValue(Data->GetBaseValue());
			AttributeValues[i].SetCurrentValue(Data->GetCurrentValue());
		}
		else
		{
			// set both base and current to same thing if float type attribute (we only serialize 1 bit for one of them in this case)
			AttributeValues[i].SetCurrentValue(Attributes[i].GetNumericValue(AttributeSet));
			AttributeValues[i].SetBaseValue(Attributes[i].GetNumericValue(AttributeSet));
		}
	}
}

bool FAttributeSetSyncData::NetSerialize(const FNetSerializeParams& P)
{
	P.Ar << AttributeSetClass;

	bool bOutSuccess = true;
	if (AttributeSetClass)
	{
		TArray<FGameplayAttribute> Attributes;
		UAttributeSet::GetAttributesFromSetClass(AttributeSetClass, Attributes);
		if (P.Ar.IsSaving())
		{
			check(AttributeValues.Num() == Attributes.Num());
			for (int32 i = 0; i < AttributeValues.Num(); ++i)
			{
				AttributeValues[i].NetSerialize(P);
			}
		}
		else if (P.Ar.IsLoading())
		{
			AttributeValues.SetNum(Attributes.Num());
			for (int32 i = 0; i < AttributeValues.Num(); ++i)
			{
				AttributeValues[i].NetSerialize(P);
			}
		}
	}
	return true;
}

bool FAttributeSetSyncData::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FAttributeSetSyncData* BaseDeltaState = P.GetBaseDeltaState<FAttributeSetSyncData>();
	AttributeSetClass = BaseDeltaState->AttributeSetClass;
	if (AttributeSetClass)
	{
		TArray<FGameplayAttribute> Attributes;
		UAttributeSet::GetAttributesFromSetClass(AttributeSetClass, Attributes);
		FNetSerializeParams DeltaParams = P;
		bool AttributesValuesAreSame = P.Ar.IsSaving() ? !ShouldReconcile(*BaseDeltaState) : false;
		P.Ar.SerializeBits(&AttributesValuesAreSame,1);
		if (AttributesValuesAreSame)
		{
			AttributeValues = BaseDeltaState->AttributeValues;
			return true;
		}

		if (P.Ar.IsSaving())
		{
			check(AttributeValues.Num() == Attributes.Num());
		}
		else if (P.Ar.IsLoading())
		{
			AttributeValues.SetNum(Attributes.Num());
		}
		
		for (int32 i = 0; i < AttributeValues.Num(); ++i)
		{
			DeltaParams.BaseDeltaStatePtr = &BaseDeltaState->AttributeValues[i];
			AttributeValues[i].NetDeltaSerialize(DeltaParams);
		}
	}
	return true;
	
}

void FAttributeSetSyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	if (AttributeSetClass)
	{
		Out.Appendf("Attribute Set: %s\n",TCHAR_TO_ANSI(*GetNameSafe(AttributeSetClass)));
		TArray<FGameplayAttribute> Attributes;
		UAttributeSet::GetAttributesFromSetClass(AttributeSetClass, Attributes);
		for (int32 i = 0; i < Attributes.Num(); ++i)
		{
			Out.Appendf("%s : Base %f,Current %f\n",TCHAR_TO_ANSI(*Attributes[i].AttributeName),AttributeValues[i].GetBaseValue() , AttributeValues[i].GetCurrentValue());
		}
	}
}

bool FAttributeSetSyncData::ShouldReconcile(const FAttributeSetSyncData& AuthorityState) const
{
	if (AttributeValues.Num() != AuthorityState.AttributeValues.Num())
	{
		return true;
	}
	for (int32 i = 0; i < AttributeValues.Num(); ++i)
	{
		if (AttributeValues[i].ShouldReconcile(AuthorityState.AttributeValues[i]))
		{
			return true;
		}
	}
	return false;
}

void FAttributeSetSyncData::Interpolate(const FAttributeSetSyncData& From, const FAttributeSetSyncData& To, float Pct)
{
	if (From.AttributeSetClass == To.AttributeSetClass)
	{
		AttributeSetClass = To.AttributeSetClass;
		AttributeValues.SetNum(From.AttributeValues.Num());
		for (int32 i = 0; i < AttributeValues.Num(); ++i)
		{
			AttributeValues[i].Interpolate(From.AttributeValues[i],To.AttributeValues[i],Pct);
		}
	}
	else
	{
		AttributeSetClass = To.AttributeSetClass;
		AttributeValues = To.AttributeValues;
	}
}

FAttributeSetSyncDataCollection::FAttributeSetSyncDataCollection(const TArray<UAttributeSet*>& AttributeSets)
{
	AttributeSetsData.Empty(AttributeSets.Num());
	for (int32 i = 0; i < AttributeSets.Num(); ++i)
	{
		AttributeSetsData.Add(FAttributeSetSyncData(AttributeSets[i]));
	}
}

bool FAttributeSetSyncDataCollection::NetSerialize(const FNetSerializeParams& P)
{
	bool bOutSuccess = true;
	uint8 NumAttributeSets = P.Ar.IsSaving() ? AttributeSetsData.Num() : 0;
	P.Ar << NumAttributeSets;
	if (P.Ar.IsLoading())
	{
		AttributeSetsData.SetNum(NumAttributeSets);
	}
	for (uint8 i = 0; i < NumAttributeSets; ++i)
	{
		AttributeSetsData[i].NetSerialize(P);
	}
	return true;
}

bool FAttributeSetSyncDataCollection::NetDeltaSerialize(const FNetSerializeParams& P)
{
	const FAttributeSetSyncDataCollection* BaseStateDelta = P.GetBaseDeltaState<FAttributeSetSyncDataCollection>();
	if (P.Ar.IsSaving())
	{
		bool AttributesChanged = ShouldReconcile(*BaseStateDelta);
		P.Ar.SerializeBits(&AttributesChanged,1);
		if (AttributesChanged)
		{
			uint8 NumAttributeSets = AttributeSetsData.Num();
			P.Ar << NumAttributeSets;
			for (int32 i = 0; i < NumAttributeSets; ++i)
			{
				FAttributeSetSyncData& AttributeSet = AttributeSetsData[i];
				FNetSerializeParams DeltaParams = P;
				DeltaParams.BaseDeltaStatePtr = nullptr;
				int8 IndexInBaseState = INDEX_NONE;
				for (int32 j = 0; j < BaseStateDelta->AttributeSetsData.Num() ; j++)
				{
					if (AttributeSet.AttributeSetClass ==  BaseStateDelta->AttributeSetsData[j].AttributeSetClass)
					{
						DeltaParams.BaseDeltaStatePtr = &BaseStateDelta->AttributeSetsData[j];
						IndexInBaseState = j;
					}
				}
				P.Ar << IndexInBaseState;
				if (IndexInBaseState == INDEX_NONE)
				{
					AttributeSet.NetSerialize(DeltaParams);
				}
				else
				{
					AttributeSet.NetDeltaSerialize(DeltaParams);
				}
			}
		}
	}
	else // Is Loading
	{
		bool AttributesChanged = false;
		P.Ar.SerializeBits(&AttributesChanged,1);
		if (!AttributesChanged)
		{
			AttributeSetsData = BaseStateDelta->AttributeSetsData;
		}
		else
		{
			uint8 NumAttributeSets = 0;
			P.Ar << NumAttributeSets;
			AttributeSetsData.SetNum(NumAttributeSets);
			for (int32 i = 0; i < NumAttributeSets; ++i)
			{
				int8 IndexInBaseState = INDEX_NONE;
				P.Ar << IndexInBaseState;
				FNetSerializeParams DeltaParams = P;
				DeltaParams.BaseDeltaStatePtr = nullptr;
				if (IndexInBaseState == INDEX_NONE)
				{
					AttributeSetsData[i].NetSerialize(DeltaParams);
				}
				else
				{
					DeltaParams.BaseDeltaStatePtr = &BaseStateDelta->AttributeSetsData[IndexInBaseState];
					AttributeSetsData[i].NetDeltaSerialize(DeltaParams);
				}
			}
		}
	}

	return true;
}

void FAttributeSetSyncDataCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	for (const FAttributeSetSyncData& SyncData : AttributeSetsData)
	{
		SyncData.ToString(Out);
		Out.Append("\n");
	}
}

bool FAttributeSetSyncDataCollection::ShouldReconcile(const FAttributeSetSyncDataCollection& AuthorityState) const
{
	if (AttributeSetsData.Num() != AuthorityState.AttributeSetsData.Num())
	{
		return true;
	}
	for (int32 i = 0; i < AttributeSetsData.Num(); ++i)
	{
		if (AttributeSetsData[i].ShouldReconcile(AuthorityState.AttributeSetsData[i]))
		{
			return true;
		}
	}
	return false;
}

void FAttributeSetSyncDataCollection::Interpolate(const FAttributeSetSyncDataCollection& From,
	const FAttributeSetSyncDataCollection& To, float Pct)
{
	// first Set it directly to To value, to be sure any sets added or removed are taken care of.
	AttributeSetsData = To.AttributeSetsData;
	// Can Loop Through "To" Sets and Try To find the class in "From", If found Interpolate it. if not set it as it is
	// Do attributes need to be interpolated? UI can interpolate the value if it wants, what other reason would there be?
}

FSyncedModifiedAttribute::FSyncedModifiedAttribute(const FGameplayEffectModifiedAttribute& ModifiedAttribute)
{
	AttributeSetClass = ModifiedAttribute.Attribute.GetAttributeSetClass();
	if (AttributeSetClass)
	{
		TArray<FGameplayAttribute> Attributes;
		UAttributeSet::GetAttributesFromSetClass(AttributeSetClass,Attributes);
		const int32 FoundIndex = Attributes.Find(ModifiedAttribute.Attribute);
		check(FoundIndex >= 0 && FoundIndex < UINT8_MAX);
		AttributeIndex = (uint8)FoundIndex;
		TotalMagnitude = ModifiedAttribute.TotalMagnitude;
	}
}

void FSyncedModifiedAttribute::MakeModifiedAttribute(const FSyncedModifiedAttribute& SyncedAttribute
                                                     ,FGameplayEffectModifiedAttribute& OutModifiedAttribute)
{
	if (!SyncedAttribute.AttributeSetClass || SyncedAttribute.AttributeIndex < 0)
	{
		return;
	}
	TArray<FGameplayAttribute> Attributes;
	UAttributeSet::GetAttributesFromSetClass(SyncedAttribute.AttributeSetClass,Attributes);
	OutModifiedAttribute.Attribute = Attributes[SyncedAttribute.AttributeIndex];
	OutModifiedAttribute.TotalMagnitude = SyncedAttribute.TotalMagnitude;
}

void FSyncedModifiedAttribute::NetSerialize(FNetSerializeParams& P)
{
	P.Ar << AttributeSetClass;
	P.Ar << AttributeIndex;
	P.Ar << TotalMagnitude;
}

void FSyncedModifiedAttributes::MakeModifiedAttributes(const FSyncedModifiedAttributes& SyncedModifiedAttributes,
	TArray<FGameplayEffectModifiedAttribute>& OutModifiedAttributes)
{
	OutModifiedAttributes.SetNum(SyncedModifiedAttributes.ModifiedAttributes.Num());
	for (int32 i = 0; i < SyncedModifiedAttributes.ModifiedAttributes.Num(); ++i)
	{
		FSyncedModifiedAttribute::MakeModifiedAttribute(SyncedModifiedAttributes.ModifiedAttributes[i],OutModifiedAttributes[i]);
	}
}

void FSyncedModifiedAttributes::NetSerialize(FNetSerializeParams& P)
{
	uint8 Num = ModifiedAttributes.Num();
	P.Ar << Num;
	if (P.Ar.IsLoading())
	{
		ModifiedAttributes.SetNum(Num);
	}
	for (int32 i = 0; i < Num; ++i)
	{
		ModifiedAttributes[i].NetSerialize(P);
	}
}
#pragma endregion
