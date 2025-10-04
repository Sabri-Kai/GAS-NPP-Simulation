// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "InstantMovementEffect.h"
#include "MoverComponent.h"
#include "Curves/CurveVector.h"

#include "ApplyMoveGameplayEffectComponent.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpecHandle;
/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FEffectMoveContainer
{
	GENERATED_BODY()
	FEffectMoveContainer(){}

	FEffectMoveContainer(const FEffectMoveContainer& Other)
	{
		InstantMove = nullptr;
		LayeredMove = nullptr;
		if (Other.LayeredMove.IsValid())
		{
			LayeredMove = TSharedPtr<FLayeredMoveBase>(Other.LayeredMove.Get()->Clone());
		}
		else if (Other.InstantMove.IsValid())
		{
			InstantMove = TSharedPtr<FInstantMovementEffect>(Other.InstantMove.Get()->Clone());
		}
	}

	FEffectMoveContainer(const FLayeredMoveBase* InLayeredMove)
	{
		LayeredMove = TSharedPtr<FLayeredMoveBase>(InLayeredMove->Clone());
		InstantMove = nullptr;
	}

	FEffectMoveContainer(const FInstantMovementEffect* InMovementEffect)
	{
		InstantMove = TSharedPtr<FInstantMovementEffect>(InMovementEffect->Clone());
		LayeredMove = nullptr;
	}

	virtual ~FEffectMoveContainer(){}

	TSharedPtr<FLayeredMoveBase> LayeredMove = nullptr;
	TSharedPtr<FInstantMovementEffect> InstantMove = nullptr;
};
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FLayeredMoveDefaultParams
{
	GENERATED_BODY()
	FLayeredMoveDefaultParams(){}

	// if this is set to false , it is up to you to ensure the DurationMS is filled inside MakeMove().
	// Either using the variable above * 1000 (Duration in seconds and layered move is in milliseconds)
	// or calculating it inside MakeMove().
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Category=LayeredMove)
	bool CopyDurationFromEffect = true;
	
	// Duration Amount can be used in MakeMove and will not be overriden by effect duration if CopyDurationFromEffect if true.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LayeredMove,meta=(EditCondition="CopyDurationFromEffect == false"))
	float Duration = 0.f;

	// Determines how this object's movement contribution should be mixed with others
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LayeredMove)
	EMoveMixMode MixMode = EMoveMixMode::OverrideAll;
	
	// Determines if this layered move should take priority over other layered moves when different moves have conflicting overrides - higher numbers taking precedent.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = LayeredMove)
	uint8 Priority = 0;

	// Settings related to velocity applied to the actor after a layered move has finished
	
	// What mode we want to happen when a Layered Move ends, see @ELayeredMoveFinishVelocityMode
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LayeredMove)
	ELayeredMoveFinishVelocityMode FinishVelocityMode = ELayeredMoveFinishVelocityMode::MaintainLastRootMotionVelocity;
	
	// Velocity that the actor will use if Mode == SetVelocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LayeredMove)
	FVector SetVelocity = FVector::ZeroVector;

	// Actor's Velocity will be clamped to this value if Mode == ClampVelocity
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = LayeredMove)
	float ClampVelocity = 0.f;
	
	// If layered move used is child of FEffectLayeredMove, will decide if the move stops when effect ends
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = LayeredMove)
	bool bStopIfNoFoundEffect = true;
	
	// If layered move used is child of FEffectLayeredMove, will decide if the move pauses when effect is inhibited
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = LayeredMove)
	bool bPauseMoveIfInhibitedEffect = true;
};
/*
 * This class exists because UGameplayEffectComponent can't be created in blueprint
 * to facilitate blueprint movement effects Through its children classes ,
 * It applies layered move or instant movement effect based on what is passed in the container
 * 
 * you can subclass from these classes in blueprint or c++ override MakeMove() and pass new move to the output container.
 * and use your subclass in UApplyMoveGameplayEffectComponent class.,
 *
 * Helps with extending this to apply something else like "MovementModifiers" or new layered moves (5.7+)
 * by adding additional needed data to FEffectMoveContainer and override base class to do something else in apply move.
 *
* Integrates FEffectLayeredMove which is a layered move that can stop/pause
 * when the effect that applied it ends/becomes inhibited , automatically setting its needed effect handle and booleans.
 *
 * if "CopyDurationFromEffect" is true and creating a layered move, will override the layered move duration with the duration of the effect
 * 
 * NOTE : Class is not a blueprint type because on its own it doesn't make any layered move
 * a subclass needs to override the function MakeMove()
 */
UCLASS(Abstract,Blueprintable,EditInlineNew,DefaultToInstanced)
class ABILITYSYSTEMSIMULATION_API UBaseMoveEffectCreator : public UObject
{
	GENERATED_UCLASS_BODY()
	

	/*
	 * these are setting useful to have by default for layered move.
	 * can get these values inside MakeMove() to use them to fill your Layered Move/Instant Movement if needed.
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category=LayeredMove)
	FLayeredMoveDefaultParams LayeredMoveDefaultParams;
	
	UFUNCTION(BlueprintNativeEvent, Category=MoveProcessor)
	FEffectMoveContainer MakeMove(UMoverComponent* TargetMoverComponent ,UAbilitySystemComponent* TargetAbilitySystemComponent
		,const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const;
	
	UFUNCTION(BlueprintCallable, CustomThunk, Category = MoveProcessor, meta = (CustomStructureParam = "Move", AllowAbstract = "false", DisplayName = "Fill Move Container"))
	static FEffectMoveContainer K2_FillMoveContainer(UPARAM(DisplayName="Move") const int32& Move);

	UFUNCTION(BlueprintPure, Category="Utility")
	static float GetDurationFromEffectSpec(const FGameplayEffectSpecHandle& EffectSpec);
	UFUNCTION(BlueprintPure, Category="Utility")
	static bool IsEffectInstant(const FGameplayEffectSpecHandle& EffectSpec);

	UFUNCTION(BlueprintPure, Category="Utility")
	static float GetSetByCallerMagnitude(const FGameplayEffectSpecHandle& EffectSpec, FGameplayTag SetByCallerTag);

	UFUNCTION()
	virtual void ApplyMove(UMoverComponent* MoverComponent ,UAbilitySystemComponent* OwningComponent,const int32& EffectHandle, const FGameplayEffectSpec& EffectSpec, const FEffectMoveContainer& MoveToApply) const;

private:
	DECLARE_FUNCTION(execK2_FillMoveContainer);
	
};

/*
 * the GameplayEffectComponent you add to your Gameplay Effect to apply mover "Move", hold a creator which would create the move.
 *
 * If The Effect has A Duration It Is Applied OnActiveGameplayEffectAdded and passes a valid handle if layered move wants to use it.
 * 
 * If This Component is used on Instant Effect , since there's no effect to follow removal , inhibition etc.. it will provide
 * INDEX_NONE as active effect handle to ApplyMove which calls the MoveProcessor ApplyMove().
 *
 * This is a simple implementation and can extend it by adding ability to apply the move on every effect execution,
 * having tag query to apply the move , ability to use period as move duration, etc...
 * 
 * for processor info look above @UBaseMoveEffectProcessor
 */
UCLASS(CollapseCategories,DisplayName= "Base Movement Effect Component")
class ABILITYSYSTEMSIMULATION_API UApplyMoveGameplayEffectComponent : public UGameplayEffectComponent
{
	GENERATED_BODY()


public:
	
	UPROPERTY(EditAnywhere,BlueprintReadOnly,Instanced,Category="Settings")
	TObjectPtr<UBaseMoveEffectCreator> MoveProcessor = nullptr;
	
	virtual bool OnActiveGameplayEffectAdded(FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const override;
	virtual void OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer, FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const override;
	
	UFUNCTION()
	void ApplyMove(UMoverComponent* MoverComponent ,UAbilitySystemComponent* OwningComponent,const int32& EffectHandle, const FGameplayEffectSpec& EffectSpec) const;
};

/*
 * Example Class How To Use The Effect Layered move (FEffectLayeredMove , More details in GameplayEffectMove.h)
 * 
 * Main Function of this class is to showcase how to create your own Layered Move Creator in c++,
 * add properties to it and directly apply a specific layered move in this case a child of FEffectLayeredMove.
 * 
 * You can always create an enum of specific layered move types and all their settings as properties 
 * in a single ULayeredMoveEffectCreator and apply a specific move based on the enum.
 * 
 * as long as you know where to get the runtime data of each specific move from effect context
 * or calculate it yourself using ability system component pointer in FActiveGameplayEffectsContainer
 * there should be no problem. 
 * 
 * This uses the Effect Context Origin vector property to define the move to location,
 * so when effect is applied the context origin is expected to already be set to desired Move to location,
 * this is how we pass data from gameplay to layered move.
 */
UCLASS(DisplayName= "Move To Layered Move Efect Creator ",BlueprintType)
class ABILITYSYSTEMSIMULATION_API UMoveToLayeredMoveCreator : public UBaseMoveEffectCreator
{
	GENERATED_BODY()

public:
	UMoveToLayeredMoveCreator();
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=MoveTo_Move)
	TObjectPtr<UCurveFloat> TimeMappingCurve = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=MoveTo_Move)
	TObjectPtr<UCurveVector> PathOffsetCurve = nullptr;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=MoveTo_Move)
	bool bRestrictSpeedToExpected = false;
	
	virtual FEffectMoveContainer MakeMove_Implementation(UMoverComponent* MoverComponent ,UAbilitySystemComponent* OwningComponent
		,const int32& EffectHandle, const FGameplayEffectSpecHandle& NewEffectSpec) const override ;
};
