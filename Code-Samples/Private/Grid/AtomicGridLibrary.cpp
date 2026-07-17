// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicGridLibrary.h"
#include "ProjectTypes/AtomicBeltTypes.h"
#include "ProjectTypes/AtomicBuildingTypes.h"


// FVector    -- float X, Y, Z
// FIntVector -- int32 X, Y, Z
// FIntPoint  -- int32 X, Y
// FVector WorldLocation = FVector(487.2f, 920.6f, 0.f); -- real world position
// FIntVector GridCoord = FIntVector(4, 9, 0);           -- grid cell coordinate


// Example 2D grid formula:
// Index = X[3] + Y[2] * Width[5] = [13]
// Width = 5
// Y=0:  0   1   2   3   4
// Y=1:  5   6   7   8   9
// Y=2: 10  11  12  13  14
// Y=3: 15  16  17  18  19
// Because each row contains "Width" number of cells, (Y * Width) = skip over all previous rows
// Y=0 -> skip 0
// Y=1 -> skip 1 row = Width cells
// Y=2 -> skip 2 rows = Width * 2 cells
// Then +X moves across that row


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cardinal Math
/////////////////////////////////////////////////////////////////////////////////////////////////////////

int32 UAtomicGridLibrary::WrapCardinalIndex(const int32 Value)
{
	return (Value % 4 + 4) % 4;
}

// NOTE: Do NOT Use for Belts, use GetBeltRotatedRoutePorts() instead.
int32 UAtomicGridLibrary::BuildingRotationToCardinalIndex(const EBuildingRotation BuildingRotation)
{
	return WrapCardinalIndex(static_cast<int32>(BuildingRotation));
}

EBuildingRotation UAtomicGridLibrary::CardinalIndexToBuildingRotation(const int32 Index)
{
	switch (WrapCardinalIndex(Index))
	{
	case 0:
		return EBuildingRotation::North;
		
	case 1:
		return EBuildingRotation::East;
		
	case 2:
		return EBuildingRotation::South;
		
	case 3:
		return EBuildingRotation::West;
		
	default:
		return EBuildingRotation::East;
	}
}

int32 UAtomicGridLibrary::GridDirectionToCardinalIndex(const EGridDirection GridDirection)
{
	return WrapCardinalIndex(static_cast<int32>(GridDirection));
}

EGridDirection UAtomicGridLibrary::CardinalIndexToGridDirection(const int32 Index)
{
	switch (WrapCardinalIndex(Index))
	{
	case 0:
		return EGridDirection::North;
		
	case 1:
		return EGridDirection::East;
		
	case 2:
		return EGridDirection::South;
		
	case 3:
		return EGridDirection::West;
		
	default:
		return EGridDirection::East;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Grid Indexing
/////////////////////////////////////////////////////////////////////////////////////////////////////////

FIntVector UAtomicGridLibrary::IndexToGridUnchecked(const int32 Index, const FIntVector GridSize)
{
	const int32 CellsPerDeck = GridSize.X * GridSize.Y;
	const int32 Z = Index / CellsPerDeck;
	const int32 Remainder = Index % CellsPerDeck;
	
	const int32 Y = Remainder / GridSize.X;
	const int32 X = Remainder % GridSize.X;
	
	return FIntVector(X, Y, Z);
}

// Returns one unique array index for one grid coordinate
// Flattens a 3D grid coord into a 1D array, and returns the cell slot in the flat array
int32 UAtomicGridLibrary::GridToIndexUnchecked(const FIntVector Coord, const FIntVector GridSize)
{
	// X = column offset
	// Y * Width = row offset
	// Z * Width * height = deck/layer offset
	return Coord.X
		+ Coord.Y * GridSize.X 
		+ Coord.Z * GridSize.X * GridSize.Y;
}

// Safe index return if outside of grid bounds
// Returns one unique array index for one grid coordinate
// Flattens a 3D grid coord into a 1D array, and returns the cell slot in the flat array
bool UAtomicGridLibrary::TryGridToIndex(const FIntVector Coord, const FIntVector GridSize, int32& OutIndex)
{
	if (!IsInsideGrid(Coord, GridSize))
	{
		OutIndex = INDEX_NONE;
		return false;
	}
	
	OutIndex = GridToIndexUnchecked(Coord, GridSize);
	return true;
}

// Check Index is within grid bounds
bool UAtomicGridLibrary::IsInsideGrid(const FIntVector Coord, const FIntVector GridSize)
{
	return Coord.X >= 0 
		&& Coord.Y >= 0 
		&& Coord.Z >= 0
		&& Coord.X < GridSize.X
		&& Coord.Y < GridSize.Y
		&& Coord.Z < GridSize.Z;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// World / Grid Conversion
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// What grid cell is this world position inside?
FIntVector UAtomicGridLibrary::WorldToGrid(const FVector WorldLocation, const FTransform GridTransform, const float CellSize, const int DeckIndex)
{
	// Convert from world space into the grid actor's local space
	// Grid Cell (0,0,0) is the first cell relative to the grid actor, not necessarily the world origin
	const FVector LocalLocation = GridTransform.InverseTransformPosition(WorldLocation);
	
	return FIntVector(
		FMath::FloorToInt(LocalLocation.X / CellSize),
		FMath::FloorToInt(LocalLocation.Y / CellSize),
		DeckIndex
	);
}

// Where is the center of this grid cell in the world?
FVector UAtomicGridLibrary::GridToWorld(const FIntVector Coord, const FTransform GridTransform, const float CellSize)
{
	const FVector LocalCenterLocation(
		(Coord.X + 0.5f) * CellSize,
		(Coord.Y + 0.5f) * CellSize,
		Coord.Z
	);
	
	// Convert local grid position back into world space
	return GridTransform.TransformPosition(LocalCenterLocation);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Grid Directions
/////////////////////////////////////////////////////////////////////////////////////////////////////////

EGridDirection UAtomicGridLibrary::OppositeGridDirection(const EGridDirection GridDirection)
{
	return CardinalIndexToGridDirection(GridDirectionToCardinalIndex(GridDirection) + 2);
}

FIntVector UAtomicGridLibrary::GridDirectionToOffset(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:
		return FIntVector(0, -1, 0);
		
	case EGridDirection::East:
		return FIntVector(1, 0, 0);
		
	case EGridDirection::South:
		return FIntVector(0, 1, 0);
		
	case EGridDirection::West:
		return FIntVector(-1, 0, 0);
		
	default:
		return FIntVector::ZeroValue;
	}
}

FVector UAtomicGridLibrary::GridDirectionToVector(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:
		return FVector(0.f, -1.f, 0.f);

	case EGridDirection::East:
		return FVector(1.f, 0.f, 0.f);

	case EGridDirection::South:
		return FVector(0.f, 1.f, 0.f);

	case EGridDirection::West:
		return FVector(-1.f, 0.f, 0.f);

	default:
		return FVector::ZeroVector;
	}
}

float UAtomicGridLibrary::GridDirectionToYawDegrees(EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:
		return 270.f;

	case EGridDirection::East:
		return 0.f;

	case EGridDirection::South:
		return 90.f;

	case EGridDirection::West:
		return 180.f;

	default:
		return 0.f;
	}
}

bool UAtomicGridLibrary::TryGetNeighbourIndex(const int32 GridIndex, const EGridDirection GridDirection, const FIntVector GridSize, int32& OutGridIndex)
{
	OutGridIndex = INDEX_NONE;
	
	if (GridIndex == INDEX_NONE) return false;
	
	const FIntVector Coord = IndexToGridUnchecked(GridIndex, GridSize);
	const FIntVector NeighbourCoord = GetNeighbourCoord(Coord, GridDirection);
	
	return TryGridToIndex(NeighbourCoord, GridSize, OutGridIndex);
}

FIntVector UAtomicGridLibrary::GetNeighbourCoord(const FIntVector GridCoord, const EGridDirection GridDirection)
{
	return GridCoord + GridDirectionToOffset(GridDirection);
}

EGridDirection UAtomicGridLibrary::RotateGridDirectionByBuildingRotation(const EGridDirection GridDirection, const EBuildingRotation BuildingRotation)
{
	// East is the authored/default object orientation.
	// Example:
	// - Local port East on an East-facing object stays East.
	// - Local port East on a South-facing object becomes South.
	const int32 AuthoredDefaultIndex = BuildingRotationToCardinalIndex(EBuildingRotation::East);
	const int32 RotationDelta = BuildingRotationToCardinalIndex(BuildingRotation) - AuthoredDefaultIndex;
	
	return CardinalIndexToGridDirection(GridDirectionToCardinalIndex(GridDirection) + RotationDelta);
}

FString UAtomicGridLibrary::GetGridDirectionLabel(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:	return TEXT("N");
	case EGridDirection::East:	return TEXT("E");
	case EGridDirection::South:	return TEXT("S");
	case EGridDirection::West:	return TEXT("W");
	default:					return TEXT("?");
	}
}

FString UAtomicGridLibrary::GetGridDirectionName(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:	return TEXT("North");
	case EGridDirection::East:	return TEXT("East");
	case EGridDirection::South:	return TEXT("South");
	case EGridDirection::West:	return TEXT("West");
	default:					return TEXT("Unknown");
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Footprints
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void UAtomicGridLibrary::GetFootprintCellsAroundPivot(const FIntVector AnchorCoord, const FIntPoint FootprintSize, const FIntPoint PivotCell, const EBuildingRotation BuildingRotation, TArray<FIntVector>& OutGridCoords)
{
	// Example: 1x2 building with pivot at the first cell
	// 
	// North: +Y
	// [X]
	// [P]
	//
	// East: +X
	// [P][X]
	//
	// South: -Y
	// [P]
	// [X]
	//
	// West: -X
	// [X][P]
	
	OutGridCoords.Reset();
	OutGridCoords.Reserve(FootprintSize.X * FootprintSize.Y);
		
	for (int32 Y = 0; Y < FootprintSize.Y; Y++)
	{
		for (int32 X = 0; X < FootprintSize.X; X++)
		{
			const FIntPoint LocalOffset(X, Y);
			const FIntPoint RelativeOffset = LocalOffset - PivotCell;
			const FIntPoint RotatedOffset = RotateOffsetAroundPivot(RelativeOffset, BuildingRotation);
						
			OutGridCoords.Add(FIntVector(AnchorCoord.X + RotatedOffset.X, AnchorCoord.Y + RotatedOffset.Y, AnchorCoord.Z));
		}
	}
}

FVector UAtomicGridLibrary::GetFootprintCenterWorldLocationFromCells(const TArray<FIntVector>& GridCoords, const FTransform& GridTransform, const float CellSize)
{
	if (GridCoords.IsEmpty())
	{
		return GridTransform.GetLocation();
	}
	
	int32 MinX = GridCoords[0].X;
	int32 MaxX = GridCoords[0].X;
	int32 MinY = GridCoords[0].Y;
	int32 MaxY = GridCoords[0].Y;
	const int32 Z = GridCoords[0].Z;
	
	for (const FIntVector& Cell : GridCoords)
	{
		MinX = FMath::Min(MinX, Cell.X);
		MaxX = FMath::Max(MaxX, Cell.X);
		MinY = FMath::Min(MinY, Cell.Y);
		MaxY = FMath::Max(MaxY, Cell.Y);
	}
	
	const float CenterX = static_cast<float>(MinX + MaxX + 1) * 0.5f;
	const float CenterY = static_cast<float>(MinY + MaxY + 1) * 0.5f;	
	const FVector LocalCenter(CenterX * CellSize, CenterY * CellSize, Z);
	
	return GridTransform.TransformPosition(LocalCenter);
	
	// 1x1 at X=3
	// MinX=3, MaxX=3
	// CenterX = (3+3+1) * 0.5 = 3.5
	// → center of cell
	//
	// 2x1 at X=3 and X=4
	// MinX=3, MaxX=4
	// CenterX = (3+4+1) * 0.5 = 4.0
	// → edge between the two cells
}

FIntPoint UAtomicGridLibrary::RotateOffsetAroundPivot(const FIntPoint RelativeOffset, const EBuildingRotation BuildingRotation)
{
	const int32 X = RelativeOffset.X;
	const int32 Y = RelativeOffset.Y;

	switch (BuildingRotation)
	{
	case EBuildingRotation::East: 
		// Default orientation: +X
		return FIntPoint(X, Y);
		
	case EBuildingRotation::South: 
		// Rotate +90 degrees
		return FIntPoint(-Y, X);
		
	case EBuildingRotation::West:
		// Rotate 180 degrees
		return FIntPoint(-X, -Y);
		
	case EBuildingRotation::North:
		// Rotate -90 degrees
		return FIntPoint(Y, -X);
		
	default:
		return RelativeOffset;
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Building Rotation
/////////////////////////////////////////////////////////////////////////////////////////////////////////

float UAtomicGridLibrary::BuildingRotationToYawDegrees(const EBuildingRotation BuildingRotation)
{
	switch (BuildingRotation)
	{
	case EBuildingRotation::North: return 270.f; // -Y
	case EBuildingRotation::East: return 0.f;    // +X
	case EBuildingRotation::South: return 90.f;  // +Y
	case EBuildingRotation::West: return 180.f;  // -X
	default: return 0.f;
	}
}

FVector UAtomicGridLibrary::BuildingRotationToForwardVector(const EBuildingRotation BuildingRotation)
{
	return GridDirectionToVector(BuildingRotationToGridDirection(BuildingRotation));
}

EGridDirection UAtomicGridLibrary::BuildingRotationToGridDirection(const EBuildingRotation BuildingRotation)
{
	switch (BuildingRotation)
	{
	case EBuildingRotation::North:
		return EGridDirection::North;

	case EBuildingRotation::East:
		return EGridDirection::East;

	case EBuildingRotation::South:
		return EGridDirection::South;

	case EBuildingRotation::West:
		return EGridDirection::West;

	default:
		return EGridDirection::East;
	}
}

EBuildingRotation UAtomicGridLibrary::GridDirectionToBuildingRotation(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:
		return EBuildingRotation::North;

	case EGridDirection::East:
		return EBuildingRotation::East;

	case EGridDirection::South:
		return EBuildingRotation::South;

	case EGridDirection::West:
		return EBuildingRotation::West;

	default:
		return EBuildingRotation::East;
	}
}

EBuildingRotation UAtomicGridLibrary::RotateBuildingClockwise(const EBuildingRotation BuildingRotation)
{
	const uint8 Value = static_cast<uint8>(BuildingRotation);
	const uint8 NewValue = (Value + 1) % 4;
	return static_cast<EBuildingRotation>(NewValue);
	
	// OR
	// return CardinalIndexToBuildingRotation(BuildingRotationToCardinalIndex(BuildingRotation) + 1);
}

EBuildingRotation UAtomicGridLibrary::RotateBuildingCounterClockwise(const EBuildingRotation BuildingRotation)
{
	const uint8 Value = static_cast<uint8>(BuildingRotation);
	const uint8 NewValue = (Value + 3) % 4;
	return static_cast<EBuildingRotation>(NewValue);
	
	// OR
	// return CardinalIndexToBuildingRotation(BuildingRotationToCardinalIndex(BuildingRotation) - 1);
}

FString UAtomicGridLibrary::GetBuildingRotationLabel(const EBuildingRotation BuildingRotation)
{
	switch (BuildingRotation)
	{
	case EBuildingRotation::North: return TEXT("N");
	case EBuildingRotation::East:  return TEXT("E");
	case EBuildingRotation::South: return TEXT("S");
	case EBuildingRotation::West:  return TEXT("W");
	default:                       return TEXT("?");
	}
}

FString UAtomicGridLibrary::GetBuildingRotationName(const EBuildingRotation BuildingRotation)
{
	switch (BuildingRotation)
	{
	case EBuildingRotation::North: return TEXT("North");
	case EBuildingRotation::East:  return TEXT("East");
	case EBuildingRotation::South: return TEXT("South");
	case EBuildingRotation::West:  return TEXT("West");
	default:                       return TEXT("Unknown");
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Building Ports
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void UAtomicGridLibrary::GetResolvedBuildingPorts(const FGuid BuildingInstanceID, const FName BuildingID,
    const FIntVector AnchorCoord, const FIntPoint PivotCell, const EBuildingRotation BuildingRotation,
    const TArray<FAtomicBuildingPortDefinition>& PortDefinitions, TArray<FAtomicResolvedBuildingPort>& OutPorts)
{
	OutPorts.Reset();
	OutPorts.Reserve(PortDefinitions.Num());
	
	for (const FAtomicBuildingPortDefinition& PortDefinition : PortDefinitions)
	{
		// Port local offset rotates around the same PivotCell as the footprint
		// Port direction rotates by the same building rotation
		const FIntPoint RelativeOffset = PortDefinition.LocalCellOffset - PivotCell;
		const FIntPoint RotatedOffset = RotateOffsetAroundPivot(RelativeOffset, BuildingRotation);
		const FIntVector PortCellCoord(AnchorCoord.X + RotatedOffset.X, AnchorCoord.Y + RotatedOffset.Y, AnchorCoord.Z);
		const EGridDirection PortGridDirection = RotateGridDirectionByBuildingRotation(PortDefinition.LocalGridDirection, BuildingRotation);
		const FIntVector AdjacentCellCoord = PortCellCoord + GridDirectionToOffset(PortGridDirection);
		
		FAtomicResolvedBuildingPort ResolvedPort;
		ResolvedPort.BuildingInstanceID = BuildingInstanceID;
		ResolvedPort.BuildingID = BuildingID;
		ResolvedPort.PortID = PortDefinition.PortID;
		ResolvedPort.PortType = PortDefinition.PortType;
		ResolvedPort.PortCellCoord = PortCellCoord;
		ResolvedPort.GridDirection = PortGridDirection;
		ResolvedPort.AdjacentCellCoord = AdjacentCellCoord;
		ResolvedPort.AcceptedItemID = PortDefinition.AcceptedItemID;
		
		OutPorts.Add(ResolvedPort);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// Belt Route Ports
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// Usually WEST for all 3 Route Types in default East rotation
EGridDirection UAtomicGridLibrary::GetDefaultInputPort(const EAtomicBeltRouteType RouteType)
{
	// Default Convention: Input is West for all simple belt route types.
	constexpr EGridDirection DefaultInputPort = EGridDirection::West;
	
	switch (RouteType)
	{
	case EAtomicBeltRouteType::Straight:	return DefaultInputPort;
	case EAtomicBeltRouteType::TurnLeft:	return DefaultInputPort;
	case EAtomicBeltRouteType::TurnRight:	return DefaultInputPort;
	default:								return DefaultInputPort;
	}
}

EGridDirection UAtomicGridLibrary::GetDefaultOutputPort(const EAtomicBeltRouteType RouteType)
{
	switch (RouteType)
	{
	case EAtomicBeltRouteType::Straight:	return EGridDirection::East;	// W->E
	case EAtomicBeltRouteType::TurnLeft:	return EGridDirection::North;	// W->N
	case EAtomicBeltRouteType::TurnRight:	return EGridDirection::South;	// W->S
	default:								return EGridDirection::East;
	}
}

EGridDirection UAtomicGridLibrary::DirectionFromCoordToCoord(const FIntVector& FromCoord, const FIntVector& ToCoord)
{
	const FIntVector Delta = ToCoord - FromCoord;
	
	if (Delta.X > 0) return EGridDirection::East;
	if (Delta.X < 0) return EGridDirection::West;
	if (Delta.Y > 0) return EGridDirection::South;
	if (Delta.Y < 0) return EGridDirection::North;
	
	return EGridDirection::East;	//Fallback
}

EBuildingRotation UAtomicGridLibrary::GetBeltVisualRotationFromInputPort(const EGridDirection InputPort)
{
	const EGridDirection FacingDirection = OppositeGridDirection(InputPort);
	return GridDirectionToBuildingRotation(FacingDirection);
}
