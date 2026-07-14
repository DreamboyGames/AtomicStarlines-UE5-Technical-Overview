// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AtomicGameTypes.h"
#include "Belts/AtomicBeltDefinition.h"
#include "Belts/AtomicBeltTypes.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Components/ActorComponent.h"
#include "Grid/AtomicGridTypes.h"
#include "AtomicPlayerPlacementComponent.generated.h"

class UAtomicBuildingRegistrySubsystem;
enum class EAtomicBeltRouteType : uint8;
class UAtomicBeltDefinition;
class AAtomicPlacementGhostActor;
class UAtomicBuildingDefinition;
class AAtomicShipGrid;
class AAtomicPlayerController;



// Where on the grid are we aiming?
USTRUCT()	
struct FAtomicPlacementTarget {
	GENERATED_BODY()
	
	UPROPERTY()
	TObjectPtr<AAtomicShipGrid> ShipGrid = nullptr;
	
	UPROPERTY()
	FTransform GridTransform = FTransform::Identity;

	UPROPERTY() 
	FIntVector GridSize = FIntVector::ZeroValue;
	
	UPROPERTY() // Anchor Coord
	FIntVector GridCoord = FIntVector::ZeroValue;

	UPROPERTY()
	int32 GridIndex = INDEX_NONE;
	
	UPROPERTY()
	float CellSize = 200.f;
	
	UPROPERTY()
	FHitResult HitResult;

	bool IsValid() const
	{
		return ShipGrid != nullptr && GridIndex != INDEX_NONE;
	}
};

// What should the ghost look like right now?
struct FAtomicPlacementPreviewResult {
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	
	// For Buildings, footprint center. For Belts, cell center. For Walls, edge/segment center.
	FVector WorldLocation = FVector::ZeroVector;
	FRotator WorldRotation = FRotator::ZeroRotator;
	
	// Optional, useful for debug drawing / footprint highlight.
	TArray<FIntVector> PreviewCells;
	
	FAtomicResolvedPreviewBelt ResolvedPreviewBelt;
	bool bHasResolvedBelt = false;
	
	bool bCanPlace = false;
	bool bShowGhost = false;

	bool IsValid() const
	{
		return bShowGhost && Mesh != nullptr;
	}
};

// Current Placement Object Selection
USTRUCT() // Component State, persists while placement mode is active and hold UObject refs
struct FAtomicPlacementSelection {
	GENERATED_BODY()
	
	UPROPERTY()
	EPlacementMode PlacementMode = EPlacementMode::None;
	
	UPROPERTY()
	FName DefinitionID = NAME_None;
	
	UPROPERTY()
	TObjectPtr<const UAtomicBuildingDefinition> BuildingDefinition = nullptr;
	
	UPROPERTY()
	TObjectPtr<const UAtomicBeltDefinition> BeltDefinition = nullptr;

	// Building Rotation, and
	// Route Rotation: which sides this belt can connect to, e.g. (E,W) or (E,S)
	UPROPERTY()
	EBuildingRotation Rotation = EBuildingRotation::East;
	
	// Tool player selected
	UPROPERTY()
	EAtomicBeltRouteType InitialRouteType = EAtomicBeltRouteType::Straight;
	
	UPROPERTY()
	EAtomicBeltRouteType RouteType = EAtomicBeltRouteType::Straight;

	UPROPERTY()
	EGridDirection InputPort = EGridDirection::West;
	
	UPROPERTY()
	EGridDirection OutputPort = EGridDirection::East;
	
	bool IsValid() const
	{
		switch (PlacementMode)
		{
		case EPlacementMode::Building:
			return !DefinitionID.IsNone() && BuildingDefinition != nullptr && BuildingDefinition->BuildingID == DefinitionID;	
			
		case EPlacementMode::Belt:
			return !DefinitionID.IsNone() && BeltDefinition != nullptr && BeltDefinition->BeltID == DefinitionID;	
			
		default:
			return false;
		}
	}
	
	void ResetPlacementSelection()
	{
		PlacementMode = EPlacementMode::None;
		DefinitionID = NAME_None;
		BuildingDefinition = nullptr;
		BeltDefinition = nullptr;
		RouteType = EAtomicBeltRouteType::Straight;
		Rotation = EBuildingRotation::East;
	}
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicPlayerPlacementComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicPlayerPlacementComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// START PLACEMENT
	bool StartPlacementWithSelectedBuilding(const FName BuildingID);
	bool StartPlacementWithSelectedBelt(const FName BeltID, const EAtomicBeltRouteType InitialRouteType);

	void RotatePlacement(const float InputValue);
	
	// CONFIRM / CANCEL PLACEMENT
	void ConfirmPlacement();
	void CancelPlacement();

protected:
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementDistance;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementDistanceStep;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float MinPlacementDistanceStep;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float MaxPlacementDistanceStep;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementInputDeadZone;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementStepRepeatDelay;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementRotationStepDelay;
	
	UPROPERTY(EditAnywhere, Category="AtomicPlacement")
	float PlacementTraceHeight;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UMaterialInterface> ValidPreviewMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UMaterialInterface> InvalidPreviewMaterial;
	
	
private:	
	// Update Preview Ghost
	void UpdatePlacementPreview();
	bool FindPlacementTarget(FAtomicPlacementTarget& OutTarget);
	void ApplyPlacementPreview(const FAtomicPlacementPreviewResult& PreviewResult);

	void EnsureGhostActor();
	void HidePlacementPreview() const;

	// Start/Clear Placement
	void StartPlacement();
	void ClearSelectedPlacement();
	
	// Build Preview
	bool BuildPlacementPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildWallPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;

	// Rotation
	void RotateBuildingPlacement(const float InputValue);
	void UpdatePlacementDistanceFromStep();
	
	void RotateBeltPlacement(const float InputValue);
	void RotateBeltRouteNormally(const bool bRotateClockwise);

	// Belt Helpers
	void UpdateAutoBeltSelectionForTarget(const FAtomicPlacementTarget& Target);
	bool TryFindSnappedConnectionFromNeighbour(const FAtomicPlacementTarget& Target, EGridDirection& OutConnectedPort, EAtomicBeltConnectionRole& OutConnectionRole) const;
	
	void SetCurrentBeltRoute(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort);
	void SetCurrentBeltRouteFromInput(const EAtomicBeltRouteType RouteType, const EGridDirection InputPort);
	void SetCurrentBeltRouteFromOutput(const EAtomicBeltRouteType RouteType, const EGridDirection OutputPort);

	void CycleConnectedBeltRoute(const bool bRotateClockwise);
	bool TryGetNextOutputPortAroundInput(const EGridDirection InputPort, const EGridDirection CurrentOutputPort, const bool bRotateClockwise, EGridDirection& OutNextOutputPort) const;
	bool TryGetNextInputPortAroundOutput(const EGridDirection OutputPort, const EGridDirection CurrentInputPort, const bool bRotateClockwise, EGridDirection& OutNextInputPort) const;

	bool bBeltInputSnappedToNeighbour = false;
	bool bBeltHasSnappedConnection = false;
	EGridDirection SnappedBeltConnectedPort = EGridDirection::East;
	EAtomicBeltConnectionRole SnappedBeltConnectionRole = EAtomicBeltConnectionRole::None;


	// Draw Debugs
	void DrawDebugPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	void DrawDebugBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	void DrawDebugBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;

	
	UPROPERTY(Transient)
	FAtomicPlacementSelection CurrentSelection;

	UPROPERTY(Transient)
	TObjectPtr<AAtomicPlacementGhostActor> GhostActor;

	bool bIsPlacementActive = false;
	bool bHasValidPlacement = false;
	
	// Belt preview auto-selection state.
	// Auto-select is allowed until the player manually rotates on this target cell.
	UPROPERTY(Transient)
	bool bBeltRotationManuallyOverridden = false;
	
	bool bPlacementRequestPending = false;
	
	float PlacementStepRepeatTimer = 0.0f;
	int32 LastPlacementStepDirection = 0;
	float LastPlacementRotationTime = -1000.0f;
	
	UPROPERTY(VisibleAnywhere, Category=Components)
	TObjectPtr<AAtomicPlayerController> LocalController;
	
	UPROPERTY()
	TObjectPtr<UAtomicBuildingRegistrySubsystem> PlacementRegistry;
	
	UPROPERTY(Transient)
	FAtomicPlacementTarget CurrentTarget;
	
	UPROPERTY(Transient)
	int32 LastTargetGridIndex = INDEX_NONE;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<AAtomicShipGrid> LastTargetShipGrid;

	
public:
	void AdjustPlacementDistance(const float InputValue, const float DeltaTime);
};
