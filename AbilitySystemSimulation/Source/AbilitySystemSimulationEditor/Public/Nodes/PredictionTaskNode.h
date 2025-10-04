// 2025 Yohoho Productions /  Sirkai

#pragma once

#include "CoreMinimal.h"
#include "K2Node.h"
#include "PredictionTaskNode.generated.h"

class UK2Node_CustomEvent;
class UK2Node_TemporaryVariable;
struct FTaskDataStructBase;
struct FBaseTaskData;
class UBasePredictionTask;
/**
 * 
 */
UCLASS(DisplayName = "Prediction Task")
class ABILITYSYSTEMSIMULATIONEDITOR_API UK2Node_PredictionTask : public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_PredictionTask(const FObjectInitializer& ObjectInitializer);

	virtual void ExpandNode(class FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph) override;
	virtual void AllocateDefaultPins() override;
	virtual bool IsCompatibleWithGraph(UEdGraph const* Graph) const override;
	virtual bool IsActionFilteredOut(class FBlueprintActionFilter const& Filter) override;
	//~ Begin UK2Node Interface.
	virtual bool ShouldShowNodeProperties() const override { return true; }
	virtual void ValidateNodeDuringCompilation(class FCompilerResultsLog& MessageLog) const override;
	virtual void GetMenuActions(FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;
	virtual bool HasExternalDependencies(TArray<class UStruct*>* OptionalOutput) const override;
	virtual FName GetCornerIcon() const override;
	virtual bool CanJumpToDefinition() const override;
	virtual void JumpToDefinition() const override;
	//Start of UEdGraphNode interface implementation
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual void DestroyNode() override;
	virtual void PostPasteNode() override;
	//End of implementation

	/*
	 * the node's instances of the runtime task, created here and kept in sync with ability class mirroring instance
	 */
	UPROPERTY()
	TObjectPtr<UBasePredictionTask> TaskInstance = nullptr;

	virtual bool NodeCausesStructuralBlueprintChange() const override {return true;};

protected:

	void AllocateFunctionPins(const bool& AllocateExecPin,UFunction* Function);

	UFunction* GetStartTaskFunction() const;
	FName GetStartTaskFunctionName() const;

	TArray<UFunction*> GetAdditionalInputFunctions() const;
	TArray<FName> GetAdditionalInputFunctionNames() const;

	struct ABILITYSYSTEMSIMULATIONEDITOR_API FBasePredictionTaskHelper
	{
		struct FOutputPinAndLocalVariable
		{
			UEdGraphPin* OutputPin;
			UK2Node_TemporaryVariable* TempVar;

			FOutputPinAndLocalVariable(UEdGraphPin* Pin, UK2Node_TemporaryVariable* Var) : OutputPin(Pin), TempVar(Var) {}
		};

		static bool ValidDataPin(const UEdGraphPin* Pin, EEdGraphPinDirection Direction);
		static bool CreateDelegateForNewFunction(UEdGraphPin* DelegateInputPin, FName FunctionName, UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);
		static bool CopyEventSignature(UK2Node_CustomEvent* CENode, UFunction* Function, const UEdGraphSchema_K2* Schema);
		static bool HandleDelegateImplementation(
			FMulticastDelegateProperty* CurrentProperty, const TArray<FBasePredictionTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs,
			UEdGraphPin* ProxyObjectPin, UEdGraphPin*& InOutLastThenPin, UEdGraphPin*& OutLastActivatedThenPin,
			UK2Node* CurrentNode, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);
	};

	virtual bool HandleDelegates(
	const TArray<FBasePredictionTaskHelper::FOutputPinAndLocalVariable>& VariableOutputs, UEdGraphPin* ProxyObjectPin,
	UEdGraphPin*& InOutLastThenPin, UEdGraph* SourceGraph, FKismetCompilerContext& CompilerContext);

	
};
