// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AtomicGridVisualComponent.generated.h"


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

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicGridVisualComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicGridVisualComponent();
	
	void Initialize();

	void SpawnOrUpdateBuildingFromRecord(const FAtomicBuildingRecord& BuildRecord);
	void DestroyLocalBuilding(const FGuid& BuildInstanceID);
	
	void NotifyVisualsToRebuildBelts();
	void NotifyVisualsToRebuildBeltsAtIndex(const int32 BeltIndex);

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
