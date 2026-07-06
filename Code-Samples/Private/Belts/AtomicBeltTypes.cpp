// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#include "Belts/AtomicBeltTypes.h"

#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridTypes.h"

bool FAtomicBeltVisualResolver::ResolveBeltVisual(const EAtomicBeltShape BeltShape, const EBuildingRotation BeltRotation, const TArray<EGridDirection>& RoutePorts, const TArray<EGridDirection>& ConnectedPorts, FAtomicResolvedBeltVisual& OutResolvedVisual)
{
	OutResolvedVisual = FAtomicResolvedBeltVisual();
	
	const int32 ConnectedPortCount = ConnectedPorts.Num();

	switch (BeltShape)
	{
	case EAtomicBeltShape::Straight:
		{
			if (ConnectedPortCount >= 2)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::Straight;
				OutResolvedVisual.VisualRotation = BeltRotation;
				return true;
			}
			if (ConnectedPortCount == 1)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::StraightEnd;
				OutResolvedVisual.VisualRotation = BeltRotation;
				return true;
			}
			
			OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::StraightDoubleEnd;
			OutResolvedVisual.VisualRotation = BeltRotation;
			return true;
		}
		
	case EAtomicBeltShape::Corner:
		{
			if (ConnectedPortCount >= 2)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::Corner;
				OutResolvedVisual.VisualRotation = BeltRotation;
				return true;
			}
			if (ConnectedPortCount == 1)
			{
				if (!ChooseCornerEndAorB(RoutePorts, ConnectedPorts[0], OutResolvedVisual.VisualVariant)) return false;
				OutResolvedVisual.VisualRotation = BeltRotation;
				return true;
			}
		
			OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::CornerDoubleEnd;
			OutResolvedVisual.VisualRotation = BeltRotation;
			return true;
		}
		
	default:
		return false;
	}
}

bool FAtomicBeltVisualResolver::ChooseCornerEndAorB(const TArray<EGridDirection>& RoutePorts, const EGridDirection ConnectedPort, EAtomicBeltVisualVariant& OutVariant)
{
	if (RoutePorts.Num() != 2) return false;
	
	if (ConnectedPort == RoutePorts[0])
	{
		OutVariant = EAtomicBeltVisualVariant::CornerEndA;
		return true;
	}
	
	if (ConnectedPort == RoutePorts[1])
	{
		OutVariant = EAtomicBeltVisualVariant::CornerEndB;
		return true;
	}
	
	// CornerEndA:
	// open side = RoutePorts[0]
	// capped side = RoutePorts[1]
	//
	// CornerEndB:
	// open side = RoutePorts[1]
	// capped side = RoutePorts[0]
	
	return false;
}

EBuildingRotation FAtomicBeltVisualResolver::GridDirectionToEquivalentBuildingRotation(const EGridDirection GridDirection)
{
	switch (GridDirection)
	{
	case EGridDirection::North:		return EBuildingRotation::North;
	case EGridDirection::East:		return EBuildingRotation::East;
	case EGridDirection::South:		return EBuildingRotation::South;
	case EGridDirection::West:		return EBuildingRotation::West;
	default:						return EBuildingRotation::East;
	}
}

bool FAtomicBeltTopologyRules::TryGetInputGridDirection(const EAtomicBeltShape BeltShape, const EBuildingRotation BeltRotation, const EGridDirection OutputGridDirection, EGridDirection& OutInputGridDirection)
{
	// This is fine for 2-port belts, straight and corner
	// @todo: splitters/mergers should not use this exact helper because they have multiple inputs/outputs
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(BeltShape, BeltRotation);
	if (!RoutePorts.Contains(OutputGridDirection)) return false;
	
	for (const EGridDirection RoutePort : RoutePorts)
	{
		if (RoutePort != OutputGridDirection)
		{
			OutInputGridDirection = RoutePort;
			return true;
		}
	}
	
	return false;
}
