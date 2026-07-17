// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "ProjectTypes/AtomicBeltTypes.h"
#include "Components/ActorComponent.h"
#include "ProjectTypes/AtomicPlacementPreviewTypes.h"
#include "AtomicPlayerPlacementComponent.generated.h"

class AAtomicBeltLineGhostActor;
class UHierarchicalInstancedStaticMeshComponent;
class UAtomicBuildingRegistrySubsystem;
enum class EAtomicBeltRouteType : uint8;
class UAtomicBeltDefinition;
class AAtomicPlacementGhostActor;
class UAtomicBuildingDefinition;
class AAtomicShipGrid;
class AAtomicPlayerController;

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
	bool StartPlacementWithSelectedBelt(const FName BeltID);

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

	void HideAllPlacementPreviews();

	// Start/Clear Placement
	void StartPlacement();
	void ClearSelectedPlacement();

	// Build Preview
	bool BuildPlacementPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult);
	bool BuildBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult);

	bool BuildWallPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult);

	void ApplySingleGhostPreview(const FAtomicPlacementPreviewResult& PreviewResult);
	void EnsureSingleGhostActor();
	void HideSingleGhostPreview() const;
	
	// Rotation
	void RotateBuildingPlacement(const float InputValue);
	void UpdatePlacementDistanceFromStep();

	// Belt Helpers
	void StartBeltLinePlacement(const FAtomicPlacementTarget& StartTarget);
	void RotateBeltPlacement(const float InputValue);

	bool BuildBeltLinePreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& OutResult);
	bool BuildBeltLinePath(const FIntVector& StartCoord, const FIntVector& EndCoord, const bool bXFirst, TArray<FAtomicBeltPlacementCell>& OutCells);
	void BuildStraightSegment(const FIntVector& StartCoord, const FIntVector& EndCoord, TArray<FIntVector>& OutPath, const bool bSkipFirstCoord);

	void ApplyBeltLinePreview(const FAtomicPlacementPreviewResult& PreviewResult);
	void EnsureBeltLineGhostActor();

	void HideBeltLinePreview() const;
	
	void CancelBeltLinePlacement();
	void ToggleBeltLineBendOrder();
	
	bool bIsDraggingBeltLine = false;
	bool bBeltLinePreferXFirst = false;
	FAtomicPlacementTarget BeltLineStartTarget;
	TArray<FAtomicBeltPlacementCell> CurrentBeltLinePreview;


	// Draw Debugs
	void DrawDebugPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	void DrawDebugBuildingPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;
	void DrawDebugBeltPreview(const FAtomicPlacementTarget& Target, FAtomicPlacementPreviewResult& PreviewResult) const;

	
	UPROPERTY(Transient)
	FAtomicPlacementSelection CurrentSelection;

	UPROPERTY()
	TArray<FAtomicBeltPlacementCell> CurrentBeltLinePreviewCells;
	
	UPROPERTY(Transient)
	TObjectPtr<AAtomicPlacementGhostActor> SingleGhostActor;
	
	UPROPERTY(Transient)
	TObjectPtr<AAtomicBeltLineGhostActor> BeltLineGhostActor;

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
