// Fill out your copyright notice in the Description page of Project Settings.


#include "Targeting/TargetingProcessor.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "Targeting/AbilityTargetingFilters.h"

UTargetingProcessor::UTargetingProcessor(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName OnStartedFuncName = FName(TEXT("K2_OnTargetingStarted"));
	UFunction* OnStartedFunction = GetClass()->FindFunctionByName(OnStartedFuncName);
	bHasOnBPTargetingStarted = IsImplementedInBlueprint(OnStartedFunction);

	static FName OnExecutedFuncName = FName(TEXT("K2_OnTargetingExecuted"));
	UFunction* OnExecutedFunction = GetClass()->FindFunctionByName(OnExecutedFuncName);
	bHasOnBPTargetingExecuted = IsImplementedInBlueprint(OnExecutedFunction);
	
	static FName OnConfirmedFuncName = FName(TEXT("K2_OnTargetingConfirmed"));
	UFunction* OnConfirmedFunction = GetClass()->FindFunctionByName(OnConfirmedFuncName);
	bHasOnBPTargetingConfirmed = IsImplementedInBlueprint(OnConfirmedFunction);

	static FName OnCanceledFuncName = FName(TEXT("K2_OnTargetingCanceled"));
	UFunction* OnCanceledFunction = GetClass()->FindFunctionByName(OnCanceledFuncName);
	bHasOnBPTargetingCanceled = IsImplementedInBlueprint(OnCanceledFunction);
	
	static FName OnShouldExecFuncName = FName(TEXT("K2_OnShouldExecute"));
	UFunction* OnShouldExecFunction = GetClass()->FindFunctionByName(OnShouldExecFuncName);
	bHasOnBPOnShouldExecute = IsImplementedInBlueprint(OnShouldExecFunction);

	static FName MaxDurReachedFuncName = FName(TEXT("K2_OnHasReachedMaxDuration"));
	UFunction* MaxDurReachedFunction = GetClass()->FindFunctionByName(MaxDurReachedFuncName);
	bHasOnBPOnMaxDurReached = IsImplementedInBlueprint(MaxDurReachedFunction);
}

bool UTargetingProcessor::IsHitFiltered(const FHitResult& Hit, const UNpAbilitySystemComponent* OwningASC
	,const TArray<UAbilityTargetingFilter*>& InFilters)
{
	for (UAbilityTargetingFilter* Filter : InFilters)
	{
		if (Filter->ValidHitResult(Hit, OwningASC) == false)
		{
			return true;
		}
	}
	return false;
}

bool UTargetingProcessor::IsActorFiltered(const AActor* Actor, const UNpAbilitySystemComponent* OwningASC
	,const TArray<UAbilityTargetingFilter*>& InFilters)
{
	for (UAbilityTargetingFilter* Filter : InFilters)
	{
		if (Filter->ValidHitActor(Actor, OwningASC) == false)
		{
			return true;
		}
	}
	return false;
}

void UTargetingProcessor::FilterHitResults(TArray<FHitResult>& HitResults, const UNpAbilitySystemComponent* OwningASC
	,const TArray<UAbilityTargetingFilter*>& InFilters) 
{
	// loop backwards and if it's not valid remove it.
	for (int32 i = HitResults.Num() - 1; i >= 0 ; --i)
	{
		const FHitResult& HitResult = HitResults[i];
		if (IsHitFiltered(HitResult,OwningASC,InFilters))
		{
			HitResults.RemoveAt(i);
		}
	}
}

void UTargetingProcessor::FilterActors(TArray<AActor*>& Actors, const UNpAbilitySystemComponent* OwningASC
	,const TArray<UAbilityTargetingFilter*>& InFilters) 
{
	// loop backwards and if it's not valid remove it.
	for (int32 i = Actors.Num() - 1; i >= 0 ; --i)
	{
		const AActor* HitActor = Actors[i];
		if (IsActorFiltered(HitActor,OwningASC,InFilters))
		{
			Actors.RemoveAt(i);
		}
	}
}


bool UTargetingProcessor::HasReachedMaxDuration(const float& CurrentDurationMS, const float& TimeSinceLastConfirmMS) const
{
	if (bHasOnBPOnMaxDurReached)
	{
		return K2_OnHasReachedMaxDuration(CurrentDurationMS,TimeSinceLastConfirmMS);
	}
	return OnHasReachedMaxDuration(CurrentDurationMS,TimeSinceLastConfirmMS);
}


bool UTargetingProcessor::ShouldExecute(const FAbilitySystemTimeStep& TimeStep, const float& CurrentDurationMS,
	const float& TimeSinceLastConfirmMS) const
{
	if (bHasOnBPOnShouldExecute)
	{
		return K2_OnShouldExecute(TimeStep,CurrentDurationMS,TimeSinceLastConfirmMS);
	}
	return OnShouldExecute(TimeStep,CurrentDurationMS,TimeSinceLastConfirmMS);
}

ETargetingResult UTargetingProcessor::StartTargeting(UNpAbilitySystemComponent* OwningAsc,
	FTargetingData& TargetingInputData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	TArray<AActor*> IgnoredActors = UpdateTargetingData(OwningAsc,TargetingInputData);
	ETargetingResult Result;
	if (bHasOnBPTargetingStarted)
	{
		Result = K2_OnTargetingStarted(OwningAsc,IgnoredActors, TargetingInputData, OutTargetDataHandle);
	}
	else
	{
		Result = OnTargetingStarted(OwningAsc,IgnoredActors, TargetingInputData, OutTargetDataHandle);
	}
	PostTargeting(OutTargetDataHandle,TargetingInputData);
	return Result;
	
}

ETargetingResult UTargetingProcessor::ExecuteTargeting(UNpAbilitySystemComponent* OwningAsc,
	const FAbilitySystemTimeStep& TimeStep, const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS,
	FTargetingData& TargetingInputData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	TArray<AActor*> IgnoredActors = UpdateTargetingData(OwningAsc,TargetingInputData);
	ETargetingResult Result;
	if (bHasOnBPTargetingExecuted)
	{
		Result = K2_OnTargetingExecuted(OwningAsc,TimeStep,IgnoredActors,CurrentDurationMS,InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	else
	{
		Result = OnTargetingExecuted(OwningAsc,TimeStep,IgnoredActors,CurrentDurationMS,InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	PostTargeting(OutTargetDataHandle,TargetingInputData);
	return Result;
}

ETargetingResult UTargetingProcessor::ConfirmTargeting(UNpAbilitySystemComponent* OwningAsc,
	const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingInputData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	TArray<AActor*> IgnoredActors = UpdateTargetingData(OwningAsc,TargetingInputData);
	ETargetingResult Result;
	if (bHasOnBPTargetingConfirmed)
	{
		Result = K2_OnTargetingConfirmed(OwningAsc,IgnoredActors, CurrentDurationMS, InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	else
	{
		Result = OnTargetingConfirmed(OwningAsc,IgnoredActors, CurrentDurationMS, InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	PostTargeting(OutTargetDataHandle,TargetingInputData);
	return Result;
}

ETargetingResult UTargetingProcessor::CancelTargeting(UNpAbilitySystemComponent* OwningAsc,
	const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingInputData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	TArray<AActor*> IgnoredActors = UpdateTargetingData(OwningAsc,TargetingInputData);
	ETargetingResult Result;
	if (bHasOnBPTargetingCanceled)
	{
		Result = K2_OnTargetingCanceled(OwningAsc,IgnoredActors, CurrentDurationMS, InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	else
	{
		Result = OnTargetingCanceled(OwningAsc,IgnoredActors, CurrentDurationMS, InTimeSinceLastConfirmMS,TargetingInputData,OutTargetDataHandle);
	}
	
	PostTargeting(OutTargetDataHandle,TargetingInputData);
	return Result;
}



#pragma region Native Overrides

bool UTargetingProcessor::OnHasReachedMaxDuration(const float& CurrentDurationMS,
	const float& TimeSinceLastConfirmMS) const
{
	if (MaxDuration > 0)
	{
		return CurrentDurationMS < FMath::RoundToInt(MaxDuration * 1000.f);
	}
	return false;
}

bool UTargetingProcessor::OnShouldExecute(const FAbilitySystemTimeStep& TimeStep, const float& CurrentDurationMS,
	const float& TimeSinceLastConfirmMS) const
{
	return HasReachedMaxDuration(CurrentDurationMS, TimeSinceLastConfirmMS) == false;
}

ETargetingResult UTargetingProcessor::OnTargetingStarted(UNpAbilitySystemComponent* OwningAsc
	,const TArray<AActor*>& IgnoredActors,FTargetingData& TargetingInputData
	,FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	return ETargetingResult::EEnd;
}

ETargetingResult UTargetingProcessor::OnTargetingExecuted(UNpAbilitySystemComponent* OwningAsc
	,const FAbilitySystemTimeStep& TimeStep,const TArray<AActor*>& IgnoredActors
	,const float& CurrentDurationMS, const float& InTimeSinceLastConfirmMS
	,FTargetingData& TargetingInputData, FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	return ETargetingResult::EEnd;
}

ETargetingResult UTargetingProcessor::OnTargetingConfirmed(UNpAbilitySystemComponent* OwningAsc
	,const TArray<AActor*>& IgnoredActors,const float& CurrentDurationMS
	,const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingInputData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	return ETargetingResult::EEnd;
}

ETargetingResult UTargetingProcessor::OnTargetingCanceled(UNpAbilitySystemComponent* OwningAsc
	,const TArray<AActor*>& IgnoredActors,const float& CurrentDurationMS
	,const float& InTimeSinceLastConfirmMS, FTargetingData& TargetingInputData,
	FGameplayAbilityTargetDataHandle& OutTargetDataHandle) const
{
	return ETargetingResult::EAbort;
}

TArray<AActor*> UTargetingProcessor::UpdateTargetingData(UNpAbilitySystemComponent* OwningAsc,FTargetingData& TargetingData) const
{
	AActor* Avatar = OwningAsc->GetAvatarActor();
	TArray<AActor*> IgnoredActors;

	
	switch (DirectionOverride)
	{
	case EDirectionOverrideType::ENone:
		{
			break;
		}
	case EDirectionOverrideType::EActorRotation:
		{
			if (Avatar)
			{
				TargetingData.Direction = Avatar->GetActorRotation().Vector();
			}
			break;
		}
	case EDirectionOverrideType::EControlRotation:
		{
			TargetingData.Direction = OwningAsc->GetSyncedControlRotation().Vector().GetSafeNormal();
			break;
		}
		
	}
	if (DirectionOverride != EDirectionOverrideType::ENone && FilterOutOverrideRotationPitch)
	{
		TargetingData.Direction.Z = 0.f;
		
	}
	if (Avatar)
	{
		if (OverrideLocationWithAvatarLocation)
		{
			TargetingData.Location = Avatar->GetActorLocation();
			if (!OffsetRelativeToAvatar.IsNearlyZero())
			{
				TargetingData.Location = Avatar->GetTransform().TransformPositionNoScale(OffsetRelativeToAvatar);
			}
		}
		
		if (AutoIgnoreAvatarOwner)
		{
			IgnoredActors.Add(Avatar);
		}
	}

	if (AutoIgnoreSavedActors && TargetingData.SavedHitActors.Num() > 0)
	{
		IgnoredActors.Append(TargetingData.SavedHitActors);
	}
	return IgnoredActors;
}

void UTargetingProcessor::PostTargeting(const FGameplayAbilityTargetDataHandle& TargetDataHandle,
	FTargetingData& TargetingInputData) const
{
	if (TargetDataHandle.Num() <= 0)
	{
		return;
	}

	if (!AutoSaveHitActors)
	{
		return;
	}
	TSet<AActor*> SavedActorsSet(TargetingInputData.SavedHitActors);
	for (int32 i = 0; i < TargetDataHandle.Num(); ++i)
	{
		const FGameplayAbilityTargetData* TargetData = TargetDataHandle.Get(i);
		if (!TargetData)
		{
			continue;
		}
		TArray<TWeakObjectPtr<AActor>> TargetActors = TargetData->GetActors();
		for (TWeakObjectPtr<AActor> Actor : TargetActors)
		{
			if (!Actor.IsValid())
			{
				continue;
			}
			if (!SavedActorsSet.Contains(Actor.Get()))
			{
				SavedActorsSet.Add(Actor.Get());
				TargetingInputData.SavedHitActors.Add(Actor.Get());
			}
		}
	}
}
#pragma endregion 



