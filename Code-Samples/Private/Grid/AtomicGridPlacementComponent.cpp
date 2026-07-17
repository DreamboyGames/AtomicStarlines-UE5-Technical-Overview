// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicGridPlacementComponent.h"

#include "AtomicStarlines.h"
#include "Building/AtomicBuildingActor.h"
#include "Logging/StructuredLog.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Core/AtomicPlayerState.h"
#include "Engine/OverlapResult.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicShipGrid.h"

namespace {
	bool IsValidCardinalGridDirection(const EGridDirection Direction)
	{
		switch (Direction)
		{
		case EGridDirection::North:
		case EGridDirection::East:
		case EGridDirection::South:
		case EGridDirection::West:
			return true;
			
		default:
			return false;
		}
	}
}

UAtomicGridPlacementComponent::UAtomicGridPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAtomicGridPlacementComponent::Initialize()
{
	ResolveCachedReferences();
}

void UAtomicGridPlacementComponent::ResolveCachedReferences()
{
	if (!ShipGrid)
	{
		ShipGrid = Cast<AAtomicShipGrid>(GetOwner());
	}
	
	if (!GridData && GetOwner())
	{
		GridData = GetOwner()->GetComponentByClass<UAtomicGridDataComponent>();
	}
	
	if (!BuildingRegistry)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				BuildingRegistry = GameInstance->GetSubsystem<UAtomicBuildingRegistrySubsystem>();
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// VALIDATION
/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UAtomicGridPlacementComponent::CanPlaceBuilding(const UAtomicBuildingDefinition* BuildingDefinition, const FIntVector GridCoord, const EBuildingRotation CurrentRotation) const
{
	if (!GridData || !GetWorld()) return false;
	if (!BuildingDefinition) return false;
	
	// @todo: Add Port Validation
	
	TArray<FIntVector> FootprintCells;
	UAtomicGridLibrary::GetFootprintCellsAroundPivot(GridCoord, BuildingDefinition->FootprintSize, BuildingDefinition->PivotCell, CurrentRotation, FootprintCells);
	
	for (const FIntVector& CellCoord : FootprintCells)
	{
		if (!CheckIfValidCell(CellCoord)) return false;
	}
	
	//UE_LOG(LogTemp, Warning, TEXT("CanPlaceBuilding TRUE"));
	return true;
}

bool UAtomicGridPlacementComponent::CanPlaceBeltCell(const UAtomicBeltDefinition* BeltDefinition, const FIntVector GridCoord, const EBuildingRotation Rotation) const
{
	if (!GridData || !GetWorld()) return false;
	if (!BeltDefinition) return false;
	
	// Cell is available?
	if (!CheckIfValidCell(GridCoord)) return false;
	
	return true;
}


// Check if Cell is available for new placement?
bool UAtomicGridPlacementComponent::CheckIfValidCell(const FIntVector CellCoord) const
{
	const FAtomicGridCell* Cell = GridData->GetCell(CellCoord);
	if (!Cell) return false;
		
	// @todo: 
	// Player is close enough?
	// Resources/cost are valid
		
	if (!UAtomicGridLibrary::IsInsideGrid(CellCoord, GridData->GetGridSize()))
	{
		return false;
	}
		
	// if (!Cell->HasFlag(EGridCellFlag::HasFloor))
	// {
	// 	return false;
	// }
	if (Cell->HasFlag(EGridCellFlag::Blocked))
	{
		return false;
	}
	if (Cell->HasFlag(EGridCellFlag::Reserved))
	{
		return false;
	}
	if (Cell->IsOccupied())
	{
		return false;
	}
	if (IsCellBlockedByPawn(CellCoord, GridData->GetCellSize()))
	{
		return false;
	}
	
	return true;
}

bool UAtomicGridPlacementComponent::AreBeltPortsValid(const EGridDirection InputPort, const EGridDirection OutputPort) const
{
	if (!IsValidCardinalGridDirection(InputPort)) return false;
	if (!IsValidCardinalGridDirection(OutputPort)) return false;
	if (InputPort == OutputPort) return false;
	return true;
}

// This Checks: Current.OutputPort points to Next cell, & Next.InputPort accepts from Current cell
bool UAtomicGridPlacementComponent::IsAdjacentBeltCellConnected(const FAtomicBeltPlacementCell& CurrentCell, const FAtomicBeltPlacementCell& NextCell) const
{
	const FIntVector Delta = NextCell.GridCoord - CurrentCell.GridCoord;
	if (Delta.Z != 0) return false;
	
	// ManhattanDistance = grid-walking distance, not diagonal distance.
	// Checks that belts Share an Edge, no diagonals, no gaps.
	const int32 ManhattanDistance = FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
	if (ManhattanDistance != 1) return false;
	
	const EGridDirection DirectionToNext = UAtomicGridLibrary::DirectionFromCoordToCoord(CurrentCell.GridCoord, NextCell.GridCoord);
	if (CurrentCell.OutputPort != DirectionToNext) return false;
	if (NextCell.InputPort != UAtomicGridLibrary::OppositeGridDirection(DirectionToNext)) return false;
	
	return true;
}

bool UAtomicGridPlacementComponent::HasDuplicateBeltCells(const TArray<FAtomicBeltPlacementCell>& BeltCells) const
{
	if (!GridData) return true;
	TSet<int32> SeenCellIndices;
	for (const FAtomicBeltPlacementCell& BeltCell : BeltCells)
	{
		int32 CellIndex = INDEX_NONE;
		if (!UAtomicGridLibrary::TryGridToIndex(BeltCell.GridCoord, GridData->GetGridSize(), CellIndex)) return true;
		if (SeenCellIndices.Contains(CellIndex)) return true;
		SeenCellIndices.Add(CellIndex);
	}
	return false;
}

bool UAtomicGridPlacementComponent::IsCellBlockedByPawn(const FIntVector& CellCoord, const float CellSize) const
{
	if (!ShipGrid) return false;

	const UWorld* World = GetWorld();
	if (!World) return false;
	
	const FVector CellCenter = UAtomicGridLibrary::GridToWorld(CellCoord, ShipGrid->GetTransform(), CellSize);
	
	// Covers roughly from floor to above character foot/body
	const float HalfHeight = 150.f;
	const FVector QueryCenter = CellCenter + FVector(0.f, 0.f, HalfHeight);
	const FVector QueryExtent(CellSize * 0.45f, CellSize * 0.45f, HalfHeight);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	
	TArray<FOverlapResult> OverlayResults;
	const bool bHitPawn = World->OverlapMultiByObjectType(OverlayResults, QueryCenter, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeBox(QueryExtent), QueryParams);
	if (!bHitPawn) return false;
	
	for (const FOverlapResult& Overlap : OverlayResults)
	{
		const AActor* Actor = Overlap.GetActor();
		if (!Actor) continue;
		
		if (Actor->IsA<APawn>())
		{
			return true;
		}
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// SERVER AUTHORITATIVE PLACEMENT
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// ---------------------------------------------------------------------
// Try Place BUILDING
// Place Building from this Selected Building Definition at this anchor coord with this rotation.
// ---------------------------------------------------------------------
bool UAtomicGridPlacementComponent::TryPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const APawn* RequestingPawn, FGuid& OutBuildingInstanceID) const
{
	if (!ShipGrid || !GridData) return false;
	if (!ShipGrid->HasAuthority()) return false;
	if (!RequestingPawn) return false;

	const UAtomicBuildingDefinition* BuildingDefinition = BuildingRegistry->FindBuildingDefinition(BuildingID);
	if (!BuildingDefinition) return false;

	// Can Place Building?
	if (!CanPlaceBuilding(BuildingDefinition, AnchorCoord, Rotation))
	{
		return false;
	}
	
	int32 AnchorCellIndex = INDEX_NONE;
	if (!UAtomicGridLibrary::TryGridToIndex(AnchorCoord, GridData->GetGridSize(), AnchorCellIndex)) return false;

	int32 OwningPlayerID = INDEX_NONE;
	if (const AAtomicPlayerState* RequestingPlayerState = Cast<AAtomicPlayerState>(RequestingPawn->GetPlayerState()))
	{
		OwningPlayerID = RequestingPlayerState->GetPlayerId();
	}
	
	const FGuid BuildingInstanceID = FGuid::NewGuid();
	
	// Add Placed Building Record
	FAtomicBuildingRecord NewRecord;
	NewRecord.BuildInstanceID = BuildingInstanceID;
	OutBuildingInstanceID = BuildingInstanceID;
	
	NewRecord.BuildingID = BuildingID;
	NewRecord.CellIndex = AnchorCellIndex;
	NewRecord.DeckIndex = static_cast<uint8>(AnchorCoord.Z);
	NewRecord.OwningPlayerID = OwningPlayerID;
	NewRecord.Rotation = Rotation;

	if (!GridData->AddBuildingRecord(NewRecord))
	{
		return false;	
	}

	UE_LOG(LogTemp, Warning, TEXT("TryPlaceBuilding TRUE"));
	return true;
}

bool UAtomicGridPlacementComponent::TryRemoveBuilding()
{
	return false;
}


// ---------------------------------------------------------------------
// Try Place BELT -- run on SERVER
// Place Belt from this Selected Belt Definition at this anchor coord with this rotation.
// ---------------------------------------------------------------------
// bool UAtomicGridPlacementComponent::TryPlaceBelt(const FName DefinitionID, const TArray<FAtomicBeltPlacementCell>& BeltCells, const APawn* RequestingPawn, FGuid& OutBeltInstanceID) const
// {
// 	if (!ShipGrid || !GridData) return false;
// 	if (!ShipGrid->HasAuthority()) return false;
// 	if (!RequestingPawn) return false;
// 	
// 	const UAtomicBeltDefinition* BeltDefinition = BuildingRegistry->FindBeltDefinition(DefinitionID);
// 	if (!BeltDefinition) return false;
// 	
// 	// Can Place?
// 	if (!CanPlaceBelt(BeltDefinition, AnchorCoord, CurrentRotation)) return false;
// 	
// 	int32 AnchorCellIndex = INDEX_NONE;
// 	if (!UAtomicGridLibrary::TryGridToIndex(AnchorCoord, GridData->GetGridSize(), AnchorCellIndex)) return false;
// 	
// 	int32 OwningPlayerID = INDEX_NONE;
// 	if (const AAtomicPlayerState* RequestingPlayerState = Cast<AAtomicPlayerState>(RequestingPawn->GetPlayerState()))
// 	{
// 		OwningPlayerID = RequestingPlayerState->GetPlayerId();
// 	}
// 	
// 	// Add Placed Building Record
// 	FAtomicBeltRecord NewRecord;
// 	NewRecord.InstanceID = FGuid::NewGuid();
// 	OutBeltInstanceID = NewRecord.InstanceID;
// 	
// 	NewRecord.DefinitionID = DefinitionID;
// 	NewRecord.CellIndex = AnchorCellIndex;
// 	NewRecord.DeckIndex = static_cast<uint8>(AnchorCoord.Z);
// 	NewRecord.OwningPlayerID = OwningPlayerID;
// 	//NewRecord.InputPort = InputPort;
// 	//NewRecord.OutputPort = OutputPort;
//
// 	if (!GridData->AddBeltRecord(NewRecord))
// 	{
// 		return false;	
// 	}
// 	
// 	UE_LOG(LogGame, Warning, TEXT("TryPlaceBelt=TRUE | Cell Index: %d, InputPort: %s, OutputPort: %s"), NewRecord.CellIndex, 
// 		*UEnum::GetValueAsString(NewRecord.InputPort), *UEnum::GetValueAsString(NewRecord.OutputPort));
// 	
// 	return true;
// }

bool UAtomicGridPlacementComponent::CanPlaceBeltLine(const UAtomicBeltDefinition* BeltDefinition, const TArray<FAtomicBeltPlacementCell>& BeltCells) const
{
	if (!GridData || !GetWorld()) return false;
	if (!BeltDefinition) return false;
	if (BeltCells.IsEmpty()) return false;
	
	if (HasDuplicateBeltCells(BeltCells)) return false;
	
	for (const FAtomicBeltPlacementCell& BeltCell : BeltCells)
	{
		if (!CheckIfValidCell(BeltCell.GridCoord)) return false;
		if (!AreBeltPortsValid(BeltCell.InputPort, BeltCell.OutputPort)) return false;
	}
	
	for (int32 Index = 0; Index + 1 < BeltCells.Num(); ++Index)
	{
		if (!IsAdjacentBeltCellConnected(BeltCells[Index], BeltCells[Index + 1])) return false;
	}
	
	return true;
}

bool UAtomicGridPlacementComponent::TryPlaceBeltLine(const FName BeltID, const TArray<FAtomicBeltPlacementCell>& BeltCells, const APawn* RequestingPawn, FGuid& OutBeltLineID) const
{
	if (!ShipGrid || !GridData) return false;
	if (!ShipGrid->HasAuthority()) return false;
	if (!RequestingPawn) return false;

	const UAtomicBeltDefinition* BeltDefinition = BuildingRegistry->FindBeltDefinition(BeltID);
	if (!BeltDefinition) return false;

	if (!CanPlaceBeltLine(BeltDefinition, BeltCells)) return false;

	const FGuid BeltLineID = FGuid::NewGuid();
	OutBeltLineID = BeltLineID;
	
	int32 OwningPlayerID = INDEX_NONE;
	if (const AAtomicPlayerState* RequestingPlayerState = Cast<AAtomicPlayerState>(RequestingPawn->GetPlayerState()))
	{
		OwningPlayerID = RequestingPlayerState->GetPlayerId();
	}
	
	TArray<FAtomicBeltRecord> NewRecords;
	for (int32 BeltIndex = 0; BeltIndex < BeltCells.Num(); ++BeltIndex)
	{
		const FAtomicBeltPlacementCell& BeltCell = BeltCells[BeltIndex];
		
		int32 CellIndex = INDEX_NONE;
		if (!UAtomicGridLibrary::TryGridToIndex(BeltCell.GridCoord, GridData->GetGridSize(), CellIndex)) return false;
		
		FAtomicBeltRecord NewRecord;
		NewRecord.LineID = BeltLineID;
		NewRecord.DefinitionID = BeltID;
		NewRecord.InstanceID = FGuid::NewGuid();
		NewRecord.OwningPlayerID = OwningPlayerID;
		
		NewRecord.CellIndex = CellIndex;
		NewRecord.DeckIndex = static_cast<uint8>(BeltCell.GridCoord.Z);
		NewRecord.InputPort = BeltCell.InputPort;
		NewRecord.OutputPort = BeltCell.OutputPort;
		
		NewRecords.Add(NewRecord);
	}
	
	if (!GridData->AddBeltRecords(NewRecords)) return false;

	UE_LOG(LogTemp, Warning, TEXT("TryPlaceBelt TRUE"));
	return true;
}

bool UAtomicGridPlacementComponent::TryRemoveBeltLine()
{
	return false;
}
