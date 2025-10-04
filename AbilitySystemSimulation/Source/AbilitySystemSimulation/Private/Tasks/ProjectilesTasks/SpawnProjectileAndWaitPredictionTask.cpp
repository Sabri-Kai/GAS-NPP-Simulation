// 2025 Yohoho Productions /  Sirkai


#include "Tasks/ProjectilesTasks/SpawnProjectileAndWaitPredictionTask.h"

#include "Abilities/NpGameplayAbility.h"

#define LOCTEXT_NAMESPACE "SpawnProjectileAndWaitTask"
#pragma region Spawn projectile Task Data

bool FSpawnProjectileTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	Params.Ar.SerializeIntPacked(ProjectileID);
	return true;
}

bool FSpawnProjectileTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FSpawnProjectileTaskData* BaseData = Params.GetBaseDeltaState<FSpawnProjectileTaskData>();
	check(BaseData);
	bool bSameCounter = Params.Ar.IsSaving() ? ProjectileID == BaseData->ProjectileID : false;
	Params.Ar.SerializeBits(&bSameCounter, 1);
	if (bSameCounter)
	{
		ProjectileID = BaseData->ProjectileID;
	}
	else
	{
		Params.Ar.SerializeIntPacked(ProjectileID);
	}
	return true;
}

UScriptStruct* FSpawnProjectileTaskData::GetScriptStruct() const
{
	return FSpawnProjectileTaskData::StaticStruct();
}

void FSpawnProjectileTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	Out.Appendf("Projectile ID : %d",ProjectileID);
}

bool FSpawnProjectileTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FSpawnProjectileTaskData* AuthState = static_cast<const FSpawnProjectileTaskData*>(&AuthorityState);
	return ProjectileID != AuthState->ProjectileID;
}

void FSpawnProjectileTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FSpawnProjectileTaskData* ToState = static_cast<const FSpawnProjectileTaskData*>(&To);
	ProjectileID = ToState->ProjectileID;
}

#pragma endregion

#pragma region Spawn projectile Task Class

USpawnProjectileAndWaitPredictionTask::USpawnProjectileAndWaitPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = FSpawnProjectileTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(USpawnProjectileAndWaitPredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}
void USpawnProjectileAndWaitPredictionTask::ExecuteTask(TSubclassOf<ASyncedProjectileBase> ProjectileClass ,
	FVector SpawnLocation, FVector Direction)
{
	if (!GetOwningAbility() || !GetAbilitySystemComponent() || IsInRollback())
	{
		return ;
	}
	UProjectilesSimulator* Simulator = GetAbilitySystemComponent()->GetProjectilesSimulator();
	if (!Simulator)
	{
		return ;
	}
	ASyncedProjectileBase* SpawnedProjectile = GetAbilitySystemComponent()->SpawnProjectile(ProjectileClass, SpawnLocation, Direction);
	ProjectileID = SpawnedProjectile->ProjectileData.ProjectileID;
	OnProjectileExplodeHandle = Simulator->OnProjectileExplode.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileExplode);
	OnProjectileBounceHandle = Simulator->OnProjectileBounce.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileBounce);
	OnProjectilePierceHandle = Simulator->OnProjectilePierce.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectilePierce);
	OnProjectileEndOfLifeHandle = Simulator->OnProjectileEndOfLife.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileEndOfLife);
	ActivateTask();
}

void USpawnProjectileAndWaitPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FSpawnProjectileTaskData* ProjectileTaskData = static_cast<const FSpawnProjectileTaskData*>(DataToRead.Get());
	ProjectileID = ProjectileTaskData->ProjectileID;
}

void USpawnProjectileAndWaitPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FSpawnProjectileTaskData* ProjectileTaskData = static_cast< FSpawnProjectileTaskData*>(DataToWrite.Get());
	ProjectileTaskData->ProjectileID = ProjectileID ;
}

void USpawnProjectileAndWaitPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	// different activation state , make sure delegates are bound or cleared accordingly
	UProjectilesSimulator* Simulator = GetAbilitySystemComponent()->GetProjectilesSimulator();
	if (!Simulator)
	{
		return ;
	}
	if (AuthoritySyncData.IsActive != IsActive())
	{
		if (AuthoritySyncData.IsActive)
		{
			if (!OnProjectileExplodeHandle.IsValid())
			{
				OnProjectileExplodeHandle = Simulator->OnProjectileExplode.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileExplode);
			}
			if (!OnProjectileBounceHandle.IsValid())
			{
				OnProjectileBounceHandle =  Simulator->OnProjectileBounce.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileBounce);
			}
			if (!OnProjectilePierceHandle.IsValid())
			{
				OnProjectilePierceHandle = Simulator->OnProjectilePierce.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectilePierce);
			}
			if (!OnProjectileEndOfLifeHandle.IsValid())
			{
				OnProjectileEndOfLifeHandle = Simulator->OnProjectileEndOfLife.AddUObject(this,&USpawnProjectileAndWaitPredictionTask::OnProjectileEndOfLife);
			}
		}
		else // we are not supposed to be active , clear delegates
		{
			if (OnProjectileExplodeHandle.IsValid())
			{
				Simulator->OnProjectileExplode.Remove(OnProjectileExplodeHandle);
				OnProjectileExplodeHandle.Reset();
			}
			if (OnProjectileBounceHandle.IsValid())
			{
				Simulator->OnProjectileBounce.Remove(OnProjectileBounceHandle);
				OnProjectileBounceHandle.Reset();
			}
			if (OnProjectilePierceHandle.IsValid())
			{
				Simulator->OnProjectilePierce.Remove(OnProjectilePierceHandle);
				OnProjectilePierceHandle.Reset();
			}
			if (OnProjectileEndOfLifeHandle.IsValid())
			{
				Simulator->OnProjectileEndOfLife.Remove(OnProjectileEndOfLifeHandle);
				OnProjectileEndOfLifeHandle.Reset();
			}
		}
	}
}


ASyncedProjectileBase* USpawnProjectileAndWaitPredictionTask::GetSpawnedProjectileInstance() const
{
	if (!GetAbilitySystemComponent())
	{
		return nullptr;
	}
	return GetAbilitySystemComponent()->GetProjectileInstanceByID(ProjectileID);
}

void USpawnProjectileAndWaitPredictionTask::OnProjectileExplode(const FHitBroadcastData& Data, const uint32& ID)
{
	if (!ShouldTriggerCallbacks() || ProjectileID != ID)
	{
		return;
	}
	OnExplode.Broadcast(Data);
}

void USpawnProjectileAndWaitPredictionTask::OnProjectileBounce(const FHitBroadcastData& Data, const uint32& ID)
{
	if (!ShouldTriggerCallbacks() || ProjectileID != ID)
	{
		return;
	}
	OnBounce.Broadcast(Data);
}

void USpawnProjectileAndWaitPredictionTask::OnProjectilePierce(const FHitBroadcastData& Data, const uint32& ID)
{
	if (!ShouldTriggerCallbacks() || ProjectileID != ID)
	{
		return;
	}
	OnPierce.Broadcast(Data);
}

void USpawnProjectileAndWaitPredictionTask::OnProjectileEndOfLife(const FHitBroadcastData& Data, const uint32& ID)
{
	if (!ShouldTriggerCallbacks() || ProjectileID != ID)
	{
		return;
	}
	OnEndOfLife.Broadcast(Data);
}

FText USpawnProjectileAndWaitPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("SpawnProjectileAndWaitPredictionTask", "Spawn Projectile And Wait Prediction Task");
}

FLinearColor USpawnProjectileAndWaitPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.694f, 0.8f, 0.325f, 1.0f);
}

void USpawnProjectileAndWaitPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	UProjectilesSimulator* Simulator = GetAbilitySystemComponent()->GetProjectilesSimulator();
	if (!Simulator)
	{
		return ;
	}
	if (OnProjectileExplodeHandle.IsValid())
	{
		Simulator->OnProjectileExplode.Remove(OnProjectileExplodeHandle);
		OnProjectileExplodeHandle.Reset();
	}
	if (OnProjectileBounceHandle.IsValid())
	{
		Simulator->OnProjectileBounce.Remove(OnProjectileBounceHandle);
		OnProjectileBounceHandle.Reset();
	}
	if (OnProjectilePierceHandle.IsValid())
	{
		Simulator->OnProjectilePierce.Remove(OnProjectilePierceHandle);
		OnProjectilePierceHandle.Reset();
	}
	if (OnProjectileEndOfLifeHandle.IsValid())
	{
		Simulator->OnProjectileEndOfLife.Remove(OnProjectileEndOfLifeHandle);
		OnProjectileEndOfLifeHandle.Reset();
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE