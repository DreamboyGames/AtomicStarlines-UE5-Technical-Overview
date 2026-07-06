// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#include "Belts/AtomicBeltRecords.h"
#include "Grid/AtomicGridDataComponent.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELT RECORDS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void FAtomicBeltRecordArray::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : AddedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBeltRecordAdded(Items[Index]);
		}
	}
}

void FAtomicBeltRecordArray::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : ChangedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBeltRecordChanged(Items[Index]);
		}
	}
}

void FAtomicBeltRecordArray::PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize)
{
	if (!OwningObject) return;
	
	for (const int32 Index : RemovedIndices)
	{
		if (Items.IsValidIndex(Index))
		{
			Cast<UAtomicGridDataComponent>(OwningObject)->HandleBeltRecordRemoved(Items[Index]);
		}
	}
}
