// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "LayeredMove.h"
#include "GameplayEffectLayaredMove.generated.h"

class UCurveVector;
class UNpAbilitySystemComponent;
/**
 *  This Layared Move is a special move that required Np Ability System Component
 *  It is applied using a Gameplay Effect Component and will only do something if effect allows it
 *  By default the Move will end if effects that applied it end and bStopIfNoFoundEffect is true, Look at ShouldForceStopMove()
 *  By Default The Move will be ignored if the effect is inhibited and bIgnoreMoveIfInhibitedEffect is true, Look at ShouldIgnoreMove().
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FAbilityEffectLayeredMove : public FLayeredMoveBase
{
	GENERATED_USTRUCT_BODY()

	FAbilityEffectLayeredMove();
	virtual ~FAbilityEffectLayeredMove() {}

	// This Is the Class Default Object of the gameplay effect that created this move
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameplayEffect)
	int32 ActiveEffectHandle = INDEX_NONE;

	// if the active handle is not valid , we will not stop when effect ends
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameplayEffect)
	bool bStopIfNoFoundEffect = true;
	// if the active handle is not valid , we will not ignore more when effect is inhibited
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = GameplayEffect)
	bool bPauseMoveIfInhibitedEffect = true;
	
	virtual bool GenerateMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
		, const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override final;
	

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	/**
	 * A replacement for GenerateMove only differences :
	 * Will not be called if ShouldStopFromEffect is true (effect ended) , instead OnGenerateEffectEndMove will be.
	 * Will not be called if ShouldIgnoreMove is true (effect inhibited). we just ignore move
	 * Will not be called on the last active frame in its duration, instead OnGenerateEffectEndMove will be.
	 */
	virtual bool GenerateEffectMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
		, const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove);
	/**
	 * Checks if we should stop the layered move based on current effect state
	 * by default if ActiveEffectHandle is valid and bStopIfNoFoundEffect is true, we stop if effect is no longer active.
	 */
	virtual bool ShouldStopMove( UNpAbilitySystemComponent* AbilitySystem) const;
	/**
	 * Checks if we should ignore the layered move based on current effect state
	 * by default if ActiveEffectHandle is valid and bPauseMoveIfInhibitedEffect is true, we pause if effect is active but inhibited.
	 */
	virtual bool ShouldIgnoreMove( UNpAbilitySystemComponent* AbilitySystem) const;
	/**
	 * if ShouldStopFromEffect is true (effect ended) , we call this to generate and "end" move.
	 * this can be called if the effect was prematurely removed or the effect duration reaching an end this frame
	 * Move will end after this.
	 * 
	 * NOTE : OnEnd Of FLayeredMoveBase will be called the next frame after OnGenerateEffectEndMove if duration ends.
	 * OnGenerateEffectEndMove signals it's last frame we will be active.
	 * 
	 * @return : True or false if the move will do something and should not be ignored . 
	 */
	virtual bool OnGenerateEffectEndMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep
		, const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove);
};



USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FEffectLayeredMove_MoveTo : public FAbilityEffectLayeredMove
{
	GENERATED_USTRUCT_BODY()

	FEffectLayeredMove_MoveTo();
	virtual ~FEffectLayeredMove_MoveTo() {}

	// Location to Start the MoveTo move from
	
	
	// Location to move towards
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	FVector TargetLocation;

	// if true, will restrict speed to where the actor is expected to be (in regard to start, end and duration)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	bool bRestrictSpeedToExpected;
	
	// Optional CurveVector used to offset the actor from the path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveVector> PathOffsetCurve;

	// Optional CurveFloat to apply to how fast the actor moves as they get closer to the target location
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mover)
	TObjectPtr<UCurveFloat> TimeMappingCurve;

	// Start Location Set when move starts
	UPROPERTY()
	FVector StartLocation;

	virtual void OnStart(const UMoverComponent* MoverComp, UMoverBlackboard* SimBlackboard) override;
	// Generate a movement
	virtual bool GenerateEffectMove(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep,
		const UMoverComponent* MoverComp,const UNpAbilitySystemComponent* AbilitySystem, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;

	virtual FLayeredMoveBase* Clone() const override;

	virtual void NetSerialize(FArchive& Ar) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;

	virtual void AddReferencedObjects(class FReferenceCollector& Collector) override;
	
protected:
	// helper function to apply movement vector offset from the PathOffsetCurve
	FVector GetPathOffsetInWorldSpace(const float MoveFraction) const;

	// Helper function to apply TimeMappingCurve to the layered move
	float EvaluateFloatCurveAtFraction(const UCurveFloat& Curve, const float Fraction) const;
};