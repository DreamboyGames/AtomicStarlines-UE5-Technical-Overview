// Copyright Dreamboy Games Pty Ltd. All rights reserved.


#include "ProjectTypes/AtomicBeltTypes.h"

#include "Grid/AtomicGridLibrary.h"
#include "ProjectTypes/AtomicGridTypes.h"


namespace {
	bool HasExactlyTwoPorts(const TArray<EGridDirection>& Ports, const EGridDirection A, const EGridDirection B)
	{
		return Ports.Num() == 2 && Ports.Contains(A) && Ports.Contains(B);
	}
	
	EBuildingRotation GetVisualRotationForOpenPort(const EGridDirection OpenPort)
	{
		switch (OpenPort)
		{
		case EGridDirection::North: return EBuildingRotation::North;
		case EGridDirection::East:	return EBuildingRotation::East;
		case EGridDirection::South: return EBuildingRotation::South;
		case EGridDirection::West:	return EBuildingRotation::West;
		default:					return EBuildingRotation::East;
		}
	}
	
	EBuildingRotation ResolveCanonicalStraightVisualRotation(const TArray<EGridDirection>& RoutePorts)
	{
		// Straight only has 2 visual orientations.
		// Pick a stable canoncial rotation based on the route axis.
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::West, EGridDirection::East))
		{
			return EBuildingRotation::East;
		}
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::North, EGridDirection::South))
		{
			return EBuildingRotation::North;
		}
		
		return EBuildingRotation::East;
	}
	
	EBuildingRotation ResolveCanonicalCornerVisualRotation(const TArray<EGridDirection>& RoutePorts)
	{
		// Default corner mesh route = { West, North } at East rotation.
		// Rotate that canonical corner around the 4 directions.
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::West, EGridDirection::North))
		{
			return EBuildingRotation::East;
		}
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::North, EGridDirection::East))
		{
			return EBuildingRotation::South;
		}
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::East, EGridDirection::South))
		{
			return EBuildingRotation::West;
		}
		if (HasExactlyTwoPorts(RoutePorts, EGridDirection::South, EGridDirection::West))
		{
			return EBuildingRotation::North;
		}
		
		return EBuildingRotation::East;
	}
}


bool FAtomicBeltVisualResolver::ResolveVisualRotationFromPorts(const EGridDirection InputPort, const EGridDirection OutputPort, EBuildingRotation& OutRotation)
{
	if (InputPort == OutputPort) return false;
	OutRotation = UAtomicGridLibrary::GetBeltVisualRotationFromInputPort(InputPort);
	return true;
}

bool FAtomicBeltVisualResolver::ResolveVisualVariantFromPorts(const EGridDirection InputPort, const EGridDirection OutputPort, EAtomicBeltVisualVariant& OutVariant)
{
	// Assuming Authored Meshes Are:
	// Input West → Output East  = Straight
	// Input West → Output North = TurnLeft
	// Input West → Output South = TurnRight
	
	// Rotate Belt so that InputPort becomes world West,
	// Inspect where OutputPort lands
	
	if (InputPort == OutputPort) return false;

	switch (InputPort)
	{
	case EGridDirection::West:
	{
		if (OutputPort == EGridDirection::East)
		{
			OutVariant = EAtomicBeltVisualVariant::Straight;
			return true;
		}
		if (OutputPort == EGridDirection::North)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnLeft;
			return true;
		}
		if (OutputPort == EGridDirection::South)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnRight;
			return true;
		}
		return false;
	}
	case EGridDirection::East:
	{
		if (OutputPort == EGridDirection::West)
		{
			OutVariant = EAtomicBeltVisualVariant::Straight;
			return true;
		}
		if (OutputPort == EGridDirection::South)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnLeft;
			return true;
		}
		if (OutputPort == EGridDirection::North)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnRight;
			return true;
		}
		return false;
	}
	case EGridDirection::North:
	{
		if (OutputPort == EGridDirection::South)
		{
			OutVariant = EAtomicBeltVisualVariant::Straight;
			return true;
		}
		if (OutputPort == EGridDirection::East)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnLeft;
			return true;
		}
		if (OutputPort == EGridDirection::West)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnRight;
			return true;
		}
		return false;
	}
	case EGridDirection::South:
	{
		if (OutputPort == EGridDirection::North)
		{
			OutVariant = EAtomicBeltVisualVariant::Straight;
			return true;
		}
		if (OutputPort == EGridDirection::West)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnLeft;
			return true;
		}
		if (OutputPort == EGridDirection::East)
		{
			OutVariant = EAtomicBeltVisualVariant::TurnRight;
			return true;
		}
		return false;
	}
	default:
		return false;
	}
}
