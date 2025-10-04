// 2025 Yohoho Productions /  Sirkai


#include "ProjectilesSimulator/SyncedProjectileBase.h"

#include "NetworkPredictionWorldManager.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "ProjectilesSimulator/ProjectilesSimulator.h"


ASyncedProjectileBase::ASyncedProjectileBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UNpAbilitySystemComponent* ASyncedProjectileBase::GetOwningAbilitySystem() const
{
	if (!OwningSimulator)
	{
		return nullptr;
	}
	return OwningSimulator->GetOwningAbilitySystem();
}

void ASyncedProjectileBase::MoveProjectile(const bool& bGeneratingTrajectory,const FProjectileMoveTimeStep& TimeStep, const FProjectileStep& InputStep,
	FProjectileMove& OutputMove, TArray<FProjectileHitBroadcast>& OutputHits)
{
	// if we exploded we return and do nothing
	if (InputStep.Move.bExploded)
	{
		OutputMove = InputStep.Move;
		return;
	}
	const float DeltaTime = TimeStep.DeltaTimeMs / 1000.f;
	// First generate the desired Velocity for this move;
	const FVector DesiredVelocity = GetDesiredVelocity(DeltaTime, InputStep);

	OutputMove = InputStep.Move;
	OutputMove.Velocity = DesiredVelocity;
	float LeftOverTimeMS = TimeStep.DeltaTimeMs;
	FVector DeltaMove = OutputMove.Velocity * DeltaTime;
	int32 Iterations = 0;
	OutputHits.Empty();
	// iterate through the move to allow for partial movement ,
	// like projectile bounce mid-move , allow it to sweep along the new velocity in time left after first bounce hit
	while (!FMath::IsNearlyZero(LeftOverTimeMS) && Iterations < 7 && !DeltaMove.IsNearlyZero())
	{
		Iterations++;
		SweepProjectile(bGeneratingTrajectory,TimeStep,InputStep,OutputMove,OutputHits,LeftOverTimeMS,DeltaMove);
	}
}

void ASyncedProjectileBase::SweepProjectile(const bool& bGeneratingTrajectory,const FProjectileMoveTimeStep& TimeStep,const FProjectileStep& InputStep,
	FProjectileMove& OutputMove, TArray<FProjectileHitBroadcast>& OutputHits,float& LeftOverTimeMS,FVector& DeltaMove)
{
	const FVector MoveStart = OutputMove.Position;
	FVector MoveEnd = MoveStart + DeltaMove;
	if (OutputMove.bExploded)
	{
		DeltaMove = FVector::ZeroVector;
		LeftOverTimeMS = 0.f;
		return;
	}
	
	FCollisionShape CollisionShape;
	switch (ProjectileCollisionShape)
	{
	case EProjectileCollisionShape::ESphere:
		{
			CollisionShape = FCollisionShape::MakeSphere(SphereRadius);
			break;
		}
	case EProjectileCollisionShape::EBox:
		{
			CollisionShape = FCollisionShape::MakeBox(BoxHalfExtent);
			break;
		}
	}
	
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bIgnoreBlocks = false;
	// if we are predicting trajectory make sure to ignore any moveable objects such as player when tracing
	// this is important to ensure a correct prediction.
	// When we run the same function but during simulation we check for dynamic also and this will flag any hit with dynamic objects,
	// and it gets mis-predicted, it will get corrected.
	QueryParams.MobilityType = bGeneratingTrajectory ? EQueryMobilityType::Static : EQueryMobilityType::Any;
	ECollisionChannel CollisionChannelToUse = bGeneratingTrajectory ? StaticOnlyCollisionChannel : CollisionChannel;
	// ToDo Gate Adding the owner to ignore list using the age?
	
	if (GetOwningAbilitySystem() && GetOwningAbilitySystem()->GetAvatarActor())
	{
		QueryParams.AddIgnoredActor(GetOwningAbilitySystem()->GetAvatarActor());
		QueryParams.AddIgnoredActor(this);
		TArray<AActor*> IgnoredActors;
		GetOwningAbilitySystem()->GetAvatarActor()->GetAttachedActors(IgnoredActors);
		QueryParams.AddIgnoredActors(IgnoredActors);
	}

	FHitResult Hit(1.f);
	// let's loop a maximum of 24 times to find first blocking actor that we don't ignore or don't phase through
	// Should Make This 24 a console variable?? 
	for (int32 i = 0; i < 24; ++i)
	{
		// ToDo : Look At Rotation Support, since velocity can become zero, we can't always rely on it for rotation.
		// if projectile stops moving change in location and velocity are zero and we don't want to snap rotation if it was following previous move
		GetWorld()->SweepSingleByChannel(Hit,MoveStart,MoveEnd,FQuat::Identity,CollisionChannelToUse
			,CollisionShape,QueryParams);
		
		if (!Hit.bBlockingHit && !Hit.bStartPenetrating)
		{
			OutputMove.Position = MoveEnd;
			DeltaMove = FVector::ZeroVector;
			LeftOverTimeMS = 0.f;
			return;
		}
		

		// we started de-penetrate if needed and return, we can't move now.
		if (Hit.bStartPenetrating)
		{
			if (Hit.PenetrationDepth > 0.f)
			{
				const FVector NewStart = MoveStart + (Hit.Normal *  (Hit.PenetrationDepth + 1.f));
				GetWorld()->SweepSingleByChannel(Hit,NewStart,MoveEnd,FQuat::Identity,CollisionChannelToUse
				,CollisionShape,QueryParams);
			}
			else
			{
				// when we are completely stuck and can't De-penetrate we explode now, is there a better option?
				OutputMove.Position = MoveStart;
				OutputMove.Velocity = FVector::ZeroVector;
				OutputMove.bExploded = true;

				FProjectileHitBroadcast BroadcastInfo;
				BroadcastInfo.MoveHitResponse = EMoveHitResponse::EExplode;
				BroadcastInfo.HitData.ProjectileAgeMS = InputStep.AgeMS + (TimeStep.DeltaTimeMs - LeftOverTimeMS);
				BroadcastInfo.HitData.ProjectileLocation = OutputMove.Position;
				
				OutputHits.Add(BroadcastInfo);

				DeltaMove = FVector::ZeroVector;
				LeftOverTimeMS = 0.f;
				return;
			}
		}

		// we are still penetrating after successful de-penetration
		if (Hit.bStartPenetrating)
		{
			// when we are completely stuck and can't De-penetrate we explode now, is there a better option?
			OutputMove.Position = MoveStart;
			OutputMove.Velocity = FVector::ZeroVector;
			OutputMove.bExploded = true;

			FProjectileHitBroadcast BroadcastInfo;
			BroadcastInfo.MoveHitResponse = EMoveHitResponse::EExplode;
			BroadcastInfo.HitData.ProjectileAgeMS = InputStep.AgeMS + (TimeStep.DeltaTimeMs - LeftOverTimeMS);
			BroadcastInfo.HitData.ProjectileLocation = OutputMove.Position;
			OutputHits.Add(BroadcastInfo);

			DeltaMove = FVector::ZeroVector;
			LeftOverTimeMS = 0.f;
			return;
		}

		if (Hit.GetActor())
		{
			QueryParams.AddIgnoredActor(Hit.GetActor());
		}

		const bool CanBounce = MaxBounces < 0 || OutputMove.CurrentBounceCount < MaxBounces;
		const EMoveHitResponse Response = GetMoveHitResponse(CanBounce,Hit);
		FProjectileHitBroadcast BroadcastInfo;
		BroadcastInfo.MoveHitResponse = Response;
		switch (Response)
		{
		case EMoveHitResponse::EIgnore:
			{
				// we don't want to do anything, if this hit says we should phase through.
				continue;
			}
		case EMoveHitResponse::EPierce:
			{
				BroadcastInfo.HitData.ProjectileAgeMS = InputStep.AgeMS + (TimeStep.DeltaTimeMs - LeftOverTimeMS) + (LeftOverTimeMS * Hit.Time);
				BroadcastInfo.HitData.ProjectileLocation = OutputMove.Position;
				BroadcastInfo.HitData.HitResult = Hit;
				OutputHits.Add(BroadcastInfo);
				// we don't want to do anything if this hit says we should phase through.
				continue;
			}
		case EMoveHitResponse::EBounce:
			{
				if (TryBounceOffHit(Hit,OutputMove,LeftOverTimeMS,DeltaMove))
				{
					BroadcastInfo.HitData.ProjectileAgeMS = InputStep.AgeMS + (TimeStep.DeltaTimeMs - LeftOverTimeMS) + (LeftOverTimeMS * Hit.Time);
					BroadcastInfo.HitData.ProjectileLocation = OutputMove.Position;
					BroadcastInfo.HitData.HitResult = Hit;
					OutputHits.Add(BroadcastInfo);
					return;
				}
				HandleBlockedMove(Hit,OutputMove,LeftOverTimeMS,DeltaMove);
				return;
			}
		case EMoveHitResponse::EBlock:
			{
				HandleBlockedMove(Hit,OutputMove,LeftOverTimeMS,DeltaMove);
				return;
			}
		case EMoveHitResponse::EExplode:
			{
				OutputMove.Position = Hit.Location;
				OutputMove.Velocity = FVector::ZeroVector;
				OutputMove.bExploded = true;
				
				// hit time = previous age + time used in previous iterations + time use in hit now 
				BroadcastInfo.HitData.ProjectileAgeMS = InputStep.AgeMS + (TimeStep.DeltaTimeMs - LeftOverTimeMS) + (LeftOverTimeMS * Hit.Time);
				BroadcastInfo.HitData.ProjectileLocation = OutputMove.Position;
				BroadcastInfo.HitData.HitResult = Hit;
				OutputHits.Add(BroadcastInfo);

				DeltaMove = FVector::ZeroVector;
				LeftOverTimeMS = 0.f;
				return;
			}
			
		}
	}
	// it could happen we go through entire loop while ignoring the hits, in that case assume nothing stops us
	LeftOverTimeMS = 0.f;
}

bool ASyncedProjectileBase::TryBounceOffHit(const FHitResult& Hit,FProjectileMove& OutputMove,float& LeftOverTimeMS,FVector& DeltaMove) const
{
	if (!Hit.IsValidBlockingHit())
	{
		DeltaMove = FVector::ZeroVector;
		LeftOverTimeMS = 0.f;
		return false;
	}
	
	FVector Normal = Hit.ImpactNormal.GetSafeNormal();

	// Reflect
	FVector IncomingVel = OutputMove.Velocity;
	FVector ReflectedVel = IncomingVel - 2.f * FVector::DotProduct(IncomingVel, Normal) * Normal;

	ReflectedVel *= BounceCoefficient;

	// Remove some tangential energy
	FVector Tangent = ReflectedVel - FVector::DotProduct(ReflectedVel, Normal) * Normal;
	ReflectedVel -= Tangent * BounceSurfaceFriction;

	if (ReflectedVel.SizeSquared() < FMath::Square(MinBounceSpeed))
	{
		OutputMove.Velocity = FVector::ZeroVector;
		OutputMove.Position = Hit.Location + Hit.ImpactNormal * 1.f;
		LeftOverTimeMS = 0.f;
		return false;
	}
	OutputMove.Velocity = ReflectedVel;
	OutputMove.Position = Hit.Location + Hit.ImpactNormal * 1.f;
	OutputMove.CurrentBounceCount++;
	LeftOverTimeMS = LeftOverTimeMS * (1 - Hit.Time);
	DeltaMove = ReflectedVel * (LeftOverTimeMS / 1000.f);
	return true;
}

void ASyncedProjectileBase::HandleBlockedMove(const FHitResult& Hit, FProjectileMove& OutputMove,float& LeftOverTimeMS,FVector& DeltaMove) const
{

	if (!Hit.IsValidBlockingHit())
	{
		DeltaMove = FVector::ZeroVector;
		LeftOverTimeMS = 0.f;
		return;
	}
	const float DeltaTime = LeftOverTimeMS / 1000.f;
    FVector Normal = Hit.ImpactNormal.GetSafeNormal();
    float NormalZ = Normal.Z;

	FVector SlideVelocity = FVector::VectorPlaneProject(OutputMove.Velocity,Normal);
	if (SlideVelocity.SizeSquared() <= FMath::Square(MinRestSpeed))
	{
		OutputMove.Velocity = FVector::ZeroVector;
		OutputMove.Position = Hit.Location;
		DeltaMove = FVector::ZeroVector;
		LeftOverTimeMS = 0.f;
		return;
	}

	// if we are on surface we consider ground
	if (NormalZ >= SlidingMaxSlope)
	{
		const float VelSize = FMath::Max(SlideVelocity.Size() - FMath::Abs(GroundFriction * SlideVelocity.Size()) * DeltaTime, 0.f);
		SlideVelocity = SlideVelocity.GetSafeNormal() * VelSize;
	}
	if (SlideVelocity.SizeSquared() <= FMath::Square(MinRestSpeed))
	{
		OutputMove.Position = Hit.Location + Hit.ImpactNormal * 1.f;
		OutputMove.Velocity = FVector::ZeroVector;
		LeftOverTimeMS = 0.f;
		return;
	}
	OutputMove.Position = Hit.Location + Hit.ImpactNormal * 1.f;
	OutputMove.Velocity = SlideVelocity;
	LeftOverTimeMS = LeftOverTimeMS * (1 - Hit.Time);
	DeltaMove = SlideVelocity * (LeftOverTimeMS / 1000.f);
}

FVector ASyncedProjectileBase::GetDesiredVelocity(const float& DeltaTime, const FProjectileStep& InputStep) const
{
	FVector NewVelocity = InputStep.Move.Velocity;

	// Apply gravity
	NewVelocity -= FVector(0.f, 0.f, Gravity) * DeltaTime;

	// Apply drag
	NewVelocity *= 1.f / (1.f + FMath::Max(Drag,0.f) * DeltaTime);

	return NewVelocity;
}

EMoveHitResponse ASyncedProjectileBase::GetMoveHitResponse(const bool& CanBounce, const FHitResult& Hit)
{
	// by default pawn hit makes projectile explode.
	if (Hit.GetActor() && Hit.GetActor()->IsA(APawn::StaticClass()))
	{
		return EMoveHitResponse::EExplode;
	}
	// otherwise try bounce and explode if we can't bounce anymore
	if (CanBounce)
	{
		return EMoveHitResponse::EBounce;
	}
	return EMoveHitResponse::EExplode;
}

void ASyncedProjectileBase::RestoreProjectile(const FProjectileData& AuthorityData , const float& DeltaTimeMs)
{
	JustRestoredFrame = true;
	// if Same Data , no need to do anything.
	if (ProjectileData == AuthorityData)
	{
		ProjectileData = AuthorityData;
		return;
	}
	check(Trajectory.Trajectory.Num() > 0)
	FProjectileStep OverrideStep;
	const int32 OverrideIndex = Trajectory.GetEntryByServerFrame(AuthorityData.LastTrajectoryChangeFrame,OverrideStep);
	
	// if the last relevant data we received matched what's in our trajectory we have nothing to restore no need to regenerate trajectory.
	if (AuthorityData.LastRelevantLocation.Equals(OverrideStep.Move.Position,4.f)
		&& AuthorityData.LastRelevantVelocity.Equals(OverrideStep.Move.Velocity,4.f)
		&& AuthorityData.BouncesAtLastTrajectoryChange == OverrideStep.Move.CurrentBounceCount
		&& AuthorityData.bExploded == OverrideStep.Move.bExploded
		&& AuthorityData.LastTrajectoryChangeFrame == ProjectileData.LastTrajectoryChangeFrame
		&& AuthorityData.SpawnFrame == ProjectileData.SpawnFrame)
	{
		ProjectileData = AuthorityData;
		return;
	}
	
	// Regenerate the trajectory from this point to the future without touching the past.
	OverrideStep.ServerFrame = AuthorityData.LastTrajectoryChangeFrame;
	OverrideStep.Move.CurrentBounceCount = AuthorityData.BouncesAtLastTrajectoryChange;
	OverrideStep.Move.Position = AuthorityData.LastRelevantLocation;
	OverrideStep.Move.Velocity = AuthorityData.LastRelevantVelocity;
	OverrideStep.AgeMS = (AuthorityData.LastTrajectoryChangeFrame - AuthorityData.SpawnFrame) * DeltaTimeMs;
	OverrideStep.Move.bExploded = AuthorityData.bExploded;

	FTrajectoryGenerationInputs GenerationInputs;
	GenerationInputs.DeltaTimeMS = DeltaTimeMs;
	GenerationInputs.LifeTimeMS = FMath::Floor(MaxLifeTime * 1000.f);
	GenerateTrajectory(GenerationInputs,OverrideStep,OverrideIndex);
	ProjectileData = AuthorityData;
}

void ASyncedProjectileBase::SimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	uint32 ServerFrame = FMath::Max(0,TimeStep.ServerFrame);
	if (ProjectileData.bExploded || ServerFrame <= ProjectileData.SpawnFrame || !OwningSimulator)
	{
		// this projectile Has reached End of Life And Did What it needs to do, could still be alive 
		return;
	}
	
	FProjectileStep PrevTrajectoryPoint;
	Trajectory.GetEntryByServerFrame(TimeStep.ServerFrame - 1, PrevTrajectoryPoint);
	//Allow Location and velocity to be effected by outside factor (ToDo : using apply force and teleport projectile.)
	if (JustRestoredFrame)
	{
		ProjectileLocation = PrevTrajectoryPoint.Move.Position;
		ProjectileVelocity = PrevTrajectoryPoint.Move.Velocity;
		JustRestoredFrame = false;
	}
	else
	{
		PrevTrajectoryPoint.Move.Position = ProjectileLocation;
		PrevTrajectoryPoint.Move.Velocity = ProjectileVelocity;
	}
	
	const float MaxLifeTimeMS = FMath::RoundToInt(FMath::Min(MaxLifeTime,10.f) * 1000.f);
	const float CurrentLifeTimeMS = (TimeStep.ServerFrame - ProjectileData.SpawnFrame) * TimeStep.StepMs;
	if ( CurrentLifeTimeMS >= MaxLifeTimeMS)
	{
		ProjectileData.bExploded = true;
		ProjectileData.LastRelevantLocation = PrevTrajectoryPoint.Move.Position;
		ProjectileData.LastRelevantVelocity = PrevTrajectoryPoint.Move.Velocity;
		ProjectileData.LastTrajectoryChangeFrame = PrevTrajectoryPoint.ServerFrame;
		ProjectileData.BouncesAtLastTrajectoryChange = PrevTrajectoryPoint.Move.CurrentBounceCount;
		
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = ProjectileData.LastRelevantLocation;
		HitBroadcastData.ProjectileAgeMS = MaxLifeTimeMS;
		HitBroadcastData.CurrentBounceCount = ProjectileData.BouncesAtLastTrajectoryChange;
		OnEndOfLife.Broadcast(HitBroadcastData);
		OwningSimulator->OnProjectileEndOfLife.Broadcast(HitBroadcastData,ProjectileData.ProjectileID);
		if (BroadcastExplodedOnEndOfLife)
		{
			OnExplode.Broadcast(HitBroadcastData);
			OwningSimulator->OnProjectileExplode.Broadcast(HitBroadcastData,ProjectileData.ProjectileID);
		}
		return;
	}
	
	FProjectileStep CurrentTrajectoryPoint;
	const int32 CurrentTrajectoryIndex = Trajectory.GetEntryByServerFrame(TimeStep.ServerFrame,CurrentTrajectoryPoint);
	check(CurrentTrajectoryPoint.ServerFrame == TimeStep.ServerFrame)
	// Just like we did during trajectory generation, we will now propose and trace the movement for this frame and this will allow for correction
	// to happen , if a hit/ checkpoint is reached now but doesn't match client motion at that frame , we correct and regenerate trajectory.
	
	FProjectileMoveTimeStep MoveTimeStep;
	MoveTimeStep.ServerFrame = TimeStep.ServerFrame;
	MoveTimeStep.DeltaTimeMs = TimeStep.StepMs;
	TArray<FProjectileHitBroadcast> BroadcastingHits;
	// Set New Move To previous move just as we do in GenerateTrajectory
	FProjectileMove NewMove = PrevTrajectoryPoint.Move;
	MoveProjectile(false,MoveTimeStep,PrevTrajectoryPoint,NewMove,BroadcastingHits);
	if (BroadcastingHits.Num() > 0)
	{
		for (const auto& BroadcastingHit : BroadcastingHits)
		{
			switch (BroadcastingHit.MoveHitResponse)
			{
			case EMoveHitResponse::EExplode:
				{
					OnExplode.Broadcast(BroadcastingHit.HitData);
					OwningSimulator->OnProjectileExplode.Broadcast(BroadcastingHit.HitData,ProjectileData.ProjectileID);
					break;
				}
			case EMoveHitResponse::EBounce:
				{
					OnBounce.Broadcast(BroadcastingHit.HitData);
					OwningSimulator->OnProjectileBounce.Broadcast(BroadcastingHit.HitData,ProjectileData.ProjectileID);
					break;
				}
			case EMoveHitResponse::EPierce:
				{
					OnPierce.Broadcast(BroadcastingHit.HitData);
					OwningSimulator->OnProjectilePierce.Broadcast(BroadcastingHit.HitData,ProjectileData.ProjectileID);
					break;
				}
			case EMoveHitResponse::EIgnore:
			case EMoveHitResponse::EBlock:
				{
					break;
				}
			}
		}
	}

	// Compress Loc And velocity to match serialization.
	NewMove.Position.X = (FMath::RoundToInt32(NewMove.Position.X * 10)) / 10.f;
	NewMove.Position.Y = (FMath::RoundToInt32(NewMove.Position.Y * 10)) / 10.f;
	NewMove.Position.Z = (FMath::RoundToInt32(NewMove.Position.Z * 10)) / 10.f;
		
	NewMove.Velocity.X = (FMath::RoundToInt32(NewMove.Velocity.X * 10)) / 10.f;
	NewMove.Velocity.Y = (FMath::RoundToInt32(NewMove.Velocity.Y * 10)) / 10.f;
	NewMove.Velocity.Z = (FMath::RoundToInt32(NewMove.Velocity.Z * 10)) / 10.f;
	
	// a change in the trajectory occured or we exploded, regenerate trajectory and force this as a checkpoint
	if (NewMove != CurrentTrajectoryPoint.Move || NewMove.bExploded)
	{
		CurrentTrajectoryPoint.Move = NewMove;
		ProjectileData.LastRelevantLocation = CurrentTrajectoryPoint.Move.Position;
		ProjectileData.LastRelevantVelocity = CurrentTrajectoryPoint.Move.Velocity;
		ProjectileData.LastTrajectoryChangeFrame = CurrentTrajectoryPoint.ServerFrame;
		ProjectileData.BouncesAtLastTrajectoryChange = CurrentTrajectoryPoint.Move.CurrentBounceCount;
		ProjectileData.bExploded = CurrentTrajectoryPoint.Move.bExploded;
		FTrajectoryGenerationInputs GenerationInputs;
		GenerationInputs.DeltaTimeMS = TimeStep.StepMs;
		GenerationInputs.LifeTimeMS = FMath::Floor(MaxLifeTime * 1000.f);
		GenerateTrajectory(GenerationInputs,CurrentTrajectoryPoint,CurrentTrajectoryIndex);
	}
	
	// Set Our Local Variables To use them next tick
	ProjectileLocation = CurrentTrajectoryPoint.Move.Position;
	ProjectileVelocity = CurrentTrajectoryPoint.Move.Velocity;
	
	
	const FProjectileStep& PreviousStep = Trajectory.Trajectory[FMath::Max(CurrentTrajectoryIndex - 1,0)];
	const FVector MoveOffset = CurrentTrajectoryPoint.Move.Position - PreviousStep.Move.Position;
	const FVector Velocity = MoveOffset / (TimeStep.StepMs / 1000.f);
	const FVector Direction = MoveOffset.GetSafeNormal();
	const FRotator TargetRotation = MoveOffset.IsNearlyZero() ? GetRootComponent()->GetComponentRotation() : Direction.ToOrientationRotator();
	const FTransform TargetTransform = FTransform(TargetRotation,ProjectileLocation);
	ProjectileRotation = TargetRotation.GetNormalized();
	// no need to update location during resim, has no effect at all just perf cost
	if (UpdateRootComponentLocation && !TimeStep.bIsResimulating)
	{
		GetRootComponent()->SetWorldTransform(TargetTransform,false,nullptr,ETeleportType::TeleportPhysics);
		GetRootComponent()->ComponentVelocity = Velocity;
	}
}

void ASyncedProjectileBase::FinalizeFrame(const float& RenderTimeMS,const FProjectileData& FinalizeData)
{
	//ToDo : Compare Against Last Finalize and trigger visual events??
	
	if (UpdateVisualComponentLocation && VisualComponent)
	{
		const float RenderAge = RenderTimeMS - (FinalizeData.SpawnFrame * GetFixedStepMS());
		FProjectileStep CurrentStep = Trajectory.GetEntryByAge(RenderAge);
		const FProjectileStep& PreviousStep = Trajectory.GetEntryByAge(RenderAge - GetFixedStepMS());
		const FVector MoveOffset = CurrentStep.Move.Position - PreviousStep.Move.Position;
		const FVector Direction = MoveOffset.GetSafeNormal();
		FQuat OldRotation = VisualComponent->GetComponentTransform().GetRotation();
		OldRotation = BaseVisualCompTransform.GetRotation().Inverse() * OldRotation;
		const FRotator TargetRotation = MoveOffset.IsNearlyZero() ?
			OldRotation.Rotator() : Direction.ToOrientationRotator();
		const FTransform TargetTransform = FTransform(TargetRotation,CurrentStep.Move.Position);
		VisualComponent->SetWorldTransform(BaseVisualCompTransform * TargetTransform,false,nullptr,ETeleportType::TeleportPhysics);
	}

	if (!LastFinalizeProjectileData.bExploded && FinalizeData.bExploded)
	{
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = ProjectileData.LastRelevantLocation;
		HitBroadcastData.ProjectileAgeMS = (ProjectileData.LastTrajectoryChangeFrame - ProjectileData.SpawnFrame) * GetFixedStepMS();
		HitBroadcastData.CurrentBounceCount = ProjectileData.BouncesAtLastTrajectoryChange;
		OnVisualExplode.Broadcast(HitBroadcastData);
	}
	
	LastFinalizeProjectileData = FinalizeData;
}

void ASyncedProjectileBase::FinalizeInterpolatedFrame(const float& RenderTimeMS, const FProjectileData& FinalizeData)
{
	const float FixedStepMS = GetFixedStepMS();
	//1- if projectile has exploded in new state but not ours, broadcast the explosion delegate.
	//ToDo : this maybe shouldn't happen?? explosion and its VFX, SFX etc.. should be in a cue. 
	if (FinalizeData.bExploded && !ProjectileData.bExploded)
	{
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = FinalizeData.LastRelevantLocation;
		HitBroadcastData.ProjectileAgeMS = FinalizeData.LastTrajectoryChangeFrame * FixedStepMS;
		HitBroadcastData.CurrentBounceCount = FinalizeData.BouncesAtLastTrajectoryChange;
		OnExplode.Broadcast(HitBroadcastData);
	}
	if (FinalizeData.BouncesAtLastTrajectoryChange != ProjectileData.BouncesAtLastTrajectoryChange)
	{
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = FinalizeData.LastRelevantLocation;
		HitBroadcastData.ProjectileAgeMS = FinalizeData.LastTrajectoryChangeFrame * FixedStepMS;
		HitBroadcastData.CurrentBounceCount = FinalizeData.BouncesAtLastTrajectoryChange;
		OnBounce.Broadcast(HitBroadcastData);
	}
	//ToDo : Call On Pierce After Adding The Pierce Count Just Like Bounce
	
	FProjectileStep OverrideStep;
	const int32 OverrideIndex = Trajectory.GetEntryByServerFrame(FinalizeData.LastTrajectoryChangeFrame,OverrideStep);
	// if the last relevant data we received matched what's in our trajectory we have nothing to restore no need to regenerate trajectory.
	if (!FinalizeData.LastRelevantLocation.Equals(OverrideStep.Move.Position,4.f)
		|| !FinalizeData.LastRelevantVelocity.Equals(OverrideStep.Move.Velocity,4.f)
		|| FinalizeData.BouncesAtLastTrajectoryChange != OverrideStep.Move.CurrentBounceCount
		|| FinalizeData.bExploded != OverrideStep.Move.bExploded
		|| FinalizeData.LastTrajectoryChangeFrame != ProjectileData.LastTrajectoryChangeFrame
		|| FinalizeData.SpawnFrame != ProjectileData.SpawnFrame)
	{
		OverrideStep.ServerFrame = FinalizeData.LastTrajectoryChangeFrame;
		OverrideStep.Move.CurrentBounceCount = FinalizeData.BouncesAtLastTrajectoryChange;
		OverrideStep.Move.Position = FinalizeData.LastRelevantLocation;
		OverrideStep.Move.Velocity = FinalizeData.LastRelevantVelocity;
		OverrideStep.AgeMS = (FinalizeData.LastTrajectoryChangeFrame - FinalizeData.SpawnFrame) * FixedStepMS;
		OverrideStep.Move.bExploded = FinalizeData.bExploded;

		FTrajectoryGenerationInputs GenerationInputs;
		GenerationInputs.DeltaTimeMS = FixedStepMS;
		GenerationInputs.LifeTimeMS = FMath::Floor(MaxLifeTime * 1000.f);
		GenerateTrajectory(GenerationInputs,OverrideStep,OverrideIndex);
	}
	//3- set current location based on trajectory and render time ms.
	const float RenderAge = RenderTimeMS - (FinalizeData.SpawnFrame * FixedStepMS);
	const FProjectileStep& CurrentStep = Trajectory.GetEntryByAge(RenderAge);
	const FProjectileStep& PreviousStep = Trajectory.GetEntryByAge(RenderAge - FixedStepMS);
	const FVector MoveOffset = CurrentStep.Move.Position - PreviousStep.Move.Position;
	const FVector Velocity = MoveOffset / (FixedStepMS / 1000.f);
	const FVector Direction = MoveOffset.GetSafeNormal();
	if (CurrentStep.AgeMS >= MaxLifeTime * 1000.f)
	{
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = CurrentStep.Move.Position;
		HitBroadcastData.ProjectileAgeMS = MaxLifeTime * 1000.f;
		HitBroadcastData.CurrentBounceCount = ProjectileData.BouncesAtLastTrajectoryChange;
		OnEndOfLife.Broadcast(HitBroadcastData);
		if (BroadcastExplodedOnEndOfLife)
		{
			OnExplode.Broadcast(HitBroadcastData);
		}
	}
	

	const FRotator TargetRotation = MoveOffset.IsNearlyZero() ? GetRootComponent()->GetComponentRotation() : Direction.ToOrientationRotator();
	if (UpdateRootComponentLocation)
	{
		const FTransform TargetTransform = FTransform(TargetRotation,CurrentStep.Move.Position);
		GetRootComponent()->SetWorldTransform(TargetTransform,false,nullptr,ETeleportType::TeleportPhysics);
	}
	else if (UpdateVisualComponentLocation && VisualComponent)
	{
		// keep rotation as it is if offset is zero
		FQuat OldRotation = VisualComponent->GetComponentTransform().GetRotation();
		OldRotation = BaseVisualCompTransform.GetRotation().Inverse() * OldRotation;
		const FRotator TargetVisRotation = MoveOffset.IsNearlyZero() ?
			OldRotation.Rotator() : Direction.ToOrientationRotator();
		const FTransform TargetTransform = FTransform(TargetVisRotation,CurrentStep.Move.Position);
		VisualComponent->SetWorldTransform(BaseVisualCompTransform * TargetTransform,false,nullptr,ETeleportType::TeleportPhysics);
	}
	ProjectileData = FinalizeData;
	ProjectileLocation = CurrentStep.Move.Position;
	ProjectileVelocity = Velocity;
	ProjectileRotation = TargetRotation.GetNormalized();
}

void ASyncedProjectileBase::InitializeProjectile(const float& ServerFrame , const float& StepTimeMS,const uint32& ProjectileID
                                                 ,const FVector& StartLocation, const FVector& StartDirection)
{
	ProjectileLocation = StartLocation;
	ProjectileVelocity = StartDirection * InitialSpeed;

	// Compress Starting Loc And velocity to match serialization.
	ProjectileLocation.X = (FMath::RoundToInt32(ProjectileLocation.X * 10)) / 10.f;
	ProjectileLocation.Y = (FMath::RoundToInt32(ProjectileLocation.Y * 10)) / 10.f;
	ProjectileLocation.Z = (FMath::RoundToInt32(ProjectileLocation.Z * 10)) / 10.f;
		
	ProjectileVelocity.X = (FMath::RoundToInt32(ProjectileVelocity.X * 10)) / 10.f;
	ProjectileVelocity.Y = (FMath::RoundToInt32(ProjectileVelocity.Y * 10)) / 10.f;
	ProjectileVelocity.Z = (FMath::RoundToInt32(ProjectileVelocity.Z * 10)) / 10.f;


	ProjectileData.ProjectileID = ProjectileID;
	ProjectileData.SpawnFrame = ServerFrame;
	ProjectileData.LastTrajectoryChangeFrame = ServerFrame;
	ProjectileData.LastRelevantLocation = ProjectileLocation;
	ProjectileData.LastRelevantVelocity = ProjectileVelocity;
	ProjectileData.BouncesAtLastTrajectoryChange = 0;
	ProjectileData.bExploded = false;
	
	FProjectileStep StartState;
	StartState.ServerFrame = ServerFrame;
	StartState.AgeMS = 0.f;
	StartState.Move.bExploded = false;
	StartState.Move.Position = ProjectileLocation;
	StartState.Move.Velocity = ProjectileVelocity;

	FTrajectoryGenerationInputs GenerationInputs;
	GenerationInputs.LifeTimeMS = FMath::Floor(MaxLifeTime * 1000.f);
	GenerationInputs.DeltaTimeMS = StepTimeMS;
	GenerateTrajectory(GenerationInputs,StartState);
	FVector Size = BoxHalfExtent * 2;
	if (ProjectileCollisionShape == EProjectileCollisionShape::ESphere)
	{
		Size.X = SphereRadius;
		Size.Y = SphereRadius;
		Size.Z = SphereRadius;
	}
	if (DrawDebugTrajectoryOnSpawn)
	{
		Trajectory.DrawFullTrajectory(GetWorld(),MaxLifeTime,ProjectileCollisionShape,Size,DebugMeshComponent);
	}
	

	LastFinalizeProjectileData = ProjectileData;
}

void ASyncedProjectileBase::ForceInitializeProjectile(const FProjectileData& AuthorityData, const float& DeltaTimeMs)
{
	ProjectileLocation = AuthorityData.LastRelevantLocation;
	ProjectileVelocity = AuthorityData.LastRelevantVelocity;

	// Compress Starting Loc And velocity to match serialization.
	ProjectileLocation.X = (FMath::RoundToInt32(ProjectileLocation.X * 10)) / 10.f;
	ProjectileLocation.Y = (FMath::RoundToInt32(ProjectileLocation.Y * 10)) / 10.f;
	ProjectileLocation.Z = (FMath::RoundToInt32(ProjectileLocation.Z * 10)) / 10.f;
		
	ProjectileVelocity.X = (FMath::RoundToInt32(ProjectileVelocity.X * 10)) / 10.f;
	ProjectileVelocity.Y = (FMath::RoundToInt32(ProjectileVelocity.Y * 10)) / 10.f;
	ProjectileVelocity.Z = (FMath::RoundToInt32(ProjectileVelocity.Z * 10)) / 10.f;


	ProjectileData = AuthorityData;

	// we are initializing into an exploded state already, this means the projectile spawned and exploded on the same frame, it's possible
	if (AuthorityData.bExploded)
	{
		FHitBroadcastData HitBroadcastData;
		HitBroadcastData.ProjectileLocation = AuthorityData.LastRelevantLocation;
		HitBroadcastData.ProjectileAgeMS = AuthorityData.LastTrajectoryChangeFrame * GetFixedStepMS();
		HitBroadcastData.CurrentBounceCount = AuthorityData.BouncesAtLastTrajectoryChange;
		OnExplode.Broadcast(HitBroadcastData);
		OwningSimulator->OnProjectileExplode.Broadcast(HitBroadcastData,AuthorityData.ProjectileID);
	}
	
	FProjectileStep StartState;
	StartState.ServerFrame = ProjectileData.LastTrajectoryChangeFrame;
	StartState.AgeMS = (ProjectileData.LastTrajectoryChangeFrame - ProjectileData.SpawnFrame) * DeltaTimeMs;
	StartState.Move.bExploded = ProjectileData.bExploded;
	StartState.Move.Position = ProjectileLocation;
	StartState.Move.Velocity = ProjectileVelocity;

	FTrajectoryGenerationInputs GenerationInputs;
	GenerationInputs.LifeTimeMS = FMath::Floor(MaxLifeTime * 1000.f);
	GenerationInputs.DeltaTimeMS = DeltaTimeMs;
	GenerateTrajectory(GenerationInputs,StartState);
	FVector Size = BoxHalfExtent * 2;
	if (ProjectileCollisionShape == EProjectileCollisionShape::ESphere)
	{
		Size.X = SphereRadius;
		Size.Y = SphereRadius;
		Size.Z = SphereRadius;
	}
	if (DrawDebugTrajectoryOnSpawn)
	{
		Trajectory.DrawFullTrajectory(GetWorld(),MaxLifeTime,ProjectileCollisionShape,Size,DebugMeshComponent);
	}
	

	LastFinalizeProjectileData = ProjectileData;
}

void ASyncedProjectileBase::GenerateTrajectory(const FTrajectoryGenerationInputs& GenerationInputs,const FProjectileStep& StartingState,
	int32 OverrideIndex)
{

	const int32 StartingIndex = FMath::Max(OverrideIndex,0);
	const float ActualLifeTime = GenerationInputs.LifeTimeMS - StartingState.AgeMS;
	if (ActualLifeTime - KINDA_SMALL_NUMBER <= 0.f || FMath::IsNearlyZero(GenerationInputs.DeltaTimeMS))
	{
		
		return;
	}
	const int32 LifeTimeFrames = FMath::CeilToInt32(ActualLifeTime / GenerationInputs.DeltaTimeMS) + 1;
	Trajectory.Trajectory.SetNum(StartingIndex + 1 + LifeTimeFrames);
	Trajectory.Trajectory[StartingIndex] = StartingState;
	
	
	FProjectileMoveTimeStep TimeStep;
	TimeStep.DeltaTimeMs = GenerationInputs.DeltaTimeMS;
	TimeStep.ServerFrame = StartingState.ServerFrame;

	FProjectileStep InputStep = StartingState;
	FProjectileStep OutputStep = StartingState;
	
	for (int32 i = StartingIndex + 1 ; i < Trajectory.Trajectory.Num(); ++i)
	{
		TArray<FProjectileHitBroadcast> BroadcastingHits;
		TimeStep.ServerFrame++;
		OutputStep.ServerFrame = TimeStep.ServerFrame;
		OutputStep.AgeMS += TimeStep.DeltaTimeMs;
		MoveProjectile(true,TimeStep,InputStep,OutputStep.Move,BroadcastingHits);
		
		// Compress Starting Loc And velocity to match serialization.
		OutputStep.Move.Position.X = (FMath::RoundToInt32(OutputStep.Move.Position.X * 10)) / 10.f;
		OutputStep.Move.Position.Y = (FMath::RoundToInt32(OutputStep.Move.Position.Y * 10)) / 10.f;
		OutputStep.Move.Position.Z = (FMath::RoundToInt32(OutputStep.Move.Position.Z * 10)) / 10.f;
		
		OutputStep.Move.Velocity.X = (FMath::RoundToInt32(OutputStep.Move.Velocity.X * 10)) / 10.f;
		OutputStep.Move.Velocity.Y = (FMath::RoundToInt32(OutputStep.Move.Velocity.Y * 10)) / 10.f;
		OutputStep.Move.Velocity.Z = (FMath::RoundToInt32(OutputStep.Move.Velocity.Z * 10)) / 10.f;
		
		Trajectory.Trajectory[i] = OutputStep;
		// copy output to be used for next iteration input state
		InputStep = OutputStep;
	}
	int32 VisualOverrideIndex = 0;
	
	const float RemainingLifeSimTime = StartingState.Move.bExploded ? 0 : ActualLifeTime / 1000.f;
	OnTrajectoryUpdated.Broadcast(Trajectory,RemainingLifeSimTime);

	const float RenderAgeMs = GetProjectileRenderAge();
	const FProjectileStep OverrideStep = Trajectory.GetEntryByAgeWithIndex(RenderAgeMs,VisualOverrideIndex);
	VisualTrajectory.UpdateFomSimTrajectory(Trajectory,OverrideStep,VisualOverrideIndex);
	const float RemainingRenderTime = OverrideStep.Move.bExploded ? 0.f :(GenerationInputs.LifeTimeMS - RenderAgeMs) / 1000.f;
	OnVisualTrajectoryUpdated.Broadcast(VisualTrajectory,RemainingRenderTime);
}

void ASyncedProjectileBase::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	TArray<UActorComponent*> VisualComponents = GetComponentsByTag(UMeshComponent::StaticClass(),"VisualMesh");
	if (VisualComponents.Num() > 0)
	{
		VisualComponent = Cast<UMeshComponent>(VisualComponents[0]);
		BaseVisualCompTransform = VisualComponent->GetRelativeTransform();
	}
	DebugMeshComponent = GetComponentByClass<UInstancedStaticMeshComponent>();
	if (DebugMeshComponent)
	{
		DebugMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	}
}

float ASyncedProjectileBase::GetFixedStepMS() const
{
	if (UNetworkPredictionWorldManager* NPManage = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>())
	{
		return NPManage->GetFixedTickState().FixedStepMS;
	}
	return 0.f;
}

float ASyncedProjectileBase::GetProjectileRenderAge()
{
	if (!OwningSimulator)
	{
		return 0.f;
	}
	return FMath::Max(OwningSimulator->GetProjectilesSimRenderTimeMS() - (ProjectileData.SpawnFrame * GetFixedStepMS()),0.f);
}






