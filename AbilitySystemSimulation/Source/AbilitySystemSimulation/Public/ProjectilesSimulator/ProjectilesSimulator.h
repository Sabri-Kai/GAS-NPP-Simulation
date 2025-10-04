// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "SyncedProjectilesData.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "UObject/Object.h"
#include "ProjectilesSimulator.generated.h"

/**
 * Projectile Simulator is an object created by the ability system component, it manages the projectile of the ASC
 * ASC simulation is responsible for replicating the data of the projectiles (this will need improvement as it is hard coded in sync state now)
 * ASC simulation is also responsible for calling the simulation function on this object.
 * This Objects manages of all projectiles currently active :
 * Spawning Projectiles (building the trajectory in the projectile Actor)
 * Ticking Projectiles (Setting ProjectileLocation Variable in a virtual function (can set actor location))
 * Notifying Projectiles of bounce (rebuilding trajectory) 
 * Notifying Projectiles Of Hit . (The Projectile Actor is responsible for deciding what to do when Hitting).
 * Notifying Projectiles when they reach end of life.
 * Comparing Changes in the Simulated proxies state with current state
 * and triggering the appropriate events (Spawn/Bounce/Hit) on the simulated proxies ,keeping trajectories updated.
 * Finally Broadcasting when a specific Projectile gets a hit , this is used by the Projectile Ability Task.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams( FOnSyncedProjectileEvent, const FHitBroadcastData&  , const uint32&);

UCLASS()
class ABILITYSYSTEMSIMULATION_API UProjectilesSimulator : public UObject
{
	GENERATED_BODY()
	friend struct FProjectileListScopeLock;

public:
	
	// Calls Tick On Each Projectile, then collects their data, manages the projectiles instances
	void SimulationTick(const FAbilitySystemTimeStep& TimeStep,const FProjectilesCollection& InputState, FProjectilesCollection& OutputState);
	// Ticks Active Projectiles
	void TickProjectiles(const FAbilitySystemTimeStep& TimeStep);
	
	// Restore Active Projectiles To an Authority State before re-simulation
	void RestoreFrame(const FProjectilesCollection& AuthorityState);
	
	// Calls finalize on projectiles for server and local client
	void FinalizeFrame(const FProjectilesCollection& FinalizeState);

	// Calls finalize on projectiles for server and local client
	void FinalizeInterpolatedFrame(const FProjectilesCollection& FinalizeState);

	void FinalizeProjectiles(const float& RenderTimeMS);
	
	//	Sets the state of projectiles for simulated proxies based on replicated state and Ticks projectiles ,
	//	updating the visual location and regenerating trajectory if a new trajectory change has been detected.
	// void TickInterpolatedProjectiles(const& FProjectileCollection& FinalizeState)

	// When Projectile is going to be spawned for sim proxies we need to calculate current loc and velocity from spawn time
	// and current interpolation time and pass that to trajectory generation , make sure to remove the interp time - spawn time
	// from the trajectory max time.
	// If Last relevant Move data for an existing projectile is different, regenerate the trajectory and trigger event on projectile
	// If HitTime is different from what for an existing projectile , trigger Hit event with correct hit location from trajectory,
	//  passing the hit time both in MS and as HitTime - Max(SpawnTime,BounceTime), to represent time relative to last bounce
	// Also takes care of updating the "visual trajectory" in case of corrections
	// and "VisualLocation" variable with a smoothed location??
	// void TickInterpolatedProjectiles(const& FProjectileCollection& InterpolationState) //SimulatedProxies

	// Spawns the projectile and adds it to active projectiles list , or pending if currently iterating through projectiles.
	ASyncedProjectileBase* SpawnProjectile(const TSubclassOf<ASyncedProjectileBase>& Class, const FVector& Location ,const FVector& Direction);
	ASyncedProjectileBase* ForceSpawnProjectile(const FSyncedProjectile& ProjectileSyncedData);
	ASyncedProjectileBase* ForceSpawnInterpolatedProjectile(const FSyncedProjectile& ProjectileSyncedData);
	// This Removes projectile from Active Projectiles and Adds it to Shelves to possibly be re-used later during re-simulation.
	void ForceRemoveProjectile(ASyncedProjectileBase* Projectile);
	// FOnProjectileHit(int32 ID,TArray<FHitResult> Hits) 

	void DestroyProjectile(ASyncedProjectileBase* Projectile);
	
	UPROPERTY()
	TArray<TObjectPtr<ASyncedProjectileBase>> ActiveProjectiles;

	UFUNCTION()
	ASyncedProjectileBase* GetProjectileInstanceByID(const uint32& ProjectileID);
	
	UFUNCTION()
	UNpAbilitySystemComponent* GetOwningAbilitySystem() const;


	//These Are simulation only events , so they do not trigger on interpolated proxies.
	FOnSyncedProjectileEvent OnProjectileBounce;
	FOnSyncedProjectileEvent OnProjectileExplode;
	FOnSyncedProjectileEvent OnProjectilePierce;
	FOnSyncedProjectileEvent OnProjectileEndOfLife;

	float GetProjectilesSimRenderTimeMS() const;

private:
	UPROPERTY()
	uint32 ProjectilesIDCount = 0;
	
	// Shelved Projectiles are instances that were removed from active list during restore frame, keeping them here until finalize frame
	// ensures if they get respawned again during re-simulation they can be re-used.
	UPROPERTY()
	TMap<uint32,TObjectPtr<ASyncedProjectileBase>> ShelvedProjectiles;

	// Lock ensures we don't affect the Active projectiles Array while we are iterating through it.
	int32 ProjectilesLockCount = 0;

	void IncrementProjectilesLockCount();
	void DecrementProjectilesLockCount();
	UPROPERTY()
	TArray<TObjectPtr<ASyncedProjectileBase>> PendingAddProjectiles;
	UPROPERTY()
	TArray<TObjectPtr<ASyncedProjectileBase>> PendingRemoveProjectiles;
};

