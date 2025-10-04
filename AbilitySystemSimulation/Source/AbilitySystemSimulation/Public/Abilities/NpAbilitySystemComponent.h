// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "Components/ActorComponent.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Engine/NetSerialization.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "AttributeSet.h"
#include "EngineDefines.h"
#include "EngineDefines.h"
#include "EnhancedInputComponent.h"
#include "GameplayPrediction.h"
#include "GameplayCueInterface.h"
#include "GameplayAbilitySpec.h"
#include "GameplayEffect.h"
#include "NetworkPredictionPlayerControllerComponent.h"
#include "NetworkPredictionProxy.h"
#include "NetworkPredictionSimulation.h"
#include "NetworkPredictionStateTypes.h"
#include "NetworkPredictionReplicationProxy.h"
#include "Abilities/GameplayAbility.h"
#include "DataTypes/AbilitySimulationDataTypes.h"
#include "DataTypes/EffectsDataTypes.h"
#include "DataTypes/TargetingTypes/TargetingDataTypes.h"
#include "ProjectilesSimulator/ProjectilesSimulator.h"
#include "NpAbilitySystemComponent.generated.h"


class UNpAbilityInputSender;
struct FAttributeSetSyncDataCollection;
struct FActiveEffectSyncData;
struct FActiveEffectSyncDataContainer;
class UNetMontageSimulator;
class UInputMappingContext;
class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProducInputEvent);
DECLARE_MULTICAST_DELEGATE_OneParam(FSyncedMontageEvent, UAnimMontage*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSyncedTargetSet, const FSyncedTarget&, const FSyncedTarget&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAbilityInputActionEvent,const UInputAction* , const ETriggerEvent&);

using AbilitySystemStateTypes = TNetworkPredictionStateTypes<FAbilitySimInputCmd, FAbilitySimSyncState, FAbilitySimAuxState>;

/**
 *	just like the basic ability system component , UNpAbilitySystemComponent is responsible for abilities, effects, cues , tag , attributes etc.
 *	what this adds is the roll-back and re-simulate functionality the entire component to a specific state, by rolling back everything it is responsible for.
 *
 *	almost all overrides is to effectively make this component not care about networking, to let the network prediction plugin take care of it.
 *	I would have hoped i didn't need to change any code in the original GAS plugins except making functions virtual , but that is not possible for structs
 *	so this component tries its best to only override functions for the base class and change what needs to be changed.
 *
 *	everything related to setup, attributes and how ASC worked before is still the exact same.
 *
 *	The main "trick" to get GAS and NPP to work together was allowing the ability system component to function just like it does
 *	and we just fill the sync state for NPP at the end of every simulation tick. and we restore the entire component to a state from the past.
 *	FillSyncState() is responsible for getting the current state data from ASC.
 *
 *	the functions related to getting the data for the abilities is in AbilitySimulationLibrary.h, rest of structs such as active effect
 *	the data extraction happen in the sync data constructor such as FActiveEffectSyncDataContainer.
 *	abilities data extraction is not in the constructor as well for no good reason. just because. probably should be unified as the others. 
 *
 *	The component also manager 2 other "simulators" , this is an idea can be extended to allow users to create "mini" simulation that work within ability simulation
 *	the ones implemented is a montage simulator and a projectile simulator. their data is current added to ability sync state hard coded, but they can use
 *	something like mover data collections.(future improvement?)
 *
 *  Another addition is the use of input actions directly. the component will keep track of input action events on the controlling player, and fill its inputs state
 *  then send it to server.
 * 
 */
UCLASS(ClassGroup=(AbilitySystem), meta=(BlueprintSpawnableComponent))
class ABILITYSYSTEMSIMULATION_API UNpAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_UCLASS_BODY()

	friend class UNpGameplayAbility;
	
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	bool GetIsRestoringFrame() const {return bIsRestoringFrame;}

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	UNetMontageSimulator* GetMontagePlayer() {return MontagePlayer;}

	UFUNCTION(BlueprintPure, Category = "AbilitySimulation")
	UAnimMontage* GetPlayingSyncedMontage(float& CurrentTime) const;

	UFUNCTION(BlueprintPure, Category = "AbilitySimulation")
	bool IsReSimulating() const;

	UFUNCTION(BlueprintPure, Category = "AbilitySimulation")
	FTransform GetMeshRelativeTransform() const {return MeshRelativeTransform;}

	/**
	 * if using blueprint only and want to just have ASC in the pawn, you can call this OnControllerChanged event to refresh
	 * Avatar actor data such as player controller etc..
	 */
	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void RefreshAvatarActor(AActor* InAvatar);
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=DefaultData)
	TArray<TSubclassOf<UAttributeSet>> DefaultAttributes;
	
	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=DefaultData)
	TArray<TSubclassOf<UNpGameplayAbility>> DefaultAbilities;

	UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category=DefaultData)
	TArray<TSubclassOf<UGameplayEffect>> DefaultEffects;
	
	FOnAbilityInputActionEvent OnInputActionEvent;

	ENetRole GetCachedSimNetRole() const;




#pragma region Synced Montage

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void PlaySyncedMontage(UAnimMontage* AnimMontage,float StartTime, float PlayRate, FName SectionName, float InRootMotionScale);

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void StopSyncedMontage(UAnimMontage* Montage, bool bInterrupted = false);

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void PauseCurrentSyncedMontage();

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void ResumeCurrentSyncedMontage();

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void JumpSyncedMontageToSection(UAnimMontage* Montage,FName SectionName);

	UFUNCTION(BlueprintCallable, Category = "AbilitySimulation")
	void SetCurrentMontagePlayRate(float PlayRate);

	// This must return 1 for no change in play rate
	virtual float GetMontagePlayRateMultiplier() {return 1.f;}
	// Anim Montage Events
	FSyncedMontageEvent OnMontageStarted;
	FSyncedMontageEvent OnMontageCompleted;
	FSyncedMontageEvent OnMontageCanceled;
	FSyncedMontageEvent OnMontageInterrupted;
	FSyncedMontageEvent OnMontageBlendOut;

private:
	UPROPERTY(Transient)
	TObjectPtr<UNetMontageSimulator> MontagePlayer = nullptr;
#pragma endregion
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Input Handling
public:

	// For Now Ability input binding is disabled
	// will add input action blocking in the future, can just block an input action from activating any abilities.
	// if you want to block/unblock specific ability, just use a tag now.
	virtual bool IsAbilityInputBlocked(int32 InputID) const override
	{
		return false;
	}
	
	UPROPERTY(EditDefaultsOnly,Category=AbilitySimulationInput)
	bool bSendCameraLocation = false;

	UPROPERTY(EditDefaultsOnly,Category=AbilitySimulationInput)
	bool bSendMouseScreenLocation = false;
	
	UFUNCTION(BlueprintCallable,Category=AbilitySimulationInput)
	void AddMappingContext(const UInputMappingContext* MappingContext);
	
	UFUNCTION(BlueprintCallable,Category=AbilitySimulationInput)
    void RemoveMappingContext(const UInputMappingContext* MappingContext);

	UFUNCTION(BlueprintCallable,Category=AbilitySimulationInput)
	void SimulateInputTrigger(UInputAction* InputAction, ETriggerEvent TriggerEvent);

	
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FAbilityInputActionState GetSyncedInputActionState(const UInputAction* InputAction) const;
	
	/**
	* Get Synced Custom Input Defined by default in CustomInput variable
	* This gets the values from latest input command.
	*/ 
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FInstancedStruct GetSyncedCustomInput() const;
	/**
	* Get Synced Custom Input Defined by default in CustomInput variable directly from an input command
	* variant to use if you have access to the specific input cmd you want to use
	*/ 
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FInstancedStruct GetCustomInputFromCommand(const FAbilitySimInputCmd& InputCmd) const;
	/**
	* do not directly use this in code to get values only set values in it. use GetCustomInputFromCommand and GetLatestCustomInput
	* to get the input value, you should update this in Pre/Post produce input events.
	* Use PreProduceInput to fill the data , and Post produce to clear any 1 frame data
	*/ 
	UPROPERTY(EditAnywhere,BlueprintReadWrite,Category=AbilitySimulationInput)
	FInstancedStruct CustomInput;
	/**
	 * Get Mouse Screen Location synced between local client and server
	 * should be used for targeting or any ability system code if mouse location or its world projection is needed
	 * NOTE : only synced if bSendMouseScreenLocation is true by default
	 */
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FVector2D GetSyncedMouseScreenLocation() const;

	/**
	 * Get Camera World Location synced between local client and server
	 * should be used for targeting or any ability system code if camera location value is needed
	 * NOTE : only synced if bSendCameraLocation is true by default
	 */
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FVector GetSyncedCameraWorldLocation() const;
	/**
	 * Get Screen Projection Data synced between local client and server
	 * should be used for targeting or any ability system code if de/projection from screen is needed
	 * NOTE : only synced if bSendScreenProjection is true by default
	 */
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FSyncedScreenProjection GetSyncedScreenProjection() const;
	/**
	* this relies on having a valid mover component on the avatar actor,
	* will try to fall back to player view if no mover
	* NOTE : Due to mover not having a public function to give read access to pending input cmd
	* we are currently getting control rotation of previous frame, not the best..
	*/ 
	UFUNCTION(BlueprintPure, Category = AbilitySimulationInput)
	FRotator GetSyncedControlRotation() const;
	/**
	 * Delegate triggered before we fill the input command,
	 * use this to effect your custom input instanced struct before sending it
	 * 
	 * This is a Local Controlling player event only, Does not trigger on server on sim proxies
	 */
	UPROPERTY(BlueprintAssignable)
	FOnProducInputEvent OnPreProduceInput;
	/**
	 * Delegate triggered After we fill the input command,
	 * use this to effect your custom input instanced struct After sending it
	 * to clear any 1 frame inputs for example.
	 * 
	 * This is a Local Controlling player event only, Does not trigger on server on sim proxies
	 */
	UPROPERTY(BlueprintAssignable)
	FOnProducInputEvent OnPostProduceInput;
	
private:

 // Local Only Variables
	UPROPERTY(transient)
	TArray<const UInputAction*> ActiveInputActions;
	UPROPERTY(transient)
	TMap<const UInputAction*,FAbilityInputActionState> LocalInputActionStates;
	UPROPERTY(transient)
	TArray<uint8> LocalActiveMappingContexts;
	UPROPERTY(Transient)
	TArray<TObjectPtr<const UInputMappingContext>> LoadedMappings;
	void LoadInputsInMemory();
	TArray<const UInputAction*> GetInputActionsFromMappingIndexes(const TArray<uint8>& MappingIndexes) const;
	const UInputAction* GetInputActionAtIndex(const TArray<uint8>& MappingIndexes,const uint8& Index) const;
	
	UFUNCTION()
    bool TryActivateAbilitiesFromInput(const UInputAction* InputAction,const ETriggerEvent& TriggerEvent);
    UFUNCTION()
    void UpdateInputActionState(const FInputActionInstance& ActionInstance);
	
    void HandleSimTickInputActionsEvents(const FAbilitySimInputCmd& InputCmd);
    void UpdateActiveInputActions();
	void ClearOneShotInputStates();
	bool BindInputActionsFromContext(const UInputMappingContext* InputContext);
	
#pragma endregion
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Network Prediction API 
public:
	void ProduceInput(const int32 DeltaTimeMS, FAbilitySimInputCmd* Cmd);
	void RestoreFrame(const FAbilitySimSyncState* SyncState, const FAbilitySimAuxState* AuxState);
	void FinalizeFrame(const FAbilitySimSyncState* SyncState, const FAbilitySimAuxState* AuxState);
	void FinalizeSmoothingFrame(const FAbilitySimSyncState* SyncState, const FAbilitySimAuxState* AuxState);
	void InitializeSimulationState(FAbilitySimSyncState* OutSync, FAbilitySimAuxState* OutAux);
	void SimulationTick(const FNetSimTimeStep& TimeStep, const TNetSimInput<AbilitySystemStateTypes>& SimInput, const TNetSimOutput<AbilitySystemStateTypes>& SimOutput);
	// Invoke the ServerRPC, called from UNetworkPredictionWorldManager via the TServerRPCService.
	//Not Called Anymore but still needs to be declared for now.
	void CallServerRPC();

	float GetFixedStepMs() const ;
	
	UFUNCTION(BlueprintPure,Category=NetworkPrediction)
	float GetSyncedInterpolationTimeMS() const ;

protected:
	
	void InternalSimulationTick(const FAbilitySystemTimeStep& TimeStep,const FAbilitySystemTickStartData& TickStartData,OUT FAbilitySystemTickEndData& TickEndData);
	
	// Classes must initialize the NetworkPredictionProxy (register with the NetworkPredictionSystem) here. EndPlay will unregister.
	void InitializeNetworkPredictionProxy();

	// Finalizes initialization when NetworkRole changes. Does not need to be overridden.
	void InitializeForNetworkRole(ENetRole Role, const bool bHasNetConnection,UNetworkPredictionPlayerControllerComponent* RPCHandler);

	// Helper: Checks if the owner's role has changed and calls InitializeForNetworkRole if necessary.
	bool CheckOwnerRoleChange();

	// Proxy to interface with the NetworkPrediction system
	UPROPERTY(Replicated, transient)
	FNetworkPredictionProxy NetworkPredictionProxy;

	// ReplicationProxies are just pointers to the data/NetSerialize functions within the NetworkSim
	UPROPERTY()
	FReplicationProxy ReplicationProxy_ServerRPC;

private:

	UPROPERTY(Replicated, transient)
	FReplicationProxy ReplicationProxy_Autonomous;

	UPROPERTY(Replicated, transient)
	FReplicationProxy ReplicationProxy_Simulated;

	UPROPERTY(Replicated, transient)
	FReplicationProxy ReplicationProxy_Replay;

protected:
	
	FReplicationProxySet GetReplicationProxies()
	{
		return FReplicationProxySet{ &ReplicationProxy_ServerRPC, &ReplicationProxy_Autonomous, &ReplicationProxy_Simulated, &ReplicationProxy_Replay };
	}

public:
	int32 GetCurrentSimFrame();
	bool ReadPendingSyncState(OUT FAbilitySimSyncState& OutSyncState);
	bool WritePendingSyncState(const FAbilitySimSyncState& SyncStateToWrite);
	bool ReadPendingInputCmd(OUT FAbilitySimInputCmd& OutInputCmd);
	
	
	// UObject interface
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	// End UObject interface

#pragma endregion
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Replication
	virtual bool IsOwnerActorAuthoritative() const override;

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void PreNetReceive() override;
	virtual void PostNetReceive() override;
	virtual void ReadyForReplication() override;

	virtual bool ShouldRecordMontageReplication() const override;

	
#pragma endregion
	// ----------------------------------------------------------------------------------------------------------------
#pragma region 	Attributes
	
	virtual void SetSpawnedAttributes(const TArray<UAttributeSet*>& NewAttributeSet) override;
	virtual  void AddSpawnedAttribute(UAttributeSet* Attribute) override;
	virtual void RemoveSpawnedAttribute(UAttributeSet* Attribute) override;
	virtual void RemoveAllSpawnedAttributes() override;
	virtual void OnSpawnedAttributesEndPlayed(AActor* InActor, EEndPlayReason::Type EndPlayReason) override;
	virtual  void ApplyModToAttribute(const FGameplayAttribute &Attribute, TEnumAsByte<EGameplayModOp::Type> ModifierOp, float ModifierMagnitude) override;

	// Added
	UAttributeSet*	GetOrCreateAttributeSubobject_Mutable(TSubclassOf<UAttributeSet> AttributeClass);
	UAttributeSet*	GetAttributeSubobject_Mutable(TSubclassOf<UAttributeSet> AttributeClass);
#pragma endregion Attributes
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Gameplay Tags

	FORCEINLINE virtual void SetTagMapCount(const FGameplayTag& Tag, int32 NewCount) override
	{
		if (GetIsRestoringFrame())
		{
			return;
		}
		Super::SetTagMapCount(Tag, NewCount);
	}
	FORCEINLINE virtual void UpdateTagMap(const FGameplayTag& BaseTag, int32 CountDelta) override
	{
		if (GetIsRestoringFrame())
		{
			return;
		}
		Super::UpdateTagMap(BaseTag, CountDelta);
	}
	FORCEINLINE virtual void UpdateTagMap(const FGameplayTagContainer& Container, int32 CountDelta) override
	{
		if (GetIsRestoringFrame())
		{
			return;
		}
		Super::UpdateTagMap(Container, CountDelta);
	}
	// All Tags Are just replicated now, there are no special replicated/minimal replicated tags. now there special NonReplicated Tags
	// Had To be added in Parent Class Directly.
	FORCEINLINE virtual void AddReplicatedLooseGameplayTag(const FGameplayTag& GameplayTag) override
	{
		AddLooseGameplayTag(GameplayTag);
	}

	FORCEINLINE virtual void AddReplicatedLooseGameplayTags(const FGameplayTagContainer& GameplayTags) override
	{
		AddLooseGameplayTags(GameplayTags);
	}

	FORCEINLINE virtual void RemoveReplicatedLooseGameplayTag(const FGameplayTag& GameplayTag) override
	{
		RemoveLooseGameplayTag(GameplayTag);
	}

	FORCEINLINE virtual void RemoveReplicatedLooseGameplayTags(const FGameplayTagContainer& GameplayTags) override
	{
		RemoveLooseGameplayTags(GameplayTags);
	}

	FORCEINLINE virtual void SetReplicatedLooseGameplayTagCount(const FGameplayTag& GameplayTag, int32 NewCount) override
	{
		SetLooseGameplayTagCount(GameplayTag, NewCount);
	}
	
	FORCEINLINE virtual void AddMinimalReplicationGameplayTag(const FGameplayTag& GameplayTag) override
	{
		AddLooseGameplayTag(GameplayTag);
	}

	FORCEINLINE virtual void AddMinimalReplicationGameplayTags(const FGameplayTagContainer& GameplayTags) override
	{
		AddLooseGameplayTags(GameplayTags);
	}

	FORCEINLINE virtual void RemoveMinimalReplicationGameplayTag(const FGameplayTag& GameplayTag) override
	{
		RemoveLooseGameplayTag(GameplayTag);
	}

	FORCEINLINE virtual void RemoveMinimalReplicationGameplayTags(const FGameplayTagContainer& GameplayTags) override
	{
		RemoveLooseGameplayTags(GameplayTags);
	}
	
	virtual void BlockAbilitiesWithTags(const FGameplayTagContainer& Tags) override
	{
		if (GetIsRestoringFrame())
		{
			return;
		}
		Super::BlockAbilitiesWithTags(Tags);
	}

	virtual void UnBlockAbilitiesWithTags(const FGameplayTagContainer& Tags) override
	{
		if (GetIsRestoringFrame())
		{
			return;
		}
		Super::UnBlockAbilitiesWithTags(Tags);
	}
#pragma endregion 
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Gemplay Effects
public:
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToTarget(const FGameplayEffectSpec& GameplayEffect, UAbilitySystemComponent *Target
		, FPredictionKey PredictionKey=FPredictionKey()) override;
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectSpecToSelf(const FGameplayEffectSpec& GameplayEffect
		, FPredictionKey PredictionKey = FPredictionKey()) override;
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectToTarget(UGameplayEffect *GameplayEffect, UAbilitySystemComponent *Target
		, float Level = UGameplayEffect::INVALID_LEVEL, FGameplayEffectContextHandle Context = FGameplayEffectContextHandle()
		, FPredictionKey PredictionKey = FPredictionKey()) override;
	virtual FActiveGameplayEffectHandle ApplyGameplayEffectToSelf(const UGameplayEffect *GameplayEffect, float Level
		, const FGameplayEffectContextHandle& EffectContext, FPredictionKey PredictionKey = FPredictionKey()) override;
	virtual bool RemoveActiveGameplayEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove = -1) override;
	virtual FActiveGameplayEffectHandle SetActiveGameplayEffectInhibit(FActiveGameplayEffectHandle&& ActiveGEHandle, bool bInhibit, bool bInvokeGameplayCueEvents) override;

	/** Gets time remaining for all effects that match query */
	virtual TArray<float> GetActiveEffectsTimeRemaining(const FGameplayEffectQuery& Query) const override;

	/** Gets both time remaining and total duration  for all effects that match query */
	virtual TArray<TPair<float,float>> GetActiveEffectsTimeRemainingAndDuration(const FGameplayEffectQuery& Query) const override;
#pragma endregion Gemplay Effects
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Abilities
	//-----------------------------------------------------------------------------------------------------------------
	//Properties 
	//-----------------------------------------------------------------------------------------------------------------
	int32 SyncedAbilitiesHandlesCount = 0;
	//-----------------------------------------------------------------------------------------------------------------
	//Function Overrides
	//-----------------------------------------------------------------------------------------------------------------
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	
	virtual bool GetShouldTick() const override;
	
	virtual FGameplayAbilitySpecHandle GiveAbility(const FGameplayAbilitySpec& AbilitySpec) override;
	
	virtual FGameplayAbilitySpecHandle GiveAbilityAndActivateOnce(FGameplayAbilitySpec& AbilitySpec, const FGameplayEventData* GameplayEventData) override;
	
	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
	virtual void ClearAbility(const FGameplayAbilitySpecHandle& Handle) override;
	
	virtual void OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec) override;
	
	virtual void CheckForClearedAbilities() override;
	
	virtual UGameplayAbility* CreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability) override;
	
	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled) override;
	
	virtual bool TryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, bool bAllowRemoteActivation = true) override;
	
	virtual bool InternalTryActivateAbility(FGameplayAbilitySpecHandle AbilityToActivate, FPredictionKey InPredictionKey = FPredictionKey()
		, UGameplayAbility ** OutInstancedAbility = nullptr, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate = nullptr
		, const FGameplayEventData* TriggerEventData = nullptr) override;
	
	virtual bool TriggerAbilityFromGameplayEvent(FGameplayAbilitySpecHandle AbilityToTrigger, FGameplayAbilityActorInfo* ActorInfo
		, FGameplayTag Tag, const FGameplayEventData* Payload, UAbilitySystemComponent& Component) override;
	
	virtual void MonitoredTagChanged(const FGameplayTag Tag, int32 NewCount) override;
	
	virtual void CancelAbility(UGameplayAbility* Ability) override;	
	
	virtual void CancelAbilityHandle(const FGameplayAbilitySpecHandle& AbilityHandle) override;
	
	virtual void CancelAbilities(const FGameplayTagContainer* WithTags=nullptr, const FGameplayTagContainer* WithoutTags=nullptr
		, UGameplayAbility* Ignore=nullptr) override;
	
	virtual void CancelAllAbilities(UGameplayAbility* Ignore=nullptr) override;
	
	virtual void DestroyActiveState() override;

	
#pragma endregion Abilities
	// ----------------------------------------------------------------------------------------------------------------
#pragma region Functions Added For Network Prediction Plugin Support
private:
	void TickAbilities(const FAbilitySystemTimeStep& TimeStep);
	void RestoreAbilities(const FActivatableAbilitiesCollection& AuthorityActivatableAbilities);
	void RestoreExistingAbility(FGameplayAbilitySpec* AbilitySpec,const FActivatableAbilitySyncState& AuthoritySyncState);
	static void RestoreAbilityInstance(UNpGameplayAbility* AbilityInstance, const FActiveAbilityInstanceData& AuthorityState);
	//-----------------------------------------------------------------------------------------------------------------
	//Additional Functions
	//-----------------------------------------------------------------------------------------------------------------
	void ForceGiveAbility(const FActivatableAbilitySyncState& AbilityToAdd);
	void OnForceGiveAbility(FGameplayAbilitySpec& AbilitySpec);
	UNpGameplayAbility* ForceCreateNewInstanceOfAbility(FGameplayAbilitySpec& Spec, const UGameplayAbility* Ability);

	void ForceClearAbility(const FGameplayAbilitySpec& AbilitySpec);
	void OnForceRemoveAbility(FGameplayAbilitySpec& AbilitySpec);
	void CheckForClearedAbilitiesAfterForceRemove();

	void ForceCreateActivateAbility(FGameplayAbilitySpec* Spec,const FActiveAbilityInstanceData& InstanceData);
	UNpGameplayAbility* ForceCreateAbilityInstance(FGameplayAbilitySpec* Spec);
	void ForceActivateAbility(UNpGameplayAbility* AbilityInstance,FGameplayAbilitySpec* Spec,const FActiveAbilityInstanceData& InstanceData);
	void ForceCancelAbility(const FGameplayAbilitySpecHandle& Handle,UNpGameplayAbility* AbilityInstance);
	void NotifyAbilityForceEnded(const FGameplayAbilitySpecHandle& Handle,UNpGameplayAbility* AbilityInstance,const bool& bWasCancelled);

	//void TickActiveGameplayEffects()
	void RestoreGameplayEffects (const FActiveEffectSyncDataContainer& AuthorityActiveEffects);
	void RestoreExitingEffect(const FActiveEffectSyncData& AuthorityData, FActiveGameplayEffect* ActiveEffect);
	void ForceRemoveEffect(const FActiveGameplayEffectHandle& Handle);
	void ForceApplyEffect(const FActiveEffectSyncData& AuthorityData);
	void RestoreAttributeSets(const FAttributeSetSyncDataCollection& AuthoritySets);
	void RestoreExistingAttributeSet(const FAttributeSetSyncData& AuthoritySet,UAttributeSet* AttributeSet);
	void RestoreTags(const FSyncedGameplayTagCount& InBlockedAbilityTags,const FSyncedGameplayTagCount& InGameplayTags);
	void FinalizeSimulatedAttributes(const FAttributeSetSyncDataCollection& AttributesData);
	void FinalizeSimulatedTags(const FSyncedGameplayTagCount& InBlockedAbilityTags,const FSyncedGameplayTagCount& InGameplayTags);
	void FinalizeSimulatedAttributeSet(const FAttributeSetSyncData& AuthoritySet,UAttributeSet* AttributeSet);
	void FillSyncState(FAbilitySimSyncState& SyncState);
public:
	virtual float GetCurrentSimulationTimeMS() const override;
#pragma endregion

#pragma region Cues
private:
	void RestoreCues(const FActiveCueSyncDataContainer& AuthorityCueSyncData);
	virtual void AddGameplayCue_Internal(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters, FActiveGameplayCueContainer& GameplayCueContainer) override;
	virtual void RemoveGameplayCue_Internal(const FGameplayTag GameplayCueTag, FActiveGameplayCueContainer& GameplayCueContainer) override;
	void FinalizeCues(const FActiveCueSyncDataContainer& CuesSyncData);
	UPROPERTY()
	FActiveCueSyncDataContainer LastSyncedCues;
public:
	// Overriding these function so we don't call RPCs anymore, Cues addition and their events are in the sync state.
	// Cue execution is a Net Prediction Cue.See FAbilitySystemModelDef.
	void NetMulticast_InvokeGameplayCueExecuted_FromSpec(const FGameplayEffectSpecForRPC Spec, FPredictionKey PredictionKey) override;
	
	void NetMulticast_InvokeGameplayCueExecuted(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext) override;
	
	void NetMulticast_InvokeGameplayCuesExecuted(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext) override;
	
	void NetMulticast_InvokeGameplayCueExecuted_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters) override;
	
	void NetMulticast_InvokeGameplayCuesExecuted_WithParams(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters) override;
	
	void NetMulticast_InvokeGameplayCueAdded(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayEffectContextHandle EffectContext) override;
	
	void NetMulticast_InvokeGameplayCueAdded_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters Parameters) override;
	
	void NetMulticast_InvokeGameplayCueAddedAndWhileActive_FromSpec(const FGameplayEffectSpecForRPC& Spec, FPredictionKey PredictionKey) override;
	
	void NetMulticast_InvokeGameplayCueAddedAndWhileActive_WithParams(const FGameplayTag GameplayCueTag, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters) override;
	
	void NetMulticast_InvokeGameplayCuesAddedAndWhileActive_WithParams(const FGameplayTagContainer GameplayCueTags, FPredictionKey PredictionKey, FGameplayCueParameters GameplayCueParameters) override;

	virtual void ExecuteGameplayCue(const FGameplayTag GameplayCueTag, FGameplayEffectContextHandle EffectContext = FGameplayEffectContextHandle()) override;
	virtual void ExecuteGameplayCue(const FGameplayTag GameplayCueTag, const FGameplayCueParameters& GameplayCueParameters) override;

	// called by ReplicatedCues Property whe cues are dispatched on the server
	UFUNCTION()
	void SendCueRPC(const FGameplayCueExecution& Execution);

	// these net Multicast function mirror the ones above, this is bit hacky to get it working making the other no longer RPCs,
	// they are being called by server and auto proxy because IsOwnerAuthoritative return true for both.
	// those add the cues to the list "ReplicatedCueExecutions" and server will send them after dispatching them in finalize frame.
	UFUNCTION(NetMulticast,Unreliable)
	void NetMulticast_GameplayCueExecuted_FromSpec(const FCueExecution_Spec& Execution);
	UFUNCTION(NetMulticast,Unreliable)
	void NetMulticast_GameplayCueExecuted_FromParams(const FCueExecution_Params& Execution);
	UFUNCTION(NetMulticast,Unreliable)
	void NetMulticast_GameplayCueExecuted_FromParamsMulti(const FCuesExecutionMulti_Params& Execution);
	UFUNCTION(NetMulticast,Unreliable)
	void NetMulticast_GameplayCueExecuted_FromEffect(const FCueExecution_EffectContext& Execution);
	UFUNCTION(NetMulticast,Unreliable)
	void NetMulticast_GameplayCueExecuted_FromEffectMulti(const FCueExecutionMulti_EffectContext& Execution);
	
	// this is used in finalize frame to check for added and removed cue and invoke their events
	// this guarantees the events triggering when cue gets received , client can predict it and would not call it twice during re-simulation

	// this variable itself is not replicated , unlike what its name says, at least not in the property replication way.
	// these are cue execution sent with RPCs, but client can predict them.
	FGameplayCueExecutionsContainer ReplicatedCueExecutions;
#pragma endregion

#pragma region Targting
public:
	UFUNCTION(BlueprintPure, Category = "AbilitySystemComponent|SyncedTarget")
	FSyncedTarget GetSyncedTarget() const {return SyncedTarget;}
	UFUNCTION(BlueprintPure, Category = "AbilitySystemComponent|SyncedTarget")
	virtual bool IsSyncedTargetValid() const;
	UFUNCTION(BlueprintCallable, Category = "AbilitySystemComponent|SyncedTarget")
	void SetSyncedTarget(AActor* Target , FGameplayTag TargetType);
	UFUNCTION(BlueprintCallable, Category = "AbilitySystemComponent|SyncedTarget")
	void ClearSyncedTarget();
	FOnSyncedTargetSet OnTargetChanged;

	//----Projectiles (technically they are part of targeting)---//

	//these have to be called from the simulation (such as an Ability) or a static event (e.g : entering or non-moving collision box), to be predicted.
	// Otherwise,Server has complete authority, this can be called from it to spawn and replicate a projectile.
	// and client will delete/re-create any projectiles it spawned/destroyed without server.
	// client can predict only his own projectiles (allowing clients to "visually, no collision" predict enemies projectiles is possible)
	UProjectilesSimulator* GetProjectilesSimulator() const {return ProjectilesSimulator;}
	UFUNCTION(BlueprintCallable,Category=Projectiles)
	ASyncedProjectileBase* SpawnProjectile(TSubclassOf<ASyncedProjectileBase> Class , const FVector& Location, const FVector& Direction); 
	UFUNCTION(BlueprintCallable,Category=Projectiles)
	void DestroyProjectile(ASyncedProjectileBase* Projectile);
	UFUNCTION(BlueprintCallable,Category=Projectiles)
	ASyncedProjectileBase* GetProjectileInstanceByID(const int32& ProjectileID) const;
private:
	UPROPERTY()
	FSyncedTarget SyncedTarget;
	UPROPERTY(Transient)
	TObjectPtr<UProjectilesSimulator> ProjectilesSimulator = nullptr;
#pragma endregion 

private:
	
	UPROPERTY(Transient)
	TObjectPtr<AController> CachedOwningController = nullptr;
	AController* TryGetOwningController();
	bool bIsRestoringFrame = false;
	UPROPERTY()
	TWeakObjectPtr<UNetworkPredictionWorldManager> CachedManager = nullptr;
	FAbilitySystemTimeStep LatestCachedTimeStep;
	FAbilitySystemTimeStep CurrentCachedTimeStep;
	FTransform MeshRelativeTransform;
};


