// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PlayMontageAndWaitPredictionTask.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "PlayMontageAndWaitEventPredictionTask.generated.h"

/**
 * 
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayMontageAndWaitGameplayEventDelegate, FGameplayTag, EventTag, FGameplayEventData, Payload);
UCLASS()
class ABILITYSYSTEMSIMULATION_API UPlayMontageAndWaitEventPredictionTask : public UPlayMontageAndWaitPredictionTask
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EventTags")
	FGameplayTagContainer TagsToListenFor;

	UPROPERTY(BlueprintAssignable)
	FPlayMontageAndWaitGameplayEventDelegate OnEvent;
	
	virtual FText GetNodeTitle() const override;

	virtual void GameplayEventContainerCallback(FGameplayTag MatchingTag, const FGameplayEventData* Payload);
protected:
	virtual void BindDelegates() override;
	virtual void UnBindDelegates() override;
private:
	FDelegateHandle OnGameplayEventHandle;

};
