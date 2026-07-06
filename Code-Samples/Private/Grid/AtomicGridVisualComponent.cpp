// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Grid/AtomicGridVisualComponent.h"

#include "Belts/AtomicBeltDefinition.h"
#include "Belts/AtomicBeltRecords.h"
#include "Belts/AtomicBeltTypes.h"
#include "Building/AtomicBuildingRegistrySubsystem.h"
#include "Grid/AtomicGridDataComponent.h"
#include "Grid/AtomicShipGrid.h"
#include "Building/AtomicBuildingActor.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Grid/AtomicGridLibrary.h"


UAtomicGridVisualComponent::UAtomicGridVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAtomicGridVisualComponent::Initialize()
{
	ResolveCachedReferences();
}

void UAtomicGridVisualComponent::ResolveCachedReferences()
{
	if (!ShipGrid)
	{
		ShipGrid = Cast<AAtomicShipGrid>(GetOwner());
	}
	
	if (!GridData && GetOwner())
	{
		GridData = GetOwner()->GetComponentByClass<UAtomicGridDataComponent>();
	}
	
	if (!BuildingRegistry)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GameInstance = World->GetGameInstance())
			{
				BuildingRegistry = GameInstance->GetSubsystem<UAtomicBuildingRegistrySubsystem>();
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BUILDINGS
/////////////////////////////////////////////////////////////////////////////////////////////////////////

void UAtomicGridVisualComponent::SpawnOrUpdateBuildingFromRecord(const FAtomicBuildingRecord& BuildRecord)
{
	const TObjectPtr<AAtomicBuildingActor>* ExistingActorPtr = LocalBuildingActorsByID.Find(BuildRecord.BuildInstanceID);
	
	if (ExistingActorPtr && IsValid(ExistingActorPtr->Get()))
	{
		// @todo: if records can change position/rotation/type, update transform here.
		// For now static placed buildings do not need updates.
		return;
	}
	
	AAtomicBuildingActor* NewActor = SpawnBuildingFromRecord(BuildRecord);
	if (!NewActor) return;
	
	LocalBuildingActorsByID.Add(BuildRecord.BuildInstanceID, NewActor);
}

AAtomicBuildingActor* UAtomicGridVisualComponent::SpawnBuildingFromRecord(const FAtomicBuildingRecord& BuildRecord)
{
	// Runs on Server after Server_RequestPlaceBuilding() -> AddBuildingRecord() -> HandleBuildingRecordAdded() 
	// Runs on Clients after FAtomicBuildingRecordArray::PostReplicatedAdd -> HandleBuildingRecordAdded() 
	
	ResolveCachedReferences();
	if (!ShipGrid || !GridData || !BuildingRegistry) return nullptr;
	
	const UAtomicBuildingDefinition* Definition = BuildingRegistry->FindBuildingDefinition(BuildRecord.BuildingID);
	if (!Definition) return nullptr;
	
	if (BuildRecord.CellIndex == INDEX_NONE) return nullptr;
	
	const FIntVector AnchorCoord = UAtomicGridLibrary::IndexToGridUnchecked(BuildRecord.CellIndex, GridData->GetGridSize());
	
	TArray<FIntVector> FootprintCells;
	UAtomicGridLibrary::GetFootprintCellsAroundPivot(AnchorCoord, Definition->FootprintSize, Definition->PivotCell, BuildRecord.Rotation, FootprintCells);
	
	const FVector SpawnLocation = UAtomicGridLibrary::GetFootprintCenterWorldLocationFromCells(FootprintCells, ShipGrid->GetTransform(), GridData->GetCellSize());
	const float SpawnYaw = UAtomicGridLibrary::BuildingRotationToYawDegrees(BuildRecord.Rotation);
	const FTransform SpawnTransform(FRotator(0.f, SpawnYaw, 0.f), SpawnLocation);
	
	UClass* BuildingClass = Definition->BuildingActorClass.LoadSynchronous();
	if (!BuildingClass) return nullptr;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = ShipGrid;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	AAtomicBuildingActor* BuildingActor = GetWorld()->SpawnActor<AAtomicBuildingActor>(BuildingClass, SpawnTransform, SpawnParams);
	if (!BuildingActor) return nullptr;
	
	BuildingActor->InitializePlacedBuilding(BuildRecord.BuildInstanceID, BuildRecord.BuildingID, AnchorCoord, BuildRecord.Rotation, ShipGrid);
	BuildingActor->ConfigureFootprintCollision(Definition->FootprintSize, GridData->GetCellSize());
	
	return BuildingActor;
}

void UAtomicGridVisualComponent::DestroyLocalBuilding(const FGuid& BuildInstanceID)
{
	const TObjectPtr<AAtomicBuildingActor>* ActorPtr = LocalBuildingActorsByID.Find(BuildInstanceID);
	
	if (ActorPtr && IsValid(ActorPtr->Get()))
	{
		ActorPtr->Get()->Destroy();
	}
	
	LocalBuildingActorsByID.Remove(BuildInstanceID);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// BELTS
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void UAtomicGridVisualComponent::NotifyVisualsToRebuildBelts()
{
	if (!ShipGrid) return;
	RebuildAllBeltVisuals();
}

void UAtomicGridVisualComponent::NotifyVisualsToRebuildBeltsAtIndex(const int32 BeltIndex)
{
	
}

UHierarchicalInstancedStaticMeshComponent* UAtomicGridVisualComponent::GetOrCreateBeltInstancedStaticMeshComponent(const EAtomicBeltVisualVariant VisualVariant, UStaticMesh* StaticMesh)
{
	if (!StaticMesh) return nullptr;
	
	// One Component per Visual Variant for all instances of that variant.
	if (TObjectPtr<UHierarchicalInstancedStaticMeshComponent>* ExistingComponent = BeltInstanceComponents.Find(VisualVariant))
	{
		if (*ExistingComponent)
		{
			return *ExistingComponent;
		}
	}
	
	if (!ShipGrid) return nullptr;

	const FName ComponentName = *FString::Printf(TEXT("BeltHISM_%s"), *UEnum::GetValueAsString(VisualVariant));
	UHierarchicalInstancedStaticMeshComponent* NewComponent = NewObject<UHierarchicalInstancedStaticMeshComponent>(ShipGrid, ComponentName);
	if (!NewComponent) return nullptr;
	
	NewComponent->SetStaticMesh(StaticMesh);
	NewComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewComponent->SetMobility(EComponentMobility::Movable); // runtime-mutable, the HISM component changes during gameplay
	NewComponent->AttachToComponent(ShipGrid->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	
	ShipGrid->AddInstanceComponent(NewComponent);
	NewComponent->RegisterComponent();
	
	BeltInstanceComponents.Add(VisualVariant, NewComponent);
	
	return NewComponent;
}

void UAtomicGridVisualComponent::ClearBeltInstances()
{
	for (const TPair<EAtomicBeltVisualVariant, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>>& Pair : BeltInstanceComponents)
	{
		if (Pair.Value)
		{
			Pair.Value->ClearInstances();
		}
	}
}

void UAtomicGridVisualComponent::RebuildAllBeltVisuals()
{
	if (!GridData || !ShipGrid) return;
	
	ClearBeltInstances();
	
	const TArray<FAtomicBeltRecord>& BeltRecords = GridData->GetPlacedBeltRecords();
	
	for (const FAtomicBeltRecord& Record : BeltRecords)
	{
		const UAtomicBeltDefinition* BeltDefinition = BuildingRegistry->FindBeltDefinition(Record.BeltID);
		if (!BeltDefinition) continue;
		
		FAtomicResolvedBeltVisual ResolvedVisual;
		if (!ShipGrid->ResolveBeltVisual(Record, ResolvedVisual))
		{
			continue;
		}
		
		UStaticMesh* Mesh = BeltDefinition->GetMeshForVariant(ResolvedVisual.VisualVariant);
		if (!Mesh) continue;
		
		UHierarchicalInstancedStaticMeshComponent* InstanceComponent = GetOrCreateBeltInstancedStaticMeshComponent(ResolvedVisual.VisualVariant, Mesh);
		if (!InstanceComponent) continue;
		
		const FTransform WorldTransform = MakeBeltWorldTransform(Record, ResolvedVisual.VisualRotation);
		InstanceComponent->AddInstance(WorldTransform, true);
	}
}

FTransform UAtomicGridVisualComponent::MakeBeltWorldTransform(const FAtomicBeltRecord& BeltRecord, const EBuildingRotation VisualRotation) const
{
	const FIntVector Coord = UAtomicGridLibrary::IndexToGridUnchecked(BeltRecord.CellIndex, GridData->GetGridSize());
	const FVector WorldLocation = UAtomicGridLibrary::GridToWorld(Coord, ShipGrid->GetTransform(), GridData->GetCellSize());
	const float Yaw = UAtomicGridLibrary::BuildingRotationToYawDegrees(VisualRotation);
	return FTransform(FRotator(0.f, Yaw, 0.f), WorldLocation, FVector::OneVector);
}

