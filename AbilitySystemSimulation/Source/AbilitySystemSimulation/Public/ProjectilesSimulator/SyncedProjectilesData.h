// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "ProjectileTrajectoryData.h"
#include "SyncedProjectilesData.generated.h"

class UProjectilesSimulator;
struct FNetSerializeParams;
class ASyncedProjectileBase;
/**
 * 
 */
#pragma region Networked data

USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FProjectileData
{
	GENERATED_BODY()

	UPROPERTY()
	uint32 ProjectileID = 0;
	UPROPERTY()
	FVector LastRelevantLocation = FVector::ZeroVector;
	UPROPERTY()
	FVector LastRelevantVelocity = FVector::ZeroVector;
	UPROPERTY()
	uint32 SpawnFrame = 0;
	UPROPERTY()
	bool bExploded = false;
	UPROPERTY()
	uint32 LastTrajectoryChangeFrame = 0;
	UPROPERTY()
	uint8 BouncesAtLastTrajectoryChange = 0;
	bool operator==(const FProjectileData& Other) const;
	/** Comparison operator */
	bool operator!=(const FProjectileData& Other) const;
};
// Hold Networked Data for a projectile
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FSyncedProjectile
{
	GENERATED_BODY()

	FSyncedProjectile() {};
	FSyncedProjectile(const TSubclassOf<ASyncedProjectileBase>& InProjectileClass,const FProjectileData& InProjectileData)
	{
		ProjectileClass = InProjectileClass;
		ProjectileData = InProjectileData;
	}
	UPROPERTY()
	TSubclassOf<ASyncedProjectileBase> ProjectileClass = nullptr;
	UPROPERTY()
	FProjectileData ProjectileData;

	void NetSerialize(const FNetSerializeParams& Params);
	void NetDeltaSerialize(const FNetSerializeParams& Params);
	bool ShouldReconcile(const FSyncedProjectile& AuthorityData) const;
	void Interpolate(const FSyncedProjectile& From,const FSyncedProjectile& To , const float& Alpha);
	void ToString(FAnsiStringBuilderBase& Out) const;

	static uint32 GetTypeHash(UClass* InClass, int32 ProjectileID)
	{
		uint64 Key = (static_cast<uint64>(reinterpret_cast<uintptr_t>(InClass)) << 32) 
					 | static_cast<uint32>(ProjectileID);
		return ::GetTypeHash(Key);
	}
};

// Takes care of serializing an array of projectiles.
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FProjectilesCollection
{
	GENERATED_BODY()

	FProjectilesCollection(){}
public:
	UPROPERTY()
	TArray<FSyncedProjectile> Projectiles;

	UPROPERTY()
	uint32 SyncedProjectilesIDCount = 0;

	void NetSerialize(const FNetSerializeParams& Params);
	void NetDeltaSerialize(const FNetSerializeParams& Params);
	bool ShouldReconcile(const FProjectilesCollection& AuthorityData) const;
	void Interpolate(const FProjectilesCollection& From,const FProjectilesCollection& To , const float& Alpha);
	void ToString(FAnsiStringBuilderBase& Out) const;
	void CollectState(const TArray<ASyncedProjectileBase*>& ProjectileInstances, const uint32& IDsCount);
};
#pragma endregion

struct ABILITYSYSTEMSIMULATION_API FProjectileListScopeLock
{
	FProjectileListScopeLock(UProjectilesSimulator& InProjectileData);
	~FProjectileListScopeLock();
private:
	UProjectilesSimulator& ProjectileSimulator;
};


// finalized


// non finalized


