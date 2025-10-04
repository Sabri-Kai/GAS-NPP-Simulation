// 2025 Yohoho Productions /  Sirkai


#include "EffectsComponents/ApplyMoveGameplayEffectComponent.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "DataTypes/GameplayEffectLayaredMove.h"

#pragma region Layered Move Effect Creator

UBaseMoveEffectCreator::UBaseMoveEffectCreator(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FEffectMoveContainer UBaseMoveEffectCreator::MakeMove_Implementation(UMoverComponent* TargetMoverComponent ,UAbilitySystemComponent* TargetAbilitySystemComponent
		,const int32& EffectHandle, const FGameplayEffectSpecHandle& EffectSpec) const
{
	return FEffectMoveContainer();
}

FEffectMoveContainer UBaseMoveEffectCreator::K2_FillMoveContainer(const int32& LayeredMove)
{
	checkNoEntry();
	return FEffectMoveContainer();
}

// static
DEFINE_FUNCTION(UBaseMoveEffectCreator::execK2_FillMoveContainer)
{
	Stack.StepCompiledIn<FStructProperty>(nullptr);
	void* MovePtr = Stack.MostRecentPropertyAddress;
	FStructProperty* StructProp = CastField<FStructProperty>(Stack.MostRecentProperty);

	P_FINISH;

	P_NATIVE_BEGIN;
	bool ValidTypes = false;
	const bool bHasValidStructProp = StructProp && StructProp->Struct;
	if (bHasValidStructProp)
	{
		ValidTypes = StructProp->Struct->IsChildOf(FLayeredMoveBase::StaticStruct())
					|| StructProp->Struct->IsChildOf(FInstantMovementEffect::StaticStruct());
	}
	
	if (ensureMsgf((MovePtr && bHasValidStructProp && ValidTypes),
		TEXT("An invalid type (%s) was sent to a MakeMoveContainer node. A struct derived from FLayeredMoveBase is required. No Move will be created."),
		StructProp ? *GetNameSafe(StructProp->Struct) : *Stack.MostRecentProperty->GetClass()->GetName()))
	{
		if (StructProp->Struct->IsChildOf(FLayeredMoveBase::StaticStruct()))
		{
			// Could we steal this instead of cloning? (move semantics)
			FLayeredMoveBase* MoveAsBasePtr = reinterpret_cast<FLayeredMoveBase*>(MovePtr);
			FLayeredMoveBase* ClonedMove = MoveAsBasePtr->Clone();
			FEffectMoveContainer* OutputContainer = static_cast<FEffectMoveContainer*>(RESULT_PARAM);
			OutputContainer->LayeredMove = TSharedPtr<FLayeredMoveBase>(ClonedMove);
			OutputContainer->InstantMove = nullptr;
		}
		else if (StructProp->Struct->IsChildOf(FInstantMovementEffect::StaticStruct()))
		{
			// Could we steal this instead of cloning? (move semantics)
			FInstantMovementEffect* MoveAsBasePtr = reinterpret_cast<FInstantMovementEffect*>(MovePtr);
			FInstantMovementEffect* ClonedMove = MoveAsBasePtr->Clone();
			FEffectMoveContainer* OutputContainer = static_cast<FEffectMoveContainer*>(RESULT_PARAM);
			OutputContainer->InstantMove = TSharedPtr<FInstantMovementEffect>(ClonedMove);
			OutputContainer->LayeredMove = nullptr;
		}
	}
	P_NATIVE_END;
}

float UBaseMoveEffectCreator::GetDurationFromEffectSpec(const FGameplayEffectSpecHandle& EffectSpec)
{
	if (EffectSpec.IsValid())
	{
		return EffectSpec.Data->GetDuration();
	}
	return 0.f;
}

bool UBaseMoveEffectCreator::IsEffectInstant(const FGameplayEffectSpecHandle& EffectSpec)
{
	if (EffectSpec.IsValid())
	{
		return EffectSpec.Data->GetDuration() == FGameplayEffectConstants::INSTANT_APPLICATION;
	}
	return true;
}

float UBaseMoveEffectCreator::GetSetByCallerMagnitude(const FGameplayEffectSpecHandle& EffectSpec, FGameplayTag SetByCallerTag)
{
	if (EffectSpec.IsValid())
	{
		return EffectSpec.Data->GetSetByCallerMagnitude(SetByCallerTag);
	}
	return 0.f;
}

void UBaseMoveEffectCreator::ApplyMove(UMoverComponent* MoverComponent ,UAbilitySystemComponent* OwningComponent,const int32& EffectHandle, const FGameplayEffectSpec& EffectSpec,
	const FEffectMoveContainer& MoveToApply) const
{
	if (MoveToApply.LayeredMove.IsValid())
	{
		// if we are of type FEffectLayeredMove, set the bools and effect handle
		if (MoveToApply.LayeredMove->GetScriptStruct()->IsChildOf(FAbilityEffectLayeredMove::StaticStruct()))
		{
			FAbilityEffectLayeredMove* EffectMove = static_cast<FAbilityEffectLayeredMove*>(MoveToApply.LayeredMove.Get());
			check(EffectMove);
			EffectMove->ActiveEffectHandle = EffectHandle;
			EffectMove->bStopIfNoFoundEffect = LayeredMoveDefaultParams.bStopIfNoFoundEffect;
			EffectMove->bPauseMoveIfInhibitedEffect = LayeredMoveDefaultParams.bPauseMoveIfInhibitedEffect;
		}
			
		if (LayeredMoveDefaultParams.CopyDurationFromEffect)
		{
			MoveToApply.LayeredMove->DurationMs = FMath::Floor(EffectSpec.GetDuration() * 1000.f);
		}
		MoverComponent->QueueLayeredMove(MoveToApply.LayeredMove);
	}
	else if (MoveToApply.InstantMove.IsValid())
	{
		MoverComponent->QueueInstantMovementEffect(MoveToApply.InstantMove);
	}
}

#pragma endregion

#pragma region Base Apply Move Effect Component

bool UApplyMoveGameplayEffectComponent::OnActiveGameplayEffectAdded(
	FActiveGameplayEffectsContainer& ActiveGEContainer, FActiveGameplayEffect& ActiveGE) const
{
	
	// return early if there's no move to be created and applied anyways
	if (!MoveProcessor)
	{
		return true;
	}
	// only run if effect is not instant
	if (ActiveGE.Spec.GetDuration() == FGameplayEffectConstants::INSTANT_APPLICATION)
	{
		return true;
	}
	UNpAbilitySystemComponent* ASC = Cast<UNpAbilitySystemComponent>(ActiveGEContainer.Owner);
	// we don't want to re-apply a layered move when we are adding an effect forcefully (from correction).
	// the move itself has its own correction and will be applied on its own
	if (ASC->GetIsRestoringFrame() || !ASC->GetAvatarActor())
	{
		return true;
	}

	UMoverComponent* MoverComponent = ASC->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	if (MoverComponent)
	{
		ApplyMove(MoverComponent,ActiveGEContainer.Owner,ActiveGE.Handle.GetHandle(), ActiveGE.Spec);
	}
	return true;
}

void UApplyMoveGameplayEffectComponent::OnGameplayEffectApplied(FActiveGameplayEffectsContainer& ActiveGEContainer,
	FGameplayEffectSpec& GESpec, FPredictionKey& PredictionKey) const
{
	// return early if there's no move to be created and applied anyways
	if (!MoveProcessor)
	{
		return;
	}
	// only run if effect is instant
	if (GESpec.GetDuration() != FGameplayEffectConstants::INSTANT_APPLICATION)
	{
		return;
	}
	UNpAbilitySystemComponent* ASC = Cast<UNpAbilitySystemComponent>(ActiveGEContainer.Owner);
	// we don't want to re-apply a layered move when we are adding an effect forcefully (from correction).
	// the move itself has its own correction and will be applied on its own
	if (ASC->GetIsRestoringFrame() || !ASC->GetAvatarActor())
	{
		return;
	}

	UMoverComponent* MoverComponent = ASC->GetAvatarActor()->GetComponentByClass<UMoverComponent>();
	if (MoverComponent)
	{
		ApplyMove(MoverComponent,ActiveGEContainer.Owner,INDEX_NONE, GESpec);
	}
}

void UApplyMoveGameplayEffectComponent::ApplyMove(UMoverComponent* MoverComponent ,UAbilitySystemComponent* OwningComponent,const int32& EffectHandle, const FGameplayEffectSpec& EffectSpec) const
{
	if (MoveProcessor && OwningComponent && MoverComponent)
	{
		FGameplayEffectSpecHandle SpecHandle(new FGameplayEffectSpec(EffectSpec));
		FEffectMoveContainer MoveContainer = MoveProcessor->MakeMove(MoverComponent,OwningComponent,
													EffectHandle,SpecHandle);
		MoveProcessor->ApplyMove(MoverComponent,OwningComponent,EffectHandle,EffectSpec,MoveContainer);
	}
}

#pragma endregion

//------------------- Examples --------------------//

#pragma region Move To Layered Move Creator

UMoveToLayeredMoveCreator::UMoveToLayeredMoveCreator()
{
}

FEffectMoveContainer UMoveToLayeredMoveCreator::MakeMove_Implementation(UMoverComponent* MoverComponent,
	UAbilitySystemComponent* OwningComponent, const int32& EffectHandle,
	const FGameplayEffectSpecHandle& NewEffectSpec) const
{
	if (!NewEffectSpec.Data.IsValid())
	{
		return FEffectMoveContainer();
	}
	
	FEffectLayeredMove_MoveTo MoveTo_Move;
	MoveTo_Move.ActiveEffectHandle = EffectHandle;
	MoveTo_Move.TargetLocation = NewEffectSpec.Data->GetEffectContext().GetOrigin();
	
	MoveTo_Move.DurationMs = FMath::Floor(LayeredMoveDefaultParams.Duration * 1000.f);
	MoveTo_Move.PathOffsetCurve = PathOffsetCurve;
	MoveTo_Move.TimeMappingCurve = TimeMappingCurve;
	MoveTo_Move.bRestrictSpeedToExpected = bRestrictSpeedToExpected;
	MoveTo_Move.Priority = LayeredMoveDefaultParams.Priority;
	MoveTo_Move.bStopIfNoFoundEffect = LayeredMoveDefaultParams.bStopIfNoFoundEffect;
	MoveTo_Move.bPauseMoveIfInhibitedEffect = LayeredMoveDefaultParams.bPauseMoveIfInhibitedEffect;
	MoveTo_Move.MixMode = EMoveMixMode::OverrideAll;
	return FEffectMoveContainer(&MoveTo_Move);
}
#pragma endregion
