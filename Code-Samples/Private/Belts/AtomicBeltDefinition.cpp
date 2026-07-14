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
		
	case EAtomicBeltVisualVariant::StraightEndDouble:
		return StraightDoubleEndMesh;
		
	case EAtomicBeltVisualVariant::TurnLeft:
		return TurnLeftMesh;
		
	case EAtomicBeltVisualVariant::TurnLeftEndInputConnected:
		return TurnLeftEndInputConnectedMesh;
		
	case EAtomicBeltVisualVariant::TurnLeftEndOutputConnected:
		return TurnLeftEndOutputConnectedMesh;
		
	case EAtomicBeltVisualVariant::TurnLeftEndDouble:
		return TurnLeftEndDoubleMesh;
		
	case EAtomicBeltVisualVariant::TurnRight:
		return TurnRightMesh;
		
	case EAtomicBeltVisualVariant::TurnRightEndInputConnected:
		return TurnRightEndInputConnectedMesh;
		
	case EAtomicBeltVisualVariant::TurnRightEndOutputConnected:
		return TurnRightEndOutputConnectedMesh;
		
	case EAtomicBeltVisualVariant::TurnRightEndDouble:
		return TurnRightEndDoubleMesh;
	
	default:
		return nullptr;
	}	
}
