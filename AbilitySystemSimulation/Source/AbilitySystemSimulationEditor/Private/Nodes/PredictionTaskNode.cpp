


#include "Nodes/PredictionTaskNode.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_AssignmentStatement.h"
#include "Tasks/BasePredictionTask.h"
#include "KismetCompiler.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Self.h"
#include "K2Node_TemporaryVariable.h"
#include "SourceCodeNavigation.h"
#include "Abilities/NpGameplayAbility.h"
#include "Compilation/NpGameplayAbilityBlueprint.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"


#define LOCTEXT_NAMESPACE "PredictionTaskNode"

class UK2Node_Self;

UK2Node_PredictionTask::UK2Node_PredictionTask(const FObjectInitializer& ObjectInitializer)
{
}


bool UK2Node_PredictionTask::HandleDelegates(
	const TArray<FBasePredictionTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs, UEdGraphPin* ProxyObjectPin,
	UEdGraphPin*& InOutLastThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext)
{
	bool bIsErrorFree = true;
	for (TFieldIterator<FMulticastDelegateProperty> PropertyIt(TaskInstance.GetClass()); PropertyIt && bIsErrorFree; ++PropertyIt)
	{
		UEdGraphPin* LastActivatedThenPin = nullptr;
		bIsErrorFree &= FBasePredictionTaskHelper::HandleDelegateImplementation(*PropertyIt, VariableOutputs, ProxyObjectPin, InOutLastThenPin, LastActivatedThenPin, this, SourceGraph, CompilerContext);
	}
	return bIsErrorFree;
}


bool UK2Node_PredictionTask::FBasePredictionTaskHelper::ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction)
{
	const bool bValidDataPin = Pin
		&& !Pin->bOrphanedPin
		&& (Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec);

	const bool bProperDirection = Pin && (Pin->Direction == Direction);

	return bValidDataPin && bProperDirection;
}

bool UK2Node_PredictionTask::FBasePredictionTaskHelper::CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(DelegateInputPin && Schema && CurrentNode && SourceGraph && (FunctionName != NAME_None));
	bool bResult = true;

	// WORKAROUND, so we can create delegate from nonexistent function by avoiding check at expanding step
	// instead simply: Schema->TryCreateConnection(AddDelegateNode->GetDelegatePin(), CurrentCENode->FindPinChecked(UK2Node_CustomEvent::DelegateOutputName));
	UK2Node_Self* SelfNode = CompilerContext.SpawnIntermediateNode<UK2Node_Self>(CurrentNode, SourceGraph);
	SelfNode->AllocateDefaultPins();

	UK2Node_CreateDelegate* CreateDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_CreateDelegate>(CurrentNode, SourceGraph);
	CreateDelegateNode->AllocateDefaultPins();
	bResult &= Schema->TryCreateConnection(DelegateInputPin, CreateDelegateNode->GetDelegateOutPin());
	bResult &= Schema->TryCreateConnection(SelfNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), CreateDelegateNode->GetObjectInPin());
	CreateDelegateNode->SetFunction(FunctionName);

	return bResult;
}

bool UK2Node_PredictionTask::FBasePredictionTaskHelper::CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema)
{
	check(CENode && Function && Schema);

	bool bResult = true;
	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		const FProperty* Param = *PropIt;
		if (!Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm))
		{
			FEdGraphPinType PinType;
			bResult &= Schema->ConvertPropertyToPinType(Param, /*out*/ PinType);
			bResult &= (nullptr != CENode->CreateUserDefinedPin(Param->GetFName(), PinType, EGPD_Output));
		}
	}
	return bResult;
}

bool UK2Node_PredictionTask::FBasePredictionTaskHelper::HandleDelegateImplementation(
	FMulticastDelegateProperty* CurrentProperty, const TArray<FOutputPinAndLocalVariable>& VariableOutputs,
	UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin, UEdGraphPin*& OutLastActivatedThenPin,
	UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext)
{
	bool bIsErrorFree = true;
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(CurrentProperty && ProxyObjectPin && InOutLastThenPin && CurrentNode && SourceGraph && Schema);

	UEdGraphPin* PinForCurrentDelegateProperty = CurrentNode->FindPin(CurrentProperty->GetFName());
	if (!PinForCurrentDelegateProperty || (UEdGraphSchema_K2::PC_Exec != PinForCurrentDelegateProperty->PinType.PinCategory))
	{
		FText ErrorMessage = FText::Format(LOCTEXT("InvalidDelegateProperty", "BasePredictionTask: Cannot find execution pin for delegate"
						" Either Delegate Has a non const Reference Param. Reference Params must be const, Or Delegate Changed"), FText::FromString(CurrentProperty->GetName()));
		CompilerContext.MessageLog.Error(*ErrorMessage.ToString(), CurrentNode);
		return false;
	}

	UK2Node_CustomEvent* CurrentCENode = CompilerContext.SpawnIntermediateNode<UK2Node_CustomEvent>(CurrentNode, SourceGraph);
	{
		UK2Node_AddDelegate* AddDelegateNode = CompilerContext.SpawnIntermediateNode<UK2Node_AddDelegate>(CurrentNode, SourceGraph);
		AddDelegateNode->SetFromProperty(CurrentProperty, false, CurrentProperty->GetOwnerClass());
		AddDelegateNode->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(AddDelegateNode->FindPinChecked(UEdGraphSchema_K2::PN_Self), ProxyObjectPin);
		bIsErrorFree &= Schema->TryCreateConnection(InOutLastThenPin, AddDelegateNode->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
		InOutLastThenPin = AddDelegateNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
		CurrentCENode->CustomFunctionName = *FString::Printf(TEXT("%s_%s"), *CurrentProperty->GetName(), *CompilerContext.GetGuid(CurrentNode));
		CurrentCENode->AllocateDefaultPins();

		bIsErrorFree &= CreateDelegateForNewFunction(AddDelegateNode->GetDelegatePin(), CurrentCENode->GetFunctionName(), CurrentNode, SourceGraph, CompilerContext);
		bIsErrorFree &= CopyEventSignature(CurrentCENode, AddDelegateNode->GetDelegateSignature(), Schema);
	}

	OutLastActivatedThenPin = CurrentCENode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	for (const FOutputPinAndLocalVariable& OutputPair : VariableOutputs) // CREATE CHAIN OF ASSIGMENTS
	{
		FString DelegateName, DataPinName;
		UEdGraphPin* PinWithData = CurrentCENode->FindPin(OutputPair.OutputPin->PinName);
		if (PinWithData == nullptr)
		{
			// this will be hit for each output data pin that is in the node but doesn't come from the delegate we are currently implementing
			continue;
		}

		UK2Node_AssignmentStatement* AssignNode = CompilerContext.SpawnIntermediateNode<UK2Node_AssignmentStatement>(CurrentNode, SourceGraph);
		AssignNode->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(OutLastActivatedThenPin, AssignNode->GetExecPin());
		bIsErrorFree &= Schema->TryCreateConnection(OutputPair.TempVar->GetVariablePin(), AssignNode->GetVariablePin());
		AssignNode->NotifyPinConnectionListChanged(AssignNode->GetVariablePin());
		bIsErrorFree &= Schema->TryCreateConnection(AssignNode->GetValuePin(), PinWithData);
		AssignNode->NotifyPinConnectionListChanged(AssignNode->GetValuePin());

		OutLastActivatedThenPin = AssignNode->GetThenPin();
	}

	bIsErrorFree &= CompilerContext.MovePinLinksToIntermediate(*PinForCurrentDelegateProperty, *OutLastActivatedThenPin).CanSafeConnect();
	return bIsErrorFree;
}




bool UK2Node_PredictionTask::IsCompatibleWithGraph(UEdGraph const* Graph) const
{
	if (Graph == nullptr) return false;

	UBlueprint* Blueprint = FBlueprintEditorUtils::FindBlueprintForGraph(Graph);
	if (Blueprint == nullptr) return false;

	bool bIsCompatible = false;
	// Can only place events in ubergraphs , macros are not allowed, each node creates a pseudo-component
	// in the ability to be able to roll back, re-simulate the ability.
	EGraphType GraphType = Graph->GetSchema()->GetGraphType(Graph);
	if (GraphType == EGraphType::GT_Ubergraph)
	{
		bIsCompatible = true;
	}
	const bool IsPredictionAbility = Blueprint->GeneratedClass->IsChildOf(UNpGameplayAbility::StaticClass());
	return bIsCompatible && IsPredictionAbility && Super::IsCompatibleWithGraph(Graph);
}

bool UK2Node_PredictionTask::IsActionFilteredOut(class FBlueprintActionFilter const& Filter)
{
	for (UBlueprint* Blueprint : Filter.Context.Blueprints)
	{
		if (!Blueprint || !Blueprint->GeneratedClass
				|| !Blueprint->GeneratedClass->IsChildOf(UNpGameplayAbility::StaticClass()))
		{
			return true;
		}
	}
	return false;
}

void UK2Node_PredictionTask::ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const
{
	if (!TaskInstance)
	{
		MessageLog.Error(TEXT("Node @@ Needs a Task Instance to Function"),this);
	}
	if(UObject const* SourceObject = MessageLog.FindSourceObject(this))
	{
		// Lets check if it's a result of macro expansion, to give a helpful error
		if(UK2Node_MacroInstance const* MacroInstance = Cast<UK2Node_MacroInstance>(SourceObject))
		{
			// Since it's not possible to check the graph's type, just check if this is a ubergraph using the schema's name for it
			if(!(GetGraph()->HasAnyFlags(RF_Transient) && GetGraph()->GetName().StartsWith(UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString())))
			{
				MessageLog.Error(*LOCTEXT("PredictionTaskInFunctionFromMacro", "@@ is being used in Function '@@' resulting from expansion of Macro '@@'").ToString(), this, GetGraph(), MacroInstance);
			}
		}
	}
	Super::ValidateNodeDuringCompilation(MessageLog);
}

void UK2Node_PredictionTask::GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	// to keep from needlessly instantiating a UBlueprintNodeSpawner, first   
	// check to make sure that the registrar is looking for actions of this type
	// (could be regenerating actions for a specific asset, and therefore the 
	// registrar would only accept actions corresponding to that asset)
	if (ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		struct GetMenuActions_Utils
	    {
	        static UBlueprintNodeSpawner* MakeNodeSpawner(TSubclassOf<UBasePredictionTask> TaskClass)
	        {
	            UBlueprintNodeSpawner* NodeSpawner = UBlueprintNodeSpawner::Create(UK2Node_PredictionTask::StaticClass());
	            check(NodeSpawner != nullptr);

	            // Setup the spawner to create the correct task type
	            NodeSpawner->CustomizeNodeDelegate = UBlueprintNodeSpawner::FCustomizeNodeDelegate::CreateLambda(
	                [TaskClass](UEdGraphNode* NewNode, bool /*bIsTemplateNode*/)
	                {
	                    UK2Node_PredictionTask* PredictionNode = CastChecked<UK2Node_PredictionTask>(NewNode);
	                    if (PredictionNode)
	                    {
	                        // Assign the task class
	                    	PredictionNode->CreateNewGuid();
	                    	PredictionNode->SetFlags(RF_Transactional);
	                    	PredictionNode->TaskInstance = NewObject<UBasePredictionTask>(PredictionNode,TaskClass,NAME_None,RF_Public | RF_Transactional);
	                    	PredictionNode->TaskInstance->TaskId = FGuid::NewGuid();
	                    	
	                        // Copy the StartTaskFunctionName from the CDO and validate it
	                        if (const UBasePredictionTask* CDO = TaskClass->GetDefaultObject<UBasePredictionTask>())
	                        {
	                            // Add warning for empty StartTaskFunctionName
	                            if (CDO->StartTaskFunctionName.IsNone())
	                            {
	                                const FString WarningMessage = FString::Printf(TEXT("Warning: Task class '%s' has no StartTaskFunctionName set in its default object."), 
	                                    *TaskClass->GetName());
	                                FMessageLog("LoadErrors").Warning(FText::FromString(WarningMessage));
	                            }
	                        }
	                    }
	                });

	            return NodeSpawner;
	        }
	    };

	    // Register node spawners for all UBasePredictionTask subclasses
	    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	    {
	        UClass* Class = *ClassIt;
	        if (!Class->HasAnyClassFlags(CLASS_Abstract) && Class->IsChildOf(UBasePredictionTask::StaticClass()))
	        {
	            if (UBlueprintNodeSpawner* NodeSpawner = GetMenuActions_Utils::MakeNodeSpawner(Class))
	            {
	                ActionRegistrar.AddBlueprintAction(Class, NodeSpawner);
	            }
	        }
	    }
	}
}

bool UK2Node_PredictionTask::HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const
{
	bool bProxyFactoryResult = false;
	if (TaskInstance && OptionalOutput)
	{
		bProxyFactoryResult = true;
		OptionalOutput->AddUnique(TaskInstance.GetClass());
	}

	const bool bSuperResult = Super::HasExternalDependencies(OptionalOutput);
	return bProxyFactoryResult || bSuperResult;
}

FName UK2Node_PredictionTask::GetCornerIcon() const
{
	return TEXT("Graph.Latent.LatentIcon");
}

bool UK2Node_PredictionTask::CanJumpToDefinition() const
{
	return true;
}

void UK2Node_PredictionTask::JumpToDefinition() const
{
	if (!TaskInstance)
	{
		Super::JumpToDefinition();
		return;
	}

	UClass* Class = TaskInstance.GetClass();

	// Case 1: Blueprint-defined class
	if (UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(Class))
	{
		if (UBlueprint* Blueprint = Cast<UBlueprint>(BPClass->ClassGeneratedBy))
		{
			// Open the Blueprint editor for this asset
			GEditor->EditObject(Blueprint);
			return;
		}
	}

	FSourceCodeNavigation::NavigateToClass(Class);
}

FText UK2Node_PredictionTask::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if(TaskInstance)
	{
		
		return TaskInstance->GetNodeTitle_Internal();
		
	}
	return LOCTEXT("K2Node_PredictionTask_NodeTitle", "Prediction Task");
}

FLinearColor UK2Node_PredictionTask::GetNodeTitleColor() const
{
	if(TaskInstance)
	{
		return TaskInstance->GetNodeTitleColor_Internal();
	}
	return Super::GetNodeTitleColor();
}

void UK2Node_PredictionTask::DestroyNode()
{
	Super::DestroyNode();
}


void UK2Node_PredictionTask::PostPasteNode()
{
	Super::PostPasteNode();
	if (TaskInstance)
	{
		TaskInstance->Rename(nullptr,this);
		TaskInstance->TaskId = FGuid::NewGuid();
	}
}

FText UK2Node_PredictionTask::GetMenuCategory() const
{
	return LOCTEXT("K2Node_PredictionTask_Category", "Prediction Task");
}

void UK2Node_PredictionTask::AllocateDefaultPins()
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
    // Create execution pins
    CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
    CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	
	if (!IsValid(TaskInstance))
	{
		return;
	}

	UClass* TaskClass = TaskInstance.GetClass();
	//Create Pins for the Start Task function which is triggers when the node input pin hits
	UFunction* StartTaskFunction = GetStartTaskFunction();
	
	if (!StartTaskFunction)
	{
		return;
	}
	AllocateFunctionPins(false,StartTaskFunction);

	
	for (UFunction* AdditionalInputFunction : GetAdditionalInputFunctions())
	{
		if (AdditionalInputFunction)
		{
			AllocateFunctionPins(true,AdditionalInputFunction);
		}
	}

	// create pins for delegates
	for (TFieldIterator<FProperty> PropertyIt(TaskClass); PropertyIt; ++PropertyIt)
	{
		
		if (FMulticastDelegateProperty* Property = CastField<FMulticastDelegateProperty>(*PropertyIt))
		{
			bool ValidDelegate = true;
			if (Property->SignatureFunction)
			{
				for (TFieldIterator<FProperty> PropIt(Property->SignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
				{
					FProperty* Param = *PropIt;
					const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
					if (!bIsFunctionInput)
					{
						ValidDelegate = false;
						break;
					}
				}
			}

			if (ValidDelegate)
			{
				UEdGraphPin* ExecPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, Property->GetFName());
				ExecPin->PinToolTip = Property->GetToolTipText().ToString();
				ExecPin->PinFriendlyName = Property->GetDisplayNameText();

				if (Property->SignatureFunction)
				{
					for (TFieldIterator<FProperty> PropIt(Property->SignatureFunction); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
					{
						FProperty* Param = *PropIt;
						const bool bIsFunctionInput = !Param->HasAnyPropertyFlags(CPF_OutParm) || Param->HasAnyPropertyFlags(CPF_ReferenceParm);
						if (bIsFunctionInput)
						{
							UEdGraphPin* Pin = CreatePin(EGPD_Output, NAME_None, Param->GetFName());
							K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);
							UK2Node_CallFunction::GeneratePinTooltipFromFunction(*Pin, Property->SignatureFunction);
						}
					}
				}
			}
			
		}
	}
}

void UK2Node_PredictionTask::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	check(SourceGraph && Schema);

	UClass* BlueprintClass = GetBlueprintClassFromNode();

	UNpGameplayAbilityBlueprint* AbilityBP = Cast<UNpGameplayAbilityBlueprint>(CompilerContext.Blueprint);
	if (!AbilityBP)
	{
		CompilerContext.MessageLog.Error(TEXT("Wrong Ability Blueprint for node @@"), this);
		return;
	}
	bool bIsErrorFree = true;
	 if (!TaskInstance)
	{
	    CompilerContext.MessageLog.Error(TEXT("Task class not specified for node @@"), this);
	    return;
	}
	UClass* TaskClass = TaskInstance.GetClass();
	UBasePredictionTask* CDO = TaskClass->GetDefaultObject<UBasePredictionTask>();
	if (!CDO)
	{
	    return;
	}

	if (!GetStartTaskFunction())
	{
		CompilerContext.MessageLog.Error(TEXT("Start Task Function not specified for node @@ , Maybe Got Deleted??"), this);
	}
	
	// 1. Generate Event name
	const FName InitializeTaskEventName = UBasePredictionTask::GetInitializationFunctionName(TaskInstance->TaskId);
	if (BlueprintClass->FindFunctionByName(InitializeTaskEventName) == nullptr)
	{
		// Create the Event for Task Initialization
		UFunction* EventFunc = NewObject<UFunction>(BlueprintClass, InitializeTaskEventName, RF_Public | RF_Transient);
		if (!EventFunc)
		{
			CompilerContext.MessageLog.Error(TEXT("Failed to create event function for @@"), this);
			return;
		}
		// Set event flags
		EventFunc->FunctionFlags |= FUNC_Event | FUNC_BlueprintEvent | FUNC_Public;
		EventFunc->SetMetaData(TEXT("Category"), TEXT("Prediction Tasks"));

		// Finalize the event function
		EventFunc->Bind();
		EventFunc->StaticLink(true);
		BlueprintClass->AddFunctionToFunctionMap(EventFunc, InitializeTaskEventName);
		EventFunc->Next = BlueprintClass->Children;
		BlueprintClass->Children = EventFunc;
	}
	
	// Create the event node
	UK2Node_Event* EventNode = CompilerContext.SpawnIntermediateNode<UK2Node_Event>(this, SourceGraph);
	EventNode->EventReference.SetExternalMember(InitializeTaskEventName, BlueprintClass);
	EventNode->bOverrideFunction = true;
	EventNode->AllocateDefaultPins();
	
	UEdGraphPin* LastThenPin = EventNode->FindPinChecked(UEdGraphSchema_K2::PN_Then);
	
	// Create call to TaskStart
	UK2Node_CallFunction* TaskStartNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	TaskStartNode->FunctionReference.SetExternalMember(GetStartTaskFunctionName(), TaskClass);
	TaskStartNode->AllocateDefaultPins();
	if (TaskStartNode->GetTargetFunction() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Couldn't find target function in task class @@"), this);
		return;
	}
	// Create Additional Input Functions Nodes
	TArray<TPair<UK2Node_CallFunction*,UK2Node_IfThenElse*>> AdditionalFunctionNodes;
	for (const FName& FunctionName : GetAdditionalInputFunctionNames())
	{
		UK2Node_CallFunction* AdditionalInputFunctionNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		AdditionalInputFunctionNode->FunctionReference.SetExternalMember(FunctionName, TaskClass);
		AdditionalInputFunctionNode->AllocateDefaultPins();
		if (AdditionalInputFunctionNode->GetTargetFunction() == nullptr)
		{
			CompilerContext.MessageLog.Error(TEXT("Couldn't find target function in task class @@"), this);
			return;
		}
		// Add check if task is in rollback , return early. tasks are not allowed to trigger execution pins if we are in currently rolling back
		UK2Node_CallFunction* IsRestoringFrame = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
		IsRestoringFrame->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UNpGameplayAbility,IsRestoringFrame),UNpGameplayAbility::StaticClass());
		IsRestoringFrame->AllocateDefaultPins();
		check(IsRestoringFrame->IsNodePure())

		UK2Node_IfThenElse* IsRestoringFrameBranch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
		IsRestoringFrameBranch->AllocateDefaultPins();
		bIsErrorFree &= Schema->TryCreateConnection(IsRestoringFrame->GetReturnValuePin(), IsRestoringFrameBranch->GetConditionPin());
		AdditionalFunctionNodes.Add({AdditionalInputFunctionNode,IsRestoringFrameBranch});
	}

	UK2Node_CallFunction* GetTaskNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	GetTaskNode->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UNpGameplayAbility,GetTaskFromGuid),UNpGameplayAbility::StaticClass());
	GetTaskNode->AllocateDefaultPins();
	
	if (GetTaskNode->GetTargetFunction() == nullptr)
	{
		CompilerContext.MessageLog.Error(TEXT("Couldn't find target function in task class @@"), this);
		return;
	}
	UEdGraphPin* TaskGuidPin = GetTaskNode->FindPinChecked(TEXT("Guid"));
	// set pin default value for "GetTaskByGuid" function call
	FGuid TaskID = TaskInstance->TaskId;
	TaskGuidPin->AutogeneratedDefaultValue = TaskID.ToString();
	TaskGuidPin->DefaultValue = TaskID.ToString();
	TaskGuidPin->DefaultTextValue = FText::FromString(TaskID.ToString());
	// Get the task instance pin from the node getter function
	UEdGraphPin* TaskInstancePin = GetTaskNode->GetReturnValuePin();
	
	// Task Instance Pin is of base type so cast it , this is a sure cast because this node is the one that is defining the task in the first place
	UK2Node_DynamicCast* CastNode = CompilerContext.SpawnIntermediateNode<UK2Node_DynamicCast>(this, SourceGraph);
	CastNode->TargetType = TaskClass;
	CastNode->SetPurity(true);
	CastNode->AllocateDefaultPins();
	bIsErrorFree &= Schema->TryCreateConnection(TaskInstancePin, CastNode->GetCastSourcePin());
	CastNode->NotifyPinConnectionListChanged(CastNode->GetCastSourcePin());
	// set task instance pin to the cast return
	TaskInstancePin = CastNode->GetCastResultPin();
	// Hook up the self connection
	UEdGraphPin* ActivateCallSelfPin = TaskStartNode->FindPinChecked(UEdGraphSchema_K2::PN_Self);
	bIsErrorFree &= Schema->TryCreateConnection(TaskInstancePin, ActivateCallSelfPin);
	// Hook up self connection for additional input functions
	for (const TPair<UK2Node_CallFunction*, UK2Node_IfThenElse*>& AdditionalInputFunctionNode : AdditionalFunctionNodes)
	{
		UEdGraphPin* SelfPin = AdditionalInputFunctionNode.Key->FindPinChecked(UEdGraphSchema_K2::PN_Self);
		bIsErrorFree &= Schema->TryCreateConnection(TaskInstancePin, SelfPin);
	}
	
	// data input pins in the Prediction Node all come from the Start Task Function.
	// idea here is simple, each input function has 1 execution input pin and possibly data pins.
	
	// we start with -2 , the first input pin we encounter will be the normal input execution which is for the start function
	// we increment index to -1 and that means we are the start function.
	// next time we encounter an input pin, it will be an execution pin of next function, setting index to 0 and now maps to array of
	// AdditionalFunctionNodes
	int32 FunctionInputIndex = -2;
	for (UEdGraphPin* CurrentPin : Pins)
	{
		if (CurrentPin->Direction == EGPD_Input && CurrentPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
		{
			FunctionInputIndex++;
			// connect the input execution pins of the node to the addition input function input
			if (FunctionInputIndex >= 0)
			{
				//Move connections from node pin to the branch checking IsRestoringFrame for this additional input.
				// then connect the Else pin so RestoringFrame is false, to our actual function node
				const TPair<UK2Node_CallFunction*, UK2Node_IfThenElse*>& AdditionalFunction = AdditionalFunctionNodes[FunctionInputIndex];
				UEdGraphPin* BranchPin = AdditionalFunction.Value->GetExecPin();
				bIsErrorFree &= BranchPin && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *BranchPin).CanSafeConnect();
				UEdGraphPin* NodePin = AdditionalFunction.Key->GetExecPin();
				bIsErrorFree &= Schema->TryCreateConnection(AdditionalFunction.Value->GetElsePin(), NodePin);
			}
		}
		else if (FBasePredictionTaskHelper::ValidDataPin(CurrentPin, EGPD_Input))
		{
			// data pins
			if (FunctionInputIndex == -1)
			{
				UEdGraphPin* DestPin = TaskStartNode->FindPin(CurrentPin->PinName); // match function inputs, to pass data to function from CallFunction node
				bIsErrorFree &= DestPin && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
			if (FunctionInputIndex >= 0)
			{
				UEdGraphPin* DestPin = AdditionalFunctionNodes[FunctionInputIndex].Key->FindPin(CurrentPin->PinName); // match function inputs, to pass data to function from CallFunction node
				bIsErrorFree &= DestPin && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
			
		}
	}
	TArray<FBasePredictionTaskHelper::FOutputPinAndLocalVariable> VariableOutputs;
	bool bPassedStartTaskOutputs = false;
	for (UEdGraphPin* CurrentPin : Pins)
	{
		if (FBasePredictionTaskHelper::ValidDataPin(CurrentPin, EGPD_Output))
		{
			if (!bPassedStartTaskOutputs)
			{
				UEdGraphPin* DestPin = TaskStartNode->FindPin(CurrentPin->PinName);
				bIsErrorFree &= DestPin && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *DestPin).CanSafeConnect();
			}
			else
			{
				const FEdGraphPinType& PinType = CurrentPin->PinType;
			
				UK2Node_TemporaryVariable* TempVarOutput = CompilerContext.SpawnInternalVariable(
					this, PinType.PinCategory, PinType.PinSubCategory,
					PinType.PinSubCategoryObject.Get(), PinType.ContainerType, PinType.PinValueType);
			
				bIsErrorFree &= TempVarOutput->GetVariablePin() && CompilerContext.MovePinLinksToIntermediate(*CurrentPin, *TempVarOutput->GetVariablePin()).CanSafeConnect();
				VariableOutputs.Add(FBasePredictionTaskHelper::FOutputPinAndLocalVariable(CurrentPin, TempVarOutput));
			}
		}
		else if (!bPassedStartTaskOutputs && CurrentPin && CurrentPin->Direction == EGPD_Output)
		{
			// the first exec that isn't the node's then pin is the start of the asyc delegate pins
			// once we hit this point, we've iterated beyond all outputs for the factory function
			bPassedStartTaskOutputs = (CurrentPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) && (CurrentPin->PinName != UEdGraphSchema_K2::PN_Then);
		}
	}
	
	UK2Node_CallFunction* IsValidFuncNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	const FName IsValidFuncName = GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, IsValid);
	IsValidFuncNode->FunctionReference.SetExternalMember(IsValidFuncName, UKismetSystemLibrary::StaticClass());
	IsValidFuncNode->AllocateDefaultPins();
	UEdGraphPin* IsValidInputPin = IsValidFuncNode->FindPinChecked(TEXT("Object"));

	bIsErrorFree &= Schema->TryCreateConnection(TaskInstancePin, IsValidInputPin);

	UK2Node_IfThenElse* ValidateProxyNode = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	ValidateProxyNode->AllocateDefaultPins();
	bIsErrorFree &= Schema->TryCreateConnection(IsValidFuncNode->GetReturnValuePin(), ValidateProxyNode->GetConditionPin());

	bIsErrorFree &= Schema->TryCreateConnection(LastThenPin, ValidateProxyNode->GetExecPin());
	LastThenPin = ValidateProxyNode->GetThenPin(); // current execution pin is IsValid Branch "True" pin

	// Add check if task is in rollback , return early. tasks are not allowed to trigger execution pin if we are in currently rolling back
	UK2Node_CallFunction* IsRestoringFrame = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	IsRestoringFrame->FunctionReference.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UNpGameplayAbility,IsRestoringFrame),UNpGameplayAbility::StaticClass());
	IsRestoringFrame->AllocateDefaultPins();
	check(IsRestoringFrame->IsNodePure())

	UK2Node_IfThenElse* IsRestoringFrameBranch = CompilerContext.SpawnIntermediateNode<UK2Node_IfThenElse>(this, SourceGraph);
	IsRestoringFrameBranch->AllocateDefaultPins();
	bIsErrorFree &= Schema->TryCreateConnection(IsRestoringFrame->GetReturnValuePin(), IsRestoringFrameBranch->GetConditionPin());

	// FOR EACH DELEGATE DEFINE EVENT, CONNECT IT TO DELEGATE AND IMPLEMENT A CHAIN OF ASSIGMENTS , starting from the initial event node
	bIsErrorFree &= HandleDelegates(VariableOutputs, TaskInstancePin, LastThenPin, SourceGraph, CompilerContext);
	// Connect execution flow , this takes connection of this node execution and then pin and connects them to task start function
	// after the branch node of IsRestoringFrame, if restoring frame is true we continue the execution of the graph without calling "TaskStart"
	CompilerContext.MovePinLinksToIntermediate(*GetExecPin(), *IsRestoringFrameBranch->GetExecPin());
	bIsErrorFree &= Schema->TryCreateConnection(IsRestoringFrameBranch->GetElsePin(), TaskStartNode->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(),*TaskStartNode->GetThenPin());
	CompilerContext.CopyPinLinksToIntermediate(*TaskStartNode->GetThenPin(),*IsRestoringFrameBranch->GetThenPin());
	
	if (EventNode->FindPinChecked(UEdGraphSchema_K2::PN_Then) == LastThenPin)
	{
		CompilerContext.MessageLog.Error(*LOCTEXT("MissingDelegateProperties", "PredictionTask: Proxy has no delegates defined. @@").ToString(), this);
		return;
	}

	if(!bIsErrorFree)
	{
		CompilerContext.MessageLog.Error(TEXT("Node @@ Has Errors final"), this);
	}
	

	// Clean up original node connections
	BreakAllNodeLinks();
}

void UK2Node_PredictionTask::AllocateFunctionPins(const bool& AllocateExecPin,UFunction* Function)
{
	if (!Function)
	{
		return;
	}
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	if (AllocateExecPin)
	{
		//ToDo should use function display name meta data if it's there.
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, Function->GetFName());
	}
	for (TFieldIterator<FProperty> PropIt(Function); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		FProperty* Param = *PropIt;
		TSet<FName> PinsToHide;
		FBlueprintEditorUtils::GetHiddenPinsForFunction(GetGraph(), Function, PinsToHide);
		const bool bIsFunctionOutput = (Param->HasAnyPropertyFlags(CPF_OutParm) && !Param->HasAnyPropertyFlags(CPF_ReferenceParm)) || Param->HasAnyPropertyFlags(CPF_ReturnParm);
		if (bIsFunctionOutput)
		{
			if (AllocateExecPin)
			{
				UEdGraphPin* Pin = CreatePin(EGPD_Output, NAME_None, Param->GetFName());
				K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType);
			}
			else
			{
				UE_LOG(LogBlueprint, Error, TEXT("Task Class %s. Has Additional Input Function %s with return values, That is Not Allowed,"
									 " They will be Ignored if you want to return some values use a delegate and broadcast it"
										" to create a connection between input and output,"),*GetNameSafe(TaskInstance.GetClass()), *GetNameSafe(Function));
			}
			
		}
		else
		{
			UEdGraphNode::FCreatePinParams PinParams;
			PinParams.bIsReference = Param->HasAnyPropertyFlags(CPF_ReferenceParm);
			UEdGraphPin* Pin = CreatePin(EGPD_Input, NAME_None, Param->GetFName(), PinParams);
			const bool bPinGood = (Pin && K2Schema->ConvertPropertyToPinType(Param, /*out*/ Pin->PinType));

			if (bPinGood)
			{
				// Check for a display name override
				const FString& PinDisplayName = Param->GetMetaData(FBlueprintMetadata::MD_DisplayName);
				if (!PinDisplayName.IsEmpty())
				{
					Pin->PinFriendlyName = FText::FromString(PinDisplayName);
				}
				
				//Flag pin as read only for const reference property
				Pin->bDefaultValueIsIgnored = Param->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm) && (!Function->HasMetaData(FBlueprintMetadata::MD_AutoCreateRefTerm) || Pin->PinType.IsContainer());

				const bool bAdvancedPin = Param->HasAllPropertyFlags(CPF_AdvancedDisplay);
				Pin->bAdvancedView = bAdvancedPin;
				if(bAdvancedPin && (ENodeAdvancedPins::NoPins == AdvancedPinDisplay))
				{
					AdvancedPinDisplay = ENodeAdvancedPins::Hidden;
				}

				FString ParamValue;
				if (K2Schema->FindFunctionParameterDefaultValue(Function, Param, ParamValue))
				{
					K2Schema->SetPinAutogeneratedDefaultValue(Pin, ParamValue);
				}
				else
				{
					K2Schema->SetPinAutogeneratedDefaultValueBasedOnType(Pin);
				}

				if (PinsToHide.Contains(Pin->PinName))
				{
					Pin->bHidden = true;
				}
			}
		}
	}
}


UFunction* UK2Node_PredictionTask::GetStartTaskFunction() const
{
	if (TaskInstance == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("Task null in %s. Was a class deleted or saved on a non promoted build?"), *GetFullName());
		return nullptr;
	}
	UClass* TaskClass = TaskInstance->GetClass();
	FName FunctionName = TaskInstance->StartTaskFunctionName;
	FMemberReference FunctionReference;
	FunctionReference.SetExternalMember(FunctionName, TaskClass);

	UFunction* FactoryFunction = FunctionReference.ResolveMember<UFunction>(GetBlueprint());
	
	if (FactoryFunction == nullptr)
	{
		FactoryFunction = TaskClass->FindFunctionByName(FunctionName);
		UE_CLOG(FactoryFunction == nullptr, LogBlueprint, Error, TEXT("Start Task Function %s null in %s. Was a class deleted or saved on a non promoted build?"), *FunctionName.ToString(), *GetFullName());
	}

	return FactoryFunction;
}

TArray<UFunction*> UK2Node_PredictionTask::GetAdditionalInputFunctions() const
{
	TArray<UFunction*> OutFunctions;
	if (TaskInstance == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("Task null in %s. Was a class deleted or saved on a non promoted build?"), *GetFullName());
		return OutFunctions;
	}
	
	if (TaskInstance->AdditionalInputFunctions.IsEmpty())
	{
		return OutFunctions;
	}
	for (const FName& FunctionName : TaskInstance->AdditionalInputFunctions)
	{
		FMemberReference FunctionReference;
		FunctionReference.SetExternalMember(FunctionName, TaskInstance.GetClass());

		UFunction* FactoryFunction = FunctionReference.ResolveMember<UFunction>(GetBlueprint());
	
		if (FactoryFunction == nullptr)
		{
			FactoryFunction = TaskInstance.GetClass()->FindFunctionByName(FunctionName);
			UE_CLOG(FactoryFunction == nullptr, LogBlueprint, Error, TEXT("Start Task Function %s null in %s. Was a class deleted or saved on a non promoted build?"), *FunctionName.ToString(), *GetFullName());
		}
		if (FactoryFunction)
		{
			OutFunctions.Add(FactoryFunction);
		}
	}
	return OutFunctions;
}

TArray<FName> UK2Node_PredictionTask::GetAdditionalInputFunctionNames() const
{
	TArray<FName> OutFunctionNames;
	if (TaskInstance == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("Task Class null in %s. Was a class deleted or saved on a non promoted build?"), *GetFullName());
		return OutFunctionNames;
	}
	OutFunctionNames = TaskInstance->AdditionalInputFunctions;
	return  OutFunctionNames;
}

FName UK2Node_PredictionTask::GetStartTaskFunctionName() const
{
	if (TaskInstance == nullptr)
	{
		UE_LOG(LogBlueprint, Error, TEXT("Task Class null in %s. Was a class deleted or saved on a non promoted build?"), *GetFullName());
		return NAME_None;
	}
	return  TaskInstance->StartTaskFunctionName;
}
#undef LOCTEXT_NAMESPACE
