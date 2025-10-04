#include "DataTypes/BaseTaskData.h"
#include "AbilitySystemLog.h"
#include "Abilities/NpGameplayAbility.h"
#include "Tasks/BasePredictionTask.h"

#define LOCTEXT_NAMESPACE "AbilityData"

///////////////////////////////////////////////////////////////////
#pragma region AbilityTaskDataBase

static void FAbilityTaskDataDeleter(FAbilityTaskDataBase* Object)
{
	if (!Object)
	{
		return;
	}
	const UScriptStruct* ScriptStruct = Object->GetScriptStruct();
	check(ScriptStruct);
	ScriptStruct->DestroyStruct(Object);
	FMemory::Free(Object);
}

TSharedPtr<FAbilityTaskDataBase> FAbilityTaskDataBase::CloneShared() const
{
	const UScriptStruct* ScriptStruct = GetScriptStruct();
	if (!ScriptStruct)
	{
		return nullptr;
	}
	FAbilityTaskDataBase* NewDataBlock = static_cast<FAbilityTaskDataBase*>(
			FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize()));
	ScriptStruct->InitializeStruct(NewDataBlock);
	ScriptStruct->CopyScriptStruct(NewDataBlock, this);
	return MakeShareable(NewDataBlock, &FAbilityTaskDataDeleter);
}


UScriptStruct* FAbilityTaskDataBase::GetScriptStruct() const
{
	ensureMsgf(false, TEXT("FAbilityTaskDataBase::GetScriptStruct() being called erroneously. This should always be overridden in derived types!"));
	UE_LOG(LogTemp, Error, TEXT("FAbilityTaskDataBase::GetScriptStruct() being called erroneously. This should always be overridden in derived types!"));
	return FAbilityTaskDataBase::StaticStruct();
}

bool FAbilityTaskDataBase::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	checkf(false, TEXT("FAbilityTaskDataBase::ShouldReconcile being called erroneously. This should always be overridden in derived types!"));
	UE_LOG(LogTemp, Error, TEXT("FAbilityTaskDataBase::ShouldReconcile being called erroneously. This should always be overridden in derived types!"));
	return false;
}

#pragma endregion

#pragma region AbilityTaskDataArray
FAbilityTaskDataContainer& FAbilityTaskDataContainer::operator=(const FAbilityTaskDataContainer& Other)
{
	if (this != &Other)
	{
		IsActive = Other.IsActive;
		if (Other.TaskDataPointer.IsValid())
		{
			TaskDataPointer = Other.TaskDataPointer->CloneShared();
		}
		else
		{
			TaskDataPointer.Reset();
		}
	}
	return *this;
}

TSharedPtr<FAbilityTaskDataBase> FAbilityTaskDataArray::CreateDataByType(const UScriptStruct* DataStructType)
{
	FAbilityTaskDataBase* NewDataBlock = static_cast<FAbilityTaskDataBase*>(FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize()));
	DataStructType->InitializeStruct(NewDataBlock);
	return TSharedPtr<FAbilityTaskDataBase>(NewDataBlock, &FAbilityTaskDataDeleter);
}

void FAbilityTaskDataArray::NetSerialize(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO)
{
	NetSerializeDataArray(Params,AbilityCDO, AbilityTasksData);
}

void FAbilityTaskDataArray::NetDeltaSerialize(const FNetSerializeParams& Params, const UNpGameplayAbility* AbilityCDO)
{
	NetDeltaSerializeDataArray(Params,AbilityCDO, AbilityTasksData);
}

bool FAbilityTaskDataArray::ShouldReconcile(const FAbilityTaskDataArray& AuthorityState) const
{
	// Deep state-by-state comparison
	return *this != AuthorityState;
}

bool FAbilityTaskDataArray::operator==(const FAbilityTaskDataArray& Other) const
{
	// Deep state-by-state comparison
	if (AbilityTasksData.Num() != Other.AbilityTasksData.Num())
	{
		return false;
	}

	for (int32 i = 0; i < AbilityTasksData.Num(); ++i)
	{
		const FAbilityTaskDataContainer& AuthorityContainer = Other.AbilityTasksData[i];
		// not same activation state early out false
		if (AbilityTasksData[i].IsActive != AuthorityContainer.IsActive)
		{
			return false;
		}

		// Active State
		if (AbilityTasksData[i].IsActive)
		{
			if (!AbilityTasksData[i].TaskDataPointer.IsValid() && !AuthorityContainer.TaskDataPointer.IsValid())
			{
				continue;
			}
			// one of them not valid early out false
			if (AbilityTasksData[i].TaskDataPointer.IsValid() != AuthorityContainer.TaskDataPointer.IsValid())
			{
				return false;
			}
			// not same type early out false
			if (AbilityTasksData[i].TaskDataPointer->GetScriptStruct() != AuthorityContainer.TaskDataPointer->GetScriptStruct())
			{
				return false;
			}
			// should reconcile (user defined equality of the use define data)
			if (AbilityTasksData[i].TaskDataPointer->ShouldReconcile(*AuthorityContainer.TaskDataPointer))
			{
				return false;
			}
		}
	}
	return true;
}

void FAbilityTaskDataArray::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (const FAbilityTaskDataContainer& PointerContainer : AbilityTasksData)
	{
		if (PointerContainer.TaskDataPointer.IsValid())
		{
			PointerContainer.TaskDataPointer->AddReferencedObjects(Collector);
		}
	}
}

void FAbilityTaskDataArray::ToString(FAnsiStringBuilderBase& Out, const TArray<FName>& TasksNames) const
{
	for (int32 i = 0 ; i < AbilityTasksData.Num() ; ++i)
	{
		const FName TaskName = TasksNames.IsValidIndex(i)
			? TasksNames[i]
			: FName(FString::FromInt(i));

		Out.Appendf("      [%s]\n",TCHAR_TO_ANSI(*TaskName.ToString()));
		Out.Appendf("      Is Active :%s\n", AbilityTasksData[i].IsActive ? "True" : "False");
		if (AbilityTasksData[i].TaskDataPointer.IsValid())
		{
			AbilityTasksData[i].TaskDataPointer->ToString(Out);
		}
		else if (AbilityTasksData[i].IsActive)
		{
			Out.Append(TEXT("      No Data\n"));
		}
	}
}

void FAbilityTaskDataArray::Interpolate(const FAbilityTaskDataArray& From, const FAbilityTaskDataArray& To,
	float Pct)
{
	check(From.AbilityTasksData.Num() == To.AbilityTasksData.Num())
	AbilityTasksData.SetNum(From.AbilityTasksData.Num());
	for (int32 i = 0; i < From.AbilityTasksData.Num(); ++i)
	{
		AbilityTasksData[i].IsActive = To.AbilityTasksData[i].IsActive;

		const FAbilityTaskDataBase* FromElement = From.AbilityTasksData[i].TaskDataPointer.Get();
		const FAbilityTaskDataBase* ToElement = To.AbilityTasksData[i].TaskDataPointer.Get();
		if(FromElement == nullptr && ToElement != nullptr)
		{
			AbilityTasksData[i].TaskDataPointer = ToElement->CloneShared();
			return;
		}
		if(ToElement == nullptr)
		{
			AbilityTasksData[i].TaskDataPointer = nullptr;
			return;
		}
		AbilityTasksData[i].TaskDataPointer = CreateDataByType(FromElement->GetScriptStruct());
		AbilityTasksData[i].TaskDataPointer->Interpolate(*FromElement, *ToElement, Pct);
	}
}

void FAbilityTaskDataArray::NetSerializeDataArray(const FNetSerializeParams& Params,const UNpGameplayAbility* AbilityCDO, TArray<FAbilityTaskDataContainer>& AbilityTasksDataArray)
{
	FArchive& Ar = Params.Ar;
	uint8 Num = AbilityCDO->PredictionTasksInstances.Num();
	if (Ar.IsLoading())
	{
		// set num zeroed DOES NOT set existing members to zero, only new ones. 
		AbilityTasksDataArray.SetNum(Num);
	}
	for (int32 i = 0; i < Num && !Ar.IsError(); ++i)
	{
		//Active Bit
		Ar.SerializeBits(&AbilityTasksDataArray[i].IsActive, 1);
		//Data
		bool NoData = Ar.IsSaving() ? !AbilityTasksDataArray[i].TaskDataPointer.IsValid()  : false;
		Ar.SerializeBits(&NoData, 1);
		if ((NoData || !AbilityTasksDataArray[i].IsActive))
		{
			AbilityTasksDataArray[i].TaskDataPointer.Reset();
			continue;
		}
		// Notify Sync data
		TCheckedObjPtr<UScriptStruct> ScriptStruct = AbilityCDO->PredictionTasksInstances[i]->DataType;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();
		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FNotifySyncStateBase for security reasons:
			// If FNotifySyncStateArray is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FLayeredMoveBase, and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FAbilityTaskDataBase::StaticStruct())
				{
					bIsDerivedFromBase = true;
					break;
				}
				CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
			}
			if (bIsDerivedFromBase)
			{
				if (Ar.IsLoading())
				{
					if (AbilityTasksDataArray[i].TaskDataPointer.IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FAbilityTaskDataBase* NewMove = static_cast<FAbilityTaskDataBase*>(
								FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize()));
						ScriptStruct->InitializeStruct(NewMove);
						AbilityTasksDataArray[i].TaskDataPointer = MakeShareable(NewMove, &FAbilityTaskDataDeleter);
					}
				}
				AbilityTasksDataArray[i].TaskDataPointer->NetSerialize(Params);
			}
			else
			{
				UE_LOG(LogAbilitySystem, Error, TEXT("FNotifySyncStateArray::NetSerialize: ScriptStruct not derived from FNotifySyncStateBase attempted to serialize."));
				Ar.SetError();
				break;
			}
		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogAbilitySystem, Error, TEXT("FNotifySyncStateArray::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}
}

void FAbilityTaskDataArray::NetDeltaSerializeDataArray(const FNetSerializeParams& Params,
	const UNpGameplayAbility* AbilityCDO, TArray<FAbilityTaskDataContainer>& AbilityTasksDataArray)
{
	check(AbilityCDO);
	FArchive& Ar = Params.Ar;
	const FAbilityTaskDataArray* BaseDelta = Params.GetBaseDeltaState<FAbilityTaskDataArray>();
	check(BaseDelta);
	FNetSerializeParams DeltaParams = Params;
	uint8 Num = AbilityCDO->PredictionTasksInstances.Num();
	if (Ar.IsLoading())
	{
		// set num zeroed DOES NOT set existing members to default, only new ones. 
		AbilityTasksDataArray.SetNum(Num);
	}
	for (int32 i = 0; i < Num && !Ar.IsError(); ++i)
	{
		//Active Bit
		Ar.SerializeBits(&AbilityTasksDataArray[i].IsActive, 1);
		//Data
		bool NoData = Ar.IsSaving() ? !AbilityTasksDataArray[i].TaskDataPointer.IsValid()  : false;
		Ar.SerializeBits(&NoData, 1);
		if (NoData)
		{
			AbilityTasksDataArray[i].TaskDataPointer.Reset();
			continue;
		}
		if (AbilityTasksDataArray[i].IsActive || BaseDelta->AbilityTasksData[i].IsActive)
		{
			// Notify Sync data
			TCheckedObjPtr<UScriptStruct> ScriptStruct = AbilityCDO->PredictionTasksInstances[i]->DataType;
			UScriptStruct* ScriptStructLocal = ScriptStruct.Get();
			if (ScriptStruct.IsValid())
			{
				// Restrict replication to derived classes of FNotifySyncStateBase for security reasons:
				// If FNotifySyncStateArray is replicated through a Server RPC, we need to prevent clients from sending us
				// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
				// for invalid structs. All provided sources are direct children of FLayeredMoveBase, and we never expect to have deep hierarchies
				// so this should not be too costly
				bool bIsDerivedFromBase = false;
				UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
				while (CurrentSuperStruct)
				{
					if (CurrentSuperStruct == FAbilityTaskDataBase::StaticStruct())
					{
						bIsDerivedFromBase = true;
						break;
					}
					CurrentSuperStruct = CurrentSuperStruct->GetSuperStruct();
				}
				if (bIsDerivedFromBase)
				{
					if (Ar.IsLoading())
					{
						if (AbilityTasksDataArray[i].TaskDataPointer.IsValid() && ScriptStructLocal == ScriptStruct.Get())
						{
							// What we have locally is the same type as we're being serialized into, so we don't need to
							// reallocate - just use existing structure
						}
						else
						{
							// For now, just reset/reallocate the data when loading.
							// Longer term if we want to generalize this and use it for property replication, we should support
							// only reallocating when necessary
							FAbilityTaskDataBase* NewMove = static_cast<FAbilityTaskDataBase*>(
									FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize()));
							ScriptStruct->InitializeStruct(NewMove);
							AbilityTasksDataArray[i].TaskDataPointer = MakeShareable(NewMove, &FAbilityTaskDataDeleter);
						}
					}
					DeltaParams.BaseDeltaStatePtr = &BaseDelta->AbilityTasksData[i].TaskDataPointer;
					AbilityTasksDataArray[i].TaskDataPointer->NetDeltaSerialize(DeltaParams);
				}
				else
				{
					UE_LOG(LogAbilitySystem, Error, TEXT("FNotifySyncStateArray::NetSerialize: ScriptStruct not derived from FNotifySyncStateBase attempted to serialize."));
					Ar.SetError();
					break;
				}
			}
			else if (ScriptStruct.IsError())
			{
				UE_LOG(LogAbilitySystem, Error, TEXT("FNotifySyncStateArray::NetSerialize: Invalid ScriptStruct serialized."));
				Ar.SetError();
				break;
			}
			continue;
		}

		// copy the data from base delta if we are not active in current and base delta (Rule : we can't change data when inactive)
		AbilityTasksDataArray[i] = BaseDelta->AbilityTasksData[i];
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
