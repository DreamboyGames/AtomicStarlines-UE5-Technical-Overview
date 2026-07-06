// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interface/AtomicInteractionInterface.h"
#include "AtomicBuildingActor.generated.h"


// AtomicBuildingDefinition DataAsset
// = source of truth for footprint, pivot, ports, actor class
//
// AtomicBuildingActor Blueprint
// = mesh + visuals + editor-only generated port arrows
//
// Generated UArrowComponents
// = viewport visualization only, not gameplay data

enum class EGridDirection : uint8;
enum class EBuildingRotation : uint8;
class AAtomicShipGrid;
class UBoxComponent;
class UTextRenderComponent;
class UArrowComponent;
class UAtomicBuildingDefinition;


UCLASS()
class ATOMICSTARLINES_API AAtomicBuildingActor : public AActor, public IAtomicInteractionInterface {
	GENERATED_BODY()

public:
	// Non-replicated local representation
	// Mesh/collision/interaction target
	// Sends interaction requests by BuildInstanceID
	AAtomicBuildingActor();
	
	virtual void OnConstruction(const FTransform& Transform) override;
	
	void InitializePlacedBuilding(const FGuid& InBuildingInstanceID, const FName InBuildingID, const FIntVector InAnchorCoord, const EBuildingRotation InRotation, AAtomicShipGrid* InOwningGrid);
	void ConfigureFootprintCollision(const FIntPoint FootprintSize, const float CellSize) const;
	
	virtual void Interact() override;

protected:
	UPROPERTY(VisibleAnywhere, Category="Atomic|Components")
	TObjectPtr<USceneComponent> BuildingRoot;
	
	UPROPERTY(EditDefaultsOnly, Category="Atomic|Components")
	TObjectPtr<UStaticMeshComponent> BaseMesh;
	
	UPROPERTY(VisibleAnywhere, Category="Atomic|Components")
	TObjectPtr<UBoxComponent> FootprintCollision;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Definition")
	TSoftObjectPtr<UAtomicBuildingDefinition> BuildingDefinition;

	UPROPERTY()
	TObjectPtr<AAtomicShipGrid> OwningGrid;
	
	FGuid BuildingInstanceID;
	FName BuildingID;
	FIntVector OriginCoord;
	EBuildingRotation Rotation;

	
private:
	
	
	
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//// EDITOR ONLY DEBUG PREVIEW /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category="Atomic|Editor")
	float EditorPreviewCellSize = 200.f;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UArrowComponent>> PortPreviewArrows;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> PortPreviewTexts;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBoxComponent>> FootprintPreviewBoxes;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> FootprintPreviewTexts;
#endif

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Atomic|Editor")
	void RefreshEditorPreview();
	
	UFUNCTION(CallInEditor, Category="Atomic|Editor")
	void CenterMeshOnFootprint() const;
#endif

private:
#if WITH_EDITOR
	const FName PortPreviewTag = TEXT("AtomicPortPreview");
	const FName PortTextTag = TEXT("AtomicPortText");
	const FName CellPreviewTag = TEXT("AtomicCellPreview");
	const FName CellTextTag = TEXT("AtomicCellText");
	
	void RebuildEditorPreview();
	void ClearEditorPreview();

	void RebuildFootprintPreview();
	void RebuildPortPreview();
	
	FVector GetEditorLocalFootprintCellCenter(FIntPoint Cell, FIntPoint FootprintSize) const;
	FVector GetEditorLocalCellCoordLabelLocation(const FIntPoint Cell, const FIntPoint FootprintSize) const;
	FVector GetEditorPortEdgeLocation(const FIntPoint LocalCellOffset, const EGridDirection LocalGridDirection, const FIntPoint FootprintSize) const;
	FVector GetEditorCellEdgeLabelLocation(const FIntPoint Cell, const FIntPoint FootprintSize, const EGridDirection EdgeGridDirection) const;
	FRotator GetEditorEdgeLabelRotation(const EGridDirection EdgeGridDirection) const;
#endif
};
