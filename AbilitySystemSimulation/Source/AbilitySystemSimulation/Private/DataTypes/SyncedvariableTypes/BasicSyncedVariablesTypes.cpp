// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTypes/SyncedvariableTypes/BasicSyncedVariablesTypes.h"

#include "NetworkPredictionReplicationProxy.h"


#pragma region FSyncBool
UScriptStruct* FSyncBool::GetScriptStruct() const
{
	return FSyncBool::StaticStruct();
}

void FSyncBool::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar.SerializeBits(&Value,1);
}

void FSyncBool::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncBool::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncBool* AuthorityVar = static_cast<const FSyncBool*>(&Auth);
	return AuthorityVar->Value != Value;
}

void FSyncBool::SetValue(const FBaseSyncVar* Other)
{
	const FSyncBool* OtherVar = static_cast<const FSyncBool*>(Other);
	Value = OtherVar->Value;
}

void FSyncBool::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", Value ? "True" : "False");
}

void FSyncBool::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncBool::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncBool* ToVar = static_cast<const FSyncBool*>(&To);
	Value = ToVar->Value;
}
#pragma endregion

#pragma region FSyncByte
UScriptStruct* FSyncByte::GetScriptStruct() const
{
	return FSyncByte::StaticStruct();
}

void FSyncByte::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncByte::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncByte::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncByte* AuthorityVar = static_cast<const FSyncByte*>(&Auth);
	return AuthorityVar->Value != Value;
}

void FSyncByte::SetValue(const FBaseSyncVar* Other)
{
	const FSyncByte* OtherVar = static_cast<const FSyncByte*>(Other);
	Value = OtherVar->Value;
}

void FSyncByte::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%d", Value);
}

void FSyncByte::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncByte::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncByte* FromVar = static_cast<const FSyncByte*>(&From);
	const FSyncByte* ToVar = static_cast<const FSyncByte*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncInteger
UScriptStruct* FSyncInteger::GetScriptStruct() const
{
	return FSyncInteger::StaticStruct();
}

void FSyncInteger::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncInteger::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncInteger::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncInteger* AuthorityVar = static_cast<const FSyncInteger*>(&Auth);
	return AuthorityVar->Value != Value;
}


void FSyncInteger::SetValue(const FBaseSyncVar* Other)
{
	const FSyncInteger* OtherVar = static_cast<const FSyncInteger*>(Other);
	Value = OtherVar->Value;
}

void FSyncInteger::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%d", Value);
}

void FSyncInteger::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncInteger::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncInteger* FromVar = static_cast<const FSyncInteger*>(&From);
	const FSyncInteger* ToVar = static_cast<const FSyncInteger*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncInteger64
UScriptStruct* FSyncInteger64::GetScriptStruct() const
{
	return FSyncInteger64::StaticStruct();
}

void FSyncInteger64::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncInteger64::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncInteger64::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncInteger* AuthorityVar = static_cast<const FSyncInteger*>(&Auth);
	return AuthorityVar->Value != Value;
}

void FSyncInteger64::SetValue(const FBaseSyncVar* Other)
{
	const FSyncInteger64* OtherVar = static_cast<const FSyncInteger64*>(Other);
	Value = OtherVar->Value;
}

void FSyncInteger64::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%d", Value);
}

void FSyncInteger64::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncInteger64::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncInteger64* FromVar = static_cast<const FSyncInteger64*>(&From);
	const FSyncInteger64* ToVar = static_cast<const FSyncInteger64*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncFloat
UScriptStruct* FSyncFloat::GetScriptStruct() const
{
	return FSyncFloat::StaticStruct();
}

void FSyncFloat::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncFloat::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncFloat::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncFloat* AuthorityVar = static_cast<const FSyncFloat*>(&Auth);
	return !FMath::IsNearlyEqual(AuthorityVar->Value,Value);
}

void FSyncFloat::SetValue(const FBaseSyncVar* Other)
{
	const FSyncFloat* OtherVar = static_cast<const FSyncFloat*>(Other);
	Value = OtherVar->Value;
}

void FSyncFloat::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%f", Value);
}

void FSyncFloat::AddReferencedObjects(FReferenceCollector& Collector)
{
	FBaseSyncVar::AddReferencedObjects(Collector);
}

void FSyncFloat::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncFloat* FromVar = static_cast<const FSyncFloat*>(&From);
	const FSyncFloat* ToVar = static_cast<const FSyncFloat*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncName
UScriptStruct* FSyncName::GetScriptStruct() const
{
	return FSyncName::StaticStruct();
}

void FSyncName::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncName::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncName::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncName* AuthorityVar = static_cast<const FSyncName*>(&Auth);
	return AuthorityVar->Value != Value;
}

void FSyncName::SetValue(const FBaseSyncVar* Other)
{
	const FSyncName* OtherVar = static_cast<const FSyncName*>(Other);
	Value = OtherVar->Value;
}

void FSyncName::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", TCHAR_TO_ANSI(*Value.ToString()));
}

void FSyncName::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncName::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncName* ToVar = static_cast<const FSyncName*>(&To);
	Value = ToVar->Value;
}
#pragma endregion

#pragma region FSyncString
UScriptStruct* FSyncString::GetScriptStruct() const
{
	return FSyncString::StaticStruct();
}

void FSyncString::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncString::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncString::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncString* AuthorityVar = static_cast<const FSyncString*>(&Auth);
	return AuthorityVar->Value != Value;
}

void FSyncString::SetValue(const FBaseSyncVar* Other)
{
	const FSyncString* OtherVar = static_cast<const FSyncString*>(Other);
	Value = OtherVar->Value;
}

void FSyncString::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", TCHAR_TO_ANSI(*Value));
}

void FSyncString::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncString::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncString* ToVar = static_cast<const FSyncString*>(&To);
	Value = ToVar->Value;
}
#pragma endregion

#pragma region FSyncVector
UScriptStruct* FSyncVector::GetScriptStruct() const
{
	return FSyncVector::StaticStruct();
}

void FSyncVector::Serialize(const FNetSerializeParams& Params)
{
	bool IgnoredSuccess = true;
	Params.Ar << Value;
}

void FSyncVector::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncVector::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncVector* AuthorityVar = static_cast<const FSyncVector*>(&Auth);
	return !Value.Equals(AuthorityVar->Value);
}

void FSyncVector::SetValue(const FBaseSyncVar* Other)
{
	const FSyncVector* OtherVar = static_cast<const FSyncVector*>(Other);
	Value = OtherVar->Value;
}

void FSyncVector::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", TCHAR_TO_ANSI(*Value.ToCompactString()));
}

void FSyncVector::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncVector::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncVector* FromVar = static_cast<const FSyncVector*>(&From);
	const FSyncVector* ToVar = static_cast<const FSyncVector*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncRotator
UScriptStruct* FSyncRotator::GetScriptStruct() const
{
	return FSyncRotator::StaticStruct();
}

void FSyncRotator::Serialize(const FNetSerializeParams& Params)
{
	Params.Ar << Value;
}

void FSyncRotator::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncRotator::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncRotator* AuthorityVar = static_cast<const FSyncRotator*>(&Auth);
	return !Value.Equals(AuthorityVar->Value);
}

void FSyncRotator::SetValue(const FBaseSyncVar* Other)
{
	const FSyncRotator* OtherVar = static_cast<const FSyncRotator*>(Other);
	Value = OtherVar->Value;
}

void FSyncRotator::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", TCHAR_TO_ANSI(*Value.ToCompactString()));
}

void FSyncRotator::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncRotator::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncRotator* FromVar = static_cast<const FSyncRotator*>(&From);
	const FSyncRotator* ToVar = static_cast<const FSyncRotator*>(&To);
	Value = FMath::Lerp(FromVar->Value,ToVar->Value,Pct);
}
#pragma endregion

#pragma region FSyncObject
UScriptStruct* FSyncObject::GetScriptStruct() const
{
	return FSyncObject::StaticStruct();
}

void FSyncObject::Serialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	bool bIsValid = Ar.IsSaving() ?  IsValid(Value) : false;
	Ar.SerializeBits(&bIsValid,1);
	if (bIsValid)
	{
		Ar << Value;
	}
	else
	{
		Value = nullptr;
	}
	
}

void FSyncObject::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncObject::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncObject* AuthorityVar = static_cast<const FSyncObject*>(&Auth);
	return Value != AuthorityVar->Value;
}

void FSyncObject::SetValue(const FBaseSyncVar* Other)
{
	const FSyncObject* OtherVar = static_cast<const FSyncObject*>(Other);
	Value = OtherVar->Value;
}

void FSyncObject::ToString(FAnsiStringBuilderBase& Out) const
{
	if (Value)
	{
		Out.Appendf("%s", TCHAR_TO_ANSI(*GetNameSafe(Value)));
	}
	else
	{
		Out.Append("Invalid");
	}
	
}

void FSyncObject::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncObject::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncObject* ToVar = static_cast<const FSyncObject*>(&To);
	Value = ToVar->Value;
}
#pragma endregion

#pragma region FSyncGameplayTag
UScriptStruct* FSyncGameplayTag::GetScriptStruct() const
{
	return FSyncGameplayTag::StaticStruct();
}

void FSyncGameplayTag::Serialize(const FNetSerializeParams& Params)
{
	bool bSuccess = true;
	Value.NetSerialize(Params.Ar, Params.Map, bSuccess);
}

void FSyncGameplayTag::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FSyncGameplayTag::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	const FSyncGameplayTag* AuthorityVar = static_cast<const FSyncGameplayTag*>(&Auth);
	return Value != AuthorityVar->Value;
}

void FSyncGameplayTag::SetValue(const FBaseSyncVar* Other)
{
	const FSyncGameplayTag* OtherVar = static_cast<const FSyncGameplayTag*>(Other);
	Value = OtherVar->Value;
}

void FSyncGameplayTag::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("%s", TCHAR_TO_ANSI(*Value.ToString()));
}

void FSyncGameplayTag::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSyncGameplayTag::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	const FSyncGameplayTag* ToVar = static_cast<const FSyncGameplayTag*>(&To);
	Value = ToVar->Value;
}
#pragma endregion