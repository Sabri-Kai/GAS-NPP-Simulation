// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "TargetingProcessor.h"
#include "DataTypes/TargetingTypes/TargetingDataTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BasicTargetingProcessors.generated.h"




/**
 * Base Trace Processor, default trace processor, can return target data of type actor or hit result.
 * performs a trace based on default settings and broadcasts success whenever we find a target and on confirmation
 * this is the most basic implementation and it's expected for it to be subclassed with additional functionality
 * based on use case. ToDo : UTraceThenOverlapProcessor is an example that performs a trace and upon confirmation
 * performs an overlap from the trace hit location. more details below
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UTraceTargetingProcessor : public UTargetingProcessor
{
	GENERATED_BODY()
public:
	/**
	 *	Trace Shape Type
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace")
	ETargetingTraceShape TargetingShape = ETargetingTraceShape::ELine;
	
	/**
	 *	Type of target data this will return. HitResults or Actors.
	 *	if multi trace is true we hit multiple hit results or multiple actors
	 *	If Overlap is picked as TraceType the return target handle will automatically be of actor type
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace")
	ETargetDataHandleReturnType TargetDataReturnType = ETargetDataHandleReturnType::EHitResult;
	/**
	 * If Trace Type is Hit not a sweep, whether it is single or multi
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace")
	bool MultiTrace = false;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace")
	FCollisionProfileName CollisionProfile;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace")
	float TraceDistance = 10.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace",meta=(EditCondition = "TargetingShape == ETargetingTraceShape::EBox",EditConditionHides = "True"))
	FVector BoxExtent = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace",meta=(EditCondition = "TargetingShape == ETargetingTraceShape::ESphere || TargetingShape == ETargetingTraceShape::ECapsule",EditConditionHides = "True"))
	float Radius = 1.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Trace",meta=(EditCondition = "TargetingShape == ETargetingTraceShape::ECapsule",EditConditionHides = "True"))
	float HalfHeight = 1.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	float DrawDebugDuration = 0.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	FColor TraceColor = FColor::Red;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	FColor TraceHitColor = FColor::Green;
	
	virtual ETargetingResult OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc
			,const TArray<AActor*>& IgnoredActors
			,UPARAM(ref) FTargetingData& TargetingData
			,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;
	
	virtual ETargetingResult OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc
		,const FAbilitySystemTimeStep& TimeStep
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;
	
	virtual ETargetingResult OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;

	UFUNCTION(BlueprintCallable,Category="Targeting")
	bool TryAddTargetDataFromHitResults(const TArray<FHitResult>& Hits,const FTransform& Origin
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	UFUNCTION(BlueprintCallable,Category="Targeting")
	bool PerformTrace(UWorld* World,const FVector& Location,const FRotator& Rotation,const FVector& Direction
												, const TArray<AActor*>& ActorsToIgnore
												,TArray<FHitResult>& OutHits) const;
	
	void DrawMultiTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,const FRotator& Rotation
							, bool bHit, const TArray<FHitResult>& OutHits, float MinDebugDur = 0.f) const;
	void DrawSingleTraceDebug(const UWorld* World, const FVector& Start, const FVector& End
							,const FRotator& Rotation, bool bHit, const FHitResult& OutHit, float MinDebugDur = 0.f) const;
};

/**
 * Overlap Targeting processor :
 * Just like Trace, but it performs an overlap check instead. by default this processor can't return hit results and
 * target data will only have actors.
 */

UCLASS()
class ABILITYSYSTEMSIMULATION_API UOverlapTargetingProcessor : public UTargetingProcessor
{
	GENERATED_BODY()

public:
	
	/**
	 *	sweep Shape Type
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	ETargetingOverlapShape TargetingShape = ETargetingOverlapShape::ESphere;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	FCollisionProfileName CollisionProfile;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "TargetingShape == ETargetingOverlapShape::EBox",EditConditionHides = "True"))
	FVector BoxExtent = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "TargetingShape == ETargetingOverlapShape::ESphere || TargetingShape == ETargetingOverlapShape::ECapsule",EditConditionHides = "True"))
	float Radius = 1.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "TargetingShape == ETargetingOverlapShape::ECapsule",EditConditionHides = "True"))
	float HalfHeight = 1.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	bool IgnoreAvatarOwner = true;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	bool AddSavedActorsToIgnored = true;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	TEnumAsByte<EDrawDebugTrace::Type> DrawDebugType = EDrawDebugTrace::Type::None;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	float DrawDebugDuration = 0.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	FColor TraceColor = FColor::Red;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Debug")
	FColor TraceHitColor = FColor::Green;


	virtual ETargetingResult OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;
	
	virtual ETargetingResult OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc
		,const FAbilitySystemTimeStep& TimeStep
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;
	
	virtual ETargetingResult OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;
	

	UFUNCTION(BlueprintCallable,Category="Targeting")
	bool PerformOverlap(UWorld* World,const FVector& Location,const FRotator& Rotation
												,const TArray<AActor*>& ActorsToIgnore
												,TArray<AActor*>& ActorsOverlapped) const;
	
	
	void DrawOverlapDebug(const UWorld* World, const FVector& Start,const FRotator& Rotation,
						bool bHit, const TArray<FOverlapResult>& OverlapResults, float MinDebugDur = 0.f) const;
};


UCLASS()
class ABILITYSYSTEMSIMULATION_API UTraceThenOverlapTargetingProcessor : public UTraceTargetingProcessor
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	ETargetingOverlapShape OverlapShape = ETargetingOverlapShape::ESphere;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap")
	FCollisionProfileName OverlapCollisionProfile;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "OverlapShape == ETargetingOverlapShape::EBox",EditConditionHides = "True"))
	FVector OverlapBoxExtent = FVector::OneVector;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "OverlapShape == ETargetingOverlapShape::ESphere || OverlapShape == ETargetingOverlapShape::ECapsule",EditConditionHides = "True"))
	float OverlapRadius = 1.f;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",meta=(EditCondition = "OverlapShape == ETargetingOverlapShape::ECapsule",EditConditionHides = "True"))
	float OverlapHalfHeight = 1.f;

	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Overlap",Instanced)
	TArray<UAbilityTargetingFilter*> OverlapFilters;

	// start and execution are same as trace targeting, but on confirm we perform a trace, if it's successful
	// we perform an overlap at the hit location to find everyone in range
	
	virtual ETargetingResult OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const override;

	UFUNCTION(BlueprintCallable,Category="Targeting")
	bool PerformOverlap(UWorld* World,const FVector& Location,const FRotator& Rotation
												,const TArray<AActor*>& ActorsToIgnore
												,TArray<AActor*>& ActorsOverlapped) const;
	
	void DrawOverlapDebug(const UWorld* World, const FVector& Start,const FRotator& Rotation,
						bool bHit, const TArray<FOverlapResult>& OverlapResults, float MinDebugDur = 0.f) const;
};