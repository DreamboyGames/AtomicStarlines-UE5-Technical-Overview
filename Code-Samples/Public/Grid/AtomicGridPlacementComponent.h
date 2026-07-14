// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtomicGridTypes.h"
#include "Components/ActorComponent.h"
#include "AtomicGridPlacementComponent.generated.h"


enum class EAtomicBeltRouteType : uint8;
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
	bool TryPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const APawn* RequestingPawn) const;
	bool TryRemoveBuilding();

	// ---------------------------------------------------------------------
	// Belts
	// ---------------------------------------------------------------------
	bool CanPlaceBelt(const UAtomicBeltDefinition* BeltDefinition, const FIntVector GridCoord, const EAtomicBeltRouteType RouteType, const EGridDirection InputPort, const EGridDirection OutputPort) const;
	bool TryPlaceBelt(const FName DefinitionID, const FIntVector AnchorCoord, const EAtomicBeltRouteType RouteType, const EGridDirection InputPort, const EGridDirection OutputPort, const APawn* RequestingPawn) const;
	bool TryRemoveBelt();

	// ---------------------------------------------------------------------
	// Helpers
	// ---------------------------------------------------------------------
	bool CheckIfValidCell(const FIntVector CellCoord) const;

	
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
