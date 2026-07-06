// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "Grid/AtomicGridTypes.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "AtomicBeltRecords.generated.h"


enum class EAtomicBeltShape : uint8;
///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELT RECORDS -- FAST ARRAY
// Keep Compact!  Send IDs, not content.  --  Do not put meshes, materials, data asset pointers, or actor pointers in this record.
// FFastArraySerializerItem -- Replication tick: Unreal serializes only added/changed/removed items
/////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Belt Record stores runtime placed truth
USTRUCT(BlueprintType)
struct FAtomicBeltRecord : public FFastArraySerializerItem {
	GENERATED_BODY()
	
	UPROPERTY()
	FGuid InstanceID;
	
	UPROPERTY()
	FName BeltID;
	
	UPROPERTY()
	int32 CellIndex = INDEX_NONE;
	
	UPROPERTY()
	int32 OwningPlayerID = INDEX_NONE;
	
	UPROPERTY()
	uint8 DeckIndex = 0;
	
	UPROPERTY()
	EAtomicBeltShape Shape;
	
	// Defines possible connection sides only. E.g. (E,W) or (E,S)
	UPROPERTY()
	EBuildingRotation RouteRotation = EBuildingRotation::East;
	
	// Flow Direction, item Output port
	UPROPERTY()
	EGridDirection OutputFlowDirection = EGridDirection::East;
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
