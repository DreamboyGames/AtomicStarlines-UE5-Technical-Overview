// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtomicGridTypes.h"
#include "Components/ActorComponent.h"
#include "AtomicGridPlacementComponent.generated.h"


enum class EAtomicBeltShape : uint8;
class UAtomicBeltDefinition;
class UAtomicBuildingRegistrySubsystem;
class UAtomicBuildingDefinition;
class AAtomicShipGrid;
class UAtomicGridDataComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicGridPlacementComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicGridPlacementComponent();
	void Initialize();

	bool CanPlaceBuilding(const UAtomicBuildingDefinition* BuildingDefinition, const FIntVector GridCoord, const EBuildingRotation CurrentRotation) const;
	bool CanPlaceBelt(const UAtomicBeltDefinition* BeltDefinition, const FIntVector GridCoord, const EAtomicBeltShape BeltShape, const EBuildingRotation CurrentRotation, const EGridDirection OutputGridDirection) const;

	bool TryPlaceBuilding(const FName BuildingID, const FIntVector AnchorCoord, const EBuildingRotation Rotation, const APawn* RequestingPawn) const;
	bool TryPlaceBelt(const FName BeltID, const FIntVector AnchorCoord, const EAtomicBeltShape BeltShape, const EBuildingRotation Rotation, const EGridDirection OutputGridDirection, const APawn* RequestingPawn) const;

	bool TryRemoveBuilding();
	bool TryRemoveBelt();

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
