// 2025 Yohoho Productions /  Sirkai


#include "Tasks/GASTasks/PlayMontageAndWaitEventPredictionTask.h"
#include "Abilities/NpAbilitySystemComponent.h"


#define LOCTEXT_NAMESPACE "Play Montage Wait Event"


UPlayMontageAndWaitEventPredictionTask::UPlayMontageAndWaitEventPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	// we don't override anything.
}

void UPlayMontageAndWaitEventPredictionTask::GameplayEventContainerCallback(FGameplayTag MatchingTag,
	const FGameplayEventData* Payload)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (!TagsToListenFor.HasTagExact(MatchingTag))
	{
		return;
	}
	FGameplayEventData PayloadData = Payload ? *Payload : FGameplayEventData();
	OnEvent.Broadcast(MatchingTag,PayloadData);
}

void UPlayMontageAndWaitEventPredictionTask::BindDelegates()
{
	Super::BindDelegates();
	if (!OnGameplayEventHandle.IsValid())
	{
		OnGameplayEventHandle = GetAbilitySystemComponent()->AddGameplayEventTagContainerDelegate(TagsToListenFor, FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(this, &UPlayMontageAndWaitEventPredictionTask::GameplayEventContainerCallback));
	}
}

void UPlayMontageAndWaitEventPredictionTask::UnBindDelegates()
{
	Super::UnBindDelegates();
	if (OnGameplayEventHandle.IsValid())
	{
		GetAbilitySystemComponent()->RemoveGameplayEventTagContainerDelegate(TagsToListenFor, OnGameplayEventHandle);
		OnGameplayEventHandle.Reset();
	}
}

FText UPlayMontageAndWaitEventPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("K2Node_PlayMontageAndWaitEventPredictionTask", "Play Montage And Wait Event Prediction Task");
}

#undef LOCTEXT_NAMESPACE
