// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AtomicGridPlacementComponent.generated.h"


enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;
struct FAtomicBeltPlacementCell;
class UAtomicBeltDefinition;
class UAtomicBuildingRegistrySubsystem;
class UAtomicBuildingDefinition;
class AAtomicShipGrid;
class UAtomicGridDataComponent;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRID PLACEMENT COMPONENT
// Placement Validation Rules
/////////////////////////////////////////////////////////////////////////////////////////////////////////
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicGridPlacementComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicGridPlacementComponent();
	void Initialize();

	// ---------------------------------------------------------------------
	// Buildings
	// ---------------------------------------------------------------------
	bool CanPlaceBuilding(const UAtomicBuildingDefinition* BuildingDefinition, const FIntVector GridCoord, const EBuildingRotation CurrentRotation) const;
	bool TryPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const APawn* RequestingPawn, FGuid& OutBuildingInstanceID) const;
	bool TryRemoveBuilding();

	// ---------------------------------------------------------------------
	// Belts
	// ---------------------------------------------------------------------
	bool CanPlaceBeltCell(const UAtomicBeltDefinition* BeltDefinition, const FIntVector GridCoord, const EBuildingRotation Rotation) const;
	//bool TryPlaceBelt(const FName DefinitionID, const TArray<FAtomicBeltPlacementCell>& BeltCells, const APawn* RequestingPawn, FGuid& OutBeltInstanceID) const;
	//bool TryRemoveBelt();
	bool CanPlaceBeltLine(const UAtomicBeltDefinition* BeltDefinition, const TArray<FAtomicBeltPlacementCell>& BeltCells) const;
	
	bool TryPlaceBeltLine(const FName BeltID, const TArray<FAtomicBeltPlacementCell>& BeltCells, const APawn* RequestingPawn, FGuid& OutBeltLineID) const;
	bool TryRemoveBeltLine();
	
	
	// ---------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------
	bool CheckIfValidCell(const FIntVector CellCoord) const;
	bool AreBeltPortsValid(const EGridDirection InputPort, const EGridDirection OutputPort) const;
	bool IsAdjacentBeltCellConnected(const FAtomicBeltPlacementCell& CurrentCell, const FAtomicBeltPlacementCell& NextCell) const;
	bool HasDuplicateBeltCells(const TArray<FAtomicBeltPlacementCell>& BeltCells) const;
	
protected:

private:
	UPROPERTY(Transient)
	TObjectPtr<AAtomicShipGrid> ShipGrid;

	UPROPERTY(Transient)
	TObjectPtr<UAtomicGridDataComponent> GridData;

	UPROPERTY(Transient)
	TObjectPtr<UAtomicBuildingRegistrySubsystem> BuildingRegistry;

	void ResolveCachedReferences();

	bool IsCellBlockedByPawn(const FIntVector& CellCoord, const float CellSize) const;
};
