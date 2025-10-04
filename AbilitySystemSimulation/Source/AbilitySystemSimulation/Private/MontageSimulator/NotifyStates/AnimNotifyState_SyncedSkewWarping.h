// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MotionWarpingMoverAdapter.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "MontageSimulator/NetMontageSimulatorData.h"
#include "MontageSimulator/SyncedNotifyInterface.h"
#include "AnimNotifyState_SyncedSkewWarping.generated.h"

class URootMotionModifier_SkewWarp;
/**
 * 
 */
USTRUCT(BlueprintType)
struct FProcessorSetupData
{
	GENERATED_BODY()

	FProcessorSetupData(){}
	
	FProcessorSetupData(UNetMontageSimulator* InMontagePlayer,UAnimMontage* InMontage,const float& InStartTime
		, const float& InEndTime, const float& InCurrentTime)
		: MontagePlayer(InMontagePlayer)
		, AnimMontage(InMontage)
		, NotifyStartTime(InStartTime)
		, NotifyEndTime(InEndTime)
		, CurrentMontageTime(InCurrentTime)	 
	{
		
	}

	explicit FProcessorSetupData(const FSimTickNotifyData& InSimInput)
		: MontagePlayer(InSimInput.MontagePlayer)
		, AnimMontage(InSimInput.AnimMontage)
		, NotifyStartTime(InSimInput.NotifyStartTime)
		, NotifyEndTime(InSimInput.NotifyEndTime)
		, CurrentMontageTime(InSimInput.CurrentMontageTime)	 
	{
		
	}

	explicit FProcessorSetupData(const FRestoreNotifyData& InSimInput)
		: MontagePlayer(InSimInput.MontagePlayer)
		, AnimMontage(InSimInput.AnimMontage)
		, NotifyStartTime(InSimInput.NotifyStartTime)
		, NotifyEndTime(InSimInput.NotifyEndTime)
		, CurrentMontageTime(InSimInput.CurrentMontageTime)	 
	{
		
	}
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs)
	TObjectPtr<UNetMontageSimulator> MontagePlayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs)
	TObjectPtr<UAnimMontage> AnimMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs)
	float NotifyStartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs)
	float NotifyEndTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Inputs)
	float CurrentMontageTime = 0.f;
	
};
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FWarpingNotifySyncData : public FSyncedNotifyData
{
	GENERATED_USTRUCT_BODY()
	FWarpingNotifySyncData();
	virtual ~FWarpingNotifySyncData() {}
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	AActor* TargetActor = nullptr;
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	FVector StartLocation = FVector::ZeroVector;
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	FRotator TargetRotation = FRotator::ZeroRotator;
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	bool WarpTranslation = true;
	UPROPERTY(BlueprintReadWrite,Category=SyncedWarping)
	bool WarpRotation = true;
	// @return newly allocated copy of this FNotifySyncStateBase. Must be overridden by child classes
	virtual FSyncedNotifyData* Clone() const override;
	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual bool ShouldReconcile(const FSyncedNotifyData& AuthorityState) const override;
	virtual void Interpolate(const FSyncedNotifyData& From, const FSyncedNotifyData& To, float Pct) override;
};
template<>
struct TStructOpsTypeTraits< FWarpingNotifySyncData > : public TStructOpsTypeTraitsBase2< FWarpingNotifySyncData >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};

UCLASS(Blueprintable,BlueprintType,editinlinenew)
class ABILITYSYSTEMSIMULATION_API USyncedSkewWarpingProcessor : public UObject
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	EWarpPointAnimProvider WarpPointAnimProvider = EWarpPointAnimProvider::None;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "WarpPointAnimProvider == EWarpPointAnimProvider::Static",EditConditionHides))
	FTransform WarpPointAnimTransform = FTransform::Identity;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "WarpPointAnimProvider == EWarpPointAnimProvider::Bone",EditConditionHides))
	FName WarpPointAnimBoneName = NAME_None;

	/** Whether to warp the translation component of the root motion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	bool bWarpTranslation = true;

	/** Whether to ignore the Z component of the translation. Z motion will remain untouched */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpTranslation"))
	bool bIgnoreZAxis = true;

	/** Easing function used when adding translation. Only relevant when there is no translation in the animation */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpTranslation"))
	EAlphaBlendOption AddTranslationEasingFunc = EAlphaBlendOption::Linear;

	/** Custom curve used to add translation when there is none to warp. Only relevant when AddTranslationEasingFunc is set to Custom */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "AddTranslationEasingFunc==EAlphaBlendOption::Custom", EditConditionHides))
	TObjectPtr<class UCurveFloat> AddTranslationEasingCurve = nullptr;

	/** Whether to warp the rotation component of the root motion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config")
	bool bWarpRotation = true;

	/** Whether rotation should be warp to match the rotation of the sync point or to face the sync point */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	EMotionWarpRotationType RotationType = EMotionWarpRotationType::Default;
	
	/** The method of rotation to use */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "bWarpRotation"))
	EMotionWarpRotationMethod RotationMethod = EMotionWarpRotationMethod::Slerp;

	/**
	 * Allow to modify how fast the rotation is warped.
	 * e.g if the window duration is 2sec and this is 0.5, the target rotation will be reached in 1sec instead of 2sec
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "RotationMethod!=EMotionWarpRotationMethod::ConstantRate && bWarpRotation"))
	float WarpRotationTimeMultiplier = 1.f;

	/** Maximum rotation rate in degrees/sec. Will be the value used in constant rotation rate*/
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Config", meta = (EditCondition = "RotationMethod!=EMotionWarpRotationMethod::Slerp && bWarpRotation"))
	float WarpMaxRotationRate = 0.f;

	/*
	 *If Set to 0 will draw debug for 1 frame , if set to negative, will disable debug draw
	 */
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Config",meta=(ClampMin = -1.f))
	float DrawDebugDuration = -1.f;
	
	UFUNCTION()
	FWarpingNotifySyncData InitializeTargetData_Internal(const bool IsReSimulating,const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);
	
	
	
	UFUNCTION()
	FWarpingNotifySyncData UpdateTargetData_Internal(const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);

	
	
	// Modified version of URootMotionModifier_SkewWarp::ProcessRootMotion
	virtual FTransform ProcessRootMotion(float DeltaSeconds,const FTransform& InRootMotion, const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput);

	virtual FTransform FinalizeTargetTransform(const FTransform& CurrentTransform,const FTransform& TargetTransform);

	static FTransform CalculateRootTransformRelativeToWarpPointAtTime(const FTransform& BaseMeshTransform, const UAnimSequenceBase* Animation, float Time, const FTransform& WarpPointTransform);
	static FTransform CalculateRootTransformRelativeToWarpPointAtTime(const UAnimInstance& AnimInstance,const UAnimSequenceBase* Animation
		, const FTransform& BaseMeshTransform,float Time, const FName& WarpPointBoneName);
	// Copied From URootMotionModifier_SkewWarp
	virtual FQuat WarpRotation(float DeltaSeconds, const FQuat& TargetRotation,
	const FTransform& RootMotionDelta, const FTransform& RootMotionTotal, const FSimTickNotifyData& SimInput) const;
	virtual FVector WarpTranslation(const FTransform& CurrentTransform, const FVector& DeltaTranslation,
							const FVector& TotalTranslation, const FVector& TargetLocation);
	// called on the very start of the notify holding this processor in begin simulation tick
	// and in Restore frame to ensure that the processor gets setup if it gets restored to active.
	// this is used just to cache static values calculated once and don't need to be synced.
	// Use InitializeTargetData for initialization of synced data
	void SetupProcessor(const FProcessorSetupData& SimInput);
protected:
	/** Cached of the offset from the warp target. Used to calculate the final target transform when a warp target is defined in the animation */
	TOptional<FTransform> CachedOffsetFromWarpPoint;

	UFUNCTION()
	virtual FWarpingNotifySyncData InitializeTargetData(const bool IsReSimulating,const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);

	UFUNCTION(BlueprintImplementableEvent,Category="SyncedSkewWarp",DisplayName="Initialize Target Data")
	FWarpingNotifySyncData K2_InitializeTargetData(const bool IsReSimulating,const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);

	UFUNCTION()
	virtual FWarpingNotifySyncData UpdateTargetData(const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);
	
	UFUNCTION(BlueprintImplementableEvent,Category="SyncedSkewWarp",DisplayName="Update Target Data")
	FWarpingNotifySyncData K2_UpdateTargetData(const FSimTickNotifyData& SimInput,const FWarpingNotifySyncData& CurrentTargetData);
private:
	bool bHasBlueprintInitializeTargetData = false;
	bool bHasBlueprintUpdateTargetData = false;
};

UCLASS()
class ABILITYSYSTEMSIMULATION_API UAnimNotifyState_SyncedSkewWarping : public UAnimNotifyState, public ISyncedNotifyInterface
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere,NoClear,BlueprintReadWrite,Instanced,Category="Settings")
	TObjectPtr<USyncedSkewWarpingProcessor> Processor = nullptr;
	
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Settings")
	FName StartOverrideMovementMode = NAME_None;

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Settings")
	FName EndOverrideMovementMode = NAME_None;
	
#pragma region NetNotifyInterface
	virtual void SimulationBegin_Implementation(const bool IsReSimulating, const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput) override;
	virtual void SimulationTick_Implementation(const FAbilitySystemTimeStep& InTimeStep, const FSimTickNotifyData& SimInput, FSimTickNotifyEndData& SimOutput) override;
	virtual void SimulationEnd_Implementation(const bool IsReSimulating, const FSimTickNotifyData& SimInput, UPARAM(ref) FSimTickNotifyEndData& SimOutput) override;
	virtual void RestoreFrame_Implementation(const FRestoreNotifyData& InputData,const FSyncedNotifyDataContainer& AuthorityState, const FSyncedNotifyDataContainer& ExpungedState, const UNetMontageSimulator* MontageSimulator) override;
	virtual UScriptStruct* GetRequiredType_Implementation() override;
	virtual bool CanTick_Implementation() const override { return true; }
#pragma endregion



};
