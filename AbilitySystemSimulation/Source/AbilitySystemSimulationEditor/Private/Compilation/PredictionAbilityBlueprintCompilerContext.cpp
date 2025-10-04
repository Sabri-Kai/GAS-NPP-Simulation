// 2025 Yohoho Productions /  Sirkai


#include "Compilation/PredictionAbilityBlueprintCompilerContext.h"

#include "Abilities/NpGameplayAbility.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Tasks/BasePredictionTask.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Nodes/PredictionTaskNode.h"
#include "Widgets/Notifications/SNotificationList.h"

FPredictionAbilityBlueprintCompilerContext::FPredictionAbilityBlueprintCompilerContext(UNpGameplayAbilityBlueprint* SourceSketch,
                                                                                       FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
: FKismetCompilerContext(SourceSketch, InMessageLog, InCompileOptions)
{
}

FPredictionAbilityBlueprintCompilerContext::~FPredictionAbilityBlueprintCompilerContext()
{
}

void FPredictionAbilityBlueprintCompilerContext::PreCompile()
{
	FKismetCompilerContext::PreCompile();
}


void FPredictionAbilityBlueprintCompilerContext::SpawnNewClass(const FString& NewClassName)
{
	// First, attempt to find the class, in case it hasn't been serialized in yet
	NewClass = FindObject<UNpAbilityGeneratedClass>(Blueprint->GetOutermost(), *NewClassName);
	if (NewClass == NULL)
	{
		// If the class hasn't been found, then spawn a new one
		NewClass = NewObject<UNpAbilityGeneratedClass>(Blueprint->GetOutermost(), FName(*NewClassName), RF_Public | RF_Transactional);
	}
	else
	{
		// Already existed, but wasn't linked in the Blueprint yet due to load ordering issues
		NewClass->ClassGeneratedBy = Blueprint;
		FBlueprintCompileReinstancer::Create(NewClass);
	}
}


void FPredictionAbilityBlueprintCompilerContext::CopyTermDefaultsToDefaultObject(UObject* DefaultObject)
{
	FKismetCompilerContext::CopyTermDefaultsToDefaultObject(DefaultObject);
	UNpGameplayAbility* AbilityCDO = Cast<UNpGameplayAbility>(DefaultObject);
	UNpGameplayAbilityBlueprint* PredictionAbilityBlueprint = Cast<UNpGameplayAbilityBlueprint>(Blueprint);
	if (AbilityCDO && PredictionAbilityBlueprint)
	{
		TArray<UBasePredictionTask*> CurrentTasks = AbilityCDO->PredictionTasksInstances;
		AbilityCDO->PredictionTasksInstances.Empty();
		
		CopyTasksFromParent(AbilityCDO, CurrentTasks);

		CopyTasksFromGraphs(AbilityCDO, CurrentTasks);
		
		CleanupDanglingFunctions(AbilityCDO);
		CheckTasksNamesAndPrioritySort(AbilityCDO);
	}
}

void FPredictionAbilityBlueprintCompilerContext::CopyTaskPropertiesIfUnmodified(UBasePredictionTask* SourceObject, UBasePredictionTask* TargetObject)
{
	if (!SourceObject || !TargetObject)
	{
		return;
	}

	// Create the comparison object
	UBasePredictionTask* ChildOriginalState = TargetObject->GetOriginalStateObject();
        
	// For each property
	for (TFieldIterator<FProperty> PropIt(SourceObject->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (!Property->HasAllPropertyFlags(CPF_Edit | CPF_BlueprintVisible) || Property->IsA(FDelegateProperty::StaticClass()))
		{
			continue;
		}
		const uint8* CurrentAddr = Property->ContainerPtrToValuePtr<uint8>(TargetObject);
		const uint8* OriginalAddr = Property->ContainerPtrToValuePtr<uint8>(ChildOriginalState);
		const uint8* ParentAddr = Property->ContainerPtrToValuePtr<uint8>(SourceObject);
		// Only propagate if child hasn't modified from original (parent value) and not equal to parent value
		if (Property->Identical(CurrentAddr, OriginalAddr) && !Property->Identical(CurrentAddr, ParentAddr))
		{
			Property->CopyCompleteValue(const_cast<uint8*>(CurrentAddr), ParentAddr);
			// a property that hasn't been modified by a child , when overriding it, also make it the "original state value"
			Property->CopyCompleteValue(const_cast<uint8*>(OriginalAddr), ParentAddr);
			if (TargetObject->GetOuter() && TargetObject->GetOuter()->CanModify())
			{
				TargetObject->GetOuter()->Modify(true);
			}
		}
		// current value is same as parent, it's possible parent changed and now is same value as child,
		// in this case make sure child now sets its default state to what the current value is.
		else if (Property->Identical(CurrentAddr, ParentAddr))
		{
			Property->CopyCompleteValue(const_cast<uint8*>(OriginalAddr), ParentAddr);
		}
	}
	ChildOriginalState->CaptureOriginalState();
	TargetObject->DefaultPropertiesValues = ChildOriginalState->DefaultPropertiesValues;
}

void FPredictionAbilityBlueprintCompilerContext::OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& Context)
{
	FKismetCompilerContext::OnPostCDOCompiled(Context);

	UNpGameplayAbility* NewCDO = Cast<UNpGameplayAbility>(NewClass->ClassDefaultObject);
	if (NewCDO && Blueprint->ParentClass->ClassGeneratedBy)
	{
		for (int32 I = 0 ; I < NewCDO->PredictionTasksInstances.Num() ; ++ I)
		{
			UBasePredictionTask* Task = NewCDO->PredictionTasksInstances[I];
			if (Task)
			{
				if (Task->DefaultPropertiesValues.Num() == 0)
				{
					Task->CaptureOriginalState();
				}
			}
		}
	}
	
}

void FPredictionAbilityBlueprintCompilerContext::TryAddInitializationFunction(UClass* Class,const FName& FunctionName)
{
	UFunction* InitializeFunction = Class->FindFunctionByName(FunctionName);
	if (!InitializeFunction)
	{
		UFunction* EventFunc = NewObject<UFunction>(Class, FunctionName, RF_Public | RF_Transient);
		// Set event flags
		EventFunc->FunctionFlags |= FUNC_Event | FUNC_BlueprintEvent | FUNC_Public;
		EventFunc->SetMetaData(TEXT("Category"), TEXT("Prediction Tasks"));

		// Finalize the event function
		EventFunc->Bind();
		EventFunc->StaticLink(true);
		Class->AddFunctionToFunctionMap(EventFunc, FunctionName);
		EventFunc->Next = Class->Children;
		Class->Children = EventFunc;
	}
}

void FPredictionAbilityBlueprintCompilerContext::TryRemoveInitializationFunction(UClass* Class,const FName& FunctionName)
{
	if (!Class)
	{
		return;
	}
	if (UFunction* EventFunc = Class->FindFunctionByName(FunctionName))
	{
		Class->RemoveFunctionFromFunctionMap(EventFunc);
	}
	
}

void FPredictionAbilityBlueprintCompilerContext::CopyTasksFromParent(UNpGameplayAbility* AbilityCDO,TArray<UBasePredictionTask*>& CurrentTasks) const
{
	// Propagate Changes From Parent Tasks
	if (!Blueprint->ParentClass)
	{
		return;
	}
	UNpGameplayAbility* ParentAbilityCDO = Cast<UNpGameplayAbility>(Blueprint->ParentClass.GetDefaultObject());
	if (!ParentAbilityCDO)
	{
		return;
	}
	for (UBasePredictionTask* ParentTask : ParentAbilityCDO->PredictionTasksInstances)
	{
		UBasePredictionTask* TaskToAdd = nullptr;
		if (!ParentTask)
		{
			continue;
		}
		for (int32 Index = CurrentTasks.Num() - 1 ; Index >= 0 ; --Index)
		{
			UBasePredictionTask* Task = CurrentTasks[Index];
			// we had this task instance before , just re-add it
			if (Task && Task->TaskId == ParentTask->TaskId)
			{
				Task->TaskName = ParentTask->TaskName;
				CopyTaskPropertiesIfUnmodified(ParentTask, Task);
				TaskToAdd = Task;
				CurrentTasks.RemoveAt(Index);
			}
		}
					
		if (!TaskToAdd)
		{
			UBasePredictionTask* DuplicateTask = DuplicateObject(ParentTask, AbilityCDO);
			DuplicateTask->TaskId = ParentTask->TaskId;
			DuplicateTask->CaptureOriginalState();
			TaskToAdd = DuplicateTask;
		}
		FName InitializeFunctionName = UBasePredictionTask::GetInitializationFunctionName(TaskToAdd->TaskId);
		TryAddInitializationFunction(AbilityCDO->GetClass(),InitializeFunctionName);
		AbilityCDO->PredictionTasksInstances.Add(TaskToAdd);
					
	}
}

void FPredictionAbilityBlueprintCompilerContext::CopyTasksFromGraphs(UNpGameplayAbility* AbilityCDO,TArray<UBasePredictionTask*>& CurrentTasks) const
{
	TArray<UK2Node_PredictionTask*> NodesTasks;

		for (TObjectPtr<UEdGraph> Graph : Blueprint->UbergraphPages)
		{
			TArray<UK2Node_PredictionTask*> PredictionTaskNodes;
			Graph->GetNodesOfClass<UK2Node_PredictionTask>(PredictionTaskNodes);
			for (UK2Node_PredictionTask* TaskNode : PredictionTaskNodes)
			{
				NodesTasks.Add(TaskNode);
			}
		}

		// loop through nodes
		// if node task instance is valid and can be found in current tasks, duplicate what we have to the task node
		// this ensures if node gets deleted, and we compile , then we undo , what's in the node has the latest settings
		for (UK2Node_PredictionTask* TaskNode : NodesTasks)
		{
			if (TaskNode->TaskInstance)
			{
				// try find node based on instanced ID
				UBasePredictionTask** FoundTask = CurrentTasks.FindByPredicate([TaskNode] (const UBasePredictionTask* Task)
				{
					if (Task)
					{
						ensure(TaskNode->TaskInstance);
						return TaskNode->TaskInstance->TaskId == Task->TaskId;
					}
					return false;
				});
				
				// we found the node task in existing one, copy exiting to node to ensure node always has latest
				if (FoundTask && *FoundTask)
				{
					UBasePredictionTask* Task = *FoundTask;
					FName InitializeFunctionName = UBasePredictionTask::GetInitializationFunctionName(Task->TaskId);
					TaskNode->TaskInstance = DuplicateObject(Task,TaskNode);
					TaskNode->TaskInstance->TaskId = Task->TaskId;
					// put task back in the list
					AbilityCDO->PredictionTasksInstances.Add(Task);
					TryAddInitializationFunction(AbilityCDO->GetClass(), InitializeFunctionName);
					continue;
				}
				// can't find task, it's new 
				UBasePredictionTask* NewTask = DuplicateObject(TaskNode->TaskInstance,AbilityCDO);
				NewTask->SetFlags(RF_Public);
				NewTask->TaskId = TaskNode->TaskInstance->TaskId;
				AbilityCDO->PredictionTasksInstances.Add(NewTask);
				FName InitializeFunctionName = UBasePredictionTask::GetInitializationFunctionName(NewTask->TaskId);
				TryAddInitializationFunction(AbilityCDO->GetClass(), InitializeFunctionName);
			}
		}
}

void FPredictionAbilityBlueprintCompilerContext::CheckTasksNamesAndPrioritySort(UNpGameplayAbility* AbilityCDO) const
{
	// check for task conflicting names, if assigned it has to be unique
		TArray<FName> Names;
		for (UBasePredictionTask* Task : AbilityCDO->PredictionTasksInstances)
		{
			if (!Task) continue;
			
			FName& TaskName = Task->TaskName;

			if (TaskName.IsNone())
			{
				continue; // Skip unnamed tasks
			}
			if (!Names.Contains(TaskName))
			{
				Names.Add(TaskName);
				continue;
			}

			// Duplicate Name Append The Task GUID Hash to the name for easily guaranteed uniqueness
			FString BaseName = TaskName.ToString();
			uint32 Hash = GetTypeHash(Task->TaskId); 

			// Mask to 16 bits for compactness
			FString Short = FString::Printf(TEXT("%04X"), Hash & 0xFFFF);
			TaskName = FName(*FString::Printf(TEXT("%s_%s"), *BaseName, *Short));
				
			const FText ErrorMessage = FText::Format(
				NSLOCTEXT("NetPredictionAbility", "AbilityBlueprint", "{0}: Task Name {1} Is Repeated , Renaming To : {2} , Change To Unique Name To Rid Of Suffix")
				,FText::FromString(*GetNameSafe(Blueprint)),FText::FromString(*BaseName),FText::FromString(*FString::Printf(TEXT("%s_%s"), *BaseName, *Short)));
			FNotificationInfo Info(ErrorMessage);
		
			Info.bUseLargeFont = false;
			Info.FadeInDuration = 0.1f;
			Info.FadeOutDuration = 0.5f;
			Info.ExpireDuration = 5.0f;
			Info.bUseThrobber = true;
			Info.bUseSuccessFailIcons = false;
			Info.bFireAndForget = true;
			Info.bAllowThrottleWhenFrameRateIsLow = true;

			const TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
			if (NotificationPtr.IsValid())
			{
				NotificationPtr->SetCompletionState(SNotificationItem::CS_None);
			}
		}

		// finally sort tasks by priority , this is so tasks that should tick ,tick in priority
		AbilityCDO->PredictionTasksInstances.Sort([](const TObjectPtr<UBasePredictionTask>& TaskA,const TObjectPtr<UBasePredictionTask>& TaskB)
		{
			if (TaskA->TaskTickPriority != TaskB->TaskTickPriority)
			{
				return TaskA->TaskTickPriority > TaskB->TaskTickPriority;
			}
			return TaskA->TaskId < TaskB->TaskId; 
		});
}

void FPredictionAbilityBlueprintCompilerContext::CleanupDanglingFunctions(UNpGameplayAbility* AbilityCDO)
{
	// This is to ensure clean up of any initialization functions that get left out, it's possible for a task instance to become null
	// when its class gets deleted for example , code above will not add null instanced to PredictionTasksInstances,
	// but it won't be able to remove its associated function.
	for (int32 i = AbilityCDO->InitializationFunctionNames.Num() - 1 ; i >= 0 ; --i) 
	{
		const FName& FunctionName  = AbilityCDO->InitializationFunctionNames[i];
		bool bFoundTaskForFunction = false;
		for (UBasePredictionTask* Task : AbilityCDO->PredictionTasksInstances)
		{
			if (FunctionName == UBasePredictionTask::GetInitializationFunctionName(Task->TaskId))
			{
				bFoundTaskForFunction = true;
				break;
			}
		}
		if (!bFoundTaskForFunction)
		{
			TryRemoveInitializationFunction(AbilityCDO->GetClass(), FunctionName);
			AbilityCDO->InitializationFunctionNames.RemoveAt(i);
		}
			
	}
}