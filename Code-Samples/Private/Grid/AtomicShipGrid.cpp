// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicShipGrid.h"

#include "AtomicGameTypes.h"
#include "VisualizeTexture.h"
#include "Belts/AtomicBeltTypes.h"
#include "Components/BoxComponent.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridLibrary.h"
#include "Grid/AtomicGridPlacementComponent.h"
#include "Grid/AtomicGridVisualComponent.h"


AAtomicShipGrid::AAtomicShipGrid()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetNetUpdateFrequency(30.f);   // 60.f, if feels laggy
	SetMinNetUpdateFrequency(10.f);
	NetPriority = 2.f;
	// Call ForceNetUpdate() after important changes

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"));
	SetRootComponent(SceneRoot);
	
	GridDataComponent = CreateDefaultSubobject<UAtomicGridDataComponent>(TEXT("GridData"));
	PlacementComponent = CreateDefaultSubobject<UAtomicGridPlacementComponent>(TEXT("PlacementComponent"));
	VisualComponent = CreateDefaultSubobject<UAtomicGridVisualComponent>(TEXT("VisualComponent"));
	
	TraceBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("TraceBounds"));
	TraceBounds->SetupAttachment(SceneRoot);
	TraceBounds->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TraceBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
	TraceBounds->SetCollisionResponseToChannel(COLLISION_GRID_TRACE, ECR_Block);
	TraceBounds->SetGenerateOverlapEvents(false);
	// For Debug. Set true for build
	TraceBounds->SetHiddenInGame(false);
}

void AAtomicShipGrid::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	UpdateGidTraceBounds();
}

void AAtomicShipGrid::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	GridDataComponent->InitializeGrid();
	PlacementComponent->Initialize();
	VisualComponent->Initialize();
	UpdateGidTraceBounds();
}

void AAtomicShipGrid::UpdateGidTraceBounds() const
{
	if (!GridDataComponent || !TraceBounds) return;
	
	const FIntVector GridSize = GridDataComponent->GetGridSize();
	const float CellSize = GridDataComponent->GetCellSize();
	
	const float GridWorldSizeX = GridSize.X * CellSize;
	const float GridWorldSizeY = GridSize.Y * CellSize;
	
	// Box extent is half-size
	const FVector BoxExtent(GridWorldSizeX * 0.5f, GridWorldSizeY * 0.5f, 1);
	
	// This assumes grid local origin is at cell (0,0) corner
	const FVector BoxCenter(GridWorldSizeX * 0.5f, GridWorldSizeY * 0.5f, 0);
	
	TraceBounds->SetBoxExtent(BoxExtent);
	TraceBounds->SetRelativeLocation(BoxCenter);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELTS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

bool AAtomicShipGrid::ResolveBeltVisual(const FAtomicBeltRecord& BeltRecord, FAtomicResolvedBeltVisual& OutResolvedVisual) const
{
	if (!GridDataComponent) return false;
	
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(BeltRecord.Shape, BeltRecord.RouteRotation);
	
	TArray<EGridDirection> ConnectedPorts;
	GridDataComponent->GetConnectedRoutePortsForBelt(BeltRecord, ConnectedPorts);
	
	return FAtomicBeltVisualResolver::ResolveBeltVisual(BeltRecord.Shape, BeltRecord.RouteRotation, RoutePorts, ConnectedPorts, OutResolvedVisual);
}

bool AAtomicShipGrid::ResolveBeltPreviewVisual(const FAtomicBeltRecord& PreviewBeltRecord, FAtomicResolvedBeltVisual& OutResolvedVisual) const
{
	// For now this is identical to placed belt resolving.
	// The Preview record does not need to exist in the BeltRecords array;
	// connection checks only inspect neighbouring existing belts.
	return ResolveBeltVisual(PreviewBeltRecord, OutResolvedVisual);
}

void AAtomicShipGrid::GetValidBeltPlacementRotations(const int32 Index, const EAtomicBeltShape Shape, TArray<EBuildingRotation>& OutRotations) const
{
	OutRotations.Reset();
	
	for (const EBuildingRotation CandidateRotation : AllRotations)
	{
		const int32 ConnectionCount = CountBeltConnectionsForRotation(Index, Shape, CandidateRotation);
		if (ConnectionCount > 0)
		{
			OutRotations.Add(CandidateRotation);
		}
	}
	
	// Isolated belt: all rotations are valid.
	if (OutRotations.IsEmpty())
	{
		for (const EBuildingRotation Rotation : AllRotations)
		{
			OutRotations.Add(Rotation);
		}
	}
}

int32 AAtomicShipGrid::CountBeltConnectionsForRotation(const int32 Index, const EAtomicBeltShape BeltShape, const EBuildingRotation CandidateRotation) const
{
	if (!GridDataComponent) return 0;
	
	const TArray<EGridDirection> RoutePorts = UAtomicGridLibrary::GetBeltRotatedRoutePorts(BeltShape, CandidateRotation);
	int32 ConnectionCount = 0;
	
	for (const EGridDirection RoutePort : RoutePorts)
	{
		if (GridDataComponent->HasReciprocalBeltConnectionAtPort(Index, RoutePort))
		{
			++ConnectionCount;
		}
	}
	
	return ConnectionCount;
}

