// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.


#include "Building/AtomicPlacementGhostActor.h"

#include "AtomicStarlines.h"


AAtomicPlacementGhostActor::AAtomicPlacementGhostActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	RootComponent = PreviewMesh;
	
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetCastShadow(false);
	PreviewMesh->SetForceDisableNanite(true);
}

void AAtomicPlacementGhostActor::SetPreviewMesh(UStaticMesh* Mesh, UMaterialInterface* Material)
{
	if (!PreviewMesh || !Mesh || !Material) return;
	
	PreviewMesh->SetStaticMesh(Mesh);
	
	// Keep visual mesh authored scale.
	PreviewMesh->SetRelativeScale3D(FVector::OneVector);
	// Optional: keep it centered if the mesh pivot/bounds are awkward.
	PreviewMesh->SetRelativeLocation(FVector::ZeroVector);
	
	const int32 MaterialSlotCount = PreviewMesh->GetNumMaterials();
	
	for (int32 SlotIndex = 0; SlotIndex < MaterialSlotCount; ++SlotIndex)
	{
		PreviewMaterial = PreviewMesh->CreateDynamicMaterialInstance(SlotIndex, Material);
	}
	
	bMeshSet = true;
}

void AAtomicPlacementGhostActor::SetPreviewMeshToNull()
{
	if (!PreviewMesh) return;
	PreviewMesh->SetStaticMesh(nullptr);
	bMeshSet = false;
}

void AAtomicPlacementGhostActor::SetPreviewMeshVisible(const bool Visible) const
{
	PreviewMesh->SetVisibility(Visible);
}

void AAtomicPlacementGhostActor::SetPlacementValid(const bool bIsValid) const
{
	if (!PreviewMaterial) return;
	PreviewMaterial->SetScalarParameterValue(TEXT("IsValid"), bIsValid ? 1.f : 0.f);
}

bool AAtomicPlacementGhostActor::IsPreviewMeshDifferent(UStaticMesh* NewMesh) const
{
	return PreviewMesh && PreviewMesh->GetStaticMesh() != NewMesh;
}


