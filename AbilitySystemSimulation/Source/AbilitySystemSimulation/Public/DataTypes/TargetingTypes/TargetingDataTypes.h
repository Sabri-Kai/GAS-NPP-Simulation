// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "TargetingDataTypes.generated.h"

class UNpAbilitySystemComponent;
struct FNetSerializeParams;
/**
 * 
 */
UENUM(BlueprintType)
enum class ETargetingTraceShape : uint8
{
	ELine UMETA(DisplayName="Line"),
	ESphere UMETA(DisplayName="sphere"),
	ECapsule UMETA(DisplayName="Capsule"),
	EBox UMETA(DisplayName="Box"),
};

UENUM(BlueprintType)
enum class ETargetingOverlapShape : uint8
{
	ESphere UMETA(DisplayName="sphere"),
	ECapsule UMETA(DisplayName="Capsule"),
	EBox UMETA(DisplayName="Box"),
};
UENUM(BlueprintType)
enum class ETargetDataHandleReturnType : uint8
{
	EHitResult,
	EActor,
};
/**
 * Direction Override Type used by targeting processors
 */
UENUM(BlueprintType)
enum class EDirectionOverrideType : uint8
{
	ENone,
	EActorRotation,
	EControlRotation,
};
UENUM(BlueprintType)
enum class ETargetingResult : uint8
{
	/** Nothing happens just continue */
	EContinue,
	/** Targeting Successful with valid data and done, Broadcast OnConfirmed and ends the task*/
	ESuccess,
	/** Targeting Successful with valid data but not done, Broadcast OnOngoing and keep going */
	ESuccessOnGoing,
	/** End Targeting , Broadcast OnEnd with canceled false*/
	EEnd,
	/** Cancel Targeting and make the task go through cancellation as well*/
	EAbort,
};

UENUM(BlueprintType)
enum class ETargetingEvent : uint8
{
	EStart,
	EExecution,
	EConfirmation,
	ECancelation,
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncedScreenProjection
{
	GENERATED_BODY()
	
public:
	
	FSyncedScreenProjection(){}
	

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CameraInput)
	float FOV = 0.f;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = CameraInput)
	float AspectRatio = 1.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Mover)
	FVector2D ViewSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Mover)
	FVector2D ViewMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Mover)
	bool bMaintainXFOV = false;
	
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	bool operator==(const FSyncedScreenProjection& Other) const;
	bool operator!=(const FSyncedScreenProjection& Other) const;

	void ToString(FAnsiStringBuilderBase& Out) const ;

	static FMatrix BuildScreenProjectionMatrix(const FSyncedScreenProjection& Data
		, const FRotator& ControlRotation,const FVector& CameraLocation);
};

template<>
struct TStructOpsTypeTraits< FSyncedScreenProjection> : public TStructOpsTypeTraitsBase2< FSyncedScreenProjection>
{
	enum
	{
		WithNetSerializer = true,
	};
};


USTRUCT(BlueprintType)
struct FTargetingInputParams
{
	GENERATED_USTRUCT_BODY()
	
	FTargetingInputParams(){}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	TObjectPtr<UNpAbilitySystemComponent> OwningAsc = nullptr;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	int32 CurrentDuration = 0.f;

	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	float InTimeSinceLastConfirm = 0.f;
	
};

USTRUCT(BlueprintType)
struct FTargetingData
{
	GENERATED_USTRUCT_BODY()

	FTargetingData(){}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	FVector Direction = FVector::ZeroVector;
	UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = Targeting)
	TArray<AActor*> SavedHitActors;
	UPROPERTY()
	uint32 StartSimTime = 0;
	UPROPERTY()
	uint32 LastConfirmationTime = 0;	
};
