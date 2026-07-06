// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "Grid/AtomicGridTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AtomicBuildingRecords.generated.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// PLACED BUILD RECORDS -- FAST ARRAY
// Keep Compact!  Send IDs, not content.  --  Do not put meshes, materials, data asset pointers, or actor pointers in this record.
// FFastArraySerializerItem -- Replication tick: Unreal serializes only added/changed/removed items
/////////////////////////////////////////////////////////////////////////////////////////////////////////
USTRUCT(BlueprintType)
struct FAtomicBuildingRecord : public FFastArraySerializerItem {
	GENERATED_BODY()
	
	// change to per-grid generated id using uint32 (smaller than FGuid)
	UPROPERTY()
	FGuid BuildInstanceID;
	
	// change to more compact numeric building type, uint16 enum, and map to BuildingDefinition through the registry
	// uint16 BuildingTypeID
	UPROPERTY()
	FName BuildingID;
	
	UPROPERTY() // uint16
	int32 CellIndex = INDEX_NONE;

	UPROPERTY() // uint8
	int32 OwningPlayerID = INDEX_NONE;
	
	UPROPERTY()
	uint8 DeckIndex = 0;

	UPROPERTY()
	EBuildingRotation Rotation = EBuildingRotation::North;
};

USTRUCT(BlueprintType)
struct FAtomicBuildingRecordArray : public FFastArraySerializer  {
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FAtomicBuildingRecord> Items;
	
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAtomicBuildingRecord, FAtomicBuildingRecordArray>(Items, DeltaParams, *this);
	}
	
	// FastArray Replication Callbacks
	// On Unreal Replication Tick: sees FastArray item/array dirty, serializes only the item delta, sends delta to clients.
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
};

template<>
struct TStructOpsTypeTraits<FAtomicBuildingRecordArray> : public TStructOpsTypeTraitsBase2<FAtomicBuildingRecordArray> {
	enum {
		WithNetDeltaSerializer = true
	};
};
