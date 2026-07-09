// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once
#include "Grid/AtomicGridTypes.h"
#include "AtomicBeltTypes.generated.h"

struct FAtomicBeltRecord;
enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;

UENUM(BlueprintType)
enum class EAtomicBeltShape : uint8 {
	Straight,
	Corner
};

UENUM(BlueprintType)
enum class EAtomicBeltVisualVariant : uint8 {
	Straight,
	StraightEnd,
	StraightDoubleEnd,
	Corner,
	CornerEndA,
	CornerEndB,
	CornerDoubleEnd,
};

USTRUCT(BlueprintType)
struct FAtomicResolvedBeltVisual {
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadOnly)
	EAtomicBeltVisualVariant VisualVariant = EAtomicBeltVisualVariant::Straight;
	
	UPROPERTY(BlueprintReadOnly)
	EBuildingRotation VisualRotation = EBuildingRotation::East;
	
	bool bReverseMaterialFlow = false;
};

// ---------------------------------------------------------------------
// BELT LOGIC PURE HELPERS
// ---------------------------------------------------------------------

// Visual Resolver
struct FAtomicBeltVisualResolver {
public:
	static bool ResolveBeltVisualFromConnectedRoutePorts(const EAtomicBeltShape BeltShape, EBuildingRotation BeltRotation, const TArray<EGridDirection>& RoutePorts, const TArray<EGridDirection>& ConnectedPorts, FAtomicResolvedBeltVisual& OutResolvedVisual);
	
private:
	static EBuildingRotation GridDirectionToEquivalentBuildingRotation(EGridDirection GridDirection);
	static bool ChooseCornerEndAorB(const TArray<EGridDirection>& RoutePorts, const EGridDirection ConnectedPort, EAtomicBeltVisualVariant& OutVariant);
};

// Gameplay Topology (route ports, input side, output side, flow direction)
struct FAtomicBeltTopologyRules {
	static bool TryGetInputGridDirection(EAtomicBeltShape BeltShape, EBuildingRotation BeltRotation, EGridDirection OutputGridDirection, EGridDirection& OutInputGridDirection);
};

struct FAtomicBeltPlacementCandidate {
	EAtomicBeltShape Shape;
	EBuildingRotation Rotation;
	EGridDirection OutputFlowDirection;
	
	TArray<EGridDirection> RoutePorts;
	TArray<EGridDirection> ConnectedRoutePorts;
	
	int32 Score = 0;
};

struct FAtomicResolvedPreviewBelt {
	EAtomicBeltShape BeltShape;
	EBuildingRotation RouteRotation;
	EGridDirection OutputFlowDirection;
	
	TArray<EGridDirection> RoutePorts;
	TArray<EGridDirection> ConnectedRoutePorts;
	
	EAtomicBeltVisualVariant VisualVariant;
	EBuildingRotation VisualRotation;
	
	bool bReverseMaterialFlow = false;
	
	bool bIsValid = false;
};
