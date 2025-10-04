// Fill out your copyright notice in the Description page of Project Settings.


#include "Tasks/GASTasks/PlayMontageAndWaitPredictionTask.h"
#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"


#define LOCTEXT_NAMESPACE "PredictionTaskNode"

#pragma region Play Montage Task Data

bool FPlayMontageTaskData::NetSerialize(const FNetSerializeParams& Params)
{
	FArchive& Ar = Params.Ar;
	bool IsMontageValid = Ar.IsSaving() ? IsValid(MontageToPlay) : false;
	Ar.SerializeBits(&IsMontageValid,1);
	if (IsMontageValid)
	{
		Ar << MontageToPlay;
	}
	else
	{
		MontageToPlay = nullptr;
	}
	
	return true;
}

bool FPlayMontageTaskData::NetDeltaSerialize(const FNetSerializeParams& Params)
{
	const FPlayMontageTaskData* BaseData = Params.GetBaseDeltaState<FPlayMontageTaskData>();
	FArchive& Ar = Params.Ar;
	bool IsMontageValid = Ar.IsSaving() ? IsValid(MontageToPlay) : false;
	Ar.SerializeBits(&IsMontageValid,1);
	if (IsMontageValid)
	{
		bool SameAsBase = Ar.IsSaving() ? MontageToPlay == BaseData->MontageToPlay : false;
		Ar.SerializeBits(&SameAsBase,1);
		if (SameAsBase)
		{
			MontageToPlay = BaseData->MontageToPlay;
			return true;
		}
		Ar << MontageToPlay;
		return true;
	}
	
	MontageToPlay = nullptr;
	return true;
}

UScriptStruct* FPlayMontageTaskData::GetScriptStruct() const
{
	return FPlayMontageTaskData::StaticStruct();
}

void FPlayMontageTaskData::ToString(FAnsiStringBuilderBase& Out) const
{
	if (MontageToPlay)
	{
		Out.Appendf("      Montage : %s\n",TCHAR_TO_ANSI(*GetNameSafe(MontageToPlay)));
	}
}

bool FPlayMontageTaskData::ShouldReconcile(const FAbilityTaskDataBase& AuthorityState) const
{
	const FPlayMontageTaskData* AuthState = static_cast<const FPlayMontageTaskData*>(&AuthorityState);
	const bool DiffMontage = MontageToPlay != AuthState->MontageToPlay;
	return DiffMontage;
}

void FPlayMontageTaskData::Interpolate(const FAbilityTaskDataBase& From, const FAbilityTaskDataBase& To, float Pct)
{
	const FPlayMontageTaskData* ToState = static_cast<const FPlayMontageTaskData*>(&To);
	MontageToPlay = ToState->MontageToPlay;
}
#pragma endregion

#pragma region Play Montage Task Class
UPlayMontageAndWaitPredictionTask::UPlayMontageAndWaitPredictionTask(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	DataType = FPlayMontageTaskData::StaticStruct();
	StartTaskFunctionName = GET_FUNCTION_NAME_CHECKED(UPlayMontageAndWaitPredictionTask,ExecuteTask);
	ShouldTaskTick = false;
}

UAnimMontage* UPlayMontageAndWaitPredictionTask::GetMontage() const
{
	return CachedMontageToPlay;
}

void UPlayMontageAndWaitPredictionTask::ExecuteTask(UAnimMontage* AnimMontage, float StartTime, float PlayRate,
                                                    FName SectionName, float RootMotionScale)
{
	if (AnimMontage)
	{
		SetInitialData(AnimMontage);
		GetAbilitySystemComponent()->PlaySyncedMontage(AnimMontage,StartTime,PlayRate,SectionName,RootMotionScale);
		BindDelegates();
		ActivateTask();
		GetOwningAbility()->SetCurrentMontage(GetMontage());
	}
	else
	{
		OnTaskCanceled.Broadcast();
	}
}

void UPlayMontageAndWaitPredictionTask::SetInitialData(UAnimMontage* AnimMontage)
{
	CachedMontageToPlay = AnimMontage;
}

void UPlayMontageAndWaitPredictionTask::OnBlendOut(UAnimMontage* Montage)
{
	if (Montage == CachedMontageToPlay && ShouldTriggerCallbacks())
	{
		if (EndTaskOnBlendOut)
		{
			DeactivateTask(false);
		}
		OnMontageBlendOut.Broadcast();
	}
}

void UPlayMontageAndWaitPredictionTask::OnCanceled(UAnimMontage* Montage)
{
	if (Montage == CachedMontageToPlay && ShouldTriggerCallbacks())
	{
		DeactivateTask(true);
		OnMontageCanceled.Broadcast();
	}
}

void UPlayMontageAndWaitPredictionTask::OnCompleted(UAnimMontage* Montage)
{
	if (Montage == CachedMontageToPlay && ShouldTriggerCallbacks())
	{
		DeactivateTask(false);
		OnMontageCompleted.Broadcast();
	}
}

void UPlayMontageAndWaitPredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	Super::StartTaskRollback(AuthoritySyncData);
	if (AuthoritySyncData.IsActive != IsActive())
	{
		if (AuthoritySyncData.IsActive)
		{
			BindDelegates();
		}
		else
		{
			UnBindDelegates();
		}
	}
}

void UPlayMontageAndWaitPredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
	const FPlayMontageTaskData* MontageData = static_cast< const FPlayMontageTaskData*>(DataToRead.Get());
	CachedMontageToPlay = MontageData->MontageToPlay;
}

void UPlayMontageAndWaitPredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
	FPlayMontageTaskData* MontageData = static_cast< FPlayMontageTaskData*>(DataToWrite.Get());
	MontageData->MontageToPlay = CachedMontageToPlay;
}

void UPlayMontageAndWaitPredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
	Super::OnPreDeactivate(bWasCanceled);
	if (GetOwningAbility()->GetCurrentMontage() == CachedMontageToPlay)
	{
		GetOwningAbility()->SetCurrentMontage(nullptr);
	}
	CachedMontageToPlay = nullptr;
	UnBindDelegates();
}

void UPlayMontageAndWaitPredictionTask::OnOwningAbilityEnded(const bool& bWasCanceled)
{
	Super::OnOwningAbilityEnded(bWasCanceled);
	if (!ShouldTriggerCallbacks())
	{
		return;
	}
	if (bWasCanceled)
	{
		if (StopMontageIfAbilityCancels && CachedMontageToPlay)
		{
			GetAbilitySystemComponent()->StopSyncedMontage(CachedMontageToPlay, true);
		}
		OnTaskCanceled.Broadcast();
		return;
	}
	if (StopMontageIfAbilityEnds && CachedMontageToPlay)
	{
		GetAbilitySystemComponent()->StopSyncedMontage(CachedMontageToPlay);
	}
}

void UPlayMontageAndWaitPredictionTask::UnBindDelegates()
{
	if (OnBlendOutHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnMontageBlendOut.Remove(OnBlendOutHandle);
		OnBlendOutHandle.Reset();
	}
	if (OnCanceledHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnMontageCanceled.Remove(OnCanceledHandle);
		OnCanceledHandle.Reset();
	}
	if (OnCompletedHandle.IsValid())
	{
		GetAbilitySystemComponent()->OnMontageCompleted.Remove(OnCompletedHandle);
		OnCompletedHandle.Reset();
	}
}

void UPlayMontageAndWaitPredictionTask::BindDelegates()
{
	if (!OnBlendOutHandle.IsValid())
	{
		OnBlendOutHandle = GetAbilitySystemComponent()->OnMontageBlendOut.AddUObject(this,&UPlayMontageAndWaitPredictionTask::OnBlendOut);
	}
	if (!OnCanceledHandle.IsValid())
	{
		OnCanceledHandle = GetAbilitySystemComponent()->OnMontageCanceled.AddUObject(this,&UPlayMontageAndWaitPredictionTask::OnCanceled);
	}
	if (!OnCompletedHandle.IsValid())
	{
		OnCompletedHandle = GetAbilitySystemComponent()->OnMontageCompleted.AddUObject(this,&UPlayMontageAndWaitPredictionTask::OnCompleted);
	}
}

FText UPlayMontageAndWaitPredictionTask::GetNodeTitle() const
{
	return LOCTEXT("K2Node_PlayMontageAndWaitPredictionTask", "Play Montage And Wait Prediction Task");
}

FLinearColor UPlayMontageAndWaitPredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.565f, 0.243f, 0.902f, 1.0f);
}
#pragma endregion

#undef LOCTEXT_NAMESPACE
