// Fill out your copyright notice in the Description page of Project Settings.


#include "Library/TargetingLibrary.h"

#include "KismetTraceUtils.h"
#include "NetworkPredictionWorldManager.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Targeting/TargetingProcessor.h"

DEFINE_LOG_CATEGORY(LogTargetingLibrary);

void UTargetingLibrary::AddTargetDataToHandleFromHitResult(FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,
                                                           const FHitResult& HitResult)
{
	TArray<FHitResult> HitResults;
	HitResults.Add(HitResult);
	AddTargetDataToHandleFromHitResults(Handle,OriginTransform, HitResults);
}

void UTargetingLibrary::AddTargetDataToHandleFromHitResults(FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,
	const TArray<FHitResult>& HitResults)
{
	for (int32 i = 0; i < HitResults.Num(); i++)
	{
		FGameplayAbilityTargetData_SingleTargetHit* ReturnData = new FGameplayAbilityTargetData_SingleTargetHit();
		ReturnData->HitResult = HitResults[i];
		Handle.Add(ReturnData);
	}
}

void UTargetingLibrary::AddTargetDataToHandleFromActors(FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,
	const TArray<AActor*>& TargetActors,bool OneActorPerHandle)
{
	FGameplayAbilityTargetingLocationInfo LocationInfo;
	LocationInfo.LocationType = EGameplayAbilityTargetingLocationType::LiteralTransform;
	LocationInfo.LiteralTransform = OriginTransform;

	FGameplayAbilityTargetData_ActorArray* ReturnData = new FGameplayAbilityTargetData_ActorArray();
	Handle.Add(ReturnData);
	ReturnData->SourceLocation = LocationInfo;
	if (OneActorPerHandle)
	{
		if (TargetActors.Num() > 0)
		{
			if (AActor* TargetActor = TargetActors[0])
			{
				ReturnData->TargetActorArray.Add(TargetActor);
			}

			for (int32 i = 1; i < TargetActors.Num(); ++i)
			{
				if (AActor* TargetActor = TargetActors[i])
				{
					FGameplayAbilityTargetData_ActorArray* CurrentData = new FGameplayAbilityTargetData_ActorArray();
					CurrentData->SourceLocation = LocationInfo;
					CurrentData->TargetActorArray.Add(TargetActor);
					Handle.Add(CurrentData);
				}
			}
		}
	}
	else
	{
		for (AActor* TargetActor : TargetActors)
		{
			ReturnData->TargetActorArray.Add(TargetActor);
		}
	}
}

bool UTargetingLibrary::PerformTargetingFromProcessor(bool bEnableLagCompensation,
	UTargetingProcessor* Processor, UNpAbilitySystemComponent* OwnerASC,FTargetingData TargetingInputData
	,FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
	if (!OwnerASC)
	{
		UE_LOG(LogTargetingLibrary,Error,TEXT(" Calling PerformTargetingFromProcessor without a Owning Ability system"));
		return false;
	}
	if (!Processor)
	{
		UE_LOG(LogTargetingLibrary,Error,TEXT(" %s Calling PerformTargetingFromProcessor without a valid processor")
			,*GetNameSafe(OwnerASC));
		return false;
	}
	UNetworkPredictionWorldManager* NpManager = OwnerASC->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>();
	bool bDidRewind = false;
	if (bEnableLagCompensation && NpManager && OwnerASC->GetAvatarActor())
	{
		bDidRewind = NpManager->RewindActors(OwnerASC->GetAvatarActor(),OwnerASC->GetSyncedInterpolationTimeMS());
	}
	
	// since instant targeting is just confirmation we pass -1 as confirmation time to indicate this is first one,
	const ETargetingResult Result = Processor->ConfirmTargeting(OwnerASC,0.f,-1.f,TargetingInputData,TargetDataHandle);
	if (bDidRewind)
	{
		NpManager->UnwindActors();
	}
	return Result == ETargetingResult::ESuccess || Result == ETargetingResult::ESuccessOnGoing;
}


bool UTargetingLibrary::ProjectWorldLocToScreen(const FVector& WorldLoc ,const FRotator& ControlRotation,const FVector& CameraLocation,const FSyncedScreenProjection& ScreenProjectionData,FVector2D& ScreenLoc)
{
	FMatrix ScreenMatrix = FSyncedScreenProjection::BuildScreenProjectionMatrix(ScreenProjectionData,ControlRotation,CameraLocation);
	FPlane Result = ScreenMatrix.TransformFVector4(FVector4(WorldLoc, 1.f));
	if ( Result.W > 0.0f )
	{
		// the result of this will be x and y coords in -1..1 projection space
		const float RHW = 1.0f / Result.W;
		const FPlane PosInScreenSpace = FPlane(Result.X * RHW, Result.Y * RHW, Result.Z * RHW, Result.W);
		// Move from projection space to normalized 0..1 UI space
		const float NormalizedX = ( PosInScreenSpace.X / 2.f ) + 0.5f;
		const float NormalizedY = 1.f - ( PosInScreenSpace.Y / 2.f ) - 0.5f;
		const FVector2D RayStartViewRectSpace((NormalizedX * ScreenProjectionData.ViewSize.X),(NormalizedY * ScreenProjectionData.ViewSize.Y));
		ScreenLoc = RayStartViewRectSpace + FVector2D(ScreenProjectionData.ViewMin.X,ScreenProjectionData.ViewMin.Y);
		return true;
	}

	return false;
}

bool UTargetingLibrary::DeProjectScreenToWorld(const FVector& ScreenLoc ,const FRotator& ControlRotation,const FVector& CameraLocation
	,const FSyncedScreenProjection& ScreenProjectionData,FVector& OutWorldLoc, FVector& OutDirection)
{
	FMatrix ViewProjMatrix = FSyncedScreenProjection::BuildScreenProjectionMatrix(ScreenProjectionData,ControlRotation,CameraLocation).InverseFast();
	
	float PixelX = FMath::TruncToFloat(ScreenLoc.X);
	float PixelY = FMath::TruncToFloat(ScreenLoc.Y);

	// Get the eye position and direction of the mouse cursor in two stages (inverse transform projection, then inverse transform view).
	// This avoids the numerical instability that occurs when a view matrix with large translation is composed with a projection matrix

	// Get the pixel coordinates into 0..1 normalized coordinates within the constrained view rectangle
	const float NormalizedX = (PixelX - ScreenProjectionData.ViewMin.X) / ScreenProjectionData.ViewSize.X;
	const float NormalizedY = (PixelY - ScreenProjectionData.ViewMin.Y) / ScreenProjectionData.ViewSize.Y;

	// Get the pixel coordinates into -1..1 projection space
	const float ScreenSpaceX = (NormalizedX - 0.5f) * 2.0f;
	const float ScreenSpaceY = ((1.0f - NormalizedY) - 0.5f) * 2.0f;

	// The start of the ray trace is defined to be at mousex,mousey,1 in projection space (z=1 is near, z=0 is far - this gives us better precision)
	// To get the direction of the ray trace we need to use any z between the near and the far plane, so let's use (mousex, mousey, 0.01)
	const FVector4 RayStartProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 1.0f, 1.0f);
	const FVector4 RayEndProjectionSpace = FVector4(ScreenSpaceX, ScreenSpaceY, 0.01f, 1.0f);

	// Projection (changing the W coordinate) is not handled by the FMatrix transforms that work with vectors, so multiplications
	// by the projection matrix should use homogeneous coordinates (i.e. FPlane).
	const FVector4 HGRayStartWorldSpace = ViewProjMatrix.TransformFVector4(RayStartProjectionSpace);
	const FVector4 HGRayEndWorldSpace = ViewProjMatrix.TransformFVector4(RayEndProjectionSpace);
	FVector RayStartWorldSpace(HGRayStartWorldSpace.X, HGRayStartWorldSpace.Y, HGRayStartWorldSpace.Z);
	FVector RayEndWorldSpace(HGRayEndWorldSpace.X, HGRayEndWorldSpace.Y, HGRayEndWorldSpace.Z);
	// divide vectors by W to undo any projection and get the 3-space coordinate
	if (HGRayStartWorldSpace.W != 0.0f)
	{
		RayStartWorldSpace /= HGRayStartWorldSpace.W;
	}
	if (HGRayEndWorldSpace.W != 0.0f)
	{
		RayEndWorldSpace /= HGRayEndWorldSpace.W;
	}
	const FVector RayDirWorldSpace = (RayEndWorldSpace - RayStartWorldSpace).GetSafeNormal();

	// Finally, store the results in the outputs
	OutWorldLoc = RayStartWorldSpace;
	OutDirection = RayDirWorldSpace;

	return false;
}

bool UTargetingLibrary::GenerateAIScreenProjection(AActor* AIActor, FSceneViewProjectionData& ProjectionData,
	int32 ScreenWidth, int32 ScreenHeight)
{
	// Validate the input
	if (!AIActor)
	{
		return false;
	}

	// Get AI location and add half height to Z (eye level approximation)
	FVector AILocation = AIActor->GetActorLocation();
	float HalfHeight = AIActor->GetSimpleCollisionHalfHeight();
	AILocation.Z += HalfHeight;

	// Get AI rotation
	FRotator AIRotation = AIActor->GetActorRotation();

	// Set up a fake viewport rectangle
	int32 X = 0;
	int32 Y = 0;
	uint32 SizeX = ScreenWidth;
	uint32 SizeY = ScreenHeight;
	FIntRect ViewRect = FIntRect(X, Y, X + SizeX, Y + SizeY);
	ProjectionData.SetViewRectangle(ViewRect);

	// Set the view origin to AI's location
	ProjectionData.ViewOrigin = AILocation;

	// Create the view rotation matrix
	ProjectionData.ViewRotationMatrix = FInverseRotationMatrix(AIRotation) * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// Set up a projection matrix with reasonable defaults for an AI's "camera"
	float FOV = 90.0f; // A default FOV value, can be adjusted
	float AspectRatio = (float)ScreenWidth / (float)ScreenHeight;
    
	// Near and far clip planes
	const float NearClip = 10.0f;
	const float FarClip = 10000.0f;
    
	// Calculate the projection matrix
	ProjectionData.ProjectionMatrix = FReversedZPerspectiveMatrix(
		FMath::DegreesToRadians(FOV) * 0.5f,
		AspectRatio,
		1.0f, // Screen scale
		NearClip,
		FarClip
	);

	return true;
}

FSyncedScreenProjection UTargetingLibrary::ComputeScreenProjectionData(AController* Controller)
{
	if(!Controller)
    {
        return FSyncedScreenProjection();
    }

    FSyncedScreenProjection OutProjectionData;

    FSceneViewProjectionData ScreenProjection;
    APlayerController* PlayerController = Cast<APlayerController>(Controller);

    float FOV = 90.f;
    float AspectRatio = 1.33334f;
    TEnumAsByte<enum EAspectRatioAxisConstraint> AspectRatioAxisConstraint = AspectRatio_MaintainYFOV;
    if (!PlayerController)
    {
        GenerateAIScreenProjection(Controller->GetPawn(), ScreenProjection);
    }
    else
    {
        ULocalPlayer* const LocalPlayer = PlayerController->GetLocalPlayer();
    	if (!LocalPlayer)
    	{
    		return OutProjectionData;
    	}
        int32 SizeX, SizeY;
        PlayerController->GetViewportSize(SizeX, SizeY);
        AspectRatio = PlayerController->PlayerCameraManager->GetCameraCacheView().AspectRatio;
        FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
        LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ScreenProjection);
    	AspectRatioAxisConstraint = PlayerController->PlayerCameraManager->GetCameraCacheView().AspectRatioAxisConstraint.Get(LocalPlayer->AspectRatioAxisConstraint);
    }
// Set Screen Projection Data To Send
    OutProjectionData.FOV = FOV;
    OutProjectionData.AspectRatio = AspectRatio;
    OutProjectionData.ViewSize.X = static_cast<float>(ScreenProjection.GetConstrainedViewRect().Width());
    OutProjectionData.ViewSize.Y = static_cast<float>(ScreenProjection.GetConstrainedViewRect().Height());
    const bool bMaintainXFOV = 
        ((OutProjectionData.ViewSize.X > OutProjectionData.ViewSize.Y) && ((AspectRatioAxisConstraint == AspectRatio_MajorAxisFOV) || (PlayerController->PlayerCameraManager->GetCameraCacheView().ProjectionMode == ECameraProjectionMode::Orthographic))) ||
        (AspectRatioAxisConstraint == AspectRatio_MaintainXFOV);
	OutProjectionData.bMaintainXFOV = bMaintainXFOV;
    // Add Bool To FSyncedScreenProjection and set it here
    OutProjectionData.ViewMin.X = static_cast<float>(ScreenProjection.GetConstrainedViewRect().Min.X);
    OutProjectionData.ViewMin.Y = static_cast<float>(ScreenProjection.GetConstrainedViewRect().Min.Y);
	
    return OutProjectionData;
}

FMatrix UTargetingLibrary::BuildScreenProjectionMatrix(FSyncedScreenProjection ProjectionData,
	FVector CameraLocation, FRotator CameraRotation)
{
	return FSyncedScreenProjection::BuildScreenProjectionMatrix(ProjectionData,CameraRotation,CameraLocation);
}

void UTargetingLibrary::DrawSingleTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,
							const FRotator& Rotation, bool bHit, const FHitResult& OutHit, float DrawDebugDuration,
							const EDrawDebugTrace::Type& DrawDebugType,float MinDebugDur,const FColor& TraceHitColor,
							const FColor& TraceColor,const ETargetingTraceShape& TargetingShape,const float& Radius,
							const float& HalfHeight,const FVector& BoxExtent)
{
#if ENABLE_DRAW_DEBUG
	if (DrawDebugType == EDrawDebugTrace::Type::None)
	{
		return;
	}
	float Duration = FMath::Max(DrawDebugDuration,MinDebugDur);
	switch (TargetingShape)
	{
	case ETargetingTraceShape::ELine:
		{
			DrawDebugLineTraceSingle(World, Start, End, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::ESphere:
		{
			DrawDebugSphereTraceSingle(World, Start, End, Radius, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::ECapsule:
		{
			DrawDebugCapsuleTraceSingle(World, Start, End, Radius,HalfHeight, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::EBox:
		{
			DrawDebugBoxTraceSingle(World, Start, End,BoxExtent * 0.5f,Rotation, DrawDebugType, bHit, OutHit, TraceColor, TraceHitColor, Duration);
			break;
		}
	}
#endif
}

void UTargetingLibrary::DrawMultiTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,const FRotator& Rotation,
						bool bHit, const TArray<FHitResult>& OutHits, float DrawDebugDuration,
						const EDrawDebugTrace::Type& DrawDebugType,float MinDebugDur,const FColor& TraceHitColor,
						const FColor& TraceColor,const ETargetingTraceShape& TargetingShape,const float& Radius,
						const float& HalfHeight,const FVector& BoxExtent)
{
#if ENABLE_DRAW_DEBUG
	if (DrawDebugType == EDrawDebugTrace::Type::None)
	{
		return;
	}
	float Duration = FMath::Max(DrawDebugDuration,MinDebugDur);
	switch (TargetingShape)
	{
	case ETargetingTraceShape::ELine:
		{
			DrawDebugLineTraceMulti(World, Start, End, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::ESphere:
		{
			DrawDebugSphereTraceMulti(World, Start, End, Radius, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::ECapsule:
		{
			DrawDebugCapsuleTraceMulti(World, Start, End, Radius,HalfHeight, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, Duration);
			break;
		}
	case ETargetingTraceShape::EBox:
		{
			DrawDebugBoxTraceMulti(World, Start, End,BoxExtent * 0.5f,Rotation, DrawDebugType, bHit, OutHits, TraceColor, TraceHitColor, Duration);
			break;
		}
	}
#endif
}


void UTargetingLibrary::DrawOverlapDebug(const UWorld* World, const FVector& Start, const FRotator& Rotation, bool bHit,
	const TArray<FOverlapResult>& OverlapResults, float DrawDebugDuration, const EDrawDebugTrace::Type& DrawDebugType,
	float MinDebugDur, const FColor& TraceHitColor, const FColor& TraceColor,
	const ETargetingOverlapShape& OverlapShape, const float& Radius, const float& HalfHeight, const FVector& BoxExtent)
{
#if ENABLE_DRAW_DEBUG
	if (DrawDebugType == EDrawDebugTrace::Type::None)
	{
		return;
	}
	float Duration = FMath::Max(DrawDebugDuration,MinDebugDur);
	const bool bPersistent = DrawDebugType == EDrawDebugTrace::Type::Persistent;
	const FColor Color = bHit ? TraceHitColor : TraceColor;
	switch (OverlapShape)
	{
	case ETargetingOverlapShape::ESphere:
		{
			DrawDebugSphere(World,Start,Radius,12,Color,bPersistent,Duration);
			break;
		}
	case ETargetingOverlapShape::ECapsule:
		{
			DrawDebugCapsule(World,Start,HalfHeight,Radius,Rotation.Quaternion(),Color,bPersistent,Duration);
			break;
		}
	case ETargetingOverlapShape::EBox:
		{
			DrawDebugBox(World,Start,BoxExtent * 0.5f,Rotation.Quaternion(),Color,bPersistent,Duration);
			break;
		}
	}
	// ToDo Draw spheres or capsules or something on the overlapped actors locations!
#endif
}
