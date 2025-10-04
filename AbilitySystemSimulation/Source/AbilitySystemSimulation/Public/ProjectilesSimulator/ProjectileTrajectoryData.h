// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "ProjectileTrajectoryData.generated.h"

/**
 * 
 */

// Finalized

UENUM(BlueprintType)
enum class EProjectileCollisionShape : uint8
{
	ESphere,
	EBox,
};

/*
 * The Response to projectile hitting something while moving, this is used also to decide on when to broadcast a hit
 * Some projectile might want to pierce so ignore the blocking hit but still broadcast the hit to do dmg or heal.
 * some projectiles might want to bounce and broadcast each bounce to do an explosion or leave damage area behind etc..
 */
UENUM(BlueprintType)
enum class EMoveHitResponse : uint8
{
	EExplode UMETA(Tooltip = "Explode projectile on hit, trigger callback and start the destruction timer "),
	EBlock UMETA(Tooltip = "Hit Blocks us, Fall Down and Along floor until coming to rest and explode at end of life, No Broadcast"),
	EBounce UMETA(Tooltip = "Bounce on the surface we hit and broadcast OnBounce"),
	EIgnore UMETA(Tooltip = "Ignore the hit and keep moving as if it didn't happen"),
	EPierce UMETA(Tooltip = "Ignore the hit and keep moving as if it didn't happen, but broadcast the OnPierce"),
};
/*
 * When broadcasting different hits with FOnProjectileExplode,FOnProjectileBounce,FOnProjectilePassThrough we send this data through
 * Hit Result might be empty sometimes as projectile can explode when reaching end of life without a hit.
 */
USTRUCT(BlueprintType)
struct FHitBroadcastData
{
	GENERATED_BODY()

	FHitBroadcastData(){}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FHitResult HitResult = FHitResult(1.f);
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FVector ProjectileLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	float ProjectileAgeMS = 0.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	int32 CurrentBounceCount = 0;
};

USTRUCT(BlueprintType)
struct FProjectileHitBroadcast
{
	GENERATED_BODY()

	FProjectileHitBroadcast(){}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	EMoveHitResponse MoveHitResponse = EMoveHitResponse::EBlock;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FHitBroadcastData HitData;
};

/*
 * Every Tick Projectile Moves and has a new movement state this represents projectile state after each move
 */
USTRUCT(BlueprintType)
struct FProjectileMove
{
	GENERATED_BODY()

	FProjectileMove(){}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FVector Position = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	int32 CurrentBounceCount = 0;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	bool bExploded = false;

	void Lerp(const FProjectileMove& From,const FProjectileMove& To,const float& Alpha)
	{
		Position = FMath::Lerp(From.Position,To.Position,Alpha);
		Velocity = FMath::Lerp(From.Velocity,To.Velocity,Alpha);
		CurrentBounceCount = Alpha > 0.5f ? To.CurrentBounceCount : From.CurrentBounceCount;
		bExploded = Alpha > 0.5f ? To.bExploded : From.bExploded;
	}
	
	bool operator==(const FProjectileMove& Other) const;
	bool operator!=(const FProjectileMove& Other) const;
};
/*
 * This represents projectile full state that can changes from frame to the next. includes movement state
 */
USTRUCT(BlueprintType)
struct FProjectileStep
{
	GENERATED_BODY()

	FProjectileStep(){}
	
	// The Simulation frame This Entry belongs to.
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	int32 ServerFrame = 0;

	// Age is SimTime - SpawnTime
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	float AgeMS = 0.f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	FProjectileMove Move;

	static FProjectileStep Lerp(const FProjectileStep& From,const FProjectileStep& To,const float& Alpha)
	{
		FProjectileStep Result;
		Result.ServerFrame = FMath::Lerp(From.ServerFrame,To.ServerFrame,Alpha);
		Result.AgeMS = FMath::Lerp(From.AgeMS,To.AgeMS,Alpha);
		Result.Move.Lerp(From.Move,To.Move,Alpha);
		return Result;
	}

	bool operator==(const FProjectileStep& Other) const;
	bool operator!=(const FProjectileStep& Other) const;
};

/*
 * Utility struct to reduce number of function inputs
 */
USTRUCT(BlueprintType)
struct FProjectileMoveTimeStep
{
	GENERATED_BODY()

	FProjectileMoveTimeStep(){}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	int32 ServerFrame = 0.f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Projectile)
	float DeltaTimeMs = 0.f;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FTrajectoryGenerationInputs
{
	GENERATED_BODY()

	FTrajectoryGenerationInputs(){}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=MoveData)
	float LifeTimeMS = 0.f;
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=MoveData)
	float DeltaTimeMS = 0.f;
};

USTRUCT(BlueprintType)
struct FProjectileTrajectory
{
	GENERATED_BODY()

	FProjectileTrajectory(){}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Trajectory)
	TArray<FProjectileStep> Trajectory;

	// Can't be used On Visual Trajectory.. Create Own Struct For Visual Trajectory With Own Functions
	int32 GetEntryByServerFrame(const int32& ServerFrame, FProjectileStep& FoundEntry);
	FProjectileStep GetEntryByAge(const float& TargetAge);

	FProjectileStep GetEntryByAgeWithIndex(const float& TargetAge,int32& FirstIndex);

	void DrawFullTrajectory(const UWorld* World,const float& DebugLifeTime,const EProjectileCollisionShape& Shape,const FVector& Size, UInstancedStaticMeshComponent* InstancedMesh = nullptr);
};

USTRUCT(BlueprintType)
struct FProjectileVisualTrajectory
{
	GENERATED_BODY()

	FProjectileVisualTrajectory(){}

	FProjectileVisualTrajectory(const FProjectileTrajectory& SimTrajectory,const FProjectileStep& OverrideStep,const int32& OverrideIndex)
	{
		UpdateFomSimTrajectory(SimTrajectory,OverrideStep,OverrideIndex);
	}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=Trajectory)
	TArray<FVector> Positions;


	// ToDo , this function can apply corrections also to smooth the projectile changed if needed
	void UpdateFomSimTrajectory(const FProjectileTrajectory& SimTrajectory,
	const FProjectileStep& OverrideStep,const int32& OverrideIndex);
};