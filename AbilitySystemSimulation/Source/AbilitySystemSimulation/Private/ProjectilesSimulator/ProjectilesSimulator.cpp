// 2025 Yohoho Productions /  Sirkai


#include "ProjectilesSimulator/ProjectilesSimulator.h"
#include "NetworkPredictionWorldManager.h"
#include "ProjectilesSimulator/SyncedProjectileBase.h"

void UProjectilesSimulator::SimulationTick(const FAbilitySystemTimeStep& TimeStep, const FProjectilesCollection& InputState,
                                           FProjectilesCollection& OutputState)
{
	TickProjectiles(TimeStep);
	// Sort projectiles , this might not be needed, but it's best for determinism , and will help with performance in other places
	// like ShouldReconcile can directly check for indexes 1 to 1 without needing a second inner loop to find the correct projectile.
	ActiveProjectiles.Sort([](const TObjectPtr<ASyncedProjectileBase>& A, const TObjectPtr<ASyncedProjectileBase>& B)
	{
		return A->ProjectileData.ProjectileID < B->ProjectileData.ProjectileID;
	});
	OutputState.CollectState(ActiveProjectiles,ProjectilesIDCount);
}

void UProjectilesSimulator::TickProjectiles(const FAbilitySystemTimeStep& TimeStep)
{
	FProjectileListScopeLock Lock(*this);
	if (ActiveProjectiles.IsEmpty())
	{
		return;
	}
	// ToDo : Add Lag compensation rewind here?? it is better for performance if it happens once for all projectiles of this actor
	
	// but what if there's a projectile that doesn't want it?? 
	for (TObjectPtr<ASyncedProjectileBase> Projectile : ActiveProjectiles)
	{
		if (!IsValid(Projectile))
		{
			continue;
		}
		if (Projectile->ProjectileData.bExploded)
		{
			const float DestroyTimerDurationMS = FMath::Floor(Projectile->DestroyTimerDuration * 1000.f);
			const float CurrentDestroyTimerMS = (TimeStep.ServerFrame - Projectile->ProjectileData.LastTrajectoryChangeFrame) * TimeStep.StepMs;
			if (CurrentDestroyTimerMS >= DestroyTimerDurationMS)
			{
				DestroyProjectile(Projectile);
			}
			continue;
		}
		Projectile->SimulationTick(TimeStep);
	}
}

void UProjectilesSimulator::RestoreFrame(const FProjectilesCollection& AuthorityState)
{
	ProjectilesIDCount = AuthorityState.SyncedProjectilesIDCount;
	// First Create A TMap To Easily Find Active Projectile by ID
	TMap<uint32, ASyncedProjectileBase*> InstanceMap;
	InstanceMap.Reserve(ActiveProjectiles.Num());

	for (ASyncedProjectileBase* Instance : ActiveProjectiles)
	{
		InstanceMap.Add(Instance->ProjectileData.ProjectileID, Instance);
	}

	// Loop Through Authority projectiles Data and if found in active projectiles restore and add it to Found IDs
	// if not found add it to Projectiles to add
	TArray<FSyncedProjectile> ProjectilesToAdd;
	TSet<int32> FoundIDs;
	for (const FSyncedProjectile& AuthorityProjectileData : AuthorityState.Projectiles)
	{
		ASyncedProjectileBase** FoundProjectilePtr = InstanceMap.Find(AuthorityProjectileData.ProjectileData.ProjectileID);
		if (FoundProjectilePtr && *FoundProjectilePtr && (*FoundProjectilePtr)->GetClass() == AuthorityProjectileData.ProjectileClass)
		{
			(*FoundProjectilePtr)->RestoreProjectile(AuthorityProjectileData.ProjectileData,GetOwningAbilitySystem()->GetFixedStepMs());
			FoundIDs.Add(AuthorityProjectileData.ProjectileData.ProjectileID);
		}
		else
		{
			ProjectilesToAdd.Add(AuthorityProjectileData);
		}
	}

	// Check if any active projectiles have ID that weren't found in authority data, if found add them to Instances to Remove.
	TArray<ASyncedProjectileBase*> InstancesToRemove;

	for (ASyncedProjectileBase* Instance : ActiveProjectiles)
	{
		if (!FoundIDs.Contains(Instance->ProjectileData.ProjectileID))
		{
			InstancesToRemove.Add(Instance);
		}
	}

	// Add Any New Instances, These currently will have a trajectory from latest relevant velocity change (bounce/slide etc..) forward in time.
	if (ProjectilesToAdd.Num() > 0)
	{
		for (const FSyncedProjectile& ProjectileToAdd : ProjectilesToAdd)
		{
			ForceSpawnProjectile(ProjectileToAdd);
		}
	}

	if (InstancesToRemove.Num() > 0)
	{
		for (ASyncedProjectileBase* InstanceToRemove : InstancesToRemove)
		{
			ForceRemoveProjectile(InstanceToRemove);
		}
	}
}

void UProjectilesSimulator::FinalizeFrame(const FProjectilesCollection& FinalizeState)
{
	// First Empty The Shelves projectiles
	for (TPair<uint32,TObjectPtr<ASyncedProjectileBase>> ShelvedInstance : ShelvedProjectiles)
	{
		ShelvedInstance.Value->Destroy();
	}
	ShelvedProjectiles.Empty();
	FinalizeProjectiles(GetProjectilesSimRenderTimeMS());
}

void UProjectilesSimulator::FinalizeInterpolatedFrame(const FProjectilesCollection& FinalizeState)
{
	
	FProjectileListScopeLock ListLock(*this);
    // Step 1: Build lookup map from active projectiles
    TMap<uint32, ASyncedProjectileBase*> ActiveMap;
    ActiveMap.Reserve(ActiveProjectiles.Num());

    for (ASyncedProjectileBase* Projectile : ActiveProjectiles)
    {
        uint32 Key = FSyncedProjectile::GetTypeHash(Projectile->GetClass(), Projectile->ProjectileData.ProjectileID);
        ActiveMap.Add(Key, Projectile);
    }

    // Step 2: Process finalize state
    for (const FSyncedProjectile& FinalizeProjectile : FinalizeState.Projectiles)
    {
        uint32 Key = FSyncedProjectile::GetTypeHash(FinalizeProjectile.ProjectileClass, FinalizeProjectile.ProjectileData.ProjectileID);
        ASyncedProjectileBase* FoundInstance = ActiveMap.FindRef(Key);

        if (FoundInstance)
        {
            // Projectile already exists → update
            FoundInstance->FinalizeInterpolatedFrame(GetProjectilesSimRenderTimeMS(), FinalizeProjectile.ProjectileData);
            ActiveMap.Remove(Key);
        }
        else
        {
            // New projectile → spawn & update
            ASyncedProjectileBase* NewInstance = ForceSpawnInterpolatedProjectile(FinalizeProjectile);
            NewInstance->FinalizeInterpolatedFrame(GetProjectilesSimRenderTimeMS(), FinalizeProjectile.ProjectileData);
        }
    }

    // Step 3: Destroy leftover projectiles
    for (auto& RemainingPair : ActiveMap)
    {
        ASyncedProjectileBase* NotFoundInstance = RemainingPair.Value;

        if (!NotFoundInstance->ProjectileData.bExploded)
        {
            FHitBroadcastData HitBroadcastData;
            HitBroadcastData.ProjectileLocation = NotFoundInstance->ProjectileLocation;
            HitBroadcastData.CurrentBounceCount = NotFoundInstance->ProjectileData.BouncesAtLastTrajectoryChange;
            HitBroadcastData.ProjectileAgeMS = GetProjectilesSimRenderTimeMS() - (NotFoundInstance->ProjectileData.SpawnFrame * GetOwningAbilitySystem()->GetFixedStepMs());
            NotFoundInstance->OnExplode.Broadcast(HitBroadcastData);
        }

        ActiveProjectiles.RemoveSingleSwap(NotFoundInstance);
        NotFoundInstance->Destroy();
    }
}

void UProjectilesSimulator::FinalizeProjectiles(const float& RenderTimeMS)
{
	FProjectileListScopeLock Lock(*this);
	for (ASyncedProjectileBase* Instance : ActiveProjectiles)
	{
		if (IsValid(Instance))
		{
			Instance->FinalizeFrame(RenderTimeMS,Instance->ProjectileData);
		}
	}
}

ASyncedProjectileBase* UProjectilesSimulator::SpawnProjectile(const TSubclassOf<ASyncedProjectileBase>& Class, const FVector& Location,const FVector& Direction)
{
	// First Check for any matching shelved projectiles, which get added when restore frame requests deleting a projectile
	if (!GetOwningAbilitySystem())
	{
		return nullptr;
	}
	ProjectilesIDCount++;
	TObjectPtr<ASyncedProjectileBase> NewProjectile = nullptr;
	TObjectPtr<ASyncedProjectileBase>* FoundProjectilePtr = ShelvedProjectiles.Find(ProjectilesIDCount);
	if (FoundProjectilePtr && *FoundProjectilePtr && (*FoundProjectilePtr)->GetClass() == Class)
	{
		NewProjectile = *FoundProjectilePtr;
		ShelvedProjectiles.Remove(ProjectilesIDCount);
	}
	if (!NewProjectile)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Instigator = Cast<APawn>(GetOwningAbilitySystem()->GetAvatarActor());
		SpawnParameters.Owner = GetOwningAbilitySystem()->GetOwner();
		const FTransform SpawnTransform = FTransform(Direction.ToOrientationRotator(),Location);
		NewProjectile = Cast<ASyncedProjectileBase>(GetWorld()->SpawnActor(Class,&SpawnTransform,SpawnParameters));
	}
	
	NewProjectile->OwningSimulator = this;
	NewProjectile->InitializeProjectile(GetOwningAbilitySystem()->GetCurrentSimFrame() - 1,GetOwningAbilitySystem()->GetFixedStepMs()
		,ProjectilesIDCount,Location,Direction);
	if (ProjectilesLockCount > 0)
	{
		PendingAddProjectiles.Add(NewProjectile);
	}
	else
	{
		ActiveProjectiles.Add(NewProjectile);
	}
	return NewProjectile;
}

ASyncedProjectileBase* UProjectilesSimulator::ForceSpawnProjectile(const FSyncedProjectile& ProjectileSyncedData)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = Cast<APawn>(GetOwningAbilitySystem()->GetAvatarActor());
	SpawnParameters.Owner = GetOwningAbilitySystem()->GetOwner();
	const FTransform SpawnTransform = FTransform(ProjectileSyncedData.ProjectileData.LastRelevantVelocity.GetSafeNormal().ToOrientationRotator(),ProjectileSyncedData.ProjectileData.LastRelevantLocation);
	ASyncedProjectileBase* NewProjectile = Cast<ASyncedProjectileBase>(GetWorld()->SpawnActor(ProjectileSyncedData.ProjectileClass,&SpawnTransform,SpawnParameters));

	NewProjectile->OwningSimulator = this;
	NewProjectile->ForceInitializeProjectile(ProjectileSyncedData.ProjectileData,GetOwningAbilitySystem()->GetFixedStepMs());
	
	if (ProjectilesLockCount > 0)
	{
		PendingAddProjectiles.Add(NewProjectile);
	}
	else
	{
		ActiveProjectiles.Add(NewProjectile);
	}
	return NewProjectile;
}

ASyncedProjectileBase* UProjectilesSimulator::ForceSpawnInterpolatedProjectile(
	const FSyncedProjectile& ProjectileSyncedData)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = Cast<APawn>(GetOwningAbilitySystem()->GetAvatarActor());
	SpawnParameters.Owner = GetOwningAbilitySystem()->GetOwner();
	const FTransform SpawnTransform = FTransform(ProjectileSyncedData.ProjectileData.LastRelevantVelocity.GetSafeNormal().ToOrientationRotator(),ProjectileSyncedData.ProjectileData.LastRelevantLocation);
	ASyncedProjectileBase* NewProjectile = Cast<ASyncedProjectileBase>(GetWorld()->SpawnActor(ProjectileSyncedData.ProjectileClass,&SpawnTransform,SpawnParameters));

	NewProjectile->OwningSimulator = this;
	NewProjectile->ForceInitializeProjectile(ProjectileSyncedData.ProjectileData,GetOwningAbilitySystem()->GetFixedStepMs());
	//Make Sure projectile data acts as if we just spawned, meaning no bounces, no explosion, no pierces
	NewProjectile->ProjectileData.bExploded = false;
	NewProjectile->ProjectileData.BouncesAtLastTrajectoryChange = 0;
	
	if (ProjectilesLockCount > 0)
	{
		PendingAddProjectiles.Add(NewProjectile);
	}
	else
	{
		ActiveProjectiles.Add(NewProjectile);
	}
	return NewProjectile;
}

void UProjectilesSimulator::ForceRemoveProjectile(ASyncedProjectileBase* Projectile)
{
	// This is only called when restoring frame and want to remove an instance, add it to shelved here
	// when spawning new instance we check in shelved projectiles if we have one that matches what we want to spawn
	ShelvedProjectiles.Add(Projectile->ProjectileData.ProjectileID,Projectile);
	ActiveProjectiles.Remove(Projectile);
}

void UProjectilesSimulator::DestroyProjectile(ASyncedProjectileBase* Projectile)
{
	if (ProjectilesLockCount > 0)
	{
		PendingRemoveProjectiles.Add(Projectile);
	}
	else
	{
		ActiveProjectiles.Remove(Projectile);
		Projectile->Destroy();
	}
}

ASyncedProjectileBase* UProjectilesSimulator::GetProjectileInstanceByID(const uint32& ProjectileID)
{
	TObjectPtr<ASyncedProjectileBase>* FoundItem = ActiveProjectiles.FindByPredicate([ProjectileID](const ASyncedProjectileBase* Projectile)
			{
				return Projectile->ProjectileData.ProjectileID == ProjectileID;
			});

	if (FoundItem)
	{
		return *FoundItem;
	}
	return nullptr;
}

UNpAbilitySystemComponent* UProjectilesSimulator::GetOwningAbilitySystem() const
{
	check(GetOuter()->IsA(UNpAbilitySystemComponent::StaticClass()))
	return Cast<UNpAbilitySystemComponent>(GetOuter());
}

float UProjectilesSimulator::GetProjectilesSimRenderTimeMS() const
{
	if (!GetOwningAbilitySystem())
	{
		return 0.f;
	}
	float RenderTimeMS = 0;
	if (GetOwningAbilitySystem()->GetCachedSimNetRole() == ROLE_SimulatedProxy)
	{
		if (UNetworkPredictionWorldManager* NppManager = GetOwningAbilitySystem()->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>())
		{
			RenderTimeMS = NppManager->GetFixedTickState().Interpolation.InterpolatedTimeMS;
		}
		return RenderTimeMS;
		
	}
	float FixedTTickTimeLeftOver = 0.f;
	if (UNetworkPredictionWorldManager* NppManager = GetOwningAbilitySystem()->GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>())
	{
		FixedTTickTimeLeftOver = NppManager->GetFixedTickState().UnspentTimeMS;
	}
	RenderTimeMS =  (GetOwningAbilitySystem()->GetCurrentSimFrame() * GetOwningAbilitySystem()->GetFixedStepMs()) + FixedTTickTimeLeftOver;
	return RenderTimeMS;
}

void UProjectilesSimulator::IncrementProjectilesLockCount()
{
	ProjectilesLockCount++;
}

void UProjectilesSimulator::DecrementProjectilesLockCount()
{
	ProjectilesLockCount = FMath::Min(ProjectilesLockCount - 1, 0);
	if (ProjectilesLockCount == 0)
	{
		if (PendingAddProjectiles.Num() > 0)
		{
			for (int32 i = 0 ; i < PendingAddProjectiles.Num()  ; ++i)
			{
				ActiveProjectiles.Add(PendingAddProjectiles[i]);
			}
			PendingAddProjectiles.Empty();
		}

		if (PendingRemoveProjectiles.Num() > 0)
		{
			for (int32 i = 0 ; i < PendingRemoveProjectiles.Num()  ; ++i)
			{
				ActiveProjectiles.Remove(PendingRemoveProjectiles[i]);
				PendingRemoveProjectiles[i]->Destroy();
			}
			PendingRemoveProjectiles.Empty();
		}
	}
}
