// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#include "Player/Components/AtomicPlayerPlacementComponent.h"

#include "AtomicStarlines.h"
#include "Belts/AtomicBeltDefinition.h"
#include "ProjectTypes//AtomicBeltTypes.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Building/AtomicPlacementGhostActor.h"
#include "DrawDebugHelpers.h"
#include "Belts/AtomicBeltLineGhostActor.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridPlacementComponent.h"
#include "Grid/AtomicGridVisualComponent.h"
#include "Grid/AtomicShipGrid.h"
#include "Kismet/GameplayStatics.h"
#include "Player/AtomicPlayerController.h"


// PlayerController / PlacementComponent
// → line traces from camera / mouse / crosshair
// → hits floor / grid
// → finds AAtomicShipGridActor
// → asks GridData to convert WorldToGrid
// → asks GridPlacementComponent CanPlace for preview validity
// → sends Server RPC when player confirms
//
// This component is local-player presentation + input state only.
// The server still owns final validation and committed build records.


namespace
{
	// Kept as a small local debug helper. Useful when comparing cardinal order
	// against visual clockwise order during placement debugging.
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
}


UAtomicPlayerPlacementComponent::UAtomicPlayerPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	PlacementDistance = 400.f;
	PlacementDistanceStep = 2.f;
	MinPlacementDistanceStep = 1.f;
	MaxPlacementDistanceStep = 3.f;
	PlacementInputDeadZone = 0.35f;
	PlacementStepRepeatDelay = 0.15f;
	PlacementRotationStepDelay = 0.18f;
	PlacementTraceHeight = 1000.f;

	CurrentTarget = FAtomicPlacementTarget();
}

void UAtomicPlayerPlacementComponent::BeginPlay()
{
	Super::BeginPlay();

	LocalController = Cast<AAtomicPlayerController>(GetOwner());

	// Placement preview is local-player presentation logic.
	// Do not run it on remote/non-local PlayerController copies.
	const bool bShouldTick = LocalController && LocalController->IsLocalController();
	SetComponentTickEnabled(bShouldTick);

	const UWorld* World = GetWorld();
	if (ensure(World))
	{
		if (const UGameInstance* GameInstance = UGameplayStatics::GetGameInstance(World))
		{
			PlacementRegistry = GameInstance->GetSubsystem<UAtomicBuildingRegistrySubsystem>();
			ensure(PlacementRegistry);
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////

bool UAtomicPlayerPlacementComponent::StartPlacementWithSelectedBuilding(const FName BuildingID)
{
	UE_LOG(LogGame, Log, TEXT("Selected Building: %s"), *BuildingID.ToString());

	if (!PlacementRegistry)
	{
		UE_LOG(LogGame, Warning, TEXT("Cannot start building placement. PlacementRegistry is null."));
		return false;
	}

	const UAtomicBuildingDefinition* SelectedDefinition = PlacementRegistry->FindBuildingDefinition(BuildingID);
	if (!SelectedDefinition)
	{
		UE_LOG(LogGame, Warning, TEXT("No building definition found for %s"), *BuildingID.ToString());
		return false;
	}

	CurrentSelection.ResetPlacementSelection();
	CurrentSelection.PlacementMode = EPlacementMode::Building;
	CurrentSelection.DefinitionID = BuildingID;
	CurrentSelection.BuildingDefinition = SelectedDefinition;
	CurrentSelection.Rotation = EBuildingRotation::East;

	UE_LOG(LogGame, Log, TEXT("Selected Building Definition: %s"), *SelectedDefinition->GetName());

	StartPlacement();
	return true;
}

bool UAtomicPlayerPlacementComponent::StartPlacementWithSelectedBelt(const FName BeltID)
{
	UE_LOG(LogGame, Log, TEXT("Selected Belt: %s"), *BeltID.ToString());

	if (!PlacementRegistry)
	{
		UE_LOG(LogGame, Warning, TEXT("Cannot start belt placement. PlacementRegistry is null."));
		return false;
	}

	const UAtomicBeltDefinition* SelectedDefinition = PlacementRegistry->FindBeltDefinition(BeltID);
	if (!SelectedDefinition)
	{
		UE_LOG(LogGame, Warning, TEXT("No Belt definition found for %s"), *BeltID.ToString());
		return false;
	}

	CurrentSelection.ResetPlacementSelection();
	CurrentSelection.PlacementMode = EPlacementMode::Belt;
	CurrentSelection.DefinitionID = BeltID;
	CurrentSelection.BeltDefinition = SelectedDefinition;
	CurrentSelection.Rotation = EBuildingRotation::East;

	UE_LOG(LogGame, Log, TEXT("Selected Belt Definition: %s"), *SelectedDefinition->GetName());

	StartPlacement();
	return true;
}

void UAtomicPlayerPlacementComponent::StartPlacement()
{
	if (!LocalController || !LocalController->IsLocalController()) return;

	UE_LOG(LogGame, Log, TEXT("StartPlacement()"));

	bIsPlacementActive = true;
	bHasValidPlacement = false;
	bPlacementRequestPending = false;

	SetComponentTickEnabled(true);
}

void UAtomicPlayerPlacementComponent::ClearSelectedPlacement()
{
	CurrentSelection.ResetPlacementSelection();
	CurrentTarget = FAtomicPlacementTarget();

	LastTargetGridIndex = INDEX_NONE;
	LastTargetShipGrid = nullptr;
}


// ---------------------------------------------------------------------
// Cancel Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::CancelPlacement()
{
	bIsPlacementActive = false;
	bHasValidPlacement = false;
	bPlacementRequestPending = false;

	SetComponentTickEnabled(false);

	CancelBeltLinePlacement();
	ClearSelectedPlacement();

	if (IsValid(SingleGhostActor))
	{
		SingleGhostActor->Destroy();
		SingleGhostActor = nullptr;
	}
	
	if (IsValid(BeltLineGhostActor))
	{
		BeltLineGhostActor->Destroy();
		BeltLineGhostActor = nullptr;
	}
}


// ---------------------------------------------------------------------
// Confirm Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::ConfirmPlacement()
{
	// @todo: allow a small pending queue for rapid drag/build.
	if (bPlacementRequestPending) return;
	if (!LocalController || !LocalController->IsLocalController()) return;
	if (!CurrentSelection.IsValid() || !CurrentTarget.IsValid()) return;

	if (!bHasValidPlacement)
	{
		UE_LOG(LogTemp, Warning, TEXT("bHasValidPlacement: FALSE"));
		UpdatePlacementPreview();
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("bHasValidPlacement: TRUE"));

	// SERVER RPC
	// Client sends the compact placement request.
	// Server validates again before writing replicated records.
	switch (CurrentSelection.PlacementMode)
	{
	case EPlacementMode::Building:
		LocalController->Server_RequestPlaceBuilding(CurrentSelection.DefinitionID, CurrentTarget.GridCoord, CurrentSelection.Rotation, CurrentTarget.ShipGrid);
		break;

	case EPlacementMode::Belt:
		// Confirm 1: client starts belt-line drag preview
		if (!bIsDraggingBeltLine)
		{
			StartBeltLinePlacement(CurrentTarget);
			return;
		}
		// Confirm 2: client sends full belt line to server
		LocalController->Server_RequestPlaceBeltLine(CurrentSelection.DefinitionID, CurrentBeltLinePreviewCells, CurrentTarget.ShipGrid);
		break;

	default:
		return;
	}

	// Simple prototype behaviour: place one, then exit placement.
	// @todo: keep selected tool active and continue placing more copies.
	LocalController->HandlePlacementConfirmed();
}

void UAtomicPlayerPlacementComponent::CancelBeltLinePlacement()
{
	bIsDraggingBeltLine = false;
	bBeltLinePreferXFirst = false;
	BeltLineStartTarget = FAtomicPlacementTarget();
	CurrentBeltLinePreviewCells.Reset();
	HideBeltLinePreview();
	bHasValidPlacement = false;
}

// ---------------------------------------------------------------------
// Rotate Placement
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::RotatePlacement(const float InputValue)
{
	if (!LocalController || !LocalController->IsLocalController()) return;
	if (FMath::IsNearlyZero(InputValue)) return;
	if (!CurrentSelection.IsValid()) return;

	const UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastPlacementRotationTime < PlacementRotationStepDelay) return;

	LastPlacementRotationTime = CurrentTime;

	// Belt Rotation.
	if (CurrentSelection.PlacementMode == EPlacementMode::Belt)
	{
		RotateBeltPlacement(InputValue);
		return;
	}

	// Building Rotation.
	if (CurrentSelection.PlacementMode == EPlacementMode::Building)
	{
		if (!CurrentSelection.BuildingDefinition || !CurrentSelection.BuildingDefinition->bCanRotate) return;
		RotateBuildingPlacement(InputValue);
		return;
	}

	// Wall Rotation.
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
// Placement Distance
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::AdjustPlacementDistance(const float InputValue, const float DeltaTime)
{
	if (!LocalController || !LocalController->IsLocalController()) return;

	if (FMath::Abs(InputValue) < PlacementInputDeadZone)
	{
		PlacementStepRepeatTimer = 0.f;
		LastPlacementStepDirection = 0;
		return;
	}

	const int32 StepDirection = InputValue > 0.f ? 1 : -1;

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
///////////////////////////////////////////////////////////////////////////////////////////////////////////
void UAtomicPlayerPlacementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
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
	if (!LocalController || !LocalController->IsLocalController()) return;
	if (CurrentSelection.PlacementMode == EPlacementMode::None) return;

	// 1. Trace for Ship Grid.
	if (!FindPlacementTarget(CurrentTarget))
	{
		HideAllPlacementPreviews();
		bHasValidPlacement = false;
		return;
	}

	// 2. Shared ghost actor update.
	FAtomicPlacementPreviewResult PreviewResult;
	if (!BuildPlacementPreview(CurrentTarget, PreviewResult))
	{
		HideAllPlacementPreviews();
		bHasValidPlacement = false;
		return;
	}

	// 3. Apply placement transform, mesh, and validity.
	ApplyPlacementPreview(PreviewResult);

	// 4. Draw Debugs.
	DrawDebugPreview(CurrentTarget, PreviewResult);

	// @todo: make async on different thread if expensive debug/preview logic grows.
}

void UAtomicPlayerPlacementComponent::EnsureSingleGhostActor()
{
	if (!SingleGhostActor)
	{
		SingleGhostActor = GetWorld()->SpawnActor<AAtomicPlacementGhostActor>();
		UE_LOG(LogGame, Log, TEXT("Single GhostActor Created."));
	}
}

void UAtomicPlayerPlacementComponent::HideSingleGhostPreview() const
{
	if (SingleGhostActor)
	{
		SingleGhostActor->SetPreviewMeshVisible(false);
	}
}


// ---------------------------------------------------------------------
// Find Placement Target
// ---------------------------------------------------------------------
bool UAtomicPlayerPlacementComponent::FindPlacementTarget(FAtomicPlacementTarget& OutTarget)
{
	if (!LocalController) return false;

	const APawn* ControlledPawn = LocalController->GetPawn();
	if (!ControlledPawn) return false;

	// 1. Get the current camera/view rotation.
	const FRotator CameraRotation = LocalController->PlayerCameraManager->GetCameraRotation();
	const FRotator YawOnlyRotation(0.0f, CameraRotation.Yaw, 0.0f);
	const FVector PlacementDirection = YawOnlyRotation.Vector();

	// 2. Desired placement position around the player.
	const FVector PlayerLocation = ControlledPawn->GetActorLocation();
	UpdatePlacementDistanceFromStep();
	const FVector DesiredWorldLocation = PlayerLocation + PlacementDirection * PlacementDistance;

	// 3. Downward trace from above the desired placement location.
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
		OutTarget = FAtomicPlacementTarget();
		return false;
	}

	AAtomicShipGrid* CurrentShipGrid = Cast<AAtomicShipGrid>(Hit.GetActor());
	if (!CurrentShipGrid || !CurrentShipGrid->GetGridData())
	{
		UE_LOG(LogGame, Log, TEXT("No Ship Grid hit"));
		OutTarget = FAtomicPlacementTarget();
		return false;
	}

	// 4. Convert hit point into grid coordinates.
	// @todo: add Deck Index logic.
	const FTransform GridTransform = CurrentShipGrid->GetTransform();
	const float CellSize = CurrentShipGrid->GetGridData()->GetCellSize();
	const FIntVector GridSize = CurrentShipGrid->GetGridData()->GetGridSize();

	const FIntVector AnchorCoord = UAtomicGridLibrary::WorldToGrid(Hit.ImpactPoint, GridTransform, CellSize, 0);

	int32 Index = INDEX_NONE;
	if (!UAtomicGridLibrary::TryGridToIndex(AnchorCoord, GridSize, Index))
	{
		UE_LOG(LogGame, Log, TEXT("Grid index outside of grid"));
		OutTarget = FAtomicPlacementTarget();
		return false;
	}

	OutTarget.HitResult = Hit;
	OutTarget.ShipGrid = CurrentShipGrid;
	OutTarget.GridCoord = AnchorCoord;
	OutTarget.GridIndex = Index;
	OutTarget.GridTransform = GridTransform;
	OutTarget.CellSize = CellSize;
	OutTarget.GridSize = GridSize;

	return true;
}


// ---------------------------------------------------------------------
// Apply Placement Preview
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::ApplyPlacementPreview(const FAtomicPlacementPreviewResult& PreviewResult)
{
	if (!PreviewResult.IsValid())
	{
		HideAllPlacementPreviews();
		bHasValidPlacement = false;
		return;
	}
	
	switch (PreviewResult.PreviewType)
	{
	case EAtomicPlacementPreviewType::SingleGhost:
		HideBeltLinePreview();
		ApplySingleGhostPreview(PreviewResult);
		return;;
		
	case EAtomicPlacementPreviewType::BeltLine:
		HideSingleGhostPreview();
		ApplyBeltLinePreview(PreviewResult);
		return;
		
	default:
		HideAllPlacementPreviews();
		return;
	}
}

void UAtomicPlayerPlacementComponent::ApplySingleGhostPreview(const FAtomicPlacementPreviewResult& PreviewResult)
{
	if (!PreviewResult.IsValid())
	{
		HideSingleGhostPreview();
		bHasValidPlacement = false;
		return;
	}
	
	EnsureSingleGhostActor();
	if (!SingleGhostActor)
	{
		bHasValidPlacement = false;
		return;
	}
	
	// First time, or if mesh/material changed.
	if (!SingleGhostActor->IsMeshSet() || SingleGhostActor->IsPreviewMeshDifferent(PreviewResult.MeshInstance.Mesh))
	{
		SingleGhostActor->SetPreviewMesh(PreviewResult.MeshInstance.Mesh, PreviewResult.MeshInstance.Material);
	}

	SingleGhostActor->SetActorLocation(PreviewResult.MeshInstance.WorldTransform.GetLocation());
	SingleGhostActor->SetActorRotation(PreviewResult.MeshInstance.WorldTransform.GetRotation());
	SingleGhostActor->SetPlacementValid(PreviewResult.CanPlace());
	SingleGhostActor->SetPreviewMeshVisible(true);

	bHasValidPlacement = PreviewResult.CanPlace();
}

void UAtomicPlayerPlacementComponent::HideAllPlacementPreviews()
{
	HideSingleGhostPreview();
	HideBeltLinePreview();
	bHasValidPlacement = false;
}

void UAtomicPlayerPlacementComponent::ApplyBeltLinePreview(const FAtomicPlacementPreviewResult& PreviewResult)
{
	if (!PreviewResult.IsValid() || PreviewResult.MeshInstances.IsEmpty())
	{
		HideBeltLinePreview();
		bHasValidPlacement = false;
		return;
	}
	
	EnsureBeltLineGhostActor();
	if (!BeltLineGhostActor)
	{
		bHasValidPlacement = false;
		return;
	}
	
	BeltLineGhostActor->SetActorHiddenInGame(false);
	BeltLineGhostActor->SetPreviewVisible(true);
	BeltLineGhostActor->ApplyPreviewInstances(PreviewResult.MeshInstances, PreviewResult.CanPlace());
	
	bHasValidPlacement = PreviewResult.CanPlace();
}

void UAtomicPlayerPlacementComponent::EnsureBeltLineGhostActor()
{
	if (BeltLineGhostActor) return;
	
	UWorld* World = GetWorld();
	if (!World) return;
	
	BeltLineGhostActor = World->SpawnActor<AAtomicBeltLineGhostActor>();
	if (BeltLineGhostActor)
	{
		BeltLineGhostActor->SetPreviewVisible(false);
		UE_LOG(LogGame, Log, TEXT("BeltLine GhostActor Created"));
	}
}

void UAtomicPlayerPlacementComponent::HideBeltLinePreview() const
{
	if (BeltLineGhostActor)
	{
		BeltLineGhostActor->ClearPreviewInstances();
		BeltLineGhostActor->SetPreviewVisible(false);
	}
}


// ---------------------------------------------------------------------
// Build Placement Preview
// ---------------------------------------------------------------------
bool UAtomicPlayerPlacementComponent::BuildPlacementPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult)
{
	OutResult = FAtomicPlacementPreviewResult{};

	switch (CurrentSelection.PlacementMode)
	{
	case EPlacementMode::Building:
		return BuildBuildingPreview(Target, OutResult);

	case EPlacementMode::Belt:
		return BuildBeltLinePreview(Target, OutResult);

	case EPlacementMode::Wall:
		return BuildWallPreview(Target, OutResult);

	default:
		return false;
	}
}

bool UAtomicPlayerPlacementComponent::BuildBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult)
{
	if (!CurrentSelection.IsValid()) return false;
	if (!Target.ShipGrid) return false;

	const UAtomicBuildingDefinition* Definition = CurrentSelection.BuildingDefinition;
	if (!Definition) return false;

	TArray<FIntVector> FootprintCells;
	UAtomicGridLibrary::GetFootprintCellsAroundPivot(
		Target.GridCoord,
		Definition->FootprintSize,
		Definition->PivotCell,
		CurrentSelection.Rotation,
		FootprintCells
	);

	// @todo: preload earlier if soft ptrs become a hitch.
	OutResult.MeshInstance.Mesh = Definition->PreviewMesh.LoadSynchronous();
	OutResult.MeshInstance.Material = Definition->PreviewMaterial.LoadSynchronous();
	
	const FVector WorldLocation = UAtomicGridLibrary::GetFootprintCenterWorldLocationFromCells(FootprintCells, Target.GridTransform, Target.CellSize);
	const FRotator WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(CurrentSelection.Rotation), 0.f);
	OutResult.MeshInstance.WorldTransform = FTransform(WorldRotation, WorldLocation, FVector::OneVector);
	
	OutResult.MeshInstance.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBuilding(Definition, Target.GridCoord, CurrentSelection.Rotation);
	OutResult.bShowGhost = OutResult.MeshInstance.Mesh != nullptr && OutResult.MeshInstance.Material != nullptr;
	OutResult.FootprintPreviewCells = MoveTemp(FootprintCells);

	OutResult.PreviewType = EAtomicPlacementPreviewType::SingleGhost;
	return OutResult.bShowGhost;
}

bool UAtomicPlayerPlacementComponent::BuildWallPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult)
{
	OutResult.PreviewType = EAtomicPlacementPreviewType::SingleGhost;
	return false;
}


// ---------------------------------------------------------------------
// Draw Placement Debugs
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::DrawDebugPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const
{
	// @todo: if (PreviewResult.PreviewType == EAtomicPlacementPreviewType::SingleGhost) {}
	
	const UWorld* World = GetWorld();
	if (!World) return;

	const FVector PivotWorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);

	// Hit Point.
	DrawDebugSphere(World, Target.HitResult.ImpactPoint, 16.f, 6, FColor::White, false, 0.f, 0, 2.f);

	// Anchor / Pivot Cell.
	DrawDebugBox(World, PivotWorldLocation, FVector(Target.CellSize * 0.5f, Target.CellSize * 0.5f, 20.f), FColor::Blue, false, 0.f, 0, 2.f);

	// Preview Occupied Cells.
	for (const FIntVector& CellCoord : PreviewResult.FootprintPreviewCells)
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
	DrawDebugSphere(World, PreviewResult.MeshInstance.WorldTransform.GetLocation() + FVector(0.f, 0.f, 10.f), 6.f, 4, FColor::Black, false, 0.f, 0, 3.f);

	// Ghost Visual Forward.
	const FVector Forward = PreviewResult.MeshInstance.WorldTransform.GetRotation().Vector().GetSafeNormal();
	const FVector ArrowStart = PreviewResult.MeshInstance.WorldTransform.GetLocation() + FVector(0.f, 0.f, 10.f);
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
		PreviewResult.CanPlace() ? TEXT("true") : TEXT("false")
	);

	DrawDebugString(World, PivotWorldLocation + FVector(0.f, 0.f, -80.f), DebugString, nullptr, FColor::White, 0.f, true, 1.1f);

	// Placement Mode Specific Debugs.
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

	const FVector ArrowStart = PivotWorldLocation + FVector(0.f, 0.f, 60.f);
	const FVector ArrowEnd = ArrowStart + WorldForward * (Target.CellSize * 0.5f);

	DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 35.f, FColor::Purple, false, 0.f, 0, 4.f);

	// Pivot Point.
	DrawDebugSphere(World, PivotWorldLocation + FVector(0.f, 0.f, 60.f), 6.f, 4, FColor::Purple, false, 0.f, 0, 3.f);

	// Building Ports.
	TArray<FAtomicResolvedBuildingPort> ResolvedPorts;
	UAtomicGridLibrary::GetResolvedBuildingPorts(
		FGuid(),
		CurrentSelection.DefinitionID,
		Target.GridCoord,
		CurrentSelection.BuildingDefinition->PivotCell,
		CurrentSelection.Rotation,
		CurrentSelection.BuildingDefinition->PortDefinitions,
		ResolvedPorts
	);

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
	// if (!PreviewResult.bHasResolvedBelt) return;
	//
	// const UWorld* World = GetWorld();
	// if (!World) return;
	//
	// if (!CurrentSelection.BeltDefinition) return;
	// if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return;
	//
	// const FAtomicResolvedPreviewBelt& BeltPreview = PreviewResult.ResolvedPreviewBelt;
	// const FVector CellCenter = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
	//
	// // Small arrows only for valid neighbour connections.
	// for (const EGridDirection Port : BeltPreview.ConnectedRoutePorts)
	// {
	// 	const FVector LocalDirection = UAtomicGridLibrary::GridDirectionToVector(Port);
	// 	const FVector WorldDirection = Target.GridTransform.TransformVectorNoScale(LocalDirection).GetSafeNormal();
	// 	const FVector EdgeCenter = CellCenter + WorldDirection * (Target.CellSize * 0.4f) + FVector(0.f, 0.f, 70.f);
	// 	const FVector ConnectedArrowStart = EdgeCenter - WorldDirection * (Target.CellSize * 0.12f);
	// 	const FVector ConnectedArrowEnd = EdgeCenter + WorldDirection * (Target.CellSize * 0.12f);
	//
	// 	DrawDebugDirectionalArrow(World, ConnectedArrowStart, ConnectedArrowEnd, 18.f, FColor::Cyan, false, 0.f, 0, 3.f);
	// }
	//
	// // Input direction marker.
	// {
	// 	const FVector InputLocalDirection = UAtomicGridLibrary::GridDirectionToVector(BeltPreview.InputPort);
	// 	const FVector InputWorldDirection = Target.GridTransform.TransformVectorNoScale(InputLocalDirection).GetSafeNormal();
	// 	const FVector InputArrowStart = CellCenter + InputWorldDirection * (Target.CellSize * 0.45f) + FVector(0.f, 0.f, 78.f);
	// 	const FVector InputArrowEnd = CellCenter + FVector(0.f, 0.f, 78.f);
	//
	// 	DrawDebugDirectionalArrow(World, InputArrowStart, InputArrowEnd, 24.f, FColor::Blue, false, 0.f, 0, 4.f);
	// }
	//
	// // Main flow arrow.
	// {
	// 	const FVector FlowLocalDirection = UAtomicGridLibrary::GridDirectionToVector(BeltPreview.OutputPort);
	// 	const FVector FlowWorldDirection = Target.GridTransform.TransformVectorNoScale(FlowLocalDirection).GetSafeNormal();
	// 	const FVector FlowArrowStart = CellCenter + FVector(0.f, 0.f, 70.f);
	// 	const FVector FlowArrowEnd = FlowArrowStart + FlowWorldDirection * (Target.CellSize * 0.45f);
	//
	// 	DrawDebugDirectionalArrow(World, FlowArrowStart, FlowArrowEnd, 35.f, FColor::Purple, false, 0.f, 0, 5.f);
	// }
	//
	// const FString DebugText = FString::Printf(
	// 	TEXT("BELT PREVIEW\nRouteType: %s\nInput: %s\nOutput: %s\nVariant: %s\nVisualRot: %s\nRoutePorts: %s\nConnected: %s"),
	// 	*UEnum::GetValueAsString(BeltPreview.RouteType),
	// 	*UEnum::GetValueAsString(BeltPreview.InputPort),
	// 	*UEnum::GetValueAsString(BeltPreview.OutputPort),
	// 	*UEnum::GetValueAsString(BeltPreview.VisualVariant),
	// 	*UEnum::GetValueAsString(BeltPreview.VisualRotation),
	// 	*GridDirectionArrayToString(BeltPreview.RoutePorts),
	// 	*GridDirectionArrayToString(BeltPreview.ConnectedRoutePorts)
	// );
	//
	// DrawDebugString(World, CellCenter + FVector(0.f, 0.f, 150.f), DebugText, nullptr, FColor::White, 0.f, true, 1.f);
}



// ---------------------------------------------------------------------
// BELTS
// ---------------------------------------------------------------------

// Called from AAtomicPlayerController::Client_PlaceBeltAccepted_Implementation() after RPC request
void UAtomicPlayerPlacementComponent::StartBeltLinePlacement(const FAtomicPlacementTarget& StartTarget)
{
	if (!StartTarget.IsValid())
	{
		CancelBeltLinePlacement();
		return;
	}

	BeltLineStartTarget = StartTarget;
	bIsDraggingBeltLine = true;

	UE_LOG(LogGame, Warning, TEXT("Start Belt Line Placement"));
}

bool UAtomicPlayerPlacementComponent::BuildBeltLinePreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult)
{
	if (!CurrentSelection.IsValid()) return false;
	if (!Target.IsValid() || !Target.ShipGrid || !Target.ShipGrid->GetGridData() || !Target.ShipGrid->GetGridPlacementComponent()) return false;
	
	const UAtomicBeltDefinition* Definition = CurrentSelection.BeltDefinition;
	if (!Definition) return false;

	if (bIsDraggingBeltLine)
	{
		if (BeltLineStartTarget.ShipGrid != Target.ShipGrid) return false;
		if (!BeltLineStartTarget.IsValid()) return false;
		
		// Update Belt Line Path
		if (!BuildBeltLinePath(BeltLineStartTarget.GridCoord, Target.GridCoord, bBeltLinePreferXFirst, OutResult.BeltCells)) return false;
		
		if (OutResult.BeltCells.IsEmpty()) return false;
		
		OutResult.MeshInstances.Reset();
		
		// Update Belt Line Preview
		for (int32 BeltLineIndex = 0; BeltLineIndex < OutResult.BeltCells.Num(); ++BeltLineIndex)
		{
			const FAtomicBeltPlacementCell& BeltCell = OutResult.BeltCells[BeltLineIndex];
			FAtomicPreviewMeshInstance BeltMeshInstance = FAtomicPreviewMeshInstance{};

			EBuildingRotation VisualRotation;
			EAtomicBeltVisualVariant VisualVariant;
			if (!FAtomicBeltVisualResolver::ResolveVisualRotationFromPorts(BeltCell.InputPort, BeltCell.OutputPort, VisualRotation)) return false;
			if (!FAtomicBeltVisualResolver::ResolveVisualVariantFromPorts(BeltCell.InputPort, BeltCell.OutputPort, VisualVariant)) return false;

			const FVector WorldLocation = UAtomicGridLibrary::GridToWorld(BeltCell.GridCoord, Target.GridTransform, Target.CellSize);
			const FRotator WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(VisualRotation), 0.f);
			
			switch (VisualVariant)
			{
			case EAtomicBeltVisualVariant::Straight:
				BeltMeshInstance.Mesh = Definition->StraightMesh;
				break;
				
			case EAtomicBeltVisualVariant::TurnLeft:
				BeltMeshInstance.Mesh = Definition->TurnLeftMesh;
				break;
				
			case EAtomicBeltVisualVariant::TurnRight:
				BeltMeshInstance.Mesh = Definition->TurnRightMesh;
				break;
				
			default:
				return false;
			}
			
			BeltMeshInstance.WorldTransform = FTransform(WorldRotation, WorldLocation, FVector::OneVector);
			BeltMeshInstance.Material = Definition->PreviewMaterial;
			BeltMeshInstance.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBeltCell(Definition, BeltCell.GridCoord, CurrentSelection.Rotation);
			
			OutResult.MeshInstances.Add(BeltMeshInstance);

			///////////// DEBUG BELT CELL BOX ////////////////////////////
			const FVector CellWorldLocation = UAtomicGridLibrary::GridToWorld(BeltCell.GridCoord, Target.GridTransform, Target.CellSize);
			bool bCellValid = false;
			if (Target.ShipGrid && Target.ShipGrid->GetGridPlacementComponent())
			{
				bCellValid = Target.ShipGrid->GetGridPlacementComponent()->CheckIfValidCell(BeltCell.GridCoord);
			}
			const FColor ValidityColor = bCellValid ? FColor::Green : FColor::Red;
			DrawDebugBox(GetWorld(), CellWorldLocation + FVector(0.f, 0.f, 10.f), FVector(Target.CellSize * 0.5f, Target.CellSize * 0.5f, 10.f), ValidityColor, false, 0.f, 0, 2.f);
		}
		
		OutResult.bShowGhost = OutResult.MeshInstances.Num() > 0;
		OutResult.bCanPlaceLine = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBeltLine(Definition, OutResult.BeltCells);
		OutResult.PreviewType = EAtomicPlacementPreviewType::BeltLine;
		CurrentBeltLinePreviewCells = OutResult.BeltCells;
		if (OutResult.bShowGhost)
		{
			return true;
		}		
	}
	else
	{
		// Single Belt Preview
		if (OutResult.MeshInstance.Mesh != Definition->StraightMesh || OutResult.MeshInstance.Material != Definition->PreviewMaterial)
		{
			OutResult.MeshInstance.Mesh = Definition->StraightMesh;
			OutResult.MeshInstance.Material = Definition->PreviewMaterial;
		}
	
		const FVector WorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
		const FRotator WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(CurrentSelection.Rotation), 0.f);
		OutResult.MeshInstance.WorldTransform = FTransform(WorldRotation, WorldLocation, FVector::OneVector);
	
		OutResult.MeshInstance.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBeltCell(Definition, Target.GridCoord, CurrentSelection.Rotation);
		OutResult.bShowGhost = OutResult.MeshInstance.Mesh != nullptr && OutResult.MeshInstance.Material != nullptr;

		OutResult.PreviewType = EAtomicPlacementPreviewType::SingleGhost;
		return OutResult.bShowGhost;
	}
	
	return false;
}

bool UAtomicPlayerPlacementComponent::BuildBeltLinePath(const FIntVector& StartCoord, const FIntVector& EndCoord, const bool bXFirst, TArray<FAtomicBeltPlacementCell>& OutCells)
{
	// Generate ordered grid coords
	// Straight line: if same X or same Y
	// Diagonal / L-shape: if both X and Y change
	// X-first: Start -> horizontal bend -> vertical end
	// Y-first: Start -> vertical bend -> horizontal end
	
	// Port Calculation: for each cell, PreviousCoord -> CurrentCoord -> NextCoord
	// InputPort = direction from current cell to previous cell
	// OutputPort = direction from current cell to next cell
	// For Endpoints: First Cell, InputPort = opposite(OutputPort); Last Cell, OutputPort = opposite(InputPort)
	
	OutCells.Reset();
	
	if (StartCoord.Z != EndCoord.Z) return false;
	
	TArray<FIntVector> PathCoords;
	
	// 1. Build Ordered Coords
	if (StartCoord == EndCoord)
	{
		PathCoords.Add(StartCoord);
	}
	else if (StartCoord.X == EndCoord.X || StartCoord.Y == EndCoord.Y)
	{
		BuildStraightSegment(StartCoord, EndCoord, PathCoords, false);
	}
	else
	{
		FIntVector BendCoord = bXFirst
			? FIntVector(EndCoord.X, StartCoord.Y, StartCoord.Z)
			: FIntVector(StartCoord.X, EndCoord.Y, StartCoord.Z);
		
		BuildStraightSegment(StartCoord, BendCoord, PathCoords, false);
		BuildStraightSegment(BendCoord, EndCoord, PathCoords, true);
	}
	
	if (PathCoords.Num() == 0) return false;
	
	// 2. Convert coords into belt cells with ports
	for (int32 CoordIndex = 0; CoordIndex < PathCoords.Num(); ++CoordIndex)
	{
		FIntVector CurrentCoord = PathCoords[CoordIndex];
		FIntVector PrevCoord = CoordIndex > 0 ? PathCoords[CoordIndex - 1] : FIntVector::NoneValue;
		FIntVector NextCoord = CoordIndex < PathCoords.Num() - 1 ? PathCoords[CoordIndex + 1] : FIntVector::NoneValue;
		
		FAtomicBeltPlacementCell Cell;
		Cell.GridCoord = CurrentCoord;
		Cell.CellIndex = UAtomicGridLibrary::GridToIndexUnchecked(CurrentCoord, CurrentTarget.GridSize);
		
		// Single Cell
		if (PathCoords.Num() == 1)
		{
			Cell.InputPort = EGridDirection::West;
			Cell.OutputPort = EGridDirection::East;
		}
		// First Cell
		else if (CurrentCoord == PathCoords[0]) 
		{
			const EGridDirection OutputDirection = UAtomicGridLibrary::DirectionFromCoordToCoord(CurrentCoord, NextCoord);
			Cell.OutputPort = OutputDirection;
			Cell.InputPort = UAtomicGridLibrary::OppositeGridDirection(OutputDirection);
		}
		// Last Cell
		else if (CurrentCoord == PathCoords.Last())
		{
			const EGridDirection InputDirection = UAtomicGridLibrary::DirectionFromCoordToCoord(CurrentCoord, PrevCoord);
			Cell.InputPort = InputDirection;
			Cell.OutputPort = UAtomicGridLibrary::OppositeGridDirection(InputDirection);
		}
		// All Other Cells
		else
		{
			Cell.InputPort = UAtomicGridLibrary::DirectionFromCoordToCoord(CurrentCoord, PrevCoord);
			Cell.OutputPort = UAtomicGridLibrary::DirectionFromCoordToCoord(CurrentCoord, NextCoord);
		}
		
		OutCells.Add(Cell);
	}

	return true;
}

// This assumes the segment is already straight: same X or same Y
void UAtomicPlayerPlacementComponent::BuildStraightSegment(const FIntVector& StartCoord, const FIntVector& EndCoord, TArray<FIntVector>& OutPath, const bool bSkipFirstCoord)
{
	const int32 DeltaX = FMath::Sign(EndCoord.X - StartCoord.X);
	const int32 DeltaY = FMath::Sign(EndCoord.Y - StartCoord.Y);
	
	FIntVector CurrentCoord = StartCoord;
	
	if (!bSkipFirstCoord)
	{
		OutPath.Add(CurrentCoord);
	}

	while (CurrentCoord != EndCoord)
	{
		CurrentCoord.X += DeltaX;
		CurrentCoord.Y += DeltaY;
		
		OutPath.Add(CurrentCoord);
	}
}

void UAtomicPlayerPlacementComponent::RotateBeltPlacement(const float InputValue)
{
	if (FMath::IsNearlyZero(InputValue)) return;
	
	// Swap Bend Order
	bBeltLinePreferXFirst = !bBeltLinePreferXFirst;
	
	// Not needed for belts, just for upto date bookeeping.
	if (InputValue > 0.f)
	{
		CurrentSelection.Rotation = UAtomicGridLibrary::RotateBuildingClockwise(CurrentSelection.Rotation);
	}
	else
	{
		CurrentSelection.Rotation = UAtomicGridLibrary::RotateBuildingCounterClockwise(CurrentSelection.Rotation);
	}
}

void UAtomicPlayerPlacementComponent::ToggleBeltLineBendOrder()
{
	
}
