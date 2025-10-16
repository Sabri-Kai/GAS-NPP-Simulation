// Fill out your copyright notice in the Description page of Project Settings.


#include "MontageSimulator/NetMontageSimulatorData.h"

#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "MoverTypes.h"
#include "NetworkPredictionReplicationProxy.h"
#include "NetworkPredictionTrace.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DefaultMovementSet/Settings/CommonLegacyMovementSettings.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MontageSimulator/SyncedNotifyInterface.h"
#include "MoveLibrary/MovementUtils.h"

inline ISyncedNotifyInterface* GetInterfaceFromPredictedNotifyEvent(const FAnimNotifyEvent& NotifyEvent)
{
	return NotifyEvent.NotifyStateClass
			   ? Cast<ISyncedNotifyInterface>(NotifyEvent.NotifyStateClass.Get())
			   : Cast<ISyncedNotifyInterface>(NotifyEvent.Notify.Get());
}

static bool IsPredictiveNotify(const FAnimNotifyEvent& Notify)
{
	return (Notify.NotifyStateClass && Notify.NotifyStateClass->Implements<USyncedNotifyInterface>())
		|| (Notify.Notify && Notify.Notify->Implements<USyncedNotifyInterface>());
}

FSyncedNotifiesArray::FSyncedNotifiesArray(const UAnimMontage* InMontage)
{
	Montage = InMontage;
	Indexes.Reset();
	if (Montage)
	{
		for (int32 Index = 0; Index < InMontage->Notifies.Num(); ++Index)
		{
			if (const FAnimNotifyEvent& Notify = InMontage->Notifies[Index];
				IsPredictiveNotify(Notify))
			{
				Indexes.Emplace(Index);
			}
		}
	}
}

bool FSyncedNotifiesArray::IsValidIndex(int32 Index) const
{
	return Indexes.IsValidIndex(Index);
}

#pragma region Synced Montage Root Motion Layered Move
bool FLayeredMove_SyncMontageRootMotion::GenerateMove(const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard,FProposedMove& OutProposedMove)
{
	const float DeltaTime = TimeStep.StepMs * 0.001;
	const FMoverDefaultSyncState* SyncState = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	const FCharacterDefaultInputs* InputState = StartState.InputCmd.InputCollection.FindDataByType<FCharacterDefaultInputs>();
	const UCommonLegacyMovementSettings* Settings = MoverComp->FindSharedSettings<UCommonLegacyMovementSettings>();
	/*
	 * using ASC to save the mesh relative transform when actor info changes is not the best, should use value from mover
	 * but there's no public read access to it in 5.5
	 */
	FTransform MeshRelativeTransform = FTransform::Identity;
	
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MoverComp->GetOwner()))
	{
		if ( const UNpAbilitySystemComponent* NpAsc = Cast<UNpAbilitySystemComponent>(ASC))
		{
			MeshRelativeTransform = NpAsc->GetMeshRelativeTransform();
		}
		
	}
	
	
	MixMode = EMoveMixMode::AdditiveVelocity;
	OutProposedMove.MixMode = EMoveMixMode::AdditiveVelocity;
	OutProposedMove = FProposedMove();
	if (!SyncState || !ExtractionData.HasRootMotion)
	{
		return false;
	}
	if (InputState)
	{
		OutProposedMove.DirectionIntent = InputState->GetMoveInput_WorldSpace();
	}
	// Get The Root Motion Transform This Move Was Added With
	FTransform LocalRootMotion = FTransform(ExtractionData.RootMotionRotation, ExtractionData.RootMotionTranslation);
	// Scale it using animation curve
	if (ExtractionData.Montage && ExtractionData.Montage->HasCurveData("RootMotionTranslationScale"))
	{
		const float LocScaleCurve = ExtractionData.Montage->EvaluateCurveData("RootMotionTranslationScale", ExtractionData.CurrentTime, false);
		LocalRootMotion.ScaleTranslation(LocScaleCurve);
	}
	// Convert Root motion to world root motion
	const FTransform ActorTransform = FTransform(SyncState->GetOrientation_WorldSpace(), SyncState->GetLocation_WorldSpace());
	const FTransform MeshBaseTransform = MeshRelativeTransform;
	FTransform WorldRootMotion = ConvertRootMotionToWorld(LocalRootMotion, ActorTransform,MeshBaseTransform);
	OutProposedMove.LinearVelocity = WorldRootMotion.GetTranslation() / DeltaTime;
	if (!Settings)
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
		MixMode = EMoveMixMode::AdditiveVelocity;
		OutProposedMove.MixMode = MixMode;
		return false;
	}
	// Add Gravity velocity if montage doesn't have "Flying" curve and we are in air mode
	const float FlyingMontage = ExtractionData.Montage ? ExtractionData.Montage->EvaluateCurveData("NoGravityRootMotion", ExtractionData.CurrentTime, false) : 0.f;
	if (StartState.SyncState.MovementMode == Settings->AirMovementModeName && FlyingMontage <= UE_KINDA_SMALL_NUMBER)
	{
		// if in air and not flying , if we have overrideZVelocity curve if so use its value, otherwise apply gravity acceleration
		const float OverrideValue = ExtractionData.Montage->EvaluateCurveData("OverrideZVelocity", ExtractionData.CurrentTime, false);
		const float OverrideZ = ExtractionData.Montage ?  OverrideValue : 0.f;
		if (OverrideZ > KINDA_SMALL_NUMBER)
		{
			OutProposedMove.LinearVelocity.Z = OverrideValue;
		}
		else
		{
			OutProposedMove.LinearVelocity.Z = FMath::Min(SyncState->GetVelocity_WorldSpace().Z,0.f);
			OutProposedMove.LinearVelocity +=  UMovementUtils::ComputeVelocityFromGravity(MoverComp->GetGravityAcceleration(), DeltaTime);
		}
	}
	OutProposedMove.AngularVelocity = WorldRootMotion.GetRotation().Rotator() * (1.f / DeltaTime);

	// if montage has curve additive root motion , Let movement mode take over the motion.
	const float AdditiveMontage = ExtractionData.Montage ? ExtractionData.Montage->EvaluateCurveData("AdditiveRootMotion", ExtractionData.CurrentTime, false) : 0.f;
	if (FMath::IsNearlyZero(AdditiveMontage))
	{
		MixMode = EMoveMixMode::OverrideVelocity;
	}
	else
	{
		OutProposedMove.LinearVelocity = FVector::ZeroVector;
		OutProposedMove.AngularVelocity = FRotator::ZeroRotator;
		MixMode = EMoveMixMode::AdditiveVelocity;
	}
	OutProposedMove.MixMode = MixMode;
	return true;
}

FLayeredMoveBase* FLayeredMove_SyncMontageRootMotion::Clone() const
{
	FLayeredMove_SyncMontageRootMotion* CopyPtr = new FLayeredMove_SyncMontageRootMotion(*this);
	return CopyPtr;
}

void FLayeredMove_SyncMontageRootMotion::NetSerialize(FArchive& Ar)
{
	/*
	uint8 MixModeAsU8 = (uint8)MixMode;
	Ar << MixModeAsU8;
	MixMode = (EMoveMixMode)MixModeAsU8;
	*/
	Ar.SerializeBits(&MixMode, 2);

	uint32 TimeAsInt = StartSimTimeMs;
	Ar.SerializeIntPacked(TimeAsInt);
	StartSimTimeMs = TimeAsInt;

	DurationMs = 0.f;
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	Priority = 0.f;
}

UScriptStruct* FLayeredMove_SyncMontageRootMotion::GetScriptStruct() const
{
	return FLayeredMove_SyncMontageRootMotion::StaticStruct();
}

FString FLayeredMove_SyncMontageRootMotion::ToSimpleString() const
{
	return "Has Active root Motion";
}

void FLayeredMove_SyncMontageRootMotion::AddReferencedObjects(class FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(ExtractionData.Montage);
}

FTransform FLayeredMove_SyncMontageRootMotion::ConvertRootMotionToWorld(const FTransform& LocalRootMotion,
	const FTransform& ActorTransform, const FTransform& MeshBaseTransform)
{
	//Calculate new actor transform after applying root motion to this component
	const FTransform ActorToWorld = ActorTransform;
	FTransform MeshWorldTransform = MeshBaseTransform * ActorToWorld;
	const FTransform ComponentToActor = ActorToWorld.GetRelativeTransform(MeshWorldTransform);
	const FTransform NewComponentToWorld = LocalRootMotion * MeshWorldTransform;
	const FTransform NewActorTransform = ComponentToActor * NewComponentToWorld;
	
	const FVector DeltaWorldTranslation = NewActorTransform.GetTranslation() - ActorToWorld.GetTranslation();
	const FQuat NewWorldRotation = MeshWorldTransform.GetRotation() * LocalRootMotion.GetRotation();
	const FQuat DeltaWorldRotation = NewWorldRotation * MeshWorldTransform.GetRotation().Inverse();
	return FTransform(DeltaWorldRotation, DeltaWorldTranslation);
}
#pragma endregion

#pragma region Notify Synced Data
FSyncedNotifyData::FSyncedNotifyData()
{
}
FSyncedNotifyData* FSyncedNotifyData::Clone() const
{
	checkNoEntry()
	return nullptr;
}
UScriptStruct* FSyncedNotifyData::GetScriptStruct() const
{
	return FSyncedNotifyData::StaticStruct();
}
void FSyncedNotifyData::ToString(FAnsiStringBuilderBase& Out) const
{
	//check(true)
}

bool FSyncedNotifyData::ShouldReconcile(const FSyncedNotifyData& AuthorityState) const
{
	//check(true)
	return false;
}
void FSyncedNotifyData::Interpolate(const FSyncedNotifyData& From, const FSyncedNotifyData& To, float Pct)
{
	//check(true)
}



// container
FSyncedNotifyDataContainer& FSyncedNotifyDataContainer::operator=(const FSyncedNotifyDataContainer& Other)
{
	if (Other.SyncStatePointer.IsValid())
	{
		SyncStatePointer = Other.SyncStatePointer->CloneShared();
	}
	else
	{
		SyncStatePointer = nullptr;
	}
	IsActive = Other.IsActive;
	return *this;
}

bool FSyncedNotifyDataContainer::operator==(const FSyncedNotifyDataContainer& Other) const
{
	if (this == &Other)
	{
		return true;
	}
	if (IsActive != Other.IsActive)
	{
		return false;
	}
	if (IsActive)
	{
		if (SyncStatePointer.IsValid() != Other.SyncStatePointer.IsValid())
		{
			return false;
		}
		if (SyncStatePointer.IsValid())
		{
			if (SyncStatePointer->GetScriptStruct() != Other.SyncStatePointer->GetScriptStruct())
			{
				return false;
			}
			return !SyncStatePointer->ShouldReconcile(*Other.SyncStatePointer);
		}
	}
	
	return true;
}

bool FSyncedNotifyDataContainer::operator!=(const FSyncedNotifyDataContainer& Other) const
{
	return !(FSyncedNotifyDataContainer::operator==(Other));
}
#pragma region // FSyncedNotifyDataArray

FSyncedNotifyDataArray::FSyncedNotifyDataArray()
{
}
struct FSyncedNotifyDataDeleter
{
	FORCEINLINE void operator()(FSyncedNotifyData* Object) const
	{
		check(Object);
		UScriptStruct* ScriptStruct = Object->GetScriptStruct();
		check(ScriptStruct);
		ScriptStruct->DestroyStruct(Object);
		FMemory::Free(Object);
	}
};
FSyncedNotifyDataContainer FSyncedNotifyDataArray::AddNotifySyncState(const UScriptStruct* DataStructType , const uint8 InNotifyIndex)
{
	FSyncedNotifyDataContainer OutContainer;
	if (DataStructType == nullptr || !DataStructType->IsChildOf(FSyncedNotifyData::StaticStruct()))
	{
		
		ActiveNotifySyncStates.Add(OutContainer);
		return OutContainer;
	}
	TSharedPtr<FSyncedNotifyData> DataToAdd = CreateDataByType(DataStructType);
	OutContainer.SyncStatePointer = DataToAdd;
	ActiveNotifySyncStates.Add(OutContainer);
	return OutContainer;
}
FSyncedNotifyDataContainer& FSyncedNotifyDataArray::OverrideNotifySyncState(const UScriptStruct* DataStructType, const uint8 InNotifyIndex)
{
	FSyncedNotifyDataContainer& Container = ActiveNotifySyncStates[InNotifyIndex];
	Container.SyncStatePointer = nullptr;
	if (DataStructType == nullptr)
	{
		return Container;
	}
	if (DataStructType->IsChildOf(FSyncedNotifyData::StaticStruct()))
	{
		Container.SyncStatePointer = CreateDataByType(DataStructType);
	}
	return Container;
}
TSharedPtr<FSyncedNotifyData> FSyncedNotifyDataArray::CreateDataByType(const UScriptStruct* DataStructType)
{
	FSyncedNotifyData* NewDataBlock = (FSyncedNotifyData*)FMemory::Malloc(DataStructType->GetCppStructOps()->GetSize());
	DataStructType->InitializeStruct(NewDataBlock);
	return TSharedPtr<FSyncedNotifyData>(NewDataBlock, FSyncedNotifyDataDeleter());
}
void FSyncedNotifyDataArray::NetSerialize(const FNetSerializeParams& P)
{
	NetSerializeNotifyStatesArray(P.Ar, ActiveNotifySyncStates);
}
bool FSyncedNotifyDataArray::ShouldReconcile(const FSyncedNotifyDataArray& AuthorityState) const
{
	// Deep state-by-state comparison
	return *this != AuthorityState;
}
FSyncedNotifyDataArray& FSyncedNotifyDataArray::operator=(const FSyncedNotifyDataArray& Other)
{
	// Perform deep copy of this Group
	if (this != &Other)
	{
		// Deep copy active moves
		ActiveNotifySyncStates.SetNum(Other.ActiveNotifySyncStates.Num());
		for (int32 i = 0; i < Other.ActiveNotifySyncStates.Num(); ++i)
		{
			// this performs a deep copy of the shared ptr
			ActiveNotifySyncStates[i] = Other.ActiveNotifySyncStates[i];
		}
	}
	return *this;
}
bool FSyncedNotifyDataArray::operator==(const FSyncedNotifyDataArray& Other) const
{
	// Deep state-by-state comparison
	if (ActiveNotifySyncStates.Num() != Other.ActiveNotifySyncStates.Num())
	{
		return false;
	}
	for (int32 i = 0; i < ActiveNotifySyncStates.Num(); ++i)
	{
		//Compare in order
		if (Other.ActiveNotifySyncStates[i] != ActiveNotifySyncStates[i])
		{
			return false;
		}
	}
	return true;
}
bool FSyncedNotifyDataArray::operator!=(const FSyncedNotifyDataArray& Other) const
{
	return !(FSyncedNotifyDataArray::operator==(Other));
}
void FSyncedNotifyDataArray::AddStructReferencedObjects(FReferenceCollector& Collector) const
{
	for (FSyncedNotifyDataContainer PointerContainer : ActiveNotifySyncStates)
	{
		if (PointerContainer.SyncStatePointer.IsValid())
		{
			PointerContainer.SyncStatePointer->AddReferencedObjects(Collector);
		}
	}
}
void FSyncedNotifyDataArray::ToString(FAnsiStringBuilderBase& Out) const
{
	for (const auto& NotifyState : ActiveNotifySyncStates)
	{
		Out.Append("\n");
		Out.Appendf(" Activation State : %s\n",NotifyState.IsActive ? "True" : "False");
		if (NotifyState.IsActive && NotifyState.SyncStatePointer.IsValid())
		{
			NotifyState.SyncStatePointer->ToString(Out);
		}
		
	}
}
void FSyncedNotifyDataArray::Clear()
{
	ActiveNotifySyncStates.Empty();
}
void FSyncedNotifyDataArray::NetSerializeNotifyStatesArray(FArchive& Ar, TArray< FSyncedNotifyDataContainer >& NotifySyncStatesArray)
{
	uint32 ArrayNum = Ar.IsSaving() ?  NotifySyncStatesArray.Num() : 0;
	Ar.SerializeIntPacked(ArrayNum);
	if (Ar.IsLoading())
	{
		// set num zeroed DOES NOT set existing members to zero, only new ones. 
		NotifySyncStatesArray.SetNumZeroed(ArrayNum);
	}
	for (uint32 i = 0; i < ArrayNum && !Ar.IsError(); ++i)
	{
		//Active Bit
		Ar.SerializeBits(&NotifySyncStatesArray[i].IsActive, 1);
		bool NoData = Ar.IsSaving() ? !NotifySyncStatesArray[i].SyncStatePointer.IsValid()  : false;
		Ar.SerializeBits(&NoData, 1);
		if (NoData || !NotifySyncStatesArray[i].IsActive)
		{
			NotifySyncStatesArray[i].SyncStatePointer = nullptr;
			continue;
		}
		// Notify Sync data
		TCheckedObjPtr<UScriptStruct> ScriptStruct = NotifySyncStatesArray[i].SyncStatePointer.IsValid() ? NotifySyncStatesArray[i].SyncStatePointer->GetScriptStruct() : nullptr;
		UScriptStruct* ScriptStructLocal = ScriptStruct.Get();
		Ar << ScriptStruct;
		if (ScriptStruct.IsValid())
		{
			// Restrict replication to derived classes of FSyncedNotifyDataBase for security reasons:
			// If FSyncedNotifyDataArray is replicated through a Server RPC, we need to prevent clients from sending us
			// arbitrary ScriptStructs due to the allocation/reliance on GetCppStructOps below which could trigger a server crash
			// for invalid structs. All provided sources are direct children of FLayeredMoveBase, and we never expect to have deep hierarchies
			// so this should not be too costly
			bool bIsDerivedFromBase = false;
			UStruct* CurrentSuperStruct = ScriptStruct->GetSuperStruct();
			while (CurrentSuperStruct)
			{
				if (CurrentSuperStruct == FSyncedNotifyData::StaticStruct())
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
					if (NotifySyncStatesArray[i].SyncStatePointer.IsValid() && ScriptStructLocal == ScriptStruct.Get())
					{
						// What we have locally is the same type as we're being serialized into, so we don't need to
						// reallocate - just use existing structure
					}
					else
					{
						// For now, just reset/reallocate the data when loading.
						// Longer term if we want to generalize this and use it for property replication, we should support
						// only reallocating when necessary
						FSyncedNotifyData* NewMove = (FSyncedNotifyData*)FMemory::Malloc(ScriptStruct->GetCppStructOps()->GetSize());
						ScriptStruct->InitializeStruct(NewMove);
						NotifySyncStatesArray[i].SyncStatePointer = TSharedPtr<FSyncedNotifyData>(NewMove, FSyncedNotifyDataDeleter());
					}
				}
				bool IgnoredSuccess = false;
				NotifySyncStatesArray[i].SyncStatePointer->NetSerialize(Ar, nullptr, IgnoredSuccess);;
			}
			else
			{
				UE_LOG(LogAbilitySystem, Error, TEXT("FSyncedNotifyDataArray::NetSerialize: ScriptStruct not derived from FSyncedNotifyDataBase attempted to serialize."));
				Ar.SetError();
				break;
			}

		}
		else if (ScriptStruct.IsError())
		{
			UE_LOG(LogAbilitySystem, Error, TEXT("FSyncedNotifyDataArray::NetSerialize: Invalid ScriptStruct serialized."));
			Ar.SetError();
			break;
		}
	}
}

const FAnimNotifyEvent* FSyncedNotifyDataArray::GetPredictedNotifyEventFromMontage(UAnimMontage* InMontage, int32 NotifyIndex)
{
	if (!InMontage)
	{
		return nullptr;
	}

	for (int32 Counter = 0, Index = 0; Index < InMontage->Notifies.Num(); ++Index)
	{
		if (IsPredictiveNotify(InMontage->Notifies[Index]))
		{
			if (Counter == NotifyIndex)
			{
				return &InMontage->Notifies[Index];
			}
			Counter++;
		}
	}

	return nullptr;
}

const FAnimNotifyEvent* FSyncedNotifyDataArray::GetNotifyEventAtIndex(UAnimMontage* Montage,const int32& InIndex) const
{
	return GetPredictedNotifyEventFromMontage(Montage, InIndex);
}

bool FSyncedNotifyDataArray::IsNotifyStateAtIndex(UAnimMontage* Montage,const int32& PredictedNotifyIndex) const
{
	if (const FAnimNotifyEvent* const NotifyState = GetNotifyEventAtIndex(Montage,PredictedNotifyIndex))
	{
		return IsValid(NotifyState->NotifyStateClass.GetClass());
	}
	return false;
}

bool FSyncedNotifyDataArray::IsNotifyState(const FAnimNotifyEvent& NotifyEvent)
{
	return IsValid(NotifyEvent.NotifyStateClass.GetClass());
}

bool FSyncedNotifyDataArray::IsNotifyTriggered(const int32& PredictedNotifyIndex)
{
	// ToDo Add Validity check by chcking for 0 index.
	return IsValidStateIndex(PredictedNotifyIndex)
		&& ActiveNotifySyncStates[PredictedNotifyIndex].IsActive;
}

bool FSyncedNotifyDataArray::ShouldNotifyTriggerThisFrame(UAnimMontage*Montage,const int32& InIndex, const float PostTickTime , const float StartTickTime)
{
	if (!IsValidStateIndex(InIndex))
	{
		return false;
	}

	const FAnimNotifyEvent* NotifyEvent = GetPredictedNotifyEventFromMontage(Montage, InIndex);

	if (!NotifyEvent || IsNotifyTriggered(InIndex))
	{
		return false;
	}
	const int32 CurrentTimeMS = FMath::Floor(PostTickTime * 1000.f);
	const int32 PreviousTimeMS = FMath::Floor(StartTickTime * 1000.f);
	const int32 TriggerTimeMS = FMath::Floor(NotifyEvent->GetTriggerTime() * 1000.f);
	const int32 EndTriggerTimeMS = FMath::Floor(NotifyEvent->GetEndTriggerTime() * 1000.f);
	if (IsNotifyState(*NotifyEvent))
	{
		if (CurrentTimeMS >= TriggerTimeMS && CurrentTimeMS < EndTriggerTimeMS)
		{
			return true;
		}
	}
	else
	{
		if ( CurrentTimeMS >= TriggerTimeMS && PreviousTimeMS < TriggerTimeMS)
		{
			return true;
		}
	}

	return false;
}

bool FSyncedNotifyDataArray::ShouldNotifyEndThisFrame(UAnimMontage* Montage,const int32& InIndex, const float PostTickTime)
{
	if (!IsValidStateIndex(InIndex))
	{
		return false;
	}

	const FAnimNotifyEvent* NotifyEvent = GetPredictedNotifyEventFromMontage(Montage, InIndex);
	// if a state didn't trigger yet , we can't end
	if (!NotifyEvent || !IsNotifyTriggered(InIndex))
	{
		return false;
	}
	// ONLY notify state have end event
	// if notify state we should End  this frame in 1 situation
	// 1 - if triggered is already true we just trigger end as long as our current time outside the state.
	const int32 CurrentTimeMS = FMath::Floor(PostTickTime * 1000.f);
	const int32 TriggerTimeMS = FMath::Floor(NotifyEvent->GetTriggerTime() * 1000.f);
	const int32 EndTriggerTimeMS = FMath::Floor(NotifyEvent->GetEndTriggerTime() * 1000.f);
	if (IsNotifyState(*NotifyEvent))
	{
		if (CurrentTimeMS < TriggerTimeMS || CurrentTimeMS > EndTriggerTimeMS)
		{
			return true;
		}
	}
	return false;
}

bool FSyncedNotifyDataArray::ShouldNotifyTickThisFrame(UAnimMontage* Montage,const int32& InIndex, const float StartTickTime)
{
	if (!IsValidStateIndex(InIndex))
	{
		return false;
	}

	const FAnimNotifyEvent* NotifyEvent = GetPredictedNotifyEventFromMontage(Montage, InIndex);

	// if a state didn't trigger yet , we can't tick
	if (!NotifyEvent || !IsNotifyTriggered(InIndex))
	{
		return false;
	}
	// ONLY notify state have Tick event
	const int32 CurrentTimeMS = FMath::Floor(StartTickTime * 1000.f);
	const int32 TriggerTimeMS = FMath::Floor(NotifyEvent->GetTriggerTime() * 1000.f);
	const int32 EndTriggerTimeMS = FMath::Floor(NotifyEvent->GetEndTriggerTime() * 1000.f);
	if (IsNotifyState(*NotifyEvent))
	{
		if (CurrentTimeMS >= TriggerTimeMS && CurrentTimeMS < EndTriggerTimeMS)
		{
			return true;
		}
	}
	return false;
}

bool FSyncedNotifyDataArray::SetupDataForNotifyAtIndex(UAnimMontage* Montage,const int32& Index)
{
	if (ActiveNotifySyncStates[Index].SyncStatePointer.IsValid())
	{
		// there's no need to create a new one when there's already a valid state
		return true;
	}

	const FAnimNotifyEvent* PredictedNotify = GetPredictedNotifyEventFromMontage(Montage, Index);
	if (!PredictedNotify)
	{
		return false;
	}

	const ISyncedNotifyInterface* PredictedNotifyInterface = nullptr;
	TObjectPtr<UObject> InterfaceOwner = nullptr;

	if (PredictedNotify->NotifyStateClass)
	{
		PredictedNotifyInterface = Cast<ISyncedNotifyInterface>(PredictedNotify->NotifyStateClass.Get());
		InterfaceOwner = PredictedNotify->NotifyStateClass.Get();
	}
	else if (PredictedNotify->Notify)
	{
		PredictedNotifyInterface = Cast<ISyncedNotifyInterface>(PredictedNotify->Notify.Get());
		InterfaceOwner = PredictedNotify->Notify.Get();
	}

	if (PredictedNotifyInterface)
	{
		OverrideNotifySyncState(PredictedNotifyInterface->Execute_GetRequiredType(InterfaceOwner), Index);
		return true;
	}
	return false;
}
#pragma endregion
#pragma endregion

#pragma region Montage Player Sync State
FMontageSimSyncState::FMontageSimSyncState()
{
	Reset();
}

void FMontageSimSyncState::NetSerialize(const FNetSerializeParams& P)
{
	FArchive& Ar = P.Ar;
	bool PlayingMontage = Ar.IsSaving() && IsValid(Montage);
	Ar.SerializeBits(&PlayingMontage, 1);
	if (PlayingMontage)
	{
		Ar << Montage;
		Ar.SerializeBits(&bIsPaused,1);
		Ar << MontageTime;
		uint32 RoundedPlayRateMS = Ar.IsSaving() ? FMath::Floor(MontagePlayRate * 1000.f) : 0;
		Ar.SerializeIntPacked(RoundedPlayRateMS);
		MontagePlayRate = RoundedPlayRateMS / 1000.f;
		uint32 RoundedRootMotionScale = Ar.IsSaving() ? FMath::Floor(RootMotionScale * 1000.f) : 0;
		Ar.SerializeIntPacked(RoundedRootMotionScale);
		RootMotionScale = RoundedRootMotionScale / 1000.f;

		NotifySyncStates.NetSerialize(P);
	}
	else
	{
		Montage = nullptr;
		MontageTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
		bIsPaused = false;
		NotifySyncStates.Clear();
	}
}

void FMontageSimSyncState::NetDeltaSerialize(const FNetSerializeParams& P)
{
	NetSerialize(P);
	/*FArchive& Ar = P.Ar;
	const FMontageSimSyncState* BaseState = P.GetBaseDeltaState<FMontageSimSyncState>();
	check(BaseState)
	bool PlayingMontage = Ar.IsSaving() && IsValid(Montage);
	Ar.SerializeBits(&PlayingMontage, 1);
	if (PlayingMontage)
	{
		bool SameMontage = false;
		bool SamePlayRate = false;
		bool SameRootMotionScale = false;
		bool SameBaseData = false;
		if (Ar.IsSaving())
		{
			SameMontage = Montage == BaseState->Montage;
			SamePlayRate = FMath::IsNearlyEqual(MontagePlayRate,BaseState->MontagePlayRate);
			SameRootMotionScale = FMath::IsNearlyEqual(RootMotionScale,BaseState->RootMotionScale);
			SameBaseData = SameMontage && SamePlayRate && SameRootMotionScale;
		}
		// Data the doesn't change often is the same
		Ar.SerializeBits(&SameBaseData, 1);
		if (SameBaseData)
		{
			Montage = BaseState->Montage;
			MontagePlayRate = BaseState->MontagePlayRate;
			RootMotionScale = BaseState->RootMotionScale;
		}
		else
		{
			// Montage
			Ar.SerializeBits(&SameMontage, 1);
			if (SameMontage)
			{
				Montage = BaseState->Montage;
			}
			else
			{
				Ar << Montage;
			}
			//Play Rate
			Ar.SerializeBits(&SamePlayRate, 1);
			if (SamePlayRate)
			{
				MontagePlayRate = BaseState->MontagePlayRate;
			}
			else
			{
				uint32 RoundedPlayRateMS = Ar.IsSaving() ? FMath::Floor(MontagePlayRate * 1000.f) : 0;
				Ar.SerializeIntPacked(RoundedPlayRateMS);
				MontagePlayRate = RoundedPlayRateMS / 1000.f;
			}
			// Root Motion
			Ar.SerializeBits(&SameRootMotionScale, 1);
			if (SameRootMotionScale)
			{
				RootMotionScale = BaseState->RootMotionScale;
			}
			else
			{
				uint32 RoundedRootMotionScale = Ar.IsSaving() ? FMath::Floor(RootMotionScale * 1000.f) : 0;
				Ar.SerializeIntPacked(RoundedRootMotionScale);
				RootMotionScale = RoundedRootMotionScale / 1000.f;
			}
		}
		// already 1 bit no need for delta
		Ar.SerializeBits(&bIsPaused,1);
		// montage time changes let's leave it like this for now
		Ar << MontageTime;
		//ToDo : Add Delta Serialization to notifies
		NotifySyncStates.NetSerialize(P);
	}
	else
	{
		Montage = nullptr;
		MontageTime = 0.f;
		MontagePlayRate = 1.f;
		RootMotionScale = 1.f;
		bIsPaused = false;
		NotifySyncStates.Clear();
	}*/
}

void FMontageSimSyncState::ToString(FAnsiStringBuilderBase& Out) const
{
	if (Montage)
	{
		Out.Appendf("Playing Montage: ");
		Out.Append(TCHAR_TO_ANSI(*GetNameSafe(Montage)));
		Out.Appendf("\n");
		Out.Appendf("Current Time: %f\n", MontageTime);
		Out.Appendf("Play Rate: %f\n", MontagePlayRate);
		Out.Appendf("Root Motion Scale: %f\n", RootMotionScale);
		Out.Appendf("Paused: %s\n", bIsPaused ? "true" : "false");
		Out.Appendf("\n");
		Out.Append("Synced Anim Notifies :\n");
		NotifySyncStates.ToString(Out);
		
	}
	else
	{
		Out.Append("No Montage playing\n");
	}
}

bool FMontageSimSyncState::ShouldReconcile(const FMontageSimSyncState& AuthorityState) const
{
	const bool NotSameMontage = Montage != AuthorityState.Montage;
	const bool NotSameTime = !FMath::IsNearlyEqual(MontageTime , AuthorityState.MontageTime, 0.01f);
	const bool NotSamePlayRate = !FMath::IsNearlyEqual(MontagePlayRate , AuthorityState.MontagePlayRate, 0.01f);
	const bool NotSameRootMotionScale = !FMath::IsNearlyEqual(RootMotionScale , AuthorityState.RootMotionScale, 0.01f);
	const bool DiffNotifiesData = NotifySyncStates.ShouldReconcile(AuthorityState.NotifySyncStates);
	const bool NotSamePause = bIsPaused != AuthorityState.bIsPaused;
	
	UE_NP_TRACE_RECONCILE(NotSameMontage, "Different Montage:");
	UE_NP_TRACE_RECONCILE(NotSamePause, "Different Montage Pause State:");
	UE_NP_TRACE_RECONCILE(NotSameTime, "Different Montage Times");
	UE_NP_TRACE_RECONCILE(NotSamePlayRate, "Different Play Rate");
	UE_NP_TRACE_RECONCILE(NotSameRootMotionScale, "Different Root Motion Scale");
	UE_NP_TRACE_RECONCILE(DiffNotifiesData, "Different Notifies Data");

	return false;
}

void FMontageSimSyncState::Interpolate(const FMontageSimSyncState& From, const FMontageSimSyncState& To, float Pct)
{
	if (From.Montage == To.Montage)
	{
		Montage = To.Montage;
		MontageTime = FMath::Lerp(From.MontageTime, To.MontageTime, Pct);
		MontagePlayRate = FMath::Lerp(From.MontagePlayRate, To.MontagePlayRate, Pct);
		RootMotionScale = FMath::Lerp(From.RootMotionScale, To.RootMotionScale, Pct);
		bIsPaused = To.bIsPaused;
	}
	else
	{
		Montage = To.Montage;
		MontageTime = To.MontageTime;
		MontagePlayRate = To.MontagePlayRate;
		RootMotionScale = To.RootMotionScale;
		bIsPaused = To.bIsPaused;
	}
}

void FMontageSimSyncState::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Montage);
}

bool FAbilityMontagePlayback::IsValidQueue() const
{
	return IsValid(MontageToPlay);
}
#pragma endregion
