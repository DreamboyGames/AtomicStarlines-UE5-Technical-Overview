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
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	FName BeltID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	int32 Tier;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	float BeltSpeed;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TMap<FName, int32> BuildCost;

	
	// TObjectPtr = better runtime simplicity/performance
	// TSoftObjectPtr = better memory/loading scalability
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> StraightMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> StraightEndMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> StraightDoubleEndMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> CornerMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> CornerEndMeshA;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> CornerEndMeshB;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UStaticMesh> CornerDoubleEndMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UMaterialInterface> BeltMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UMaterialInterface> BeltBaseMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TObjectPtr<UMaterialInterface> PreviewMaterial;
};
