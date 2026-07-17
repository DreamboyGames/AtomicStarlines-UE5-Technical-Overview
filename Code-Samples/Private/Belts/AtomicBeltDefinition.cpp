// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Belts/AtomicBeltDefinition.h"
#include "ProjectTypes/AtomicBeltTypes.h"


UStaticMesh* UAtomicBeltDefinition::GetMeshForVariant(const EAtomicBeltVisualVariant VisualVariant) const
{
	switch (VisualVariant)
	{
	case EAtomicBeltVisualVariant::Straight:
		return StraightMesh;
		
	case EAtomicBeltVisualVariant::TurnLeft:
		return TurnLeftMesh;
		
	case EAtomicBeltVisualVariant::TurnRight:
		return TurnRightMesh;
	
	default:
		return nullptr;
	}	
}
