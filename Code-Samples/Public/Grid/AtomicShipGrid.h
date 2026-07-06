// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtomicShipGrid.generated.h"

enum class EBuildingRotation : uint8;
enum class EAtomicBeltShape : uint8;
struct FAtomicResolvedBeltVisual;
struct FAtomicBeltRecord;
class UBoxComponent;
class UAtomicGridVisualComponent;
class UAtomicGridPlacementComponent;
class UAtomicGridDataComponent;


// UAtomicGridPlacementComponent:
// Server-only placement validation
// Creates records
//
// UAtomicGridDataComponent
// Replicated PlacedBuildingRecords
// Local Cells cache
// Broadcasts record changes
//
// UAtomicGridVisualComponent
// Materializes records into local building actors
// Keeps map of BuildInstanceID → local actor
//
// AAtomicBuildingActor
// Non-replicated local representation
// Mesh/collision/interaction target
// Sends interaction requests by BuildInstanceID

UCLASS()
class ATOMICSTARLINES_API AAtomicShipGrid : public AActor {
	GENERATED_BODY()

public:
	AAtomicShipGrid();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostInitializeComponents() override;
	
	UAtomicGridDataComponent* GetGridData() const { return GridDataComponent; }
	
	UAtomicGridPlacementComponent* GetGridPlacementComponent() const { return PlacementComponent; }
	UAtomicGridVisualComponent* GetGridVisualComponent() const { return VisualComponent; } 
	
	bool ResolveBeltVisual(const FAtomicBeltRecord& BeltRecord, FAtomicResolvedBeltVisual& OutResolvedVisual) const;
	bool ResolveBeltPreviewVisual(const FAtomicBeltRecord& PreviewBeltRecord, FAtomicResolvedBeltVisual& OutResolvedVisual) const;
	
	void GetValidBeltPlacementRotations(const int32 Index, const EAtomicBeltShape Shape, TArray<EBuildingRotation>& OutRotations) const;
	int32 CountBeltConnectionsForRotation(const int32 Index, const EAtomicBeltShape BeltShape, const EBuildingRotation CandidateRotation) const;

	
protected:
	// @todo: UAtomicShipGridDefinition Data Asset to set ship grid defaults per ship 
	//TObjectPtr<UAtomicShipGridDefinition> GridDefinition;

	// Replicated PlacedBuildingRecords
	// Local Cells cache
	// Broadcasts record changes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AtomicGrid")
	TObjectPtr<UAtomicGridDataComponent> GridDataComponent;

	// Not Replicated
	// Server-only placement validation. Client-side preview checks.
	// Creates records
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AtomicGrid")
	TObjectPtr<UAtomicGridPlacementComponent> PlacementComponent;

	// Not Replicated. Rebuilds local visuals.
	// Materializes records into local building actors
	// Keeps map of BuildInstanceID → local actor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AtomicGrid")
	TObjectPtr<UAtomicGridVisualComponent> VisualComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AtomicGrid")
	TObjectPtr<USceneComponent> SceneRoot;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AtomicGrid")
	TObjectPtr<UBoxComponent> TraceBounds;
	
private:
	void UpdateGidTraceBounds() const;

public:
};

// Controller Placement Component = player intent / preview
// Grid Placement Component       = rules / validation / mutation
// Grid Data Component            = grid state / occupancy cache / placed records
// Building Definition            = type data / footprint / cost / actor class
// Building Actor                 = spawned runtime object
// Placed Building Record         = lightweight saved/replicated instance data


// UAtomicPlayerPlacementComponent
// = chooses BuildableId, Coord, Rotation
// = previews placement
//
// UAtomicGridPlacementComponent
// = resolves BuildingDefinition
// = calculates footprint
// = validates placement
// = tells GridData to commit
//
// UAtomicGridDataComponent
// = stores Cells and PlacedBuildRecords
// = marks occupancy cache
// = rebuilds cache from records
//
// UAtomicBuildableDefinition
// = footprint, actor class, cost, ports
//
// FAtomicPlacedBuildRecord
// = one placed instance: BuildableId + Coord + Rotation + InstanceId
//
// AAtomicBuildableActor
// = spawned visual/interactive runtime object
