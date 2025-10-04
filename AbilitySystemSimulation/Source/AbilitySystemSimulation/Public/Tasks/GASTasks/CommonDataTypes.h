// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "DataTypes/BaseTaskData.h"
#include "UObject/Object.h"
#include "CommonDataTypes.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FExternalTargetTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FExternalTargetTaskData() override {}

	UPROPERTY()
	TObjectPtr<AActor> ExternalTarget = nullptr;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};


// Fixed Wait Task Data 
USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FWaitFixedDurationTaskData : public FAbilityTaskDataBase
{
	GENERATED_USTRUCT_BODY()

	virtual ~FWaitFixedDurationTaskData() override {}

	UPROPERTY(Transient)
	uint32 StartTimeMS = 0;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};

// Dynamic Wait Task Data (Can Inherit from fixed and add the Total Duration only , but oh well, this felt easier at the time!!)

USTRUCT()
struct ABILITYSYSTEMSIMULATION_API FWaitDynamicDurationTaskData : public FWaitFixedDurationTaskData
{
	GENERATED_USTRUCT_BODY()

	virtual ~FWaitDynamicDurationTaskData() override {}

	UPROPERTY(Transient)
	uint32 TotalDurationMS = 0;

	virtual bool NetSerialize(const FNetSerializeParams& Params) override;
	virtual bool NetDeltaSerialize(const FNetSerializeParams& Params) override;
	/** Gets the type info of this FAbilityDataStructBase. MUST be overridden by derived types. */
	virtual UScriptStruct* GetScriptStruct() const override;
	/** Get string representation of this struct instance */
	virtual void ToString(FAnsiStringBuilderBase& Out) const override; 
	/** Checks if the contained data is equal, within reason. MUST be override by types that compose STATE data (sync or aux).
	 *   AuthorityState is guaranteed to be the same concrete type as 'this'.
	 */
	virtual bool ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const override;
	/** Interpolates contained data between a starting and ending block. MUST be override by types that compose STATE data (sync or aux).
	 * From and To are guaranteed to be the same concrete type as 'this'.
	 */
	virtual void Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct) override;
};