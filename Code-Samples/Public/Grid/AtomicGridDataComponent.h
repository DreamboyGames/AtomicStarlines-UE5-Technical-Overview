// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtomicGridTypes.h"
#include "Belts/AtomicBeltRecords.h"
#include "Building/AtomicBuildingRecords.h"
#include "Components/ActorComponent.h"
#include "AtomicGridDataComponent.generated.h"


struct FAtomicBeltRecord;
struct FAtomicBuildingRecord;
class UAtomicGridVisualComponent;
class AAtomicShipGrid;
class UAtomicBuildingRegistrySubsystem;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRID DATA COMPONENT
// This class stores the actual grid state
//
// Authoritative Belt Records + Connection Queries + Occupancy
/////////////////////////////////////////////////////////////////////////////////////////////////////////
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicGridDataComponent : public UActorComponent {
	GENERATED_BODY()
	
	
public:
	UAtomicGridDataComponent();
	
	void InitializeGrid();
	
	// ---------------------------------------------------------------------
	// GRID
	// ---------------------------------------------------------------------
	float GetCellSize() const { return CellSize; }
	FIntVector GetGridSize() const { return GridSize; }

	const FAtomicGridCell* GetCell(const FIntVector& Coord) const;
	const FAtomicGridCell* GetCell(const int32 Index) const;

	bool IsCellBuildable(const int32 Index) const;
	bool MarkCellsOccupied(const TArray<FIntVector>& CellCoords, const FGuid& InstanceID, const EGridOccupancyType OccupancyType);
	bool MarkCellOccupied(const FIntVector CellCoord, const FGuid& InstanceID, const EGridOccupancyType OccupancyType);
	// ---------------------------------------------------------------------

	
	// ---------------------------------------------------------------------
	// BUILD RECORDS
	// ---------------------------------------------------------------------
	const TArray<FAtomicBuildingRecord>& GetPlacedBuildingRecords() const { return BuildingRecords.Items; }
	const TArray<FAtomicBeltRecord>& GetPlacedBeltRecords() const { return BeltRecords.Items; }
	
	bool AddBuildingRecord(const FAtomicBuildingRecord& NewRecord);
	void MarkBuildingRecordChanged(FAtomicBuildingRecord& Record);
	bool RemoveBuildingRecord(const FGuid& BuildInstanceID);
	
	bool AddBeltRecord(const FAtomicBeltRecord& NewRecord);
	void MarkBeltRecordChanged(FAtomicBeltRecord& Record);
	bool RemoveBeltRecord(const FGuid& InstanceID);
	// ---------------------------------------------------------------------
	
	
	// ---------------------------------------------------------------------
	// REPLICATION CALLBACKS, from Fast Array deltas.
	// DO: rebuild local cells occupancy cache & rebuild visuals & collision.
	// ---------------------------------------------------------------------
	void HandleBuildingRecordAdded(const FAtomicBuildingRecord& Record);
	void HandleBuildingRecordChanged(const FAtomicBuildingRecord& Record);
	void HandleBuildingRecordRemoved(const FAtomicBuildingRecord& Record);
	
	void HandleBeltRecordAdded(const FAtomicBeltRecord& Record);
	void HandleBeltRecordChanged(const FAtomicBeltRecord& Record);
	void HandleBeltRecordRemoved(const FAtomicBeltRecord& Record);
	// ---------------------------------------------------------------------

	
	// ---------------------------------------------------------------------
	// Belt Helpers
	// ---------------------------------------------------------------------
	FAtomicBeltRecord* FindBeltRecordAtIndex(const int32 Index);
	const FAtomicBeltRecord* FindBeltRecordAtIndex(const int32 Index) const;
	const FAtomicBeltRecord* GetNeighbourBeltRecord(const int32 Index, const EGridDirection Direction) const;

	void GetRoutePortsForBeltRecord(const FAtomicBeltRecord& BeltRecord, TArray<EGridDirection>& OutRoutePorts) const;
	void GetConnectedRoutePortsForBeltRecord(const FAtomicBeltRecord& BeltRecord, TArray<EGridDirection>& OutConnectedRoutePorts) const;
	void GetConnectedRoutePortsForBeltCandidate(const int32 CellIndex, const EAtomicBeltRouteType RouteType, const EGridDirection InputPort, const EGridDirection OutputPort, TArray<EGridDirection>& OutConnectedRoutePorts) const;

	bool DoesBeltRecordHavePort(const FAtomicBeltRecord& BeltRecord, EGridDirection Port) const;
	bool HasReciprocalBeltConnectionAtPort(const int32 Index, const EGridDirection PortDirection) const;
	bool HasAnyNeighbourBeltConnection(const int32 Index) const;

	// ---------------------------------------------------------------------
	
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AtomicGrid")
	float CellSize;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AtomicGrid")
	FIntVector GridSize;

	//UPROPERTY(VisibleAnywhere, Category="AtomicGrid")
	//TArray<FIntVector> Coords;

	// Building Definitions Database
	// TObjectPtr<> BuildingDatabase;

	// ---------------------------------------------------------------------
	// Occupancy Cache
	// ---------------------------------------------------------------------
	bool ApplyBuildingRecordToOccupancyCache(const FAtomicBuildingRecord& BuildRecord);
	bool ApplyBeltRecordToOccupancyCache(const FAtomicBeltRecord& BeltRecord);
	void RemoveBeltRecordFromOccupancyCache(const FAtomicBeltRecord& BeltRecord);
	void RebuildFullOccupancyCache();
	void ClearOccupancyCache();

	// @todo: for now, compute ports on demand. Later, add cache or a dedicated network component 
	// When place/remove building: PlacedBuildingRecord changes -> Rebuild occupancy cache -> Rebuild port cache
	// TMap<FGuid, TArray<FAtomicResolvedBuildingPort>> PortsByBuilding;
	// TMultiMap<FIntVector, FAtomicResolvedBuildingPort> PortsByCell;

private:
	UPROPERTY()
	TObjectPtr<AAtomicShipGrid> ShipGrid;
	
	UPROPERTY()
	TObjectPtr<UAtomicGridVisualComponent> VisualComponent;

	UPROPERTY()
	TObjectPtr<UAtomicBuildingRegistrySubsystem> BuildingRegistry;
	
	// Authoritative occupancy cache. Fast lookup cache. Derived state, not replicated.
	UPROPERTY(VisibleAnywhere, Category="AtomicGrid")
	TArray<FAtomicGridCell> Cells;

	// Replicated Source Placed Buildings List
	// FastArray tacks items by replication IDs/keys. Replication IDs are synced, but indices ar not guaranteed to be identical between client and server in all cases.
	UPROPERTY(Replicated, VisibleAnywhere, Category="AtomicGrid")
	FAtomicBuildingRecordArray BuildingRecords;
	
	// Replicated Source Placed Belts List
	// FastArray tacks items by replication IDs/keys. Replication IDs are synced, but indices ar not guaranteed to be identical between client and server in all cases.
	UPROPERTY(Replicated, VisibleAnywhere, Category="AtomicGrid")
	FAtomicBeltRecordArray BeltRecords;
	
	
	// PlacedBuildRecords Flow:
	// SERVER AddPlacedBuildRecord()
	// -> Items.Add
	// -> MarkItemDirty
	// -> Server manually applies occupancy + visual spawn
	// -> ForceNetUpdate
	//
	// CLIENT receives delta
	// -> PostReplicatedAdd
	// -> Client applies occupancy + visual spawn for only that one record

	
	bool bGridInitialized = false;

public:
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};

