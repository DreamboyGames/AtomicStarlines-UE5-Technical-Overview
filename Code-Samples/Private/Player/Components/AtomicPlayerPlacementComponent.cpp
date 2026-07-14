// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Player/Components/AtomicPlayerPlacementComponent.h"

#include "AtomicGameTypes.h"
#include "AtomicStarlines.h"
#include "Belts/AtomicBeltDefinition.h"
#include "Belts/AtomicBeltTypes.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Building/AtomicPlacementGhostActor.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridPlacementComponent.h"
#include "Grid/AtomicGridVisualComponent.h"
#include "Grid/AtomicShipGrid.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AtomicPlayerController.h"


// PlayerController / PlacementComponent
// → line traces from camera/mouse/crosshair
// → hits floor/grid
// → finds AAtomicShipGridActor
// → asks GridData to convert WorldToGrid
// → asks PlacementComponent CanPlace for preview
// → sends Server RPC when player confirms


// Line trace
// Preview ghost
// Current hovered grid coord
// Build confirm input

namespace {
	int32 GetRotationClockwiseIndex(const EBuildingRotation Rotation)
	{
		switch (Rotation)
		{
		case EBuildingRotation::North:
			return 0;
			
		case EBuildingRotation::East:
			return 1;
			
		case EBuildingRotation::South:
			return 2;
			
		case EBuildingRotation::West:
			return 3;
			
		default:
			return 0;
		}
	}
	
	FString GridDirectionArrayToString(const TArray<EGridDirection>& Directions)
	{
		FString Result = TEXT("{ ");
		
		for (int32 Index = 0; Index < Directions.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}
			Result += UEnum::GetValueAsString(Directions[Index]);
		}
		
		Result += TEXT(" }");
		return Result;
	}
	
	FVector GridDirectionToDebugVector(const EGridDirection Direction)
	{
		switch (Direction)
		{
		case EGridDirection::West:
			return FVector(1,0,0);
		
		case EGridDirection::East:
			return FVector(-1,0,0);

		case EGridDirection::North:
			return FVector(0,1,0);
		
		case EGridDirection::South:
			return FVector(0,-1,0);

		default:
			return FVector::ZeroVector;
		}
	}
}


UAtomicPlayerPlacementComponent::UAtomicPlayerPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	
	PlacementDistance = 400;
	PlacementDistanceStep = 2;
	MinPlacementDistanceStep = 1;
	MaxPlacementDistanceStep = 3;
	PlacementInputDeadZone = 0.35f;
	PlacementStepRepeatDelay = 0.15f;
	PlacementRotationStepDelay = 0.18f;
	PlacementTraceHeight = 1000;
	
	CurrentTarget = FAtomicPlacementTarget();
}

void UAtomicPlayerPlacementComponent::BeginPlay()
{
	Super::BeginPlay();
	
	LocalController = Cast<AAtomicPlayerController>(GetOwner());
	
	// Building Placement is local-player presentation logic.
	// Do not run it on remote/non-local PlayerController copies.
	const bool bShouldTick = LocalController && LocalController->IsLocalController();
	SetComponentTickEnabled(bShouldTick);
	
	const UWorld* World = GetWorld();
	if (ensure(World))
	{
		if (const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(World))
		{
			if (ensure(GameInstance))
			{
				PlacementRegistry = GameInstance->GetSubsystem<UAtomicBuildingRegistrySubsystem>();
				ensure(PlacementRegistry);
			}
		}	
	}
}

void UAtomicPlayerPlacementComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CancelPlacement();
	LocalController = nullptr;
	Super::EndPlay(EndPlayReason);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// START PLACEMENT
/////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UAtomicPlayerPlacementComponent::StartPlacementWithSelectedBuilding(const FName BuildingID)
{
	UE_LOG(LogGame, Log, TEXT("Selected Building: %s"), *BuildingID.ToString());
	
	const UAtomicBuildingDefinition* SelectedDefinition = PlacementRegistry->FindBuildingDefinition(BuildingID);
	if (!SelectedDefinition)
	{
		UE_LOG(LogGame, Warning, TEXT("No building definition found for %s"), *BuildingID.ToString());
		return false;
	}

	CurrentSelection.PlacementMode = EPlacementMode::Building;
	CurrentSelection.DefinitionID = BuildingID;
	CurrentSelection.BuildingDefinition = SelectedDefinition;
	CurrentSelection.Rotation = EBuildingRotation::East;
	
	UE_LOG(LogGame, Log, TEXT("Selected Building Definition: %s"), *SelectedDefinition->GetName());
	
	StartPlacement();
	return true;
}

bool UAtomicPlayerPlacementComponent::StartPlacementWithSelectedBelt(const FName BeltID, const EAtomicBeltRouteType InitialRouteType)
{
	UE_LOG(LogGame, Log, TEXT("Selected Belt: %s"), *BeltID.ToString());
	
	const UAtomicBeltDefinition* SelectedDefinition = PlacementRegistry->FindBeltDefinition(BeltID);
	if (!SelectedDefinition)
	{
		UE_LOG(LogGame, Warning, TEXT("No Belt definition found for %s"), *BeltID.ToString());
		return false;
	}
	
	CurrentSelection.PlacementMode = EPlacementMode::Belt;
	CurrentSelection.DefinitionID = BeltID;
	CurrentSelection.BeltDefinition = SelectedDefinition;
	
	CurrentSelection.InitialRouteType = InitialRouteType;
	CurrentSelection.RouteType = InitialRouteType;
	
	CurrentSelection.InputPort = EGridDirection::West;
	CurrentSelection.OutputPort = UAtomicGridLibrary::GetOutputPortForInput(CurrentSelection.RouteType, CurrentSelection.InputPort);
	
	CurrentSelection.Rotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(CurrentSelection.InputPort);

	bBeltInputSnappedToNeighbour = false;
	bBeltRotationManuallyOverridden = false;
	LastTargetGridIndex = INDEX_NONE;
	LastTargetShipGrid = nullptr;
	
	UE_LOG(LogGame, Log, TEXT("Selected Belt Definition: %s"), *SelectedDefinition->GetName());
	
	StartPlacement();
	return true;
}

void UAtomicPlayerPlacementComponent::StartPlacement()
{
	if (!LocalController->IsLocalController()) return;
	
	UE_LOG(LogGame, Log, TEXT("StartPlacement()"));
	bIsPlacementActive = true;
	SetComponentTickEnabled(true);
}

// ---------------------------------------------------------------------
// Cancel Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::CancelPlacement()
{
	bIsPlacementActive = false;
	bHasValidPlacement = false;
	SetComponentTickEnabled(false);
	CurrentSelection.ResetPlacementSelection();
	
	if (IsValid(GhostActor))
	{
		GhostActor->Destroy();
		GhostActor = nullptr;
	}
}

// ---------------------------------------------------------------------
// Confirm Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::ConfirmPlacement()
{
	// @todo: allow a small pending queue for rapid drag/build
	if (bPlacementRequestPending) return;
	if (!LocalController->IsLocalController()) return;
	if (!CurrentSelection.IsValid() || !CurrentTarget.IsValid()) return;

	if (!bHasValidPlacement)
	{
		UE_LOG(LogTemp, Warning, TEXT("bHasValidPlacement: FALSE"));
		UpdatePlacementPreview();
		return;
	}
	UE_LOG(LogTemp, Warning, TEXT("bHasValidPlacement: TRUE"));
	
	
	// SERVER RPC
	// @todo: bPlacementRequestPending = true;
	switch (CurrentSelection.PlacementMode)
	{
	case EPlacementMode::Building:
		LocalController->Server_RequestPlaceBuilding(CurrentSelection.DefinitionID, CurrentTarget.GridCoord, CurrentSelection.Rotation, CurrentTarget.ShipGrid);
		break;
		
	case EPlacementMode::Belt:
		LocalController->Server_RequestPlaceBelt(CurrentSelection.DefinitionID, CurrentTarget.GridCoord, CurrentSelection.RouteType, CurrentSelection.InputPort, CurrentSelection.OutputPort, CurrentTarget.ShipGrid);
		break;
		
	default:
		return;
	}

	// @todo: pending placement confirmation, lock additional placements
	
	// Simple prototype behaviour: place one, then exit placement
	LocalController->HandlePlacementConfirmed();
	
	// @todo: keep selected building active and continue placing more copies.
}


// ---------------------------------------------------------------------
// Rotate Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::RotatePlacement(const float InputValue)
{
	if (!LocalController->IsLocalController()) return;
	if (FMath::IsNearlyZero(InputValue)) return;
	if (!CurrentSelection.IsValid()) return;
	
	const UWorld* World = GetWorld();
	if (!World) return;
	
	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastPlacementRotationTime < PlacementRotationStepDelay) return;
	
	LastPlacementRotationTime = CurrentTime;
	
	// Belt Rotation
	if (CurrentSelection.PlacementMode == EPlacementMode::Belt)
	{
		RotateBeltPlacement(InputValue);
		return;
	}
	
	// Building Rotation
	if (CurrentSelection.PlacementMode == EPlacementMode::Building)
	{
		if (!CurrentSelection.BuildingDefinition->bCanRotate) return;
		RotateBuildingPlacement(InputValue);
		return;
	}
	
	// Wall Rotation
	if (CurrentSelection.PlacementMode == EPlacementMode::Wall)
	{
		RotateBuildingPlacement(InputValue);
		return;
	}
}

void UAtomicPlayerPlacementComponent::RotateBuildingPlacement(const float InputValue)
{
	if (FMath::IsNearlyZero(InputValue)) return;
	
	if (InputValue > 0.f)
	{
		CurrentSelection.Rotation = UAtomicGridLibrary::RotateBuildingClockwise(CurrentSelection.Rotation);
	}
	else
	{
		CurrentSelection.Rotation = UAtomicGridLibrary::RotateBuildingCounterClockwise(CurrentSelection.Rotation);
	}
}


// ---------------------------------------------------------------------
// BELTS
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::RotateBeltPlacement(const float InputValue)
{
	if (FMath::IsNearlyZero(InputValue)) return;

	const bool bRotateClockwise = InputValue > 0.f;
	bBeltRotationManuallyOverridden = true;

	if (bBeltHasSnappedConnection)
	{
		CycleConnectedBeltRoute(bRotateClockwise);
	}
	else
	{
		RotateBeltRouteNormally(bRotateClockwise);
	}
	
	UpdatePlacementPreview();	
}

void UAtomicPlayerPlacementComponent::CycleConnectedBeltRoute(const bool bRotateClockwise)
{
	if (!bBeltHasSnappedConnection) return;
	
	if (SnappedBeltConnectionRole == EAtomicBeltConnectionRole::InputPort)
	{
		EGridDirection NextOutputPort = EGridDirection::East;
		if (!TryGetNextOutputPortAroundInput(CurrentSelection.InputPort, CurrentSelection.OutputPort, bRotateClockwise, NextOutputPort))
		{
			return;
		}
	
		EAtomicBeltRouteType NextRouteType = EAtomicBeltRouteType::Straight;
		if (!UAtomicGridLibrary::TryGetRouteTypeForInputAndOutput(CurrentSelection.InputPort, NextOutputPort, NextRouteType))
		{
			return;
		}
	
		SetCurrentBeltRoute(NextRouteType, CurrentSelection.InputPort);
		return;
	}
	if (SnappedBeltConnectionRole == EAtomicBeltConnectionRole::OutputPort)
	{
		EGridDirection NextInputPort = EGridDirection::West;
		if (!TryGetNextInputPortAroundOutput(CurrentSelection.OutputPort, CurrentSelection.InputPort, bRotateClockwise, NextInputPort))
		{
			return;
		}
	
		EAtomicBeltRouteType NextRouteType = EAtomicBeltRouteType::Straight;
		if (!UAtomicGridLibrary::TryGetRouteTypeForInputAndOutput(NextInputPort, CurrentSelection.OutputPort, NextRouteType))
		{
			return;
		}
	
		SetCurrentBeltRoute(NextRouteType, CurrentSelection.OutputPort);
		return;
	}
	
}

bool UAtomicPlayerPlacementComponent::TryGetNextOutputPortAroundInput(const EGridDirection InputPort, const EGridDirection CurrentOutputPort, const bool bRotateClockwise, EGridDirection& OutNextOutputPort) const
{
	const int32 Step = bRotateClockwise ? 1 : -1;
	
	EGridDirection TestDirection = CurrentOutputPort;
	
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const int32 CurrentIndex = static_cast<int32>(TestDirection);
		const int32 NextIndex = UAtomicGridLibrary::WrapCardinalIndex(CurrentIndex + Step);
		
		TestDirection = static_cast<EGridDirection>(NextIndex);
		if (TestDirection == InputPort) continue;
		
		EAtomicBeltRouteType TestRouteType;
		if (UAtomicGridLibrary::TryGetRouteTypeForInputAndOutput(InputPort, TestDirection, TestRouteType))
		{
			OutNextOutputPort = TestDirection;
			return true;
		}
	}
	
	return false;
}

bool UAtomicPlayerPlacementComponent::TryGetNextInputPortAroundOutput(const EGridDirection OutputPort, const EGridDirection CurrentInputPort, const bool bRotateClockwise, EGridDirection& OutNextInputPort) const
{
	const int32 Step = bRotateClockwise ? 1 : -1;
	
	EGridDirection TestDirection = CurrentInputPort;
	
	for (int32 Attempt = 0; Attempt < 4; ++Attempt)
	{
		const int32 CurrentIndex = static_cast<int32>(TestDirection);
		const int32 NextIndex = UAtomicGridLibrary::WrapCardinalIndex(CurrentIndex + Step);
		
		TestDirection = static_cast<EGridDirection>(NextIndex);
		if (TestDirection == OutputPort) continue;
		
		EAtomicBeltRouteType TestRouteType;
		if (UAtomicGridLibrary::TryGetRouteTypeForInputAndOutput(TestDirection, OutputPort, TestRouteType))
		{
			OutNextInputPort = TestDirection;
			return true;
		}
	}
	
	return false;
}

void UAtomicPlayerPlacementComponent::RotateBeltRouteNormally(const bool bRotateClockwise)
{
	const EBuildingRotation CurrentRotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(CurrentSelection.InputPort);
	
	const EBuildingRotation NewRotation = bRotateClockwise 
		? UAtomicGridLibrary::RotateBuildingClockwise(CurrentRotation)
		: UAtomicGridLibrary::RotateBuildingCounterClockwise(CurrentRotation);
	
	const EGridDirection NewInputPort = UAtomicGridLibrary::GetInputPortForBuildingRotation(NewRotation);
	
	SetCurrentBeltRoute(CurrentSelection.RouteType, NewInputPort);
}

void UAtomicPlayerPlacementComponent::SetCurrentBeltRoute(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort)
{
	CurrentSelection.RouteType = RouteType;
	CurrentSelection.InputPort = InputPort;
	CurrentSelection.OutputPort = UAtomicGridLibrary::GetOutputPortForInput(RouteType, InputPort);
	CurrentSelection.Rotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(InputPort);
}

void UAtomicPlayerPlacementComponent::SetCurrentBeltRouteFromInput(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort)
{
	CurrentSelection.RouteType = RouteType;
	CurrentSelection.InputPort = InputPort;
	CurrentSelection.OutputPort = UAtomicGridLibrary::GetOutputPortForInput(RouteType, InputPort);
	CurrentSelection.Rotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(InputPort);
}

void UAtomicPlayerPlacementComponent::SetCurrentBeltRouteFromOutput(const EAtomicBeltRouteType RouteType, const EGridDirection OutputPort)
{
	EGridDirection InputPort = EGridDirection::West;
	if (!UAtomicGridLibrary::TryGetInputPortForRouteTypeAndOutput(RouteType, OutputPort, InputPort)) return;
	
	CurrentSelection.RouteType = RouteType;
	CurrentSelection.InputPort = InputPort;
	CurrentSelection.OutputPort = OutputPort;
	CurrentSelection.Rotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(InputPort);
}

void UAtomicPlayerPlacementComponent::UpdateAutoBeltSelectionForTarget(const FAtomicPlacementTarget& Target)
{
	if (CurrentSelection.PlacementMode != EPlacementMode::Belt) return;
	if (!Target.IsValid()) return;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return;
	
	// Target Changed?
	const bool bTargetChanged = LastTargetGridIndex != Target.GridIndex || LastTargetShipGrid.Get() != Target.ShipGrid;
	if (!bTargetChanged) return;
	
	LastTargetGridIndex = Target.GridIndex;
	LastTargetShipGrid = Target.ShipGrid;

	bBeltRotationManuallyOverridden = false;
	bBeltInputSnappedToNeighbour = false;
	bBeltHasSnappedConnection = false;
	SnappedBeltConnectionRole = EAtomicBeltConnectionRole::None;
	
	EGridDirection ConnectedPort = EGridDirection::West;
	EAtomicBeltConnectionRole ConnectionRole = EAtomicBeltConnectionRole::None;
	
	if (TryFindSnappedConnectionFromNeighbour(Target, ConnectedPort, ConnectionRole))
	{
		bBeltHasSnappedConnection = true;
		SnappedBeltConnectedPort = ConnectedPort;
		SnappedBeltConnectionRole = ConnectionRole;
		
		if (ConnectionRole == EAtomicBeltConnectionRole::InputPort)
		{
			SetCurrentBeltRouteFromInput(CurrentSelection.InitialRouteType, ConnectedPort);
		}
		else if (ConnectionRole == EAtomicBeltConnectionRole::OutputPort)
		{
			SetCurrentBeltRouteFromOutput(CurrentSelection.InitialRouteType, ConnectedPort);
		}
	}
}

bool UAtomicPlayerPlacementComponent::TryFindSnappedConnectionFromNeighbour(const FAtomicPlacementTarget& Target, EGridDirection& OutConnectedPort, EAtomicBeltConnectionRole& OutConnectionRole) const
{
	OutConnectedPort = EGridDirection::West;
	OutConnectionRole = EAtomicBeltConnectionRole::None;
	
	if (!Target.IsValid()) return false;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return false;
	
	const UAtomicGridDataComponent* GridData = Target.ShipGrid->GetGridData();
	
	for (const EGridDirection DirectionToNeighbour : AllDirections)
	{
		const FAtomicBeltRecord* NeighbourBelt = GridData->GetNeighbourBeltRecord(Target.GridIndex, DirectionToNeighbour);
		if (!NeighbourBelt)
		{
			continue;
		}
		
		const EGridDirection NeighbourPortFacingPreview = UAtomicGridLibrary::OppositeGridDirection(DirectionToNeighbour);
		
		// Case 1:
		// Neighbour outputs into this preview cell.
		// Preview connected side is its InputPort.
		if (NeighbourBelt->OutputPort == NeighbourPortFacingPreview)
		{
			OutConnectedPort = DirectionToNeighbour;
			OutConnectionRole = EAtomicBeltConnectionRole::InputPort;
			return true;
		}
		
		// Case 2:
		// Neighbour input faces this preview cell.
		// Preview connected side is its OutputPort.
		if (NeighbourBelt->InputPort == NeighbourPortFacingPreview)
		{
			OutConnectedPort = DirectionToNeighbour;
			OutConnectionRole = EAtomicBeltConnectionRole::OutputPort;
			return true;
		}
	}
	
	return false;
}


// ---------------------------------------------------------------------
// Placement Distance
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::AdjustPlacementDistance(const float InputValue, const float DeltaTime)
{
	if (!LocalController->IsLocalController()) return;
	
	if (FMath::Abs(InputValue) < PlacementInputDeadZone)
	{
		PlacementStepRepeatTimer = 0;
		LastPlacementStepDirection = 0;
		return;
	}
	
	const int32 StepDirection = InputValue > 0 ? 1 : -1;
	
	PlacementStepRepeatTimer -= DeltaTime;
	
	const bool bDirectionChanged = StepDirection != LastPlacementStepDirection;
	const bool bCanStep = bDirectionChanged || PlacementStepRepeatTimer <= 0.f;
	
	if (!bCanStep) return;
	
	PlacementDistanceStep = FMath::Clamp(PlacementDistanceStep + StepDirection, MinPlacementDistanceStep, MaxPlacementDistanceStep);
	
	UpdatePlacementDistanceFromStep();
	
	LastPlacementStepDirection = StepDirection;
	PlacementStepRepeatTimer = PlacementStepRepeatDelay;
}

void UAtomicPlayerPlacementComponent::UpdatePlacementDistanceFromStep()
{
	PlacementDistanceStep = FMath::Clamp(PlacementDistanceStep, MinPlacementDistanceStep, MaxPlacementDistanceStep);
	PlacementDistance = PlacementDistanceStep * CurrentTarget.CellSize;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// TICK
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void UAtomicPlayerPlacementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (bIsPlacementActive)
	{
		UpdatePlacementPreview();
	}
}

// ---------------------------------------------------------------------
// Update Placement Preview
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::UpdatePlacementPreview()
{
	if (!LocalController->IsLocalController()) return;
	if (CurrentSelection.PlacementMode == EPlacementMode::None) return;
	
	EnsureGhostActor();
	
	// 2. Trace for Ship Grid
	if (!FindPlacementTarget(CurrentTarget))
	{
		HidePlacementPreview();
		bHasValidPlacement = false;
		return;
	}
	
	// Auto belt selection must happen After target is found and Before BuildPlacementPreview()
	if (CurrentSelection.PlacementMode == EPlacementMode::Belt)
	{
		UpdateAutoBeltSelectionForTarget(CurrentTarget);
	}

	// 3. Shared ghost actor update
	FAtomicPlacementPreviewResult PreviewResult;
	if (!BuildPlacementPreview(CurrentTarget, PreviewResult))
	{
		HidePlacementPreview();
		bHasValidPlacement = false;
		return;
	}
	
	// 4. Apply placement transform, mesh, and validity
	ApplyPlacementPreview(PreviewResult);
	
	// 5. Draw Debugs
	DrawDebugPreview(CurrentTarget, PreviewResult);
	
	// @todo: make async on different thread 
}

void UAtomicPlayerPlacementComponent::EnsureGhostActor()
{
	if (!GhostActor)
	{
		GhostActor = GetWorld()->SpawnActor<AAtomicPlacementGhostActor>();
		UE_LOG(LogGame, Log, TEXT("GhostActor Created."));
	}
}

// ---------------------------------------------------------------------
// Find Placement Target
// ---------------------------------------------------------------------
bool UAtomicPlayerPlacementComponent::FindPlacementTarget(FAtomicPlacementTarget& OutTarget)
{
	const APawn* ControlledPawn = LocalController->GetPawn();
	if (!ControlledPawn) return false;
	
	// 1. Get the current camera/view rotation
	const FRotator CameraRotation = LocalController->PlayerCameraManager->GetCameraRotation();
	const FRotator YawOnlyRotation(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector PlacementDirection = YawOnlyRotation.Vector();
	
	// 2. Desired placement position around the player
	const FVector PlayerLocation = ControlledPawn->GetActorLocation();
	UpdatePlacementDistanceFromStep();
	const FVector DesiredWorldLocation = PlayerLocation + PlacementDirection * PlacementDistance;
	
	// 3. Downward trace from above the desired placement location
	const FVector TraceStart = DesiredWorldLocation + FVector(0.f, 0.f, PlacementTraceHeight);
	const FVector TraceEnd = DesiredWorldLocation - FVector(0.f, 0.f, PlacementTraceHeight);
	
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(ControlledPawn);
	
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, COLLISION_GRID_TRACE, QueryParams);
	
	if (!bHit)
	{
		UE_LOG(LogGame, Log, TEXT("bHit: False"));
		OutTarget.ShipGrid = nullptr;
		GhostActor->SetPreviewMeshVisible(false);
		return false;
	}
	
	AAtomicShipGrid* CurrentShipGrid = Cast<AAtomicShipGrid>(Hit.GetActor());
	if (!CurrentShipGrid || !CurrentShipGrid->GetGridData())
	{
		UE_LOG(LogGame, Log, TEXT("No Ship Grid hit"));
		OutTarget.ShipGrid = nullptr;
		GhostActor->SetPreviewMeshVisible(false);
		return false;
	}
	
	// 4. Convert hit point into grid coordinates
	// @todo: add Deck Index logic
	const FTransform GridTransform = CurrentShipGrid->GetTransform();
	const float CellSize = CurrentShipGrid->GetGridData()->GetCellSize();
	const FIntVector GridSize = CurrentShipGrid->GetGridData()->GetGridSize();
	
	const FIntVector AnchorCoord = UAtomicGridLibrary::WorldToGrid(Hit.ImpactPoint, GridTransform, CellSize, 0);
	int32 Index;
	if (!UAtomicGridLibrary::TryGridToIndex(AnchorCoord, GridSize, Index))
	{
		UE_LOG(LogGame, Log, TEXT("Grid index outside of grid"));
		OutTarget.ShipGrid = nullptr;
		GhostActor->SetPreviewMeshVisible(false);
		return false;
	}

	OutTarget.HitResult = Hit;
	OutTarget.ShipGrid = CurrentShipGrid;
	OutTarget.GridCoord = AnchorCoord;
	OutTarget.GridIndex = Index;
	OutTarget.GridTransform = GridTransform;
	OutTarget.CellSize = CellSize;
	OutTarget.GridSize = GridSize;

	GhostActor->SetPreviewMeshVisible(true);
	
	return true;
}

// ---------------------------------------------------------------------
// Apply Placement Preview
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::ApplyPlacementPreview(const FAtomicPlacementPreviewResult& PreviewResult)
{
	if (!GhostActor) return;
	
	if (!PreviewResult.IsValid())
	{
		HidePlacementPreview();
		bHasValidPlacement = false;
		return;
	}

	// First time, or if mesh/material changed
	if (!GhostActor->IsMeshSet() || GhostActor->IsPreviewMeshDifferent(PreviewResult.Mesh))
	{
		GhostActor->SetPreviewMesh(PreviewResult.Mesh, PreviewResult.Material);
	}
	
	GhostActor->SetActorLocation(PreviewResult.WorldLocation);
	GhostActor->SetActorRotation(PreviewResult.WorldRotation);
	GhostActor->SetPlacementValid(PreviewResult.bCanPlace);	
	GhostActor->SetPreviewMeshVisible(true);
	
	bHasValidPlacement = PreviewResult.bCanPlace;
}

void UAtomicPlayerPlacementComponent::HidePlacementPreview() const
{
	if (!GhostActor) return;
	GhostActor->SetPreviewMeshVisible(false);
}

// ---------------------------------------------------------------------
// Build Placement Preview
// ---------------------------------------------------------------------
bool UAtomicPlayerPlacementComponent::BuildPlacementPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const
{
	OutResult = FAtomicPlacementPreviewResult{};

	switch (CurrentSelection.PlacementMode)
	{
	case EPlacementMode::Building:
		return BuildBuildingPreview(Target, OutResult);

	case EPlacementMode::Belt:
		return BuildBeltPreview(Target, OutResult);

	case EPlacementMode::Wall:
		return BuildWallPreview(Target, OutResult);

	default:
		return false;
	}
}

bool UAtomicPlayerPlacementComponent::BuildBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const
{
	if (!CurrentSelection.IsValid()) return false;
	if (!Target.ShipGrid) return false;

	const UAtomicBuildingDefinition* Definition = CurrentSelection.BuildingDefinition;
	
	TArray<FIntVector> FootprintCells;
	UAtomicGridLibrary::GetFootprintCellsAroundPivot(Target.GridCoord, Definition->FootprintSize, Definition->PivotCell, CurrentSelection.Rotation, FootprintCells);
	
	// OutResult.Mesh = Definition->PreviewMesh.Get(); // preload earlier if soft ptr
	//OutResult.Material = Definition->PreviewMaterial.Get();
	OutResult.Mesh = Definition->PreviewMesh.LoadSynchronous();
	OutResult.Material = Definition->PreviewMaterial.LoadSynchronous();
	
	OutResult.WorldLocation = UAtomicGridLibrary::GetFootprintCenterWorldLocationFromCells(FootprintCells, Target.GridTransform, Target.CellSize);
	OutResult.WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(CurrentSelection.Rotation), 0.f);
	
	OutResult.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBuilding(Definition, Target.GridCoord, CurrentSelection.Rotation);
	
	OutResult.bShowGhost = OutResult.Mesh != nullptr && OutResult.Material != nullptr;
	OutResult.PreviewCells = MoveTemp(FootprintCells);
	
	return OutResult.bShowGhost;
}

bool UAtomicPlayerPlacementComponent::BuildBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const
{
	if (!CurrentSelection.IsValid()) return false;
	if (!Target.IsValid() || !Target.ShipGrid || !Target.ShipGrid->GetGridData() || !Target.ShipGrid->GetGridVisualComponent()) return false;
	
	const UAtomicBeltDefinition* Definition = CurrentSelection.BeltDefinition;
	if (!Definition) return false;

	FAtomicBeltRecord PreviewRecord;
	PreviewRecord.InstanceID = FGuid();
	PreviewRecord.DefinitionID = CurrentSelection.DefinitionID;
	PreviewRecord.CellIndex = Target.GridIndex;
	PreviewRecord.RouteType = CurrentSelection.RouteType;
	PreviewRecord.InputPort = CurrentSelection.InputPort;
	PreviewRecord.OutputPort = CurrentSelection.OutputPort;
	
	FAtomicResolvedPreviewBelt ResolvedPreview;
	if (!Target.ShipGrid->GetGridVisualComponent()->ResolveBeltVisualForPreviewRecord(PreviewRecord, ResolvedPreview))
	{
		//CancelPlacement();
		return false;
	}
		
	OutResult.Mesh = Definition->GetMeshForVariant(ResolvedPreview.VisualVariant);
	OutResult.Material = Definition->PreviewMaterial.Get();
	
	OutResult.WorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
	OutResult.WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(ResolvedPreview.VisualRotation), 0.f);
	
	OutResult.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBelt(Definition, Target.GridCoord, CurrentSelection.RouteType, CurrentSelection.InputPort, CurrentSelection.OutputPort);
	
	OutResult.bShowGhost = OutResult.Mesh != nullptr && OutResult.Material != nullptr;
	OutResult.PreviewCells = { Target.GridCoord };
	
	OutResult.ResolvedPreviewBelt = ResolvedPreview;
	OutResult.bHasResolvedBelt = true;
	
	return OutResult.bShowGhost;
}

bool UAtomicPlayerPlacementComponent::BuildWallPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const
{
	return false;
}

// ---------------------------------------------------------------------
// Draw Placement Debugs
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::DrawDebugPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const
{
	const UWorld* World = GetWorld();
	if (!World) return;
	
	const FVector PivotWorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
	
	// Hit Point.
	DrawDebugSphere(World, Target.HitResult.ImpactPoint, 16.f, 6, FColor::White, false, 0.f, 0, 2.f);
	
	// Anchor / Pivot Cell.
	DrawDebugBox(World, PivotWorldLocation, FVector(Target.CellSize * 0.5f, Target.CellSize * 0.5f, 20.f), FColor::Blue, false, 0.f, 0, 2.f);
	
	// Preview Occupied Cells.
	for (const FIntVector& CellCoord : PreviewResult.PreviewCells)
	{
		const FVector CellWorldLocation = UAtomicGridLibrary::GridToWorld(CellCoord, Target.GridTransform, Target.CellSize);
		
		bool bCellValid = false;
		if (Target.ShipGrid && Target.ShipGrid->GetGridPlacementComponent())
		{
			bCellValid = Target.ShipGrid->GetGridPlacementComponent()->CheckIfValidCell(CellCoord);
		}
		
		const FColor ValidityColor = bCellValid ? FColor::Green : FColor::Red;
		DrawDebugBox(World, CellWorldLocation + FVector(0.f, 0.f, 10.f), FVector(Target.CellSize * 0.5f, Target.CellSize * 0.5f, 10.f), ValidityColor, false, 0.f, 0, 2.f);
	}
	
	// Ghost Center.
	DrawDebugSphere(World, PreviewResult.WorldLocation + FVector(0.f, 0.f, 10.f), 6.f, 4, FColor::Black, false, 0.f, 0, 3.f);
	
	// Ghost Visual Forward.
	const FVector Forward = PreviewResult.WorldRotation.Vector().GetSafeNormal();
	const FVector ArrowStart = PreviewResult.WorldLocation + FVector(0.f, 0.f, 10.f);
	const FVector ArrowEnd = ArrowStart + Forward * (Target.CellSize * 0.9f);
	
	DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 50.f, FColor::Black, false, 0.f, 0, 3.f);
	
	// Text.
	const FString DebugString = FString::Printf(
		TEXT("Coord: (%d, %d, %d)\nIndex: %d\n%s\n%s\nCanPlace: %s"),
		Target.GridCoord.X,
		Target.GridCoord.Y,
		Target.GridCoord.Z,
		Target.GridIndex,
		*UEnum::GetValueAsString(CurrentSelection.PlacementMode),
		*UEnum::GetValueAsString(CurrentSelection.Rotation),
		PreviewResult.bCanPlace ? TEXT("true") : TEXT("false")
	);
	
	DrawDebugString(World, PivotWorldLocation + FVector(0.f, 0.f, -80.f), DebugString, nullptr, FColor::White, 0.f, true, 1.1f);
	
	
	// Placement Mode Specific Debugs
	switch (CurrentSelection.PlacementMode)
	{
	case EPlacementMode::Building:
		DrawDebugBuildingPreview(Target, PreviewResult);
		break;
		
	case EPlacementMode::Belt:
		DrawDebugBeltPreview(Target, PreviewResult);
		break;
		
	default:
		break;
	}
}

void UAtomicPlayerPlacementComponent::DrawDebugBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const
{
	const UWorld* World = GetWorld();
	if (!World) return;
	if (!CurrentSelection.BuildingDefinition) return;
	
	// Logical Building Forward.
	const FVector LocalForward = UAtomicGridLibrary::BuildingRotationToForwardVector(CurrentSelection.Rotation);
	const FVector WorldForward = Target.GridTransform.TransformVectorNoScale(LocalForward).GetSafeNormal();
	const FVector PivotWorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
	
	const FVector ArrowStart = PivotWorldLocation + FVector(0, 0, 60.f);
	const FVector ArrowEnd = ArrowStart + WorldForward * (Target.CellSize * 0.5f);
	
	DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 35.f, FColor::Purple, false, 0.f, 0, 4.f);
	
	// Pivot Point
	DrawDebugSphere(World, PivotWorldLocation + FVector(0, 0, 60.f), 6.f, 4, FColor::Purple, false, 0.f, 0, 3.f);
	
	// Building Ports.
	TArray<FAtomicResolvedBuildingPort> ResolvedPorts;
	UAtomicGridLibrary::GetResolvedBuildingPorts(FGuid(), CurrentSelection.DefinitionID, Target.GridCoord, CurrentSelection.BuildingDefinition->PivotCell, CurrentSelection.Rotation, CurrentSelection.BuildingDefinition->PortDefinitions, ResolvedPorts);
	
	for (const FAtomicResolvedBuildingPort& Port : ResolvedPorts)
	{
		const FVector CellCenter = UAtomicGridLibrary::GridToWorld(Port.PortCellCoord, Target.GridTransform, Target.CellSize);
		const FVector LocalDirection = UAtomicGridLibrary::GridDirectionToVector(Port.GridDirection);
		const FVector WorldDirection = Target.GridTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
		const FVector PortWorldLocation = CellCenter + WorldDirection * (Target.CellSize * 0.5f) + FVector(0.f, 0.f, 60.f);
		
		if (Port.PortType == EBuildingPortType::Input)
		{
			const FColor PortColor = FColor::Cyan;
			DrawDebugDirectionalArrow(World, PortWorldLocation + WorldDirection * (Target.CellSize * 0.3f), PortWorldLocation, 50.f, PortColor, false, 0.f, 0, 4.f);
		}
		else
		{
			const FColor PortColor = FColor::Orange;
			DrawDebugDirectionalArrow(World, PortWorldLocation, PortWorldLocation + WorldDirection * (Target.CellSize * 0.3f), 50.f, PortColor, false, 0.f, 0, 4.f);
		}
	}
}

void UAtomicPlayerPlacementComponent::DrawDebugBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const
{
	if (!PreviewResult.bHasResolvedBelt) return;
	
	const UWorld* World = GetWorld();
	if (!World) return;
	
	if (!CurrentSelection.BeltDefinition) return;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return;

	const FVector CellCenter = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);

	// Small arrows only for valid neighbour connections
	for (const EGridDirection Port : PreviewResult.ResolvedPreviewBelt.ConnectedRoutePorts)
	{
		const FVector LocalDirection = UAtomicGridLibrary::GridDirectionToVector(Port);
		const FVector WorldDirection = Target.GridTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
		const FVector EdgeCenter = CellCenter + WorldDirection * (Target.CellSize * 0.4f) + FVector(0.f, 0.f, 70.f);
		const FVector ArrowStart = EdgeCenter - WorldDirection * (Target.CellSize * 0.12f);
		const FVector ArrowEnd = EdgeCenter + WorldDirection * (Target.CellSize * 0.12f);
		
		DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 18.f, FColor::Cyan, false, 0.f, 0, 3.f);
	}
	
	// Main flow arrow
	const EGridDirection OutputDirection = PreviewResult.ResolvedPreviewBelt.OutputPort;
	const FVector FlowLocalDirection = UAtomicGridLibrary::GridDirectionToVector(OutputDirection);
	const FVector FlowWorldDirection = Target.GridTransform.TransformVectorNoScale(FlowLocalDirection).GetSafeNormal();
	const FVector FlowArrowStart = CellCenter + FVector(0.f, 0.f, 70.f);
	const FVector FlowArrowEnd = FlowArrowStart + FlowWorldDirection * (Target.CellSize * 0.4f);
	
	DrawDebugDirectionalArrow(World, FlowArrowStart, FlowArrowEnd, 35.f, FColor::Purple, false, 0.f, 0, 4.f);
	
	// Belt Shape Text
	const FString BeltString = FString::Printf(
		TEXT("Belt Shape: %s\nSelection Rotation: %s\nVisual Rotation: %s\nOutput Flow: %s\nVisual Variant: %s"),
		*UEnum::GetValueAsString(CurrentSelection.RouteType), *UEnum::GetValueAsString(CurrentSelection.Rotation), *UEnum::GetValueAsString(PreviewResult.ResolvedPreviewBelt.VisualRotation),
		*UEnum::GetValueAsString(CurrentSelection.OutputPort), *UEnum::GetValueAsString(PreviewResult.ResolvedPreviewBelt.VisualVariant)
	);
	DrawDebugString(World, CellCenter + FVector(0.f, 0.f, 150.f), BeltString, nullptr, FColor::Purple, 0.f, true, 1.1f);
}
