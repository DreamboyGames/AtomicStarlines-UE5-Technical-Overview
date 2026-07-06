// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#include "Building/AtomicBuildingRecords.h"
#include "Grid/AtomicGridDataComponent.h"



///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BUILDING RECORDS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

// FastArray Replication Callback
// On Unreal Replication Tick: sees FastArray item/array dirty, serializes only the item delta, sends delta to clients.
void FAtomicBuildingRecordArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : AddedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBuildingRecordAdded(Items[Index]);
		}
	}
}
// FastArray Replication Callback
// On Unreal Replication Tick: sees FastArray item/array dirty, serializes only the item delta, sends delta to clients.
void FAtomicBuildingRecordArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : ChangedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBuildingRecordChanged(Items[Index]);
		}
	}
}
// FastArray Replication Callback
// On Unreal Replication Tick: sees FastArray item/array dirty, serializes only the item delta, sends delta to clients.
void FAtomicBuildingRecordArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : RemovedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBuildingRecordRemoved(Items[Index]);
		}
	}
}
