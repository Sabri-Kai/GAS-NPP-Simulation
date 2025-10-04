#pragma once

#include "CoreMinimal.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailCustomization.h"
#include "Abilities/NpGameplayAbility.h"
#include "Compilation/NpGameplayAbilityBlueprint.h"
#include "Modules/ModuleManager.h"
#include "Nodes/PredictionTaskNode.h"
#include "Tasks/BasePredictionTask.h"

class FAbilitySystemSimulationEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

// Create a Details Customization class
class FPredictionAbilityDetailsCustomization final : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FPredictionAbilityDetailsCustomization);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{

		// Hide Variable that are not relevant in the context of Net prediction ability system.
		
		TSharedRef<IPropertyHandle> bServerRespectsRemoteAbilityCancellationHandle = DetailBuilder.GetProperty("bServerRespectsRemoteAbilityCancellation",UGameplayAbility::StaticClass());
		if (bServerRespectsRemoteAbilityCancellationHandle->IsValidHandle())
		{
			// Hide the bServerRespectsRemoteAbilityCancellation property
			DetailBuilder.HideProperty(bServerRespectsRemoteAbilityCancellationHandle);
		}
		TSharedRef<IPropertyHandle> ReplicationPolicyHandle = DetailBuilder.GetProperty("ReplicationPolicy",UGameplayAbility::StaticClass());
		if (ReplicationPolicyHandle->IsValidHandle())
		{
			// Hide the ReplicationPolic property
			DetailBuilder.HideProperty(ReplicationPolicyHandle);
		}
		TSharedRef<IPropertyHandle> NetSecurityPolicyHandle = DetailBuilder.GetProperty("NetSecurityPolicy",UGameplayAbility::StaticClass());
		if (NetSecurityPolicyHandle->IsValidHandle())
		{
			// Hide the NetSecurityPolicy property
			DetailBuilder.HideProperty(NetSecurityPolicyHandle);
		}
		TSharedRef<IPropertyHandle> bReplicateInputDirectlyHandle = DetailBuilder.GetProperty("bReplicateInputDirectly",UGameplayAbility::StaticClass());
		if (bReplicateInputDirectlyHandle->IsValidHandle())
		{
			// Hide the bReplicateInputDirectly property
			DetailBuilder.HideProperty(bReplicateInputDirectlyHandle);
		}
	}
};

class FPredictionNodeNodeDetails : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShareable(new FPredictionNodeNodeDetails);
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override
	{
		 TArray<TWeakObjectPtr<UObject>> SelectedObjects;
        DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
        
        UK2Node_PredictionTask* CustomNode = nullptr;
        
        for (TWeakObjectPtr<UObject> Object : SelectedObjects)
        {
            if (UK2Node_PredictionTask* Node = Cast<UK2Node_PredictionTask>(Object.Get()))
            {
                CustomNode = Node;
                break;
            }
        }
        
        if (!CustomNode)
            return;
            
        // Find the corresponding task
        UBasePredictionTask* CorrespondingTask = FindTaskForNode(CustomNode);
        if (!CorrespondingTask)
            return;
            
        // Create a category for the Task properties
        IDetailCategoryBuilder& TaskCategory = DetailBuilder.EditCategory("Task Properties", FText::FromString("Task Properties"), ECategoryPriority::Important);
        
        // This is the key part - use the details view itself to display the task object's properties
        FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		// Create details view args
		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.bHideSelectionTip = true;
        
		// Create the details view
		TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
        
		// Set the object to show
		DetailsView->SetObject(CorrespondingTask);
        
		// Add the details view to the task category
		FDetailWidgetRow& Row = TaskCategory.AddCustomRow(FText::FromString("Task Data"))
			.WholeRowContent()
			[
				SNew(SBox)
				.MinDesiredHeight(300)
				[
					DetailsView
				]
			];
	}

	static UBasePredictionTask* FindTaskForNode(UK2Node_PredictionTask* Node)
	{
		if (!Node || !Node->GetBlueprint())
		{
			return nullptr;
		}
		UNpGameplayAbilityBlueprint* NpBP = Cast<UNpGameplayAbilityBlueprint>(Node->GetBlueprint());
		if (NpBP->GeneratedClass)
		{
			if (UNpGameplayAbility* AbilityCDO = Cast<UNpGameplayAbility>(NpBP->GeneratedClass.GetDefaultObject()))
			{
				if (Node->TaskInstance)
				{
					if (UBasePredictionTask* TaskInClass = AbilityCDO->GetTaskFromGuid(Node->TaskInstance->TaskId))
					{
						return TaskInClass;
					}
				}
			}
		}
		return Node->TaskInstance;
	}
};
