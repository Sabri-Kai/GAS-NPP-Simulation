// 2025 Yohoho Productions /  Sirkai


#include "DataTypes/GameplayEffectLayaredMove.h"

#include "AbilitySystemGlobals.h"
#include "MoverComponent.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Curves/CurveVector.h"

FAbilityEffectLayeredMove::FAbilityEffectLayeredMove()
{
	FinishVelocitySettings.FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
}

FLayeredMoveBase* FAbilityEffectLayeredMove::Clone() const
{
	return new FAbilityEffectLayeredMove(*this);
}

void FAbilityEffectLayeredMove::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);
	
	bool ValidHandle = ActiveEffectHandle >= 0;
	Ar.SerializeBits(&ValidHandle, 1);
	if (ValidHandle)
	{
		Ar << ActiveEffectHandle;
		Ar.SerializeBits(&bStopIfNoFoundEffect, 1);
		Ar.SerializeBits(&bPauseMoveIfInhibitedEffect, 1);
	}
	else
	{
		ActiveEffectHandle = INDEX_NONE;
		bStopIfNoFoundEffect = false;
		bPauseMoveIfInhibitedEffect = false;
	}
	
}

UScriptStruct* FAbilityEffectLayeredMove::GetScriptStruct() const
{
	return FAbilityEffectLayeredMove::StaticStruct();
}

FString FAbilityEffectLayeredMove::ToSimpleString() const
{
	return FLayeredMoveBase::ToSimpleString();
}

void FAbilityEffectLayeredMove::AddReferencedObjects(class FReferenceCollector& Collector)
{
	FLayeredMoveBase::AddReferencedObjects(Collector);
}


bool FAbilityEffectLayeredMove::GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
                                      const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{

	UNpAbilitySystemComponent* ASC = Cast<UNpAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MoverComp->GetOwner()));
	if (!ASC)
	{
		return false;
	}
	if (ShouldStopMove(ASC))
	{
		MixMode = EMoveMixMode::AdditiveVelocity;
		OutProposedMove.MixMode = MixMode;
		const bool StillActive = OnGenerateEffectEndMove(StartState,TimeStep,MoverComp,ASC,SimBlackboard,OutProposedMove);
		DurationMs = 0.f;
		return StillActive;
	}
	if (ShouldIgnoreMove(ASC))
	{
		MixMode = EMoveMixMode::AdditiveVelocity;
		OutProposedMove.MixMode = MixMode;
		return false;
	}
	// if we will be done next frame and this is last active frame
	// call OnGenerateEffectEndMove instead
	if (IsFinished(TimeStep.BaseSimTimeMs + TimeStep.StepMs))
	{
		return OnGenerateEffectEndMove(StartState,TimeStep,MoverComp,ASC,SimBlackboard,OutProposedMove);
	}
	return GenerateEffectMove(StartState, TimeStep, MoverComp,ASC, SimBlackboard, OutProposedMove);
}

bool FAbilityEffectLayeredMove::GenerateEffectMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
	const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	return false;
}

bool FAbilityEffectLayeredMove::ShouldStopMove(UNpAbilitySystemComponent* AbilitySystem) const
{
	// we don't want to stop whether effect is found or not, so no need to check for it.
	if (ActiveEffectHandle == INDEX_NONE || !bStopIfNoFoundEffect)
	{
		return false;
	}
	
	if (!AbilitySystem)
	{
		return false;
	}
	
	FActiveGameplayEffectHandle Handle;
	Handle.SetSyncedHandle(AbilitySystem,ActiveEffectHandle);
	return bStopIfNoFoundEffect && AbilitySystem->GetActiveGameplayEffect(Handle) == nullptr;
}

bool FAbilityEffectLayeredMove::ShouldIgnoreMove(UNpAbilitySystemComponent* AbilitySystem) const
{
	if (ActiveEffectHandle == INDEX_NONE || !bPauseMoveIfInhibitedEffect)
	{
		return false;
	}
	if (!AbilitySystem)
	{
		return false;
	}
	FActiveGameplayEffectHandle Handle;
	Handle.SetSyncedHandle(AbilitySystem,ActiveEffectHandle);
	const FActiveGameplayEffect* ActiveEffect = AbilitySystem->GetActiveGameplayEffect(Handle);
	if (!ActiveEffect && (bStopIfNoFoundEffect || bPauseMoveIfInhibitedEffect))
	{
		return true;
	}
	return AbilitySystem->GetActiveGameplayEffect(Handle)->bIsInhibited;
}

bool FAbilityEffectLayeredMove::OnGenerateEffectEndMove(const FMoverTickStartData& StartState,
	const FMoverTimeStep& TimeStep, const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem, UMoverBlackboard* SimBlackboard,
	FProposedMove& OutProposedMove)
{
	return false;
}




#pragma region Move To effect Move

FEffectLayeredMove_MoveTo::FEffectLayeredMove_MoveTo()
	: TargetLocation(ForceInitToZero)
	, bRestrictSpeedToExpected(false)
	, PathOffsetCurve(nullptr)
	, TimeMappingCurve(nullptr)
	, StartLocation(ForceInitToZero)
{
	DurationMs = 1000.f;
	MixMode = EMoveMixMode::OverrideVelocity;
}

FVector FEffectLayeredMove_MoveTo::GetPathOffsetInWorldSpace(const float MoveFraction) const
{
	if (PathOffsetCurve)
	{
		float MinCurveTime(0.f);
		float MaxCurveTime(1.f);

		PathOffsetCurve->GetTimeRange(MinCurveTime, MaxCurveTime);
		
		// Calculate path offset
		const FVector PathOffsetInFacingSpace = PathOffsetCurve->GetVectorValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), MoveFraction));;
		FRotator FacingRotation((TargetLocation-StartLocation).Rotation());
		FacingRotation.Pitch = 0.f; // By default we don't include pitch in the offset, but an option could be added if necessary
		return FacingRotation.RotateVector(PathOffsetInFacingSpace);
	}

	return FVector::ZeroVector;
}

float FEffectLayeredMove_MoveTo::EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const
{
	float MinCurveTime(0.f);
	float MaxCurveTime(1.f);

	Curve.GetTimeRange(MinCurveTime, MaxCurveTime);
	return Curve.GetFloatValue(FMath::GetRangeValue(FVector2f(MinCurveTime, MaxCurveTime), Fraction));
}

void FEffectLayeredMove_MoveTo::OnStart(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard)
{
	FAbilityEffectLayeredMove::OnStart(MoverComp, SimBlackboard);
	StartLocation = MoverComp->GetUpdatedComponentTransform().GetLocation();
}

bool FEffectLayeredMove_MoveTo::GenerateEffectMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
	, const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem
	, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove)
{
	MixMode = EMoveMixMode::OverrideAll;
	OutProposedMove.MixMode = MixMode;

	const float DeltaSeconds = TimeStep.StepMs / 1000.f;
	
	float MoveFraction = (TimeStep.BaseSimTimeMs - StartSimTimeMs) / DurationMs;

	if (TimeMappingCurve)
	{
		MoveFraction = EvaluateFloatCurveAtFraction(*TimeMappingCurve, MoveFraction);
	}
	
	const AActor* MoverActor = MoverComp->GetOwner();
	
	FVector CurrentTargetLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, MoveFraction);
	FVector PathOffset = GetPathOffsetInWorldSpace(MoveFraction);
	CurrentTargetLocation += PathOffset;

	const FVector CurrentLocation = MoverActor->GetActorLocation();

	FVector Velocity = (CurrentTargetLocation - CurrentLocation) / DeltaSeconds;

	if (bRestrictSpeedToExpected && !Velocity.IsNearlyZero(UE_KINDA_SMALL_NUMBER))
	{
		// Calculate expected current location (if we didn't have collision and moved exactly where our velocity should have taken us)
		const float PreviousMoveFraction = (TimeStep.BaseSimTimeMs - StartSimTimeMs - TimeStep.StepMs) / DurationMs;
		FVector CurrentExpectedLocation = FMath::Lerp<FVector, float>(StartLocation, TargetLocation, PreviousMoveFraction);
		CurrentExpectedLocation += GetPathOffsetInWorldSpace(PreviousMoveFraction);

		// Restrict speed to the expected speed, allowing some small amount of error
		const FVector ExpectedForce = (CurrentTargetLocation - CurrentExpectedLocation) / DeltaSeconds;
		const float ExpectedSpeed = ExpectedForce.Size();
		const float CurrentSpeedSqr = Velocity.SizeSquared();

		const float ErrorAllowance = 0.5f; // in cm/s
		if (CurrentSpeedSqr > FMath::Square(ExpectedSpeed + ErrorAllowance))
		{
			Velocity.Normalize();
			Velocity *= ExpectedSpeed;
		}
	}
	
	OutProposedMove.LinearVelocity = Velocity;
	
	return true;
}

FLayeredMoveBase* FEffectLayeredMove_MoveTo::Clone() const
{
	FEffectLayeredMove_MoveTo* CopyPtr = new FEffectLayeredMove_MoveTo(*this);
	return CopyPtr;
}

void FEffectLayeredMove_MoveTo::NetSerialize(FArchive& Ar)
{
	Super::NetSerialize(Ar);

	SerializePackedVector<10,16>(StartLocation,Ar);
	SerializePackedVector<10,16>(TargetLocation,Ar);
	Ar.SerializeBits(&bRestrictSpeedToExpected,1);
	bool HasPathOffset = Ar.IsSaving() ? IsValid(PathOffsetCurve) : false;
	Ar.SerializeBits(&HasPathOffset,1);
	if (HasPathOffset)
	{
		Ar << PathOffsetCurve;
	}
	bool HasTimeMapping = Ar.IsSaving() ? IsValid(TimeMappingCurve) : false;
	Ar.SerializeBits(&HasTimeMapping,1);
	if (HasTimeMapping)
	{
		Ar << TimeMappingCurve;
	}
}

UScriptStruct* FEffectLayeredMove_MoveTo::GetScriptStruct() const
{
	return FEffectLayeredMove_MoveTo::StaticStruct();
}

FString FEffectLayeredMove_MoveTo::ToSimpleString() const
{
	return FString::Printf(TEXT("Effect Move To"));
}

void FEffectLayeredMove_MoveTo::AddReferencedObjects(FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(Collector);
}

#pragma endregion