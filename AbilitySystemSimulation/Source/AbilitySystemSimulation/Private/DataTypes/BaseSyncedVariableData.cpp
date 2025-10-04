// Fill out your copyright notice in the Description page of Project Settings.


#include "DataTypes/BaseSyncedVariableData.h"

#include "Abilities/NpGameplayAbility.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "SyncVariablesData"
FBaseSyncVar::FBaseSyncVar()
{
}

UScriptStruct* FBaseSyncVar::GetScriptStruct() const
{
	checkf(false, TEXT("FBaseSyncVar::GetScriptStruct() being called erroneously. This should always be overridden in derived types!"));
	return FBaseSyncVar::StaticStruct();
}

void FBaseSyncVar::NetSerialize(const FNetSerializeParams& Params, const FBaseSyncVar* CDOVar)
{
	bool SameAsDefault = Params.Ar.IsSaving() ? !ShouldReconcile(*CDOVar) : false;
	Params.Ar.SerializeBits(&SameAsDefault,1);
	if (SameAsDefault)
	{
		SetValue(CDOVar);
		return;
	}
	Serialize(Params);
}

void FBaseSyncVar::NetDeltaSerialize(const FNetSerializeParams& Params, const FBaseSyncVar* CDOVar)
{
	bool SameAsDefault = Params.Ar.IsSaving() ? !ShouldReconcile(*CDOVar) : false;
	Params.Ar.SerializeBits(&SameAsDefault,1);
	if (SameAsDefault)
	{
		SetValue(CDOVar);
		return;
	}
	const FBaseSyncVar* BaseDeltaVar = Params.GetBaseDeltaState<FBaseSyncVar>();
	bool SameAsDelta = Params.Ar.IsSaving() ? !ShouldReconcile(*BaseDeltaVar) : false;
	Params.Ar.SerializeBits(&SameAsDelta,1);
	if (SameAsDelta)
	{
		SetValue(BaseDeltaVar);
		return;
	}
	SerializeDelta(Params);
}

void FBaseSyncVar::Serialize(const FNetSerializeParams& Params)
{
	checkf(false, TEXT("FBaseSyncVar::Serialize() being called erroneously. This should always be overridden in derived types!"));
}

void FBaseSyncVar::SerializeDelta(const FNetSerializeParams& Params)
{
	Serialize(Params);
}

bool FBaseSyncVar::ShouldReconcile(const FBaseSyncVar& Auth) const
{
	checkf(false, TEXT("FBaseSyncVar::SerializeDelta() being called erroneously. This should always be overridden in derived types!"));
	return false;
}

static void FSyncVarDeleter(FBaseSyncVar* Object)
{
	if (!Object)
	{
		return;
	}
	check(Object);
	UScriptStruct* ScriptStruct = Object->GetScriptStruct();
	check(ScriptStruct);
	ScriptStruct->DestroyStruct(Object);
	FMemory::Free(Object);
}

TSharedPtr<FBaseSyncVar> FBaseSyncVar::CloneShared() const
{
	const UScriptStruct* ScriptStruct = GetScriptStruct();
	if (!ScriptStruct)
	{
		return nullptr;
	}
	FBaseSyncVar* NewDataBlock = static_cast<FBaseSyncVar*>(
			FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize()));
	ScriptStruct->InitializeStruct(NewDataBlock);
	ScriptStruct->CopyScriptStruct(NewDataBlock, this);
	return MakeShareable(NewDataBlock, &FSyncVarDeleter);
}

void FBaseSyncVar::SetValue(const FBaseSyncVar* Other)
{
	checkf(false, TEXT("FBaseSyncVar::SetValue() being called erroneously. This should always be overridden in derived types!"));
}

void FBaseSyncVar::Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct)
{
	checkf(false, TEXT("FBaseSyncVar::Interpolate() being called erroneously. This should always be overridden in derived types!"));
}

struct FSyncVarDeleter
{
	FORCEINLINE void operator()(FBaseSyncVar* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};

#pragma region FSyncVarCollection
FSyncVarCollection::FSyncVarCollection()
{
}

bool FSyncVarCollection::NetSerialize(const FNetSerializeParams& Params, const UNpGameplayAbility* AbilityCDO)
{
	NetSerializeDataArray(Params,SyncedVars,AbilityCDO);
	return true;
}

bool FSyncVarCollection::NetDeltaSerialize(const FNetSerializeParams& Params, const UNpGameplayAbility* AbilityCDO)
{
	NetDeltaSerializeDataArray(Params,SyncedVars,AbilityCDO);
	return true;
}

FSyncVarCollection& FSyncVarCollection::operator=(const FSyncVarCollection& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		// Deep copy active data blocks
		SyncedVars.SetNum(Other.SyncedVars.Num());
		for (int32 i = 0; i < Other.SyncedVars.Num(); ++i)
		{
			if (Other.SyncedVars[i].IsValid())
			{
				SyncedVars[i] = Other.SyncedVars[i]->CloneShared();
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("FAbilityDataCollection::operator= trying to copy invalid Other DataArray element"));
			}
		}
	}
	return *this;
}

bool FSyncVarCollection::operator==(const FSyncVarCollection& Other) const
{
	// Deep move-by-move comparison
	if (SyncedVars.Num() != Other.SyncedVars.Num())
	{
		return false;
	}
	for (int32 i = 0; i < SyncedVars.Num(); ++i)
	{
		check(SyncedVars[i].IsValid() && Other.SyncedVars[i].IsValid());
		if (SyncedVars[i]->ShouldReconcile(*Other.SyncedVars[i]))
		{
			return false;
		}
	}
	return true;
}

bool FSyncVarCollection::operator!=(const FSyncVarCollection& Other) const
{
	return  !(FSyncVarCollection::operator==(Other));
}

bool FSyncVarCollection::ShouldReconcile(const FSyncVarCollection& Other) const
{
	if (SyncedVars.Num() != Other.SyncedVars.Num())
	{
		return true;
	}
	for (int32 i = 0; i < SyncedVars.Num(); ++i)
	{
		const FBaseSyncVar* DataElement = SyncedVars[i].Get();
		const FBaseSyncVar* OtherDataElement = Other.SyncedVars[i].Get();
		// use check for validity here. this should never happen,
		// tasks in the same ability class should be always valid and always 1 to 1.
		check(OtherDataElement != nullptr && DataElement != nullptr)

		if (OtherDataElement->GetScriptStruct() != DataElement->GetScriptStruct())
		{
			return true;
		}

		if (DataElement->ShouldReconcile(*OtherDataElement))
		{
			return true;
		}
	}
	return false;
}

void FSyncVarCollection::Interpolate(const FSyncVarCollection& From, const FSyncVarCollection& To, float Pct)
{
	// interpolating between collections can only happen for the same ability , meaning collections need to match
	check(From.SyncedVars.Num() == To.SyncedVars.Num());
	SyncedVars.SetNumZeroed(From.SyncedVars.Num());
	// Piece-wise interpolation of matching data blocks
	for (int32 i = 0; i < From.SyncedVars.Num(); ++i)
	{
		const FBaseSyncVar* FromElement = From.SyncedVars[i].Get();
		const FBaseSyncVar* ToElement = From.SyncedVars[i].Get();
		check(ToElement != nullptr && FromElement != nullptr)
		check(ToElement->GetScriptStruct() == FromElement->GetScriptStruct())
		SyncedVars[i] = CreateDataByType(FromElement->GetScriptStruct());
		SyncedVars[i]->Interpolate(*FromElement, *ToElement, Pct);
	}
}

void FSyncVarCollection::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const TSharedPtr<FBaseSyncVar>& Data : SyncedVars)
	{
		if (Data.IsValid())
		{
			Data->AddReferencedObjects(Collector);
		}
	}
}

void FSyncVarCollection::ToString(FAnsiStringBuilderBase& Out, const UNpGameplayAbility* AbilityCDO) const
{
	TArray<FSyncVarDef> CDOSyncVars;
	AbilityCDO->GetSyncedVars(CDOSyncVars);
	for (int32 i = 0; i < SyncedVars.Num() ; ++i)
	{
		const TSharedPtr<FBaseSyncVar>& Data = SyncedVars[i];
		if (Data.IsValid())
		{
			const FName VariableName = CDOSyncVars[i].MemberSyncVariable->GetFName();
			Out.Appendf("\n    [%s] :", TCHAR_TO_ANSI(*VariableName.ToString()));
			Data->ToString(Out);
		}
	}
}

TArray<TSharedPtr<FBaseSyncVar>>::TConstIterator FSyncVarCollection::GetCollectionDataIterator() const
{
	return SyncedVars.CreateConstIterator();
}

void FSyncVarCollection::AddData(const TSharedPtr<FBaseSyncVar> DataInstance)
{
	SyncedVars.Add(DataInstance);
}

TSharedPtr<FBaseSyncVar> FSyncVarCollection::CreateDataByType(const UScriptStruct* DataStructType)
{
	FBaseSyncVar* NewDataBlock = static_cast<FBaseSyncVar*>(
		FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize()));
	DataStructType->InitializeStruct(NewDataBlock);
	return TSharedPtr<FBaseSyncVar>(NewDataBlock, &FSyncVarDeleter);
}

FBaseSyncVar* FSyncVarCollection::AddDataByType(const UScriptStruct* DataStructType)
{
	TSharedPtr<FBaseSyncVar> NewDataInstance = CreateDataByType(DataStructType);
	AddData(NewDataInstance);
	return NewDataInstance.Get();
}

FBaseSyncVar* FSyncVarCollection::GetDataAtIndex(const int32& Index) const
{
	if (SyncedVars.IsValidIndex(Index))
	{
		return SyncedVars[Index].Get();
	}
	return nullptr;
}

void FSyncVarCollection::NetSerializeDataArray(const FNetSerializeParams& Params, TArray<TSharedPtr<FBaseSyncVar>>& DataArray,
	const UNpGameplayAbility* AbilityCDO)
{
	TArray<FSyncVarDef> CDOSyncVars;
	AbilityCDO->GetSyncedVars(CDOSyncVars);
	if ( Params.Ar.IsSaving())
	{
		check(DataArray.Num() == CDOSyncVars.Num());
            
		for (int32 i = 0; i < CDOSyncVars.Num(); ++i)
		{
			if (const FProperty* Property = CDOSyncVars[i].MemberSyncVariable)
			{
				void const* VarPtr = Property->ContainerPtrToValuePtr<void>(AbilityCDO);
				const FBaseSyncVar* CDOVar = static_cast<const FBaseSyncVar*>(VarPtr);
                    
				if (DataArray[i].IsValid())
				{
					DataArray[i]->NetSerialize(Params, CDOVar);
				}
			}
		}
	}
	else if ( Params.Ar.IsLoading())
	{
		DataArray.SetNum(CDOSyncVars.Num());

		for (int32 i = 0; i < CDOSyncVars.Num(); ++i)
		{
			if (const FProperty* Property = CDOSyncVars[i].MemberSyncVariable)
			{
				const void* VarPtr = Property->ContainerPtrToValuePtr<void>(AbilityCDO);
				const FBaseSyncVar* CDOVar = static_cast<const FBaseSyncVar*>(VarPtr);
                    
				if (CDOVar)
				{
					DataArray[i] = CDOVar->CloneShared();
					DataArray[i]->NetSerialize(Params, CDOVar);
				}
			}
		}
	}
}

void FSyncVarCollection::NetDeltaSerializeDataArray(const FNetSerializeParams& Params, TArray<TSharedPtr<FBaseSyncVar>>& DataArray,
	const UNpGameplayAbility* AbilityCDO)
{
	TArray<FSyncVarDef> CDOSyncVars;
	AbilityCDO->GetSyncedVars(CDOSyncVars);
	const FSyncVarCollection* BaseCollectionDelta = Params.GetBaseDeltaState<FSyncVarCollection>();
	check(BaseCollectionDelta)
	FNetSerializeParams DeltaParams = Params;
	if (Params.Ar.IsSaving())
	{
		check(DataArray.Num() == CDOSyncVars.Num() == BaseCollectionDelta->SyncedVars.Num());
            
		for (int32 i = 0; i < CDOSyncVars.Num(); ++i)
		{
			if (const FProperty* Property = CDOSyncVars[i].MemberSyncVariable)
			{
				void const* VarPtr = Property->ContainerPtrToValuePtr<void>(AbilityCDO);
				const FBaseSyncVar* CDOVar = static_cast<const FBaseSyncVar*>(VarPtr);
                    
				if (DataArray[i].IsValid())
				{
					DeltaParams.BaseDeltaStatePtr = BaseCollectionDelta->SyncedVars[i].Get();
					DataArray[i]->NetDeltaSerialize(DeltaParams, CDOVar);
				}
			}
		}
	}
	else if (Params.Ar.IsLoading())
	{
		DataArray.SetNum(CDOSyncVars.Num());

		for (int32 i = 0; i < CDOSyncVars.Num(); ++i)
		{
			if (const FProperty* Property = CDOSyncVars[i].MemberSyncVariable)
			{
				const void* VarPtr = Property->ContainerPtrToValuePtr<void>(AbilityCDO);
				const FBaseSyncVar* CDOVar = static_cast<const FBaseSyncVar*>(VarPtr);
                    
				if (CDOVar)
				{
					DataArray[i] = CDOVar->CloneShared();
					DeltaParams.BaseDeltaStatePtr = BaseCollectionDelta->SyncedVars[i].Get();
					DataArray[i]->NetDeltaSerialize(DeltaParams,CDOVar);
				}
			}
		}
	}
}
#pragma endregion

#undef LOCTEXT_NAMESPACE