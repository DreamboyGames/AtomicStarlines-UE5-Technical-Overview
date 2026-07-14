// Copyright Dreamboy Games Pty Ltd. All rights reserved.


#include "Player/AtomicPlayerController.h"

#include "AtomicStarlines.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Grid/AtomicGridPlacementComponent.h"
#include "Grid/AtomicShipGrid.h"
#include "Player/AtomicPlayerCharacter.h"
#include "Player/Components/AtomicInteractionComponent.h"
#include "Player/Components/AtomicPlayerPlacementComponent.h"


// SERVER -- has PlayerController for every player
// CLIENT -- has Only its own PlayerController
// Has Player's OwningNetConnection (how the server determines Ownership)
// NetUpdateFrequency: (100 times/seconds) (Net Priority 3)
AAtomicPlayerController::AAtomicPlayerController()
{
	PlayerCharacter = nullptr;
	
	bNetLoadOnClient = true;			// true = Actor will load on clients during map load | false = Actor will only exist on the Server level
	bReplicates = true;					// Designates this Actor to be a replicated Actor, allows Variables to replicate (if marked for replication) (forces NetLoadOnClient true)
	
	InteractionComponent = CreateDefaultSubobject<UAtomicInteractionComponent>(TEXT("InteractionComponent"));
	PlacementController = CreateDefaultSubobject<UAtomicPlayerPlacementComponent>(TEXT("PlacementController"));
	
	RightStickMode = EAtomicRightStickMode::Neutral;
	StickNeutralThreshold = 0.25f;
	YawStickPressThreshold = 0.45f;
	ZoomStickPressThreshold = 0.9f;
	ZoomRepeatInterval = 0.2f;
}

void AAtomicPlayerController::BeginPlay()
{
	Super::BeginPlay();
	
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->ClearAllMappings();
			InputSubsystem->AddMappingContext(IMC_PlayerCore, PlayerCoreContextPriority);
		}
	}
}

// Server-side event

void AAtomicPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	CacheControlledCharacter(InPawn);
}

// Client-side function used to confirm and safely handle possession change once the new Pawn reference has successfully replicated from the server.

// Because of network latency, a client does not instantly know which Pawn their PlayerController possess.

void AAtomicPlayerController::AcknowledgePossession(class APawn* InPawn)
{
	Super::AcknowledgePossession(InPawn);
	CacheControlledCharacter(InPawn);
}

void AAtomicPlayerController::OnUnPossess()
{
	PlayerCharacter = nullptr;
	Super::OnUnPossess();
}

void AAtomicPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	PlayerCharacter = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AAtomicPlayerController::CacheControlledCharacter(APawn* InPawn)
{
	PlayerCharacter = Cast<AAtomicPlayerCharacter>(InPawn);
	if (!PlayerCharacter)
	{
		UE_LOG(LogGame, Warning, TEXT("Possessed pawn is not AAtomicPlayerCharacter."));
		return;
	}
}

void AAtomicPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	if (!ensure(EnhancedInput)) return;
	if (!ensure(Input_Move && Input_Look && Input_Zoom && Input_Jump && Input_GamepadMove && Input_GamepadCamera && Input_Interact && Input_OpenBuildMenu && Input_CloseBuildMenu && Input_ConfirmPlacement && Input_CancelPlacement && Input_RotatePlacement && Input_PlacementDistance && Input_ZoomModifier)) 
	{
		return;
	}
		
	// Player Core Inputs
	EnhancedInput->BindAction(Input_Move, ETriggerEvent::Triggered, this, &ThisClass::MoveAction);
	EnhancedInput->BindAction(Input_Look, ETriggerEvent::Triggered, this, &ThisClass::YawAction);
	EnhancedInput->BindAction(Input_Zoom, ETriggerEvent::Started, this, &ThisClass::ZoomAction);
	EnhancedInput->BindAction(Input_Jump, ETriggerEvent::Started, this, &ThisClass::JumpAction);
	EnhancedInput->BindAction(Input_GamepadMove, ETriggerEvent::Triggered, this, &ThisClass::GamepadMoveAction);
	EnhancedInput->BindAction(Input_GamepadCamera, ETriggerEvent::Triggered, this, &ThisClass::GamepadCameraAction);

	// Explore Mode Inputs
	EnhancedInput->BindAction(Input_Interact, ETriggerEvent::Started, this, &ThisClass::InteractAction);
	EnhancedInput->BindAction(Input_OpenBuildMenu, ETriggerEvent::Started, this, &ThisClass::OpenBuildMenu);
	
	// Build Menu Inputs
	EnhancedInput->BindAction(Input_CloseBuildMenu, ETriggerEvent::Started, this, &ThisClass::CloseBuildMenu);
	EnhancedInput->BindAction(Input_ConfirmPlacement, ETriggerEvent::Started, this, &ThisClass::ConfirmPlacementAction);
	EnhancedInput->BindAction(Input_CancelPlacement, ETriggerEvent::Started, this, &ThisClass::CancelPlacementAction);
	EnhancedInput->BindAction(Input_RotatePlacement, ETriggerEvent::Started, this, &ThisClass::RotatePlacementAction);
	EnhancedInput->BindAction(Input_PlacementDistance, ETriggerEvent::Triggered, this, &ThisClass::PlacementDistanceAction);
	EnhancedInput->BindAction(Input_ZoomModifier, ETriggerEvent::Started, this, &ThisClass::ModifiedZoomAction);
	EnhancedInput->BindAction(Input_ZoomModifier, ETriggerEvent::Completed, this, &ThisClass::ModifiedZoomActionCompleted);
	
	// TEMP DEVELOPER BINDING
	EnhancedInput->BindAction(Input_DEV_Belts, ETriggerEvent::Started, this, &ThisClass::PlaceConveyorBeltAction);
}

bool AAtomicPlayerController::IsUsingGamepad() const
{
	if (InputSource == EAtomicInputSource::Gamepad)
	{
		UE_LOGFMT(LogTemp, Warning, "InputSource |Gamepad: {PC}", InputSource);
		return true;
	}
	else
	{
		UE_LOGFMT(LogTemp, Warning, "InputSource | Keyboard: {PC}", InputSource);
		return false;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// INPUT ACTIONS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerController::MoveAction(const FInputActionInstance& InValue)
{
	if (!PlayerCharacter) return;
	
	bMoveWasLastInputValue = true;
	const FVector2D InputValue = InValue.GetValue().Get<FVector2D>();
	PlayerCharacter->Move(InputValue);
	
	InputSource = EAtomicInputSource::MouseKeyboard;
}

void AAtomicPlayerController::JumpAction()
{
	if (!PlayerCharacter) return;
	
	PlayerCharacter->JumpStart();
}

void AAtomicPlayerController::YawAction(const FInputActionInstance& InValue)
{
	bMoveWasLastInputValue = false;
	const float MouseX = InValue.GetValue().Get<float>();
	AddYawInput(MouseX);
	
	InputSource = EAtomicInputSource::MouseKeyboard;
	
	// @todo: Right Click to rotate camera when in aim/build mode
}

void AAtomicPlayerController::ZoomAction(const FInputActionInstance& InValue)
{
	if (!PlayerCharacter) return;
	
	UE_LOGFMT(LogTemp, Warning, "bIsPlacementMode: {bIsPlacementMode}", bIsPlacementMode);
	UE_LOGFMT(LogTemp, Warning, "bZoomModifier: {bZoomModifier}", bZoomModifier);
	
	const float InputValue = InValue.GetValue().Get<float>();
	if (!bIsPlacementMode)
	{
		PlayerCharacter->Zoom(InputValue);
		UE_LOGFMT(LogTemp, Warning, "ZoomAction: {InValue}", InputValue);
	}
	else
	{
		if (bZoomModifier)
		{
			PlayerCharacter->Zoom(InputValue);
			UE_LOGFMT(LogTemp, Warning, "ZoomAction: {InValue}", InputValue);
		}
		else
		{
			PlacementController->RotatePlacement(InputValue);
		}
	}

	InputSource = EAtomicInputSource::MouseKeyboard;	
}

void AAtomicPlayerController::GamepadMoveAction(const FInputActionInstance& InValue)
{
	bMoveWasLastInputValue = true;
	MoveAction(InValue);
	
	InputSource = EAtomicInputSource::Gamepad;
}

void AAtomicPlayerController::PlacementDistanceAction(const FInputActionInstance& InValue)
{
	if (!bIsPlacementMode || bIsPlacementMode && bZoomModifier) return;
	const float InputValue = InValue.GetValue().Get<float>();
	PlacementController->AdjustPlacementDistance(InputValue, GetWorld()->DeltaTimeSeconds);
}

void AAtomicPlayerController::ModifiedZoomAction()
{
	bZoomModifier = true;
}

void AAtomicPlayerController::ModifiedZoomActionCompleted()
{
	bZoomModifier = false;
}

void AAtomicPlayerController::GamepadCameraAction(const FInputActionInstance& InValue)
{
	if (!PlayerCharacter) return;
	
	bMoveWasLastInputValue = false;
	const FVector2D InputValue = InValue.GetValue().Get<FVector2D>();
	const bool bCanZoom = !bIsPlacementMode || bIsPlacementMode && bZoomModifier;

	const float AbsStickX = FMath::Abs(InputValue.X);
	const float AbsStickY = FMath::Abs(InputValue.Y);

	// Gamepad Input, Right Stick Must return to Neutral before changing mode
	if (AbsStickX <= StickNeutralThreshold && AbsStickY <= StickNeutralThreshold)
	{
		RightStickMode = EAtomicRightStickMode::Neutral;
		ZoomRepeatTimer = 0.0f;
		return;
	}
	
	if (RightStickMode == EAtomicRightStickMode::Neutral)
	{
		const bool bShouldZoom = AbsStickY >= ZoomStickPressThreshold && AbsStickY > AbsStickX;
		const bool bShouldRotate = AbsStickX >= YawStickPressThreshold;
		
		if (bShouldZoom && bCanZoom)
		{
			RightStickMode = EAtomicRightStickMode::Zoom;
			ZoomRepeatTimer = 0.0f;
			PlayerCharacter->Zoom(InputValue.Y);
			return;
		}
		
		if (bShouldRotate)
		{
			RightStickMode = EAtomicRightStickMode::Yaw;
		}
	}
	
	// Locked Yaw mode
	if (RightStickMode == EAtomicRightStickMode::Yaw)
	{
		AddYawInput(InputValue.X);
		return;
	}
	
	// Locked Zoom mode
	if (RightStickMode == EAtomicRightStickMode::Zoom && bCanZoom)
	{
		ZoomRepeatTimer += GetWorld()->GetDeltaSeconds();
		if (ZoomRepeatTimer >= ZoomRepeatInterval)
		{
			ZoomRepeatTimer = 0.0f;
			PlayerCharacter->Zoom(InputValue.Y);
		}
	}
	
	InputSource = EAtomicInputSource::Gamepad;
}

void AAtomicPlayerController::InteractAction()
{
	InteractionComponent->Interact();
}

void AAtomicPlayerController::EnterPlacementMode()
{
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(IMC_PlayerPlacement, PlacementContextPriority);
		}
	}
	
	// Start placement preview
	bIsPlacementMode = true;
	PlacementController->StartPlacementWithSelectedBuilding(FName("CardboardBox"));
}

void AAtomicPlayerController::ConfirmPlacementAction()
{
	UE_LOG(LogTemp, Warning, TEXT("ConfirmPlacementAction"));
	PlacementController->ConfirmPlacement();
}

void AAtomicPlayerController::CancelPlacementAction()
{
	PlacementController->CancelPlacement();
}

void AAtomicPlayerController::RotatePlacementAction(const FInputActionInstance& InValue)
{
	if (!PlacementController) return;
	
	const float InputValue = InValue.GetValue().Get<float>();
	PlacementController->RotatePlacement(InputValue);
}

void AAtomicPlayerController::OpenBuildMenu()
{
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(IMC_BuildMenu, BuildMenuContextPriority);
		}
	}
	
	// Show build menu UI
	bIsBuildMenuOpen = true;
	
	// DEBUG
	EnterPlacementMode();
}

void AAtomicPlayerController::CloseBuildMenu()
{
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->RemoveMappingContext(IMC_BuildMenu);
		}
	}
	
	// Hide build menu UI
	bIsBuildMenuOpen = false;
	
	// DEBUG
	ExitPlacementMode();
}

// TEMP DEVELOPER INPUT ACTION
void AAtomicPlayerController::PlaceConveyorBeltAction()
{
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(IMC_BuildMenu, BuildMenuContextPriority);
			InputSubsystem->AddMappingContext(IMC_PlayerPlacement, PlacementContextPriority);
		}
	}
	
	bIsBuildMenuOpen = true;
	bIsPlacementMode = true;
	PlacementController->StartPlacementWithSelectedBelt(FName("ConveyorBelt"), EAtomicBeltRouteType::Straight);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Helper Functions
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerController::ExitPlacementMode()
{
	if (!IsLocalController()) return;
	if (const ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->RemoveMappingContext(IMC_PlayerPlacement);
		}
	}
	
	// Stop placement preview
	PlacementController->CancelPlacement();
	bIsPlacementMode = false;
}

void AAtomicPlayerController::HandlePlacementConfirmed()
{
	// Prototype Behaviour: place one building, then fully leave build/placement mode.
	CloseBuildMenu();
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// SERVER RPCs
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------
// Request Place BUILDING
// ---------------------------------------------------------------------
void AAtomicPlayerController::Server_RequestPlaceBuilding_Implementation(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const AAtomicShipGrid* TargetGrid)
{
	UE_LOG(LogGame, Log, TEXT("Server_RequestPlaceBuilding_Implementation"));
	if (!TargetGrid || !TargetGrid->HasAuthority()) return;

	const APawn* RequestingPawn = GetPawn();
	if (!RequestingPawn) return;

	const UAtomicGridPlacementComponent* PlacementComponent = TargetGrid->GetGridPlacementComponent();
	if (!PlacementComponent) return;

	const bool bPlaced = PlacementComponent->TryPlaceBuilding(BuildingID, AnchorCoord, Rotation, RequestingPawn);
	
	if (!bPlaced)
	{
		UE_LOG(LogGame, Warning, TEXT("TryPlaceBuilding = False!"));
		Client_PlaceBuildingRejected(EAtomicPlacementFailReason::Blocked);
	}
	
	UE_LOG(LogGame, Log, TEXT("Placement Successful!"));
}

void AAtomicPlayerController::Client_PlaceBuildingAccepted_Implementation(const FGuid& BuildInstanceID)
{
	// Accepted sound / flash / clear pending ghost
	// Final placement truth OnRep_PlacedBuildingRecords()
}

void AAtomicPlayerController::Client_PlaceBuildingRejected_Implementation(EAtomicPlacementFailReason Reason)
{
	
}


// ---------------------------------------------------------------------
// Request Place BELT
// ---------------------------------------------------------------------
void AAtomicPlayerController::Server_RequestPlaceBelt_Implementation(const FName BuildingID, const FIntVector AnchorCoord, const EAtomicBeltRouteType BeltShape, const EGridDirection InputPort, const EGridDirection OutputPort, const AAtomicShipGrid* TargetGrid)
{
	UE_LOG(LogGame, Log, TEXT("Server_RequestPlaceBuilding_Implementation"));
	if (!TargetGrid || !TargetGrid->HasAuthority()) return;

	const APawn* RequestingPawn = GetPawn();
	if (!RequestingPawn) return;

	const UAtomicGridPlacementComponent* PlacementComponent = TargetGrid->GetGridPlacementComponent();
	if (!PlacementComponent) return;

	const bool bPlaced = PlacementComponent->TryPlaceBelt(BuildingID, AnchorCoord, BeltShape, InputPort, OutputPort, RequestingPawn);
	
	if (!bPlaced)
	{
		UE_LOG(LogGame, Warning, TEXT("TryPlaceBelt = False!"));
		Client_PlaceBeltRejected(EAtomicPlacementFailReason::Blocked);
	}
	
	UE_LOG(LogGame, Log, TEXT("Placement Successful!"));
}

void AAtomicPlayerController::Client_PlaceBeltAccepted_Implementation(const FGuid& BuildInstanceID)
{
	
}

void AAtomicPlayerController::Client_PlaceBeltRejected_Implementation(EAtomicPlacementFailReason Reason)
{
	
}
