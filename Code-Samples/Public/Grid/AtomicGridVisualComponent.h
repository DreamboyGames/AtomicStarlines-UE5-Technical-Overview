// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AtomicGridVisualComponent.generated.h"


struct FAtomicResolvedPreviewBelt;
struct FAtomicResolvedBeltVisual;
class UAtomicBeltDefinition;
enum class EBuildingRotation : uint8;
enum class EAtomicBeltVisualVariant : uint8;
struct FAtomicBeltRecord;
struct FAtomicBuildingRecord;
class UHierarchicalInstancedStaticMeshComponent;
class UAtomicBuildingRegistrySubsystem;
class UAtomicGridDataComponent;
class AAtomicShipGrid;
class AAtomicBuildingActor;


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRID VISUAL COMPONENT
// Render Placed Belts using HISM: Hierarchical Instanced Static Mesh
/////////////////////////////////////////////////////////////////////////////////////////////////////////
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicGridVisualComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicGridVisualComponent();
	
	void Initialize();

	// ---------------------------------------------------------------------
	// Buildings
	// ---------------------------------------------------------------------
	void SpawnOrUpdateBuildingFromRecord(const FAtomicBuildingRecord& BuildRecord);
	void DestroyLocalBuilding(const FGuid& BuildInstanceID);

	// ---------------------------------------------------------------------
	// Belts
	// ---------------------------------------------------------------------
	void RequestBeltVisualRebuild();
	void NotifyVisualsToRebuildBelts();
	void NotifyVisualsToRebuildBeltsAtIndex(const int32 BeltIndex);
	
	bool ResolveBeltVisualForRecord(const FAtomicBeltRecord& BeltRecord, FAtomicResolvedBeltVisual& OutResolvedVisual) const;
	bool ResolveBeltVisualForPreviewRecord(const FAtomicBeltRecord& PreviewBeltRecord, FAtomicResolvedPreviewBelt& OutResolvedPreview) const;

protected:

private:
	void ResolveCachedReferences();
	
	AAtomicBuildingActor* SpawnBuildingFromRecord(const FAtomicBuildingRecord& BuildRecord);

	UPROPERTY(Transient)
	TObjectPtr<AAtomicShipGrid> ShipGrid;

	UPROPERTY(Transient)
	TObjectPtr<UAtomicGridDataComponent> GridData;
	
	UPROPERTY(Transient)
	TObjectPtr<UAtomicBuildingRegistrySubsystem> BuildingRegistry;
	
	UPROPERTY(Transient)
	TMap<FGuid, TObjectPtr<AAtomicBuildingActor>> LocalBuildingActorsByID;
	
	UPROPERTY(Transient)
	TMap<EAtomicBeltVisualVariant, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> BeltInstanceComponents;
	
	void RebuildAllBeltVisuals();
	void ClearBeltInstances();
	
	UHierarchicalInstancedStaticMeshComponent* GetOrCreateBeltInstancedStaticMeshComponent(const EAtomicBeltVisualVariant VisualVariant, UStaticMesh* StaticMesh);
	//bool ResolvedBeltVisualVariant(const FAtomicBeltRecord& BeltRecord, EAtomicBeltVisualVariant& OutVisualVariant, EBuildingRotation& OutVisualRotation) const;
	UStaticMesh* GetBeltMeshForVariant(const UAtomicBeltDefinition* BeltDefinition, EAtomicBeltVisualVariant VisualVariant) const;
	FTransform MakeBeltWorldTransform(const FAtomicBeltRecord& BeltRecord, EBuildingRotation VisualRotation) const;
};
