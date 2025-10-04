// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DataTypes/BaseSyncedVariableData.h"
#include "UObject/Object.h"
#include "BasicSyncedVariablesTypes.generated.h"

/**
 * These are the most basic property types as sync vars.
 * Sync vars still need work to support an array of them.
 * and they need to properly implement delta serialization. most sync vars don't change much at all
 * and sending 1 bit instead of the entire variable is very beneficial.
 */
USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncBool : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncBool() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	bool Value = false;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncByte : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncByte() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	uint8 Value = 0;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncInteger : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncInteger() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	int32 Value = 0;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncInteger64 : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncInteger64() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	int64 Value = 0;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncFloat : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncFloat() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	float Value = 0.0f;
	//ToDo @Kai Add Serialization Config For float
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncName : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncName() {}

	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	FName Value = NAME_None;

	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncString : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncString() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	FString Value;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncVector : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	FSyncVector() {};
	virtual ~FSyncVector() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	FVector Value = FVector::ZeroVector;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncRotator : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncRotator() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	FRotator Value = FRotator::ZeroRotator;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;

	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncObject : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncObject() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	TObjectPtr<UObject> Value = nullptr;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};

USTRUCT(BlueprintType)
struct ABILITYSYSTEMSIMULATION_API FSyncGameplayTag : public FBaseSyncVar
{
	GENERATED_USTRUCT_BODY()

	virtual ~FSyncGameplayTag() {}
	
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Value")
	FGameplayTag Value;
	
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual void Serialize(const FNetSerializeParams& Params) override;
	virtual void SerializeDelta(const FNetSerializeParams& Params) override;
	virtual bool ShouldReconcile(const FBaseSyncVar& Auth) const override;
	virtual void SetValue(const FBaseSyncVar* Other) override;
	virtual void ToString(FAnsiStringBuilderBase& Out) const override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Interpolate(const FBaseSyncVar& From, const FBaseSyncVar& To, float Pct) override;
};