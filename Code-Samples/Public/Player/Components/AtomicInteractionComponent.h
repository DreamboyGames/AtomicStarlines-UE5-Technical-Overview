// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AtomicInteractionComponent.generated.h"


class AAtomicPlayerController;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ATOMICSTARLINES_API UAtomicInteractionComponent : public UActorComponent {
	GENERATED_BODY()

public:
	UAtomicInteractionComponent();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	void Interact();

protected:
	UPROPERTY(EditDefaultsOnly, Category="AtomicInteraction")
	float InteractionRadius;

	UPROPERTY()
	TObjectPtr<AActor> SelectedActor;
	
	UPROPERTY(EditDefaultsOnly, Category="AtomicInteraction")
	float DistanceToWeightScale = 1.0f;
	
	UPROPERTY(EditDefaultsOnly, Category="AtomicInteraction")
	float DirectionToWeightScale = 2.0f;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<AAtomicPlayerController> LocalController;
};
