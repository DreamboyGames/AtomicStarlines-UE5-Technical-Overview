// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AtomicBuildingDefinition.generated.h"

struct FAtomicBuildingPortDefinition;
class UMaterialInstanceDynamic;
class UStaticMesh;
class AAtomicBuildingActor;


/**
 * Building Definition stores invariant config.
 * And authored defaults.
 */
UCLASS()
class ATOMICSTARLINES_API UAtomicBuildingDefinition : public UPrimaryDataAsset {
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	FName BuildingID;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	FText DisplayName;
	
	// TSubclassOf<> is a hard reference. Loading the Building Definition would load all referenced building blueprints and their dependencies 
	// Use TSoftClassPtr instead. You must load the class before spawning or reading its CDO
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TSoftClassPtr<AAtomicBuildingActor> BuildingActorClass;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TSoftObjectPtr<UMaterialInterface> PreviewMaterial;
	
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	//TSoftClassPtr<AAtomicPlacementGhostActor> PreviewActorClass;

	// FootprintSize.X = Forward length in default East/+X direction
	// FootprintSize.Y = side width
	// @todo: Add editor validation that warns: "Linear belt-like building has Y > X. Did you mean X-forward?"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building", meta=(ClampMax=3)) 
	FIntPoint FootprintSize = FIntPoint(1, 1);
	
	// Start/End Anchor: PivotCell is the cell inside the unrotated footprint that sits on the player's selected grid cell
	// 1x1 & 2x1 = PivotCell(0,0). 3x1 = PivotCell(1,0) [X][P][X]. 
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building", meta=(ClampMax=3))
	FIntPoint PivotCell = FIntPoint(0, 0);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	bool bCanRotate = true;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TMap<FName, int32> BuildCost;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TArray<FAtomicBuildingPortDefinition> PortDefinitions;
};
