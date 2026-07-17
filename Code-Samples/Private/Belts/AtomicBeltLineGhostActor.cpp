// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Belts/AtomicBeltLineGhostActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "ProjectTypes/AtomicPlacementPreviewTypes.h"


AAtomicBeltLineGhostActor::AAtomicBeltLineGhostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	GhostRoot = CreateDefaultSubobject<USceneComponent>(TEXT("GhostRoot"));
	SetRootComponent(GhostRoot);
}

void AAtomicBeltLineGhostActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyPreviewComponents();
	Super::EndPlay(EndPlayReason);
}

void AAtomicBeltLineGhostActor::SetPreviewVisible(const bool bVisible)
{
	SetActorHiddenInGame(!bVisible);
	
	for (UInstancedStaticMeshComponent* PreviewISM : PreviewISMs)
	{
		if (PreviewISM)
		{
			PreviewISM->SetVisibility(bVisible, true);
			PreviewISM->SetHiddenInGame(!bVisible, true);
		}
	}
}

void AAtomicBeltLineGhostActor::ApplyPreviewInstances(const TArray<FAtomicPreviewMeshInstance>& MeshInstances, const bool bIsValid)
{	
	SetPreviewVisible(true);
	ClearPreviewInstances();
	
	for (const FAtomicPreviewMeshInstance& PreviewInstance : MeshInstances)
	{
		if (!PreviewInstance.Mesh) continue;
		
		UInstancedStaticMeshComponent* PreviewISM = GetOrCreateISM(PreviewInstance.Mesh, PreviewInstance.Material);
		if (!PreviewISM) continue;
		
		PreviewISM->SetVisibility(true,true);
		PreviewISM->SetHiddenInGame(false, true);
		
		PreviewISM->AddInstance(PreviewInstance.WorldTransform, true);
		
		UE_LOG(LogTemp, Warning,
			TEXT("PreviewMaterial=%s"),
			*GetNameSafe(PreviewInstance.Material)
		);
	}
	
	for (UInstancedStaticMeshComponent* PreviewISM : PreviewISMs)
	{
		if (PreviewISM)
		{
			PreviewISM->MarkRenderStateDirty();
		}
	}
	
	SetPlacementValid(bIsValid);
}

void AAtomicBeltLineGhostActor::ClearPreviewInstances()
{
	for (UInstancedStaticMeshComponent* PreviewISM : PreviewISMs)
	{
		if (PreviewISM)
		{
			PreviewISM->ClearInstances();
		}
	}
}

void AAtomicBeltLineGhostActor::DestroyPreviewComponents()
{
	for (UInstancedStaticMeshComponent* PreviewISM : PreviewISMs)
	{
		if (PreviewISM)
		{
			PreviewISM->ClearInstances();
			PreviewISM->DestroyComponent();
		}
	}
	
	PreviewISMs.Reset();
}

UInstancedStaticMeshComponent* AAtomicBeltLineGhostActor::GetOrCreateISM(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!Mesh || !Material) return nullptr;
	
	for (UInstancedStaticMeshComponent* ExistingISM : PreviewISMs)
	{
		if (!ExistingISM) continue;
		
		if (ExistingISM->GetStaticMesh() == Mesh)
		{
			ApplyPreviewMaterialToISM(ExistingISM, Mesh, Material);
			return ExistingISM;
		}
	}
	
	UInstancedStaticMeshComponent* NewISM = NewObject<UInstancedStaticMeshComponent>(this);
	if (!NewISM) return nullptr;
	
	NewISM->CreationMethod = EComponentCreationMethod::Instance;
	NewISM->SetStaticMesh(Mesh);	
	NewISM->SetMobility(EComponentMobility::Movable);
	NewISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NewISM->SetGenerateOverlapEvents(false);
	NewISM->SetCastShadow(false);
	NewISM->SetVisibility(true, true);
	NewISM->SetHiddenInGame(false, true);
	NewISM->SetupAttachment(GhostRoot);
	NewISM->RegisterComponent();
	
	AddInstanceComponent(NewISM);
	PreviewISMs.Add(NewISM);
	
	ApplyPreviewMaterialToISM(NewISM, Mesh, Material);
	
	return NewISM;	
}

void AAtomicBeltLineGhostActor::ApplyPreviewMaterialToISM(UInstancedStaticMeshComponent* ISM, UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!ISM || !Mesh || !Material) return;
	
	const int32 MaterialSlotCount = Mesh->GetStaticMaterials().Num();
	
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
	{
		UMaterialInterface* CurrentMaterial = ISM->GetMaterial(MaterialIndex);
		
		// If it is already a MID using this preview material, keep it.
		if (UMaterialInstanceDynamic* ExistingMid = Cast<UMaterialInstanceDynamic>(CurrentMaterial))
		{
			continue;
		}
		
		ISM->CreateDynamicMaterialInstance(MaterialIndex, Material);
	}
	
	ISM->MarkRenderStateDirty();
}

void AAtomicBeltLineGhostActor::SetPlacementValid(const bool bIsValid) const
{
	for (UInstancedStaticMeshComponent* PreviewISM : PreviewISMs)
	{
		if (!PreviewISM) continue;
		
		const int32 MaterialSlotCount = PreviewISM->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialSlotCount; ++MaterialIndex)
		{
			UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(PreviewISM->GetMaterial(MaterialIndex));
			if (!DynamicMaterial) continue;
			
			DynamicMaterial->SetScalarParameterValue(TEXT("IsValid"), bIsValid ? 1.f : 0.f);
		}
		
		PreviewISM->MarkRenderStateDirty();
	}
}
