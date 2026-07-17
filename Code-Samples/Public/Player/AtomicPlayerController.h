// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "ProjectTypes/AtomicGameTypes.h"
#include "AtomicPlayerController.generated.h"

struct FAtomicBeltPlacementCell;
enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;
enum class EPlacementMode : uint8;
class AAtomicShipGrid;
class UAtomicPlayerPlacementComponent;
struct FInputActionInstance;
class UInputMappingContext;
class AAtomicPlayerCharacter;
class UInputAction;
class UAtomicInteractionComponent;

UENUM()
enum EAtomicInputSource : uint8 {
	MouseKeyboard,
	Gamepad
};

UENUM()
enum EAtomicRightStickMode : uint8 {
	Neutral,
	Yaw,
	Zoom
};


/**
 * The OwningConnection for any given Actor is the connection associated with that Actor's Owning PlayerController, if it has one.
 * NetConnection is used to determine who is in charge of the Actor (not the same as Authority).
 * 
 * NetPriority is used for bandwidth load balancing. Default of 1. Higher num = more bandwidth (ratio), relative to other actors.
 * 
 * SERVER -- has PlayerController for every player
 * CLIENT -- has Only its own PlayerController
 * Has Player's OwningNetConnection (how the server determines Ownership)
 * NetUpdateFrequency: (100 times/seconds) (Net Priority 3)
 */
UCLASS()
class ATOMICSTARLINES_API AAtomicPlayerController : public APlayerController {
	GENERATED_BODY()
	
public:
	AAtomicPlayerController();
	
	bool IsUsingGamepad() const;
	bool GetMoveWasLastInputValue() const { return bMoveWasLastInputValue; }
	
	void HandlePlacementConfirmed();
	
	//// RPC Data Rules ////////////////////////
	///
	//// DO SEND: Compact Intent, not full object state
	//// AAtomicShipGrid* TargetGrid, FName BuildingID, FIntVector AnchorCoord, EBuildingRotation
	///
	//// DO NOT SEND: BuildingDefinition, GridCell array, bool functions, AActor* PlacementInstigator
	///
	//// SERVER FUNCTIONS ////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	UFUNCTION(Server, Reliable)
	void Server_RequestPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const AAtomicShipGrid* TargetGrid);
	
	UFUNCTION(Server, Reliable)
	void Server_RequestPlaceBeltLine(const FName BeltID, const TArray<FAtomicBeltPlacementCell>& BeltCells, const AAtomicShipGrid* TargetGrid);
	
	//// CLIENT FUNCTIONS ///////////////////////////////////////////////////////////////////////////////////////////////////////////////
	UFUNCTION(Client, Reliable)
	void Client_PlaceBuildingAccepted(const FGuid& BuildInstanceID);
	
	UFUNCTION(Client, Reliable)
	void Client_PlaceBeltLineAccepted(const FGuid& BeltLineID);

	UFUNCTION(Client, Reliable)
	void Client_PlaceBuildingRejected(EAtomicPlacementFailReason Reason);
	
	UFUNCTION(Client, Reliable)
	void Client_PlaceBeltLineRejected(EAtomicPlacementFailReason Reason);
	
	//// END OF SERVER FUNCTIONS/////////////////////
	// Server:
	// - Owns authoritative PlacedBuildingRecords
	// - Owns authoritative Cells cache
	// - Validates placement using server Cells
	// - Marks server Cells occupied immediately
	//
	// Clients:
	// - Receive PlacedBuildingRecords through replication
	// - Rebuild local Cells cache in OnRep
	// - Use local Cells only for preview/UI
	// - Never trust client Cells for final placement
	
	
protected:
	virtual void BeginPlay() override;

	EAtomicInputSource InputSource;
	virtual void SetupInputComponent() override;

	// A cached pointer can become invalid when: Player respawns, Pawn is unpossessed, Seamless travel happens, Controller changes, Actor is destroyed, PIE restarts badly
	UPROPERTY(Transient) // Only exists at runtime. Never saved to disk. Zero/Null at load.
	TObjectPtr<AAtomicPlayerCharacter> PlayerCharacter;
	
	virtual void OnPossess(APawn* InPawn) override;
	virtual void AcknowledgePossession(class APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	void CacheControlledCharacter(APawn* InPawn);


	//// COMPONENTS //////////////////////////////////////////////////////
	UPROPERTY(EditDefaultsOnly, Category=Components)
	TObjectPtr<UAtomicInteractionComponent> InteractionComponent;

	UPROPERTY(EditDefaultsOnly, Category=Components)
	TObjectPtr<UAtomicPlayerPlacementComponent> PlacementController;
	
	
	//// INPUT //////////////////////////////////////////////////////////
	// Left Stick    = Move
	// Right Stick X = Camera yaw around character
	// Y / Triangle  = Build menu
	// X / Square    = Interact
	// B / Circle    = Cancel/back
	// A / Cross     = Jump / menu confirm
	// Explore Mode:
		// Right Stick Y = Zoom/pitch
		// LB/RB         = Optional zoom steps
	// Placement Mode
		// Right Stick Y       = Placement distance step
		// LB/RB               = Rotate building left/right
		// RT                  = Place
		// LT + Right Stick Y  = Zoom/pitch camera
		// B                   = Cancel placement
		// Y                   = Change selected buildable
	
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputMappingContext> IMC_PlayerCore;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputMappingContext> IMC_PlayerPlacement;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputMappingContext> IMC_BuildMenu;
	
	int32 PlayerCoreContextPriority = 0;
	int32 PlacementContextPriority = 10;
	int32 BuildMenuContextPriority = 100;

	
	//// TEMP DEVELOPER INPUTS
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_DEV_Belts;
	
	//// PLAYER CORE INPUTS
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_Move;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_GamepadMove;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_Look;
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	
	TObjectPtr<UInputAction> Input_GamepadCamera;
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	
	TObjectPtr<UInputAction> Input_Zoom;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_Jump;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_Interact;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_OpenBuildMenu;

	// Placement Mode Inputs
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_ConfirmPlacement;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_CancelPlacement;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_RotatePlacement;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_PlacementDistance;
	
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_ZoomModifier;

	// Build Menu Inputs
	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	TObjectPtr<UInputAction> Input_CloseBuildMenu;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	float StickNeutralThreshold;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	float YawStickPressThreshold;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	float ZoomStickPressThreshold;

	UPROPERTY(EditDefaultsOnly, Category=Inputs)
	float ZoomRepeatInterval;
	
	EAtomicRightStickMode RightStickMode;

	void MoveAction(const FInputActionInstance& InValue);
	void YawAction(const FInputActionInstance& InValue);
	void PlacementDistanceAction(const FInputActionInstance& InValue);
	void ZoomAction(const FInputActionInstance& InValue);
	void GamepadCameraAction(const FInputActionInstance& InValue);
	void GamepadMoveAction(const FInputActionInstance& InValue);
	void ModifiedZoomAction();
	void ModifiedZoomActionCompleted();
	void JumpAction();
	void InteractAction();

	void OpenBuildMenu();
	void CloseBuildMenu();
	void EnterPlacementMode();
	void ExitPlacementMode();
	
	void ConfirmPlacementAction();
	void CancelPlacementAction();
	void RotatePlacementAction(const FInputActionInstance& InValue);
	
	//// TEMP DEVELOPER INPUT FUNCTIONS
	void PlaceConveyorBeltAction();
	
private:
	bool bIsPlacementMode = false;
	bool bIsBuildMenuOpen = false;
	
	bool bZoomModifier = false;
	bool bIsStickZooming = false;
	float ZoomRepeatTimer = 0.0f;
	bool bMoveWasLastInputValue = false;
};
