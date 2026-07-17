#pragma once

#include "CoreMinimal.h"
#include "AtomicGridTypes.generated.h"

/**
 * Gird Types, Building Types, and State Types
 * 
 * Building Definition (UAtomicBuildingDefinition) is in AtomicBuildingDefinition.h
 * Port Definition (FAtomicBuildingPortDefinition) is in AtomicBuildingDefinition.h
 * Building Records (FAtomicPlacedBuildRecord) is in AtomicPlacedBuildRecords.h
 * Belt Records (FAtomicBeltRecord) is in AtomicPlacedBuildRecords.h
 * 
 * Naming Rules:
 * - Direction = cardinal grid side / neighbour relation
 * - Rotation  = placed object orientation / yaw
 */


UENUM(BlueprintType)
enum class EPlacementMode : uint8{
	None,
	Building,
	Belt,
	Wall
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRID
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// Which neighbouring side/direction you are talking about
UENUM(BlueprintType)
enum class EGridDirection : uint8 {
	North,  // -Y (green axis)
	East,   // +X (red axis)
	South,  // +Y (green axis)
	West    // -X (red axis)
};

static constexpr EGridDirection AllDirections[] = {
	EGridDirection::East,
	EGridDirection::West,
	EGridDirection::North,
	EGridDirection::South,
};

UENUM(BlueprintType)
enum class EGridOccupancyType : uint8 {
	None,
	Building,
	Wall,
	Belt,
};


// bit-mask flags
UENUM(BlueprintType)
enum class EGridCellFlag : uint8 {
	None     = 0,
	HasFloor = 1 << 0,
	Blocked  = 1 << 1,
	Reserved = 1 << 2,
};


USTRUCT(BlueprintType)
struct FAtomicGridCell {
	GENERATED_BODY()
	
	// a cell can have multiple flags at once using a bit-mask
	UPROPERTY()
	uint8 Flags = 0;
	
	EGridOccupancyType OccupancyType = EGridOccupancyType::None;
	
	UPROPERTY()
	FGuid OccupyingInstanceID;
	
	bool IsOccupied() const
	{
		// OccupyingBuildInstanceID -> PlacedBuildRecord -> BuildingID -> BuildingDefinition
		return OccupancyType != EGridOccupancyType::None || OccupyingInstanceID.IsValid();
	}
	
	bool HasFlag(const EGridCellFlag Flag) const
	{
		const uint8 FlagMask = static_cast<uint8>(Flag);
		return (Flags & FlagMask) != 0;
	}
	
	void AddFlag(const EGridCellFlag Flag)
	{
		Flags |= static_cast<uint8>(Flag);
	}
	
	void RemoveFlag(const EGridCellFlag Flag)
	{
		Flags &= ~static_cast<uint8>(Flag);
	}
	
	void SetFlag(const EGridCellFlag Flag, const bool bEnabled)
	{
		if (bEnabled)
		{
			AddFlag(Flag);
		}
		else
		{
			RemoveFlag(Flag);
		}
	}
	
	void ClearFlags()
	{
		Flags = 0;
	}
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BUILDINGS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// How an object is Rotated
UENUM(BlueprintType)
enum class EBuildingRotation : uint8 {
	North,  // -Y
	East,   // +X
	South,  // +Y
	West    // -X
};

static constexpr EBuildingRotation AllRotations[] = {
	EBuildingRotation::East,
	EBuildingRotation::West,
	EBuildingRotation::North,
	EBuildingRotation::South,
};


// Replicated dynamic state
// Applied to existing local actors
USTRUCT(BlueprintType)
struct FAtomicBuildingStateRecord {
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid BuildInstanceID;
	
	UPROPERTY()
	int32 StoredInputAmount = 0;
	
	UPROPERTY()
	int32 StoredOutputAmount = 0;
	
	UPROPERTY()
	float ProductionProgress = 0.0f;
	
	UPROPERTY()
	uint8 bPowered = false;
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BUILDING PORTS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
UENUM(BlueprintType)
enum class EBuildingPortType : uint8 {
	Input,
	Output,
};


USTRUCT(BlueprintType)
struct FAtomicResolvedBuildingPort {
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid BuildingInstanceID;
	
	UPROPERTY()
	FName BuildingID;
	
	UPROPERTY()
	FName PortID;
	
	UPROPERTY()
	EBuildingPortType PortType = EBuildingPortType::Input;
	
	// Occupied building cell that owns the port
	UPROPERTY()
	FIntVector PortCellCoord;
	
	// Side the port faces after rotation
	UPROPERTY()
	EGridDirection GridDirection = EGridDirection::East;
	
	UPROPERTY()
	FIntVector AdjacentCellCoord;
	
	UPROPERTY()
	FName AcceptedItemID;
};

