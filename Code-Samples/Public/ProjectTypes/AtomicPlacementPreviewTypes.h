// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtomicGridTypes.h"
#include "AtomicPlacementPreviewTypes.generated.h"

struct FAtomicBeltPlacementCell;
class AAtomicShipGrid;
class UAtomicBuildingDefinition;
class UAtomicBeltDefinition;
enum class EBuildingRotation : uint8;

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

UENUM()
enum class EAtomicPlacementPreviewType : uint8 {
	None,
	SingleGhost,
	BeltLine
};

struct FAtomicPreviewMeshInstance {
	UStaticMesh* Mesh = nullptr;
	UMaterialInterface* Material = nullptr;
	FTransform WorldTransform = FTransform::Identity;
	bool bCanPlace = false;
};

// What should the ghost look like right now?
struct FAtomicPlacementPreviewResult {
	
	EAtomicPlacementPreviewType PreviewType = EAtomicPlacementPreviewType::None;
	
	// Building / Single Object Preview
	FAtomicPreviewMeshInstance MeshInstance;
	TArray<FIntVector> FootprintPreviewCells;
	
	// Belt Line Preview
	TArray<FAtomicPreviewMeshInstance> MeshInstances;
	TArray<FAtomicBeltPlacementCell> BeltCells;
	
	bool bShowGhost = false;
	bool bCanPlaceLine = false;
	
	bool CanPlace() const
	{
		// Belt Line
		if (PreviewType == EAtomicPlacementPreviewType::BeltLine)
		{
			if (MeshInstances.IsEmpty()) return false;
			for (const FAtomicPreviewMeshInstance& BeltMesh : MeshInstances)
			{
				if (!BeltMesh.bCanPlace) return false;
			}
			return true;
			// return bCanPlaceLine;
		} 
		
		// Single Ghost
		if (PreviewType == EAtomicPlacementPreviewType::SingleGhost)
		{
			return MeshInstance.bCanPlace;
		}
		
		return false;
	}

	bool IsValid() const
	{
		return bShowGhost;
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
	EBuildingRotation Rotation = EBuildingRotation::East;
	
	bool IsValid() const;
	
	void ResetPlacementSelection()
	{
		PlacementMode = EPlacementMode::None;
		DefinitionID = NAME_None;
		BuildingDefinition = nullptr;
		BeltDefinition = nullptr;
		Rotation = EBuildingRotation::East;
	}
};