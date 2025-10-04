// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DataTypes/TargetingTypes/TargetingDataTypes.h"
#include "UObject/Object.h"
#include "TargetingProcessor.generated.h"

class UAbilityTargetingFilter;
struct FGameplayAbilityTargetDataHandle;
struct FGameplayTag;
class UNpAbilitySystemComponent;
/**
 * This A Class responsible for doing the actual targeting, it simplifies blueprint implementation
 * of targeting.
 * it is used by Targeting tasks to manage its targeting while the task holds any runtime variables that need to be synced
 * can be used to just perform targeting instant and get a result
 * it is responsible for filtering the hits and providing return true if targeting result is valid.
 * 
 * All of the functions used are const to ensure that these calls do not mutate any state other than what's being provided
 * as inputs to the functions. these inputs are being synced by the task itself and processors can mutate them however they like
 *
 * Comes with 3 function StartTargeting, PerformTargeting and ConfirmTargeting, these are called by tasks
 * the idea behind these functions is that the processor can can override perform targeting ,
 * and do different targeting based on the TargetingState. StartTargeting and ConfirmTargeting can just set the targeting state tag
 * and then call perform targeting which does actual traces differently based on the state tag.
 * add to this the fact that confirmation can have different tags, this allows the processor to do multiple types of traces
 * based on different confirmation types.
 *
 * Possible extension : add synced "SetByCaller" tag and float values to allow user to arbitrarily send custom float values to the processor
 * and have them synced
 */
UCLASS(Blueprintable,Abstract,DefaultToInstanced,EditInlineNew)
class ABILITYSYSTEMSIMULATION_API UTargetingProcessor : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	
	/**
	 * Max Task Duration, if Negative it means infinite. 
	 */
	UPROPERTY(EditDefaultsOnly,Category="Duration")
	float MaxDuration = 0.f;

	/**
	 * When Task has a valid duration , if this is false we call cancel if true we call confirm.
	 * this is useful for targeting that can't be "canceled" once initiated. 
	 */
	UPROPERTY(EditDefaultsOnly,Category="Duration")
	bool ConfirmOnMaxDuration = false;
	/**
	 * if false, we will ignore any additional confirmation after the first one.
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="General")
	bool bAllowMultipleConfirmations = true;
	/**
	 * When True the synced control rotation from mover component will be used as direction for the trace
	 * if there's no mover component we will fall nack on player view.
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Direction Override")
	EDirectionOverrideType DirectionOverride = EDirectionOverrideType::ENone;

	/**
	 * if true ,and we override rotation we will zero out the pitch and keep the yaw.
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Direction Override",meta=(EditCondition="DirectionOverride != EDirectionOverrideType::ENone"))
	bool FilterOutOverrideRotationPitch = false;

	/**
	 * When True the location passed to targeting will be overriden by the actor location
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Location Override")
	bool OverrideLocationWithAvatarLocation = false;
	/**
	 * if OverrideLocationWithAvatarLocation is true we will apply this offset to actor location
	 * 
	 * this offset is in relative space and will be transformed by actor transform.
	 * Example : if this offset is X40,Y0,Z80  with actor having half height is 80 and radius is 40
	 * it will practically be his forehead, always. this is useful for deterministic targeting
	 * without having to send socket location in input.
	 * 
	 * giving power to client to decide where start location goes against server has full authority
	 * and should be avoided . if can "fake" it, by having a static relative offset to actor 
	 * and line it up to for example where weapon muzzle is during shooting. (spread is done with rotation usually)
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Location Override",meta=(EditCondition = "OverrideLocationWithAvatarLocation"))
	FVector OffsetRelativeToAvatar = FVector::ZeroVector;

	/**
	 * If set to true, the processor will automatically save the hit actors after every targeting event
	 * should be set to false if you want to manage the saved actors differently than just adding up hit actors
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="General")
	bool AutoSaveHitActors = false;
	/**
	* Add Avatar actor to ignore actors before every targeting event.
	*/
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ignored Actors")
	bool AutoIgnoreAvatarOwner = true;
	/**
	 * Add Saved Actors, which can be saved during targeting, to ignore actors before every targeting event.
	 * useful for sweeps overtime where if you hit someone once you won't then hit again like a weapon swing.
	 * should be set to false if you want to manage ignored actors and saved actors manually
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Ignored Actors")
	bool AutoIgnoreSavedActors = false;
	
	/**
	 * Filter classes, can be created in blueprint and override IsValidHit() or ValidHitActor() to determine if a specific hit result or actor is acceptable.
	 * Provides the owning ASC as input to check for any needed data.
	 */
	UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="Targeting|Filter",Instanced)
	TArray<UAbilityTargetingFilter*> DefaultFilters;

	UFUNCTION(blueprintCallable,Category=Targeting)
	static bool IsHitFiltered(const FHitResult& Hit,const UNpAbilitySystemComponent* OwningASC,const TArray<UAbilityTargetingFilter*>& InFilters);
	UFUNCTION(blueprintCallable,Category=Targeting)
	static bool IsActorFiltered(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC,const TArray<UAbilityTargetingFilter*>& InFilters);
	UFUNCTION(blueprintCallable,Category=Targeting)
	static void FilterHitResults(TArray<FHitResult>& HitResults, const UNpAbilitySystemComponent* OwningASC,const TArray<UAbilityTargetingFilter*>& InFilters);
	UFUNCTION(blueprintCallable,Category=Targeting)
	static void FilterActors(TArray<AActor*>& Actors, const UNpAbilitySystemComponent* OwningASC,const TArray<UAbilityTargetingFilter*>& InFilters);
	
	bool ShouldExecute(const FAbilitySystemTimeStep& TimeStep,const float& CurrentDurationMS,const float& TimeSinceLastConfirmMS) const;
	
	bool HasReachedMaxDuration(const float& CurrentDurationMS, const float& TimeSinceLastConfirmMS) const;
	
	ETargetingResult StartTargeting(UNpAbilitySystemComponent* OwningAsc
			,UPARAM(ref) FTargetingData& TargetingInputData
			,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * This will be called when targeting is triggered after starting, this can be on tick
	 * or on pulse x times every y duration
	 */
	 ETargetingResult ExecuteTargeting(UNpAbilitySystemComponent* OwningAsc
		,const FAbilitySystemTimeStep& TimeStep
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * Called when targeting is confirmed , usually by a Targeting prediction Task.
	 * DurationSinceLastConfirmation : will provide duration Since previous confirmation , not this one.
	 * if there's no previous activation it will be -1.f.
	 */
	 ETargetingResult ConfirmTargeting(UNpAbilitySystemComponent* OwningAsc
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * Called when targeting is deemed to be canceled by the task. either from cancel event 
	 * or reached max duration.
	 */
	 ETargetingResult CancelTargeting(UNpAbilitySystemComponent* OwningAsc
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;
	
	
protected:

	
	// Targeting Events To Override, with native and blueprint versions

	
	virtual bool OnHasReachedMaxDuration(const float& CurrentDurationMS, const float& TimeSinceLastConfirmMS) const;

	/**
	 * by default, we don't execute EI we don't tick if Max Duration is > 0 , and it has been exceeded.
	 * some targeting would want to execute very X times for a duration. they can do that math check here.
	 */
	virtual bool OnShouldExecute(const FAbilitySystemTimeStep& TimeStep,const float& CurrentDurationMS
		,const float& TimeSinceLastConfirmMS) const;
	
	
	virtual ETargetingResult OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * This will be called when targeting is triggered after starting, this can be on tick
	 * or on pulse x times every y duration , by default this will return End if max duration > 0
	 * and it has been exceeded
	 */
	virtual ETargetingResult OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc
		,const FAbilitySystemTimeStep& TimeStep
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * Called when targeting is confirmed , usually by a Targeting prediction Task.
	 * @param : DurationSinceLastConfirmation : will provide duration Since previous confirmation , not this one.
	 * if there's no previous activation it will be -1.f.
	 */
	virtual ETargetingResult OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;


	/**
	 * Called When Targeting was canceled, this is done this way to allow for
	 * some special case when canceling doesn't mean abort but means doing different
	 * trace or smaller radius/distance etc..
	 */
	virtual ETargetingResult OnTargetingCanceled(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;
	
	// Blueprint Overrides

	UFUNCTION(BlueprintImplementableEvent,Category=Targeting)
	bool K2_OnHasReachedMaxDuration(const float& CurrentDurationMS, const float& TimeSinceLastConfirmMS) const;
	
	/**
	 * by default we don't execute EI we don't tick if Max Duration is > 0 and current duration is past it
	 * some targeting would want to execute very X times for a duration. they can do that math check here.
	 */
	UFUNCTION(BlueprintImplementableEvent,Category=Targeting)
	bool K2_OnShouldExecute(const FAbilitySystemTimeStep& TimeStep,const float& CurrentDurationMS
		,const float& TimeSinceLastConfirmMS) const;


	/**
	 * Called When Targeting just started, this can perform the targeting
	 * and provide result that ends it instantly if desired
	 */
	UFUNCTION(BlueprintImplementableEvent,BlueprintCallable,category=Targeting,DisplayName="OnTargetingStarted")
	ETargetingResult K2_OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * This will be called when targeting is triggered after starting, this can be on tick
	 * or on pulse x times every y duration , by default this will return End if max duration > 0
	 * and it has been exceeded
	 */
	UFUNCTION(BlueprintImplementableEvent,BlueprintCallable,category=Targeting,DisplayName="OnTargetingExecuted")
	ETargetingResult K2_OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc
		,const FAbilitySystemTimeStep& TimeStep
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * Called when targeting is confirmed , usually by a Targeting prediction Task.
	 * @param : DurationSinceLastConfirmation : will provide duration Since previous confirmation , not this one.
	 * if there's no previous activation it will be -1.f.
	 */
	UFUNCTION(BlueprintImplementableEvent,BlueprintCallable,category=Targeting,DisplayName="OnTargetingConfirmed")
	ETargetingResult K2_OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

	/**
	 * Called When Targeting was canceled, this is done this way to allow for
	 * some special case when canceling doesn't mean abort but means doing different
	 * trace or smaller radius/distance etc..
	 */
	
	UFUNCTION(BlueprintImplementableEvent,BlueprintCallable,category=Targeting,DisplayName="OnTargetingCanceled")
	ETargetingResult K2_OnTargetingCanceled(UNpAbilitySystemComponent* OwningAsc
		,const TArray<AActor*>& IgnoredActors
		,const float& CurrentDurationMS
		,const float& InTimeSinceLastConfirmMS
		,UPARAM(ref) FTargetingData& TargetingInputData
		,UPARAM(ref) FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const;

private:

	TArray<AActor*> UpdateTargetingData(UNpAbilitySystemComponent* OwningAsc,FTargetingData& TargetingData) const;

	void PostTargeting(const FGameplayAbilityTargetDataHandle& TargetDataHandle,FTargetingData& TargetingInputData) const;
	

	bool bHasOnBPTargetingStarted = false;
	bool bHasOnBPTargetingExecuted = false;
	bool bHasOnBPTargetingConfirmed = false;
	bool bHasOnBPTargetingCanceled = false;
	bool bHasOnBPOnShouldExecute = false;
	bool bHasOnBPOnMaxDurReached = false;
};