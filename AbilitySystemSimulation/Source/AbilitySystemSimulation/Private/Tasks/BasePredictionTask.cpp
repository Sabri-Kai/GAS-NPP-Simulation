


#include "Tasks/BasePredictionTask.h"

#include "Abilities/NpAbilitySystemComponent.h"
#include "Abilities/NpGameplayAbility.h"
#include "DataTypes/BaseTaskData.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#define LOCTEXT_NAMESPACE "PredictionTaskNode"

UBasePredictionTask::UBasePredictionTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	FProperty* Property = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBasePredictionTask, DataType));
	Property->PropertyFlags |= CPF_DisableEditOnInstance;
	FProperty* StartFunctionNameProperty = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBasePredictionTask, StartTaskFunctionName));
	StartFunctionNameProperty->PropertyFlags |= CPF_DisableEditOnInstance;
	FProperty* ShouldTickProperty = GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UBasePredictionTask, ShouldTaskTick));
	ShouldTickProperty->PropertyFlags |= CPF_DisableEditOnInstance;
#endif
	auto IsImplementedInBlueprint = [](const UFunction* Func) -> bool
	{
		return Func && ensure(Func->GetOuter())
			&& Func->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
	};

	static FName GetNodeTitleFuncName = FName(TEXT("K2_GetNodeTitle"));
	UFunction*GetNodeTitleFunction = GetClass()->FindFunctionByName(GetNodeTitleFuncName);
	bHasBlueprintGetNodeTitle = IsImplementedInBlueprint(GetNodeTitleFunction);

	static FName GetNodeTitleColorFuncName = FName(TEXT("K2_GetNodeTitleColor"));
	UFunction*GetNodeTitleColorFunction = GetClass()->FindFunctionByName(GetNodeTitleColorFuncName);
	bHasBlueprintGetNodeTitleColor = IsImplementedInBlueprint(GetNodeTitleColorFunction);
	
}

void UBasePredictionTask::CaptureOriginalState()
{
	UBasePredictionTask* TempOriginal = DuplicateObject(this,GetTransientPackage());
	FBufferArchive BufferArchive;
	FObjectAndNameAsStringProxyArchive Ar(BufferArchive, true);
	
	Ar.SetIsSaving(true);
	Ar.SetIsPersistent(true);
	TempOriginal->Serialize(Ar);
	
	BufferArchive.Close();
	DefaultPropertiesValues.Empty(BufferArchive.Num());
	DefaultPropertiesValues.Append(BufferArchive);
}

UBasePredictionTask* UBasePredictionTask::GetOriginalStateObject() const
{
	// You could cache this for performance if needed
	UBasePredictionTask* TempOriginal = NewObject<UBasePredictionTask>(GetTransientPackage(), GetClass());
        
	if (DefaultPropertiesValues.Num() > 0)
	{
		FMemoryReader MemReader(DefaultPropertiesValues);
		FObjectAndNameAsStringProxyArchive Ar(MemReader, true);
		Ar.SetIsLoading(true);
		Ar.SetIsPersistent(true);
		TempOriginal->Serialize(Ar);
	}
        
	return TempOriginal;
}

FText UBasePredictionTask::GetNodeTitle_Internal() const
{
	if (bHasBlueprintGetNodeTitle)
	{
		return K2_GetNodeTitle();
	}
	return GetNodeTitle();
}

FLinearColor UBasePredictionTask::GetNodeTitleColor_Internal() const
{
	if (bHasBlueprintGetNodeTitleColor)
	{
		return K2_GetNodeTitleColor();
	}
	return GetNodeTitleColor();
}

FName UBasePredictionTask::GetInitializationFunctionName(const FGuid& Guid)
{
	FString InitializationFunctionName = TEXT("InitializeTask");
	InitializationFunctionName += Guid.ToString();
	return FName(*InitializationFunctionName);
}

void UBasePredictionTask::RestoreFrame(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	StartTaskRollback(AuthoritySyncData);
	SetSyncedData(AuthoritySyncData);
}

void UBasePredictionTask::StartTaskRollback(const FAbilityTaskDataContainer& AuthoritySyncData)
{
}


void UBasePredictionTask::SimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
	OnSimulationTick(TimeStep);
}

void UBasePredictionTask::OnSimulationTick(const FAbilitySystemTimeStep& TimeStep)
{
}

void UBasePredictionTask::SetSyncedData(const FAbilityTaskDataContainer& AuthoritySyncData)
{
	bIsActive = AuthoritySyncData.IsActive;
	if(AuthoritySyncData.TaskDataPointer.IsValid())
	{
		ReadFromSyncedData(AuthoritySyncData.TaskDataPointer);
	}
}

void UBasePredictionTask::CancelTask() 
{
	DeactivateTask(true);
}

UNpAbilitySystemComponent* UBasePredictionTask::GetAbilitySystemComponent() const
{
	return Cast<UNpAbilitySystemComponent>(GetOwningAbility()->GetAbilitySystemComponentFromActorInfo());
}

AActor* UBasePredictionTask::GetAvatarActor() const
{
	const FGameplayAbilityActorInfo* Info = GetOwningAbility()->GetCurrentActorInfo();
	if (Info)
	{
		return Info->AvatarActor.IsValid(false) ? Info->AvatarActor.Get() : nullptr;
	}
	return nullptr;
}


void UBasePredictionTask::ActivateTask()
{
	bIsActive = true;
}

void UBasePredictionTask::DeactivateTask(const bool& bWasCanceled)
{
	if (bIsActive)
	{
		OnPreDeactivate(bWasCanceled);
		bIsActive = false;
	}
}


void UBasePredictionTask::OnOwningAbilityEnded(const bool& bWasCanceled)
{
}


void UBasePredictionTask::OnPreDeactivate(const bool& bWasCanceled)
{
}

void UBasePredictionTask::WriteToSyncedData(TSharedPtr<FAbilityTaskDataBase> DataToWrite)
{
}

void UBasePredictionTask::ReadFromSyncedData(TSharedPtr<const FAbilityTaskDataBase> DataToRead)
{
}

bool UBasePredictionTask::ShouldTriggerCallbacks() const
{
	return !IsInRollback() && IsActive();
}

bool UBasePredictionTask::IsInRollback() const
{
	return GetAbilitySystemComponent()->GetIsRestoringFrame();
}


FText UBasePredictionTask::GetNodeTitle() const
{
	return FText::FromName(GetClass()->GetFName());
}

FLinearColor UBasePredictionTask::GetNodeTitleColor() const
{
	return FLinearColor(0.190525f, 0.583898f, 1.0f, 1.0f);
}
#undef LOCTEXT_NAMESPACE
