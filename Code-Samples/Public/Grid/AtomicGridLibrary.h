// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtomicGridTypes.h"
#include "Belts/AtomicBeltTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AtomicGridLibrary.generated.h"

struct FAtomicBuildingPortDefinition;


/**
 * Pure helper library for grid math, footprint rotation, cardinal directions,
 * building rotation, building port resolution, and belt route-port logic.
 * 
 * Naming Rules:
 * - Direction = cardinal grid side / neighbour relation
 * - Rotation  = placed object orientation / yaw
 */
UCLASS()
class ATOMICSTARLINES_API UAtomicGridLibrary : public UBlueprintFunctionLibrary {
	GENERATED_BODY()
	
public:
	
	// ---------------------------------------------------------------------
	// Cardinal Math
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Cardinal")
	static int32 WrapCardinalIndex(const int32 Value);
	
	
	// ---------------------------------------------------------------------
	// Grid Indexing
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Indexing")
	static FIntVector IndexToGridUnchecked(const int32 Index, const FIntVector GridSize);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Indexing")
	static int32 GridToIndexUnchecked(const FIntVector Coord, const FIntVector GridSize);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Indexing")
	static bool TryGridToIndex(const FIntVector Coord, const FIntVector GridSize, int32& OutIndex);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Indexing")
	static bool IsInsideGrid(const FIntVector Coord, const FIntVector GridSize);

	
	// ---------------------------------------------------------------------
	// World / Grid Conversion
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Conversion")
	static FIntVector WorldToGrid(const FVector WorldLocation, const FTransform GridTransform, const float CellSize, const int DeckIndex);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Conversion")
	static FVector GridToWorld(const FIntVector Coord, const FTransform GridTransform, const float CellSize);

	
	// ---------------------------------------------------------------------
	// Grid Directions
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static EGridDirection OppositeGridDirection(const EGridDirection GridDirection);

	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static FIntVector GridDirectionToOffset(const EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static FVector GridDirectionToVector(const EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static float GridDirectionToYawDegrees(EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static bool TryGetNeighbourIndex(const int32 GridIndex, const EGridDirection GridDirection, const FIntVector GridSize, int32& OutGridIndex);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static FIntVector GetNeighbourCoord(const FIntVector GridCoord, const EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static EGridDirection RotateGridDirectionByBuildingRotation(const EGridDirection GridDirection, const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static FString GetGridDirectionLabel(const EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Directions")
	static FString GetGridDirectionName(const EGridDirection GridDirection);
	
	
	// ---------------------------------------------------------------------
	// Footprints
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Footprints")
	static void GetFootprintCellsAroundPivot(const FIntVector AnchorCoord, const FIntPoint FootprintSize, const FIntPoint PivotCell, const EBuildingRotation BuildingRotation, TArray<FIntVector>& OutGridCoords);

	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Footprints")
	static FVector GetFootprintCenterWorldLocationFromCells(const TArray<FIntVector>& GridCoords, const FTransform& GridTransform, const float CellSize);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Footprints")
	static FIntPoint RotateOffsetAroundPivot(const FIntPoint RelativeOffset, const EBuildingRotation BuildingRotation);


	// ---------------------------------------------------------------------
	// Building Rotation
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static float BuildingRotationToYawDegrees(const EBuildingRotation BuildingRotation);

	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static FVector BuildingRotationToForwardVector(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static EGridDirection BuildingRotationToGridDirection(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static EBuildingRotation GridDirectionToBuildingRotation(const EGridDirection GridDirection);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static EBuildingRotation RotateBuildingClockwise(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static EBuildingRotation RotateBuildingCounterClockwise(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static FString GetBuildingRotationLabel(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Rotation")
	static FString GetBuildingRotationName(const EBuildingRotation BuildingRotation);


	// ---------------------------------------------------------------------
	// Building Ports
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Ports")
	static void GetResolvedBuildingPorts(const FGuid BuildingInstanceID, const FName BuildingID, const FIntVector AnchorCoord, const FIntPoint PivotCell, const EBuildingRotation BuildingRotation, const TArray<FAtomicBuildingPortDefinition>& PortDefinitions, TArray<FAtomicResolvedBuildingPort>& OutPorts);


	// ---------------------------------------------------------------------
	// Belt Route Ports
	// ---------------------------------------------------------------------
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EGridDirection GetDefaultInputPort(const EAtomicBeltRouteType RouteType);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EGridDirection GetDefaultOutputPort(const EAtomicBeltRouteType RouteType);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EGridDirection GetOutputPortForInput(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static void GetRoutePortsForInput(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort, TArray<EGridDirection>& OutRoutePorts);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static bool TryGetRouteTypeForInputAndOutput(const EGridDirection InputPort, const EGridDirection OutputPort, EAtomicBeltRouteType& OutRouteType);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static bool TryGetInputPortForRouteTypeAndOutput(const EAtomicBeltRouteType RouteType, const EGridDirection OutputPort, EGridDirection& OutInputPort);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static void GetRoutePortsForBuildingRotation(const EAtomicBeltRouteType RouteType, const EBuildingRotation BuildingRotation, TArray<EGridDirection>& OutRoutePorts);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EGridDirection GetInputPortForBuildingRotation(const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EGridDirection GetOutputPortForBuildingRotation(const EAtomicBeltRouteType RouteType, const EBuildingRotation BuildingRotation);
	
	UFUNCTION(BlueprintPure, Category = "AtomicStarlines|GridLibrary|Belts")
	static EBuildingRotation GetBuildingRotationForInputPort(const EGridDirection InputPort);


	
private:
	static int32 BuildingRotationToCardinalIndex(const EBuildingRotation BuildingRotation);
	static EBuildingRotation CardinalIndexToBuildingRotation(const int32 Index);
	
	static int32 GridDirectionToCardinalIndex(const EGridDirection GridDirection);
	static EGridDirection CardinalIndexToGridDirection(const int32 Index);	
};
