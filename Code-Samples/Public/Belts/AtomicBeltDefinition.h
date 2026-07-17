// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AtomicBeltDefinition.generated.h"


enum class EAtomicBeltVisualVariant : uint8;
/**
 * Belt Definition stores invariant config.
 * And authored defaults.
 */
UCLASS()
class ATOMICSTARLINES_API UAtomicBeltDefinition : public UPrimaryDataAsset {
	GENERATED_BODY()
	
public:
	UStaticMesh* GetMeshForVariant(const EAtomicBeltVisualVariant VisualVariant) const;
	
	// Build Menu Icon
	// Mesh Variant set
	// Sound
	// Item Capacity
	// Visual Theme
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	FName BeltID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	int32 Tier;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	float BeltSpeed;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TMap<FName, int32> BuildCost;

	
	// TObjectPtr = better runtime simplicity/performance
	// TSoftObjectPtr = better memory/loading scalability
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UStaticMesh> StraightMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UStaticMesh> TurnLeftMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UStaticMesh> TurnRightMesh;
	
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UMaterialInterface> BeltMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UMaterialInterface> BeltBaseMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Belt")
	TObjectPtr<UMaterialInterface> PreviewMaterial;
};
