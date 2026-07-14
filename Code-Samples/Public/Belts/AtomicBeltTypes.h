// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once
#include "Grid/AtomicGridTypes.h"
#include "AtomicBeltTypes.generated.h"

struct FAtomicBeltRecord;
enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;

UENUM(BlueprintType)
enum class EAtomicBeltRouteType : uint8 {
	// Straight: Default Orientation { Input West -> Output East }
	Straight,
	// CornerUp: Default Orientation { Input West -> Output North }
	TurnLeft,
	// CornerDown: Default Orientation { Input West -> Output South }
	TurnRight
};

UENUM(BlueprintType)
enum class EAtomicBeltVisualVariant : uint8 {
	Straight,
	StraightEnd,
	StraightEndDouble,
	
	TurnLeft,
	TurnLeftEndInputConnected,		// open input, cap output
	TurnLeftEndOutputConnected,		// open output, cap input
	TurnLeftEndDouble,
	
	TurnRight,
	TurnRightEndInputConnected,		// open input, cap output
	TurnRightEndOutputConnected,	// open output, cap input
	TurnRightEndDouble
};

UENUM(BlueprintType)
enum class EAtomicBeltConnectionRole : uint8 {
	None,
	InputPort,
	OutputPort
};

USTRUCT(BlueprintType)
struct FAtomicResolvedBeltVisual {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EAtomicBeltVisualVariant VisualVariant = EAtomicBeltVisualVariant::StraightEndDouble;
	
	UPROPERTY(BlueprintReadOnly)
	EBuildingRotation VisualRotation = EBuildingRotation::East;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<EGridDirection> RoutePorts;
	
	UPROPERTY(BlueprintReadOnly)
	TArray<EGridDirection> ConnectedRoutePorts;
	
	UPROPERTY(BlueprintReadOnly)
	bool bReverseMaterialFlow = false;
	
	UPROPERTY(BlueprintReadOnly)
	bool bIsValid = false;
};

struct FAtomicBeltPlacementCandidate {
	// Shape/Path from Input to Output
	EAtomicBeltRouteType RouteType;
	EBuildingRotation RouteRotation;
	EGridDirection InputPort;
	EGridDirection OutputPort;
	
	// Derived: { InputPort, OutputPort }
	TArray<EGridDirection> RoutePorts;
	TArray<EGridDirection> ConnectedRoutePorts;
	
	int32 Score = 0;
};

struct FAtomicResolvedPreviewBelt {
	// Shape/Path from Input to Output
	EAtomicBeltRouteType RouteType;
	EBuildingRotation RouteRotation;
	EGridDirection InputPort;
	EGridDirection OutputPort;
	
	// Derived: { InputPort, OutputPort }
	TArray<EGridDirection> RoutePorts;
	TArray<EGridDirection> ConnectedRoutePorts;
	
	EAtomicBeltVisualVariant VisualVariant;
	EBuildingRotation VisualRotation;
	
	bool bReverseMaterialFlow = false;
	
	bool bIsValid = false;
};

// ---------------------------------------------------------------------
// BELT LOGIC PURE HELPERS
// ---------------------------------------------------------------------
// These Functions Assume:
//
// Rotation = direction the belt piece is extending / capped toward.
// Connected/Open Port = Opposite(Rotation)
//
// InputPort  = side items enter from
// OutputPort = side items leave from
// RouteType  = Straight / Corner-up / Corner-down
//
// Straight default route ports: { West, East }
// Corner   default route ports: { West, North }
//
// StraightEnd mesh:
// - default East rotation = open East, cap West
//
// CornerUpEnd:
// - open RoutePorts[0]
// - cap  RoutePorts[1]
//
// CornerDownEnd:
// - open RoutePorts[1]
// - cap  RoutePorts[0]

// Visual Resolver
struct FAtomicBeltVisualResolver {
	static bool ResolveBeltVisual(const EAtomicBeltRouteType RouteType, EGridDirection InputPort, EGridDirection OutputPort, const TArray<EGridDirection>& ConnectedRoutePorts, FAtomicResolvedBeltVisual& OutResolvedVisual);
};
