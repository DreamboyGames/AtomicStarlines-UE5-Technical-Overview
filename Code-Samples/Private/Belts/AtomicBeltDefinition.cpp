// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Belts/AtomicBeltDefinition.h"
#include "Belts/AtomicBeltTypes.h"


UStaticMesh* UAtomicBeltDefinition::GetMeshForVariant(const EAtomicBeltVisualVariant VisualVariant) const
{
	switch (VisualVariant)
	{
	case EAtomicBeltVisualVariant::Straight:
		return StraightMesh;
		
	case EAtomicBeltVisualVariant::StraightEnd:
		return StraightEndMesh;
		
	case EAtomicBeltVisualVariant::StraightDoubleEnd:
		return StraightDoubleEndMesh;
		
	case EAtomicBeltVisualVariant::Corner:
		return CornerMesh;
		
	case EAtomicBeltVisualVariant::CornerEndA:
		return CornerEndMeshA;
		
	case EAtomicBeltVisualVariant::CornerEndB:
		return CornerEndMeshB;
		
	case EAtomicBeltVisualVariant::CornerDoubleEnd:
		return CornerDoubleEndMesh;
	
	default:
		return nullptr;
	}	
}
// CornerEndA:
// open side = RoutePorts[0]
// capped side = RoutePorts[1]
//
// CornerEndB:
// open side = RoutePorts[1]
// capped side = RoutePorts[0]