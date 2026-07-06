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
enum class EAtomicBeltShape : uint8;
class UAtomicBeltDefinition;
class AAtomicPlacementGhostActor;
class UAtomicBuildingDefinition;
class AAtomicShipGrid;
class AAtomicPlayerController;


USTRUCT()	// Where on the grid are we aiming?
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
	
	bool bCanPlace = false;
	bool bShowGhost = false;
	
	// Optional, useful for debug drawing / footprint highlight.
	TArray<FIntVector> PreviewCells;
	
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
	
	UPROPERTY()
	EAtomicBeltShape BeltShape = EAtomicBeltShape::Straight;
	
	// Topology Direction, visual route orientation.
	// Preferred route rotation.
	UPROPERTY()
	EBuildingRotation Rotation = EBuildingRotation::East;
	
	// Flow Direction, item Output port
	UPROPERTY()
	EGridDirection OutputFlowDirection = EGridDirection::East;
	
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
		BeltShape = EAtomicBeltShape::Straight;
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

	bool StartPlacementWithSelectedBuilding(const FName BuildingID);
	bool StartPlacementWithSelectedBelt(const FName BeltID, const EAtomicBeltShape BeltShape);
	
	void CancelPlacement();
	void RotatePlacement(const float InputValue);
	
	void ConfirmPlacement();

	
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
	void UpdatePlacementPreview();
	bool FindPlacementTarget(FAtomicPlacementTarget& OutTarget);
	void ApplyPlacementPreview(const FAtomicPlacementPreviewResult& PreviewResult);
	void DrawDebugPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;

	void EnsureGhostActor();
	void HidePlacementPreview() const;
	
	bool BuildPlacementPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult) const;
	bool BuildWallPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	
	void RotateBuildingPlacement(const float InputValue);
	void RotateBeltPlacement(const float InputValue);
	void GetBeltRotationSearchOrder(const EBuildingRotation StartRotation, const bool bRotateRight, TArray<EBuildingRotation>& OutRotations) const;
	void GetConnectedBeltRoutePortsForCandidate(const int32 GridIndex, const EAtomicBeltShape BeltShape, const EBuildingRotation CandidateRotation, TArray<EGridDirection>& OutConnectedPorts) const;
	bool TryGetOtherRoutePort(const TArray<EGridDirection>& RoutePorts, const EGridDirection KnownPort, EGridDirection& OutOtherPort) const;
	EGridDirection ChooseOutputDirectionForBeltCandidate(const EAtomicBeltShape BeltShape, const EBuildingRotation OldRotation, const EGridDirection OldOutputDirection, const EBuildingRotation CandidateRotation, const TArray<EGridDirection>& ConnectedPorts) const;
	void EnsureOutputDirectionIsValidForCurrentBelt();
	void FlipBeltFlowDirection();
	
	void UpdatePlacementDistanceFromStep();

	void StartPlacement();
	void ClearSelectedPlacement();
	
	void DrawDebugBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	void DrawDebugBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	
	UPROPERTY(Transient)
	FAtomicPlacementSelection CurrentSelection;
	
	UPROPERTY()
	TObjectPtr<AAtomicPlacementGhostActor> GhostActor;

	bool bIsPlacementActive = false;
	bool bHasValidPlacement = false;
	
	bool bPlacementRequestPending = false;
	
	float PlacementStepRepeatTimer = 0.0f;
	int32 LastPlacementStepDirection = 0;
	float LastPlacementRotationTime = -1000.0f;
	
	UPROPERTY(VisibleAnywhere, Category=Components)
	TObjectPtr<AAtomicPlayerController> LocalController;
	
	UPROPERTY()
	TObjectPtr<UAtomicBuildingRegistrySubsystem> PlacementRegistry;
	
	UPROPERTY()
	FAtomicPlacementTarget CurrentTarget;

	
public:
	void AdjustPlacementDistance(const float InputValue, const float DeltaTime);
};
