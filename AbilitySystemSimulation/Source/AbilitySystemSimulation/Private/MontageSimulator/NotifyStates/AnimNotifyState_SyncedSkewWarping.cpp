// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotifyState_SyncedSkewWarping.h"

#include "MotionWarpingComponent.h"
#include "MoverComponent.h"
#include "NetworkPredictionTrace.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "MontageSimulator/NetMontageSimulator.h"


#pragma region Synced data
FWarpingNotifySyncData::FWarpingNotifySyncData()
{
}
FSyncedNotifyData* FWarpingNotifySyncData::Clone() const
{
	FWarpingNotifySyncData* Copy = new FWarpingNotifySyncData(*this);
	return Copy;
}
bool FWarpingNotifySyncData::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	bOutSuccess = true;
	// Serialize the actor reference
	bool bHasActor = IsValid(TargetActor);
	Ar.SerializeBits(&bHasActor, 1);
	if (bHasActor)
	{
		Ar << TargetActor;
	}
	else
	{
		TargetActor = nullptr;
	}
	SerializePackedVector<100, 30>(TargetLocation, Ar);
	bool IsStartLocZero = StartLocation.IsNearlyZero();
	Ar.SerializeBits(&IsStartLocZero, 1);
	if (IsStartLocZero)
	{
		StartLocation = FVector::ZeroVector;
	}
	else
	{
		SerializePackedVector<100, 30>(StartLocation, Ar);
	}
	
	TargetRotation.SerializeCompressedShort(Ar);

	Ar.SerializeBits(&WarpTranslation, 1);
	Ar.SerializeBits(&WarpRotation, 1);
	return bOutSuccess;
}
UScriptStruct* FWarpingNotifySyncData::GetScriptStruct() const
{
	return StaticStruct();
}
void FWarpingNotifySyncData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append(TCHAR_TO_ANSI(*GetScriptStruct()->GetName()));
	Out.Appendf("\n");
	Out.Appendf("Loc: X=%.2f Y=%.2f Z=%.2f\n", TargetLocation.X, TargetLocation.Y, TargetLocation.Z);
	Out.Appendf("Rot: Yaw=%.2f Pitch=%.2f Roll=%.2f\n", TargetRotation.Yaw, TargetRotation.Pitch, TargetRotation.Roll);
	if (TargetActor)
	{
		Out.Appendf("Target Actor %s\n", TCHAR_TO_ANSI(*GetNameSafe(TargetActor)));
	}
}
bool FWarpingNotifySyncData::ShouldReconcile(const FSyncedNotifyData& AuthorityState) const
{
	const FWarpingNotifySyncData* Authority = static_cast<const FWarpingNotifySyncData*>(&AuthorityState);
	UE_NP_TRACE_RECONCILE(TargetActor != Authority->TargetActor, "Different Target Actor");
	UE_NP_TRACE_RECONCILE(WarpRotation != Authority->WarpRotation, "Different bWarpLocation");
	UE_NP_TRACE_RECONCILE(WarpTranslation != Authority->WarpTranslation, "Different bWarpRotation"); 
	if (!TargetActor)
	{
		const bool DifferentTargetLoc = !TargetLocation.Equals(Authority->TargetLocation, 5.f);
		const bool DifferentTargetRot = !TargetRotation.Equals(Authority->TargetRotation, 5.f);
		UE_NP_TRACE_RECONCILE(DifferentTargetLoc, "Different Target locations:");
		UE_NP_TRACE_RECONCILE(DifferentTargetRot, "Different Target Rotations");
	}
	return false; //UE_NP_TRACE_RECONCILE macro returns true if bool is true
}
void FWarpingNotifySyncData::Interpolate(const FSyncedNotifyData& From, const FSyncedNotifyData& To, float Pct)
{
	const FWarpingNotifySyncData* ToState = static_cast<const FWarpingNotifySyncData*>(&To);
	const FWarpingNotifySyncData* FromState = static_cast<const FWarpingNotifySyncData*>(&From);
	TargetLocation = FMath::Lerp(FromState->TargetLocation, ToState->TargetLocation, Pct);
	TargetRotation = FMath::Lerp(FromState->TargetRotation, ToState->TargetRotation, Pct);
	TargetActor = ToState->TargetActor;
	WarpRotation = ToState->WarpRotation;
	WarpTranslation = ToState->WarpTranslation;
}
#pragma endregion

#pragma region Root Motion Warping Processor Class
USyncedSkewWarpingProcessor::USyncedSkewWarpingProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FWarpingNotifySyncData USyncedSkewWarpingProcessor::UpdateTargetData_Internal(const FSimTickNotifyData& SimInput
	,const FWarpingNotifySyncData& CurrentTargetData)
{
	if (bHasBlueprintUpdateTargetData)
	{
		return K2_UpdateTargetData(SimInput, CurrentTargetData);
	}
	return UpdateTargetData(SimInput, CurrentTargetData);
}

FWarpingNotifySyncData USyncedSkewWarpingProcessor::UpdateTargetData(const FSimTickNotifyData& SimInput,
	const FWarpingNotifySyncData& CurrentTargetData)
{
	FWarpingNotifySyncData OutData = CurrentTargetData;
	// default implementation is the simplest possible, if we don't have actor we don't update,
	// if we do , we get the closest location based on bounds, otherwise we keep same data we set at initialization
	AActor* OwnerActor = SimInput.MontagePlayer->GetAvatarActor();
	if (!OwnerActor || !OutData.TargetActor)
	{
		return OutData;
	}
	
	FVector TargetOrigin = FVector::ZeroVector;
	FVector TargetBounds = FVector::ZeroVector;
	OutData.TargetActor->GetActorBounds(true,TargetOrigin,TargetBounds);
	const float TargetBoundsSize = FMath::Max(TargetBounds.X,TargetBounds.Y);
	FVector OwnOrigin = FVector::ZeroVector;
	FVector OwnBounds = FVector::ZeroVector;
	OwnerActor->GetActorBounds(true,OwnOrigin,OwnBounds);
	const float OwnBoundsSize = FMath::Max(OwnBounds.X,OwnBounds.Y);
	FVector Offset = OutData.TargetActor->GetActorLocation() - OwnerActor->GetActorLocation();
	Offset = Offset.GetSafeNormal() * (Offset.Length() - (TargetBoundsSize + OwnBoundsSize));
	OutData.TargetLocation = OwnerActor->GetActorLocation() + Offset;
	return OutData;
}

FWarpingNotifySyncData USyncedSkewWarpingProcessor::InitializeTargetData_Internal(const bool IsReSimulating,
	const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& TargetData)
{
	if (bHasBlueprintInitializeTargetData)
	{
		return K2_InitializeTargetData(IsReSimulating,SimInput,TargetData);
	}
	return InitializeTargetData(IsReSimulating,SimInput,TargetData);
}

FWarpingNotifySyncData USyncedSkewWarpingProcessor::InitializeTargetData(const bool IsReSimulating,
	const FSimTickNotifyData& SimInput, const FWarpingNotifySyncData& CurrentTargetData)
{
	return CurrentTargetData;
}

FTransform USyncedSkewWarpingProcessor::ProcessRootMotion(float DeltaSeconds, const FTransform& InRootMotion,
                                                          const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput)
{
	TSharedPtr<FWarpingNotifySyncData> SnappingSyncData = StaticCastSharedPtr<FWarpingNotifySyncData>(SimOutput.SharedNotifyDataState);
	UMoverComponent* MoverComp = SimInput.MontagePlayer->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	UNpAbilitySystemComponent* Asc = SimInput.MontagePlayer->GetNpAbilitySystemComponent();
	if (!SnappingSyncData.IsValid() || !SimInput.MontagePlayer->GetNpAbilitySystemComponent() || !MoverComp)
	{
		return InRootMotion;
	}

	FTransform FinalRootMotion = InRootMotion;

	const FTransform RootMotionTotalInState = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(SimInput.AnimMontage, SimInput.NotifyStartTime, SimInput.NotifyEndTime);
	const FTransform RootMotionTotal = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(SimInput.AnimMontage, SimInput.NotifyTickStartTime, SimInput.NotifyEndTime);
	const FTransform RootMotionDelta = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(SimInput.AnimMontage, SimInput.NotifyTickStartTime, FMath::Min(SimInput.NotifyTickStartTime + SimInput.DeltaSeconds, SimInput.NotifyEndTime));
	FTransform ExtraRootMotion = FTransform::Identity;
	
	if (SimInput.NotifyTickStartTime > SimInput.NotifyEndTime)
	{
		ExtraRootMotion = UMotionWarpingUtilities::ExtractRootMotionFromAnimation(SimInput.AnimMontage, SimInput.NotifyEndTime, SimInput.NotifyTickStartTime);
	}
	const FTransform ActorTransform = MoverComp->GetUpdatedComponentTransform();
	const FTransform MeshRelativeTransform = Asc->GetMeshRelativeTransform();
	const FVector CurrentLocation = ActorTransform.GetLocation() + MeshRelativeTransform.GetTranslation();
	const FRotator CurrentRotation = ActorTransform.Rotator();
	FTransform TargetTransform = FTransform(SnappingSyncData->TargetRotation, SnappingSyncData->TargetLocation);
	TargetTransform = FinalizeTargetTransform(FTransform(CurrentRotation,CurrentLocation),TargetTransform);
	SnappingSyncData->TargetLocation = TargetTransform.GetTranslation();
	SnappingSyncData->TargetRotation = TargetTransform.Rotator();
	if (DrawDebugDuration >= 0.f)
	{
		const float DebugDuration = FMath::Max(DeltaSeconds,DrawDebugDuration);
		DrawDebugSphere(SimInput.MontagePlayer->GetWorld(),SnappingSyncData->StartLocation,60.f,4,FColor::Green,false,DebugDuration);
		DrawDebugSphere(SimInput.MontagePlayer->GetWorld(),SnappingSyncData->TargetLocation,60.f,4,FColor::Red,false,DebugDuration);
	}
	
	if (bWarpTranslation)
	{
		const FVector DeltaTranslation = RootMotionDelta.GetLocation();
		const FVector TotalTranslation = RootMotionTotal.GetLocation();
		const FTransform MeshTransform = MeshRelativeTransform * ActorTransform;
		// if there is translation in the animation, warp it
		if (!RootMotionTotalInState.GetTranslation().IsNearlyZero())
		{
			if (!DeltaTranslation.IsNearlyZero())
			{
				
				const FVector TargetLocation = MeshTransform.InverseTransformPositionNoScale(TargetTransform.GetLocation());
				// warp translation is a static function , safe to use
				const FVector WarpedTranslation = WarpTranslation(FTransform::Identity, DeltaTranslation, TotalTranslation, TargetLocation) + ExtraRootMotion.GetLocation();
				FinalRootMotion.SetTranslation(WarpedTranslation);
			}
		}
		// if there is no translation in the animation, add it
		else
		{
			const FVector DeltaToTarget = TargetTransform.GetLocation() - CurrentLocation;
			if (DeltaToTarget.IsNearlyZero())
			{
				FinalRootMotion.SetTranslation(FVector::ZeroVector);
			}
			else
			{
				float Alpha = FMath::Clamp((SimInput.NotifyTickStartTime - SimInput.NotifyStartTime) / (SimInput.NotifyEndTime - SimInput.NotifyStartTime), 0.f, 1.f);
				Alpha = FAlphaBlend::AlphaToBlendOption(Alpha, AddTranslationEasingFunc, AddTranslationEasingCurve);
				const FVector NextLocation = FMath::Lerp<FVector, float>(SnappingSyncData->StartLocation, TargetTransform.GetLocation(), Alpha);
				FVector FinalDeltaTranslation = (NextLocation - CurrentLocation);
				FinalDeltaTranslation = MeshTransform.InverseTransformVectorNoScale(FinalDeltaTranslation);
				FinalRootMotion.SetTranslation(FinalDeltaTranslation + ExtraRootMotion.GetLocation());
			}
		}
	}

	if (bWarpRotation)
	{
		const FQuat WarpedRotation = ExtraRootMotion.GetRotation() * WarpRotation(DeltaSeconds, TargetTransform.GetRotation(), RootMotionDelta, RootMotionTotal, SimInput);
		FinalRootMotion.SetRotation(WarpedRotation);
	}
	return FinalRootMotion;
}

FTransform USyncedSkewWarpingProcessor::FinalizeTargetTransform(const FTransform& CurrentTransform,
	const FTransform& TargetTransform)
{
	FTransform OutTransform = TargetTransform;
	
	if (RotationType == EMotionWarpRotationType::Facing)
	{
		const FVector ToSyncPoint = (TargetTransform.GetLocation() - CurrentTransform.GetLocation()).GetSafeNormal2D();
		OutTransform.SetRotation(FRotationMatrix::MakeFromXZ(ToSyncPoint, FVector::UpVector).ToQuat());
	}
	if (CachedOffsetFromWarpPoint.IsSet())
	{
		OutTransform = CachedOffsetFromWarpPoint.GetValue() * OutTransform;
	}
	if (bIgnoreZAxis)
	{
		FVector TargetLocation = OutTransform.GetTranslation();
		TargetLocation.Z = CurrentTransform.GetTranslation().Z;
		OutTransform.SetLocation(TargetLocation);
	}
	return OutTransform;
	
}


FTransform USyncedSkewWarpingProcessor::CalculateRootTransformRelativeToWarpPointAtTime(const FTransform& BaseMeshTransform, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform)
{
	// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
	const FTransform MeshCompRelativeRotInverse = FTransform(BaseMeshTransform.GetRotation().Inverse());
	const FTransform RootTransform = MeshCompRelativeRotInverse * UMotionWarpingUtilities::ExtractRootTransformFromAnimation(Animation, Time);
	return RootTransform.GetRelativeTransform((MeshCompRelativeRotInverse * WarpPointTransform));
}

FTransform USyncedSkewWarpingProcessor::CalculateRootTransformRelativeToWarpPointAtTime(const UAnimInstance& AnimInstance,
	const UAnimSequenceBase* Animation, const FTransform& BaseMeshTransform,
	float Time, const FName& WarpPointBoneName)
{
	const FBoneContainer& FullBoneContainer = AnimInstance.GetRequiredBones();
	const int32 BoneIndex = FullBoneContainer.GetPoseBoneIndexForBoneName(WarpPointBoneName);
	if (BoneIndex != INDEX_NONE)
	{
		TArray<FBoneIndexType> RequiredBoneIndexArray = { 0, (FBoneIndexType)BoneIndex };
		FullBoneContainer.GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBoneIndexArray);

		FBoneContainer LimitedBoneContainer(RequiredBoneIndexArray, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *FullBoneContainer.GetAsset());

		FCSPose<FCompactPose> Pose;
		UMotionWarpingUtilities::ExtractComponentSpacePose(Animation, LimitedBoneContainer, Time, false, Pose);

		// Inverse of mesh's relative rotation. Used to convert root and warp point in the animation from Y forward to X forward
		const FTransform MeshCompRelativeRotInverse = FTransform(BaseMeshTransform.GetRotation().Inverse());

		const FTransform RootTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(0));
		const FTransform WarpPointTransform = MeshCompRelativeRotInverse * Pose.GetComponentSpaceTransform(FCompactPoseBoneIndex(1));
		return RootTransform.GetRelativeTransform(WarpPointTransform);
	}

	return FTransform::Identity;
}

FQuat USyncedSkewWarpingProcessor::WarpRotation(float DeltaSeconds, const FQuat& TargetRotation,
	const FTransform& RootMotionDelta, const FTransform& RootMotionTotal, const FSimTickNotifyData& SimInput) const
{
	UMoverComponent* MoverComp = SimInput.MontagePlayer->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	UNpAbilitySystemComponent* Asc = SimInput.MontagePlayer->GetNpAbilitySystemComponent();
	if (!MoverComp)
	{
		return RootMotionDelta.GetRotation();
	}
	FQuat MeshRotationOffset =Asc->GetMeshRelativeTransform().GetRotation();
	FQuat CurrentRotation = MoverComp->GetUpdatedComponentTransform().GetRotation() * MeshRotationOffset;
	FQuat FinalTargetRot = CurrentRotation.Inverse() * (TargetRotation * MeshRotationOffset);

	const FQuat TotalRootMotionRotation = RootMotionTotal.GetRotation();
	const float TimeRemaining = (SimInput.NotifyEndTime - SimInput.NotifyTickStartTime) * WarpRotationTimeMultiplier;
	const float Alpha = FMath::Clamp(DeltaSeconds / TimeRemaining, 0.f, 1.f);
	FQuat TargetRotThisFrame = FQuat::Slerp(TotalRootMotionRotation, FinalTargetRot, Alpha);

	if (RotationMethod != EMotionWarpRotationMethod::Slerp)
	{
		const float AngleDeltaThisFrame = TotalRootMotionRotation.AngularDistance(TargetRotThisFrame);
		const float MaxAngleDelta = FMath::Abs(FMath::DegreesToRadians(DeltaSeconds * WarpMaxRotationRate));
		const float TotalAngleDelta = TotalRootMotionRotation.AngularDistance(FinalTargetRot);
		if (RotationMethod == EMotionWarpRotationMethod::ConstantRate && (TotalAngleDelta <= MaxAngleDelta))
		{
			TargetRotThisFrame = FinalTargetRot;
		}
		else if ((AngleDeltaThisFrame > MaxAngleDelta) || RotationMethod == EMotionWarpRotationMethod::ConstantRate)
		{
			const FVector CrossProduct = FVector::CrossProduct(TotalRootMotionRotation.Vector(), FinalTargetRot.Vector());
			const float SignDirection = FMath::Sign(CrossProduct.Z);
			const FQuat ClampedRotationThisFrame = FQuat(FVector(0, 0, 1), MaxAngleDelta * SignDirection);
			TargetRotThisFrame = ClampedRotationThisFrame;
		}
	}

	const FQuat DeltaOut = TargetRotThisFrame * TotalRootMotionRotation.Inverse();

	return (DeltaOut * RootMotionDelta.GetRotation());
}


FVector USyncedSkewWarpingProcessor::WarpTranslation(const FTransform& CurrentTransform, const FVector& DeltaTranslation, const FVector& TotalTranslation, const FVector& TargetLocation)
{
	if (!DeltaTranslation.IsNearlyZero())
	{
		const FQuat CurrentRotation = CurrentTransform.GetRotation();
		const FVector CurrentLocation = CurrentTransform.GetLocation();
		const FVector FutureLocation = CurrentLocation + TotalTranslation;
		const FVector CurrentToWorldOffset = TargetLocation - CurrentLocation;
		const FVector CurrentToRootOffset = FutureLocation - CurrentLocation;

		// Create a matrix we can use to put everything into a space looking straight at RootMotionSyncPosition. "forward" should be the axis along which we want to scale. 
		FVector ToRootNormalized = CurrentToRootOffset.GetSafeNormal();

		float BestMatchDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisX()));
		FMatrix ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisZ());

		float ZDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisZ()));
		if (ZDot > BestMatchDot)
		{
			ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisX());
			BestMatchDot = ZDot;
		}

		float YDot = FMath::Abs(FVector::DotProduct(ToRootNormalized, CurrentRotation.GetAxisY()));
		if (YDot > BestMatchDot)
		{
			ToRootSyncSpace = FRotationMatrix::MakeFromXZ(ToRootNormalized, CurrentRotation.GetAxisZ());
		}

		// Put everything into RootSyncSpace.
		const FVector RootMotionInSyncSpace = ToRootSyncSpace.InverseTransformVector(DeltaTranslation);
		const FVector CurrentToWorldSync = ToRootSyncSpace.InverseTransformVector(CurrentToWorldOffset);
		const FVector CurrentToRootMotionSync = ToRootSyncSpace.InverseTransformVector(CurrentToRootOffset);

		FVector CurrentToWorldSyncNorm = CurrentToWorldSync;
		CurrentToWorldSyncNorm.Normalize();

		FVector CurrentToRootMotionSyncNorm = CurrentToRootMotionSync;
		CurrentToRootMotionSyncNorm.Normalize();

		// Calculate skew Yaw Angle. 
		FVector FlatToWorld = FVector(CurrentToWorldSyncNorm.X, CurrentToWorldSyncNorm.Y, 0.0f);
		FlatToWorld.Normalize();
		FVector FlatToRoot = FVector(CurrentToRootMotionSyncNorm.X, CurrentToRootMotionSyncNorm.Y, 0.0f);
		FlatToRoot.Normalize();
		float AngleAboutZ = FMath::Acos(FVector::DotProduct(FlatToWorld, FlatToRoot));
		float AngleAboutZNorm = FMath::DegreesToRadians(FRotator::NormalizeAxis(FMath::RadiansToDegrees(AngleAboutZ)));
		if (FlatToWorld.Y < 0.0f)
		{
			AngleAboutZNorm *= -1.0f;
		}

		// Calculate Skew Pitch Angle. 
		FVector ToWorldNoY = FVector(CurrentToWorldSyncNorm.X, 0.0f, CurrentToWorldSyncNorm.Z);
		ToWorldNoY.Normalize();
		FVector ToRootNoY = FVector(CurrentToRootMotionSyncNorm.X, 0.0f, CurrentToRootMotionSyncNorm.Z);
		ToRootNoY.Normalize();
		const float AngleAboutY = FMath::Acos(FVector::DotProduct(ToWorldNoY, ToRootNoY));
		float AngleAboutYNorm = FMath::DegreesToRadians(FRotator::NormalizeAxis(FMath::RadiansToDegrees(AngleAboutY)));
		if (ToWorldNoY.Z < 0.0f)
		{
			AngleAboutYNorm *= -1.0f;
		}

		FVector SkewedRootMotion = FVector::ZeroVector;
		float ProjectedScale = FVector::DotProduct(CurrentToWorldSync, CurrentToRootMotionSyncNorm) / CurrentToRootMotionSync.Size();
		if (ProjectedScale != 0.0f)
		{
			FMatrix ScaleMatrix;
			ScaleMatrix.SetIdentity();
			ScaleMatrix.SetAxis(0, FVector(ProjectedScale, 0.0f, 0.0f));
			ScaleMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ScaleMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ShearXAlongYMatrix;
			ShearXAlongYMatrix.SetIdentity();
			ShearXAlongYMatrix.SetAxis(0, FVector(1.0f, FMath::Tan(AngleAboutZNorm), 0.0f));
			ShearXAlongYMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ShearXAlongYMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ShearXAlongZMatrix;
			ShearXAlongZMatrix.SetIdentity();
			ShearXAlongZMatrix.SetAxis(0, FVector(1.0f, 0.0f, FMath::Tan(AngleAboutYNorm)));
			ShearXAlongZMatrix.SetAxis(1, FVector(0.0f, 1.0f, 0.0f));
			ShearXAlongZMatrix.SetAxis(2, FVector(0.0f, 0.0f, 1.0f));

			FMatrix ScaledSkewMatrix = ScaleMatrix * ShearXAlongYMatrix * ShearXAlongZMatrix;

			// Skew and scale the Root motion. 
			SkewedRootMotion = ScaledSkewMatrix.TransformVector(RootMotionInSyncSpace);
		}
		else if (!CurrentToRootMotionSync.IsZero() && !CurrentToWorldSync.IsZero() && !RootMotionInSyncSpace.IsZero())
		{
			// Figure out ratio between remaining Root and remaining World. Then project scaled length of current Root onto World.
			const float Scale = CurrentToWorldSync.Size() / CurrentToRootMotionSync.Size();
			const float StepTowardTarget = RootMotionInSyncSpace.ProjectOnTo(RootMotionInSyncSpace).Size();
			SkewedRootMotion = CurrentToWorldSyncNorm * (Scale * StepTowardTarget);
		}

		// Put our result back in world space.  
		return ToRootSyncSpace.TransformVector(SkewedRootMotion);
	}

	return FVector::ZeroVector;
}

void USyncedSkewWarpingProcessor::SetupProcessor(const FProcessorSetupData& SimInput)
{
	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName InitializeTargetDataFuncName = FName(TEXT("K2_InitializeTargetData"));
	UFunction* InitializeTargetDataFunction = GetClass()->FindFunctionByName(InitializeTargetDataFuncName);
	bHasBlueprintInitializeTargetData = IsImplementedInBlueprint(InitializeTargetDataFunction);

	static FName UpdateTargetDataFuncName = FName(TEXT("K2_UpdateTargetData"));
	UFunction* UpdateTargetDataFunction = GetClass()->FindFunctionByName(UpdateTargetDataFuncName);
	bHasBlueprintUpdateTargetData = IsImplementedInBlueprint(UpdateTargetDataFunction);
	
	UMoverComponent* MoverComp = SimInput.MontagePlayer->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	UNpAbilitySystemComponent* Asc = SimInput.MontagePlayer->GetNpAbilitySystemComponent();
	if (!MoverComp)
	{
		return;
	}
	// initialize the static data of this modifier. these values don't change
	FTransform BaseMeshTransform = Asc->GetMeshRelativeTransform();
	if (WarpPointAnimProvider != EWarpPointAnimProvider::None)
	{
		if (!CachedOffsetFromWarpPoint.IsSet())
		{
			if (WarpPointAnimProvider == EWarpPointAnimProvider::Static)
			{
				// cached offset is from a static pre-defined transform
				CachedOffsetFromWarpPoint = CalculateRootTransformRelativeToWarpPointAtTime(BaseMeshTransform, SimInput.AnimMontage
					, SimInput.NotifyEndTime, WarpPointAnimTransform);
			}
			else if (WarpPointAnimProvider == EWarpPointAnimProvider::Bone)
			{
				// cached offset is from bone transform in the montage at the end of the notify state. static data.
				CachedOffsetFromWarpPoint = CalculateRootTransformRelativeToWarpPointAtTime(*SimInput.MontagePlayer->GetAnimInstance()
					, SimInput.AnimMontage,BaseMeshTransform, SimInput.NotifyEndTime, WarpPointAnimBoneName);
			}
		}
	}
}
#pragma endregion

#pragma region Synced Notify State 
UAnimNotifyState_SyncedSkewWarping::UAnimNotifyState_SyncedSkewWarping(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Processor =  ObjectInitializer.CreateDefaultSubobject<USyncedSkewWarpingProcessor>(this, TEXT("Processor"));
}
void UAnimNotifyState_SyncedSkewWarping::SimulationBegin_Implementation(const bool IsReSimulating,
                                                                        const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput)
{
	if (!SimInput.MontagePlayer->GetAvatarActor())
	{
		return;
	}
	TSharedPtr<FWarpingNotifySyncData> OutState = StaticCastSharedPtr<FWarpingNotifySyncData>(SimOutput.SharedNotifyDataState);
	UMoverComponent* MoverComp = SimInput.MontagePlayer->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	UNpAbilitySystemComponent* Asc = SimInput.MontagePlayer->GetNpAbilitySystemComponent();
	if (!OutState.IsValid() || !MoverComp)
	{
		return;
	}
	const FTransform MeshTransform = Asc->GetMeshRelativeTransform() * MoverComp->GetUpdatedComponentTransform();
	Processor->SetupProcessor(FProcessorSetupData(SimInput));
	OutState->TargetLocation = MeshTransform.GetTranslation();
	OutState->TargetRotation = MoverComp->GetUpdatedComponentTransform().GetRotation().Rotator();
	OutState->StartLocation  = MeshTransform.GetTranslation();
	*OutState = Processor->InitializeTargetData_Internal(IsReSimulating,SimInput,*OutState);
	// make sure start location can't be overriden , that will break movement
	OutState->StartLocation  = MeshTransform.GetTranslation();
	// ensure bWarpRotation and Translation are set from processor
	OutState->WarpRotation = Processor->bWarpRotation;
	OutState->WarpTranslation = Processor->bWarpTranslation;
	// Set the movement mode if desired
	if (!StartOverrideMovementMode.IsNone())
	{
		MoverComp->QueueNextMode(StartOverrideMovementMode);
	}
}

void UAnimNotifyState_SyncedSkewWarping::SimulationTick_Implementation(const FAbilitySystemTimeStep& InTimeStep,
	const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput)
{
	TSharedPtr<FWarpingNotifySyncData> OutState = StaticCastSharedPtr<FWarpingNotifySyncData>(SimOutput.SharedNotifyDataState);
	check(OutState.IsValid());
	const float DeltaSeconds = InTimeStep.StepMs / 1000.f;
	*OutState = Processor->UpdateTargetData_Internal(SimInput,*OutState);
	SimOutput.ExtractionData = Processor->ProcessRootMotion(DeltaSeconds,SimOutput.ExtractionData,SimInput,SimOutput);
	// ensure bWarpRotation and Translation are set from processor
	OutState->WarpRotation = Processor->bWarpRotation;
	OutState->WarpTranslation = Processor->bWarpTranslation;
}

void UAnimNotifyState_SyncedSkewWarping::SimulationEnd_Implementation(const bool IsReSimulating,
	const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput)
{
	if (!EndOverrideMovementMode.IsNone())
	{
		UMoverComponent* MoverComp = SimInput.MontagePlayer->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
		MoverComp->QueueNextMode(EndOverrideMovementMode);
	}
}

void UAnimNotifyState_SyncedSkewWarping::RestoreFrame_Implementation(const FRestoreNotifyData& InputData,const FSyncedNotifyDataContainer& AuthorityState,
	const FSyncedNotifyDataContainer& ExpungedState, const UNetMontageSimulator* MontageSimulator)
{
	
	if (!Processor || !MontageSimulator || !MontageSimulator->GetAvatarActor())
	{
		return;
	}
	UMoverComponent* MoverComp = MontageSimulator->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	if (!MoverComp)
	{
		return;
	}
	if (AuthorityState.IsActive)
	{
		TSharedPtr<const FWarpingNotifySyncData> AuthoritySyncState = StaticCastSharedPtr<const FWarpingNotifySyncData>(AuthorityState.SyncStatePointer);
		Processor->SetupProcessor(FProcessorSetupData(InputData));
		// let processor update bWarpRotation and bWarpTranslation at runtime. and they will be synced
		Processor->bWarpRotation = AuthoritySyncState->WarpRotation;
		Processor->bWarpTranslation = AuthoritySyncState->WarpTranslation;
	}
}

UScriptStruct* UAnimNotifyState_SyncedSkewWarping::GetRequiredType_Implementation()
{
	return FWarpingNotifySyncData::StaticStruct();
}
#pragma endregion 


