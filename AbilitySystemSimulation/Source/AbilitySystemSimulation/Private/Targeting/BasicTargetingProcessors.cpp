// Fill out your copyright notice in the Description page of Project Settings.


#include "Targeting/BasicTargetingProcessors.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Library/TargetingLibrary.h"


#pragma region Trace Targeting processor

bool UTraceTargetingProcessor::PerformTrace(UWorld* World,const FVector& Location,const FRotator& Rotation,const FVector& Direction
                                                           ,const TArray<AActor*>& ActorsToIgnore,TArray<FHitResult>& OutHits) const
{
	const FVector End = Location + (Direction * FMath::Abs(TraceDistance));// ensure trace distance can't change direction
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UTraceTargetingProcessor), false);
	Params.AddIgnoredActors(ActorsToIgnore);
	if (TargetingShape == ETargetingTraceShape::ELine)
	{
		if (MultiTrace)
		{
			const bool bBlockingHit = World->LineTraceMultiByProfile(OutHits,Location,End,CollisionProfile.Name,Params);
			DrawMultiTraceDebug(World,Location,End,Rotation,bBlockingHit,OutHits);
			return bBlockingHit;
		}

		FHitResult Hit;
		const bool bBlockingHit = World->LineTraceSingleByProfile(Hit,Location,End,CollisionProfile.Name,Params);
		DrawSingleTraceDebug(World,Location,End,Rotation,bBlockingHit,Hit);
		OutHits.Add(Hit);
		return bBlockingHit;
	}

	FCollisionShape CollisionShape;
	switch (TargetingShape)
	{
	case ETargetingTraceShape::ESphere:
		{
			CollisionShape = FCollisionShape::MakeSphere(Radius);
			break;
		}
	case ETargetingTraceShape::ECapsule:
		{
			CollisionShape = FCollisionShape::MakeCapsule(Radius,HalfHeight);
			break;
		}
	case ETargetingTraceShape::EBox:
		{
			CollisionShape = FCollisionShape::MakeBox(BoxExtent * 0.5f);
			break;
		}
	case ETargetingTraceShape::ELine:
		{
			break; // line shouldn't get here and custom is not yet implemented
		}
	}
	
	if (MultiTrace)
	{
		const bool bBlockingHit = World->SweepMultiByProfile(OutHits,Location,End,Rotation.Quaternion(),CollisionProfile.Name,CollisionShape,Params);
		DrawMultiTraceDebug(World,Location,End,Rotation,bBlockingHit,OutHits);
		return bBlockingHit;
	}
	FHitResult Hit;
	const bool bBlockingHit = World->SweepSingleByProfile(Hit,Location,End,Rotation.Quaternion(),CollisionProfile.Name,CollisionShape,Params);
	DrawSingleTraceDebug(World,Location,End,Rotation,bBlockingHit,Hit);
	OutHits.Add(Hit);
	return bBlockingHit;
}

ETargetingResult UTraceTargetingProcessor::OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc,const TArray<AActor*>& IgnoredActors,
	FTargetingData& TargetingData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	
	TArray<FHitResult> Hits;
	const bool SuccessfulTrace = PerformTrace(World,TargetingData.Location,TargetingData.Rotation,TargetingData.Direction,IgnoredActors,Hits);
	if (!SuccessfulTrace || Hits.Num() <= 0)
	{
		return ETargetingResult::EContinue;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	FilterHitResults(Hits,OwningAsc,DefaultFilters);
	
	if (TryAddTargetDataFromHitResults(Hits,OriginTransform,OutTargetDataHandle))
	{
		return ETargetingResult::ESuccessOnGoing;
	}
	return ETargetingResult::EContinue;
}

ETargetingResult UTraceTargetingProcessor::OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc,
	const FAbilitySystemTimeStep& TimeStep,const TArray<AActor*>& IgnoredActors, const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS,
	FTargetingData& TargetingData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	TArray<FHitResult> Hits;
	const bool SuccessfulTrace = PerformTrace(World,TargetingData.Location,TargetingData.Rotation,TargetingData.Direction,IgnoredActors,Hits);
	if (!SuccessfulTrace || Hits.Num() <= 0)
	{
		return ETargetingResult::EContinue;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	FilterHitResults(Hits,OwningAsc,DefaultFilters);
	if (TryAddTargetDataFromHitResults(Hits,OriginTransform,OutTargetDataHandle))
	{
		return ETargetingResult::ESuccessOnGoing;
	}
	return ETargetingResult::EContinue;
}

ETargetingResult UTraceTargetingProcessor::OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc,const TArray<AActor*>& IgnoredActors,
	const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	
	TArray<FHitResult> Hits;
	const bool SuccessfulTrace = PerformTrace(World,TargetingData.Location,TargetingData.Rotation,TargetingData.Direction,IgnoredActors,Hits);
	if (!SuccessfulTrace || Hits.Num() <= 0)
	{
		return ETargetingResult::EEnd;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	FilterHitResults(Hits,OwningAsc,DefaultFilters);
	if (TryAddTargetDataFromHitResults(Hits,OriginTransform,OutTargetDataHandle))
	{
		return ETargetingResult::ESuccess;
	}
	return ETargetingResult::EEnd;
}


bool UTraceTargetingProcessor::TryAddTargetDataFromHitResults(const TArray<FHitResult>& Hits,const FTransform& Origin,
                                                              FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	if (Hits.Num() > 0)
	{
		switch (TargetDataReturnType)
		{
		case ETargetDataHandleReturnType::EActor:
			{
				// make actors array.
				TArray<AActor*> Actors;
				for (int32 i = 0; i < Hits.Num(); i++)
				{
					if (Hits[i].GetActor() != nullptr)
					{
						Actors.AddUnique(Hits[i].GetActor());
					}
				}
				if (Actors.Num() > 0)
				{
					UTargetingLibrary::AddTargetDataToHandleFromActors(OutTargetDataHandle,Origin,Actors);
					return true;
				}
				return false;
			}
		case ETargetDataHandleReturnType::EHitResult:
			{
				if (Hits.Num() > 0)
				{
					if (Hits.Num() == 1)
					{
						UTargetingLibrary::AddTargetDataToHandleFromHitResult(OutTargetDataHandle,Origin,Hits[0]);
						return true;
					}
					UTargetingLibrary::AddTargetDataToHandleFromHitResults(OutTargetDataHandle,Origin,Hits);                                                     
					return true;
				}
			}
		}
	}
	return false;
}



void UTraceTargetingProcessor::DrawMultiTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,const FRotator& Rotation,
	bool bHit, const TArray<FHitResult>& OutHits,float MinDebugDur) const
{
	UTargetingLibrary::DrawMultiTraceDebug(World,Start,End,Rotation,bHit,OutHits,DrawDebugDuration,DrawDebugType,MinDebugDur,
				TraceHitColor,TraceColor,TargetingShape,Radius,HalfHeight,BoxExtent);
}

void UTraceTargetingProcessor::DrawSingleTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,
	const FRotator& Rotation, bool bHit, const FHitResult& OutHit,float MinDebugDur) const
{
	UTargetingLibrary::DrawSingleTraceDebug(World,Start,End,Rotation,bHit,OutHit,DrawDebugDuration,DrawDebugType,MinDebugDur,
			TraceHitColor,TraceColor,TargetingShape,Radius,HalfHeight,BoxExtent);
}
#pragma endregion

#pragma region Overlap Targeting processor

ETargetingResult UOverlapTargetingProcessor::OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc,const TArray<AActor*>& IgnoredActors,
	FTargetingData& TargetingData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	TArray<AActor*> OverlappedActors;
	PerformOverlap(World,TargetingData.Location,TargetingData.Rotation,IgnoredActors,OverlappedActors);
	FilterActors(OverlappedActors,OwningAsc,DefaultFilters);
	if (OverlappedActors.Num() <= 0)
	{
		return ETargetingResult::EContinue;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	UTargetingLibrary::AddTargetDataToHandleFromActors(OutTargetDataHandle,OriginTransform,OverlappedActors);
	return ETargetingResult::ESuccessOnGoing;
}

ETargetingResult UOverlapTargetingProcessor::OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc,
	const FAbilitySystemTimeStep& TimeStep,const TArray<AActor*>& IgnoredActors, const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS,
	FTargetingData& TargetingData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	
	TArray<AActor*> OverlappedActors;
	PerformOverlap(World,TargetingData.Location,TargetingData.Rotation,IgnoredActors,OverlappedActors);
	FilterActors(OverlappedActors,OwningAsc,DefaultFilters);
	if (OverlappedActors.Num() <= 0)
	{
		return ETargetingResult::EContinue;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	UTargetingLibrary::AddTargetDataToHandleFromActors(OutTargetDataHandle,OriginTransform,OverlappedActors);
	return ETargetingResult::ESuccessOnGoing;
}

ETargetingResult UOverlapTargetingProcessor::OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
	,const TArray<AActor*>& IgnoredActors,const float& CurrentDurationMS
	, const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	
	TArray<AActor*> OverlappedActors;
	PerformOverlap(World,TargetingData.Location,TargetingData.Rotation,IgnoredActors,OverlappedActors);
	FilterActors(OverlappedActors,OwningAsc,DefaultFilters);
	if (OverlappedActors.Num() <= 0)
	{
		return ETargetingResult::EEnd;
	}
	const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
	UTargetingLibrary::AddTargetDataToHandleFromActors(OutTargetDataHandle,OriginTransform,OverlappedActors);
	return ETargetingResult::ESuccess;
}

bool UOverlapTargetingProcessor::PerformOverlap(UWorld* World, const FVector& Location,
                                                const FRotator& Rotation, const TArray<AActor*>& ActorsToIgnore,
                                                TArray<AActor*>& ActorsOverlapped) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UInstantTargetingPredictionTask), false);
	Params.AddIgnoredActors(ActorsToIgnore);
	FCollisionShape CollisionShape;
	switch (TargetingShape)
	{
	case ETargetingOverlapShape::ESphere:
		{
			CollisionShape = FCollisionShape::MakeSphere(Radius);
			break;
		}
	case ETargetingOverlapShape::ECapsule:
		{
			CollisionShape = FCollisionShape::MakeCapsule(Radius,HalfHeight);
			break;
		}
	case ETargetingOverlapShape::EBox:
		{
			CollisionShape = FCollisionShape::MakeBox(BoxExtent * 0.5f);
			break;
		}
	}
	TArray<FOverlapResult> OverlapResults;
	const bool bHit = World->OverlapMultiByProfile(OverlapResults,Location,Rotation.Quaternion(),CollisionProfile.Name,CollisionShape,Params);
	DrawOverlapDebug(World,Location,Rotation,bHit,OverlapResults);
	if (bHit && OverlapResults.Num() > 0)
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			if (Overlap.GetActor())
			{
				ActorsOverlapped.Add(Overlap.GetActor());
			}
		}
	}
	return bHit;
}


void UOverlapTargetingProcessor::DrawOverlapDebug(const UWorld* World, const FVector& Start,const FRotator& Rotation,
                                                         bool bHit, const TArray<FOverlapResult>& OverlapResults, float MinDebugDur) const
{
	UTargetingLibrary::DrawOverlapDebug(World,Start,Rotation,bHit,OverlapResults,DrawDebugDuration,DrawDebugType,MinDebugDur,
			TraceHitColor,TraceColor,TargetingShape,Radius,HalfHeight,BoxExtent);
}
#pragma endregion 

#pragma region Trace Then Overlap Targeting Processor
bool UTraceThenOverlapTargetingProcessor::PerformOverlap(UWorld* World, const FVector& Location,
	const FRotator& Rotation,const TArray<AActor*>& ActorsToIgnore,
	TArray<AActor*>& ActorsOverlapped) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(UInstantTargetingPredictionTask), false);
	Params.AddIgnoredActors(ActorsToIgnore);
	FCollisionShape CollisionShape;
	switch (OverlapShape)
	{
	case ETargetingOverlapShape::ESphere:
		{
			CollisionShape = FCollisionShape::MakeSphere(OverlapRadius);
			break;
		}
	case ETargetingOverlapShape::ECapsule:
		{
			CollisionShape = FCollisionShape::MakeCapsule(OverlapRadius,OverlapHalfHeight);
			break;
		}
	case ETargetingOverlapShape::EBox:
		{
			CollisionShape = FCollisionShape::MakeBox(OverlapBoxExtent * 0.5f);
			break;
		}
	}
	TArray<FOverlapResult> OverlapResults;
	const bool bHit = World->OverlapMultiByProfile(OverlapResults,Location,Rotation.Quaternion(),CollisionProfile.Name,CollisionShape,Params);
	DrawOverlapDebug(World,Location,Rotation,bHit,OverlapResults);
	if (bHit && OverlapResults.Num() > 0)
	{
		for (const FOverlapResult& Overlap : OverlapResults)
		{
			if (Overlap.GetActor())
			{
				ActorsOverlapped.Add(Overlap.GetActor());
			}
		}
	}
	return bHit;
}

ETargetingResult UTraceThenOverlapTargetingProcessor::OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
	,const TArray<AActor*>& IgnoredActors,const float& CurrentDurationMS
	,const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	UWorld* World = GetWorld();
	if (!World || !OwningAsc)
	{
		return ETargetingResult::EAbort;
	}
	TArray<FHitResult> Hits;
	// perform trace without filtering. filters are for overlap. find first thing that blocks us
	const bool SuccessfulTrace = PerformTrace(World,TargetingData.Location,TargetingData.Rotation,TargetingData.Direction,IgnoredActors,Hits);
	// latest Hit result is the blocking one
	if (SuccessfulTrace && Hits.Num() > 0 && Hits.Last().bBlockingHit)
	{
		// if we have valid hits, use the impact location of the last one as start from now on.
		// this will modify the start location for all targeting from this point on.
		TargetingData.Location = Hits.Last().ImpactPoint;
		TArray<AActor*> HitActors;
		PerformOverlap(World,TargetingData.Location,TargetingData.Rotation,TargetingData.SavedHitActors,HitActors);
		FilterActors(HitActors,OwningAsc,OverlapFilters);
		if (HitActors.Num() <= 0)
		{
			return ETargetingResult::EEnd;
		}
		const FTransform OriginTransform = FTransform(TargetingData.Rotation, TargetingData.Location);
		UTargetingLibrary::AddTargetDataToHandleFromActors(OutTargetDataHandle,OriginTransform,HitActors);
		return ETargetingResult::ESuccess;
	}
	return ETargetingResult::EEnd;
}

void UTraceThenOverlapTargetingProcessor::DrawOverlapDebug(const UWorld* World, const FVector& Start,
                                                           const FRotator& Rotation, bool bHit,
                                                           const TArray<FOverlapResult>& OverlapResults,
                                                           float MinDebugDur) const
{
	UTargetingLibrary::DrawOverlapDebug(World,Start,Rotation,bHit,OverlapResults,DrawDebugDuration,DrawDebugType,MinDebugDur,
		TraceHitColor,TraceColor,OverlapShape,OverlapRadius,OverlapHalfHeight,OverlapBoxExtent);
}
#pragma endregion 