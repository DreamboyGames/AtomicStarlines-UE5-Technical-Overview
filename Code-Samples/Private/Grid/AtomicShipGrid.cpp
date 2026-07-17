// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicShipGrid.h"

#include "Components/BoxComponent.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicGridPlacementComponent.h"
#include "Grid/AtomicGridVisualComponent.h"
#include "ProjectTypes/AtomicGameTypes.h"


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

