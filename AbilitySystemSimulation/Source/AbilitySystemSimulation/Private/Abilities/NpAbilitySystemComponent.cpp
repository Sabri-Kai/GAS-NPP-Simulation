// Fill out your copyright notice in the Description page of Project Settings.


#include "Abilities/NpAbilitySystemComponent.h"

#include "AbilitySimulationSettings.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemLog.h"
#include "AbilitySystemStats.h"
#include "GameplayCueManager.h"
#include "GameplayTagResponseTable.h"
#include "MoverComponent.h"
#include "MoverDataModelTypes.h"
#include "MoverTypes.h"

#include "NetworkPredictionModelDef.h"
#include "NetworkPredictionProxyInit.h"
#include "NetworkPredictionProxyWrite.h"
#include "NetworkPredictionModelDefRegistry.h"
#include "NetworkPredictionBuffer.h"
#include "Abilities/NpGameplayAbility.h"
#include "DataTypes/EffectsDataTypes.h"
#include "NetworkPredictionWorldManager.h"
#include "MontageSimulator/NetMontageSimulator.h"
#include "Net/UnrealNetwork.h"
#include "ProjectilesSimulator/SyncedProjectileBase.h"


#define LOCTEXT_NAMESPACE "NpAbilitySystemComponent"

DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp ApplyGameplayEffectSpecToTarget"), STAT_AbilitySystemComp_ApplyGameplayEffectSpecToTarget, STATGROUP_AbilitySystem);
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp ApplyGameplayEffectSpecToSelf"), STAT_AbilitySystemComp_ApplyGameplayEffectSpecToSelf, STATGROUP_AbilitySystem); 
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp OnImmunityBlockGameplayEffect"), STAT_AbilitySystemComp_OnImmunityBlockGameplayEffect, STATGROUP_AbilitySystem);
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp InvokeGameplayCueEvent"), STAT_AbilitySystemComp_InvokeGameplayCueEvent, STATGROUP_AbilitySystem);
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp OnGameplayEffectAppliedToSelf"), STAT_AbilitySystemComp_OnGameplayEffectAppliedToSelf, STATGROUP_AbilitySystem);
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp OnGameplayEffectAppliedToTarget"), STAT_AbilitySystemComp_OnGameplayEffectAppliedToTarget, STATGROUP_AbilitySystem);
DECLARE_CYCLE_STAT(TEXT("AbilitySystemComp ExecuteGameplayEffect"), STAT_AbilitySystemComp_ExecuteGameplayEffect, STATGROUP_AbilitySystem);

#pragma region Network prediction Model Definition

class FAbilitySystemModelDef : public FNetworkPredictionModelDef
{
public:

	NP_MODEL_BODY();

	using Simulation = UNpAbilitySystemComponent;
	using StateTypes = AbilitySystemStateTypes;
	using Driver = UNpAbilitySystemComponent;

	static const TCHAR* GetName() { return TEXT("AbilitySystemSimulation"); }
	static constexpr int32 GetSortPriority() { return (int32)ENetworkPredictionSortPriority::PreKinematicMovers - 2; }
};

NP_MODEL_REGISTER(FAbilitySystemModelDef);
#pragma endregion

#pragma region Component Default Interface
UNpAbilitySystemComponent::UNpAbilitySystemComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bWantsInitializeComponent = true;

	PrimaryComponentTick.bStartWithTickEnabled = true; 
	bAutoActivate = true;	

	SetIsReplicatedByDefault(true);
	
	bCachedIsNetSimulated = false;
	UserAbilityActivationInhibited = false;

	GenericConfirmInputID = INDEX_NONE;
	GenericCancelInputID = INDEX_NONE;

	bSuppressGrantAbility = false;
	bSuppressGameplayCues = false;
	bPendingMontageRep = false;
	AffectedAnimInstanceTag = NAME_None; 

	AbilityScopeLockCount = 0;
	bAbilityPendingClearAll = false;
	AbilityLastActivatedTime = 0.f;

	// must use minimal replication mode for cues to work correctly!!!
	// in any case that is the only thing the original GAS is still replicating.
	ReplicationMode = EGameplayEffectReplicationMode::Minimal;

	ClientActivateAbilityFailedStartTime = 0.f;
	ClientActivateAbilityFailedCountRecent = 0;
	
	
}
void UNpAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	SetIsReplicated(true);
	if (UNetworkPredictionWorldManager* NetworkPredictionWorldManager = GetWorld()->GetSubsystem<UNetworkPredictionWorldManager>())
	{
		CachedManager = NetworkPredictionWorldManager;
		// Init RepProxies
		ReplicationProxy_ServerRPC.Init(&NetworkPredictionProxy, EReplicationProxyTarget::ServerRPC);
		ReplicationProxy_Autonomous.Init(&NetworkPredictionProxy, EReplicationProxyTarget::AutonomousProxy);
		ReplicationProxy_Simulated.Init(&NetworkPredictionProxy, EReplicationProxyTarget::SimulatedProxy);
		ReplicationProxy_Replay.Init(&NetworkPredictionProxy, EReplicationProxyTarget::Replay);

		InitializeNetworkPredictionProxy();

		CheckOwnerRoleChange();
	}

	MontagePlayer = NewObject<UNetMontageSimulator>(this, TEXT("MontageSimulator"), RF_Transient);
	MontagePlayer->AbilitySystemComponent = this;

	ProjectilesSimulator =  NewObject<UProjectilesSimulator>(this, TEXT("ProjectilesSimulator"), RF_Transient);
	
	LoadInputsInMemory();
}
void UNpAbilitySystemComponent::UninitializeComponent()
{
	Super::UninitializeComponent();

	if(MontagePlayer)
	{
		MontagePlayer->MarkAsGarbage();
	}

	if(ProjectilesSimulator)
	{
		ProjectilesSimulator->MarkAsGarbage();
	}
}
void UNpAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();
}
void UNpAbilitySystemComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	Super::EndPlay(Reason);
	NetworkPredictionProxy.EndPlay();
}

bool UNpAbilitySystemComponent::GetShouldTick() const
{
	return false;
}
void UNpAbilitySystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
	// don't call parent tick.
}
#pragma endregion

#pragma region Synced Montage Playback
UAnimMontage* UNpAbilitySystemComponent::GetPlayingSyncedMontage(float& CurrentTime) const
{
	if (!MontagePlayer)
	{
		CurrentTime = 0.f;
		return nullptr;
	}
	return MontagePlayer->GetPlayingMontage(CurrentTime);
}

ENetRole UNpAbilitySystemComponent::GetCachedSimNetRole() const
{
	return NetworkPredictionProxy.GetCachedNetRole();
}

void UNpAbilitySystemComponent::PlaySyncedMontage(UAnimMontage* AnimMontage, float StartTime, float PlayRate, FName SectionName, float InRootMotionScale)
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->PlayMontage(FAbilityMontagePlayback(AnimMontage,StartTime,PlayRate,SectionName,InRootMotionScale));
}

void UNpAbilitySystemComponent::StopSyncedMontage(UAnimMontage* Montage, bool bInterrupted/* = false*/)
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->StopMontage(FAbilityMontageCancel(Montage), bInterrupted);
}

void UNpAbilitySystemComponent::PauseCurrentSyncedMontage()
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->PauseCurrentMontage();
}

void UNpAbilitySystemComponent::ResumeCurrentSyncedMontage()
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->ResumeCurrentMontage();
}

void UNpAbilitySystemComponent::JumpSyncedMontageToSection(UAnimMontage* Montage, FName SectionName)
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->MontageJumpToSection(Montage,SectionName);
}

void UNpAbilitySystemComponent::SetCurrentMontagePlayRate(float PlayRate)
{
	if (!MontagePlayer)
	{
		return;
	}
	MontagePlayer->SetMontagePlayRate(PlayRate);
}
#pragma endregion

#pragma region Input Handling
bool UNpAbilitySystemComponent::TryActivateAbilitiesFromInput(const UInputAction* InputAction,const ETriggerEvent& TriggerEvent)
{
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (Spec.Ability)
		{
			UNpGameplayAbility* NpGameplayAbility = Cast<UNpGameplayAbility>(Spec.Ability);
			check(NpGameplayAbility)
			for (int32 i = 0 ; i < NpGameplayAbility->ActivationInputs.Num() ; ++i)
			{
				const FAbilityActivationTrigger& ActivationTrigger = NpGameplayAbility->ActivationInputs[i];
				if (ActivationTrigger.InputAction == InputAction && ActivationTrigger.TriggerEvent == TriggerEvent)
				{
					Spec.InputID = i;
					return TryActivateAbility(Spec.Handle,true);
				}
			}
		}
	}
	return false;
}
void UNpAbilitySystemComponent::HandleSimTickInputActionsEvents(const FAbilitySimInputCmd& InputCmd)
{
	if (InputCmd.InputActionStates.Num() > 0 && InputCmd.ActiveMappingContexts.Num() > 0)
	{
		TArray<const UInputAction*> InputActions = GetInputActionsFromMappingIndexes(InputCmd.ActiveMappingContexts);
		check(InputActions.Num() == InputCmd.InputActionStates.Num())
		// here we loop 3 times,
		// First to trigger callback for events except ongoing
		// Second to activate abilities , this means newly activated abilities won't get input events from the same frame
		// Except OnGoing which is expected to be every frame if true. so it makes sense to still trigger that frame if it is.
		// Third and final loop for the on going event. this should be ok, the loops are small
		// and third loop will be even smaller

		TArray<uint8> OngoingEventsIndexes;
		OngoingEventsIndexes.Reserve(InputCmd.InputActionStates.Num());
		for (uint8 i = 0; i < InputCmd.InputActionStates.Num(); i++)
		{
			const FAbilityInputActionState& ActionState = InputCmd.InputActionStates[i];
			const UInputAction* Action = InputActions[i];
			if (ActionState.bStarted)
			{
				OnInputActionEvent.Broadcast(Action,ETriggerEvent::Started);
			}
			if (ActionState.bTriggered)
			{
				OnInputActionEvent.Broadcast(Action,ETriggerEvent::Triggered);
			}
			if (ActionState.bOngoing)
			{
				OngoingEventsIndexes.Add(i);
			}
			if (ActionState.bCanceled)
			{
				OnInputActionEvent.Broadcast(Action,ETriggerEvent::Canceled);
			}
			if (ActionState.bCompleted)
			{
				OnInputActionEvent.Broadcast(Action,ETriggerEvent::Completed);
			}
		}
		
		for (uint8 i = 0; i < InputCmd.InputActionStates.Num(); i++)
		{
			const FAbilityInputActionState& ActionState = InputCmd.InputActionStates[i];
			const UInputAction* Action = InputActions[i];
			if (ActionState.bStarted)
			{
				TryActivateAbilitiesFromInput(Action,ETriggerEvent::Started);
			}
			if (ActionState.bTriggered)
			{
				TryActivateAbilitiesFromInput(Action,ETriggerEvent::Triggered);
			}
			if (ActionState.bOngoing)
			{
				TryActivateAbilitiesFromInput(Action,ETriggerEvent::Ongoing);
			}
			if (ActionState.bCanceled)
			{
				TryActivateAbilitiesFromInput(Action,ETriggerEvent::Canceled);
			}
			if (ActionState.bCompleted)
			{
				TryActivateAbilitiesFromInput(Action,ETriggerEvent::Completed);
			}
		}

		for (uint8 OngoingIndex : OngoingEventsIndexes)
		{
			const UInputAction* Action = InputActions[OngoingIndex];
			OnInputActionEvent.Broadcast(Action,ETriggerEvent::Ongoing);
		}
	}
}
void UNpAbilitySystemComponent::AddMappingContext(const UInputMappingContext* MappingContext)
{
	if (!MappingContext)
	{
		return;
	}
	const UAbilitySimulationSettings* AbilitySimSettings = UAbilitySimulationSettings::Get();
	const int32 IndexFound = AbilitySimSettings->AbilitySystemMappingContexts.Find(MappingContext);
	if (IndexFound == INDEX_NONE)
	{
		UE_LOG(LogAbilitySystem,Error,TEXT("Trying To Add Mapping Context That Is Not Added To AbilitySimulationSettings %s , "
									 "Please Add To AbilitySimulationSettings Or Inputs in it will not be considered"),*GetNameSafe(MappingContext));
		return;
	}
	if (!LocalActiveMappingContexts.Contains(IndexFound))
	{
		LocalActiveMappingContexts.Add(IndexFound);
		UpdateActiveInputActions();
		if (!BindInputActionsFromContext(MappingContext))
		{
			UE_LOG(LogAbilitySystem,Error,TEXT("Added Mapping Context %s but couldn't bind its inputs"),*GetNameSafe(MappingContext))
		}
	}
	
}
void UNpAbilitySystemComponent::RemoveMappingContext(const UInputMappingContext* MappingContext)
{
	const UAbilitySimulationSettings* AbilitySimSettings = UAbilitySimulationSettings::Get();
	const int32 IndexFound = AbilitySimSettings->AbilitySystemMappingContexts.Find(MappingContext);
	if (IndexFound == INDEX_NONE)
	{
		UE_LOG(LogAbilitySystem,Error,TEXT("Trying To Remove Mapping Context That Is Not Added To AbilitySimulationSettings %s , "
									 "Please Add to AbilitySimulationSettings Or Inputs in it will not be considered"),*GetNameSafe(MappingContext));
		return;
	}
	if (LocalActiveMappingContexts.Contains(IndexFound))
	{
		LocalActiveMappingContexts.Remove(IndexFound);
		UpdateActiveInputActions();
	}
}
void UNpAbilitySystemComponent::UpdateActiveInputActions()
{
	ActiveInputActions = GetInputActionsFromMappingIndexes(LocalActiveMappingContexts);
	// Update Input Action States And be Sure to keep existing states.
	TMap<const UInputAction*,FAbilityInputActionState> NewInputsStates;
	NewInputsStates.Reserve(ActiveInputActions.Num());
	for (const UInputAction* InputAction : ActiveInputActions)
	{
		if (const FAbilityInputActionState* ExitingState = LocalInputActionStates.Find(InputAction))
		{
			NewInputsStates.Add(InputAction,*ExitingState);
		}
		else
		{
			NewInputsStates.Add(InputAction);
		}
	}
	LocalInputActionStates = NewInputsStates;
}
void UNpAbilitySystemComponent::UpdateInputActionState(const FInputActionInstance& ActionInstance)
{
	const UInputAction* InputAction = ActionInstance.GetSourceAction();
	const ETriggerEvent& TriggerEvent = ActionInstance.GetTriggerEvent();
	if (FAbilityInputActionState* ExitingState = LocalInputActionStates.Find(InputAction))
	{
		switch (TriggerEvent)
		{
		case ETriggerEvent::Started:
			{
				ExitingState->bStarted = true;
				break;
			}
		case ETriggerEvent::Triggered:
			{
				ExitingState->bTriggered = true;
				break;
			}
		case ETriggerEvent::Ongoing:
			{
				ExitingState->bOngoing = true;
				break;
			}
		case ETriggerEvent::Canceled:
			{
				ExitingState->bCanceled = true;
				ExitingState->bOngoing = false;
				break;
			}
		case ETriggerEvent::Completed:
			{
				ExitingState->bCompleted = true;
				ExitingState->bOngoing = false;
				break;
			}
		case ETriggerEvent::None:
			{
				ExitingState->bOngoing = false;
				break;
			}
		}
	}
}
void UNpAbilitySystemComponent::SimulateInputTrigger(UInputAction* InputAction, ETriggerEvent TriggerEvent)
{
	if (!AbilityActorInfo || !GetAvatarActor())
	{
		return;
	}

	// Can't use IsLocallyControlledPlayer because internally, that's casting to PlayerController which will always fail for AI.
	// We want AI to have the ability to behave just like a player if we want it to. for example using a Neural Network for the AI
	// makes more sense for it to learn from players and have same outputs as a player would. which translates to move input, button presses etc..
	if (!AbilityActorInfo->IsLocallyControlled())
	{
		return;
	}

	if (FAbilityInputActionState* ExitingState = LocalInputActionStates.Find(InputAction))
	{
		ExitingState->Reset();
		switch (TriggerEvent)
		{
		case ETriggerEvent::Started:
			{
				ExitingState->bStarted = true;
				break;
			}
		case ETriggerEvent::Triggered:
			{
				ExitingState->bTriggered = true;
				break;
			}
		case ETriggerEvent::Ongoing:
			{
				ExitingState->bOngoing = true;
				break;
			}
		case ETriggerEvent::Canceled:
			{
				ExitingState->bCanceled = true;
				ExitingState->bOngoing = false;
				break;
			}
		case ETriggerEvent::Completed:
			{
				ExitingState->bCompleted = true;
				ExitingState->bOngoing = false;
				break;
			}
		case ETriggerEvent::None:
			{
				ExitingState->bOngoing = false;
				break;
			}
		}
	}
}

FAbilityInputActionState UNpAbilitySystemComponent::GetSyncedInputActionState(const UInputAction* InputAction) const
{
	FAbilityInputActionState OutState;
	const FAbilitySimInputCmd* LatestCmd = NetworkPredictionProxy.ReadInputCmd<FAbilitySimInputCmd>();
	if (LatestCmd)
	{
		TArray<const UInputAction*> InputActions = GetInputActionsFromMappingIndexes(LatestCmd->ActiveMappingContexts);
		// get index of this input action in the array
		int32 Index = InputActions.Find(InputAction);
		if (Index != INDEX_NONE)
		{
			OutState = LatestCmd->InputActionStates[Index];
		}
	}
	return OutState;
}

FInstancedStruct UNpAbilitySystemComponent::GetSyncedCustomInput() const
{
	const FAbilitySimInputCmd* LatestCmd = NetworkPredictionProxy.ReadInputCmd<FAbilitySimInputCmd>();
	if (LatestCmd && LatestCmd->CustomInput.IsValid())
	{
		return LatestCmd->CustomInput;
	}
	FInstancedStruct DefaultStruct;
	if (CustomInput.IsValid())
	{
		DefaultStruct.InitializeAs(CustomInput.GetScriptStruct());
	}
	return DefaultStruct;
}

FInstancedStruct UNpAbilitySystemComponent::GetCustomInputFromCommand(const FAbilitySimInputCmd& InputCmd) const
{
	if (InputCmd.CustomInput.IsValid())
	{
		return InputCmd.CustomInput;
	}
	FInstancedStruct DefaultStruct;
	if (CustomInput.IsValid())
	{
		DefaultStruct.InitializeAs(CustomInput.GetScriptStruct());
	}
	return DefaultStruct;
}

FVector2D UNpAbilitySystemComponent::GetSyncedMouseScreenLocation() const
{
	const FAbilitySimInputCmd* LatestCmd = NetworkPredictionProxy.ReadInputCmd<FAbilitySimInputCmd>();
	if (LatestCmd)
	{
		return LatestCmd->MouseScreenLocation;
	}
	return FVector2D::ZeroVector;
}

FVector UNpAbilitySystemComponent::GetSyncedCameraWorldLocation() const
{
	const FAbilitySimInputCmd* LatestCmd = NetworkPredictionProxy.ReadInputCmd<FAbilitySimInputCmd>();
	if (LatestCmd)
	{
		return LatestCmd->CameraLocation;
	}
	return FVector::ZeroVector;
}

FSyncedScreenProjection UNpAbilitySystemComponent::GetSyncedScreenProjection() const
{
	const FAbilitySimInputCmd* LatestCmd = NetworkPredictionProxy.ReadInputCmd<FAbilitySimInputCmd>();
	if (LatestCmd)
	{
		return LatestCmd->ScreenProjectionData;
	}
	return FSyncedScreenProjection();
}

FRotator UNpAbilitySystemComponent::GetSyncedControlRotation() const
{
	if (GetAvatarActor())
	{
		UMoverComponent* MoverComp = GetAvatarActor()->GetComponentByClass<UMoverComponent>();
		if (MoverComp)
		{
			if (const FCharacterDefaultInputs* Inputs = MoverComp->GetLastInputCmd().InputCollection.FindDataByType<FCharacterDefaultInputs>())
			{
				return Inputs->ControlRotation;
			}
		}

		AController* Controller = AbilityActorInfo.Get()->PlayerController.Get();
		if (!Controller)
		{
			if (const APawn* OwnerPawn = Cast<APawn>(GetAvatarActor()))
			{
				Controller = OwnerPawn->GetController();
			}
		}
		if (Controller)
		{
			FVector ViewLoc = FVector::ZeroVector;
			FRotator ViewRot = FRotator::ZeroRotator;
			Controller->GetPlayerViewPoint(ViewLoc,ViewRot);
			return ViewRot;
		}
	}
	return FRotator::ZeroRotator;
}

void UNpAbilitySystemComponent::ClearOneShotInputStates()
{
	for (TPair<const UInputAction*,FAbilityInputActionState>& Pair : LocalInputActionStates)
	{
		Pair.Value.bStarted = false;
		Pair.Value.bTriggered = false;
		Pair.Value.bCanceled = false;
		Pair.Value.bCompleted = false;
	}
}
bool UNpAbilitySystemComponent::BindInputActionsFromContext(const UInputMappingContext* InputContext)
{
	if (!AbilityActorInfo->AvatarActor.IsValid())
	{
		return false;
	}
	APawn* AvatarAsPawn = Cast<APawn>(AbilityActorInfo->AvatarActor.Get());
	if (!AvatarAsPawn || !AvatarAsPawn->IsLocallyControlled())
	{
	    return false;
	}
	APlayerController* AvatarPlayerController = Cast<APlayerController>(AvatarAsPawn->GetController());
	if (!IsValid(AvatarPlayerController))
	{
		return false;
	}
	if(!IsValid(AvatarPlayerController->InputComponent))
	{
		return false;
	}
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(AvatarPlayerController->InputComponent);
	
	for (const FEnhancedActionKeyMapping& ActionKeyMapping : InputContext->GetMappings())
	{
		EnhancedInput->BindAction(ActionKeyMapping.Action,ETriggerEvent::Started,this,&UNpAbilitySystemComponent::UpdateInputActionState);
		EnhancedInput->BindAction(ActionKeyMapping.Action,ETriggerEvent::Triggered,this,&UNpAbilitySystemComponent::UpdateInputActionState);
		EnhancedInput->BindAction(ActionKeyMapping.Action,ETriggerEvent::Ongoing,this,&UNpAbilitySystemComponent::UpdateInputActionState);
		EnhancedInput->BindAction(ActionKeyMapping.Action,ETriggerEvent::Canceled,this,&UNpAbilitySystemComponent::UpdateInputActionState);
		EnhancedInput->BindAction(ActionKeyMapping.Action,ETriggerEvent::Completed,this,&UNpAbilitySystemComponent::UpdateInputActionState);
	}
	return true;
}
void UNpAbilitySystemComponent::LoadInputsInMemory()
{
	const UAbilitySimulationSettings* AbilitySimSettings = UAbilitySimulationSettings::Get();
	if (AbilitySimSettings && AbilitySimSettings->AbilitySystemMappingContexts.Num())
	{
		LoadedMappings.Empty();
		for (const TSoftObjectPtr<const UInputMappingContext>& Mapping : AbilitySimSettings->AbilitySystemMappingContexts)
		{
			LoadedMappings.Add(Mapping.LoadSynchronous());
		}
	}
	
}
TArray<const UInputAction*> UNpAbilitySystemComponent::GetInputActionsFromMappingIndexes(
	const TArray<uint8>& MappingIndexes) const
{
	TArray<const UInputAction*> Actions;
	for (const uint8& MappingIndex : MappingIndexes)
	{
		if (LoadedMappings.IsValidIndex(MappingIndex))
		{
			const UInputMappingContext* Context = LoadedMappings[MappingIndex];
			for (const FEnhancedActionKeyMapping& ActionKeyMapping : Context->GetMappings())
			{
				Actions.AddUnique(ActionKeyMapping.Action);
			}
		}
	}
	return Actions;
}
const UInputAction* UNpAbilitySystemComponent::GetInputActionAtIndex(const TArray<uint8>& MappingIndexes,
	const uint8& Index) const
{
	TArray<const UInputAction*> Actions = GetInputActionsFromMappingIndexes(MappingIndexes);
	if(Actions.IsValidIndex(Index))
	{
		return Actions[Index];
	}
	return nullptr;
}

#pragma endregion

#pragma region Network Prediction API
void UNpAbilitySystemComponent::ProduceInput(const int32 DeltaTimeMS, FAbilitySimInputCmd* Cmd)
{
	if (!GetAvatarActor())
	{
		return;
	}
	OnPreProduceInput.Broadcast();
	Cmd->ActiveMappingContexts = LocalActiveMappingContexts;
	LocalInputActionStates.GenerateValueArray(Cmd->InputActionStates);
	Cmd->CustomInput = CustomInput;
	// Send Mouse Location and relative camera location
	AController* Controller = TryGetOwningController();
	APlayerController* PlayerController = nullptr;
	if (Controller)
	{
		if (bSendCameraLocation)
		{
			FRotator CameraRot;
			Controller->GetPlayerViewPoint(Cmd->CameraLocation,CameraRot);
		}
		else
		{
			Cmd->CameraLocation = FVector::ZeroVector;
		}
		PlayerController = Cast<APlayerController>(Controller);
	}
	if (PlayerController && bSendMouseScreenLocation)
	{
		PlayerController->GetMousePosition(Cmd->MouseScreenLocation.X, Cmd->MouseScreenLocation.Y);
	}
	else
	{
		Cmd->MouseScreenLocation = FVector2D::ZeroVector;
	}
	ClearOneShotInputStates();
	OnPostProduceInput.Broadcast();
}
void UNpAbilitySystemComponent::RestoreFrame(const FAbilitySimSyncState* SyncState, const FAbilitySimAuxState* AuxState)
{
	//When restoring Frame nothing is allowed to further change the state of ASC , this is important to have proper resimulation.
	// we're checking for this in a lot of function that might get called to effect the state,
	// such as giving/clearing or activating/deactivating an ability ,applying an effect or giving a tag
	// to not do anything while restoring a frame.
	// Improvement : diff ASC state before and after re-simulation and call events based on difference
	bIsRestoringFrame = true;
	const bool OldSuppressCues = bSuppressGameplayCues;
	bSuppressGameplayCues = true; // suppress cues during restoring frame, they will be restored themselves
	SyncedTarget = SyncState->SyncedTarget;

	ProjectilesSimulator->RestoreFrame(SyncState->ProjectilesCollection);
	MontagePlayer->RestoreFrame(SyncState->MontageSimulatorData);
	
	// Restore Handles Count
	SyncedAbilitiesHandlesCount = SyncState->ActivatableAbilitiesHandleCount;
	bSuppressGrantAbility = SyncState->bSuppressGrantAbility;
	UserAbilityActivationInhibited = SyncState->UserAbilityActivationInhibited;

	RestoreAttributeSets(SyncState->AttributeSets);
	RestoreGameplayEffects(SyncState->ActiveGameplayEffects);
	RestoreAbilities(SyncState->Abilities);
	RestoreTags(SyncState->BlockedAbilityTags,SyncState->GameplayTagCountContainer);
	RestoreCues(SyncState->SyncedCues);
	
	bIsRestoringFrame = false;
	bSuppressGameplayCues = OldSuppressCues;
}
void UNpAbilitySystemComponent::FinalizeFrame(const FAbilitySimSyncState* SyncState,const FAbilitySimAuxState* AuxState)
{
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_SimulatedProxy)
	{
		FinalizeSimulatedAttributes(SyncState->AttributeSets);
		FinalizeSimulatedTags(SyncState->BlockedAbilityTags,SyncState->GameplayTagCountContainer);
		if (AbilityActorInfo && GetAvatarActor())
		{
			//Finalize Montage
			MontagePlayer->FinalizeFrame(SyncState->MontageSimulatorData);
		}
		ProjectilesSimulator->FinalizeInterpolatedFrame(SyncState->ProjectilesCollection);
	}
	else
	{
		ProjectilesSimulator->FinalizeFrame(SyncState->ProjectilesCollection);
		
		bool HasSmoothing = false;
		if (CachedManager.IsValid(false))
		{
			HasSmoothing = CachedManager->GetSettings().bEnableFixedTickSmoothing;
		}
		if (!HasSmoothing)
		{
			if (AbilityActorInfo && GetAvatarActor())
			{
				//Finalize Montage
				MontagePlayer->FinalizeFrame(SyncState->MontageSimulatorData);
			}
		}
	}
	
	FinalizeCues(SyncState->SyncedCues);
}
void UNpAbilitySystemComponent::FinalizeSmoothingFrame(const FAbilitySimSyncState* SyncState,const FAbilitySimAuxState* AuxState)
{
	//Finalize Smoothed Montage for local player , this smoothes the montage playback
	if (AbilityActorInfo && GetAvatarActor())
	{
		//Finalize Montage
		MontagePlayer->FinalizeFrame(SyncState->MontageSimulatorData);
	}
	
}
void UNpAbilitySystemComponent::InitializeSimulationState(FAbilitySimSyncState* OutSync, FAbilitySimAuxState* OutAux)
{
	//This is the function that will initialize the ability system component data.
	for (int32 i=0; i < DefaultAbilities.Num(); ++i)
	{
		if (DefaultAbilities[i])
		{
			K2_GiveAbility(DefaultAbilities[i]);
		}
	}
	// Init starting data
	for (int32 i=0; i < DefaultAttributes.Num(); ++i)
	{
		TSubclassOf<UAttributeSet> AttributeClass = DefaultAttributes[i];
		if (AttributeClass)
		{
			GetOrCreateAttributeSubobject(AttributeClass);
		}
	}
	// init default effects
	for (int32 i=0; i < DefaultEffects.Num(); ++i)
	{
		if (DefaultEffects[i])
		{
			ApplyGameplayEffectToSelf(DefaultEffects[i].GetDefaultObject(),0,MakeEffectContext());
		}
	}
}

void UNpAbilitySystemComponent::InitializeNetworkPredictionProxy()
{
	NetworkPredictionProxy.Init<FAbilitySystemModelDef>(GetWorld(), GetReplicationProxies(), this, this);
}

void UNpAbilitySystemComponent::InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection,UNetworkPredictionPlayerControllerComponent* RPCHandler)
{
	NetworkPredictionProxy.InitForNetworkRole(Role, bHasNetConnection,RPCHandler);
}

bool UNpAbilitySystemComponent::CheckOwnerRoleChange()
{
	ENetRole CurrentRole = ROLE_SimulatedProxy;
	if (APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		CurrentRole = PawnOwner->GetLocalRole();
	}
	// owner is not a pawn, try to find the role from an owning controller if it exists, otherwise this is sim proxy
	else 
	{
		if (AController* Controller = TryGetOwningController())
		{
			// Found the controller
			CurrentRole = Controller->GetLocalRole();
		}
	}
	const bool bHasNetConnection = GetOwner()->GetNetConnection() != nullptr;
	UNetworkPredictionPlayerControllerComponent* RPCHandler = NetworkPredictionProxy.GetCachedRPCHandler();
	if (CurrentRole != ROLE_SimulatedProxy && bHasNetConnection && !IsValid(NetworkPredictionProxy.GetCachedRPCHandler()))
	{
		if (GetOwner()->GetNetConnection()->OwningActor)
		{
			RPCHandler = GetOwner()->GetNetConnection()->OwningActor->GetComponentByClass<UNetworkPredictionPlayerControllerComponent>();
			if (!RPCHandler)
			{
				// Create and register a new component dynamically
				RPCHandler = NewObject<UNetworkPredictionPlayerControllerComponent>(GetOwner()->GetNetConnection()->OwningActor);

				if (RPCHandler)
				{
					RPCHandler->SetNetAddressable();
					RPCHandler->SetIsReplicated(true);
					RPCHandler->RegisterComponent();
					if (RPCHandler->bWantsInitializeComponent)
					{
						RPCHandler->InitializeComponent();
					}
					RPCHandler->Activate(true);
				}
			}
		}
	}
	
	if (CurrentRole != NetworkPredictionProxy.GetCachedNetRole() || bHasNetConnection != NetworkPredictionProxy.GetCachedHasNetConnection()
		|| RPCHandler != NetworkPredictionProxy.GetCachedRPCHandler())
	{
		InitializeForNetworkRole(CurrentRole, bHasNetConnection,RPCHandler);
		return true;
	}

	return false;
}

void UNpAbilitySystemComponent::SimulationTick(const FNetSimTimeStep& TimeStep,
                                               const TNetSimInput<AbilitySystemStateTypes>& SimInput, const TNetSimOutput<AbilitySystemStateTypes>& SimOutput)
{
	FAbilitySystemTickStartData StartData;
	FAbilitySystemTickEndData EndData;

	StartData.InputCmd  = *SimInput.Cmd;
	StartData.SyncState = *SimInput.Sync;
	StartData.AuxState  = *SimInput.Aux;

	InternalSimulationTick(FAbilitySystemTimeStep(TimeStep), StartData, OUT EndData);

	*SimOutput.Sync = EndData.SyncState;
	*SimOutput.Aux.Get() = EndData.AuxState;
}

void UNpAbilitySystemComponent::CallServerRPC()
{
	//Doesn't Do anything , doesn't even get called 
}

int32 UNpAbilitySystemComponent::GetCurrentSimFrame()
{
	return NetworkPredictionProxy.GetPendingFrame();
}

bool UNpAbilitySystemComponent::ReadPendingSyncState(OUT FAbilitySimSyncState& OutSyncState)
{
	if (const FAbilitySimSyncState* PendingSyncState = NetworkPredictionProxy.ReadSyncState<FAbilitySimSyncState>())
	{
		OutSyncState = *PendingSyncState;
		return true;
	}

	return false;
}

bool UNpAbilitySystemComponent::WritePendingSyncState(const FAbilitySimSyncState& SyncStateToWrite)
{
	NetworkPredictionProxy.WriteSyncState<FAbilitySimSyncState>([&SyncStateToWrite](FAbilitySimSyncState& PendingSyncStateRef)
		{
			PendingSyncStateRef = SyncStateToWrite;
		});

	return true;
}

bool UNpAbilitySystemComponent::ReadPendingInputCmd(FAbilitySimInputCmd& OutInputCmd)
{
	if (const FAbilitySimInputCmd* PendingInputCmd = NetworkPredictionProxy.ReadSyncState<FAbilitySimInputCmd>())
	{
		OutInputCmd = *PendingInputCmd;
		return true;
	}

	return false;
}



float UNpAbilitySystemComponent::GetFixedStepMs() const
{
	if (CachedManager.IsValid(false))
	{
		return CachedManager->GetFixedTickState().FixedStepMS;
	}
	return CurrentCachedTimeStep.StepMs;
}

float UNpAbilitySystemComponent::GetSyncedInterpolationTimeMS() const
{
	return NetworkPredictionProxy.GetFixedInterpolationTime();
}

float UNpAbilitySystemComponent::GetCurrentSimulationTimeMS() const
{
	return NetworkPredictionProxy.GetTotalSimTimeMS();;
}

void UNpAbilitySystemComponent::InternalSimulationTick(const FAbilitySystemTimeStep& TimeStep,const FAbilitySystemTickStartData& TickStartData
                                                       , FAbilitySystemTickEndData& TickEndData)
{
	
	CurrentCachedTimeStep = TimeStep;
	if (TimeStep.ServerFrame > LatestCachedTimeStep.ServerFrame || TimeStep.BaseSimTimeMs > LatestCachedTimeStep.BaseSimTimeMs)
	{
		LatestCachedTimeStep = TimeStep;
	}
	//Send Input Events
	HandleSimTickInputActionsEvents(TickStartData.InputCmd);

	// Simulation Tick For Abilities Which Will tick Tasks
	TickAbilities(TimeStep);
	//ToDo @Kai : Add stat counter for this.
	//Tick Active Gameplay Effects.
	const uint32 BaseSimTimeMS = FMath::FloorToInt32(TimeStep.BaseSimTimeMs);
	const uint32 StepMs = FMath::FloorToInt32(TimeStep.StepMs);
	ActiveGameplayEffects.TickActiveEffects(BaseSimTimeMS,StepMs);
	//ToDo @Kai : Need to tick attribute sets that want to
	//Tick Montage PLayer
	MontagePlayer->SimulationTick(TimeStep,TickStartData.SyncState.MontageSimulatorData,TickEndData.SyncState.MontageSimulatorData);
	ProjectilesSimulator->SimulationTick(TimeStep,TickStartData.SyncState.ProjectilesCollection,TickEndData.SyncState.ProjectilesCollection);

	// In The End Fill The Sync State From The Current Ability System Variables.
	FillSyncState(TickEndData.SyncState);
}

void UNpAbilitySystemComponent::FillSyncState(FAbilitySimSyncState& SyncState)
{
	SyncState.SyncedTarget = SyncedTarget;
	SyncState.bSuppressGrantAbility = bSuppressGrantAbility;
	SyncState.UserAbilityActivationInhibited = UserAbilityActivationInhibited;
	SyncState.BlockedAbilityTags.FillFromGameplayTagCountContainer(BlockedAbilityTags);
	SyncState.GameplayTagCountContainer.FillFromGameplayTagCountContainer(GameplayTagCountContainer);
	SyncState.GameplayTagCountContainer.RemoveTags(NonReplicatedTags);
	SyncState.ActivatableAbilitiesHandleCount = SyncedAbilitiesHandlesCount;
	SyncState.Abilities.FillFromActivatableAbilities(ActivatableAbilities);
	SyncState.ActiveGameplayEffects = FActiveEffectSyncDataContainer(ActiveGameplayEffects);
	SyncState.AttributeSets = FAttributeSetSyncDataCollection(SpawnedAttributes);
	SyncState.SyncedCues = FActiveCueSyncDataContainer(ActiveGameplayCues);
}

void UNpAbilitySystemComponent::FinalizeSimulatedTags(const FSyncedGameplayTagCount& InBlockedAbilityTags,const FSyncedGameplayTagCount& InGameplayTags)
{
	RestoreTags(InBlockedAbilityTags,InGameplayTags);
}


#pragma endregion

#pragma region Replication
bool UNpAbilitySystemComponent::IsOwnerActorAuthoritative() const
{
	// Because this function is used in multiple places in the ASC code to allow for certain things to happen only on server
	// which is not something we need to do with NPP and ASC implementation with it.
	// both auto proxy and server have the authority to do anything
	// proxy can predict anything but server has final authority and will correct client to the right state, always.
	return NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority || NetworkPredictionProxy.GetCachedNetRole() == ROLE_AutonomousProxy;
}

void UNpAbilitySystemComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	// Skip Parent Ability System Component replicated props, intentionally not calling super
	DISABLE_ALL_CLASS_REPLICATED_PROPERTIES_FAST(UGameplayTasksComponent, EFieldIteratorFlags::ExcludeSuper);
	DISABLE_ALL_CLASS_REPLICATED_PROPERTIES_FAST(UAbilitySystemComponent, EFieldIteratorFlags::ExcludeSuper);
	UActorComponent::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Base ASC replication we want to keep
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;
	Params.Condition = COND_None;
	// keep Avatar and owner Replication outside network prediction to keep it in sync with construction and destruction replication
	// and not interpolated.
	DOREPLIFETIME_WITH_PARAMS_FAST(UAbilitySystemComponent, OwnerActor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UAbilitySystemComponent, AvatarActor, Params);

	Params.Condition = COND_ReplayOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UAbilitySystemComponent, ClientDebugStrings, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UAbilitySystemComponent, ServerDebugStrings, Params);
	
	// Network prediction Replication
	DOREPLIFETIME( UNpAbilitySystemComponent, NetworkPredictionProxy);
	DOREPLIFETIME_CONDITION( UNpAbilitySystemComponent, ReplicationProxy_Autonomous, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION( UNpAbilitySystemComponent, ReplicationProxy_Simulated, COND_SkipOwner);
	DOREPLIFETIME_CONDITION( UNpAbilitySystemComponent, ReplicationProxy_Replay, COND_ReplayOnly);
	
}

bool UNpAbilitySystemComponent::ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch,
	FReplicationFlags* RepFlags)
{
	// Skip Parent Ability System Component replicated Subobjects
	return UActorComponent::ReplicateSubobjects(Channel, Bunch, RepFlags);
}

void UNpAbilitySystemComponent::PreNetReceive()
{
	UActorComponent::PreNetReceive();
	CheckOwnerRoleChange();
}

void UNpAbilitySystemComponent::PostNetReceive()
{
	UActorComponent::PostNetReceive();
}

void UNpAbilitySystemComponent::ReadyForReplication()
{
	UActorComponent::ReadyForReplication();
}

bool UNpAbilitySystemComponent::ShouldRecordMontageReplication() const
{
	return false;
}

void UNpAbilitySystemComponent::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);
	CheckOwnerRoleChange();

	// We have to update our replication proxies so they can be accurately compared against client shadowstate during property replication. ServerRPC proxy does not need to do this.
	ReplicationProxy_Autonomous.OnPreReplication();
	ReplicationProxy_Simulated.OnPreReplication();
	ReplicationProxy_Replay.OnPreReplication();

	if (UNetConnection* Conn = GetOwner()->GetNetConnection())
	{
		if (Conn->IsReplay())
		{
			// This override disables replication for this property for this connection,
			// even if Super activated it earlier
			DOREPLIFETIME_ACTIVE_OVERRIDE(UNpAbilitySystemComponent, ReplicationProxy_Simulated, false);
		}
	}
}
#pragma endregion

#pragma region Attributes

void UNpAbilitySystemComponent::SetSpawnedAttributes(const TArray<UAttributeSet*>& NewSpawnedAttributes)
{
	for (UAttributeSet* AttributeSet : SpawnedAttributes)
	{
		if (AttributeSet)
		{
			AActor* ActorOwner = AttributeSet->GetTypedOuter<AActor>();
			if (ActorOwner)
			{
				ActorOwner->OnEndPlay.RemoveDynamic(this, &UNpAbilitySystemComponent::OnSpawnedAttributesEndPlayed);
			}
		}
	}

	// Clean the previous list
	RemoveAllSpawnedAttributes();

	// Add the elements from the new list
	for (UAttributeSet* NewAttribute : NewSpawnedAttributes)
	{
		if (NewAttribute)
		{
			AddSpawnedAttribute(NewAttribute);

			AActor* ActorOwner = NewAttribute->GetTypedOuter<AActor>();
			if (ActorOwner)
			{
				ActorOwner->OnEndPlay.AddUniqueDynamic(this, &UAbilitySystemComponent::OnSpawnedAttributesEndPlayed);
			}
		}
	}
}

void UNpAbilitySystemComponent::AddSpawnedAttribute(UAttributeSet* Attribute)
{
	if (!IsValid(Attribute))
	{
		return;
	}

	if (SpawnedAttributes.Find(Attribute) == INDEX_NONE)
	{
		SpawnedAttributes.Add(Attribute);
	}
}

void UNpAbilitySystemComponent::RemoveSpawnedAttribute(UAttributeSet* AttributeSet)
{
	if (SpawnedAttributes.RemoveSingle(AttributeSet) > 0)
	{
		
		TArray<FGameplayAttribute> Attributes;
		UAttributeSet::GetAttributesFromSetClass(AttributeSet->GetClass(), Attributes);
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			ABILITY_LOG(Log, TEXT("Cleaning up aggregator for attribute '%s' due to RemoveSpawnedAttribute removing attribute set '%s'"), *Attribute.GetName(), *AttributeSet->GetName());
			ActiveGameplayEffects.CleanupAttributeAggregator(Attribute);
		}
	}
}

void UNpAbilitySystemComponent::RemoveAllSpawnedAttributes()
{
	SpawnedAttributes.Empty();
}

void UNpAbilitySystemComponent::OnSpawnedAttributesEndPlayed(AActor* InActor, EEndPlayReason::Type EndPlayReason)
{
	for (int32 Index = SpawnedAttributes.Num() - 1; Index >= 0; --Index)
	{
		UAttributeSet* AttributeSet = SpawnedAttributes[Index];
		if (AttributeSet && AttributeSet->GetTypedOuter<AActor>() == InActor)
		{
			SpawnedAttributes[Index] = nullptr;
		}
	}
}

void UNpAbilitySystemComponent::ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude)
{
	ActiveGameplayEffects.ApplyModToAttribute(Attribute, ModifierOp, ModifierMagnitude);
}

UAttributeSet* UNpAbilitySystemComponent::GetOrCreateAttributeSubobject_Mutable(
	TSubclassOf<UAttributeSet> AttributeClass)
{
	AActor* OwningActor = GetOwner();
	UAttributeSet* MyAttributes = nullptr;
	if (OwningActor && AttributeClass)
	{
		MyAttributes = GetAttributeSubobject_Mutable(AttributeClass);
		if (!MyAttributes)
		{
			UAttributeSet* Attributes = NewObject<UAttributeSet>(OwningActor, AttributeClass);
			AddSpawnedAttribute(Attributes);
			MyAttributes = Attributes;
		}
	}

	return MyAttributes;
}

UAttributeSet* UNpAbilitySystemComponent::GetAttributeSubobject_Mutable(TSubclassOf<UAttributeSet> AttributeClass)
{
	for (UAttributeSet* Set : GetSpawnedAttributes())
	{
		if (Set && Set->IsA(AttributeClass))
		{
			return Set;
		}
	}
	return nullptr;
}

void UNpAbilitySystemComponent::FinalizeSimulatedAttributeSet(const FAttributeSetSyncData& AuthoritySet,
	UAttributeSet* AttributeSet)
{
	// sim proxies do not get base value. just current.
	TArray<FGameplayAttribute> Attributes;
	UAttributeSet::GetAttributesFromSetClass(AttributeSet->GetClass(), Attributes);
	for (int32 i = 0; i < Attributes.Num(); ++i)
	{
		const FGameplayAttribute& Attribute = Attributes[i];
		
		const float AuthorityCurrentValue = AuthoritySet.AttributeValues[i].GetCurrentValue();
		//This Broadcasts the events which can effect the value , we can't allow that so force set values after
		float NewValue = AuthorityCurrentValue;
		SetNumericAttribute_Internal(Attribute,NewValue);
		FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet);
		if (AttributeData)
		{
			AttributeData->SetBaseValue(AuthorityCurrentValue);
			AttributeData->SetCurrentValue(AuthorityCurrentValue);
		}
		else
		{
			Attribute.RestoreNumericValue(AuthorityCurrentValue,AttributeSet);
		}
	}
}

void UNpAbilitySystemComponent::FinalizeSimulatedAttributes(const FAttributeSetSyncDataCollection& AttributesData)
{
	TArray<UAttributeSet*> SetsToRemove;
	SetsToRemove.Reserve(SpawnedAttributes.Num());
	TSet<TSubclassOf<UAttributeSet>> AuthorityClasses;
	AuthorityClasses.Reserve(AttributesData.AttributeSetsData.Num());
	//Loop Through Server Sets and find or create attribute set for that class, then finalize its values
	for (int32 i = 0; i < AttributesData.AttributeSetsData.Num(); ++i)
	{
		UAttributeSet* Set = GetOrCreateAttributeSubobject_Mutable(AttributesData.AttributeSetsData[i].AttributeSetClass);
		FinalizeSimulatedAttributeSet(AttributesData.AttributeSetsData[i], Set);
		AuthorityClasses.Add(AttributesData.AttributeSetsData[i].AttributeSetClass);
	}

	// Loop Through Existing Attribute Sets , If Can't find one in authority add to pending remove
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		if (!AuthorityClasses.Contains(Set->GetClass()))
		{
			SetsToRemove.Add(Set);
		}
	}

	for (UAttributeSet* SetToRemove : SetsToRemove)
	{
		RemoveSpawnedAttribute(SetToRemove);
	}
}
#pragma endregion

#pragma region Gameplay Effects

FActiveGameplayEffectHandle UNpAbilitySystemComponent::ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpec &Spec, UAbilitySystemComponent *Target, FPredictionKey PredictionKey)
{
	if (GetIsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
	SCOPE_CYCLE_COUNTER(STAT_AbilitySystemComp_ApplyGameplayEffectSpecToTarget);
	FActiveGameplayEffectHandle ReturnHandle;
	if (Target)
	{
		ReturnHandle = Target->ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
	}
	return ReturnHandle;
}

FActiveGameplayEffectHandle UNpAbilitySystemComponent::ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec &Spec, FPredictionKey PredictionKey)
{
	if (GetIsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
#if WITH_SERVER_CODE
	SCOPE_CYCLE_COUNTER(STAT_AbilitySystemComp_ApplyGameplayEffectSpecToSelf);
#endif

	//Don't apply gameplay effect if we are sim proxy

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FGameplayAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == nullptr || !ActorInfo->OwnerActor.IsValid())
	{
		return FActiveGameplayEffectHandle();;
	}

	if (ActorInfo->AvatarActor.IsValid())
	{
		const ENetRole NetMode = ActorInfo->AvatarActor->GetLocalRole();

		// This should only come from button presses/local instigation (AI, etc).
		if (NetMode == ROLE_SimulatedProxy)
		{
			return FActiveGameplayEffectHandle();;
		}
	}
	
	
	// Scope lock the container after the addition has taken place to prevent the new effect from potentially getting mangled during the remainder
	// of the add operation
	FScopedActiveGameplayEffectLock ScopeLock(ActiveGameplayEffects);

	FScopeCurrentGameplayEffectBeingApplied ScopedGEApplication(&Spec, this);

	// Check if there is a registered "application" query that can block the application
	for (const FGameplayEffectApplicationQuery& ApplicationQuery : GameplayEffectApplicationQueries)
	{
		const bool bAllowed = ApplicationQuery.Execute(ActiveGameplayEffects, Spec);
		if (!bAllowed)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// check if the effect being applied actually succeeds
	if (!Spec.Def->CanApply(ActiveGameplayEffects, Spec))
	{
		return FActiveGameplayEffectHandle();
	}

	// Check AttributeSet requirements: make sure all attributes are valid
	// We may want to cache this off in some way to make the runtime check quicker.
	// We also need to handle things in the execution list
	for (const FGameplayModifierInfo& Mod : Spec.Def->Modifiers)
	{
		if (!Mod.Attribute.IsValid())
		{
			ABILITY_LOG(Warning, TEXT("%s has a null modifier attribute."), *Spec.Def->GetPathName());
			return FActiveGameplayEffectHandle();
		}
	}
	

	// Make sure we create our copy of the spec in the right place
	// We initialize the FActiveGameplayEffectHandle here with INDEX_NONE to handle the case of instant GE
	// Initializing it like this will set the bPassedFiltersAndWasExecuted on the FActiveGameplayEffectHandle to true so we can know that we applied a GE
	FActiveGameplayEffectHandle	MyHandle(INDEX_NONE);
	bool bInvokeGameplayCueApplied = Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant; // Cache this now before possibly modifying predictive instant effect to infinite duration effect.
	bool bFoundExistingStackableGE = false;

	FActiveGameplayEffect* AppliedEffect = nullptr;
	FGameplayEffectSpec* OurCopyOfSpec = nullptr;
	TUniquePtr<FGameplayEffectSpec> StackSpec;
	{
		if (Spec.Def->DurationPolicy != EGameplayEffectDurationType::Instant)
		{
			AppliedEffect = ActiveGameplayEffects.NpApplyGameplayEffectSpec(Spec, bFoundExistingStackableGE);
			if (!AppliedEffect)
			{
				return FActiveGameplayEffectHandle();
			}

			MyHandle = AppliedEffect->Handle;
			OurCopyOfSpec = &(AppliedEffect->Spec);

			// Log results of applied GE spec
			if (UE_LOG_ACTIVE(VLogAbilitySystem, Log))
			{
				UE_VLOG(GetOwnerActor(), VLogAbilitySystem, Log, TEXT("Applied %s"), *OurCopyOfSpec->Def->GetFName().ToString());

				for (const FGameplayModifierInfo& Modifier : Spec.Def->Modifiers)
				{
					float Magnitude = 0.f;
					Modifier.ModifierMagnitude.AttemptCalculateMagnitude(Spec, Magnitude);
					UE_VLOG(GetOwnerActor(), VLogAbilitySystem, Log, TEXT("         %s: %s %f"), *Modifier.Attribute.GetName(), *EGameplayModOpToString(Modifier.ModifierOp), Magnitude);
				}
			}
		}

		if (!OurCopyOfSpec)
		{
			StackSpec = MakeUnique<FGameplayEffectSpec>(Spec);
			OurCopyOfSpec = StackSpec.Get();

			UAbilitySystemGlobals::Get().GlobalPreGameplayEffectSpecApply(*OurCopyOfSpec, this);
			OurCopyOfSpec->CaptureAttributeDataFromTarget(this);
		}
	}

	// Update (not push) the global spec being applied [we want to switch it to our copy, from the const input copy)
	UAbilitySystemGlobals::Get().SetCurrentAppliedGE(OurCopyOfSpec);

	// UE5.4: We are following the same previous implementation that there is a special case for Gameplay Cues here (caveat: may not be true):
	// We are Stacking an existing Gameplay Effect.  That means the GameplayCues should already be Added/WhileActive and we do not have a proper
	// way to replicate the fact that it's been retriggered, hence the RPC here.  I say this may not be true because any number of things could have
	// removed the GameplayCue by the time we getting a Stacking GE (e.g. RemoveGameplayCue).
	if (!bSuppressGameplayCues && !Spec.Def->bSuppressStackingCues && bFoundExistingStackableGE && AppliedEffect && !AppliedEffect->bIsInhibited)
	{
		ensureMsgf(OurCopyOfSpec, TEXT("OurCopyOfSpec will always be valid if bFoundExistingStackableGE"));
		if (OurCopyOfSpec && OurCopyOfSpec->GetStackCount() > Spec.GetStackCount())
		{
			// Because PostReplicatedChange will get called from modifying the stack count
			// (and not PostReplicatedAdd) we won't know which GE was modified.
			// So instead we need to explicitly RPC the client so it knows the GC needs updating
			//ToDo :@ Kai Double Check the necessity of this
			UAbilitySystemGlobals::Get().GetGameplayCueManager()->InvokeGameplayCueAddedAndWhileActive_FromSpec(this, *OurCopyOfSpec, PredictionKey);
		}
	}
	
	// Execute the GE at least once (if instant, this will execute once and be done. If persistent, it was added to ActiveGameplayEffects in ApplyGameplayEffectSpec)
    if (Spec.Def->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		// This is an instant effect (it never gets added to ActiveGameplayEffects)
		ExecuteGameplayEffect(*OurCopyOfSpec, PredictionKey);
	}

	// Notify the Gameplay Effect (and its Components) that it has been successfully applied
	Spec.Def->OnApplied(ActiveGameplayEffects, *OurCopyOfSpec, PredictionKey);

	UAbilitySystemComponent* InstigatorASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();

	// Send ourselves a callback	
	OnGameplayEffectAppliedToSelf(InstigatorASC, *OurCopyOfSpec, MyHandle);

	// Send the instigator a callback
	if (InstigatorASC)
	{
		InstigatorASC->OnGameplayEffectAppliedToTarget(this, *OurCopyOfSpec, MyHandle);
	}

	return MyHandle;
}

/** This is a helper function used in automated testing, I'm not sure how useful it will be to gamecode or blueprints */
FActiveGameplayEffectHandle UNpAbilitySystemComponent::ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target, float Level, FGameplayEffectContextHandle Context, FPredictionKey PredictionKey)
{
	if (GetIsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
	check(GameplayEffect);
	if (!Context.IsValid())
	{
		Context = MakeEffectContext();
	}

	FGameplayEffectSpec	Spec(GameplayEffect, Context, Level);
	return ApplyGameplayEffectSpecToTarget(Spec, Target, PredictionKey);
}

/** This is a helper function - it seems like this will be useful as a blueprint interface at the least, but Level parameter may need to be expanded */
FActiveGameplayEffectHandle UNpAbilitySystemComponent::ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey)
{
	if (GetIsRestoringFrame())
	{
		return FActiveGameplayEffectHandle();
	}
	if (GameplayEffect == nullptr)
	{
		ABILITY_LOG(Error, TEXT("UAbilitySystemComponent::ApplyGameplayEffectToSelf called by Instigator %s with a null GameplayEffect."), *EffectContext.ToString());
		return FActiveGameplayEffectHandle();
	}

	FGameplayEffectSpec	Spec(GameplayEffect, EffectContext, Level);
	return ApplyGameplayEffectSpecToSelf(Spec, PredictionKey);
}

bool UNpAbilitySystemComponent::RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove)
{
	return ActiveGameplayEffects.NpRemoveActiveGameplayEffect(Handle, StacksToRemove);
}

FActiveGameplayEffectHandle UNpAbilitySystemComponent::SetActiveGameplayEffectInhibit(
	FActiveGameplayEffectHandle&& ActiveGEHandle, bool bInhibit, bool bInvokeGameplayCueEvents)
{
	FActiveGameplayEffect* ActiveGE = ActiveGameplayEffects.GetActiveGameplayEffect(ActiveGEHandle);
	if (!ActiveGE)
	{
		ABILITY_LOG(Error, TEXT("%s received bad Active GameplayEffect Handle: %s"), ANSI_TO_TCHAR(__func__), *ActiveGEHandle.ToString());
		return FActiveGameplayEffectHandle();
	}

	if (ActiveGE->bIsInhibited != bInhibit)
	{
		ActiveGE->bIsInhibited = bInhibit;

		// It's possible the adding or removing of the tags can invalidate the ActiveGE.  As such,
		// let's make sure we hold on to that memory until this function is done.
		FScopedActiveGameplayEffectLock ScopeLockActiveGameplayEffects(ActiveGameplayEffects);

		// All OnDirty callbacks must be inhibited until we update this entire GameplayEffect.
		FScopedAggregatorOnDirtyBatch	AggregatorOnDirtyBatcher;
		if (bInhibit)
		{
			// Remove our ActiveGameplayEffects modifiers with our Attribute Aggregators
			ActiveGameplayEffects.NpRemoveActiveGameplayEffectGrantedTagsAndModifiers(*ActiveGE, bInvokeGameplayCueEvents);
		}
		else
		{
			ActiveGameplayEffects.NpAddActiveGameplayEffectGrantedTagsAndModifiers(*ActiveGE, bInvokeGameplayCueEvents);
		}

		// The act of executing anything on the ActiveGE can invalidate it.  So we need to recheck if we can continue to execute the callbacks.
		if (!ActiveGE->IsPendingRemove)
		{
			ActiveGE->EventSet.OnInhibitionChanged.Broadcast(ActiveGEHandle, ActiveGE->bIsInhibited);
		}

		// We lost that it was active somewhere along the way, let the caller know
		if (ActiveGE->IsPendingRemove)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// Normal case is the passed-in ActiveGEHandle is still active and thus can continue execution
	return MoveTemp(ActiveGEHandle);
}

TArray<float> UNpAbilitySystemComponent::GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.NpGetActiveEffectsTimeRemaining(Query);
}

TArray<TPair<float, float>> UNpAbilitySystemComponent::GetActiveEffectsTimeRemainingAndDuration(
	const FGameplayEffectQuery& Query) const
{
	return ActiveGameplayEffects.NpGetActiveEffectsTimeRemainingAndDuration(Query);
}

#pragma endregion 

#pragma region Abilities
void UNpAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	check(AbilityActorInfo.IsValid());
	bool WasAbilityActorNull = (AbilityActorInfo->AvatarActor == nullptr);
	bool AvatarChanged = (InAvatarActor != AbilityActorInfo->AvatarActor);

	AbilityActorInfo->InitFromActor(InOwnerActor, InAvatarActor, this);

	SetOwnerActor(InOwnerActor);

	// caching the previous value of the actor so we can check against it but then setting the value to the new because it may get used
	const AActor* PrevAvatarActor = GetAvatarActor_Direct();
	SetAvatarActor_Direct(InAvatarActor);

	// if the avatar actor was null but won't be after this, we want to run the deferred gameplaycues that may not have run in NetDeltaSerialize
	// Conversely, if the ability actor was previously null, then the effects would not run in the NetDeltaSerialize. As such we want to run them now.
	if ((WasAbilityActorNull || PrevAvatarActor == nullptr) && InAvatarActor != nullptr)
	{
		HandleDeferredGameplayCues(&ActiveGameplayEffects);
	}

	if (AvatarChanged)
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
		{
			if (Spec.Ability)
			{
				if (Spec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
				{
					UGameplayAbility* AbilityInstance = Spec.GetPrimaryInstance();
					// If we don't have the ability instance, it was either already destroyed or will get called on creation
					if (AbilityInstance)
					{
						AbilityInstance->OnAvatarSet(AbilityActorInfo.Get(), Spec);
					}
				}
				else
				{
					Spec.Ability->OnAvatarSet(AbilityActorInfo.Get(), Spec);
				}
			}
		}
	}

	if (UGameplayTagReponseTable* TagTable = UAbilitySystemGlobals::Get().GetGameplayTagResponseTable())
	{
		TagTable->RegisterResponseForEvents(this);
	}
	/*
	 * This doesn't need to be here, Mover keeps track of 
	 */
	if (AbilityActorInfo->SkeletalMeshComponent.IsValid())
	{
		MeshRelativeTransform = AbilityActorInfo->SkeletalMeshComponent.Get()->GetRelativeTransform();
	}
}

FGameplayAbilitySpecHandle UNpAbilitySystemComponent::GiveAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	if (GetIsRestoringFrame())
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called when restoring frame, not allowed."));

		return FGameplayAbilitySpecHandle();
	}
	
	if (!IsValid(AbilitySpec.Ability))
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called with an invalid Ability Class."));

		return FGameplayAbilitySpecHandle();
	}

	if (!AbilitySpec.Ability.IsA(UNpGameplayAbility::StaticClass()))
	{
		ABILITY_LOG(Error, TEXT("UNppAbilitySystemComponent GiveAbility called with an Ability Class that is not child of UNppGameplayAbility."));

		return FGameplayAbilitySpecHandle();
	}

	if (AbilitySpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Error, TEXT("UNppAbilitySystemComponent GiveAbility called with an Ability Class that has Instancing policy Set to NonInstanced , Only instanced Abilities Are Supported."));

		return FGameplayAbilitySpecHandle();
	}

	if (!IsOwnerActorAuthoritative())
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called on ability %s on simulated proxy, not allowed!"), *AbilitySpec.Ability->GetName());

		return FGameplayAbilitySpecHandle();
	}

	// Set the Handle to a Synced Value
	SyncedAbilitiesHandlesCount++;
	FGameplayAbilitySpec SpecWithSyncedHandle = AbilitySpec;
	SpecWithSyncedHandle.SetSyncedHandle(SyncedAbilitiesHandlesCount);
	// If locked, add to pending list. The Spec.Handle is not regenerated when we receive, so returning this is ok.
	if (AbilityScopeLockCount > 0)
	{
		UE_LOG(LogAbilitySystem, Verbose, TEXT("%s: GiveAbility %s delayed (ScopeLocked)"), *GetNameSafe(GetOwner()), *GetNameSafe(AbilitySpec.Ability));
		AbilityPendingAdds.Add(SpecWithSyncedHandle);
		return SpecWithSyncedHandle.Handle;
	}
	
	ABILITYLIST_SCOPE_LOCK();
	FGameplayAbilitySpec& OwnedSpec = ActivatableAbilities.Items[ActivatableAbilities.Items.Add(SpecWithSyncedHandle)];
	
	if (OwnedSpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		// Create the instance at creation time
		CreateNewInstanceOfAbility(OwnedSpec, SpecWithSyncedHandle.Ability);
	}
	
	OnGiveAbility(OwnedSpec);

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: GiveAbility %s [%s] Level: %d Source: %s"), *GetNameSafe(GetOwner()), *GetNameSafe(AbilitySpec.Ability), *AbilitySpec.Handle.ToString(), AbilitySpec.Level, *GetNameSafe(AbilitySpec.SourceObject.Get()));
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("GiveAbility %s [%s] Level: %d Source: %s"), *GetNameSafe(AbilitySpec.Ability), *AbilitySpec.Handle.ToString(), AbilitySpec.Level, *GetNameSafe(AbilitySpec.SourceObject.Get()));
	return OwnedSpec.Handle;
}
FGameplayAbilitySpecHandle UNpAbilitySystemComponent::GiveAbilityAndActivateOnce(FGameplayAbilitySpec& AbilitySpec,
	const FGameplayEventData* GameplayEventData)
{
	if (GetIsRestoringFrame())
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called when restoring frame, not allowed."));

		return FGameplayAbilitySpecHandle();
	}
	
	if (!IsValid(AbilitySpec.Ability))
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called with an invalid Ability Class."));

		return FGameplayAbilitySpecHandle();
	}

	if (AbilitySpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called on ability %s that is non instanced, not allowed!"), *AbilitySpec.Ability->GetName());

		return FGameplayAbilitySpecHandle();
	}
	
	if (!IsOwnerActorAuthoritative())
	{
		ABILITY_LOG(Error, TEXT("GiveAbilityAndActivateOnce called on ability %s on simulated Proxy, not allowed!"), *AbilitySpec.Ability->GetName());

		return FGameplayAbilitySpecHandle();
	}

	AbilitySpec.bActivateOnce = true;

	FGameplayAbilitySpecHandle AddedAbilityHandle = GiveAbility(AbilitySpec);

	FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(AddedAbilityHandle);

	if (FoundSpec)
	{
		FoundSpec->RemoveAfterActivation = true;

		if (!InternalTryActivateAbility(AddedAbilityHandle, FPredictionKey(), nullptr, nullptr, GameplayEventData))
		{
			// We failed to activate it, so remove it now
			ClearAbility(AddedAbilityHandle);

			return FGameplayAbilitySpecHandle();
		}
	}
	else if (GameplayEventData)
	{
		// Cache the GameplayEventData in the pending spec (if it was correctly queued)
		FGameplayAbilitySpec& PendingSpec = AbilityPendingAdds.Last();
		if (PendingSpec.Handle == AddedAbilityHandle)
		{
			PendingSpec.GameplayEventData = MakeShared<FGameplayEventData>(*GameplayEventData);
		}
	}

	return AddedAbilityHandle;
}
void UNpAbilitySystemComponent::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	if (!AbilitySpec.Ability)
	{
		return;
	}

	const UGameplayAbility* SpecAbility = AbilitySpec.Ability;
	if (SpecAbility->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		if (AbilitySpec.Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| AbilitySpec.Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
		{
			// If we don't replicate and are missing an instance, add one
			if (AbilitySpec.NonReplicatedInstances.Num() == 0)
			{
				CreateNewInstanceOfAbility(AbilitySpec, SpecAbility);
			}
		}
	}

	// If this Ability Spec specified that it was created from an Active Gameplay Effect, then link the handle to the Active Gameplay Effect.
	if (AbilitySpec.GameplayEffectHandle.IsValid())
	{
		UAbilitySystemComponent* SourceASC = AbilitySpec.GameplayEffectHandle.GetOwningAbilitySystemComponent();
		UE_CLOG(!SourceASC, LogAbilitySystem, Error, TEXT("OnGiveAbility Spec '%s' GameplayEffectHandle had invalid Owning Ability System Component"), *AbilitySpec.GetDebugString());
		if (SourceASC)
		{
			FActiveGameplayEffect* SourceActiveGE = SourceASC->GetActiveGameplayEffectContainer_Mutable().GetActiveGameplayEffect(AbilitySpec.GameplayEffectHandle);
			UE_CLOG(!SourceActiveGE, LogAbilitySystem, Error, TEXT("OnGiveAbility Spec '%s' GameplayEffectHandle was not active on Owning Ability System Component '%s'"), *AbilitySpec.GetDebugString(), *SourceASC->GetName());
			if (SourceActiveGE)
			{
				SourceActiveGE->GrantedAbilityHandles.AddUnique(AbilitySpec.Handle);
			}
		}
	}

	for (const FAbilityTriggerData& TriggerData : AbilitySpec.Ability->AbilityTriggers)
	{
		FGameplayTag EventTag = TriggerData.TriggerTag;

		auto& TriggeredAbilityMap = (TriggerData.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent) ? GameplayEventTriggeredAbilities : OwnedTagTriggeredAbilities;

		if (TriggeredAbilityMap.Contains(EventTag))
		{
			TriggeredAbilityMap[EventTag].AddUnique(AbilitySpec.Handle);	// Fixme: is this right? Do we want to trigger the ability directly of the spec?
		}
		else
		{
			TArray<FGameplayAbilitySpecHandle> Triggers;
			Triggers.Add(AbilitySpec.Handle);
			TriggeredAbilityMap.Add(EventTag, Triggers);
		}

		if (TriggerData.TriggerSource != EGameplayAbilityTriggerSource::GameplayEvent)
		{
			FOnGameplayEffectTagCountChanged& CountChangedEvent = RegisterGameplayTagEvent(EventTag);
			// Add a change callback if it isn't on it already

			if (!CountChangedEvent.IsBoundToObject(this))
			{
				MonitoredTagChangedDelegateHandle = CountChangedEvent.AddUObject(this, &UNpAbilitySystemComponent::MonitoredTagChanged);
			}
		}
	}

	// If there's already a primary instance, it should be the one to receive the OnGiveAbility call
	UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
	if (PrimaryInstance)
	{
		PrimaryInstance->OnGiveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}
	else
	{
		AbilitySpec.Ability->OnGiveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}
	
}
void UNpAbilitySystemComponent::ClearAbility(const FGameplayAbilitySpecHandle& Handle)
{
	if (bIsRestoringFrame)
	{
		ABILITY_LOG(Error, TEXT("Attempted to call ClearAbility() While restoring frame. This is not allowed!"));
	}
	Super::ClearAbility(Handle);
}
void UNpAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	ensureMsgf(AbilityScopeLockCount > 0, TEXT("%hs called without an Ability List Lock.  It can produce side effects and should be locked to pin the Spec argument."), __func__);

	if (!AbilitySpec.Ability)
	{
		return;
	}

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: Removing Ability [%s] %s Level: %d"), *GetNameSafe(GetOwner()), *AbilitySpec.Handle.ToString(), *GetNameSafe(AbilitySpec.Ability), AbilitySpec.Level);
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("Removing Ability [%s] %s Level: %d"), *AbilitySpec.Handle.ToString(), *GetNameSafe(AbilitySpec.Ability), AbilitySpec.Level);

	for (const FAbilityTriggerData& TriggerData : AbilitySpec.Ability->AbilityTriggers)
	{
		FGameplayTag EventTag = TriggerData.TriggerTag;

		auto& TriggeredAbilityMap = (TriggerData.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent) ? GameplayEventTriggeredAbilities : OwnedTagTriggeredAbilities;

		if (ensureMsgf(TriggeredAbilityMap.Contains(EventTag), 
			TEXT("%s::%s not found in TriggeredAbilityMap while removing, TriggerSource: %d"), *AbilitySpec.Ability->GetName(), *EventTag.ToString(), (int32)TriggerData.TriggerSource))
		{
			TriggeredAbilityMap[EventTag].Remove(AbilitySpec.Handle);
			if (TriggeredAbilityMap[EventTag].Num() == 0)
			{
				TriggeredAbilityMap.Remove(EventTag);
			}
		}
	}

	TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();

	for (auto Instance : Instances)
	{
		if (Instance)
		{
			if (Instance->IsActive())
			{
				// End the ability but don't replicate it, OnRemoveAbility gets replicated
				bool bReplicateEndAbility = false;
				bool bWasCancelled = false;
				Instance->EndAbility(Instance->CurrentSpecHandle, Instance->CurrentActorInfo, Instance->CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
			else
			{
				if (Instance->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
				{
					ABILITY_LOG(Error, TEXT("%s was InActive, yet still instanced during OnRemove"), *Instance->GetName());
					Instance->MarkAsGarbage();
				}
			}
		}
	}

	// Notify the ability that it has been removed.  It follows the same pattern as OnGiveAbility() and is only called on the primary instance of the ability or the CDO.
	UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
	if (PrimaryInstance)
	{
		PrimaryInstance->OnRemoveAbility(AbilityActorInfo.Get(), AbilitySpec);
		PrimaryInstance->MarkAsGarbage();
	}
	else
	{
		// If we're non-instanced and still active, we need to End
		if (AbilitySpec.IsActive())
		{
			if (ensureMsgf(AbilitySpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("We should never have an instanced Gameplay Ability that is still active by this point. All instances should have EndAbility called just before here.")))
			{
				// Seems like it should be cancelled, but we're just following the existing pattern (could be due to functionality from OnRep)
				constexpr bool bReplicateEndAbility = false;
				constexpr bool bWasCancelled = false;
				AbilitySpec.Ability->EndAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), AbilitySpec.ActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
		}

		AbilitySpec.Ability->OnRemoveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}

	// If this Ability Spec specified that it was created from an Active Gameplay Effect, then unlink the handle to the Active Gameplay Effect.
	// Note: It's possible (maybe even likely) that the ActiveGE is no longer considered active by this point.
	// That means we can't use FindActiveGameplayEffectHandle (which fails if ActiveGE is PendingRemove), but also many of these checks will fail
	// if the ActiveGE has completed its removal.
	if (AbilitySpec.GameplayEffectHandle.IsValid()) 
	{
		if (UAbilitySystemComponent* SourceASC = AbilitySpec.GameplayEffectHandle.GetOwningAbilitySystemComponent())
		{
			if (FActiveGameplayEffect* SourceActiveGE = SourceASC->GetActiveGameplayEffectContainer_Mutable().GetActiveGameplayEffect(AbilitySpec.GameplayEffectHandle))
			{
				SourceActiveGE->GrantedAbilityHandles.Remove(AbilitySpec.Handle);
			}
		}
	}

	AbilitySpec.ReplicatedInstances.Empty();
	AbilitySpec.NonReplicatedInstances.Empty();
}
void UNpAbilitySystemComponent::CheckForClearedAbilities()
{
	for (auto& Triggered : GameplayEventTriggeredAbilities)
	{
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
			}
		}
		
		// We leave around the empty trigger stub, it's likely to be added again
	}

	for (auto& Triggered : OwnedTagTriggeredAbilities)
	{
		bool bRemovedTrigger = false;
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
				bRemovedTrigger = true;
			}
		}
		
		if (bRemovedTrigger && Triggered.Value.Num() == 0)
		{
			// If we removed all triggers, remove the callback
			FOnGameplayEffectTagCountChanged& CountChangedEvent = RegisterGameplayTagEvent(Triggered.Key);
		
			if (CountChangedEvent.IsBoundToObject(this))
			{
				CountChangedEvent.Remove(MonitoredTagChangedDelegateHandle);
			}
		}

		// We leave around the empty trigger stub, it's likely to be added again
	}

	// Clear any out of date ability spec handles on active gameplay effects
	for (FActiveGameplayEffect& ActiveGE : &ActiveGameplayEffects)
	{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		for (FGameplayAbilitySpecDef& AbilitySpec : ActiveGE.Spec.GrantedAbilitySpecs)
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		{
			if (AbilitySpec.AssignedHandle.IsValid() && FindAbilitySpecFromHandle(AbilitySpec.AssignedHandle) == nullptr)
			{
				bool bIsPendingAdd = false;
				for (const FAbilityListLockActiveChange* ActiveChange : AbilityListLockActiveChanges)
				{
					for (const FGameplayAbilitySpec& PendingSpec : ActiveChange->Adds)
					{
						if (PendingSpec.Handle == AbilitySpec.AssignedHandle)
						{
							bIsPendingAdd = true;
							break;
						}
					}

					if (bIsPendingAdd)
					{
						break;
					}
				}

				for (const FGameplayAbilitySpec& PendingSpec : AbilityPendingAdds)
				{
					if (PendingSpec.Handle == AbilitySpec.AssignedHandle)
					{
						bIsPendingAdd = true;
						break;
					}
				}

				if (bIsPendingAdd)
				{
					ABILITY_LOG(Verbose, TEXT("Skipped clearing AssignedHandle %s from GE %s / %s, as it is pending being added."), *AbilitySpec.AssignedHandle.ToString(), *ActiveGE.GetDebugString(), *ActiveGE.Handle.ToString());
					continue;
				}

				ABILITY_LOG(Verbose, TEXT("::CheckForClearedAbilities is clearing AssignedHandle %s from GE %s / %s"), *AbilitySpec.AssignedHandle.ToString(), *ActiveGE.GetDebugString(), *ActiveGE.Handle.ToString() );
				AbilitySpec.AssignedHandle = FGameplayAbilitySpecHandle();
			}
		}
	}
}
UGameplayAbility* UNpAbilitySystemComponent::CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec,
	const UGameplayAbility* Ability)
{
	check(Ability);
	check(Ability->HasAllFlags(RF_ClassDefaultObject));
	check(Ability->IsA(UNpGameplayAbility::StaticClass()));
	AActor* Owner = GetOwner();
	check(Owner);

	UNpGameplayAbility * AbilityInstance = NewObject<UNpGameplayAbility>(Owner, Ability->GetClass());
	check(AbilityInstance);
	// Setup Tasks Instances when the ability instance is created.
	AbilityInstance->InstantiateNetPredictionTasks();
	// Add it to one of our instance lists so that it doesn't GC.
	// if execution policy is local only, it is for local player only , we don't sync it. 
	// if execution policy is server only , it is only on server, we don't sync it
	//BEWARE , if these effect the simulation it will cause a correction
	if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		Spec.NonReplicatedInstances.Add(AbilityInstance);
	}
	else
	{
		Spec.ReplicatedInstances.Add(AbilityInstance);
	}
	
	
	return AbilityInstance;
}
void UNpAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability,
	bool bWasCancelled)
{
	check(Ability);
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec == nullptr)
	{
		// The ability spec may have been removed while we were ending. We can assume everything was cleaned up if the spec isnt here.
		return;
	}

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: Ended [%s] %s. Level: %d. WasCancelled: %d."), *GetNameSafe(GetOwner()), *Handle.ToString(), Spec->GetPrimaryInstance() ? *Spec->GetPrimaryInstance()->GetName() : *Ability->GetName(), Spec->Level, bWasCancelled);
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("Ended [%s] %s. Level: %d. WasCancelled: %d."), *Handle.ToString(), Spec->GetPrimaryInstance() ? *Spec->GetPrimaryInstance()->GetName() : *Ability->GetName(), Spec->Level, bWasCancelled);

	// If AnimatingAbility ended, clear the pointer
	if (LocalAnimMontageInfo.AnimatingAbility.Get() == Ability)
	{
		ClearAnimatingAbility(Ability);
	}

	// check to make sure we do not cause a roll over to uint8 by decrementing when it is 0
	if (ensureMsgf(Spec->ActiveCount > 0, TEXT("NotifyAbilityEnded called when the Spec->ActiveCount <= 0 for ability %s"), *Ability->GetName()))
	{
		Spec->ActiveCount--;
	}

	// Broadcast that the ability ended
	AbilityEndedCallbacks.Broadcast(Ability);
	OnAbilityEnded.Broadcast(FAbilityEndedData(Ability, Handle, false, bWasCancelled));
	
	// Above callbacks could have invalidated the Spec pointer, so find it again
	Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Error, TEXT("%hs(%s): %s lost its active handle halfway through the function."), __func__, *GetNameSafe(Ability), *Handle.ToString());
		return;
	}

	/** If this is instanced per execution or flagged for cleanup, mark pending kill and remove it from our instanced lists */
	if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		check(Ability->HasAnyFlags(RF_ClassDefaultObject) == false);	// Should never be calling this on a CDO for an instanced ability!

		if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
			|| Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
		{
			Spec->NonReplicatedInstances.Remove(Ability);
		}
		else
		{
			Spec->ReplicatedInstances.Remove(Ability);
		}
		

		Ability->MarkAsGarbage();
	}

	if (Spec->RemoveAfterActivation && !Spec->IsActive())
	{
		// If we should remove after activation and there are no more active instances, kill it now
		ClearAbility(Handle);
	}
}
bool UNpAbilitySystemComponent::TryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate,
	bool bAllowRemoteActivation)
{
	FGameplayTagContainer FailureTags;
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityToActivate);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Handle"));
		return false;
	}

	// don't activate abilities that are waiting to be removed
	if (Spec->PendingRemove || Spec->RemoveAfterActivation)
	{
		return false;
	}

	UGameplayAbility* Ability = Spec->Ability;

	if (!Ability)
	{
		ABILITY_LOG(Warning, TEXT("TryActivateAbility called with invalid Ability"));
		return false;
	}

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FGameplayAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == nullptr || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

		
	const ENetRole NetMode = ActorInfo->AvatarActor->GetLocalRole();

	// This should only come from button presses/local instigation (AI, etc).
	if (NetMode == ROLE_SimulatedProxy)
	{
		return false;
	}
	bool bIsLocal = AbilityActorInfo->IsLocallyControlled();

	// Check to see if this a local only or server only ability, if so either remotely execute or fail
	if (!bIsLocal && Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly)
	{
		ABILITY_LOG(Log, TEXT("Can't activate LocalOnly ability %s when not local."), *Ability->GetName());
		return false;
	}

	if (NetMode != ROLE_Authority && (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly || Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerInitiated))
	{
		ABILITY_LOG(Log, TEXT("Can't activate ServerOnly or ServerInitiated ability %s when not the server."), *Ability->GetName());
		return false;
	}
	return InternalTryActivateAbility(AbilityToActivate);
}
bool UNpAbilitySystemComponent::InternalTryActivateAbility(FGameplayAbilitySpecHandle Handle,
	FPredictionKey InPredictionKey, UGameplayAbility** OutInstancedAbility,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData)
{
	InternalTryActivateAbilityFailureTags.Reset();

	if (Handle.IsValid() == false)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called with invalid Handle! ASC: %s. AvatarActor: %s"), *GetPathName(), *GetNameSafe(GetAvatarActor_Direct()));
		return false;
	}

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called with a valid handle but no matching ability was found. Handle: %s ASC: %s. AvatarActor: %s"), *Handle.ToString(), *GetPathName(), *GetNameSafe(GetAvatarActor_Direct()));
		return false;
	}

	// Lock ability list so our Spec doesn't get destroyed while activating
	ABILITYLIST_SCOPE_LOCK();

	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

	// make sure the ActorInfo and then Actor on that FGameplayAbilityActorInfo are valid, if not bail out.
	if (ActorInfo == nullptr || !ActorInfo->OwnerActor.IsValid() || !ActorInfo->AvatarActor.IsValid())
	{
		return false;
	}

	// This should only come from button presses/local instigation (AI, etc)
	ENetRole NetMode = ROLE_SimulatedProxy;

	// Use PC netmode if its there
	if (APlayerController* PC = ActorInfo->PlayerController.Get())
	{
		NetMode = PC->GetLocalRole();
	}
	// Fallback to avataractor otherwise. Edge case: avatar "dies" and becomes torn off and ROLE_Authority. We don't want to use this case (use PC role instead).
	else if (AActor* LocalAvatarActor = GetAvatarActor_Direct())
	{
		NetMode = LocalAvatarActor->GetLocalRole();
	}

	if (NetMode == ROLE_SimulatedProxy)
	{
		return false;
	}
	
	UGameplayAbility* Ability = Spec->Ability;

	if (!Ability)
	{
		ABILITY_LOG(Warning, TEXT("InternalTryActivateAbility called with invalid Ability"));
		return false;
	}

	// If it's an instanced one, the instanced ability will be set, otherwise it will be null
	UGameplayAbility* InstancedAbility = Spec->GetPrimaryInstance();
	UGameplayAbility* AbilitySource = InstancedAbility ? InstancedAbility : Ability;

	if (TriggerEventData)
	{
		if (!AbilitySource->ShouldAbilityRespondToEvent(ActorInfo, TriggerEventData))
		{
			UE_LOG(LogAbilitySystem, Verbose, TEXT("%s: Can't activate %s because ShouldAbilityRespondToEvent was false."), *GetNameSafe(GetOwner()), *Ability->GetName());
			UE_VLOG(GetOwner(), VLogAbilitySystem, Verbose, TEXT("Can't activate %s because ShouldAbilityRespondToEvent was false."), *Ability->GetName());

			NotifyAbilityFailed(Handle, AbilitySource, InternalTryActivateAbilityFailureTags);
			return false;
		}
	}

	{
		const FGameplayTagContainer* SourceTags = TriggerEventData ? &TriggerEventData->InstigatorTags : nullptr;
		const FGameplayTagContainer* TargetTags = TriggerEventData ? &TriggerEventData->TargetTags : nullptr;

		FScopedCanActivateAbilityLogEnabler LogEnabler;
		if (!AbilitySource->CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, &InternalTryActivateAbilityFailureTags))
		{
			// CanActivateAbility with LogEnabler will have UE_LOG/UE_VLOG so don't add more failure logs here
			NotifyAbilityFailed(Handle, AbilitySource, InternalTryActivateAbilityFailureTags);
			return false;
		}
	}

	// If we're instance per actor and we're already active, don't let us activate again as this breaks the graph
	if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor)
	{
		if (Spec->IsActive())
		{
			if (Ability->bRetriggerInstancedAbility && InstancedAbility)
			{
				UE_LOG(LogAbilitySystem, Verbose, TEXT("%s: Ending %s prematurely to retrigger."), *GetNameSafe(GetOwner()), *Ability->GetName());
				UE_VLOG(GetOwner(), VLogAbilitySystem, Verbose, TEXT("Ending %s prematurely to retrigger."), *Ability->GetName());

				bool bReplicateEndAbility = true;
				bool bWasCancelled = false;
				InstancedAbility->EndAbility(Handle, ActorInfo, Spec->ActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
			else
			{
				UE_LOG(LogAbilitySystem, Verbose, TEXT("Can't activate instanced per actor ability %s when their is already a currently active instance for this actor."), *Ability->GetName());
				return false;
			}
		}
	}

	// Make sure we have a primary
	if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerActor && !InstancedAbility)
	{
		UE_LOG(LogAbilitySystem, Warning, TEXT("InternalTryActivateAbility called but instanced ability is missing! NetMode: %d. Ability: %s"), (int32)NetMode, *Ability->GetName());
		return false;
	}

	// Setup a fresh ActivationInfo for this AbilitySpec.
	Spec->ActivationInfo = FGameplayAbilityActivationInfo(ActorInfo->OwnerActor.Get());
	FGameplayAbilityActivationInfo &ActivationInfo = Spec->ActivationInfo;

	// ----------------------------------------------
	//	Call ActivateAbility (note this could end the ability too!)
	// ----------------------------------------------

	// Create instance of this ability if necessary
	if (Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		InstancedAbility = CreateNewInstanceOfAbility(*Spec, Ability);
		UNpGameplayAbility* NpInstancedAbility = Cast<UNpGameplayAbility>(InstancedAbility);
		NpInstancedAbility->ActivatedByInputActionIndex = Spec->InputID;
		InstancedAbility->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	}
	else
	{
		UNpGameplayAbility* NpInstancedAbility = Cast<UNpGameplayAbility>(AbilitySource);
		NpInstancedAbility->ActivatedByInputActionIndex = Spec->InputID;
		AbilitySource->CallActivateAbility(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	}
	
	if (InstancedAbility)
	{
		if (OutInstancedAbility)
		{
			*OutInstancedAbility = InstancedAbility;
		}
	}

	//ToDo : Check if this is used for anything other than AFK detection, if so make it use simulation time
	AbilityLastActivatedTime = GetWorld()->GetTimeSeconds();

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: Activated [%s] %s. Level: %d. PredictionKey: %s."), *GetNameSafe(GetOwner()), *Spec->Handle.ToString(), *GetNameSafe(AbilitySource), Spec->Level, *ActivationInfo.GetActivationPredictionKey().ToString());
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("Activated [%s] %s. Level: %d. PredictionKey: %s."), *Spec->Handle.ToString(), *GetNameSafe(AbilitySource), Spec->Level, *ActivationInfo.GetActivationPredictionKey().ToString());
	return true;
}
bool UNpAbilitySystemComponent::TriggerAbilityFromGameplayEvent(FGameplayAbilitySpecHandle AbilityToTrigger,
	FGameplayAbilityActorInfo* ActorInfo, FGameplayTag Tag, const FGameplayEventData* Payload,
	UAbilitySystemComponent& Component)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityToTrigger);
	if (!ensureMsgf(Spec, TEXT("Failed to find gameplay ability spec %s"), *Tag.ToString()))
	{
		return false;
	}

	const UGameplayAbility* InstancedAbility = Spec->GetPrimaryInstance();
	const UGameplayAbility* Ability = InstancedAbility ? InstancedAbility : Spec->Ability;
	if (!ensure(Ability))
	{
		return false;
	}

	if (!ensure(Payload))
	{
		return false;
	}
	
	// Make a temp copy of the payload, and copy the event tag into it
	FGameplayEventData TempEventData = *Payload;
	TempEventData.EventTag = Tag;

	// Run on the non-instanced ability
	return InternalTryActivateAbility(AbilityToTrigger, ScopedPredictionKey, nullptr, nullptr, &TempEventData);
}
void UNpAbilitySystemComponent::MonitoredTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	ABILITYLIST_SCOPE_LOCK();
	if (OwnedTagTriggeredAbilities.Contains(Tag))
	{
		TArray<FGameplayAbilitySpecHandle> TriggeredAbilityHandles = OwnedTagTriggeredAbilities[Tag];

		for (auto AbilityHandle : TriggeredAbilityHandles)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);

			if (!Spec || !IsOwnerActorAuthoritative())
			{
				continue;
			}

			if (Spec->Ability)
			{
				TArray<FAbilityTriggerData> AbilityTriggers = Spec->Ability->AbilityTriggers;
				for (const FAbilityTriggerData& TriggerData : AbilityTriggers)
				{
					FGameplayTag EventTag = TriggerData.TriggerTag;

					if (EventTag == Tag)
					{
						if (NewCount > 0)
						{
							// Populate event data so this will use the same blueprint node to activate as gameplay triggers
							FGameplayEventData EventData;
							EventData.EventMagnitude = NewCount;
							EventData.EventTag = EventTag;
							EventData.Instigator = GetOwnerActor();
							EventData.Target = EventData.Instigator;
							// Try to activate it
							InternalTryActivateAbility(Spec->Handle, FPredictionKey(), nullptr, nullptr, &EventData);
						}
						else if (NewCount == 0 && TriggerData.TriggerSource == EGameplayAbilityTriggerSource::OwnedTagPresent)
						{
							// Try to cancel, but only if the type is right
							CancelAbilitySpec(*Spec, nullptr);
						}
					}
				}
			}
		}
	}
}

void UNpAbilitySystemComponent::CancelAbility(UGameplayAbility* Ability)
{
	if (GetIsRestoringFrame())
	{
		return;
	}
	Super::CancelAbility(Ability);
}

void UNpAbilitySystemComponent::CancelAbilityHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (GetIsRestoringFrame())
	{
		return;
	}
	Super::CancelAbilityHandle(AbilityHandle);
}

void UNpAbilitySystemComponent::CancelAbilities(const FGameplayTagContainer* WithTags,
	const FGameplayTagContainer* WithoutTags, UGameplayAbility* Ignore)
{
	if (GetIsRestoringFrame())
	{
		return;
	}
	Super::CancelAbilities(WithTags, WithoutTags, Ignore);
}

void UNpAbilitySystemComponent::CancelAllAbilities(UGameplayAbility* Ignore)
{
	if (GetIsRestoringFrame())
	{
		return;
	}
	Super::CancelAllAbilities(Ignore);
}

void UNpAbilitySystemComponent::DestroyActiveState()
{
	if (GetIsRestoringFrame())
	{
		return;
	}
	Super::DestroyActiveState();
}

void UNpAbilitySystemComponent::TickAbilities(const FAbilitySystemTimeStep& TimeStep)
{
	ABILITYLIST_SCOPE_LOCK();
	TArray<FGameplayAbilitySpec> ActivatableAbilitiesSpecs = GetActivatableAbilities();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilitiesSpecs)
	{
		if (Spec.PendingRemove)
		{
			continue;
		}
		for (UGameplayAbility* Ability : Spec.ReplicatedInstances)
		{
			UNpGameplayAbility* NpAbility = Cast<UNpGameplayAbility>(Ability);
			if (NpAbility->IsActive())
			{
				NpAbility->SimulationTick(TimeStep);
			}
		}
	}
}

void UNpAbilitySystemComponent::ForceGiveAbility(const FActivatableAbilitySyncState& AbilityToAdd)
{
	FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityToAdd.AbilityClass,AbilityToAdd.Level,INDEX_NONE,AbilityToAdd.SourceObject);
	AbilitySpec.SetSyncedHandle(AbilityToAdd.ActivatableAbilityHandle);
	
	if (!IsValid(AbilitySpec.Ability))
	{
		ABILITY_LOG(Error, TEXT("GiveAbility called with an invalid Ability Class."));

		return;
	}

	if (!AbilitySpec.Ability.IsA(UNpGameplayAbility::StaticClass()))
	{
		ABILITY_LOG(Error, TEXT("UNppAbilitySystemComponent ForceGiveAbility called with an Ability Class that is not child of UNppGameplayAbility."));

		return;
	}

	if (AbilitySpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced)
	{
		ABILITY_LOG(Error, TEXT("UNppAbilitySystemComponent ForceGiveAbility called with an Ability Class that has Instancing policy Set to NonInstanced , Only instanced Abilities Are Supported."));

		return;
	}
	FGameplayAbilitySpec& OwnedSpec = ActivatableAbilities.Items[ActivatableAbilities.Items.Add(AbilitySpec)];
	TArray<UNpGameplayAbility*> AbilityInstances;
	
	// Add the instances of the ability and set their state correctly
	if (AbilityToAdd.ActiveInstances.ActiveAbilityInstances.Num() > 0)
	{
		AbilityInstances.Reserve(AbilityToAdd.ActiveInstances.ActiveAbilityInstances.Num());
		for (int32 i = 0 ; i < AbilityToAdd.ActiveInstances.ActiveAbilityInstances.Num() ; ++i)
		{
			UNpGameplayAbility* Instance = ForceCreateAbilityInstance(&OwnedSpec);
			AbilityInstances.Add(Instance);
		}
	}
	OnForceGiveAbility(OwnedSpec);

	for (int32 i = 0 ; i < AbilityInstances.Num() ; ++i)
	{
		const FActiveAbilityInstanceData& AbilityInstanceData = AbilityToAdd.ActiveInstances.ActiveAbilityInstances[i];
		UNpGameplayAbility* Instance = AbilityInstances[i];
		ForceActivateAbility(Instance,&OwnedSpec,AbilityInstanceData);
	}
	UE_LOG(LogAbilitySystem, Log, TEXT("%s: ForceGiveAbility %s [%s] Level: %d Source: %s"), *GetNameSafe(GetOwner()), *GetNameSafe(AbilitySpec.Ability), *AbilitySpec.Handle.ToString(), AbilitySpec.Level, *GetNameSafe(AbilitySpec.SourceObject.Get()));
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("ForceGiveAbility %s [%s] Level: %d Source: %s"), *GetNameSafe(AbilitySpec.Ability), *AbilitySpec.Handle.ToString(), AbilitySpec.Level, *GetNameSafe(AbilitySpec.SourceObject.Get()));
}
void UNpAbilitySystemComponent::OnForceGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	if (!AbilitySpec.Ability)
	{
		return;
	}

	if (AbilitySpec.Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| AbilitySpec.Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		// If we don't replicate and are missing an instance, add one
		if (AbilitySpec.NonReplicatedInstances.Num() == 0)
		{
			ForceCreateNewInstanceOfAbility(AbilitySpec, AbilitySpec.Ability);
		}
	}
	for (const FAbilityTriggerData& TriggerData : AbilitySpec.Ability->AbilityTriggers)
	{
		FGameplayTag EventTag = TriggerData.TriggerTag;

		auto& TriggeredAbilityMap = (TriggerData.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent) ? GameplayEventTriggeredAbilities : OwnedTagTriggeredAbilities;

		if (TriggeredAbilityMap.Contains(EventTag))
		{
			TriggeredAbilityMap[EventTag].AddUnique(AbilitySpec.Handle);
		}
		else
		{
			TArray<FGameplayAbilitySpecHandle> Triggers;
			Triggers.Add(AbilitySpec.Handle);
			TriggeredAbilityMap.Add(EventTag, Triggers);
		}

		if (TriggerData.TriggerSource != EGameplayAbilityTriggerSource::GameplayEvent)
		{
			FOnGameplayEffectTagCountChanged& CountChangedEvent = RegisterGameplayTagEvent(EventTag);
			// Add a change callback if it isn't on it already

			if (!CountChangedEvent.IsBoundToObject(this))
			{
				MonitoredTagChangedDelegateHandle = CountChangedEvent.AddUObject(this, &UNpAbilitySystemComponent::MonitoredTagChanged);
			}
		}
	}

	// If there's already a primary instance, it should be the one to receive the OnGiveAbility call
	if (UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance())
	{
		PrimaryInstance->OnGiveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}
	else
	{
		AbilitySpec.Ability->OnGiveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}
}
UNpGameplayAbility* UNpAbilitySystemComponent::ForceCreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec,
	const UGameplayAbility* Ability)
{
	check(Ability);
	check(Ability->HasAllFlags(RF_ClassDefaultObject));
	check(Ability->IsA(UNpGameplayAbility::StaticClass()));
	AActor* Owner = GetOwner();
	check(Owner);

	UNpGameplayAbility* AbilityInstance = NewObject<UNpGameplayAbility>(Owner, Ability->GetClass());
	check(AbilityInstance);
	AbilityInstance->CurrentSpecHandle = Spec.Handle;
	// Setup Tasks Instances when the ability instance is created.
	AbilityInstance->InstantiateNetPredictionTasks();
	// Add it to one of our instance lists so that it doesn't GC.
	if (Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		Spec.NonReplicatedInstances.Add(AbilityInstance);
	}
	else
	{
		// Add it to one of our instance lists so that it doesn't GC.
		Spec.ReplicatedInstances.Add(AbilityInstance);
	}
	return AbilityInstance;
}
void UNpAbilitySystemComponent::ForceClearAbility(const FGameplayAbilitySpec& AbilitySpec)
{
	const FGameplayAbilitySpecHandle& Handle = AbilitySpec.Handle;
	
	for (int Idx = 0; Idx < AbilityPendingAdds.Num(); ++Idx)
	{
		if (AbilityPendingAdds[Idx].Handle == Handle)
		{
			AbilityPendingAdds.RemoveAtSwap(Idx, 1, EAllowShrinking::No);
			return;
		}
	}

	for (int Idx = 0; Idx < ActivatableAbilities.Items.Num(); ++Idx)
	{
		check(ActivatableAbilities.Items[Idx].Handle.IsValid());
		if (ActivatableAbilities.Items[Idx].Handle == Handle)
		{
			// OnRemoveAbility will possibly call EndAbility. EndAbility can "do anything" including remove this ability Spec again.
			// So a scoped list lock is necessary here.
			OnForceRemoveAbility(ActivatableAbilities.Items[Idx]);
			ActivatableAbilities.Items.RemoveAtSwap(Idx);
			CheckForClearedAbilitiesAfterForceRemove();
			return;
		}
	}
}
void UNpAbilitySystemComponent::OnForceRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	ensureMsgf(AbilityScopeLockCount > 0, TEXT("%hs called without an Ability List Lock.  It can produce side effects and should be locked to pin the Spec argument."), __func__);

	if (!AbilitySpec.Ability)
	{
		return;
	}

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: Removing Ability [%s] %s Level: %d"), *GetNameSafe(GetOwner()), *AbilitySpec.Handle.ToString(), *GetNameSafe(AbilitySpec.Ability), AbilitySpec.Level);
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("Removing Ability [%s] %s Level: %d"), *AbilitySpec.Handle.ToString(), *GetNameSafe(AbilitySpec.Ability), AbilitySpec.Level);

	for (const FAbilityTriggerData& TriggerData : AbilitySpec.Ability->AbilityTriggers)
	{
		FGameplayTag EventTag = TriggerData.TriggerTag;

		auto& TriggeredAbilityMap = (TriggerData.TriggerSource == EGameplayAbilityTriggerSource::GameplayEvent) ? GameplayEventTriggeredAbilities : OwnedTagTriggeredAbilities;

		if (ensureMsgf(TriggeredAbilityMap.Contains(EventTag), 
			TEXT("%s::%s not found in TriggeredAbilityMap while removing, TriggerSource: %d"), *AbilitySpec.Ability->GetName(), *EventTag.ToString(), (int32)TriggerData.TriggerSource))
		{
			TriggeredAbilityMap[EventTag].Remove(AbilitySpec.Handle);
			if (TriggeredAbilityMap[EventTag].Num() == 0)
			{
				TriggeredAbilityMap.Remove(EventTag);
			}
		}
	}

	TArray<UGameplayAbility*> Instances = AbilitySpec.GetAbilityInstances();

	for (auto Instance : Instances)
	{
		if (Instance)
		{
			if (Instance->IsActive())
			{
				bool bReplicateEndAbility = false;
				bool bWasCancelled = false;
				Instance->EndAbility(Instance->CurrentSpecHandle, Instance->CurrentActorInfo, Instance->CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
			else
			{
				if (Instance->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
				{
					ABILITY_LOG(Error, TEXT("%s was InActive, yet still instanced during OnRemove"), *Instance->GetName());
					Instance->MarkAsGarbage();
				}
			}
		}
	}

	// Notify the ability that it has been removed.  It follows the same pattern as OnGiveAbility() and is only called on the primary instance of the ability or the CDO.
	UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
	if (PrimaryInstance)
	{
		PrimaryInstance->OnRemoveAbility(AbilityActorInfo.Get(), AbilitySpec);
		PrimaryInstance->MarkAsGarbage();
	}
	else
	{
		// If we're non-instanced and still active, we need to End
		if (AbilitySpec.IsActive())
		{
			if (ensureMsgf(AbilitySpec.Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::NonInstanced, TEXT("We should never have an instanced Gameplay Ability that is still active by this point. All instances should have EndAbility called just before here.")))
			{
				// Seems like it should be cancelled, but we're just following the existing pattern (could be due to functionality from OnRep)
				constexpr bool bReplicateEndAbility = false;
				constexpr bool bWasCancelled = false;
				AbilitySpec.Ability->EndAbility(AbilitySpec.Handle, AbilityActorInfo.Get(), AbilitySpec.ActivationInfo, bReplicateEndAbility, bWasCancelled);
			}
		}

		AbilitySpec.Ability->OnRemoveAbility(AbilityActorInfo.Get(), AbilitySpec);
	}

	AbilitySpec.ReplicatedInstances.Empty();
	AbilitySpec.NonReplicatedInstances.Empty();
}
void UNpAbilitySystemComponent::CheckForClearedAbilitiesAfterForceRemove()
{
	for (auto& Triggered : GameplayEventTriggeredAbilities)
	{
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
			}
		}
		
		// We leave around the empty trigger stub, it's likely to be added again
	}

	for (auto& Triggered : OwnedTagTriggeredAbilities)
	{
		bool bRemovedTrigger = false;
		// Make sure all triggered abilities still exist, if not remove
		for (int32 i = 0; i < Triggered.Value.Num(); i++)
		{
			FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Triggered.Value[i]);

			if (!Spec)
			{
				Triggered.Value.RemoveAt(i);
				i--;
				bRemovedTrigger = true;
			}
		}
		
		if (bRemovedTrigger && Triggered.Value.Num() == 0)
		{
			// If we removed all triggers, remove the callback
			FOnGameplayEffectTagCountChanged& CountChangedEvent = RegisterGameplayTagEvent(Triggered.Key);
		
			if (CountChangedEvent.IsBoundToObject(this))
			{
				CountChangedEvent.Remove(MonitoredTagChangedDelegateHandle);
			}
		}
		// We leave around the empty trigger stub, it's likely to be added again
	}
}
void UNpAbilitySystemComponent::ForceCreateActivateAbility(FGameplayAbilitySpec* Spec,
	const FActiveAbilityInstanceData& InstanceData)
{
	UNpGameplayAbility* AbilityInstance = ForceCreateAbilityInstance(Spec);
	ForceActivateAbility(AbilityInstance, Spec, InstanceData);
}
UNpGameplayAbility* UNpAbilitySystemComponent::ForceCreateAbilityInstance(FGameplayAbilitySpec* Spec)
{
	const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();
	Spec->ActivationInfo = FGameplayAbilityActivationInfo(ActorInfo->OwnerActor.Get());
	return  ForceCreateNewInstanceOfAbility(*Spec,Spec->Ability);
}
void UNpAbilitySystemComponent::ForceActivateAbility(UNpGameplayAbility* AbilityInstance, FGameplayAbilitySpec* Spec,
	const FActiveAbilityInstanceData& InstanceData)
{
	AbilityInstance->PreForceActivateAbilityInstance(Spec->Handle,AbilityActorInfo.Get(),Spec->ActivationInfo,InstanceData);
	RestoreAbilityInstance(AbilityInstance,InstanceData);
}
void UNpAbilitySystemComponent::ForceCancelAbility(const FGameplayAbilitySpecHandle& Handle, UNpGameplayAbility* AbilityInstance)
{
	AbilityInstance->SetCanBeCanceled(true);
	AbilityInstance->ForceCancelAbilityInstance(Handle,AbilityInstance->CurrentActorInfo,AbilityInstance->CurrentActivationInfo);
}
void UNpAbilitySystemComponent::NotifyAbilityForceEnded(const FGameplayAbilitySpecHandle& Handle,
	UNpGameplayAbility* AbilityInstance, const bool& bWasCancelled)
{
	check(AbilityInstance);
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (Spec == nullptr)
	{
		// The ability spec may have been removed while we were ending. We can assume everything was cleaned up if the spec isnt here.
		return;
	}

	UE_LOG(LogAbilitySystem, Log, TEXT("%s: Ended [%s] %s. Level: %d. WasCancelled: %d."), *GetNameSafe(GetOwner()), *Handle.ToString(), Spec->GetPrimaryInstance() ? *Spec->GetPrimaryInstance()->GetName() : *AbilityInstance->GetName(), Spec->Level, bWasCancelled);
	UE_VLOG(GetOwner(), VLogAbilitySystem, Log, TEXT("Ended [%s] %s. Level: %d. WasCancelled: %d."), *Handle.ToString(), Spec->GetPrimaryInstance() ? *Spec->GetPrimaryInstance()->GetName() : *AbilityInstance->GetName(), Spec->Level, bWasCancelled);
	
	// Broadcast that the ability ended
	AbilityEndedCallbacks.Broadcast(AbilityInstance);
	OnAbilityEnded.Broadcast(FAbilityEndedData(AbilityInstance, Handle, false, bWasCancelled));
	
	// Above callbacks could have invalidated the Spec pointer, so find it again
	Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		ABILITY_LOG(Error, TEXT("%hs(%s): %s lost its active handle halfway through the function."), __func__, *GetNameSafe(AbilityInstance), *Handle.ToString());
		return;
	}

	if (AbilityInstance->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		check(AbilityInstance->HasAnyFlags(RF_ClassDefaultObject) == false);	// Should never be calling this on a CDO for an instanced ability!

		Spec->ReplicatedInstances.Remove(AbilityInstance);

		AbilityInstance->MarkAsGarbage();
	}
}
void UNpAbilitySystemComponent::RestoreAbilities(const FActivatableAbilitiesCollection& AuthorityActivatableAbilities)
{
	ABILITYLIST_SCOPE_LOCK()
	TArray<FGameplayAbilitySpec>& CurrentActivatableAbilities = GetActivatableAbilities();
	TArray<FGameplayAbilitySpec> AbilitiesToRemove;
	AbilitiesToRemove.Reserve(CurrentActivatableAbilities.Num());
	TArray<FActivatableAbilitySyncState> AbilitiesToAdd;
	AbilitiesToAdd.Reserve(AuthorityActivatableAbilities.GetCollectionData().Num());

	// first loop through server activatable abilities and try to find them in current abilities array.
	// if  handle is found with correct class, roll-back that ability state.
	// if handle is found with wrong class , need to remove the one with wrong class and add another with right class and handle.
	// if handle is not found we need to add it.
	for (const FActivatableAbilitySyncState& AbilitySyncState : AuthorityActivatableAbilities.GetCollectionData())
	{
		FGameplayAbilitySpec* FoundSpec = FindAbilitySpecFromHandle(FGameplayAbilitySpecHandle(AbilitySyncState.ActivatableAbilityHandle),EConsiderPending::None);
		// found the spec with this handle
		if (FoundSpec)
		{
			// found spec is same ability
			if (FoundSpec->Ability.GetClass() == AbilitySyncState.AbilityClass)
			{
				RestoreExistingAbility(FoundSpec,AbilitySyncState);
			}
			else // if not same class need to remove this spec and add new one with correct handle
			{
				AbilitiesToRemove.Add(*FoundSpec);
				AbilitiesToAdd.Add(AbilitySyncState);
			}
		}
		else
		{
			AbilitiesToAdd.Add(AbilitySyncState);
		}
	}
	// Loop through current activatable abilities and if specific handle is not found in authority state remove it.
	for (FGameplayAbilitySpec& Spec : CurrentActivatableAbilities)
	{
		bool Found = false;
		for (const FActivatableAbilitySyncState& AbilitySyncState : AuthorityActivatableAbilities.GetCollectionData())
		{
			if (Spec.Handle == FGameplayAbilitySpecHandle(AbilitySyncState.ActivatableAbilityHandle))
			{
				Found = true;
				break;
			}
		}
		if (!Found)
		{
			AbilitiesToRemove.Add(Spec);
		}
	}
	// First remove the specs pending remove
	for (FGameplayAbilitySpec& AbilitySpecToRemove : AbilitiesToRemove)
	{
		ForceClearAbility(AbilitySpecToRemove);
	}
	for (const FActivatableAbilitySyncState& AbilitySyncState : AbilitiesToAdd)
	{
		ForceGiveAbility(AbilitySyncState);
	}
}
void UNpAbilitySystemComponent::RestoreExistingAbility(FGameplayAbilitySpec* AbilitySpec,
	const FActivatableAbilitySyncState& AuthoritySyncState)
{
	
	AbilitySpec->Level = AuthoritySyncState.Level;
	AbilitySpec->RemoveAfterActivation = AuthoritySyncState.RemoveAfterActivation;
	AbilitySpec->SourceObject = AuthoritySyncState.SourceObject.Get();
	AbilitySpec->DynamicAbilityTags = AuthoritySyncState.DynamicAbilityTags;
	AbilitySpec->SetByCallerTagMagnitudes = AuthoritySyncState.SetByCallerTagMagnitudes;
	AbilitySpec->GameplayEffectHandle = FActiveGameplayEffectHandle(AuthoritySyncState.GrantingGameplayEffectHandle);
	
	// return early if ability activation is not supposed to be synced
	if (AbilitySpec->Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::LocalOnly
		|| AbilitySpec->Ability->GetNetExecutionPolicy() == EGameplayAbilityNetExecutionPolicy::ServerOnly)
	{
		return;
	}
	AbilitySpec->ActiveCount = AuthoritySyncState.ActiveCount;

	//if we need to remove active instances just remove the last ones and roll-back the correct amount of instances left
	// to the correct state in order.
	//if we need to add just add at the end and restore the correct amount of instances in order simulation they are at
	// don't need to restore anything about local only or server only data, expect the fact it is there and its handle.
	const uint32 CurrentInstancesCount = AbilitySpec->ReplicatedInstances.Num();
	const uint32 RollbackInstancesCount = AuthoritySyncState.ActiveInstances.ActiveAbilityInstances.Num();
	// Same amount of active Instances, Roll them back
	if (RollbackInstancesCount == CurrentInstancesCount)
	{
		// if we have no active instances nothing to do
		if (RollbackInstancesCount > 0)
		{
			for (uint32 I = 0; I < CurrentInstancesCount; ++I)
			{
				UNpGameplayAbility* AbilityInstance = Cast<UNpGameplayAbility>(AbilitySpec->ReplicatedInstances[I]);
				const FActiveAbilityInstanceData& AbilityData = AuthoritySyncState.ActiveInstances.ActiveAbilityInstances[I];
				RestoreAbilityInstance(AbilityInstance,AbilityData);
			}
		}
		
	}
	else // Different amount of active instances 
	{
		// we have more active instances now, need to deactivate the extra ones
		if (CurrentInstancesCount > RollbackInstancesCount)
		{
			const int32 InstancesToRemoveNum = CurrentInstancesCount - RollbackInstancesCount;
			for (int32 I = CurrentInstancesCount - 1; I >= 0; --I)
			{
				const int32 LastIndexToRemove = CurrentInstancesCount - InstancesToRemoveNum - 1;
				UNpGameplayAbility* AbilityInstance = Cast<UNpGameplayAbility>(AbilitySpec->ReplicatedInstances[I]);
				if (I > LastIndexToRemove) // these are the extra instances we need to remove
				{
					ForceCancelAbility(AbilitySpec->Handle,AbilityInstance);
				}
				else // these will be the active instance we restore to authority state
				{
					const FActiveAbilityInstanceData& AbilityData = AuthoritySyncState.ActiveInstances.ActiveAbilityInstances[I];
					RestoreAbilityInstance(AbilityInstance,AbilityData);
				}
			}
		}
		else // we have less active instances now, need to activate the extra ones
		{
			for (uint32 I = 0; I < RollbackInstancesCount; ++I)
			{
				const FActiveAbilityInstanceData& AbilityData = AuthoritySyncState.ActiveInstances.ActiveAbilityInstances[I];
				// these will be the active instance we restore to authority state
				if (I < CurrentInstancesCount)
				{
					UNpGameplayAbility* AbilityInstance = Cast<UNpGameplayAbility>(AbilitySpec->ReplicatedInstances[I]);
					RestoreAbilityInstance(AbilityInstance,AbilityData);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("%s: Restore Existing %s"), *GetNameSafe(GetOwner()), *GetNameSafe(AbilitySpec->Ability));
					ForceCreateActivateAbility(AbilitySpec,AbilityData);
				}
			}
		}
		
	}
}
void UNpAbilitySystemComponent::RestoreAbilityInstance(UNpGameplayAbility* AbilityInstance,
	const FActiveAbilityInstanceData& AuthorityState)
{
	AbilityInstance->RestoreFrame(AuthorityState);
}
#pragma endregion

#pragma region Restore/Rollback Functions
bool UNpAbilitySystemComponent::IsReSimulating() const
{
	return LatestCachedTimeStep.bIsResimulating;
}

void UNpAbilitySystemComponent::RefreshAvatarActor(AActor* InAvatar)
{
	SetAvatarActor(InAvatar);
}

void UNpAbilitySystemComponent::RestoreGameplayEffects(const FActiveEffectSyncDataContainer& AuthorityActiveEffects)
{
	// first restore count
	ActiveGameplayEffects.ActiveEffectsHandleCount = AuthorityActiveEffects.ActiveEffectsHandleCount;
	
	TArray<FActiveGameplayEffectHandle> EffectsToRemove;
	EffectsToRemove.Reserve(ActiveGameplayEffects.GetNumGameplayEffects());
	TArray<FActiveEffectSyncData> EffectsToAdd;
	EffectsToAdd.Reserve(ActiveGameplayEffects.GetNumGameplayEffects());
	
	// first loop through server active Effect and try to find them in current Active Effects Container.
	// if  handle is found with correct class, roll-back that Effect state.
	// if handle is found with wrong class , need to remove the one with wrong class and add another with right class and handle.
	// if handle is not found we need to add it.
	for (const FActiveEffectSyncData& SyncData : AuthorityActiveEffects.ActiveEffects)
	{
		FActiveGameplayEffect* FoundEffect = ActiveGameplayEffects.GetActiveGameplayEffect(FActiveGameplayEffectHandle(SyncData.EffectHandle));
		// found the spec with this handle
		if (FoundEffect)
		{
			// found spec is same ability
			if (FoundEffect->Spec.Def == SyncData.EffectSpecData.Def && FoundEffect->Spec.GetContext().GetInstigator() == SyncData.EffectSpecData.EffectContext.GetInstigator())
			{
				RestoreExitingEffect(SyncData,FoundEffect);
			}
			else // if not same class need to remove this spec and add new one with correct handle
			{
				EffectsToRemove.Add(FoundEffect->Handle);
				EffectsToAdd.Add(SyncData);
			}
		}
		else
		{
			EffectsToAdd.Add(SyncData);
		}
	}

	// Loop through current active Effect and if specific handle is not found in authority state remove it.
	for (int32 i = 0; i < ActiveGameplayEffects.GetNumGameplayEffects(); ++i)
	{
		FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(i);
		bool Found = false;
		for (const FActiveEffectSyncData& SyncData : AuthorityActiveEffects.ActiveEffects)
		{
			if (ActiveEffect->Handle == FActiveGameplayEffectHandle(SyncData.EffectHandle))
			{
				Found = true;
				break;
			}
		}
		if (!Found)
		{
			EffectsToRemove.Add(ActiveEffect->Handle);
		}
	}

	// First remove the effects pending remove
	for (FActiveGameplayEffectHandle& EffectHandleToRemove : EffectsToRemove)
	{
		ForceRemoveEffect(EffectHandleToRemove);
	}
	for (const FActiveEffectSyncData& EffectSyncData : EffectsToAdd)
	{
		ForceApplyEffect(EffectSyncData);
	}
}
void UNpAbilitySystemComponent::RestoreExitingEffect(const FActiveEffectSyncData& AuthorityData,
	FActiveGameplayEffect* ActiveEffect)
{
	
	ActiveEffect->Spec.SetDuration(AuthorityData.EffectSpecData.GetDuration(),AuthorityData.EffectSpecData.bDurationLocked);
	ActiveEffect->Spec.Period = AuthorityData.EffectSpecData.GetPeriod();
	ActiveEffect->CurrentPeriodTime = AuthorityData.GetPeriodTimeMS();
	ActiveEffect->StartWorldTime = AuthorityData.GetStartTime();
	ActiveEffect->StartServerWorldTime = AuthorityData.GetStartTime();
	ActiveEffect->CachedStartServerWorldTime = AuthorityData.GetStartTime();
	ActiveEffect->bIsInhibited = AuthorityData.bIsInhibited;
	ActiveEffect->Spec.SetStackCount(AuthorityData.EffectSpecData.StackCount);
	ActiveEffect->Spec.CapturedSourceTags = AuthorityData.EffectSpecData.CapturedSourceTags;
	ActiveEffect->Spec.CapturedRelevantAttributes.SetCapturedAttributesValues(AuthorityData.EffectSpecData.CapturedRelevantAttributes.CapturedSourceAttributeValues
		,AuthorityData.EffectSpecData.CapturedRelevantAttributes.CapturedTargetAttributeValues);
	ActiveEffect->Spec.DynamicGrantedTags = AuthorityData.EffectSpecData.DynamicGrantedTags;
	ActiveEffect->Spec.SetByCallerTagMagnitudes = AuthorityData.EffectSpecData.SetByCallerTagMagnitudes;
	if (AuthorityData.EffectSpecData.ModifiedAttributesValues.Num() > 0)
	{
		int32 ModifierIndex = -1;
		for (const FGameplayModifierInfo& Mod : ActiveEffect->Spec.Def->Modifiers)
		{
			++ModifierIndex;
			
			// Add to ModifiedAttribute list if it doesn't exist already
			FGameplayEffectModifiedAttribute* ModifiedAttribute = ActiveEffect->Spec.GetModifiedAttribute(Mod.Attribute);
			if (!ModifiedAttribute)
			{
				ModifiedAttribute = ActiveEffect->Spec.AddModifiedAttribute(Mod.Attribute);
			}
			ModifiedAttribute->TotalMagnitude = AuthorityData.EffectSpecData.ModifiedAttributesValues[ModifierIndex];
		}
	}
	// this function would only be called when authority data and active effect have same instigator. we don't need to reset any data.
	ActiveEffect->Spec.OverrideContext(AuthorityData.EffectSpecData.EffectContext.Duplicate());
}
void UNpAbilitySystemComponent::ForceRemoveEffect(const FActiveGameplayEffectHandle& Handle)
{
	ActiveGameplayEffects.NpForceRemoveActiveGameplayEffect(Handle,-1);
}
void UNpAbilitySystemComponent::ForceApplyEffect(const FActiveEffectSyncData& AuthorityData)
{
	FGameplayEffectSpec SpecToAdd = FGameplayEffectSpec(AuthorityData.EffectSpecData.Def,AuthorityData.EffectSpecData.EffectContext.Duplicate(),AuthorityData.EffectSpecData.GetLevel());
	SpecToAdd.SetDuration(AuthorityData.EffectSpecData.GetDuration(),AuthorityData.EffectSpecData.bDurationLocked);
	SpecToAdd.Period = AuthorityData.EffectSpecData.GetPeriod();
	SpecToAdd.CapturedSourceTags = AuthorityData.EffectSpecData.CapturedSourceTags;
	SpecToAdd.DynamicGrantedTags = AuthorityData.EffectSpecData.DynamicGrantedTags;
	SpecToAdd.SetByCallerTagMagnitudes = AuthorityData.EffectSpecData.SetByCallerTagMagnitudes;
	FActiveGameplayEffectHandle Handle;
	Handle.SetSyncedHandle(this,AuthorityData.EffectHandle);
	ActiveGameplayEffects.NpForceApplyGameplayEffectSpec(SpecToAdd,Handle
		,AuthorityData.EffectSpecData.CapturedRelevantAttributes.CapturedSourceAttributeValues,AuthorityData.EffectSpecData.CapturedRelevantAttributes.CapturedTargetAttributeValues
		,AuthorityData.EffectSpecData.ModifiedAttributesValues,AuthorityData.GetPeriodTimeMS(),AuthorityData.GetStartTime());
}

void UNpAbilitySystemComponent::RestoreAttributeSets(const FAttributeSetSyncDataCollection& AuthoritySets)
{
	TArray<UAttributeSet*> SetsToRemove;
	SetsToRemove.Reserve(SpawnedAttributes.Num());
	//Loop Through Server Sets and find or create attribute set for that class, then restore its values
	for (int32 i = 0; i < AuthoritySets.AttributeSetsData.Num(); ++i)
	{
		UAttributeSet* Set = GetOrCreateAttributeSubobject_Mutable(AuthoritySets.AttributeSetsData[i].AttributeSetClass);
		RestoreExistingAttributeSet(AuthoritySets.AttributeSetsData[i], Set);
	}

	// Loop Through Existing Attribute Sets , If Can't find one in authority add to pending remove
	for (UAttributeSet* Set : SpawnedAttributes)
	{
		bool FoundSetClass = false;
		for (const FAttributeSetSyncData& AuthoritySet : AuthoritySets.AttributeSetsData)
		{
			if (Set->GetClass() == AuthoritySet.AttributeSetClass)
			{
				FoundSetClass = true;
				break;
			}
		}
		if (!FoundSetClass)
		{
			SetsToRemove.Add(Set);
		}
	}

	for (UAttributeSet* SetToRemove : SetsToRemove)
	{
		RemoveSpawnedAttribute(SetToRemove);
	}
}

void UNpAbilitySystemComponent::RestoreExistingAttributeSet(const FAttributeSetSyncData& AuthoritySet,UAttributeSet* AttributeSet)
{
	TArray<FGameplayAttribute> Attributes;
	UAttributeSet::GetAttributesFromSetClass(AttributeSet->GetClass(), Attributes);
	for (int32 i = 0; i < Attributes.Num(); ++i)
	{
		const FGameplayAttribute& Attribute = Attributes[i];
		
		const float AuthorityBaseValue = AuthoritySet.AttributeValues[i].GetBaseValue();
		const float AuthorityCurrentValue = AuthoritySet.AttributeValues[i].GetCurrentValue();
		//This Broadcasts the events which can effect the value , we can't allow that so force set values after
		const float PreviousBase = GetNumericAttributeBase(Attribute);
		SetBaseAttributeValueFromReplication(Attribute,AuthorityBaseValue,PreviousBase);
		FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet);
		if (AttributeData)
		{
			AttributeData->SetBaseValue(AuthorityBaseValue);
			AttributeData->SetCurrentValue(AuthorityCurrentValue);
		}
		else
		{
			Attribute.RestoreNumericValue(AuthorityCurrentValue,AttributeSet);
		}
	}
}

void UNpAbilitySystemComponent::RestoreTags(const FSyncedGameplayTagCount& InBlockedAbilityTags,
	const FSyncedGameplayTagCount& InGameplayTags)
{
	//First, Remove Any Tags That Shouldn't exist
	TMap<FGameplayTag,int32> CurrAbilityTags = BlockedAbilityTags.GetExplicitTagCountMap();
	for (const auto& CurrBlockedAbilityTagCount : CurrAbilityTags)
	{
		if (!InBlockedAbilityTags.GetExplicitTagCountMap().Contains(CurrBlockedAbilityTagCount.Key))
		{
			BlockedAbilityTags.SetTagCount(CurrBlockedAbilityTagCount.Key,0);
		}
	}
	// Second, Set Tag Count of received tags
	for (const auto& BlockedAbilityTagCount : InBlockedAbilityTags.GetExplicitTagCountMap())
	{
		BlockedAbilityTags.SetTagCount(BlockedAbilityTagCount.Key, BlockedAbilityTagCount.Value);
	}
	//First, Remove Any Tags That Shouldn't exist 
	TMap<FGameplayTag,int32> CurrTags = GameplayTagCountContainer.GetExplicitTagCountMap();
	for (const auto& CurrTagCount : CurrTags)
	{
		if (!InGameplayTags.GetExplicitTagCountMap().Contains(CurrTagCount.Key))
		{
			//if it's in non replicated tags don't remove it , ensure its count is correct.
			if (NonReplicatedTags.GetExplicitTagCountMap().Num() > 0 && NonReplicatedTags.GetExplicitTagCountMap().Contains(CurrTagCount.Key))
			{
				GameplayTagCountContainer.SetTagCount(CurrTagCount.Key,NonReplicatedTags.GetTagCount(CurrTagCount.Key));
			}
			else
			{
				GameplayTagCountContainer.SetTagCount(CurrTagCount.Key,0);
			}
			
		}
	}
	// Second, Set Tag Count of received tags
	for (const auto& TagCount : InGameplayTags.GetExplicitTagCountMap())
	{
		GameplayTagCountContainer.SetTagCount(TagCount.Key, TagCount.Value);
	}
}
#pragma endregion

#pragma region Cues
void UNpAbilitySystemComponent::RestoreCues(const FActiveCueSyncDataContainer& AuthorityCueSyncData)
{
	FActiveCueSyncDataContainer::FillGameplayCueContainer(AuthorityCueSyncData,ActiveGameplayCues);
}

void UNpAbilitySystemComponent::AddGameplayCue_Internal(const FGameplayTag GameplayCueTag,
	const FGameplayCueParameters& GameplayCueParameters, FActiveGameplayCueContainer& GameplayCueContainer)
{
	GameplayCueContainer.AddCue(GameplayCueTag, ScopedPredictionKey, GameplayCueParameters);
}

void UNpAbilitySystemComponent::RemoveGameplayCue_Internal(const FGameplayTag GameplayCueTag,
	FActiveGameplayCueContainer& GameplayCueContainer)
{
	GameplayCueContainer.RemoveCue(GameplayCueTag);
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_FromSpec(const FGameplayEffectSpecForRPC Spec,
	FPredictionKey PredictionKey)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(Spec,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted(const FGameplayTag GameplayCueTag,
	FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTag,EffectContext,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCuesExecuted(const FGameplayTagContainer GameplayCueTags,
	FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTags,EffectContext,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueExecuted_WithParams(const FGameplayTag GameplayCueTag,
	FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTag,GameplayCueParameters,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCuesExecuted_WithParams(
	const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey,
	FGameplayCueParameters GameplayCueParameters)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTags,GameplayCueParameters,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded(const FGameplayTag GameplayCueTag,
	FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext)
{
	// don't do anything anymore
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueAdded_WithParams(const FGameplayTag GameplayCueTag,
	FPredictionKey PredictionKey, FGameplayCueParameters Parameters)
{
	// don't do anything anymore
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(
	const FGameplayEffectSpecForRPC& Spec, FPredictionKey PredictionKey)
{
	// don't do anything anymore
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams(
	const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters)
{
	// don't do anything anymore
}

void UNpAbilitySystemComponent::NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams(
	const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey,
	FGameplayCueParameters GameplayCueParameters)
{
	// don't do anything anymore
}

void UNpAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag,
	FGameplayEffectContextHandle EffectContext)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTag,EffectContext,NetworkPredictionProxy.GetPendingFrame())
		,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::ExecuteGameplayCue(const FGameplayTag GameplayCueTag,
	const FGameplayCueParameters& GameplayCueParameters)
{
	ReplicatedCueExecutions.AddCueExecution(FGameplayCueExecution(GameplayCueTag,GameplayCueParameters,NetworkPredictionProxy.GetPendingFrame())
			,NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::SendCueRPC(const FGameplayCueExecution& Execution)
{
	switch (Execution.ExecutionType)
	{
	case ECueExecutionType::ENone:
		{
			return;
		}
	case ECueExecutionType::ESpec:
		{
			NetMulticast_GameplayCueExecuted_FromSpec(Execution.SpecExecution);
			break;
		}
	case ECueExecutionType::EParams:
		{
			NetMulticast_GameplayCueExecuted_FromParams(Execution.ParamsExecution);
			break;
		}
	case ECueExecutionType::EMultiParams:
		{
			NetMulticast_GameplayCueExecuted_FromParamsMulti(Execution.MultiParamsExecution);
			break;
		}
	case ECueExecutionType::EEffect:
		{
			NetMulticast_GameplayCueExecuted_FromEffect(Execution.EffectExecution);
			break;
		}
	case ECueExecutionType::EMultiEffect:
		{
			NetMulticast_GameplayCueExecuted_FromEffectMulti(Execution.MultiEffectExecution);
			break;
		}
	}
}

void UNpAbilitySystemComponent::NetMulticast_GameplayCueExecuted_FromEffectMulti_Implementation(
	const FCueExecutionMulti_EffectContext& Execution)
{
	// authority already dispatched the cue before sending it
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority)
	{
		return;
	}
	ReplicatedCueExecutions.OnExecutionReceived(FGameplayCueExecution(Execution),NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_GameplayCueExecuted_FromEffect_Implementation(
	const FCueExecution_EffectContext& Execution)
{
	// authority already dispatched the cue before sending it
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority)
	{
		return;
	}
	ReplicatedCueExecutions.OnExecutionReceived(FGameplayCueExecution(Execution),NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_GameplayCueExecuted_FromParamsMulti_Implementation(
	const FCuesExecutionMulti_Params& Execution)
{
	// authority already dispatched the cue before sending it
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority)
	{
		return;
	}
	ReplicatedCueExecutions.OnExecutionReceived(FGameplayCueExecution(Execution),NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_GameplayCueExecuted_FromParams_Implementation(
	const FCueExecution_Params& Execution)
{
	// authority already dispatched the cue before sending it
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority)
	{
		return;
	}
	ReplicatedCueExecutions.OnExecutionReceived(FGameplayCueExecution(Execution),NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::NetMulticast_GameplayCueExecuted_FromSpec_Implementation(
	const FCueExecution_Spec& Execution)
{
	// authority already dispatched the cue before sending it
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_Authority)
	{
		return;
	}
	ReplicatedCueExecutions.OnExecutionReceived(FGameplayCueExecution(Execution),NetworkPredictionProxy.GetCachedNetRole());
}

void UNpAbilitySystemComponent::FinalizeCues(const FActiveCueSyncDataContainer& CuesSyncData)
{
	FActiveCueSyncDataContainer ActiveCueSyncData = CuesSyncData;
	// auto proxy does not replicate cue params if it comes from an effect.
	// server sets the activating effect handle to index none if effect is not active before sending the data.
	
	if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_AutonomousProxy)
	{
		for (auto& ActiveCue : ActiveCueSyncData.ActiveCues)
		{
			// if our data is supposed to come from an effect and it's not filled yet, fill it.
			if (ActiveCue.EffectHandle != INDEX_NONE && !ActiveCue.Parameters.EffectContext.IsValid())
			{
				FActiveGameplayEffectHandle Handle;
				Handle.SetSyncedHandle(this, ActiveCue.EffectHandle);
				const FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(Handle);
				if (ActiveEffect != nullptr )
				{
					UAbilitySystemGlobals::Get().InitGameplayCueParameters_GESpec(ActiveCue.Parameters, ActiveEffect->Spec);
				}
			}
		}
	}
	// ToDo: @Kai investigate if these need to be ignored on dedicated server?
	TArray<FActiveCueSyncData> AddedCues;
	TArray<FActiveCueSyncData> RemovedCues;
	FActiveCueSyncDataContainer::GetCuesDifference(ActiveCueSyncData,LastSyncedCues,AddedCues,RemovedCues);
	for (FActiveCueSyncData& CueToAdd : AddedCues)
	{
		InvokeGameplayCueEvent(CueToAdd.GameplayCueTag,EGameplayCueEvent::OnActive,CueToAdd.Parameters);
		// maybe should check if LastSyncedCues had this specific tag even if not the specific  tag + ID combo?
		// original code checks if tag existed before , if it didn't it calls while active.
		InvokeGameplayCueEvent(CueToAdd.GameplayCueTag,EGameplayCueEvent::WhileActive,CueToAdd.Parameters);
	}
	for (FActiveCueSyncData& CueToRemove : RemovedCues)
	{
		InvokeGameplayCueEvent(CueToRemove.GameplayCueTag,EGameplayCueEvent::Removed,CueToRemove.Parameters);
	}
	
	LastSyncedCues = ActiveCueSyncData;

	// now dispatch the execution cues. 
	
	int32 Frame = NetworkPredictionProxy.GetPendingFrame();
	int32 FixedTickMS = 16;
	
	if (CachedManager.IsValid(false))
	{
		if (NetworkPredictionProxy.GetCachedNetRole() == ROLE_SimulatedProxy)
		{
			Frame = CachedManager->GetFixedTickState().Interpolation.ToFrame;
		}
		FixedTickMS = CachedManager->GetFixedTickState().FixedStepMS;
	}
	check(FixedTickMS > 0); // should never tick step of 0 or less

	// Only Sim proxies now uses prune frames
	float Ping = 16; // less than 16MS , meaning living above the server is very rare.
	const AController* Controller = GetOwnerActor()->GetInstigatorController();
	if (Controller && Controller->PlayerState)
	{
		Ping = FMath::Max(16,Controller->PlayerState->GetPingInMilliseconds());
	}
	// min prune frames is 2 if someone has ping <= fixed ticks ms. would be <=1. 2 minimum for safety
	int32 PruneFrames = FMath::Max(2,(Ping * 2) / FixedTickMS);
	ReplicatedCueExecutions.DispatchCues(this,Frame,NetworkPredictionProxy.GetCachedNetRole(),PruneFrames);
}
#pragma endregion

#pragma region Synced Target

bool UNpAbilitySystemComponent::IsSyncedTargetValid() const
{
	return IsValid(SyncedTarget.Target) && SyncedTarget.TargetType.IsValid();
}
void UNpAbilitySystemComponent::SetSyncedTarget(AActor* Target, FGameplayTag TargetType)
{
	AActor* Avatar = GetAvatarActor_Direct();
	
	if (!IsValid(Target) || !TargetType.IsValid())
	{
		//Log Error Here
		return;
	}

	if (!Avatar || (Avatar == Target) || (GetAvatarActor_Direct()->GetLocalRole() == ROLE_SimulatedProxy))
	{
		return;
	}

	FSyncedTarget OldData = SyncedTarget;
	
	SyncedTarget.Target = Target;
	SyncedTarget.TargetType = TargetType;

	OnTargetChanged.Broadcast(OldData, SyncedTarget);
}

void UNpAbilitySystemComponent::ClearSyncedTarget()
{
	SyncedTarget.Target = nullptr;
	SyncedTarget.TargetType = FGameplayTag();
}
#pragma endregion

#pragma region Projectiles
ASyncedProjectileBase* UNpAbilitySystemComponent::SpawnProjectile(TSubclassOf<ASyncedProjectileBase> Class,
	const FVector& Location, const FVector& Direction)
{
	if (ProjectilesSimulator && IsValid(Class))
	{
		return ProjectilesSimulator->SpawnProjectile(Class, Location, Direction.GetSafeNormal());
	}
	return nullptr;
}

void UNpAbilitySystemComponent::DestroyProjectile(ASyncedProjectileBase* Projectile)
{
	if (ProjectilesSimulator)
	{
		ProjectilesSimulator->DestroyProjectile(Projectile);
	}
}

ASyncedProjectileBase* UNpAbilitySystemComponent::GetProjectileInstanceByID(const int32& ProjectileID) const
{
	if (ProjectilesSimulator)
	{
		return  ProjectilesSimulator->GetProjectileInstanceByID(static_cast<uint32>(ProjectileID));
	}
	return nullptr;
}
#pragma endregion




AController* UNpAbilitySystemComponent::TryGetOwningController()
{
	if (IsValid(CachedOwningController))
	{
		return CachedOwningController;
	}
	AActor* CurrentOwner = GetOwner();
	while (CurrentOwner)
	{
		if (AController* Controller = Cast<AController>(CurrentOwner))
		{
			// Found the controller
			CachedOwningController = Controller;
			return Controller;
		}
		CurrentOwner = CurrentOwner->GetOwner();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE