// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/TargetingTypes/TargetingDataTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "TargetingLibrary.generated.h"

struct FGameplayTag;
class UNpAbilitySystemComponent;
class UTargetingProcessor;
struct FGameplayAbilityTargetDataHandle;
ABILITYSYSTEMSIMULATION_API DECLARE_LOG_CATEGORY_EXTERN(LogTargetingLibrary, Display, All);
/**
 * 
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UTargetingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="TargetingLibrary")
	static void  AddTargetDataToHandleFromHitResult(UPARAM(ref) FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,const FHitResult& HitResult);
	UFUNCTION(BlueprintCallable, Category="TargetingLibrary")
	static void AddTargetDataToHandleFromHitResults(UPARAM(ref) FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,const TArray<FHitResult>& HitResults);
	UFUNCTION(BlueprintCallable, Category="TargetingLibrary")
	static void AddTargetDataToHandleFromActors(UPARAM(ref) FGameplayAbilityTargetDataHandle& Handle,const FTransform& OriginTransform,const TArray<AActor*>& TargetActors, bool OneActorPerHandle = false);
	UFUNCTION(BlueprintCallable, Category="TargetingLibrary")
	/**
	 * this directly calls confirm targeting on the processor passing in ConfirmationTag, useful if processor can do different
	 * targeting types based on a tag
	 */
	static bool PerformTargetingFromProcessor(bool bEnableLagCompensation,UTargetingProcessor* Processor
		,UNpAbilitySystemComponent* OwnerASC,FTargetingData TargetingInputData,FGameplayAbilityTargetDataHandle& TargetDataHandle);

	UFUNCTION(BlueprintCallable,Category="TargetingLibrary")
	static bool ProjectWorldLocToScreen(const FVector& WorldLoc ,const FRotator& ControlRotation,const FVector& CameraLocation,const FSyncedScreenProjection& ScreenProjectionData, FVector2D& ScreenLoc);

	UFUNCTION(BlueprintCallable,Category="TargetingLibrary")
	static bool DeProjectScreenToWorld(const FVector& ScreenLoc ,const FRotator& ControlRotation,const FVector& CameraLocation
	,const FSyncedScreenProjection& ScreenProjectionData,FVector& OutWorldLoc, FVector& OutDirection);

	static bool GenerateAIScreenProjection(AActor* AIActor, FSceneViewProjectionData& ProjectionData, int32 ScreenWidth = 1080, int32 ScreenHeight = 1920);
	
	UFUNCTION(BlueprintPure,Category="TargetingLibrary")
	static FSyncedScreenProjection ComputeScreenProjectionData(AController* Controller);

	UFUNCTION(BlueprintPure,Category="TargetingLibrary")
	static FMatrix BuildScreenProjectionMatrix( FSyncedScreenProjection ProjectionData, FVector CameraLocation, FRotator CameraRotation);

	static void DrawSingleTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,
							const FRotator& Rotation, bool bHit, const FHitResult& OutHit, float DrawDebugDuration,
							const EDrawDebugTrace::Type& DrawDebugType,float MinDebugDur,const FColor& TraceHitColor,
							const FColor& TraceColor,const ETargetingTraceShape& TargetingShape,const float& Radius,
							const float& HalfHeight,const FVector& BoxExtent);

	static void DrawMultiTraceDebug(const UWorld* World, const FVector& Start, const FVector& End,const FRotator& Rotation,
						bool bHit, const TArray<FHitResult>& OutHits, float DrawDebugDuration,
						const EDrawDebugTrace::Type& DrawDebugType,float MinDebugDur,const FColor& TraceHitColor,
						const FColor& TraceColor,const ETargetingTraceShape& TargetingShape,const float& Radius,
						const float& HalfHeight,const FVector& BoxExtent);

	static void DrawOverlapDebug(const UWorld* World, const FVector& Start,const FRotator& Rotation, bool bHit,
								const TArray<FOverlapResult>& OverlapResults,float DrawDebugDuration,
								const EDrawDebugTrace::Type& DrawDebugType,float MinDebugDur,const FColor& TraceHitColor,
								const FColor& TraceColor,const ETargetingOverlapShape& OverlapShape,const float& Radius,
								const float& HalfHeight,const FVector& BoxExtent);
};
