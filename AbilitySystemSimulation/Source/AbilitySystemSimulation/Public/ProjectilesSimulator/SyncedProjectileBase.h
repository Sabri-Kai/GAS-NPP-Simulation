// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "ProjectileTrajectoryData.h"
#include "SyncedProjectilesData.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "SyncedProjectileBase.generated.h"
/**
 * 
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam( FOnProjectileEvent, const FHitBroadcastData& , HitData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FOnProjectileTrajectoryChanged, const FProjectileTrajectory& ,Trajectory,const float& ,RemainingLifeTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams( FOnProjectileVisualTrajectoryChanged, const FProjectileVisualTrajectory& ,Trajectory,const float& ,RemainingLifeTime);

UCLASS(Blueprintable,BlueprintType)
class ABILITYSYSTEMSIMULATION_API ASyncedProjectileBase : public AActor
{
	GENERATED_UCLASS_BODY()

	friend class UProjectilesSimulator;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileDebug)
	bool DrawDebugTrajectoryOnSpawn = false;
	/** 
	* Start Speed Of Projectile 
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float InitialSpeed = 2000.f;
	/** 
	* Max bounce This Projectile Can perform  
	* Negative value means it can bounce as much as its speed and bounce parameters allow it
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	int32 MaxBounces = 0;
	/** 
	* Scalar that controls how much speed is retained after bouncing.  
	* 1.0 = perfect elastic bounce (no energy loss), 0.0 = no bounce at all.
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float BounceCoefficient = 0.6f;
	/** 
	* Amount of tangential velocity (slide along the surface) removed on bounce.  
	* 0.0 = no surface friction, projectile keeps sliding fully; 1.0 = maximum friction, removes all tangential slide.
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float BounceSurfaceFriction = 0.2f;
	/** 
	* Minimum speed required to allow bouncing.  
	* If the reflected speed after a bounce is lower than this, the projectile will stop instead of bouncing again.
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float MinBounceSpeed = 50.f;
	/** 
	* If the projectile’s speed is below this when hitting a flat surface, it comes to rest. 
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float MinRestSpeed = 10.f;
	/** 
	* Down Force Applied To projectile every frame
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float Gravity = 980.f;
	/** 
	* Down Force Applied To projectile every frame
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float Drag = 0.05f;
	/** 
	* If the floor normal is higher than this we consider this a floor and apply ground friction
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float SlidingMaxSlope = 0.85f;
	/** 
	* Ground friction to apply to projectile when it hits a surface. the steeper the surface is the less friction we apply
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement,meta=(ClampMin = 0))
	float GroundFriction = 0.85f;
	/** 
	* Set The Actor's root component location to what the simulation decides on projectile location or keep actor in spawn location
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement)
	bool UpdateRootComponentLocation = false;
	/** 
	* Set a visual component transform to a smoothed transform (for interpolated proxies if updating root component we don't need to update visual)
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement)
	bool UpdateVisualComponentLocation = false;
	/** 
	*  Max Duration this projectile can be alive, once this period passes projectile will explode and start Destruction timer
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileLifeTime,meta=(ClampMin = 0))
	float MaxLifeTime = 3.f;
	/** 
	*  when a projectile has exploded , it waits for this duration to get destroyed.
	*  The projectile will have no effect while "Exploded" and pending destroy, this is needed to ensure client receive explosion
	*  However post explosion logic should be outside the projectile itself in a gameplay cue, not a fan of this timer. 
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileLifeTime,meta=(ClampMin = 0))
	float DestroyTimerDuration = 1.f;
	
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileMovement)
	bool BroadcastExplodedOnEndOfLife = false;
	/** 
	*  the Collision Shape Of the projectile
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileCollision)
	EProjectileCollisionShape ProjectileCollisionShape = EProjectileCollisionShape::ESphere;
	/** 
	*  Box Size of projectile collision if it's Box Type
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileCollision,meta=(EditCondition = "ProjectileCollisionShape == EProjectileCollisionShape::EBox",EditConditionHides))
	FVector BoxHalfExtent = FVector(5.f,5.f,5.f);
	/** 
	*  Sphere radius of projectile collision if it's Sphere Type
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileCollision,meta=(ClampMin = 0.1f,EditCondition = "ProjectileCollisionShape == EProjectileCollisionShape::ESphere",EditConditionHides))
	float SphereRadius = 10.f;

	/** 
	* It is CRUCIAL this is a custom channel that everything spawned at runtime or moves that has collision is set to ignore
	* and only static non-moving objects placed in the level are set to block.
	* this is a necessary constraint to allow for smooth , well predicted projectile , server authoritative, with low networking bandwidth usage.
	*/
	// ToDo : this should be a project settings?????
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileCollision)
	TEnumAsByte<ECollisionChannel> StaticOnlyCollisionChannel = ECC_WorldStatic;

	/** 
	*  This is the collision channel for normal projectile collision, pawns , spawned and moving actors and static, everything blocking.
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=ProjectileCollision)
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

	// Delegates

	// For this delegate to trigger on Pass through, the Projectile class needs to override GetMoveHitResponse
	// and return IgnoreAndBroadcast when a passthrough with broadcast is desired (useful for piercing projectiles)
	// ToDo : Make pierce its own move response along with Max pierce amount just like bounces.
	// E.g : projectile can pierce up to 2 enemies, so 3rd enemy becomes an explode 
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnPierce;
	// For this delegate to trigger on bounce, the Projectile class needs to override GetMoveHitResponse
	// and return BounceAndBroadcast when a bounce is desired
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnBounce;
	
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnExplode;

	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnEndOfLife;

	UPROPERTY(BlueprintAssignable)
	FOnProjectileTrajectoryChanged OnTrajectoryUpdated;

	UPROPERTY(BlueprintAssignable)
	FOnProjectileVisualTrajectoryChanged OnVisualTrajectoryUpdated;


	// this event will be called only once when projectile explodes first time.
	UPROPERTY(BlueprintAssignable)
	FOnProjectileEvent OnVisualExplode;
	
	UPROPERTY()
	FProjectileData ProjectileData;
	
	// Trajectory
	UPROPERTY()
	FProjectileTrajectory Trajectory;


	UPROPERTY()
	FProjectileVisualTrajectory VisualTrajectory;

	UFUNCTION(BlueprintPure,Category=Projectile)
	FVector GetProjectileLocation() const {return ProjectileLocation;}
	UFUNCTION(BlueprintPure,Category=Projectile)
	FVector GetProjectileVelocity() const {return ProjectileVelocity;}
	UFUNCTION(BlueprintPure,Category=Projectile)
	FRotator GetProjectileRotation() const {return ProjectileRotation;}

	UNpAbilitySystemComponent* GetOwningAbilitySystem() const ;

	// Finalized
	virtual FVector GetDesiredVelocity(const float& DeltaTime,const FProjectileStep& InputStep) const;
	void MoveProjectile(const bool& bGeneratingTrajectory,const FProjectileMoveTimeStep& TimeStep,const FProjectileStep& InputStep
		,FProjectileMove& OutputMove, TArray<FProjectileHitBroadcast>& OutputHits);
	void SweepProjectile(const bool& bGeneratingTrajectory,const FProjectileMoveTimeStep& TimeStep,const FProjectileStep& InputStep
		,FProjectileMove& OutputMove, TArray<FProjectileHitBroadcast>& OutputHits,float& LeftOverTimeMS,FVector& DeltaMove);
	// ToDo Allow these functions to re-trace their own moves and handle Hits and broadcasts
	bool TryBounceOffHit(const FHitResult& Hit,FProjectileMove& OutputMove,float& LeftOverTimeMS,FVector& DeltaMove) const;
	void HandleBlockedMove(const FHitResult& Hit,FProjectileMove& OutputMove,float& LeftOverTimeMS,FVector& DeltaMove) const;
	

	// Return How should Projectile react to a detected hit,
	// by default it will explode when hitting a pawn, otherwise bounce if it can or explode
	// Doesn't pass through anything and doesn't broadcast bounces
	//ToDo Expose to blueprint
	virtual EMoveHitResponse GetMoveHitResponse(const bool& CanBounce ,const FHitResult& Hit);
	
	// Visual Trajectory
	// updated when trajectory is updated 
	// Differences:
	// when correction of trajectory happens, this one interpolates the difference in the corrected location over few frames.
	// And only updates future points after the Render Age, this allows updating existing effects trajectory without causing
	// already drawn ribbons to break
	
	// FVector Visual Location
	// extracted every frame from the visual trajectory for everyone.
	// This Uses RENDER AGER, render ager is determined by simulated proxies as interpolation time,
	// for auto and server it would be sim time + what is left over in the time bank in the tick state of NPP manager.
	// If Extrapolation is enabled, the Render Age for sim proxies is determined the same way it is for auto and server.


	// Non Finalized
	void RestoreProjectile(const FProjectileData& AuthorityData,const float& DeltaTimeMs);

	void SimulationTick(const FAbilitySystemTimeStep& TimeStep);

	void FinalizeFrame(const float& RenderTimeMS,const FProjectileData& FinalizeData);

	void FinalizeInterpolatedFrame(const float& RenderTimeMS,const FProjectileData& FinalizeData);

	void InitializeProjectile(const float& ServerFrame , const float& StepTimeMS,const uint32& ProjectileID,const FVector& StartLocation, const FVector& StartDirection);
	void ForceInitializeProjectile(const FProjectileData& AuthorityData,const float& DeltaTimeMs);

	void GenerateTrajectory(const FTrajectoryGenerationInputs& GenerationInputs,const FProjectileStep& StartingState,
		int32 OverrideIndex = INDEX_NONE);


	// Actor interface
	virtual void PostInitializeComponents() override;

	float GetFixedStepMS() const;

	UFUNCTION(BlueprintCallable)
	float GetProjectileRenderAge();
private:

	//ToDo Use Current State Which is an Entry from trajectory as saved location and velocity. 
	// Location
	UPROPERTY()
	FVector ProjectileLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector ProjectileVelocity = FVector::ZeroVector;
	
	UPROPERTY()
	FRotator ProjectileRotation = FRotator::ZeroRotator;
	// this is used to compare against finalize state and trigger visual/render events. 
	UPROPERTY()
	FProjectileData LastFinalizeProjectileData;

	UPROPERTY()
	TObjectPtr<UProjectilesSimulator> OwningSimulator = nullptr;

	UPROPERTY()
	TObjectPtr<UMeshComponent> VisualComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UInstancedStaticMeshComponent> DebugMeshComponent = nullptr;
	UPROPERTY()
	FTransform BaseVisualCompTransform = FTransform::Identity;

	bool JustRestoredFrame = false;


	
};
