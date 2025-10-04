// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitGameplayTagPredictionTask.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Tasks/GASTasks/CommonDataTypes.h"


#define LOCTEXT_NAMESPACE "WaitGameplayEventTask"



#pragma region Wait Gameplay Tag Added
UWaitGameplayTagAddedPredictionTask::UWaitGameplayTagAddedPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FExternalTargetTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayTagAddedPredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitGameplayTagAddedPredictionTask::ExecuteTaskWithTarget(AActor* ExternalTarget)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (ExternalTarget)
	{
		CachedExternalTarget = ExternalTarget;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?  GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		// if moment of activation we have the tag , this won't be a latent task and won't start, just instantly broadcast
		if (TargetAsc->HasMatchingGameplayTag(GameplayTag) && TriggerOnStartIfPresentTag)
		{
			Added.Broadcast();
			if (TriggerOnce)
			{
				return;
			}
		}
		WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedPredictionTask::GameplayTagCallback);
		ActivateTask();
	}
}

void UWaitGameplayTagAddedPredictionTask::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount == 1)
	{
		if (ShouldTriggerCallbacks())
		{
			Added.Broadcast();
			if (TriggerOnce)
			{
				DeactivateTask(false);
			}
		}
	}
}

UAbilitySystemComponent* UWaitGameplayTagAddedPredictionTask::GetFocusedASC()
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitGameplayTagAddedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (WaitTagHandle.IsValid())
	{
		GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
		WaitTagHandle.Reset();
	}
	
}

void UWaitGameplayTagAddedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AuthorityWaitEventData = static_cast<const FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AuthorityWaitEventData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AuthorityWaitEventData->ExternalTarget);;
		// we are already bound
		if (WaitTagHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
				WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedPredictionTask::GameplayTagCallback);
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedPredictionTask::GameplayTagCallback);
		}
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (WaitTagHandle.IsValid())
		{
			GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
			WaitTagHandle.Reset();
		}
	}
}

void UWaitGameplayTagAddedPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FExternalTargetTaskData* ExternalTargetData = static_cast<const FExternalTargetTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
}

void UWaitGameplayTagAddedPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FExternalTargetTaskData* ExternalTargetData = static_cast< FExternalTargetTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	
}

FText UWaitGameplayTagAddedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayTagAddedTask", "Wait Gameplay Tag Added Prediction Task");
}

FLinearColor UWaitGameplayTagAddedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#pragma region Wait Gameplay Tag Removed
UWaitGameplayTagARemovedPredictionTask::UWaitGameplayTagARemovedPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FExternalTargetTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayTagAddedPredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitGameplayTagARemovedPredictionTask::ExecuteTaskWithTarget(AActor* ExternalTarget)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (ExternalTarget)
	{
		CachedExternalTarget = ExternalTarget;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?  GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		// if moment of activation we don't have the tag , this won't be a latent task and won't start, just instantly broadcast
		if (TargetAsc->HasMatchingGameplayTag(GameplayTag) == false && TriggerOnStartIfAbsentTag)
		{
			Removed.Broadcast();
			if (TriggerOnce)
			{
				return;
			}
		}
		WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagARemovedPredictionTask::GameplayTagCallback);
		ActivateTask();
	}
}

void UWaitGameplayTagARemovedPredictionTask::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount == 0)
	{
		if (ShouldTriggerCallbacks())
		{
			Removed.Broadcast();
			if (TriggerOnce)
			{
				DeactivateTask(false);
			}
		}
	}
}

UAbilitySystemComponent* UWaitGameplayTagARemovedPredictionTask::GetFocusedASC() const
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitGameplayTagARemovedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (WaitTagHandle.IsValid())
	{
		GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
		WaitTagHandle.Reset();
	}
	
}

void UWaitGameplayTagARemovedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AuthorityWaitEventData = static_cast<FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AuthorityWaitEventData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AuthorityWaitEventData->ExternalTarget);
		// we are already bound
		if (WaitTagHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
				WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagARemovedPredictionTask::GameplayTagCallback);
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagARemovedPredictionTask::GameplayTagCallback);
		}
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (WaitTagHandle.IsValid())
		{
			GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
			WaitTagHandle.Reset();
		}
	}
}

void UWaitGameplayTagARemovedPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FExternalTargetTaskData* ExternalTargetData = static_cast<const FExternalTargetTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
}

void UWaitGameplayTagARemovedPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FExternalTargetTaskData* ExternalTargetData = static_cast< FExternalTargetTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	
}

FText UWaitGameplayTagARemovedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayTagRemovedTask", "Wait Gameplay Tag Removed Prediction Task");
}

FLinearColor UWaitGameplayTagARemovedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#pragma region Wait Gameplay Tag Added Then Removed


#pragma region Gameplay Tag Added Then Removed Task Data

bool FWaitTagAddedThenRemovedTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	bool HasExternalTarget = Ar.IsSaving() ? ExternalTarget != nullptr : false;
	Ar.SerializeBits(&HasExternalTarget,1);
	if (HasExternalTarget)
	{
		Ar << ExternalTarget;
	}
	else
	{
		ExternalTarget = nullptr;
	}
	Ar.SerializeBits(&bAlreadyAdded,1);
	return true;
}

bool FWaitTagAddedThenRemovedTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FWaitTagAddedThenRemovedTaskData* BaseDelta = Params.GetBaseDeltaState<FWaitTagAddedThenRemovedTaskData>();
	check(BaseDelta)
	FArchive& Ar = Params.Ar;
	bool HasExternalTarget = Ar.IsSaving() ? ExternalTarget != nullptr : false;
	Ar.SerializeBits(&HasExternalTarget,1);
	if (HasExternalTarget)
	{
		bool SameAsDelta = Ar.IsSaving() ? ExternalTarget == BaseDelta->ExternalTarget : false;
		Ar.SerializeBits(&SameAsDelta,1);
		if (SameAsDelta)
		{
			ExternalTarget = BaseDelta->ExternalTarget;
			return true;
		}
		Ar << ExternalTarget;
		return true;
	}
	ExternalTarget = nullptr;
	Ar.SerializeBits(&bAlreadyAdded,1);
	return true;
}

UScriptStruct* FWaitTagAddedThenRemovedTaskData::GetScriptStruct() const
{
	return FWaitTagAddedThenRemovedTaskData::StaticStruct();
}

void FWaitTagAddedThenRemovedTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	if (ExternalTarget)
	{
		Out.Appendf("Target : %s" , TCHAR_TO_ANSI(*ExternalTarget.GetName()));
	}
	Out.Appendf("Tag Got Added : %s" , bAlreadyAdded ? TEXT("True") : TEXT("False"));
}

bool FWaitTagAddedThenRemovedTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FWaitTagAddedThenRemovedTaskData* AuthState = static_cast<const FWaitTagAddedThenRemovedTaskData*>(&AuthorityState);
	const bool DiffExternalTarget = ExternalTarget != AuthState->ExternalTarget;
	const bool DiffAddedState = bAlreadyAdded != AuthState->bAlreadyAdded;
	return DiffExternalTarget || DiffAddedState;
}

void FWaitTagAddedThenRemovedTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FWaitTagAddedThenRemovedTaskData* ToState = static_cast<const FWaitTagAddedThenRemovedTaskData*>(&To);
	ExternalTarget = ToState->ExternalTarget;
	bAlreadyAdded = ToState->bAlreadyAdded;
}

#pragma endregion

UWaitGameplayTagAddedThenRemovedPredictionTask::UWaitGameplayTagAddedThenRemovedPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FWaitTagAddedThenRemovedTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayTagAddedThenRemovedPredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::ExecuteTaskWithTarget(AActor* ExternalTarget)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (ExternalTarget)
	{
		CachedExternalTarget = ExternalTarget;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?  GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		// if moment of activation we don't have the tag , this won't be a latent task and won't start, just instantly broadcast
		if (TargetAsc->HasMatchingGameplayTag(GameplayTag) && WaitRemovedOnStartIfPresentTag)
		{
			bAlreadyAdded = true;
		}
		WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedThenRemovedPredictionTask::GameplayTagCallback);
		ActivateTask();
	}
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (NewCount == 1)
	{
		bAlreadyAdded = true;
	}
	if (NewCount == 0)
	{
		if (ShouldTriggerCallbacks() && bAlreadyAdded)
		{
			Removed.Broadcast();
			if (TriggerOnce)
			{
				DeactivateTask(false);
			}
		}
	}
}

UAbilitySystemComponent* UWaitGameplayTagAddedThenRemovedPredictionTask::GetFocusedASC() const
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (WaitTagHandle.IsValid())
	{
		GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
		WaitTagHandle.Reset();
	}
	
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FWaitTagAddedThenRemovedTaskData* AuthorityWaitEventData = static_cast<FWaitTagAddedThenRemovedTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AuthorityWaitEventData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AuthorityWaitEventData->ExternalTarget);
		// we are already bound
		if (WaitTagHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
				WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedThenRemovedPredictionTask::GameplayTagCallback);
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagAddedThenRemovedPredictionTask::GameplayTagCallback);
		}
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (WaitTagHandle.IsValid())
		{
			GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
			WaitTagHandle.Reset();
		}
	}
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FWaitTagAddedThenRemovedTaskData* ExternalTargetData = static_cast<const FWaitTagAddedThenRemovedTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
	bAlreadyAdded = ExternalTargetData->bAlreadyAdded;
}

void UWaitGameplayTagAddedThenRemovedPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FWaitTagAddedThenRemovedTaskData* ExternalTargetData = static_cast< FWaitTagAddedThenRemovedTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	ExternalTargetData->bAlreadyAdded = bAlreadyAdded;
	
}

FText UWaitGameplayTagAddedThenRemovedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayTagAddedThenRemovedTask", "Wait Gameplay Tag Added Then Removed Prediction Task");
}

FLinearColor UWaitGameplayTagAddedThenRemovedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#pragma region Wait Gameplay Tag Coount Change
UWaitGameplayTagCountChangedPredictionTask::UWaitGameplayTagCountChangedPredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DataType = FExternalTargetTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitGameplayTagAddedPredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitGameplayTagCountChangedPredictionTask::ExecuteTaskWithTarget(AActor* ExternalTarget)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (ExternalTarget)
	{
		CachedExternalTarget = ExternalTarget;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?
		GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag,EGameplayTagEventType::AnyCountChange).AddUObject
		(this, &UWaitGameplayTagCountChangedPredictionTask::GameplayTagCallback);
		ActivateTask();
	}
}

void UWaitGameplayTagCountChangedPredictionTask::GameplayTagCallback(const FGameplayTag Tag, int32 NewCount)
{
	if (ShouldTriggerCallbacks())
	{
		TagCountChanged.Broadcast(NewCount);
	}
}

UAbilitySystemComponent* UWaitGameplayTagCountChangedPredictionTask::GetFocusedASC() const
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitGameplayTagCountChangedPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (WaitTagHandle.IsValid())
	{
		GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
		WaitTagHandle.Reset();
	}
	
}

void UWaitGameplayTagCountChangedPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AuthorityWaitEventData = static_cast<FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AuthorityWaitEventData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AuthorityWaitEventData->ExternalTarget);
		// we are already bound
		if (WaitTagHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
				WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagCountChangedPredictionTask::GameplayTagCallback);
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			WaitTagHandle = TargetAsc->RegisterGameplayTagEvent(GameplayTag).AddUObject(this, &UWaitGameplayTagCountChangedPredictionTask::GameplayTagCallback);
		}
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (WaitTagHandle.IsValid())
		{
			GetFocusedASC()->RegisterGameplayTagEvent(GameplayTag).Remove(WaitTagHandle);
			WaitTagHandle.Reset();
		}
	}
}

void UWaitGameplayTagCountChangedPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FExternalTargetTaskData* ExternalTargetData = static_cast<const FExternalTargetTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
}

void UWaitGameplayTagCountChangedPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FExternalTargetTaskData* ExternalTargetData = static_cast< FExternalTargetTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	
}

FText UWaitGameplayTagCountChangedPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitGameplayTagRemovedTask", "Wait Gameplay Tag Count Changed Prediction Task");
}

FLinearColor UWaitGameplayTagCountChangedPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#undef LOCTEXT_NAMESPACE
