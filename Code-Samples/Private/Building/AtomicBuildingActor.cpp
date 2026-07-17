// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#include "Building/AtomicBuildingActor.h"

#include "AtomicStarlines.h"
#include "Building/AtomicBuildingDefinition.h"
#include "ProjectTypes/AtomicBuildingTypes.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Grid/AtomicGridLibrary.h"


AAtomicBuildingActor::AAtomicBuildingActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Building actors are local visual/gameplay representations reconstructed from replicated build records.
	// The authoritative state is the grid/build record, not the actor instance.
	bReplicates = false;

	BuildingRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BuildingRoot"));
	RootComponent = BuildingRoot;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMeshComponent"));
	BaseMesh->SetupAttachment(BuildingRoot);
	BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	FootprintCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("FootprintCollision"));
	FootprintCollision->SetupAttachment(BuildingRoot);
	FootprintCollision->SetCollisionProfileName(TEXT("Building"));
	FootprintCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
}

void AAtomicBuildingActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	RebuildEditorPreview();
#endif
}

void AAtomicBuildingActor::InitializePlacedBuilding(const FGuid& InBuildingInstanceID, const FName InBuildingID, const FIntVector InAnchorCoord, const EBuildingRotation InRotation, AAtomicShipGrid* InOwningGrid)
{
	BuildingInstanceID = InBuildingInstanceID;
	BuildingID = InBuildingID;
	OriginCoord = InAnchorCoord;
	Rotation = InRotation;
	OwningGrid = InOwningGrid;
}

void AAtomicBuildingActor::ConfigureFootprintCollision(const FIntPoint FootprintSize, const float CellSize) const
{
	if (!FootprintCollision)
	{
		return;
	}

	FootprintCollision->SetBoxExtent(
		FVector(
			FootprintSize.X * CellSize * 0.5f,
			FootprintSize.Y * CellSize * 0.5f,
			250.f
		),
		true
	);
}

void AAtomicBuildingActor::Interact()
{
	UE_LOGFMT(
		LogGame,
		Warning,
		"Player interacted with: {ObjectName}",
		this->GetName()
	);
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EDITOR DEBUG PREVIEW
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR

void AAtomicBuildingActor::RefreshEditorPreview()
{
	RebuildEditorPreview();
}

void AAtomicBuildingActor::CenterMeshOnFootprint() const
{
	if (!BaseMesh)
	{
		return;
	}

	UStaticMesh* StaticMesh = BaseMesh->GetStaticMesh();
	if (!StaticMesh)
	{
		return;
	}

	const FBox LocalBounds = StaticMesh->GetBoundingBox();
	const FVector LocalCenter = LocalBounds.GetCenter();
	const FRotator MeshRotation = BaseMesh->GetRelativeRotation();

	const FVector RotatedCenterXY =
		MeshRotation.RotateVector(FVector(LocalCenter.X, LocalCenter.Y, 0.f));

	const float ZOffset = -LocalBounds.Min.Z;

	// Keeps mesh scale and rotation, and offsets the mesh so its bounds center aligns to actor root.
	BaseMesh->SetRelativeLocation(
		FVector(
			-RotatedCenterXY.X,
			-RotatedCenterXY.Y,
			ZOffset
		)
	);
}

void AAtomicBuildingActor::RebuildEditorPreview()
{
	ClearEditorPreview();
	RebuildFootprintPreview();
	RebuildPortPreview();
}

void AAtomicBuildingActor::ClearEditorPreview()
{
	TArray<UActorComponent*> Components;
	GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		const bool bIsPreviewComponent =
			Component->ComponentHasTag(PortPreviewTag) ||
			Component->ComponentHasTag(CellPreviewTag) ||
			Component->ComponentHasTag(PortTextTag) ||
			Component->ComponentHasTag(CellTextTag);

		if (bIsPreviewComponent)
		{
			Component->DestroyComponent();
		}
	}

	PortPreviewArrows.Reset();
	PortPreviewTexts.Reset();
	FootprintPreviewBoxes.Reset();
	FootprintPreviewTexts.Reset();
}

void AAtomicBuildingActor::RebuildPortPreview()
{
	USceneComponent* Root = GetRootComponent();
	if (!Root || BuildingDefinition.IsNull())
	{
		return;
	}

	UAtomicBuildingDefinition* Definition = BuildingDefinition.LoadSynchronous();
	if (!Definition)
	{
		return;
	}

	const FIntPoint FootprintSize = Definition->FootprintSize;

	for (int32 PortIndex = 0; PortIndex < Definition->PortDefinitions.Num(); ++PortIndex)
	{
		const FAtomicBuildingPortDefinition Port = Definition->PortDefinitions[PortIndex];

		const bool bIsValidCell =
			Port.LocalCellOffset.X >= 0 &&
			Port.LocalCellOffset.Y >= 0 &&
			Port.LocalCellOffset.X < FootprintSize.X &&
			Port.LocalCellOffset.Y < FootprintSize.Y;

		const FVector PortEdgeLocation =
			GetEditorPortEdgeLocation(
				Port.LocalCellOffset,
				Port.LocalGridDirection,
				FootprintSize
			);

		// Input arrows visually point into the building.
		// Output arrows visually point away from the building.
		const EGridDirection VisualArrowDirection =
			Port.PortType == EBuildingPortType::Input
				? UAtomicGridLibrary::OppositeGridDirection(Port.LocalGridDirection)
				: Port.LocalGridDirection;

		const FVector VisualArrowVector =
			UAtomicGridLibrary::GridDirectionToVector(VisualArrowDirection);

		const float ArrowLength = 50.f;

		const FVector ArrowLocation =
			Port.PortType == EBuildingPortType::Input
				? PortEdgeLocation - VisualArrowVector * (ArrowLength + 30.f)
				: PortEdgeLocation;

		const float ArrowYaw =
			UAtomicGridLibrary::GridDirectionToYawDegrees(VisualArrowDirection);

		const FName ArrowName =
			MakeUniqueObjectName(
				this,
				UArrowComponent::StaticClass(),
				*FString::Printf(TEXT("PortPreview_%d"), PortIndex)
			);

		const FColor PortColor =
			!bIsValidCell
				? FColor::Black
				: Port.PortType == EBuildingPortType::Input
					? FColor::Cyan
					: FColor::Orange;

		// Arrow.
		UArrowComponent* Arrow = NewObject<UArrowComponent>(this, ArrowName);
		if (!Arrow)
		{
			continue;
		}

		Arrow->ComponentTags.Add(PortPreviewTag);
		Arrow->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Arrow->SetupAttachment(Root);
		Arrow->ArrowSize = 1.7f;
		Arrow->ArrowLength = ArrowLength;
		Arrow->ArrowColor = PortColor;
		Arrow->SetHiddenInGame(true);
		Arrow->bIsEditorOnly = true;
		Arrow->SetRelativeLocation(ArrowLocation);
		Arrow->SetRelativeRotation(FRotator(0.f, ArrowYaw, 0.f));
		Arrow->RegisterComponent();
		Arrow->MarkRenderStateDirty();

		PortPreviewArrows.Add(Arrow);

		// Port label.
		const float EdgeYaw =
			UAtomicGridLibrary::GridDirectionToYawDegrees(Port.LocalGridDirection);

		const float LabelYaw =
			FRotator::NormalizeAxis(EdgeYaw);

		UTextRenderComponent* PortText = NewObject<UTextRenderComponent>(this);
		if (!PortText)
		{
			continue;
		}

		PortText->ComponentTags.Add(PortTextTag);
		PortText->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		PortText->SetupAttachment(Root);
		PortText->SetHiddenInGame(true);
		PortText->bIsEditorOnly = true;
		PortText->SetHorizontalAlignment(EHTA_Center);
		PortText->SetWorldSize(26.f);
		PortText->SetCastShadow(false);
		PortText->bCastDynamicShadow = false;
		PortText->bCastStaticShadow = false;
		PortText->SetTextRenderColor(PortColor);

		const FString EdgeDirectionLabel =
			UAtomicGridLibrary::GetGridDirectionName(Port.LocalGridDirection);

		const FString PortLabelText =
			FString::Printf(
				TEXT("%s\n%s"),
				*Port.PortID.ToString(),
				*EdgeDirectionLabel
			);

		PortText->SetText(FText::FromString(PortLabelText));

		// Put the label slightly above and outward from the port edge.
		const FVector LabelDirection =
			UAtomicGridLibrary::GridDirectionToVector(Port.LocalGridDirection) * 65.f;

		PortText->SetRelativeLocation(
			PortEdgeLocation + LabelDirection + FVector(0.f, 0.f, 10.f)
		);

		// Text lies flat on the grid plane.
		PortText->SetRelativeRotation(FRotator(90.f, LabelYaw, 0.f));
		PortText->RegisterComponent();

		PortPreviewTexts.Add(PortText);
	}
}

void AAtomicBuildingActor::RebuildFootprintPreview()
{
	USceneComponent* Root = GetRootComponent();
	if (!Root || BuildingDefinition.IsNull())
	{
		return;
	}

	const UAtomicBuildingDefinition* Definition = BuildingDefinition.LoadSynchronous();
	if (!Definition)
	{
		return;
	}

	const FIntPoint FootprintSize = Definition->FootprintSize;

	for (int32 Y = 0; Y < FootprintSize.Y; ++Y)
	{
		for (int32 X = 0; X < FootprintSize.X; ++X)
		{
			const FIntPoint CellCoord(X, Y);

			const FVector CellCenter =
				GetEditorLocalFootprintCellCenter(CellCoord, FootprintSize);

			// Cell footprint preview box.
			UBoxComponent* CellBox = NewObject<UBoxComponent>(this);
			if (!CellBox)
			{
				continue;
			}

			CellBox->ComponentTags.Add(CellPreviewTag);
			CellBox->CreationMethod = EComponentCreationMethod::UserConstructionScript;
			CellBox->SetupAttachment(Root);
			CellBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CellBox->SetCastShadow(false);
			CellBox->SetHiddenInGame(true);
			CellBox->bIsEditorOnly = true;

			const float HalfCell = EditorPreviewCellSize * 0.48f;
			CellBox->SetBoxExtent(FVector(HalfCell, HalfCell, 4.f));
			CellBox->SetRelativeLocation(CellCenter + FVector(0.f, 0.f, -2.f));
			CellBox->ShapeColor = FColor::Black;

			CellBox->RegisterComponent();
			FootprintPreviewBoxes.Add(CellBox);

			// Edge labels for local cell coordinate + direction.
			const TArray<EGridDirection> EdgeGridDirections =
			{
				EGridDirection::North,
				EGridDirection::East,
				EGridDirection::South,
				EGridDirection::West
			};

			for (const EGridDirection EdgeGridDirection : EdgeGridDirections)
			{
				UTextRenderComponent* EdgeText = NewObject<UTextRenderComponent>(this);
				if (!EdgeText)
				{
					continue;
				}

				EdgeText->ComponentTags.Add(CellTextTag);
				EdgeText->CreationMethod = EComponentCreationMethod::UserConstructionScript;
				EdgeText->SetupAttachment(Root);
				EdgeText->SetHiddenInGame(true);
				EdgeText->bIsEditorOnly = true;
				EdgeText->SetHorizontalAlignment(EHTA_Center);
				EdgeText->SetVerticalAlignment(EVRTA_TextCenter);
				EdgeText->SetWorldSize(25.f);
				EdgeText->SetCastShadow(false);
				EdgeText->bCastDynamicShadow = false;
				EdgeText->bCastStaticShadow = false;
				EdgeText->SetTextRenderColor(FColor::Black);

				const FString DirectionLabel =
					UAtomicGridLibrary::GetGridDirectionLabel(EdgeGridDirection);

				const FString LabelText =
					FString::Printf(
						TEXT("%s (%d, %d)"),
						*DirectionLabel,
						X,
						Y
					);

				EdgeText->SetText(FText::FromString(LabelText));
				EdgeText->SetRelativeLocation(
					GetEditorCellEdgeLabelLocation(CellCoord, FootprintSize, EdgeGridDirection)
				);
				EdgeText->SetRelativeRotation(
					GetEditorEdgeLabelRotation(EdgeGridDirection)
				);

				EdgeText->RegisterComponent();
				FootprintPreviewTexts.Add(EdgeText);
			}
		}
	}
}

FVector AAtomicBuildingActor::GetEditorLocalFootprintCellCenter(const FIntPoint Cell, const FIntPoint FootprintSize) const
{
	// This assumes the building actor's root/pivot is the center of its footprint.
	const float CellSize = EditorPreviewCellSize;
	const float LocalX = (Cell.X + 0.5f - FootprintSize.X * 0.5f) * CellSize;
	const float LocalY = (Cell.Y + 0.5f - FootprintSize.Y * 0.5f) * CellSize;

	return FVector(LocalX, LocalY, 0.f);
}

FVector AAtomicBuildingActor::GetEditorLocalCellCoordLabelLocation(const FIntPoint Cell, const FIntPoint FootprintSize) const
{
	const FVector CellCenter =	GetEditorLocalFootprintCellCenter(Cell, FootprintSize);
	const float OutsideOffset = 26.f;

	// Put label just outside the south edge of the cell.
	return CellCenter +	FVector(0.f, EditorPreviewCellSize * 0.5f + OutsideOffset, 0.f);
}

FVector AAtomicBuildingActor::GetEditorPortEdgeLocation(const FIntPoint LocalCellOffset, const EGridDirection LocalGridDirection, const FIntPoint FootprintSize) const
{
	const FVector CellCenter = GetEditorLocalFootprintCellCenter(LocalCellOffset, FootprintSize);
	const FVector Direction = UAtomicGridLibrary::GridDirectionToVector(LocalGridDirection);

	const float EdgeOffset = EditorPreviewCellSize * 0.5f;
	const float OutsideOffset = 5.f;
	const float Height = 40.f;

	return CellCenter +
		Direction * (EdgeOffset + OutsideOffset) +
		FVector(0.f, 0.f, Height);
}

FVector AAtomicBuildingActor::GetEditorCellEdgeLabelLocation(const FIntPoint Cell, const FIntPoint FootprintSize, const EGridDirection EdgeGridDirection) const
{
	const FVector CellCenter = GetEditorLocalFootprintCellCenter(Cell, FootprintSize);
	const FVector Direction = UAtomicGridLibrary::GridDirectionToVector(EdgeGridDirection);

	const float EdgeOffset = EditorPreviewCellSize * 0.5f;
	const float OutsideOffset = 30.f;
	const float Height = 0.f;

	return CellCenter +	Direction * (EdgeOffset + OutsideOffset) + FVector(0.f, 0.f, Height);
}

FRotator AAtomicBuildingActor::GetEditorEdgeLabelRotation(const EGridDirection EdgeGridDirection) const
{
	const float EdgeYaw = UAtomicGridLibrary::GridDirectionToYawDegrees(EdgeGridDirection);
	return FRotator(90.f, EdgeYaw, 0.f);
}

#endif