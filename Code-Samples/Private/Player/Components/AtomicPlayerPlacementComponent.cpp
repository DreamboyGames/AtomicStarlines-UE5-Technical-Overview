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

bool UAtomicPlayerPlacementComponent::StartPlacementWithSelectedBelt(const FName BeltID, const EAtomicBeltShape BeltShape)
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
	CurrentSelection.BeltShape = BeltShape;
	CurrentSelection.Rotation = EBuildingRotation::East;
	CurrentSelection.OutputFlowDirection = EGridDirection::East;
	
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
		LocalController->Server_RequestPlaceBelt(CurrentSelection.DefinitionID, CurrentTarget.GridCoord, CurrentSelection.BeltShape, CurrentSelection.Rotation, CurrentSelection.OutputFlowDirection, CurrentTarget.ShipGrid);
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
	
	SetBeltOutputToDefaultForCurrentRoute();
}


// ---------------------------------------------------------------------
// BELTS
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::RotateBeltPlacement(const float InputValue)
{
	if (FMath::IsNearlyZero(InputValue)) return;
	if (!CurrentTarget.IsValid()) return;
	if (!CurrentTarget.ShipGrid || !CurrentTarget.ShipGrid->GetGridData()) return;

	const bool bRotateClockwise = InputValue > 0.f;
	
	// From now on, the player is overriding the auto choice for this target cell.
	bBeltRotationManuallyOverridden = true;
	
	TArray<FAtomicBeltPlacementCandidate> BeltPlacementCandidates;
	BuildBeltPlacementCandidates(CurrentTarget, BeltPlacementCandidates);
	
	if (BeltPlacementCandidates.IsEmpty())
	{
		// Rotate Belt Normally
		CurrentSelection.Rotation = bRotateClockwise
			? UAtomicGridLibrary::RotateBuildingClockwise(CurrentSelection.Rotation)
			: UAtomicGridLibrary::RotateBuildingCounterClockwise(CurrentSelection.Rotation);
		
		EnsureOutputDirectionIsValidForCurrentBelt();
		return;
	}
	
	// Build clockwise/counter-clockwise route order.
	TArray<EBuildingRotation> RotationSearchOrder;
	GetBeltRotationSearchOrder(CurrentSelection.Rotation, bRotateClockwise, RotationSearchOrder);
	
	// First Preference:
	// Find the next route roation that has at least one valid connection.
	for (const EBuildingRotation CandidateRotation : RotationSearchOrder)
	{
		if (CandidateRotation == CurrentSelection.Rotation)
		{
			continue;
		}
		
		FAtomicBeltPlacementCandidate BestCandidateForRotation;
		if (TryChooseBestOutputForRouteRotation(BeltPlacementCandidates, CandidateRotation, true, BestCandidateForRotation)) // require connection
		{
			// This picks the best OutputFlowDirection for this route rotation.
			// It does Not cycle all output variants.
			CurrentSelection.Rotation = BestCandidateForRotation.Rotation;
			CurrentSelection.OutputFlowDirection = BestCandidateForRotation.OutputFlowDirection;
			EnsureOutputDirectionIsValidForCurrentBelt();
			return;
		}
	}
	
	// Second Preference:
	// No connected route alternative exists.
	// Force one normal rotation step so the player can still override.
	const EBuildingRotation ForcedRotation = bRotateClockwise
		? UAtomicGridLibrary::RotateBuildingClockwise(CurrentSelection.Rotation)
		: UAtomicGridLibrary::RotateBuildingCounterClockwise(CurrentSelection.Rotation);
	
	FAtomicBeltPlacementCandidate BestForcedCandidate;
	if (TryChooseBestOutputForRouteRotation(BeltPlacementCandidates, ForcedRotation, false, BestForcedCandidate)) // allow disconnected
	{
		CurrentSelection.Rotation = BestForcedCandidate.Rotation;
		CurrentSelection.OutputFlowDirection = BestForcedCandidate.OutputFlowDirection;
		EnsureOutputDirectionIsValidForCurrentBelt();
		return;
	}
	
	// Last fallback.
	CurrentSelection.Rotation = BestForcedCandidate.Rotation;
	SetBeltOutputToDefaultForCurrentRoute();
}

bool UAtomicPlayerPlacementComponent::TryGetOtherRoutePort(const TArray<EGridDirection>& RoutePorts, const EGridDirection KnownPort, EGridDirection& OutOtherPort) const
{
	if (RoutePorts.Num() != 2) return false;
	
	if (RoutePorts[0] == KnownPort)
	{
		OutOtherPort = RoutePorts[1];
		return true;
	}
	
	if (RoutePorts[1] == KnownPort)
	{
		OutOtherPort = RoutePorts[0];
		return true;
	}
	
	return false;
}

// This preserves normal clockwise/counter-clockwise feel
void UAtomicPlayerPlacementComponent::GetBeltRotationSearchOrder(const EBuildingRotation StartRotation, const bool bClockwise, TArray<EBuildingRotation>& OutRotations) const
{
	OutRotations.Reset();
	
	EBuildingRotation TestRotation = StartRotation;
	
	for (int32 Step = 0; Step < 4; ++Step)
	{
		TestRotation = bClockwise 
			? UAtomicGridLibrary::RotateBuildingClockwise(TestRotation)
			: UAtomicGridLibrary::RotateBuildingCounterClockwise(TestRotation);
		
		OutRotations.Add(TestRotation);
	}
}

// After any rotation/shape change, make sure output is still physically possible.
void UAtomicPlayerPlacementComponent::EnsureOutputDirectionIsValidForCurrentBelt()
{
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(CurrentSelection.BeltShape, CurrentSelection.Rotation);
	if (RoutePorts.Contains(CurrentSelection.OutputFlowDirection))
	{
		return;
	}
	
	SetBeltOutputToDefaultForCurrentRoute();
}

void UAtomicPlayerPlacementComponent::SetBeltOutputToDefaultForCurrentRoute()
{
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(CurrentSelection.BeltShape, CurrentSelection.Rotation);
	
	// Convention:
	// RoutePorts[0] = default input side
	// RoutePorts[1] = default output side
	if (RoutePorts.Num() >= 2)
	{
		CurrentSelection.OutputFlowDirection = RoutePorts[1];
		return;
	}
	
	if (RoutePorts.Num() == 1)
	{
		CurrentSelection.OutputFlowDirection = RoutePorts[0];
		return;
	}
	
	CurrentSelection.OutputFlowDirection = EGridDirection::East;
}

void UAtomicPlayerPlacementComponent::FlipBeltFlowDirection()
{
	if (CurrentSelection.PlacementMode != EPlacementMode::Belt) return;
	
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(CurrentSelection.BeltShape, CurrentSelection.Rotation);
	if (RoutePorts.Num() != 2) return;
	
	CurrentSelection.OutputFlowDirection = (CurrentSelection.OutputFlowDirection == RoutePorts[0]) ? RoutePorts[1] : RoutePorts[0];
	bBeltRotationManuallyOverridden = true;
}

// ---------------------------------------------------------------------
// Belt Placement Candidates
// ---------------------------------------------------------------------
void UAtomicPlayerPlacementComponent::UpdateAutoBeltSelectionForTarget(const FAtomicPlacementTarget& Target)
{
	if (CurrentSelection.PlacementMode != EPlacementMode::Belt) return;
	if (!Target.IsValid()) return;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return;
	
	const bool bTargetChanged = LastTargetGridIndex != Target.GridIndex || LastTargetShipGrid.Get() != Target.ShipGrid;
	if (bTargetChanged)
	{
		LastTargetGridIndex = Target.GridIndex;
		LastTargetShipGrid = Target.ShipGrid;
		
		// New cell = allow auto-connect again.
		bBeltRotationManuallyOverridden = false;
	}
	
	// Player manually rotated on this target cell.
	// Do not fight player's rotation
	if (bBeltRotationManuallyOverridden)
	{
		EnsureOutputDirectionIsValidForCurrentBelt();
		return;
	}
	
	TArray<FAtomicBeltPlacementCandidate> BeltPlacementCandidates;
	BuildBeltPlacementCandidates(Target, BeltPlacementCandidates);

	FAtomicBeltPlacementCandidate BestCandidate;
	
	// Auto-default should only snap if there is at least one real connection.
	// if there are no neighbours, preserve the player's current/default rotation.
	if (TryChooseBestBeltCandidate(BeltPlacementCandidates, true, BestCandidate))
	{
		CurrentSelection.Rotation = BestCandidate.Rotation;
		CurrentSelection.OutputFlowDirection = BestCandidate.OutputFlowDirection;
		EnsureOutputDirectionIsValidForCurrentBelt();
		return;
	}
	
	// Isolated belt. Keep current rotation, just make sure output is valid.
	EnsureOutputDirectionIsValidForCurrentBelt();
}

void UAtomicPlayerPlacementComponent::BuildBeltPlacementCandidates(const FAtomicPlacementTarget& Target, TArray<FAtomicBeltPlacementCandidate>& OutCandidates) const
{
	OutCandidates.Reset();
	
	if (!Target.IsValid()) return;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return;
	
	const UAtomicGridDataComponent* GridData = Target.ShipGrid->GetGridData();
	
	for (const EBuildingRotation CandidateRotation : AllRotations)
	{
		const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(CurrentSelection.BeltShape, CandidateRotation);
		if (RoutePorts.IsEmpty()) continue;
		
		TArray<EGridDirection> ConnectedRoutePorts;
		GridData->GetConnectedRoutePortsForBeltCandidate(Target.GridIndex, CurrentSelection.BeltShape, CandidateRotation, ConnectedRoutePorts);
		
		for (const EGridDirection OutputDirection : RoutePorts)
		{
			FAtomicBeltPlacementCandidate Candidate;
			Candidate.Shape = CurrentSelection.BeltShape;
			Candidate.Rotation = CandidateRotation;
			Candidate.OutputFlowDirection = OutputDirection;
			Candidate.RoutePorts = RoutePorts;
			Candidate.ConnectedRoutePorts = ConnectedRoutePorts;
			Candidate.Score = ScoreBeltPlacementCandidate(Target, Candidate);
			
			OutCandidates.Add(Candidate);
		}
	}
}

bool UAtomicPlayerPlacementComponent::TryChooseBestBeltCandidate(const TArray<FAtomicBeltPlacementCandidate>& Candidates, const bool bRequireConnection, FAtomicBeltPlacementCandidate& OutBestCandidate) const
{
	bool bFound = false;
	int32 BestScore = MIN_int32;

	for (const FAtomicBeltPlacementCandidate& Candidate : Candidates)
	{
		if (bRequireConnection && Candidate.ConnectedRoutePorts.IsEmpty())
		{
			continue;
		}
		
		if (!bFound || Candidate.Score > BestScore)
		{
			bFound = true;
			BestScore = Candidate.Score;
			OutBestCandidate = Candidate;
		}
	}
	
	return bFound;
}

// Choose Best OUTPUT direction for route rotation
bool UAtomicPlayerPlacementComponent::TryChooseBestOutputForRouteRotation(const TArray<FAtomicBeltPlacementCandidate>& Candidates, const EBuildingRotation Rotation, const bool bRequireConnection, FAtomicBeltPlacementCandidate& OutBestCandidate) const
{
	bool bFound = false;
	int32 BestScore = MIN_int32;

	for (const FAtomicBeltPlacementCandidate& Candidate : Candidates)
	{
		if (Candidate.Rotation != Rotation)
		{
			continue;
		}
		
		if (bRequireConnection && Candidate.ConnectedRoutePorts.IsEmpty())
		{
			continue;
		}
		
		if (!bFound || Candidate.Score > BestScore)
		{
			bFound = true;
			BestScore = Candidate.Score;
			OutBestCandidate = Candidate;
		}
	}
	
	return bFound;
}

int32 UAtomicPlayerPlacementComponent::ScoreBeltPlacementCandidate(const FAtomicPlacementTarget& Target, const FAtomicBeltPlacementCandidate& Candidate) const
{
	// This function is only a heuristic.
	// It does Not validate final placement.
	// It answers: "How good does this route rotation + output direction feel for auto-preview?"
	
	// Why These Score Values? (Priority Bands)
	// +1000 per connection		= physical route connection matters most
	// +500-ish flow score		= logical item movement matters second
	// +20/30/60 tie-breakers	= preserve current/player intent
	// +40 isolated default		= make no-neighbour belts predictable
	
	static constexpr int32 InvalidScore = -1000000;
	
	if (!Target.IsValid()) return InvalidScore;
	if (!Target.ShipGrid || !Target.ShipGrid->GetGridData()) return InvalidScore;
	
	// Output direction must always be one of the belt's route ports.
	if (!Candidate.RoutePorts.Contains(Candidate.OutputFlowDirection)) return InvalidScore;

	const UAtomicGridDataComponent* GridData = Target.ShipGrid->GetGridData();
	
	int32 Score = 0;
	
	// 1. CONNECTION SCORE:
	// A belt that physically connects to more reciprocal neighbour ports should usually be preferred over one that floats disconnected.
	const int32 ConnectedPortCount = Candidate.ConnectedRoutePorts.Num();
	Score += ConnectedPortCount * 1000;
	
	// Small bonus for fully connected 2-port belts.
	// Straight/Corner with both route ports connected should generally be preferred.
	if (ConnectedPortCount == Candidate.RoutePorts.Num() && Candidate.RoutePorts.Num() > 0)
	{
		Score += 250;
	}
	
	// Small penalty for disconnected candidates.
	// Do not make this huge, because isolated placement is still valid.
	if (ConnectedPortCount == 0)
	{
		Score -= 50;
	}
	
	
	// 2. FLOW COMPATIBILITY SCORE:
	// For each connected route port, inspect the neighbouring belt's output.
	//
	// Candidate Cell: ConnectedPort = direction from candidate TO neighbour.
	// Neighbour Cell: NeighbourPortTowardCandidate = opposite side
	//
	// Example:
	//		Candidate has neighbour on West
	//		ConnectedPort = West
	//		NeighbourPortTowardCandidate = East
	//
	// If neighbour outputs East, it is sending items into this candidate.
	// Then this candidate should output somewhere else, not back into the neighbour.

	for (const EGridDirection ConnectedPort : Candidate.ConnectedRoutePorts)
	{
		const FAtomicBeltRecord* NeighbourBelt = Target.ShipGrid->GetGridData()->GetNeighbourBeltRecord(Target.GridIndex, ConnectedPort);
		if (!NeighbourBelt) continue;
		
		const EGridDirection NeighbourPortTowardCandidate = UAtomicGridLibrary::OppositeGridDirection(ConnectedPort);
		const TArray<EGridDirection> NeighbourRoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(NeighbourBelt->Shape, NeighbourBelt->Rotation);
		
		// This should already be true if ConnectedRoutePorts was built correctly.
		if (!NeighbourRoutePorts.Contains(NeighbourPortTowardCandidate)) continue;
		
		const bool bCandidateOutputsTowardNeighbour = Candidate.OutputFlowDirection == ConnectedPort;
		const bool bNeighbourOutputsTowardCandidate = NeighbourBelt->OutputFlowDirection == NeighbourPortTowardCandidate;
		
		// Best Case: Neighbour sends into candidate, candidate sends away.
		// Flow: Neighbour -> Candidate -> Other Side
		if (bNeighbourOutputsTowardCandidate && !bCandidateOutputsTowardNeighbour)
		{
			Score += 500;
			continue;
		}
		
		// Also Good: Candidate sends into neighbour, and neighbour is not outputting back.
		// Flow: Candidate -> Neighbour -> Other Side
		if (!bNeighbourOutputsTowardCandidate && bCandidateOutputsTowardNeighbour)
		{
			Score += 450;
			continue;
		}
		
		// Bad: Both belts output into each other.
		// Flow: Candidate -> <- Neighbour
		if (bNeighbourOutputsTowardCandidate && bCandidateOutputsTowardNeighbour)
		{
			Score -= 350;
			continue;
		}
		
		// Weird / Weak Case: Both belts output away from the connection.
		// Flow: Candidate <- connection -> Neighbour
		Score -= 100;
	}
	
	// 3. PRESERVE CURRENT PREVIEW INTENT WHERE POSSIBLE
	// These are small tie-breakers.
	// They should never overpower actual connection + flow scoring.
	if (Candidate.Rotation == CurrentSelection.Rotation)
	{
		Score += 20;
	}
	
	if (Candidate.OutputFlowDirection == CurrentSelection.OutputFlowDirection)
	{
		Score += 50;
	}
	
	// Try to preserve old input side if possible.
	// For simple 2-port belts: Input = the route port that is not output.
	// This helps rotation feel stable instead of randomly flipping flow.
	TArray<EGridDirection> CurrentRoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(CurrentSelection.BeltShape, CurrentSelection.Rotation);
	if (CurrentRoutePorts.Num() == 2 && CurrentRoutePorts.Contains(CurrentSelection.OutputFlowDirection))
	{
		EGridDirection CurrentInputDirection = CurrentRoutePorts[0];
		if (CurrentRoutePorts[0] == CurrentSelection.OutputFlowDirection)
		{
			CurrentInputDirection = CurrentRoutePorts[1];
		}
		
		// If the new candidate still has the previous input side, prefer using the other route port as output.
		if (Candidate.RoutePorts.Contains(CurrentInputDirection) && Candidate.OutputFlowDirection != CurrentInputDirection)
		{
			Score += 60;
		}
	}
	
	// 4. DEFAULT ISOLATED-BELT PREFERENCE
	// If there are no connections, prefer authoring default:
	// RoutePort[0] = input side
	// RoutePort[1] = output side
	if (ConnectedPortCount == 0 && Candidate.RoutePorts.Num() >= 2)
	{
		const EGridDirection DefaultOutputDirection = Candidate.RoutePorts[1];
		if (Candidate.OutputFlowDirection == DefaultOutputDirection)
		{
			Score += 40;
		}
	}
	
	return Score;
}

EGridDirection UAtomicPlayerPlacementComponent::ChooseOutputDirectionForBeltCandidate(const EAtomicBeltShape BeltShape, const EBuildingRotation OldRotation, const EGridDirection OldOutputDirection,
																					  const EBuildingRotation CandidateRotation, const TArray<EGridDirection>& ConnectedPorts) const
{
	const TArray<EGridDirection> CandidateRoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(BeltShape, CandidateRotation);
	
	// 1. Prefer keeping the same output flow direction.
	if (CandidateRoutePorts.Contains(OldOutputDirection))
	{
		return OldOutputDirection;
	}
	
	// 2. Prefer preserving old input side, then redirecting output.
	EGridDirection OldInputDirection;
	if (FAtomicBeltTopologyRules::TryGetInputGridDirection(BeltShape, OldRotation, OldOutputDirection, OldInputDirection))
	{
		EGridDirection NewOutputDirection;
		if (TryGetOtherRoutePort(CandidateRoutePorts, OldInputDirection, NewOutputDirection))
		{
			return NewOutputDirection;
		}
	}
	
	// 3. If connected to one neighbour, treat that side as input and output away from it.
	if (ConnectedPorts.Num() == 1)
	{
		EGridDirection NewOutputDirection;
		if (TryGetOtherRoutePort(CandidateRoutePorts, ConnectedPorts[0], NewOutputDirection))
		{
			return NewOutputDirection;
		}
	}
	
	// 4. Fallback.
	return CandidateRoutePorts.IsEmpty() ? EGridDirection::East : CandidateRoutePorts[0];
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
	
	// Auto belt selection must happen after target is found and before BuildPlacementPreview()
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
	if (!Target.ShipGrid) return false;

	FAtomicBeltRecord PreviewRecord;
	PreviewRecord.InstanceID = FGuid();
	PreviewRecord.BeltID = CurrentSelection.DefinitionID;
	PreviewRecord.CellIndex = Target.GridIndex;
	PreviewRecord.Shape = CurrentSelection.BeltShape;
	PreviewRecord.Rotation = CurrentSelection.Rotation;
	PreviewRecord.OutputFlowDirection = CurrentSelection.OutputFlowDirection;
	
	FAtomicResolvedPreviewBelt ResolvedPreview;
	if (!Target.ShipGrid->ResolveBeltPreviewVisual(PreviewRecord, ResolvedPreview)) return false;
	
	const UAtomicBeltDefinition* Definition = CurrentSelection.BeltDefinition;
	
	OutResult.Mesh = Definition->GetMeshForVariant(ResolvedPreview.VisualVariant);
	OutResult.Material = Definition->PreviewMaterial.Get();
	
	OutResult.WorldLocation = UAtomicGridLibrary::GridToWorld(Target.GridCoord, Target.GridTransform, Target.CellSize);
	OutResult.WorldRotation = FRotator(0.f, UAtomicGridLibrary::BuildingRotationToYawDegrees(ResolvedPreview.VisualRotation), 0.f);
	
	OutResult.bCanPlace = Target.ShipGrid->GetGridPlacementComponent()->CanPlaceBelt(Definition, Target.GridCoord, CurrentSelection.BeltShape, CurrentSelection.Rotation, CurrentSelection.OutputFlowDirection);
	
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
	const EGridDirection OutputDirection = PreviewResult.ResolvedPreviewBelt.OutputFlowDirection;
	const FVector FlowLocalDirection = UAtomicGridLibrary::GridDirectionToVector(OutputDirection);
	const FVector FlowWorldDirection = Target.GridTransform.TransformVectorNoScale(FlowLocalDirection).GetSafeNormal();
	const FVector FlowArrowStart = CellCenter + FVector(0.f, 0.f, 70.f);
	const FVector FlowArrowEnd = FlowArrowStart + FlowWorldDirection * (Target.CellSize * 0.4f);
	
	DrawDebugDirectionalArrow(World, FlowArrowStart, FlowArrowEnd, 35.f, FColor::Purple, false, 0.f, 0, 4.f);
	
	// Belt Shape Text
	const FString BeltString = FString::Printf(
		TEXT("Belt Shape: %s"),
		*UEnum::GetValueAsString(CurrentSelection.BeltShape)
	);
	DrawDebugString(World, CellCenter + FVector(0.f, 0.f, 150.f), BeltString, nullptr, FColor::Purple, 0.f, true, 1.1f);
}

