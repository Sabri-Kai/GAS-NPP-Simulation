// Fill out your copyright notice in the Description page of Project Settings.


#include "MontageSimulator/NetMontageSimulator.h"

#include "MoverComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "Engine/SkeletalMeshSocket.h"
#include "MontageSimulator/SyncedNotifyInterface.h"


UNetMontageSimulator::UNetMontageSimulator(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	
}

void UNetMontageSimulator::SimulationTick(const FAbilitySystemTimeStep& TimeStep,
	const FMontageSimSyncState& InputState, FMontageSimSyncState& OutputState)
{
	LatestTimeStep = TimeStep;
	
	//OutputState = SimulationState;

	// now step the montage forward if we have one
	if (SimulationState.GetPlayingMontage())
	{
		const float DeltaTime = TimeStep.StepMs / 1000.f;
		const float NexTime = FMath::Clamp(SimulationState.GetCurrentTime() + DeltaTime,
										   0.f,SimulationState.GetPlayingMontage()->GetPlayLength());
		const float MontageStepTime = (NexTime - SimulationState.GetCurrentTime());
		constexpr float MinStepTime = 0.001f; // max step time is 1 MS
		if (MontageStepTime > MinStepTime)
		{
			// advance montage
			AdvanceSimMontage(MontageStepTime,TimeStep, SimulationState);
		}
		// step time is too small assume montage ended
		else
		{
			CompleteSimMontage(TimeStep,SimulationState);
		}
	}
	OutputState = SimulationState;
}

void UNetMontageSimulator::RestoreFrame(const FMontageSimSyncState& AuthorityState)
{
	// First Check If Authority wants to play montage
	if(AuthorityState.GetPlayingMontage())
	{
		// check if we are currently playing a montage
		if(SimulationState.GetPlayingMontage())
		{
			// is it the same montage?
			if (AuthorityState.GetPlayingMontage() == SimulationState.GetPlayingMontage())
			{
				RestoreMontage(SimulationState,AuthorityState);
				SimulationState = AuthorityState;
				return;
			}
			// Not Same Montage
			{
				ForceEndMontage(SimulationState);
				ForceStartMontage(AuthorityState);
			}
			SimulationState = AuthorityState;
			return;
		}
		// we are currently not playing a montage
		{
			ForceStartMontage(AuthorityState);
			SimulationState = AuthorityState;
			return;
		}
	}
	// Authority State is not playing a montage
	if (SimulationState.GetPlayingMontage())
	{
		ForceEndMontage(SimulationState);
	}

	SimulationState = AuthorityState;
}

void UNetMontageSimulator::FinalizeFrame(const FMontageSimSyncState& SyncState)
{
	FinalizeMontageFrame(SyncState);
}

UNpAbilitySystemComponent* UNetMontageSimulator::GetNpAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

AActor* UNetMontageSimulator::GetAvatarActor() const
{
	return AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
}

UAnimInstance* UNetMontageSimulator::GetAnimInstance() const
{
	if (!AbilitySystemComponent || !AbilitySystemComponent->AbilityActorInfo)
	{
		return nullptr;
	}
	return AbilitySystemComponent->AbilityActorInfo->GetAnimInstance();
}

USkeletalMeshComponent* UNetMontageSimulator::GetMeshComponent() const
{
	UAnimInstance* AnimInst = GetAnimInstance();
	return AnimInst ? AnimInst->GetOwningComponent() : nullptr;
}

float UNetMontageSimulator::GetMontageBlendOutTime(const UAnimMontage* Montage)
{
	if (!Montage)
	{
		return 0.0f;
	}
	const bool bCustomBlendOutTriggerTime = (Montage->BlendOutTriggerTime >= 0);
	const float DefaultBlendOutTime = Montage->BlendOut.GetBlendTime();
	const float BlendOutTriggerTime = bCustomBlendOutTriggerTime ? Montage->BlendOutTriggerTime : DefaultBlendOutTime;
	return Montage->GetPlayLength() - BlendOutTriggerTime;
}

bool UNetMontageSimulator::ShouldFinalizeMontage(const FMontageSimSyncState& SyncState)
{
	if (!SyncState.Montage)
	{
		return false;
	}

	if (SyncState.GetCurrentTime() > GetMontageBlendOutTime(SyncState.Montage))
	{
		return false;
	}
	return true;
}

void UNetMontageSimulator::PlayMontage(const FAbilityMontagePlayback& MontagePlayback)
{
	if (!GetNpAbilitySystemComponent() || GetNpAbilitySystemComponent()->GetIsRestoringFrame() || !MontagePlayback.MontageToPlay)
	{
		return;
	}
	FMontageSimSyncState OldState = SimulationState;
	SimulationState.SetIsPaused(false);
	if (MontagePlayback.IsValidQueue())
	{
		// check if we have a currently playing montage,
		if (SimulationState.GetPlayingMontage())
		{
			// if we do , check if it's same montage
			if (SimulationState.GetPlayingMontage() == MontagePlayback.MontageToPlay)
			{
				//if same montage, check if queued time is negative,
				// this will happen when we call montage set play rate
				if (!FMath::IsNearlyEqual(SimulationState.GetCurrentTime() ,MontagePlayback.MontageStartTime))
				{
					// we have valid time , this would happen when we want to jump montage section for example
					//We Are Changing current montage time, Notifies will trigger correctly in the simulation
					SimulationState.SetCurrentTime(MontagePlayback.MontageStartTime);
				}
				// time is negative , if play rate is not negative set it
				if (MontagePlayback.MontagePlayRate >= 0)
				{
					SimulationState.SetPlayRate(MontagePlayback.MontagePlayRate);
				}
			}
			// it's a different montage, stop current one and start new one
			else
			{
				CancelSimMontage(LatestTimeStep,OldState,true);
				SimulationState.Reset();
				StartSimMontage(MontagePlayback, SimulationState);
			}
		}
		// we don't have a currently playing montage, just start new one
		else
		{
			StartSimMontage(MontagePlayback, SimulationState);
		}
	}
}

void UNetMontageSimulator::StopMontage(const FAbilityMontageCancel& MontageCancel, bool bInterrupted/* = false*/)
{
	if (!GetNpAbilitySystemComponent() || GetNpAbilitySystemComponent()->GetIsRestoringFrame() || !MontageCancel.MontageToCancel)
	{
		return;
	}
	SimulationState.SetIsPaused(false);
	if (MontageCancel.IsValidQueue())
	{
		if (SimulationState.GetPlayingMontage() == MontageCancel.MontageToCancel)
		{
			CancelSimMontage(LatestTimeStep,SimulationState,bInterrupted);
		}
	}
}

void UNetMontageSimulator::MontageJumpToSection(UAnimMontage* Montage, FName SectionName)
{
	if (!GetNpAbilitySystemComponent() || GetNpAbilitySystemComponent()->GetIsRestoringFrame() || !Montage)
	{
		return;
	}
	float CurrentTime = 0.f;
	UAnimMontage* PlayingMontage = GetPlayingMontage(CurrentTime);
	if (PlayingMontage && Montage == PlayingMontage)
	{
		const int32 SectionIndex = PlayingMontage->GetSectionIndex(SectionName);
		if (SectionIndex != INDEX_NONE)
		{
			float SectionStartTime = 0.f;
			float SectionEndTime = 0.f;
			PlayingMontage->GetSectionStartAndEndTime(SectionIndex,SectionStartTime,SectionEndTime);
			SimulationState.SetCurrentTime(SectionStartTime);
		}
	}
}

void UNetMontageSimulator::PauseCurrentMontage()
{
	SimulationState.SetIsPaused(true);
}

void UNetMontageSimulator::ResumeCurrentMontage()
{
	SimulationState.SetIsPaused(false);
}

void UNetMontageSimulator::SetMontagePlayRate(const float& PlayRate)
{
	SimulationState.SetPlayRate(PlayRate);
}

UAnimMontage* UNetMontageSimulator::GetPlayingMontage(float& OutCurrentTime) const
{
	OutCurrentTime = SimulationState.GetCurrentTime();
	return SimulationState.GetPlayingMontage();
}

FTransform UNetMontageSimulator::GetAttachedSocketWorldTransformFromMontage(UAnimMontage* Montage, float MontageTime,
	FTransform MeshRelativeTransform, FTransform ActorTransform, const USkeletalMeshComponent* MeshComp,
	FName AttachementSocket, const USkeletalMeshComponent* AttachedMeshComponent, FName AttachedMeshSocket)
{
	if (!MeshComp || !AttachedMeshComponent)
		return FTransform::Identity;

	// --- 1. Get attachment socket world transform on parent mesh ---
	FTransform AttachmentWorld = GetSocketWorldTransformFromMontage(
		Montage, MontageTime, MeshRelativeTransform, ActorTransform, MeshComp, AttachementSocket);

	// --- 2. Get attached mesh socket bone hierarchy ---
	FName SocketBoneName = AttachedMeshSocket;
	if (const USkeletalMeshSocket* Socket = AttachedMeshComponent->GetSocketByName(AttachedMeshSocket))
	{
		SocketBoneName = AttachedMeshComponent->GetSocketBoneName(AttachedMeshSocket);
	}

	const USkeleton* AttachedMeshSkeleton = AttachedMeshComponent->GetSkeletalMeshAsset()->GetSkeleton();
	if (!AttachedMeshSkeleton)
		return AttachmentWorld;

	const FReferenceSkeleton& RefSkeleton = AttachedMeshSkeleton->GetReferenceSkeleton();
	int32 BoneIndex = RefSkeleton.FindRawBoneIndex(SocketBoneName);
	if (BoneIndex == INDEX_NONE)
		return AttachmentWorld;

	// --- 3. Walk from socket bone to root to calculate component-space transform ---
	TArray<FTransform> Bones;
	while (BoneIndex != INDEX_NONE)
	{
		FTransform BoneLocal = AttachedMeshComponent->GetBoneTransform(BoneIndex);
		Bones.Add(BoneLocal);
		BoneIndex = RefSkeleton.GetRawParentIndex(BoneIndex);
	}

	// Multiply up the chain from root to socket
	FTransform SocketCompSpace = FTransform::Identity;
	for (int32 i = Bones.Num() - 1; i >= 0; --i)
	{
		SocketCompSpace = Bones[i] * SocketCompSpace;
	}

	// --- 4. Apply socket local offset ---
	if (const USkeletalMeshSocket* Socket = AttachedMeshComponent->GetSocketByName(AttachedMeshSocket))
	{
		SocketCompSpace = Socket->GetSocketLocalTransform() * SocketCompSpace;
	}

	// --- 5. Multiply by parent attachment socket world transform ---
	FTransform FinalWorld = SocketCompSpace * AttachmentWorld;
	return FinalWorld;
}

FTransform UNetMontageSimulator::GetSocketWorldTransformFromMontage(UAnimMontage* Montage, float MontageTime,
                                                                    FTransform MeshRelativeTransform, FTransform ActorTransform ,const USkeletalMeshComponent* MeshComp, FName SocketName)
{
	 if (!Montage || !Montage->GetSkeleton() || !MeshComp)
    {
        return FTransform::Identity;
    }

    const USkeleton* Skeleton = Montage->GetSkeleton();
    FName TargetBoneName = SocketName;
	const USkeletalMeshSocket* Socket = MeshComp->GetSocketByName(SocketName);
    // If it's a socket, use its bone
    if (Socket)
    {
        TargetBoneName = MeshComp->GetSocketBoneName(SocketName);
    }

    // Find the current segment in the first slot track
    const FAnimTrack& AnimTrack = Montage->SlotAnimTracks[0].AnimTrack;
    if (const FAnimSegment* Segment = AnimTrack.GetSegmentAtTime(MontageTime))
    {
        if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Segment->GetAnimReference()))
        {
            // Build local transforms up to root
            TArray<FTransform> LocalChain;
            int32 BoneIndex = Skeleton->GetReferenceSkeleton().FindRawBoneIndex(TargetBoneName);
            while (BoneIndex != INDEX_NONE)
            {
                FSkeletonPoseBoneIndex SkeletonBoneIndex(BoneIndex);
                FTransform BoneLocal = FTransform::Identity;
                Sequence->GetBoneTransform(BoneLocal, SkeletonBoneIndex, MontageTime, false);
                LocalChain.Add(BoneLocal);

                BoneIndex = Skeleton->GetReferenceSkeleton().GetRawParentIndex(BoneIndex);
            }

            // Multiply from root to target to get component-space
            FTransform CompSpace = FTransform::Identity;
            for (int32 i = LocalChain.Num() - 1; i >= 0; --i)
            {
                CompSpace = LocalChain[i] * CompSpace;
            }

            // If socket, apply socket relative transform
            if (Socket)
            {
                CompSpace = Socket->GetSocketLocalTransform() * CompSpace;
            }

            // Apply mesh + actor transforms to get world space
            return CompSpace * MeshRelativeTransform * ActorTransform;
        }
    }

    return FTransform::Identity;
}

void UNetMontageSimulator::StartSimMontage(const FAbilityMontagePlayback& Montage, FMontageSimSyncState& SyncState) const
{
	SyncState.Montage = Montage.MontageToPlay;
	SyncState.SetCurrentTime(Montage.MontageStartTime);
	SyncState.SetPlayRate(Montage.MontagePlayRate);
	SyncState.SetRootMotionScale(Montage.RootMotionScale);
	// Setup The Synced Notifies
	const FSyncedNotifiesArray AnimNotifies(SyncState.Montage);
	SyncState.NotifySyncStates.ActiveNotifySyncStates.Reset(AnimNotifies.Num());
	for (int32 i = 0; i < AnimNotifies.Num(); ++i)
	{
		const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
		TObjectPtr<UObject> NotifyObject = SyncState.NotifySyncStates.GetNotifyObject(NotifyEvent);
		check(NotifyObject->Implements<USyncedNotifyInterface>());
		UScriptStruct* Type = ISyncedNotifyInterface::Execute_GetRequiredType(NotifyObject);
		SyncState.NotifySyncStates.AddNotifySyncState(Type,i);
	}
	AbilitySystemComponent->OnMontageStarted.Broadcast(SyncState.Montage);
}

void UNetMontageSimulator::AdvanceSimMontage(const float& StepTime, const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& SyncState)
{
	check(SyncState.Montage);
	FRootMotionMovementParams RootMotionMovementParams;
	const float PreviousTime = SyncState.GetCurrentTime();
	float NewPosition = SyncState.GetCurrentTime();
	float RateMultiplier = 1.f;
	UNpAbilitySystemComponent* Asc = GetNpAbilitySystemComponent();
	if (Asc)
	{
		RateMultiplier = Asc->GetMontagePlayRateMultiplier();
	}
	MontageAdvanceInstance.Initialize(SyncState.GetPlayingMontage());
	MontageAdvanceInstance.SetPosition(SyncState.GetCurrentTime());
	MontageAdvanceInstance.SetPlayRate(SyncState.GetPlayRate() * RateMultiplier);
	MontageAdvanceInstance.SetPlaying(true);
	if (SyncState.GetIsPaused())
	{
		MontageAdvanceInstance.SetPlayRate(0.f);
	}
	MontageAdvanceInstance.bEnableAutoBlendOut = false;
	MontageAdvanceInstance.SimulateAdvance(StepTime,NewPosition,RootMotionMovementParams);
	SyncState.SetCurrentTime(NewPosition);
	
	RootMotionMovementParams.ScaleRootMotionTranslation(SyncState.GetRootMotionScale());
	FTransform RootMotionTransform = RootMotionMovementParams.GetRootMotionTransform();
	TickSyncedNotifies(StepTime,TimeStep,PreviousTime,SyncState,RootMotionTransform);
	if (!SyncState.GetPlayingMontage())
	{
		return;
	}
	if (MontageAdvanceInstance.Montage->HasRootMotion() && SyncState.GetCurrentTime() < GetMontageBlendOutTime(MontageAdvanceInstance.Montage))
	{
		//ToDo @Kai : Remove This GetComponentByClass Every Sim Frame cache mover component like cmc used to be.
		if (AbilitySystemComponent->GetAvatarActor())
		{
			if(UMoverComponent* MoverComp = AbilitySystemComponent->GetAvatarActor()->GetComponentByClass<UMoverComponent>())
			{
				FSyncedMontageRootMotion RootMotionToConsume;
				RootMotionToConsume.Montage = MontageAdvanceInstance.Montage;
				RootMotionToConsume.CurrentTime = SyncState.GetCurrentTime();
				RootMotionToConsume.HasRootMotion = true;
				RootMotionToConsume.RootMotionTranslation = RootMotionTransform.GetTranslation();
				RootMotionToConsume.RootMotionRotation = RootMotionTransform.Rotator();
				// if montage is paused make sure to override any root motion to 0
				if (SyncState.GetIsPaused())
				{
					RootMotionToConsume.RootMotionTranslation = FVector::ZeroVector;
					RootMotionToConsume.RootMotionRotation = FRotator::ZeroRotator;
				}
				FLayeredMove_SyncMontageRootMotion RootMotionMove = FLayeredMove_SyncMontageRootMotion(RootMotionToConsume);
				MoverComp->QueueLayeredMove(MakeShared<FLayeredMove_SyncMontageRootMotion>(RootMotionMove));
			}
		}
	}
	// call montage blend out if previous time was before it and new time is after it.
	if (SyncState.GetCurrentTime() >= GetMontageBlendOutTime(SyncState.GetPlayingMontage()) && PreviousTime < GetMontageBlendOutTime(SyncState.GetPlayingMontage()))
	{
		AbilitySystemComponent->OnMontageBlendOut.Broadcast(SyncState.GetPlayingMontage());
	}
	
}

void UNetMontageSimulator::CompleteSimMontage(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& SyncState)
{
	AbilitySystemComponent->OnMontageCompleted.Broadcast(SyncState.GetPlayingMontage());
	EndSyncedNotifies(TimeStep,SyncState);
	SyncState.Reset();
}

void UNetMontageSimulator::CancelSimMontage(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& SyncState, const bool& InterruptedByAnother)
{
	// if we have no montage
	if (!SyncState.GetPlayingMontage())
	{
		SyncState.Reset();
		return;
	}
	// montage technically ended (we keep ticking it until end in simulation for synced notifies)
	// so assume it is completed
	if (SyncState.GetCurrentTime() >= GetMontageBlendOutTime(SyncState.GetPlayingMontage()))
	{
		EndSyncedNotifies(TimeStep,SyncState);
		AbilitySystemComponent->OnMontageCompleted.Broadcast(SyncState.GetPlayingMontage());
		SyncState.Reset();
		return;
	}
	EndSyncedNotifies(TimeStep,SyncState);
	if (InterruptedByAnother)
	{
		AbilitySystemComponent->OnMontageInterrupted.Broadcast(SyncState.GetPlayingMontage());
	}
	AbilitySystemComponent->OnMontageCanceled.Broadcast(SyncState.GetPlayingMontage());
	// call on blend out for this montage
	AbilitySystemComponent->OnMontageBlendOut.Broadcast(SyncState.GetPlayingMontage());
	SyncState.Reset();
}

void UNetMontageSimulator::RestoreMontage(const FMontageSimSyncState& CurrentSyncState,
	const FMontageSimSyncState& AuthoritySyncState)
{
	const FSyncedNotifiesArray AnimNotifies(AuthoritySyncState.GetPlayingMontage());
	FRestoreNotifyData InputData;
	InputData.AnimMontage = AuthoritySyncState.GetPlayingMontage();
	InputData.MontagePlayer = this;
	// make sure current time doesn't exceed montage time
	InputData.CurrentMontageTime = FMath::Min(AuthoritySyncState.GetCurrentTime(), AuthoritySyncState.GetPlayingMontage()->GetPlayLength());

	for (int32 i = 0; i < AnimNotifies.Num(); ++i)
	{
		// can only restore notify states because they have duration
		const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
		TObjectPtr<UObject> NotifyObject = AuthoritySyncState.NotifySyncStates.GetNotifyObject(NotifyEvent);
		check(NotifyObject->Implements<USyncedNotifyInterface>());// we should never have invalid interface at this point
		if (AuthoritySyncState.NotifySyncStates.IsNotifyState(NotifyEvent))
		{
			InputData.NotifyStartTime = NotifyEvent.GetTriggerTime();
			InputData.NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			FSyncedNotifyDataContainer AuthorityContainer = AuthoritySyncState.NotifySyncStates[i];
			FSyncedNotifyDataContainer LocalContainer = CurrentSyncState.NotifySyncStates[i];
			// if authority is not active , it won't send us any data to rollback to.
			// there isn't any. so if this has data, just provide default data
			if (!AuthorityContainer.IsActive && LocalContainer.SyncStatePointer.IsValid())
			{
				AuthorityContainer.SyncStatePointer = FSyncedNotifyDataArray::CreateDataByType(LocalContainer.SyncStatePointer->GetScriptStruct());
			}
			
			ISyncedNotifyInterface::Execute_RestoreFrame(NotifyObject,InputData,AuthorityContainer,LocalContainer, this);
		}
	}
}

void UNetMontageSimulator::ForceEndMontage(const FMontageSimSyncState& CurrentSyncState)
{
	const FSyncedNotifiesArray AnimNotifies(CurrentSyncState.GetPlayingMontage());
	FRestoreNotifyData InputData;
	InputData.AnimMontage = CurrentSyncState.GetPlayingMontage();
	InputData.MontagePlayer = this;
	// make sure current time doesn't exceed montage time
	InputData.CurrentMontageTime = FMath::Min(CurrentSyncState.GetCurrentTime(), CurrentSyncState.GetPlayingMontage()->GetPlayLength());
	for (int32 i = 0; i < AnimNotifies.Num(); ++i)
	{
		// can only restore notify states because they have duration
		const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
		TObjectPtr<UObject> NotifyObject = CurrentSyncState.NotifySyncStates.GetNotifyObject(NotifyEvent);
		check(NotifyObject->Implements<USyncedNotifyInterface>());// we should never have invalid interface at this point
		if (CurrentSyncState.NotifySyncStates.IsNotifyState(NotifyEvent))
		{
			InputData.NotifyStartTime = NotifyEvent.GetTriggerTime();
			InputData.NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			FSyncedNotifyDataContainer LocalContainer = CurrentSyncState.NotifySyncStates[i];
			// montage is force ended, there's no authority data for it, Set is active to false and copy the current data
			FSyncedNotifyDataContainer AuthorityContainer = LocalContainer;
			AuthorityContainer.IsActive = false;
			ISyncedNotifyInterface::Execute_RestoreFrame(NotifyObject,InputData,AuthorityContainer,LocalContainer, this);
		}
	}
}

void UNetMontageSimulator::ForceStartMontage(const FMontageSimSyncState& AuthoritySyncState)
{
	const FSyncedNotifiesArray AnimNotifies(AuthoritySyncState.GetPlayingMontage());
	FRestoreNotifyData InputData;
	InputData.AnimMontage = AuthoritySyncState.GetPlayingMontage();
	InputData.MontagePlayer = this;
	// make sure current time doesn't exceed montage time
	InputData.CurrentMontageTime = FMath::Min(AuthoritySyncState.GetCurrentTime(), AuthoritySyncState.GetPlayingMontage()->GetPlayLength());
	for (int32 i = 0; i < AnimNotifies.Num(); ++i)
	{
		// can only restore notify states because they have duration
		const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
		TObjectPtr<UObject> NotifyObject = AuthoritySyncState.NotifySyncStates.GetNotifyObject(NotifyEvent);
		check(NotifyObject->Implements<USyncedNotifyInterface>());// we should never have invalid interface at this point
		if (AuthoritySyncState.NotifySyncStates.IsNotifyState(NotifyEvent))
		{
			InputData.NotifyStartTime = NotifyEvent.GetTriggerTime();
			InputData.NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			FSyncedNotifyDataContainer AuthorityContainer = AuthoritySyncState.NotifySyncStates[i];
			// montage is force ended, there's no authority data for it, Set is active to false and copy the current data
			FSyncedNotifyDataContainer LocalContainer = AuthorityContainer;
			LocalContainer.IsActive = false;
			ISyncedNotifyInterface::Execute_RestoreFrame(NotifyObject,InputData,AuthorityContainer,LocalContainer, this);
		}
	}
}

void UNetMontageSimulator::TickSyncedNotifies(const float& MontageStepTime,const FAbilitySystemTimeStep& TimeStep,const float& PreviousTime,
                                              FMontageSimSyncState& SyncOutput,OUT FTransform& ExtractedRootMotion)
{
	// Shared Input Data between All Active Notify States
	static FSimTickNotifyData InputData;
	InputData.AnimMontage = SyncOutput.GetPlayingMontage();
	InputData.MontagePlayer = this;
	// since notifies tick after advancing the montage , let's try using previous time for them if it's less than current time
	// previous time would be higher than current only in a loop situation
	
	// ToDo : Setting the time notifies will start ticking from (time before the animation pose was advance ei previous time)
	// to current time , in the case of loop is a naive implementation, in truth we should keep tack of the section we looped on
	// and do sub-tick for the : (Section end - pre-advance time) and (Post-Advance time - Section Start)
	// handled outside in AdvanceMontage so this function always gets a proper previous and current times.
	if (PreviousTime < SyncOutput.GetCurrentTime())
	{
		InputData.NotifyTickStartTime = PreviousTime;
	}
	else
	{
		InputData.NotifyTickStartTime = SyncOutput.GetCurrentTime();
	}
	InputData.CurrentSimTimeMS = TimeStep.BaseSimTimeMs;
	const FSyncedNotifiesArray AnimNotifies(SyncOutput.GetPlayingMontage());
	for (int32 i = 0; i < AnimNotifies.Num(); ++i)
	{
		// Notifies Can End the Montage And Make the current playing montage null, break if it happens
		if (!SyncOutput.GetPlayingMontage())
		{
			break;
		}
		const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
		TObjectPtr<UObject> NotifyObject = SyncOutput.NotifySyncStates.GetNotifyObject(NotifyEvent);
		check(NotifyObject->Implements<USyncedNotifyInterface>());// we should never have invalid interface at this point
		InputData.DeltaSeconds = MontageStepTime;
		// make sure advancing with delta time does not exceed the notify time
		if (SyncOutput.NotifySyncStates.IsNotifyState(NotifyEvent))
		{
			// by clamping the time we also clamp the delta time.
			const float TimeAfterAdvance = FMath::Clamp(InputData.NotifyTickStartTime + InputData.DeltaSeconds,
				NotifyEvent.GetTriggerTime(), NotifyEvent.GetEndTriggerTime());
			InputData.DeltaSeconds = TimeAfterAdvance - InputData.NotifyTickStartTime;
		}
		InputData.NotifyStartTime = NotifyEvent.GetTriggerTime();
		InputData.NotifyEndTime = NotifyEvent.GetEndTriggerTime();
		InputData.RootMotionTransform = ExtractedRootMotion;
		// Set the tick input data for this specific notify
		//Output
		static FSimTickNotifyEndData OutputData;
		OutputData.SharedNotifyDataState = nullptr;
		OutputData.ExtractionData = ExtractedRootMotion;
		if (SyncOutput.NotifySyncStates.IsValidStateIndex(i))
		{
			if (const FSyncedNotifyDataContainer& SyncState = SyncOutput.NotifySyncStates[i];
				SyncState.SyncStatePointer.IsValid())
			{
				// create new data blocks here and copy it, don't give the simulation data as it for safety.
				InputData.SharedNotifyDataState = SyncState.SyncStatePointer->CloneShared();
				OutputData.SharedNotifyDataState = SyncState.SyncStatePointer->CloneShared();
			}
		}
		if (SyncOutput.NotifySyncStates.ShouldNotifyTriggerThisFrame(SyncOutput.GetPlayingMontage(),i, SyncOutput.GetCurrentTime(), InputData.NotifyTickStartTime))
		{
			//Set Triggered Flag To false
			FSyncedNotifyDataContainer& SyncState = SyncOutput.NotifySyncStates[i];
			SyncState.IsActive = true;
			SyncOutput.NotifySyncStates.SetupDataForNotifyAtIndex(SyncOutput.GetPlayingMontage(),i);
			OutputData.SharedNotifyDataState = nullptr;
			OutputData.ExtractionData = ExtractedRootMotion;
			if (SyncState.SyncStatePointer.IsValid())
			{
				InputData.SharedNotifyDataState = SyncState.SyncStatePointer->CloneShared();
				OutputData.SharedNotifyDataState = SyncState.SyncStatePointer->CloneShared();
			}
			// set trigger to true in input and output data for this notify tick
			ISyncedNotifyInterface::Execute_SimulationBegin(NotifyObject, TimeStep.bIsResimulating, InputData, OutputData);

			// if notify event causes montage to end, then break the loop
			if (!SyncOutput.GetPlayingMontage())
			{
				break;
			}
		}
		// Triggered This frame , So Called triggered Then Tick , we still want to tick on first frame
		if (SyncOutput.NotifySyncStates.ShouldNotifyTickThisFrame(SyncOutput.GetPlayingMontage(),i, InputData.NotifyTickStartTime)
			&& ISyncedNotifyInterface::Execute_CanTick(NotifyObject))
		{
			//ToDo @Kai : Bring The Cycle Counter Back
			//SCOPE_CYCLE_COUNTER(STAT_NetMontagePlayer_Notifies_Tick);
			// clamp delta time so it is max to end of notify
			const int32 MaxTickDeltaMS = FMath::RoundToInt32(NotifyEvent.GetEndTriggerTime() * 1000.f) -  FMath::RoundToInt32(InputData.NotifyTickStartTime * 1000.f);
			const int32 DeltaSecondsMS = FMath::RoundToInt32(InputData.DeltaSeconds * 1000.f);
			InputData.DeltaSeconds = FMath::Clamp(DeltaSecondsMS, 0.f, MaxTickDeltaMS) / 1000.f;
			// call the interface
			ISyncedNotifyInterface::Execute_SimulationTick(NotifyObject, TimeStep, InputData, OutputData);

			// if notify event causes montage to end, then break the loop
			if (!SyncOutput.GetPlayingMontage())
			{
				break;
			}
		}

		// Triggered This frame , So Called triggered Then Tick , we still want to tick on first frame
		if (SyncOutput.NotifySyncStates.ShouldNotifyEndThisFrame(SyncOutput.GetPlayingMontage(),i, SyncOutput.GetCurrentTime()))
		{
			//SCOPE_CYCLE_COUNTER(STAT_NetMontagePlayer_Notifies_End);
			//Set Triggered Flag To True
			SyncOutput.NotifySyncStates[i].IsActive = false;
			// clamp delta time so it is max to end of notify
			const float MaxTickDelta = NotifyEvent.GetEndTriggerTime() - InputData.NotifyTickStartTime;
			InputData.DeltaSeconds = FMath::Clamp(InputData.DeltaSeconds, 0.f, MaxTickDelta);
			ISyncedNotifyInterface::Execute_SimulationEnd(NotifyObject, TimeStep.bIsResimulating, InputData, OutputData);

			// if notify event causes montage to end, then break the loop
			if (!SyncOutput.GetPlayingMontage())
			{
				break;
			}
			SyncOutput.NotifySyncStates[i].SyncStatePointer = nullptr;
		}
		
		//copy Root Motion From Notify State
		ExtractedRootMotion = OutputData.ExtractionData;

		if (SyncOutput.NotifySyncStates.IsValidStateIndex(i)
			&& SyncOutput.NotifySyncStates[i].SyncStatePointer.IsValid()
			&& OutputData.SharedNotifyDataState.IsValid())
		{
			SyncOutput.NotifySyncStates[i].SyncStatePointer = OutputData.SharedNotifyDataState->CloneShared();
		}
	}
}

void UNetMontageSimulator::EndSyncedNotifies(const FAbilitySystemTimeStep& TimeStep,FMontageSimSyncState& State)
{
	if (State.GetPlayingMontage())
	{
		const float DeltaSeconds = TimeStep.StepMs / 1000.f;
		// Shared Input Data between All Active Notify States
		static FSimTickNotifyData InputData;
		InputData.AnimMontage = State.GetPlayingMontage();
		InputData.MontagePlayer = this;
		// make sure current time doesn't exceed montage time
		InputData.NotifyTickStartTime = FMath::Min(State.GetCurrentTime(), State.GetPlayingMontage()->GetPlayLength());
		InputData.DeltaSeconds = DeltaSeconds;
		InputData.CurrentSimTimeMS = TimeStep.BaseSimTimeMs;
		const FSyncedNotifiesArray AnimNotifies(State.GetPlayingMontage());

		checkf(AnimNotifies.Num() == State.NotifySyncStates.Num(),TEXT("Montage %s Notifies Num %d but SyncState Notifies Num %d ??"),*GetNameSafe(State.GetPlayingMontage()),AnimNotifies.Num(),State.NotifySyncStates.Num())
		for (int32 i = 0; i < AnimNotifies.Num(); i++)
		{
			const FAnimNotifyEvent& NotifyEvent = AnimNotifies[i];
			InputData.NotifyStartTime = NotifyEvent.GetTriggerTime();
			InputData.NotifyEndTime = NotifyEvent.GetEndTriggerTime();
			//Output
			static FSimTickNotifyEndData OutputData;
			OutputData.SharedNotifyDataState = nullptr;
			OutputData.ExtractionData = FTransform::Identity;
			// Set the tick data for this specific notify
			if (State.NotifySyncStates.IsValidStateIndex(i)
			   && State.NotifySyncStates[i].SyncStatePointer.IsValid())
			{
				const FSyncedNotifyDataContainer& NotifySyncState = State.NotifySyncStates[i];
				InputData.SharedNotifyDataState = NotifySyncState.SyncStatePointer->CloneShared();
				OutputData.SharedNotifyDataState = NotifySyncState.SyncStatePointer->CloneShared();
			}

			if (State.NotifySyncStates.IsNotifyState(NotifyEvent) && State.NotifySyncStates.IsNotifyTriggered(i))
			{
				// Set Activation Flag To false
				State.NotifySyncStates[i].IsActive = false;
				State.NotifySyncStates[i].SyncStatePointer = nullptr;
				ISyncedNotifyInterface::Execute_SimulationEnd(State.NotifySyncStates.GetNotifyObject(NotifyEvent),
					TimeStep.bIsResimulating, InputData, OutputData);
			}
		}
	}
}

void UNetMontageSimulator::FinalizeMontageFrame(const FMontageSimSyncState& SyncState)
{
	UAnimInstance* AnimInstance = AbilitySystemComponent->AbilityActorInfo->GetAnimInstance();
	if (!IsValid(AnimInstance))
	{
		return;
	}
	float RateMultiplier = 1.f;
	if (UNpAbilitySystemComponent* Asc = GetNpAbilitySystemComponent())
	{
		RateMultiplier = Asc->GetMontagePlayRateMultiplier();
	}
	//if we want to play a montage
	if (UAnimMontage* DesiredMontage = SyncState.GetPlayingMontage())
	{
		// if there's an active instance of the montage (is not blending out) , end it
		if (FAnimMontageInstance* MontageInstance = AnimInstance->GetActiveInstanceForMontage(DesiredMontage))
		{
			// if time is past the blend out time, we active montage instance
			if (SyncState.GetCurrentTime() >= GetMontageBlendOutTime(DesiredMontage))
			{
				// make sure the instance is past the blend out time , if it's not blend it out
				MontageInstance->Stop(MontageInstance->GetBlend(), false);
				return;
			}
			// if position is too far , restart the montage from desired new position, this makes it blend to target time
			if (SyncState.GetCurrentTime() - MontageInstance->GetPosition() > 0.1f)
			{
				MontageInstance->Stop(MontageInstance->GetBlend(), false);
				AnimInstance->Montage_Play(DesiredMontage, SyncState.GetPlayRate() * RateMultiplier, EMontagePlayReturnType::MontageLength, SyncState.GetCurrentTime());
				LastPlayingMontage = DesiredMontage;
				return;
			}
			//Set position if there's enough divergence ,
			//otherwise let it be to try and eliminate notifies that triggered last frame from triggering again
			if (FMath::Abs(SyncState.GetCurrentTime() - MontageInstance->GetPosition()) > 0.01)
			{
				MontageInstance->SetPosition(SyncState.GetCurrentTime());
			}
			MontageInstance->SetPlayRate(SyncState.GetPlayRate() * RateMultiplier);
			LastPlayingMontage = DesiredMontage;
			return;
		}
		// there's no active instance , if our time is less than blend out time , start the montage
		if (SyncState.GetCurrentTime() < GetMontageBlendOutTime(DesiredMontage))
		{
			AnimInstance->Montage_Play(DesiredMontage, SyncState.GetPlayRate() * RateMultiplier, EMontagePlayReturnType::MontageLength, SyncState.GetCurrentTime());
			LastPlayingMontage = DesiredMontage;
			return;
		}
	}

	//We Don't want to be playing any montage. check if we have a valid last playing montage
	// if we do, and it has a valid active instance, Stop it , then invalidate it.
	if (LastPlayingMontage)
	{
		if (FAnimMontageInstance* LastMontageInstance = AnimInstance->GetActiveInstanceForMontage(LastPlayingMontage))
		{
			LastMontageInstance->Stop(LastMontageInstance->GetBlend());
		}
		LastPlayingMontage = nullptr;
	}
}
