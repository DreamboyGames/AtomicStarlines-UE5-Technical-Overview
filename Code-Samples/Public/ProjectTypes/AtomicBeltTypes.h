// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once
#include "AtomicGridTypes.h"
#include "AtomicBeltTypes.generated.h"

struct FAtomicBeltRecord;
enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;

UENUM(BlueprintType)
enum class EAtomicBeltRouteType : uint8 {
	Straight,	// Straight: Default Orientation { Input West -> Output East }
	TurnLeft,	// TurnLeft: Default Orientation { Input West -> Output North }
	TurnRight	// TurnRight: Default Orientation { Input West -> Output South }
};

UENUM(BlueprintType)
enum class EAtomicBeltVisualVariant : uint8 {
	Straight,	
	TurnLeft,
	TurnRight,
};

USTRUCT(BlueprintType)
struct FAtomicBeltPlacementCell {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	FIntVector GridCoord = FIntVector::ZeroValue;
	
	UPROPERTY(BlueprintReadOnly)
	int32 CellIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly)
	EAtomicBeltVisualVariant VisualVariant;
	
	UPROPERTY(BlueprintReadOnly)
	EGridDirection InputPort = EGridDirection::West;

	UPROPERTY(BlueprintReadOnly)
	EGridDirection OutputPort = EGridDirection::East;
};

USTRUCT(BlueprintType)
struct FAtomicResolvedBeltVisual {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EAtomicBeltVisualVariant VisualVariant = EAtomicBeltVisualVariant::Straight;
	
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

// Visual Resolver
struct FAtomicBeltVisualResolver {
	//static bool ResolveBeltVisual(const EAtomicBeltRouteType RouteType, EGridDirection InputPort, EGridDirection OutputPort, const TArray<EGridDirection>& ConnectedRoutePorts, FAtomicResolvedBeltVisual& OutResolvedVisual);
	static bool ResolveVisualVariantFromPorts(const EGridDirection InputPort, const EGridDirection OutputPort, EAtomicBeltVisualVariant& OutVariant);
	static bool ResolveVisualRotationFromPorts(const EGridDirection InputPort, const EGridDirection , EBuildingRotation& OutRotation);
};

// THESE FUNCTIONS ASSUME:
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