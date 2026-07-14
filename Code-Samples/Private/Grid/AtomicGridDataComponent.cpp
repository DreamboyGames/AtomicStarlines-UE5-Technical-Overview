// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicGridDataComponent.h"

#include "AtomicStarlines.h"
#include "Belts/AtomicBeltRecords.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridVisualComponent.h"
#include "Grid/AtomicShipGrid.h"
#include "Net/UnrealNetwork.h"


UAtomicGridDataComponent::UAtomicGridDataComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	
	// @todo: UAtomicShipGridDefinition Data Asset to set ship grid defaults per ship 
	GridSize = FIntVector(20, 20, 1);
	CellSize = 200.0f;
	
	BuildingRegistry = nullptr;
}

void UAtomicGridDataComponent::InitializeGrid()
{
	if (bGridInitialized) return;
	
	const int32 NumCells = GridSize.X * GridSize.Y * GridSize.Z;
	Cells.SetNum(NumCells);
	
	ShipGrid = Cast<AAtomicShipGrid>(GetOwner());
	VisualComponent = ShipGrid->GetGridVisualComponent();

	if (const UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		BuildingRegistry = GameInstance->GetSubsystem<UAtomicBuildingRegistrySubsystem>();
		ensure(BuildingRegistry);
	}
	
	BuildingRecords.OwningObject = this;
	BeltRecords.OwningObject = this;
	bGridInitialized = true;
}

const FAtomicGridCell* UAtomicGridDataComponent::GetCell(const FIntVector& Coord) const
{
	int32 Index = INDEX_NONE;
	
	if (!UAtomicGridLibrary::TryGridToIndex(Coord, GridSize, Index))
	{
		return nullptr;
	}
	
	return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
}

const FAtomicGridCell* UAtomicGridDataComponent::GetCell(const int32 Index) const
{
	return Cells.IsValidIndex(Index) ? &Cells[Index] : nullptr;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// OCCUPANCY CACHE
/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UAtomicGridDataComponent::ApplyBuildingRecordToOccupancyCache(const FAtomicBuildingRecord& BuildRecord)
{
	if (!BuildingRegistry) return false;
	if (BuildRecord.CellIndex == INDEX_NONE) return false;
	
	const UAtomicBuildingDefinition* BuildingDefinition = BuildingRegistry->FindBuildingDefinition(BuildRecord.BuildingID);
	if (!BuildingDefinition) return false;

	const FIntVector OriginCoord = UAtomicGridLibrary::IndexToGridUnchecked(BuildRecord.CellIndex, GridSize);
		
	TArray<FIntVector> FootprintCells;
	UAtomicGridLibrary::GetFootprintCellsAroundPivot(OriginCoord, BuildingDefinition->FootprintSize, BuildingDefinition->PivotCell, BuildRecord.Rotation, FootprintCells);
		
	return MarkCellsOccupied(FootprintCells, BuildRecord.BuildInstanceID, EGridOccupancyType::Building);
}

bool UAtomicGridDataComponent::ApplyBeltRecordToOccupancyCache(const FAtomicBeltRecord& BeltRecord)
{
	if (BeltRecord.CellIndex == INDEX_NONE) return false;
	const FIntVector CellCoord = UAtomicGridLibrary::IndexToGridUnchecked(BeltRecord.CellIndex, GridSize);
	
	return MarkCellOccupied(CellCoord, BeltRecord.InstanceID, EGridOccupancyType::Belt);
}

void UAtomicGridDataComponent::RemoveBeltRecordFromOccupancyCache(const FAtomicBeltRecord& BeltRecord)
{
	if (BeltRecord.CellIndex == INDEX_NONE) return;
	Cells[BeltRecord.CellIndex].OccupancyType = EGridOccupancyType::None;
	Cells[BeltRecord.CellIndex].RemoveFlag(EGridCellFlag::Blocked);
	Cells[BeltRecord.CellIndex].OccupyingInstanceID.Invalidate();
}

void UAtomicGridDataComponent::RebuildFullOccupancyCache()
{
	if (!BuildingRegistry) return;
	
	ClearOccupancyCache();
	
	for (const FAtomicBuildingRecord& BuildRecord : BuildingRecords.Items)
	{
		if (BuildRecord.CellIndex == INDEX_NONE) continue;
		
		ApplyBuildingRecordToOccupancyCache(BuildRecord);
	}
	
	for (const FAtomicBeltRecord& BeltRecord : BeltRecords.Items)
	{
		if (BeltRecord.CellIndex == INDEX_NONE) continue;
		
		ApplyBeltRecordToOccupancyCache(BeltRecord);
	}
}

void UAtomicGridDataComponent::ClearOccupancyCache()
{
	for (FAtomicGridCell& Cell : Cells)
	{
		Cell.OccupancyType = EGridOccupancyType::None;
		Cell.RemoveFlag(EGridCellFlag::Blocked);
		Cell.OccupyingInstanceID.Invalidate();
	}
}


bool UAtomicGridDataComponent::MarkCellsOccupied(const TArray<FIntVector>& CellCoords, const FGuid& InstanceID, const EGridOccupancyType OccupancyType)
{
	for (const FIntVector CellCoord : CellCoords)
	{
		int32 Index;
		const bool bValidIndex = UAtomicGridLibrary::TryGridToIndex(CellCoord, GridSize, Index);
		if (!bValidIndex) return false;
		
		Cells[Index].OccupancyType = OccupancyType;
		Cells[Index].AddFlag(EGridCellFlag::Blocked);
		Cells[Index].OccupyingInstanceID = InstanceID;
	}
	return true;
}

bool UAtomicGridDataComponent::MarkCellOccupied(const FIntVector CellCoord, const FGuid& InstanceID, const EGridOccupancyType OccupancyType)
{
	int32 Index;
	const bool bValidIndex = UAtomicGridLibrary::TryGridToIndex(CellCoord, GridSize, Index);
	if (!bValidIndex) return false;
		
	Cells[Index].OccupancyType = OccupancyType;
	Cells[Index].AddFlag(EGridCellFlag::Blocked);
	Cells[Index].OccupyingInstanceID = InstanceID;
	
	return true;
}

bool UAtomicGridDataComponent::IsCellBuildable(const int32 Index) const
{
	if (Cells[Index].OccupancyType == EGridOccupancyType::None && Cells[Index].HasFlag(EGridCellFlag::None))
	{
		return true;
	}
	return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BUILDING RECORDS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UAtomicGridDataComponent::AddBuildingRecord(const FAtomicBuildingRecord& NewRecord)
{
	if (!ShipGrid || !ShipGrid->HasAuthority()) return false;
	
	FAtomicBuildingRecord& AddedRecord = BuildingRecords.Items.Add_GetRef(NewRecord);
	BuildingRecords.MarkItemDirty(AddedRecord);
	
	// Server updates local derived state immediately.
	// The Server does not wait for replication callbacks. Must update state directly.
	HandleBuildingRecordAdded(AddedRecord);
	ShipGrid->ForceNetUpdate();
	
	return true;
}

void UAtomicGridDataComponent::MarkBuildingRecordChanged(FAtomicBuildingRecord& Record)
{
	BuildingRecords.MarkItemDirty(Record);
}

bool UAtomicGridDataComponent::RemoveBuildingRecord(const FGuid& BuildInstanceID)
{
	const int32 Index = BuildingRecords.Items.IndexOfByPredicate(
		[&](const FAtomicBuildingRecord& Record)
		{
			return Record.BuildInstanceID == BuildInstanceID;
		}
	);
	
	if (Index == INDEX_NONE) return false;
	
	const FAtomicBuildingRecord RemovedRecord = BuildingRecords.Items[Index];
	HandleBuildingRecordRemoved(RemovedRecord);
	
	BuildingRecords.Items.RemoveAt(Index);
	BuildingRecords.MarkArrayDirty();
	
	ShipGrid->ForceNetUpdate();
	
	return true;
}


void UAtomicGridDataComponent::HandleBuildingRecordAdded(const FAtomicBuildingRecord& Record)
{
	ApplyBuildingRecordToOccupancyCache(Record);
	
	if (ShipGrid && VisualComponent)
	{
		VisualComponent->SpawnOrUpdateBuildingFromRecord(Record);
	}
}

void UAtomicGridDataComponent::HandleBuildingRecordChanged(const FAtomicBuildingRecord& Record)
{
	// If placement records are mostly immutable, this may rarely run.
	// Use later for rotation/move/build-owner corrections.
	
	// Changed cells may affect olf cells and new cells, so full Occupancy Cache rebuild for now.
	
	RebuildFullOccupancyCache();
	
	if (ShipGrid && VisualComponent)
	{
		VisualComponent->SpawnOrUpdateBuildingFromRecord(Record);
	}
}

void UAtomicGridDataComponent::HandleBuildingRecordRemoved(const FAtomicBuildingRecord& Record)
{
	// Remove is awkward for occupancy because multiple cells were marked.
	// Clearing only the removed building's footprint is slightly more error-prone if buildings can overlap in future systems, reserve cells, walls, belts, etc.
	// Rebuilding full Occupancy Cache is the simpler solution for the moment.
	
	RebuildFullOccupancyCache();
	
	if (ShipGrid && VisualComponent)
	{
		VisualComponent->DestroyLocalBuilding(Record.BuildInstanceID);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELT RECORDS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
bool UAtomicGridDataComponent::AddBeltRecord(const FAtomicBeltRecord& NewRecord)
{
	if (!ShipGrid || !ShipGrid->HasAuthority()) return false;
	
	FAtomicBeltRecord& AddedRecord = BeltRecords.Items.Add_GetRef(NewRecord);
	BeltRecords.MarkItemDirty(AddedRecord);
	
	// Server updates local derived state immediately.
	// The Server does not wait for replication callbacks. Must update state directly.
	HandleBeltRecordAdded(AddedRecord);
	ShipGrid->ForceNetUpdate();
	
	return true;
}

void UAtomicGridDataComponent::MarkBeltRecordChanged(FAtomicBeltRecord& Record)
{
	BeltRecords.MarkItemDirty(Record);
}

bool UAtomicGridDataComponent::RemoveBeltRecord(const FGuid& InstanceID)
{
	const int32 Index = BeltRecords.Items.IndexOfByPredicate(
		[&](const FAtomicBeltRecord& Record)
		{
			return Record.InstanceID == InstanceID;
		}
	);
	
	if (Index == INDEX_NONE) return false;
	
	const FAtomicBeltRecord RemovedRecord = BeltRecords.Items[Index];

	BeltRecords.Items.RemoveAt(Index);
	BeltRecords.MarkArrayDirty();

	HandleBeltRecordRemoved(RemovedRecord);
	if (ShipGrid) ShipGrid->ForceNetUpdate();
	
	return true;
}

// Replication Callback
void UAtomicGridDataComponent::HandleBeltRecordAdded(const FAtomicBeltRecord& Record)
{
	// ApplyBeltRecordToOccupancyCache(Record);
	//
	// if (ShipGrid && VisualComponent)
	// {
	// 	VisualComponent->SpawnOrUpdateBeltFromRecord(Record);
	// }
	
	ApplyBeltRecordToOccupancyCache(Record);
	
	if (ShipGrid && VisualComponent)
	{
		VisualComponent->NotifyVisualsToRebuildBelts();
	}
}

// Replication Callback
void UAtomicGridDataComponent::HandleBeltRecordChanged(const FAtomicBeltRecord& Record)
{
	// Safe first version, Changed belt may affect old cell, and neighbours.
	RebuildFullOccupancyCache();
	
	// @todo: make more efficient by not rebuilding full cache
	//ApplyBeltRecordToOccupancyCache(Record);
	
	if (ShipGrid && VisualComponent)
	{
		VisualComponent->NotifyVisualsToRebuildBelts();
	}
}

// Replication Callback
void UAtomicGridDataComponent::HandleBeltRecordRemoved(const FAtomicBeltRecord& Record)
{
	// RemoveBeltRecordFromOccupancyCache(Record);
	//
	// if (ShipGrid && VisualComponent)
	// {
	// 	VisualComponent->DestroyLocalBelt(Record.InstanceID);
	// }
	
	// Record should already be gone from BeltRecords.Items on server remove.
	// On replicated remove, full rebuild is still the sagest simple path.
	// @todo: make more efficient by not rebuilding full cache
	RebuildFullOccupancyCache();

	if (ShipGrid && VisualComponent)
	{
		VisualComponent->NotifyVisualsToRebuildBelts();
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Belt Helpers
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// Get modifiable belt record at index
FAtomicBeltRecord* UAtomicGridDataComponent::FindBeltRecordAtIndex(const int32 Index)
{
	for (FAtomicBeltRecord& BeltRecord : BeltRecords.Items)
	{
		if (BeltRecord.CellIndex == Index)
		{
			return &BeltRecord;
		}	
	}
	return nullptr;
}

// Read-Only query to belt record at index
const FAtomicBeltRecord* UAtomicGridDataComponent::FindBeltRecordAtIndex(const int32 Index) const
{
	for (const FAtomicBeltRecord& BeltRecord : BeltRecords.Items)
	{
		if (BeltRecord.CellIndex == Index)
		{
			return &BeltRecord;
		}	
	}
	return nullptr;
}

const FAtomicBeltRecord* UAtomicGridDataComponent::GetNeighbourBeltRecord(const int32 Index, const EGridDirection Direction) const
{
	int32 NeighbourIndex = INDEX_NONE;
	if (!UAtomicGridLibrary::TryGetNeighbourIndex(Index, Direction, GridSize, NeighbourIndex)) return nullptr;
	return FindBeltRecordAtIndex(NeighbourIndex);
}

void UAtomicGridDataComponent::GetRoutePortsForBeltRecord(const FAtomicBeltRecord& BeltRecord, TArray<EGridDirection>& OutRoutePorts) const
{
	UAtomicGridLibrary::GetRoutePortsForInput(BeltRecord.RouteType, BeltRecord.InputPort, OutRoutePorts);
}

void UAtomicGridDataComponent::GetConnectedRoutePortsForBeltRecord(const FAtomicBeltRecord& BeltRecord, TArray<EGridDirection>& OutConnectedRoutePorts) const
{
	GetConnectedRoutePortsForBeltCandidate(BeltRecord.CellIndex, BeltRecord.RouteType, BeltRecord.InputPort, BeltRecord.OutputPort, OutConnectedRoutePorts);
}

// Get any valid (reciprocal) belt connections for this belt candidate's RoutePorts
void UAtomicGridDataComponent::GetConnectedRoutePortsForBeltCandidate(const int32 CellIndex, const EAtomicBeltRouteType RouteType, const EGridDirection InputPort, const EGridDirection OutputPort, TArray<EGridDirection>& OutConnectedRoutePorts) const
{
	OutConnectedRoutePorts.Reset();
	
	if (!Cells.IsValidIndex(CellIndex)) return;
	
	const EGridDirection ExpectedOutputPort = UAtomicGridLibrary::GetOutputPortForInput(RouteType, InputPort);
	if (ExpectedOutputPort != OutputPort) return;
	
	TArray<EGridDirection> RoutePorts;
	UAtomicGridLibrary::GetRoutePortsForInput(RouteType, InputPort, RoutePorts);
	
	for (const EGridDirection RoutePort : RoutePorts)
	{
		if (HasReciprocalBeltConnectionAtPort(CellIndex, RoutePort))
		{
			OutConnectedRoutePorts.Add(RoutePort);
		}
	}
}

bool UAtomicGridDataComponent::DoesBeltRecordHavePort(const FAtomicBeltRecord& BeltRecord, EGridDirection Port) const
{
	TArray<EGridDirection> RoutePorts;
	GetRoutePortsForBeltRecord(BeltRecord, RoutePorts);
	return RoutePorts.Contains(Port);
}

// Does belt at this index have a valid (reciprocal) connection to neighbour belt in port direction?
// (e.g, if this index A has route port East, is there a belt at index B on the East side & does it have a route port West?)
bool UAtomicGridDataComponent::HasReciprocalBeltConnectionAtPort(const int32 Index, const EGridDirection PortDirection) const
{
	if (!Cells.IsValidIndex(Index)) return false;
	
	const FAtomicBeltRecord* NeighbourBelt = GetNeighbourBeltRecord(Index, PortDirection);
	if (!NeighbourBelt) return false;
	
	const EGridDirection NeighbourPortBackToThisBelt = UAtomicGridLibrary::OppositeGridDirection(PortDirection);
	return DoesBeltRecordHavePort(*NeighbourBelt, NeighbourPortBackToThisBelt);
}

// Check All Directions for valid (reciprocal) belt connections
// Does Not care about flow direction
bool UAtomicGridDataComponent::HasAnyNeighbourBeltConnection(const int32 Index) const
{
	int32 NeighbourCount = 0;
	for (const EGridDirection Direction : AllDirections)
	{
		if (HasReciprocalBeltConnectionAtPort(Index, Direction))
		{
			++NeighbourCount;
		}
	}
	return NeighbourCount != 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// REPLICATED VARIABLES & REP-NOTIFIES
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void UAtomicGridDataComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UAtomicGridDataComponent, BuildingRecords);
	DOREPLIFETIME(UAtomicGridDataComponent, BeltRecords);
}
