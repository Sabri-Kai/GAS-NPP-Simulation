// 2025 Yohoho Productions /  Sirkai


#include "Tasks/GASTasks/CommonDataTypes.h"


#pragma region External Target Task Data

bool FExternalTargetTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	bool HasExternalTarget = Ar.IsSaving() ? ExternalTarget != nullptr : false;
	Ar.SerializeBits(&HasExternalTarget,1);
	if (HasExternalTarget)
	{
		Ar << ExternalTarget;
	}
	else
	{
		ExternalTarget = nullptr;
	}
	return true;
}

bool FExternalTargetTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FExternalTargetTaskData* BaseDelta = Params.GetBaseDeltaState<FExternalTargetTaskData>();
	check(BaseDelta)
	FArchive& Ar = Params.Ar;
	bool HasExternalTarget = Ar.IsSaving() ? ExternalTarget != nullptr : false;
	Ar.SerializeBits(&HasExternalTarget,1);
	if (HasExternalTarget)
	{
		bool SameAsDelta = Ar.IsSaving() ? ExternalTarget == BaseDelta->ExternalTarget : false;
		Ar.SerializeBits(&SameAsDelta,1);
		if (SameAsDelta)
		{
			ExternalTarget = BaseDelta->ExternalTarget;
			return true;
		}
		Ar << ExternalTarget;
		return true;
	}
	ExternalTarget = nullptr;
	return true;
}

UScriptStruct* FExternalTargetTaskData::GetScriptStruct() const
{
	return FExternalTargetTaskData::StaticStruct();
}

void FExternalTargetTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	if (ExternalTarget)
	{
		Out.Appendf("Target : %s" , TCHAR_TO_ANSI(*ExternalTarget.GetName()));
	}
}

bool FExternalTargetTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FExternalTargetTaskData* AuthState = static_cast<const FExternalTargetTaskData*>(&AuthorityState);
	const bool DiffExternalTarget = ExternalTarget != AuthState->ExternalTarget;
	return DiffExternalTarget;
}

void FExternalTargetTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FExternalTargetTaskData* ToState = static_cast<const FExternalTargetTaskData*>(&To);
	ExternalTarget = ToState->ExternalTarget;
}

#pragma endregion


#pragma region Wait Fixed Duration Data

bool FWaitFixedDurationTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	Params.Ar.SerializeIntPacked(StartTimeMS);
	return true;
}

bool FWaitFixedDurationTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FWaitFixedDurationTaskData* BaseDelta = Params.GetBaseDeltaState<FWaitFixedDurationTaskData>();
	check(BaseDelta);
	bool bSameAsBase = Params.Ar.IsSaving() ? StartTimeMS == BaseDelta->StartTimeMS : false;
	Params.Ar.SerializeBits(&bSameAsBase, 1);
	if (bSameAsBase)
	{
		StartTimeMS = BaseDelta->StartTimeMS;
	}
	else
	{
		Params.Ar.SerializeIntPacked(StartTimeMS);
	}
	return true;
}

UScriptStruct* FWaitFixedDurationTaskData::GetScriptStruct() const
{
	return FWaitFixedDurationTaskData::StaticStruct();
}

void FWaitFixedDurationTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("      Start Time MS : %f\n",StartTimeMS / 1000.f);
}

bool FWaitFixedDurationTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FWaitFixedDurationTaskData* AuthState = static_cast<const FWaitFixedDurationTaskData*>(&AuthorityState);
	return StartTimeMS != AuthState->StartTimeMS;
}

void FWaitFixedDurationTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FWaitFixedDurationTaskData* ToState = static_cast<const FWaitFixedDurationTaskData*>(&To);
	StartTimeMS = ToState->StartTimeMS;
}

#pragma endregion

#pragma region Wait Dynamic Duration Data

bool FWaitDynamicDurationTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	Super::NetSerialize(Params);
	Params.Ar.SerializeIntPacked(TotalDurationMS);
	return true;
}
bool FWaitDynamicDurationTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	Super::NetDeltaSerialize(Params);
	const FWaitDynamicDurationTaskData* BaseDelta = Params.GetBaseDeltaState<FWaitDynamicDurationTaskData>();
	check(BaseDelta);
	bool bSameTotal = Params.Ar.IsSaving() ? TotalDurationMS == BaseDelta->TotalDurationMS : false;
	Params.Ar.SerializeBits(&bSameTotal, 1);
	if (bSameTotal)
	{
		TotalDurationMS = BaseDelta->TotalDurationMS;
	}
	else
	{
		Params.Ar.SerializeIntPacked(TotalDurationMS);
	}
	return true;
}

UScriptStruct* FWaitDynamicDurationTaskData::GetScriptStruct() const
{
	return FWaitDynamicDurationTaskData::StaticStruct();
}

void FWaitDynamicDurationTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Super::ToString(Out);
	Out.Appendf("      Total Wait Duration : %f\n",TotalDurationMS / 1000.f);
	
}

bool FWaitDynamicDurationTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FWaitDynamicDurationTaskData* AuthState = static_cast<const FWaitDynamicDurationTaskData*>(&AuthorityState);
	
	const bool DiffParent = Super::ShouldReconcile(AuthorityState);
	const bool DiffTotalDuration = TotalDurationMS != AuthState->TotalDurationMS;
	
	return DiffParent || DiffTotalDuration;
}

void FWaitDynamicDurationTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	Super::Interpolate(From, To, Pct);
	const FWaitDynamicDurationTaskData* FromState = static_cast<const FWaitDynamicDurationTaskData*>(&From);
	const FWaitDynamicDurationTaskData* ToState = static_cast<const FWaitDynamicDurationTaskData*>(&To);
	TotalDurationMS = FMath::Lerp(FromState->TotalDurationMS, ToState->TotalDurationMS, Pct);
}

#pragma endregion