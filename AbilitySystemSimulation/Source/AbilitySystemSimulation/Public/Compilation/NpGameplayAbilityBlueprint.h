// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "Tasks/BasePredictionTask.h"
#include "NpGameplayAbilityBlueprint.generated.h"

struct FSavedPredictionTask;
/**
 * 
 */
UCLASS()
class ABILITYSYSTEMSIMULATION_API UNpGameplayAbilityBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	
#if WITH_EDITOR

	virtual UClass* GetBlueprintClass() const override;
	
	// UBlueprint
	virtual bool SupportedByDefaultBlueprintFactory() const override { return false; }
	// --
#endif
};
