// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "ProjectTypes/AtomicGridTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AtomicBeltRecords.generated.h"

struct FAtomicBeltPlacementCell;
enum class EAtomicBeltRouteType : uint8;

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELT RECORDS -- FAST ARRAY
// Keep Compact!  Send IDs, not content.  --  Do not put meshes, materials, data asset pointers, or actor pointers in this record.
// FFastArraySerializerItem -- Replication tick: Unreal serializes only added/changed/removed items
/////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Belt Record stores runtime placed truth
/// One Belt Cell used for simulation, visuals, occupancy
USTRUCT(BlueprintType)
struct FAtomicBeltRecord : public FFastArraySerializerItem {
	GENERATED_BODY()
	// Straight: Default Orientation { Input West, Output East }
	// CornerUp: Default Orientation { Input West, Output North }
	// CornerDown: Default Orientation { Input West, Output South }
	
	// InputPort = side items enter from / connected neighbour side
	// OutputPort = side items leave from
	// RouteType = shape/path from input to output
	
	
	// Unique belt instance ID
	UPROPERTY()
	FGuid InstanceID;
	
	// The Belt Line this belt instance belongs to
	UPROPERTY()
	FGuid LineID;
	
	// ID to lookup Belt Definition in UAtomicBuildingRegistrySubsystem
	UPROPERTY()
	FName DefinitionID;
	
	UPROPERTY()
	int32 CellIndex = INDEX_NONE;
	
	UPROPERTY()
	int32 OwningPlayerID = INDEX_NONE;
	
	UPROPERTY()
	uint8 DeckIndex = 0;
	
	UPROPERTY()
	EGridDirection OutputPort = EGridDirection::East;
	
	UPROPERTY()
	EGridDirection InputPort = EGridDirection::West;
};

USTRUCT(BlueprintType)
struct FAtomicBeltRecordArray : public FFastArraySerializer  {
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FAtomicBeltRecord> Items;
	
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAtomicBeltRecord, FAtomicBeltRecordArray>(Items, DeltaParams, *this);
	}
	
	// FastArray Replication Callbacks
	// On Unreal Replication Tick: sees FastArray item/array dirty, serializes only the item delta, sends delta to clients.
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FAtomicBeltRecordArray> : public TStructOpsTypeTraitsBase2<FAtomicBeltRecordArray> {
	enum {
		WithNetDeltaSerializer = true
	};
};


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// One Player-Created Belt Line / Removable Group
USTRUCT(BlueprintType)
struct FAtomicBeltLineRecord {
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid LineID;
	
	UPROPERTY()
	FName BeltID;
	
	UPROPERTY()
	int32 OwningPlayerID = INDEX_NONE;
	
	UPROPERTY()
	FIntVector StartCoord;
	
	UPROPERTY()
	FIntVector EndCoord;
	
	UPROPERTY()
	bool bXFirst;
	
	UPROPERTY()
	TArray<FAtomicBeltPlacementCell> BeltCells;
};
