// 2025 Yohoho Productions /  Sirkai


#include "ProjectilesSimulator/SyncedProjectilesData.h"

#include "NetworkPredictionReplicationProxy.h"
#include "ProjectilesSimulator/SyncedProjectileBase.h"
#include "ProjectilesSimulator/ProjectilesSimulator.h"


#pragma region Networked data

bool FProjectileData::operator==(const FProjectileData& Other) const
{
	if (!LastRelevantLocation.Equals(Other.LastRelevantLocation, 4.f))
	{
		return false;
	}
	if (!LastRelevantVelocity.Equals(Other.LastRelevantVelocity, 4.f))
	{
		return false;
	}
	if ( ProjectileID !=  Other.ProjectileID)
	{
		return false;
	}
	if ( SpawnFrame !=  Other.SpawnFrame)
	{
		return false;
	}
	if ( bExploded !=  Other.bExploded)
	{
		return false;
	}
	if ( LastTrajectoryChangeFrame !=  Other.LastTrajectoryChangeFrame)
	{
		return false;
	}
	if ( BouncesAtLastTrajectoryChange !=  Other.BouncesAtLastTrajectoryChange)
	{
		return false;
	}
	return true;
}

bool FProjectileData::operator!=(const FProjectileData& Other) const
{
	return !(*this == Other);
}

void FSyncedProjectile::NetSerialize(const FNetSerializeParams& Params)
{
	Params.Ar << ProjectileClass;
	check(IsValid(ProjectileClass));
	Params.Ar.SerializeIntPacked(ProjectileData.ProjectileID);
	SerializePackedVector<10,16>(ProjectileData.LastRelevantLocation, Params.Ar);
	SerializePackedVector<10,16>(ProjectileData.LastRelevantVelocity, Params.Ar);
	Params.Ar.SerializeIntPacked(ProjectileData.SpawnFrame);
	Params.Ar.SerializeBits(&ProjectileData.bExploded,1);
	Params.Ar.SerializeIntPacked(ProjectileData.LastTrajectoryChangeFrame);
	Params.Ar << ProjectileData.BouncesAtLastTrajectoryChange;
}
void FSyncedProjectile::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FSyncedProjectile* BaseState = Params.GetBaseDeltaState<FSyncedProjectile>();
	check(BaseState)

	// These properties are check against to find correct projectile in base state array,
	// so if we have a base state it means we found projectile in base state array that has our ID and class 
	// we are sending the index of base state array where these are , we can just copy them in this case.
	ProjectileClass = BaseState->ProjectileClass;
	ProjectileData.ProjectileID = BaseState->ProjectileData.ProjectileID;
	// a projectile should cost 8 bits per update after receiving its first update
	bool SameSpawnFrame = false;
	bool SameRelevantLocation = false;
	bool SameRelevantVel = false;
	bool SameImpactData = true;
	bool SameData = false;
	if (Params.Ar.IsSaving())
	{
		SameSpawnFrame = BaseState->ProjectileData.SpawnFrame == ProjectileData.SpawnFrame;
		SameRelevantLocation = BaseState->ProjectileData.LastRelevantLocation.Equals(ProjectileData.LastRelevantLocation,4.f);
		SameRelevantVel = BaseState->ProjectileData.LastRelevantVelocity.Equals(ProjectileData.LastRelevantLocation,4.f);
		bool SameReachedEndOfLife = BaseState->ProjectileData.bExploded == ProjectileData.bExploded;
		SameImpactData = ProjectileData.LastTrajectoryChangeFrame == BaseState->ProjectileData.LastTrajectoryChangeFrame
			&& ProjectileData.BouncesAtLastTrajectoryChange == BaseState->ProjectileData.BouncesAtLastTrajectoryChange;
		SameData = SameSpawnFrame && SameRelevantLocation && SameRelevantVel && SameReachedEndOfLife && SameImpactData;
	}
	Params.Ar.SerializeBits(&SameData,1);
	if (SameData)
	{
		ProjectileData.LastRelevantLocation = BaseState->ProjectileData.LastRelevantLocation;
		ProjectileData.LastRelevantLocation = BaseState->ProjectileData.LastRelevantLocation;
		ProjectileData.SpawnFrame = BaseState->ProjectileData.SpawnFrame;
		ProjectileData.bExploded = BaseState->ProjectileData.bExploded;
		ProjectileData.LastTrajectoryChangeFrame = BaseState->ProjectileData.LastTrajectoryChangeFrame;
		ProjectileData.BouncesAtLastTrajectoryChange = BaseState->ProjectileData.BouncesAtLastTrajectoryChange;
	}
	else
	{
		// Start Time
		Params.Ar.SerializeBits(&SameSpawnFrame,1);
		if (SameSpawnFrame)
		{
			ProjectileData.SpawnFrame = BaseState->ProjectileData.SpawnFrame;
		}
		else
		{
			Params.Ar.SerializeIntPacked(ProjectileData.SpawnFrame);
		}
		// Relevant Location
		Params.Ar.SerializeBits(&SameRelevantLocation,1);
		if (SameRelevantLocation)
		{
			ProjectileData.LastRelevantLocation = BaseState->ProjectileData.LastRelevantLocation;
		}
		else
		{
			SerializePackedVector<10,16>(ProjectileData.LastRelevantLocation, Params.Ar);
		}
		// Relevant velocity
		Params.Ar.SerializeBits(&SameRelevantVel,1);
		if (SameRelevantVel)
		{
			ProjectileData.LastRelevantVelocity = BaseState->ProjectileData.LastRelevantVelocity;
		}
		else
		{
			SerializePackedVector<10,16>(ProjectileData.LastRelevantVelocity, Params.Ar);
		}
		// End of Life
		Params.Ar.SerializeBits(&ProjectileData.bExploded,1);
		// Impact Data
		Params.Ar.SerializeBits(&SameImpactData,1);
		if (SameImpactData)
		{
			ProjectileData.LastTrajectoryChangeFrame = BaseState->ProjectileData.LastTrajectoryChangeFrame;
			ProjectileData.BouncesAtLastTrajectoryChange = BaseState->ProjectileData.BouncesAtLastTrajectoryChange;
		}
		else
		{
			Params.Ar.SerializeIntPacked(ProjectileData.LastTrajectoryChangeFrame);
			Params.Ar << ProjectileData.BouncesAtLastTrajectoryChange;
		}
		
	}
}

bool FSyncedProjectile::ShouldReconcile(const FSyncedProjectile& AuthorityData) const
{
	if (ProjectileClass != AuthorityData.ProjectileClass)
	{
		return true;
	}
	if (ProjectileData != AuthorityData.ProjectileData)
	{
		return true;
	}
	return false;
}

void FSyncedProjectile::Interpolate(const FSyncedProjectile& From, const FSyncedProjectile& To, const float& Alpha)
{
	*this = To;
}

void FSyncedProjectile::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("\n");
	Out.Appendf(" \nProjectile Class : %s\n", TCHAR_TO_ANSI(*GetNameSafe(ProjectileClass)));
	Out.Appendf(" \nID : %d\n", ProjectileData.ProjectileID);
	Out.Appendf(" \nSpawn Frame : %d\n", ProjectileData.SpawnFrame);
	Out.Appendf(" \nRelevant Change Frame : %d\n", ProjectileData.LastTrajectoryChangeFrame);
	Out.Appendf(" \nRelevant Change Loc : %s\n", TCHAR_TO_ANSI(*ProjectileData.LastRelevantLocation.ToCompactString()));
	Out.Appendf(" \nRelevant Change Vel : %s\n", TCHAR_TO_ANSI(*ProjectileData.LastRelevantVelocity.ToCompactString()));
	Out.Appendf(" \nRelevant Bounces : %d\n", ProjectileData.BouncesAtLastTrajectoryChange);
	Out.Appendf(" \nExploded : %s\n", ProjectileData.bExploded ? "true" : "false");
}

void FProjectilesCollection::NetSerialize(const FNetSerializeParams& Params)
{
	Params.Ar.SerializeIntPacked(SyncedProjectilesIDCount);
	
	uint32 Num = Params.Ar.IsSaving() ? Projectiles.Num() : 0;
	Params.Ar.SerializeIntPacked(Num);
	
	if (Num == 0)
	{
		Projectiles.Empty();
		return;
	}
	if (Params.Ar.IsLoading())
	{
		Projectiles.SetNum(Num);
	}
	for (uint32 i = 0; i < Num; ++i)
	{
		Projectiles[i].NetSerialize(Params);
	}
}
void FProjectilesCollection::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FProjectilesCollection* BaseState = Params.GetBaseDeltaState<FProjectilesCollection>();
	check(BaseState)
	
	bool SameIDCount = BaseState->SyncedProjectilesIDCount == SyncedProjectilesIDCount;
	Params.Ar.SerializeBits(&SameIDCount,1);
	if (SameIDCount)
	{
		SyncedProjectilesIDCount = BaseState->SyncedProjectilesIDCount;
	}
	else
	{
		Params.Ar.SerializeIntPacked(SyncedProjectilesIDCount);
	}
	// Num Serialized
	uint32 Num = Params.Ar.IsSaving() ? Projectiles.Num() : 0;
	Params.Ar.SerializeIntPacked(Num);
	if (Num == 0)
	{
		Projectiles.Empty();
		return;
	}
	if (Params.Ar.IsLoading())
	{
		Projectiles.SetNum(Num);
	}
	FNetSerializeParams DeltaParams = Params;
	for (uint32 i = 0; i < Num; ++i)
	{
		DeltaParams.BaseDeltaStatePtr = nullptr;
		FSyncedProjectile& SyncedProjectile = Projectiles[i];
		uint32 BaseStateIndex = 0;
		bool HasBaseState = false;
		// Try To Find the Base State only if saving and send its index if found
		if (Params.Ar.IsSaving())
		{
			for (int32 j = 0; j < BaseState->Projectiles.Num(); ++j)
			{
				const FSyncedProjectile& BaseStateSyncedProjectile = BaseState->Projectiles[j];
				if (BaseStateSyncedProjectile.ProjectileClass == SyncedProjectile.ProjectileClass
					&& BaseStateSyncedProjectile.ProjectileData.ProjectileID == SyncedProjectile.ProjectileData.ProjectileID)
				{
					BaseStateIndex = j;
					HasBaseState = true;
				}
			}
		}
		
		Params.Ar.SerializeBits(&HasBaseState,1);
		if (HasBaseState)
		{
			Params.Ar.SerializeIntPacked(BaseStateIndex);
			DeltaParams.BaseDeltaStatePtr = &BaseState->Projectiles[BaseStateIndex];
			SyncedProjectile.NetDeltaSerialize(DeltaParams);
		}
		else
		{
			SyncedProjectile.NetSerialize(DeltaParams);
		}
		
	}
}

bool FProjectilesCollection::ShouldReconcile(const FProjectilesCollection& AuthorityData) const
{
	if (Projectiles.Num() != AuthorityData.Projectiles.Num())
	{
		return true;
	}
	for (const auto& Projectile : Projectiles)
	{
		bool FoundInAuthority = false;
		for (const auto& AuthorityProjectile : AuthorityData.Projectiles)
		{
			if (AuthorityProjectile.ProjectileData.ProjectileID == Projectile.ProjectileData.ProjectileID)
			{
				FoundInAuthority = true;
				if (Projectile.ShouldReconcile(AuthorityProjectile))
				{
					return true;
				}
			}
		}

		if (!FoundInAuthority)
		{
			return true;
		}
	}
	return false;
}

void FProjectilesCollection::Interpolate(const FProjectilesCollection& From, const FProjectilesCollection& To,
	const float& Alpha)
{
	Projectiles = To.Projectiles;
}

void FProjectilesCollection::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Append("\n Projectiles :");
	Out.Appendf(" \nProjectiles ID count : %d\n", SyncedProjectilesIDCount);
	for (const FSyncedProjectile& Projectile : Projectiles)
	{
		Projectile.ToString(Out);
	}
}

void FProjectilesCollection::CollectState(const TArray<ASyncedProjectileBase*>& ProjectileInstances, const uint32& IDsCount)
{
	SyncedProjectilesIDCount = IDsCount;
	Projectiles.SetNum(ProjectileInstances.Num());
	for (int32 i = 0; i < Projectiles.Num(); ++i)
	{
		Projectiles[i].ProjectileClass = ProjectileInstances[i]->GetClass();
		Projectiles[i].ProjectileData = ProjectileInstances[i]->ProjectileData;
	}
}

FProjectileListScopeLock::FProjectileListScopeLock(UProjectilesSimulator& InProjectileData)
	: ProjectileSimulator(InProjectileData)
{
	ProjectileSimulator.IncrementProjectilesLockCount();
}

FProjectileListScopeLock::~FProjectileListScopeLock()
{
	ProjectileSimulator.DecrementProjectilesLockCount();
}
#pragma endregion
