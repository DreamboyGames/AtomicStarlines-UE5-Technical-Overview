// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtomicPlacementGhostActor.generated.h"

UCLASS()
class ATOMICSTARLINES_API AAtomicPlacementGhostActor : public AActor {
	GENERATED_BODY()

public:
	AAtomicPlacementGhostActor();

	void SetPreviewMesh(UStaticMesh* Mesh, UMaterialInterface* Material);
	void SetPreviewMeshToNull();
	void SetPreviewMeshVisible(const bool Visible) const;
	void SetPlacementValid(bool bIsValid) const;
	
	bool IsMeshSet() const { return bMeshSet; }
	bool IsPreviewMeshDifferent(UStaticMesh* NewMesh) const;
	
protected:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> PreviewMesh;
	
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> PreviewMaterial;

private:
	bool bMeshSet = false;
};
