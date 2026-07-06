// Copyright Dreamboy Games Pty Ltd. All rights reserved.


#include "Player/Components/AtomicInteractionComponent.h"

#include "AtomicGameTypes.h"
#include "AtomicStarlines.h"
#include "Engine/OverlapResult.h"
#include "Interface/AtomicInteractionInterface.h"
#include "Player/AtomicPlayerController.h"

TAutoConsoleVariable<bool> CVarInteractionDebugDrawing(TEXT("game.interaction.DebugDraw"), false, TEXT("Enable Interaction Debug Drawing. (0 = off, 1 = enabled)"), ECVF_Cheat);

UAtomicInteractionComponent::UAtomicInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	
	InteractionRadius = 300.0f;
}

void UAtomicInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	LocalController = Cast<AAtomicPlayerController>(GetOwner());
	
	// Interaction selection is local-player presentation logic.
	// Do not run it on remote/non-local PlayerController copies.
	const bool bShouldTick = LocalController && LocalController->IsLocalController();
	SetComponentTickEnabled(bShouldTick);
}

void UAtomicInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LocalController = nullptr;
	Super::EndPlay(EndPlayReason);
}

void UAtomicInteractionComponent::Interact()
{
	if (!LocalController) return;
	if (!LocalController->IsLocalController()) return;
	
	if (IAtomicInteractionInterface* InteractInterface = Cast<IAtomicInteractionInterface>(SelectedActor))
	{
		InteractInterface->Interact();
	}
}

void UAtomicInteractionComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (!LocalController) return;
	if (!LocalController->IsLocalController()) return;
	if (!LocalController->PlayerCameraManager) return;

	// Pawn is not stable and can change during respawn, death, travel possession,
	// So it cannot be cached.
	const APawn* Pawn = LocalController->GetPawn();
	if (!Pawn) return;

	const FVector Center = Pawn->GetActorLocation();
	const FVector CameraLocation = LocalController->PlayerCameraManager->GetCameraLocation();
	
	// Check for All Interactables in range
	const float InteractionRadiusSqrd = (InteractionRadius * InteractionRadius);
	ECollisionChannel CollisionChannel = COLLISION_INTERACTION;
	FCollisionShape CollisionShape;
	CollisionShape.SetSphere(InteractionRadius);
	
	// @todo: make async on different thread
	TArray<FOverlapResult> OverlapResults;
	GetWorld()->OverlapMultiByChannel(OverlapResults, Center, FQuat::Identity, CollisionChannel, CollisionShape);

	AActor* BestActor = nullptr;
	float HighestWeight = 0.0;

	const bool bEnabledDebugDraw = CVarInteractionDebugDrawing.GetValueOnGameThread();
	FVector OverlapBoxOrigin;
	FVector OverlapBoxSize;
	
	// Calculate BestActor from DistanceTo Player & Player Rotation & Camera Rotation
	for (FOverlapResult& Overlap : OverlapResults)
	{
		FVector Origin;
		FVector BoxExtends;
		Overlap.GetActor()->GetActorBounds(true, Origin, BoxExtends);
		//FVector OverlapLocation = Overlap.GetActor()->GetActorLocation();
		
		FVector OverlapDirection = (Origin - Center).GetSafeNormal();
		FVector CameraDirection = LocalController->GetControlRotation().Vector();
		FVector PlayerFacingDirection = Pawn->GetActorRotation().Vector();
		
		// Distance from Player to Overlap origin
		const float DistanceToSqrd = (Origin - Center).SizeSquared();
		// Normalize and Invert, smaller dist is higher weight
		const float NormalizedDistanceTo = 1.0 - (DistanceToSqrd / InteractionRadiusSqrd);
		
		// DotProduct of CameraDirection to OverlapDirection
		const float CamDotResult = FVector::DotProduct(OverlapDirection, CameraDirection);
		
		// DotProduct of PlayerFacingDirection to OverlapDirection
		const float PlayerDotResult = FVector::DotProduct(OverlapDirection, PlayerFacingDirection);
		
		float NormalizedDotResult = 0.0f;
		
		if (LocalController->GetMoveWasLastInputValue())
		{
			NormalizedDotResult = PlayerDotResult * 0.5f + 0.5f; // Normalize: (-1 to 1) -> (0 to 1)
		}
		else
		{
			NormalizedDotResult = CamDotResult * 0.5f + 0.5f; // Normalize: (-1 to 1) -> (0 to 1)
		}
		
		const float Weight = (NormalizedDotResult * DirectionToWeightScale) + (NormalizedDistanceTo * DistanceToWeightScale);
		if (Weight > HighestWeight)
		{
			BestActor = Overlap.GetActor();
			HighestWeight = Weight;
			OverlapBoxOrigin = Origin;
			OverlapBoxSize = BoxExtends;
		}
		
		if (bEnabledDebugDraw)
		{
			FString DebugString = FString::Printf(TEXT("Weight: %f,  Dot: %f,  Dist: %f"), Weight, NormalizedDotResult, NormalizedDistanceTo);
			DrawDebugString(GetWorld(), Origin, DebugString, nullptr, FColor::White, 0.0f, true, 1.2f);
			DrawDebugBox(GetWorld(), Origin, BoxExtends, FColor::Red);
		}
	}

	// Found best Actor for Interaction
	if (BestActor)
	{
		SelectedActor = BestActor;
		if (bEnabledDebugDraw) DrawDebugBox(GetWorld(), OverlapBoxOrigin, OverlapBoxSize + 5.0f, FColor::Green);
	}

	if (bEnabledDebugDraw) DrawDebugSphere(GetWorld(), Center, InteractionRadius, 16, FColor::Black);
}