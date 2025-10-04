// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/WaitAttributeChangePredictionTask.h"

#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameplayEffectExtension.h"
#include "GameplayEffectTypes.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Tasks/GASTasks/CommonDataTypes.h"

#define LOCTEXT_NAMESPACE "WaitAttributeChangedTask"

#pragma region Wait Owner Atrribute Changed Task Class
UWaitOwnerAttributeChangePredictionTask::UWaitOwnerAttributeChangePredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = nullptr;
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitOwnerAttributeChangePredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}

void UWaitOwnerAttributeChangePredictionTask::ExecuteTask()
{
	if (IsInRollback())
	{
		return;
	}
	if (GetFocusedASC())
	{
		OnAttributeChangeDelegateHandle = GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UWaitOwnerAttributeChangePredictionTask::OnAttributeChange);
		ActivateTask();
	}
}

void UWaitOwnerAttributeChangePredictionTask::OnAttributeChange(const FOnAttributeChangeData& CallbackData)
{
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	const float NewValue = CallbackData.NewValue;
	const float OldValue = CallbackData.OldValue;
	const FGameplayEffectModCallbackData* Data = CallbackData.GEModData;

	if (Data == nullptr)
	{
		// There may be no execution data associated with this change, for example a GE being removed. 
		// In this case, we auto fail any WithTag requirement and auto pass any WithoutTag requirement
		if (WithTag.IsValid())
		{
			return;
		}
	}
	else
	{
		if ((WithTag.IsValid() && !Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithTag)) ||
			(WithoutTag.IsValid() && Data->EffectSpec.CapturedSourceTags.GetAggregatedTags()->HasTag(WithoutTag)))
		{
			// Failed tag check
			return;
		}
	}	

	bool PassedComparison = true;
	switch (ComparisonType)
	{
	case ExactlyEqualTo:
		PassedComparison = (NewValue == ComparisonValue);
		break;		
	case GreaterThan:
		PassedComparison = (NewValue > ComparisonValue);
		break;
	case GreaterThanOrEqualTo:
		PassedComparison = (NewValue >= ComparisonValue);
		break;
	case LessThan:
		PassedComparison = (NewValue < ComparisonValue);
		break;
	case LessThanOrEqualTo:
		PassedComparison = (NewValue <= ComparisonValue);
		break;
	case NotEqualTo:
		PassedComparison = (NewValue != ComparisonValue);
		break;
	default:
		break;
	}
	if (PassedComparison)
	{
		OnChange.Broadcast(OldValue,NewValue);
		if (bTriggerOnce)
		{
			DeactivateTask(false);
			GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
			OnAttributeChangeDelegateHandle.Reset();
		}
	}
}

UAbilitySystemComponent* UWaitOwnerAttributeChangePredictionTask::GetFocusedASC() const
{
	return GetAbilitySystemComponent();
}

void UWaitOwnerAttributeChangePredictionTask::OnPreDeactivate(const bool& bWasCancelled)
{
	Super::OnPreDeactivate(bWasCancelled);
	if (OnAttributeChangeDelegateHandle.IsValid())
	{
		GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
		OnAttributeChangeDelegateHandle.Reset();
	}
}

void UWaitOwnerAttributeChangePredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AttributeChangedAuthorityData = static_cast<FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AttributeChangedAuthorityData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AttributeChangedAuthorityData->ExternalTarget);
		// we are Not bound
		if (!OnAttributeChangeDelegateHandle.IsValid())
		{
			OnAttributeChangeDelegateHandle = GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UWaitOwnerAttributeChangePredictionTask::OnAttributeChange);
		}
		
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (OnAttributeChangeDelegateHandle.IsValid())
		{
			GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
			OnAttributeChangeDelegateHandle.Reset();
		}
	}
}

FText UWaitOwnerAttributeChangePredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitOwnerAttributeChangedTask", "Wait Owner Attribute Changed Prediction Task");
}

FLinearColor UWaitOwnerAttributeChangePredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#pragma region Wait Target Atrribute Changed Task Class
UWaitTargetAttributeChangePredictionTask::UWaitTargetAttributeChangePredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = FExternalTargetTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UWaitTargetAttributeChangePredictionTask,ExecuteTaskWithTarget);
	ShouldTaskTick = false;
}

void UWaitTargetAttributeChangePredictionTask::ExecuteTaskWithTarget(AActor* TargetActor)
{
	if (IsInRollback())
	{
		return;
	}
	CachedExternalTarget = nullptr;
	if (TargetActor)
	{
		CachedExternalTarget = TargetActor;
	}

	if (UAbilitySystemComponent* TargetAsc = CachedExternalTarget == nullptr ?  GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget))
	{
		OnAttributeChangeDelegateHandle = TargetAsc->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UWaitTargetAttributeChangePredictionTask::OnAttributeChange);
		ActivateTask();
	}
}

UAbilitySystemComponent* UWaitTargetAttributeChangePredictionTask::GetFocusedASC() const
{
	if (CachedExternalTarget)
	{
		return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(CachedExternalTarget);
	}
	return GetAbilitySystemComponent();
}

void UWaitTargetAttributeChangePredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	// this override the super to ensure we have the same target and bind to its ASC.
	
	// we should be active
	if (AuthoritySyncData.IsActive)
	{
		const FExternalTargetTaskData* AttributeChangedAuthorityData = static_cast<FExternalTargetTaskData*>(AuthoritySyncData.TaskDataPointer.Get());
		UAbilitySystemComponent* TargetAsc = AttributeChangedAuthorityData->ExternalTarget == nullptr ? GetAbilitySystemComponent() : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(AttributeChangedAuthorityData->ExternalTarget);
		// we are already bound
		if (OnAttributeChangeDelegateHandle.IsValid())
		{
			// if we are bound to wrong target, unbind and rebind to correct target, otherwise we are already bound as we should
			if (GetFocusedASC() != TargetAsc)
			{
				GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
				OnAttributeChangeDelegateHandle = TargetAsc->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UWaitTargetAttributeChangePredictionTask::OnAttributeChange);
			}
		}
		else // we are not bound , but should be, directly bind to correct target
		{
			OnAttributeChangeDelegateHandle = TargetAsc->GetGameplayAttributeValueChangeDelegate(Attribute).AddUObject(this, &UWaitTargetAttributeChangePredictionTask::OnAttributeChange);
		}
	}
	else // we shouldn't be active
	{
		// we are bound , so unbind and reset.
		if (OnAttributeChangeDelegateHandle.IsValid())
		{
			GetFocusedASC()->GetGameplayAttributeValueChangeDelegate(Attribute).Remove(OnAttributeChangeDelegateHandle);
			OnAttributeChangeDelegateHandle.Reset();
		}
	}
}
void UWaitTargetAttributeChangePredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FExternalTargetTaskData* ExternalTargetData = static_cast<const FExternalTargetTaskData*>(DataToRead.Get());
	CachedExternalTarget = ExternalTargetData->ExternalTarget;
}

void UWaitTargetAttributeChangePredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FExternalTargetTaskData* ExternalTargetData = static_cast< FExternalTargetTaskData*>(DataToWrite.Get());
	ExternalTargetData->ExternalTarget = CachedExternalTarget;
	
}

FText UWaitTargetAttributeChangePredictionTask::GetNodeTitle() const
{
	return LOCTEXT("WaitTargetAttributeChangedTask", "Wait Target Attribute Changed Prediction Task");
}

FLinearColor UWaitTargetAttributeChangePredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.243f, 0.902f, 0.62f, 1.0f);
}
#pragma endregion

#undef LOCTEXT_NAMESPACE
