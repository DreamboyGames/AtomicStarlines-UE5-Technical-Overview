// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtomicBeltLineGhostActor.generated.h"

// UAtomicPlayerPlacementComponent:
//		- Owns placement state + input flow
//		- Builds preview data
//		- Tells preview actors what to display

// AAtomicPlacementGhostActor:
//		- Owns one Static Mesh Component
//		- Used for building preview / single belt start preview

// AAtomicBeltLineGhostActor
//		- Owns HISM/ISM components
//		- Used only for drag belt-line preview

struct FAtomicPreviewMeshInstance;
class UInstancedStaticMeshComponent;

UCLASS()
class ATOMICSTARLINES_API AAtomicBeltLineGhostActor : public AActor {
	GENERATED_BODY()

public:
	AAtomicBeltLineGhostActor();
	
	void ApplyPreviewInstances(const TArray<FAtomicPreviewMeshInstance>& MeshInstances, const bool bIsValid);
	void SetPreviewVisible(bool bVisible);
	void SetPlacementValid(bool bIsValid) const;
	
	void ClearPreviewInstances();
	void DestroyPreviewComponents();
	
protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
private:
	UInstancedStaticMeshComponent* GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material);
	void ApplyPreviewMaterialToISM(UInstancedStaticMeshComponent* ISM, UStaticMesh* Mesh, UMaterialInterface* Material);
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> GhostRoot;
	
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> PreviewISMs;
};
