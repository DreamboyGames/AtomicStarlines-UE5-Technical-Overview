// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicGridPlacementComponent.h"

#include "Building/AtomicBuildingActor.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Core/AtomicPlayerState.h"
#include "Engine/OverlapResult.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicShipGrid.h"


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

bool UAtomicGridPlacementComponent::CanPlaceBelt(const UAtomicBeltDefinition* BeltDefinition, const FIntVector GridCoord, const EAtomicBeltShape BeltShape, const EBuildingRotation CurrentRotation, const EGridDirection OutputGridDirection) const
{
	if (!GridData || !GetWorld()) return false;
	if (!BeltDefinition) return false;
	
	if (!CheckIfValidCell(GridCoord)) return false;
	
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(BeltShape, CurrentRotation);
	if (!RoutePorts.Contains(OutputGridDirection)) return false;
	
	//UE_LOG(LogTemp, Warning, TEXT("CanPlaceBelt TRUE"));
	return true;
}

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
bool UAtomicGridPlacementComponent::TryPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const APawn* RequestingPawn) const
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

// ---------------------------------------------------------------------
// Try Place BELT
// Place Belt from this Selected Belt Definition at this anchor coord with this rotation.
// ---------------------------------------------------------------------
bool UAtomicGridPlacementComponent::TryPlaceBelt(const FName BeltID, const FIntVector AnchorCoord, const EAtomicBeltShape BeltShape, const EBuildingRotation Rotation, const EGridDirection OutputGridDirection, const APawn* RequestingPawn) const
{
	if (!ShipGrid || !GridData) return false;
	if (!ShipGrid->HasAuthority()) return false;
	if (!RequestingPawn) return false;

	const UAtomicBeltDefinition* BeltDefinition = BuildingRegistry->FindBeltDefinition(BeltID);
	if (!BeltDefinition) return false;

	// Can Place Building?
	if (!CanPlaceBelt(BeltDefinition, AnchorCoord, BeltShape, Rotation, OutputGridDirection))
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
	
	const FGuid InstanceID = FGuid::NewGuid();
	
	// Add Placed Building Record
	FAtomicBeltRecord NewRecord;
	NewRecord.InstanceID = InstanceID;
	NewRecord.BeltID = BeltID;
	NewRecord.CellIndex = AnchorCellIndex;
	NewRecord.DeckIndex = static_cast<uint8>(AnchorCoord.Z);
	NewRecord.OwningPlayerID = OwningPlayerID;
	NewRecord.RouteRotation = Rotation;
	NewRecord.OutputFlowDirection = OutputGridDirection;
	NewRecord.Shape = BeltShape;

	if (!GridData->AddBeltRecord(NewRecord))
	{
		return false;	
	}

	UE_LOG(LogTemp, Warning, TEXT("TryPlaceBelt TRUE"));
	return true;
}

bool UAtomicGridPlacementComponent::TryRemoveBuilding()
{
	return false;
}

bool UAtomicGridPlacementComponent::TryRemoveBelt()
{
	return false;
}
