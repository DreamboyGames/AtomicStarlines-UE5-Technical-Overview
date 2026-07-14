// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#include "Belts/AtomicBeltTypes.h"

#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridTypes.h"


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


bool FAtomicBeltVisualResolver::ResolveBeltVisual(const EAtomicBeltRouteType RouteType, EGridDirection InputPort, EGridDirection OutputPort, const TArray<EGridDirection>& ConnectedRoutePorts, FAtomicResolvedBeltVisual& OutResolvedVisual)
{
	OutResolvedVisual = FAtomicResolvedBeltVisual();
	OutResolvedVisual.bIsValid = false;
	
	// 1. Validate route truth.
	const EGridDirection ExpectedOutputPort = UAtomicGridLibrary::GetOutputPortForInput(RouteType, InputPort);
	if (ExpectedOutputPort != OutputPort)
	{
		return false;
	}
	
	TArray<EGridDirection> RoutePorts;
	UAtomicGridLibrary::GetRoutePortsForInput(RouteType, InputPort, RoutePorts);
	
	if (!RoutePorts.Contains(InputPort) || !RoutePorts.Contains(OutputPort)) return false;
	
	// 2. Keep only valid connected ports that belong to this route.
	TArray<EGridDirection> ValidConnectedPorts;
	ValidConnectedPorts.Reserve(2);

	for (const EGridDirection ConnectedPort : ConnectedRoutePorts)
	{
		if (RoutePorts.Contains(ConnectedPort))
		{
			ValidConnectedPorts.AddUnique(ConnectedPort);
		}
	}

	const int32 ConnectedCount = ValidConnectedPorts.Num();
	if (ConnectedCount > 2) return false;
	
	const bool bHasSingleConnectedPort = ConnectedCount == 1;
	const EGridDirection SingleConnectedPort = bHasSingleConnectedPort ? ValidConnectedPorts[0] : EGridDirection::West;

	// 3. Choose visual variant from RouteType + connected count.
	switch (RouteType)
	{
		case EAtomicBeltRouteType::Straight:
		{
			if (ConnectedCount == 0)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::StraightEndDouble;
			}
			else if (ConnectedCount == 1)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::StraightEnd;
			}
			else
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::Straight;
			}
			break;
		}
		case EAtomicBeltRouteType::TurnLeft:
		{
			if (ConnectedCount == 0)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::TurnLeftEndDouble;
			}
			else if (ConnectedCount == 1)
			{
				OutResolvedVisual.VisualVariant = SingleConnectedPort == InputPort 
					? EAtomicBeltVisualVariant::TurnLeftEndInputConnected 
					: EAtomicBeltVisualVariant::TurnLeftEndOutputConnected;
			}
			else
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::TurnLeft;
			}
			break;
		}
		case EAtomicBeltRouteType::TurnRight:
		{
			if (ConnectedCount == 0)
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::TurnRightEndDouble;
			}
			else if (ConnectedCount == 1)
			{
				OutResolvedVisual.VisualVariant = SingleConnectedPort == InputPort 
					? EAtomicBeltVisualVariant::TurnRightEndInputConnected 
					: EAtomicBeltVisualVariant::TurnRightEndOutputConnected;
			}
			else
			{
				OutResolvedVisual.VisualVariant = EAtomicBeltVisualVariant::TurnRight;
			}
			break;
		}
		default: 
			return false;
	}
	
	// 4. Choose visual rotation
	//		- Full / Double-end: use normal route facing from InputPort
	//		- Single-end: rotate mesh toward the capped side.
	EGridDirection VisualFacingPort = OutputPort;
	if (ConnectedCount == 1)
	{
		for (const EGridDirection RoutePort : RoutePorts)
		{
			if (RoutePort != SingleConnectedPort)
			{
				VisualFacingPort = RoutePort;
				break;
			}
		}
		
		OutResolvedVisual.VisualRotation = UAtomicGridLibrary::GridDirectionToBuildingRotation(VisualFacingPort);
	}
	else
	{
		OutResolvedVisual.VisualRotation = UAtomicGridLibrary::GetBuildingRotationForInputPort(InputPort);
	}
	
	// 5. Fill result.
	OutResolvedVisual.RoutePorts = RoutePorts;
	OutResolvedVisual.ConnectedRoutePorts = ValidConnectedPorts;

	OutResolvedVisual.bIsValid = true;
	return true;
}
