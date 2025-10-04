// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/TargetingTasks/TargetingPredictionTask.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Library/AbilitySimulationLibrary.h"
#include "Targeting/TargetingProcessor.h"

#define LOCTEXT_NAMESPACE "TargetingTask"

#pragma region Synced Data
bool FBaseTargetingTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	bool OutSuccess = true;
	uint32 SimTimeAsUint = FMath::RoundToInt32(FMath::Max(StartSimTime,0.f));
	Params.Ar.SerializeIntPacked(SimTimeAsUint);
	StartSimTime = SimTimeAsUint;

	uint32 LastConfirmTimeAsUint = FMath::RoundToInt32(FMath::Max(LastConfirmationTime,0.f));
	Params.Ar.SerializeIntPacked(LastConfirmTimeAsUint);
	LastConfirmationTime = LastConfirmTimeAsUint;
	
	Location.NetSerialize(Params.Ar,Params.Map,OutSuccess);
	Direction.NetSerialize(Params.Ar,Params.Map,OutSuccess);
	// Rot is not used often. as many trace either don't need it or can infer it from the direction.
	bool HasRot = Params.Ar.IsSaving() ? !Rotation.IsNearlyZero(1.f) : false;
	Params.Ar.SerializeBits(&HasRot,1);
	if (HasRot)
	{
		Rotation.SerializeCompressedShort(Params.Ar);
	}
	else
	{
		Rotation = FRotator::ZeroRotator;
	}
	
	UAbilitySimulationLibrary::NetSerializeUniqueActorsArrays(Params, SavedHitActors);
	return true;
}

bool FBaseTargetingTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	bool OutSuccess = true;
	const FBaseTargetingTaskData* TargetingDataDeltaState = Params.GetBaseDeltaState<FBaseTargetingTaskData>();
	// using bools is easier to read and understand for someone new to net serialization, so no flags usage
	// still end result
	bool SameData = false;
	bool SameStartTime = false;
	bool SameConfirmTime = false;
	bool SameLoc = false;
	bool SameDir = false;
	bool SameState = false;
	bool SameRotation = false;
	if (Params.Ar.IsSaving())
	{
		SameStartTime = FMath::IsNearlyEqual(StartSimTime,TargetingDataDeltaState->StartSimTime);
		SameConfirmTime = FMath::IsNearlyEqual(LastConfirmationTime,TargetingDataDeltaState->LastConfirmationTime);
		SameLoc = Location.Equals(TargetingDataDeltaState->Location,2.f);
		SameDir = Direction.Equals(TargetingDataDeltaState->Direction,2.f);
		SameRotation = Rotation.Equals(TargetingDataDeltaState->Rotation,1.f);
		SameData = SameLoc && SameDir && SameRotation && SameState && SameStartTime && SameConfirmTime;
	}
	Params.Ar.SerializeBits(&SameData,1);
	if (SameData)
	{
		Location = TargetingDataDeltaState->Location;
		Direction = TargetingDataDeltaState->Direction;
		Rotation = TargetingDataDeltaState->Rotation;
		StartSimTime = TargetingDataDeltaState->StartSimTime;
		LastConfirmationTime = TargetingDataDeltaState->LastConfirmationTime;
		UAbilitySimulationLibrary::NetDeltaSerializeUniqueActorsArrays(Params,TargetingDataDeltaState->SavedHitActors, SavedHitActors);
		return true;
	}
	
	Params.Ar.SerializeBits(&SameStartTime,1);
	if (SameStartTime)
	{
		StartSimTime = TargetingDataDeltaState->StartSimTime;
	}
	else
	{
		uint32 SimTimeAsUint = FMath::RoundToInt32(FMath::Max(StartSimTime,0.f));
		Params.Ar.SerializeIntPacked(SimTimeAsUint);
		StartSimTime = SimTimeAsUint;
	}

	Params.Ar.SerializeBits(&SameConfirmTime,1);
	if (SameConfirmTime)
	{
		LastConfirmationTime = TargetingDataDeltaState->LastConfirmationTime;
	}
	else
	{
		uint32 LastConfirmTimeAsUint = FMath::RoundToInt32(FMath::Max(LastConfirmationTime,0.f));
		Params.Ar.SerializeIntPacked(LastConfirmTimeAsUint);
		LastConfirmationTime = LastConfirmTimeAsUint;
	}
		
	Params.Ar.SerializeBits(&SameLoc,1);
	if (SameLoc)
	{
		Location = TargetingDataDeltaState->Location;
	}
	else
	{
		Location.NetSerialize(Params.Ar,Params.Map,OutSuccess);
	}

	Params.Ar.SerializeBits(&SameDir,1);
	if (SameDir)
	{
		Direction = TargetingDataDeltaState->Direction;
	}
	else
	{
		Direction.NetSerialize(Params.Ar,Params.Map,OutSuccess);
	}

	Params.Ar.SerializeBits(&SameRotation,1);
	if (SameRotation)
	{
		Rotation = TargetingDataDeltaState->Rotation;
	}
	else
	{
		Rotation.SerializeCompressedShort(Params.Ar);
	}
	UAbilitySimulationLibrary::NetDeltaSerializeUniqueActorsArrays(Params,TargetingDataDeltaState->SavedHitActors, SavedHitActors);
	
	return true;
}

UScriptStruct* FBaseTargetingTaskData::GetScriptStruct() const
{
	return FBaseTargetingTaskData::StaticStruct();
}

void FBaseTargetingTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("\nIgnoredActors:\n");
	for (AActor* Actor : SavedHitActors)
	{
		if (Actor)
		{
			Out.Appendf("%s\n",TCHAR_TO_ANSI(*GetNameSafe(Actor)));
		}
	}
}

bool FBaseTargetingTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FBaseTargetingTaskData* AuthorityData = static_cast<const FBaseTargetingTaskData*>(&AuthorityState);
	if (SavedHitActors.Num() != AuthorityData->SavedHitActors.Num())
	{
		return true;
	}
	if (SavedHitActors.Num() == 0)
	{
		return false;
	}
	TSet<AActor*> AuthoritySet (AuthorityData->SavedHitActors);
	for (AActor* ActorA : SavedHitActors)
	{
		if (!AuthoritySet.Contains(ActorA))
		{
			return true;
		}
	}
	return false;
}

void FBaseTargetingTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FBaseTargetingTaskData* ToData = static_cast<const FBaseTargetingTaskData*>(&To);
	SavedHitActors = ToData->SavedHitActors;
}
#pragma endregion 

#pragma region Base Targeting Task Class
UTargetingPredictionTask::UTargetingPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = FBaseTargetingTaskData::StaticStruct(); 
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UTargetingPredictionTask,ExecuteTask);
	AdditionalInputFunctions.Add(GET_FUNCTION_NAME_CHECKED(UTargetingPredictionTask,ConfirmTargeting));
	AdditionalInputFunctions.Add(GET_FUNCTION_NAME_CHECKED(UTargetingPredictionTask,CancelTargeting));
	ShouldTaskTick = true;
}

void UTargetingPredictionTask::ExecuteTask(FVector InLocation, FVector InDirection, FRotator InRotation)
{
	TargetingData.StartSimTime = GetAbilitySystemComponent()->GetCurrentSimulationTimeMS();
	TargetingData.Location = InLocation;
	TargetingData.Direction = InDirection;
	TargetingData.Rotation = InRotation;
	TargetingData.SavedHitActors.Empty();
	

	if (!TargetingProcessor)
	{
		UE_LOG(LogAbilitySystemComponent,Error,TEXT("Targeting prediction Task (%s) inside (%s) Has no targeting processor")
			,*GetNameSafe(this),*GetNameSafe(GetOuter()))
		return;
	}
	ActivateTask();
	//Broadcast pre Targeting to allow for easy input data update
	OnPreTargeting.Broadcast(ETargetingEvent::EStart,this);
	
	UNpAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	bool DidRewind = false;
	if (NpManager && bEnableLagCompensation)
	{
		DidRewind = NpManager->RewindActors(AbilitySystemComponent->GetAvatarActor(),GetAbilitySystemComponent()->GetSyncedInterpolationTimeMS());
	}
	// if targeting processor StartTargeting returns true, we broadcast Target Found.
	// if end on first target found we just deactivate here
	FGameplayAbilityTargetDataHandle TargetDataHandle;
	const ETargetingResult Result = TargetingProcessor->StartTargeting(AbilitySystemComponent,TargetingData,TargetDataHandle);
	// we unwind actors before the rest of the code for safety, we don't know what can happen to this task or its owning ability
	// or what the user might do. so put everyone back where they are supposed to be then continue
	// can get actor state at time to further get lag compensation transform
	if (DidRewind)
	{
		NpManager->UnwindActors();
	}
	HandleTargetingResult(Result,TargetDataHandle);
}

void UTargetingPredictionTask::ConfirmTargeting()
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (!TargetingProcessor)
	{
		return;
	}
	if (!TargetingProcessor->bAllowMultipleConfirmations && DidConfirmationTrigger())
	{
		return;
	}
	//Broadcast pre Targeting to allow for easy input data update
	OnPreTargeting.Broadcast(ETargetingEvent::EConfirmation,this);
	
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	UNpAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	bool DidRewind = false;
	if (NpManager && bEnableLagCompensation)
	{
		DidRewind = NpManager->RewindActors(AbilitySystemComponent->GetAvatarActor(),AbilitySystemComponent->GetSyncedInterpolationTimeMS());
	}
	FGameplayAbilityTargetDataHandle TargetDataHandle;
	const ETargetingResult Result = TargetingProcessor->ConfirmTargeting(AbilitySystemComponent,GetCurrentTaskDurationMS()
		,GetTimeSinceLastConfirmMS()
		,TargetingData,TargetDataHandle);
	// we unwind actors before the rest of the code for safety, we don't know what can happen to this task or its owning ability
	// or what the user might do. so put everyone back where they are supposed to be then continue
	// can get actor state at time to further get lag compensation transform
	if (DidRewind)
	{
		NpManager->UnwindActors();
	}
	// set last confirmation time after confirming the targeting. this allows the processor to know time between confirmations
	// no point in passing in zero, since we explicitly let it know we just got confirmed by calling a specific function
	// processor children can override it and assume it's 0 if they don't need time between confirmations
	TargetingData.LastConfirmationTime = GetAbilitySystemComponent()->GetCurrentSimulationTimeMS();
	HandleTargetingResult(Result,TargetDataHandle);
}

void UTargetingPredictionTask::CancelTargeting()
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (!TargetingProcessor)
	{
		return;
	}
	//Broadcast pre Targeting to allow for easy input data update
	OnPreTargeting.Broadcast(ETargetingEvent::ECancelation,this);
	
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	UNpAbilitySystemComponent* AbilitySystemComponent = GetAbilitySystemComponent();
	bool DidRewind = false;
	if (NpManager && bEnableLagCompensation)
	{
		DidRewind = NpManager->RewindActors(AbilitySystemComponent->GetAvatarActor(),AbilitySystemComponent->GetSyncedInterpolationTimeMS());
	}
	FGameplayAbilityTargetDataHandle TargetDataHandle;
	const ETargetingResult Result = TargetingProcessor->CancelTargeting(AbilitySystemComponent,GetCurrentTaskDurationMS()
		,GetTimeSinceLastConfirmMS()
		,TargetingData,TargetDataHandle);
	// we unwind actors before the rest of the code for safety, we don't know what can happen to this task or its owning ability
	// or what the user might do. so put everyone back where they are supposed to be then continue
	// can get actor state at time to further get lag compensation transform
	if (DidRewind)
	{
		NpManager->UnwindActors();
	}
	// set last confirmation time after confirming the targeting. this allows the processor to know time between confirmations
	// no point in passing in zero, since we explicitly let it know we just got confirmed by calling a specific function
	// processor children can override it and assume it's 0 if they don't need time between confirmations
	TargetingData.LastConfirmationTime = GetAbilitySystemComponent()->GetCurrentSimulationTimeMS();
	HandleTargetingResult(Result,TargetDataHandle);
}

void UTargetingPredictionTask::HandleTargetingResult(const ETargetingResult& Result,
	const FGameplayAbilityTargetDataHandle& TargetHandle)
{
	switch (Result)
	{
	case ETargetingResult::EContinue:
		{
			break;
		}
	case ETargetingResult::EEnd:
		{
			OnEnded.Broadcast(false);
			DeactivateTask(false);
			break;
		}
	case ETargetingResult::EAbort:
		{
			OnEnded.Broadcast(true);
			DeactivateTask(true);
			break;
		}
		// successful targeting we end now
	case ETargetingResult::ESuccess:
		{
			OnConfirmedSuccess.Broadcast(TargetHandle);
			DeactivateTask(false);
			break;
		}
		// successful targeting but on going
	case ETargetingResult::ESuccessOnGoing:
		{
			OnOngoingSuccess.Broadcast(TargetHandle);
			break;
		}
	
	}
}

const TArray<AActor*>& UTargetingPredictionTask::GetSavedHitActors() const
{
	return TargetingData.SavedHitActors;
}

float UTargetingPredictionTask::GetCurrentTaskDurationMS() const
{
	return (GetAbilitySystemComponent()->GetCurrentSimulationTimeMS() - TargetingData.StartSimTime);
}

float UTargetingPredictionTask::GetTimeSinceLastConfirmMS() const
{
	if (TargetingData.LastConfirmationTime == 0)
	{
		return -1.f;
	}
	return (GetAbilitySystemComponent()->GetCurrentSimulationTimeMS() - TargetingData.LastConfirmationTime);
}

bool UTargetingPredictionTask::DidConfirmationTrigger() const
{
	return TargetingData.LastConfirmationTime > 0;
}

float UTargetingPredictionTask::GetProcessorMaxDurationMS() const
{
	if (!TargetingProcessor)
	{
		return -1.f;
	}
	return FMath::RoundToInt32(TargetingProcessor->MaxDuration * 1000.f);
}

void UTargetingPredictionTask::UpdateLocation(FVector NewLocation)
{
	TargetingData.Location = NewLocation;
}

void UTargetingPredictionTask::UpdateRotation(FRotator NewRotation)
{
	TargetingData.Rotation = NewRotation;
}

/* Prediction Task API */

void UTargetingPredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	Super::OnSimulationTick(TimeStep);
	
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	UNpAbilitySystemComponent* Asc = GetAbilitySystemComponent();
	if (!TargetingProcessor || !Asc)
	{
		return;
	}
	UE_LOG(LogTemp,Error,TEXT("UBaseTargetingPredictionTask::ExecuteTask , SimTime %f , CurrentSimTime %f")
		,TimeStep.BaseSimTimeMs,GetAbilitySystemComponent()->GetCurrentSimulationTimeMS());
	// this is our first tick, we just called Start Targeting we shouldn't perform targeting this frame
	// or if we just called confirm targeting, don't do it again this frame
	if (TargetingData.StartSimTime == TimeStep.BaseSimTimeMs || TargetingData.LastConfirmationTime == TimeStep.BaseSimTimeMs)
	{
		return;
	}
	const float DurationMS = GetCurrentTaskDurationMS();\
	const float TimeSinceConfirm = GetTimeSinceLastConfirmMS();
	if (TargetingProcessor->HasReachedMaxDuration(DurationMS,TimeSinceConfirm))
	{
		if (TargetingProcessor->ConfirmOnMaxDuration)
		{
			ConfirmTargeting();
		}
		else
		{
			CancelTargeting();
		}
		return;
	}
	// if processor doesn't want to tick early out
	if (!TargetingProcessor->ShouldExecute(TimeStep,GetCurrentTaskDurationMS(),GetTimeSinceLastConfirmMS()))
	{
		return;
	}
	//Broadcast pre Targeting to allow for easy input data update
	OnPreTargeting.Broadcast(ETargetingEvent::EExecution,this);
	
	UNetworkPredictionWorldManager* NpManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	bool DidRewind = false;
	if (NpManager && bEnableLagCompensation)
	{
		DidRewind = NpManager->RewindActors(Asc->GetAvatarActor(),Asc->GetSyncedInterpolationTimeMS());
	}
	FGameplayAbilityTargetDataHandle TargetDataHandle;
	const ETargetingResult Result = TargetingProcessor->ExecuteTargeting(Asc,TimeStep,GetCurrentTaskDurationMS()
		,GetTimeSinceLastConfirmMS(),TargetingData,TargetDataHandle);
	// we unwind actors before the rest of the code for safety, we don't know what can happen to this task or its owning ability
	// or what the user might do. so put everyone back where they are supposed to be then continue
	// can use get actor state at time for Np Manager after this to further get lag compensation transform
	if (DidRewind)
	{
		NpManager->UnwindActors();
	}
	HandleTargetingResult(Result,TargetDataHandle);
}

void UTargetingPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FBaseTargetingTaskData* SyncedData = static_cast<const FBaseTargetingTaskData*>(DataToRead.Get());
	TargetingData.Location = SyncedData->Location;
	TargetingData.Direction = SyncedData->Direction;
	TargetingData.Rotation = SyncedData->Rotation;
	TargetingData.SavedHitActors = SyncedData->SavedHitActors;
	TargetingData.StartSimTime = SyncedData->StartSimTime;
	TargetingData.LastConfirmationTime = SyncedData->LastConfirmationTime;
}

void UTargetingPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FBaseTargetingTaskData* SyncedData = static_cast<FBaseTargetingTaskData*>(DataToWrite.Get());
	SyncedData->Location = TargetingData.Location;
	SyncedData->Direction = TargetingData.Direction;
	SyncedData->Rotation = TargetingData.Rotation;
	SyncedData->SavedHitActors = TargetingData.SavedHitActors;
	SyncedData->StartSimTime = TargetingData.StartSimTime;
	SyncedData->LastConfirmationTime = TargetingData.LastConfirmationTime;
}

FText UTargetingPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("BaseTargetingPredictionTask", "Base Targeting Prediction Task");
}

FLinearColor UTargetingPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.702f, 0.0f, 0.0f, 1.0f);
}
#pragma endregion



#undef LOCTEXT_NAMESPACE