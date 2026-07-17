// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AtomicPlayerCharacter.generated.h"

class AAtomicPlayerController;
class UCurveVector;
class UAnimMontage;
struct FInputActionInstance;
struct FInputActionValue;
class UInputAction;
class USpringArmComponent;
class UCameraComponent;

UCLASS(meta=(PrioritizeCategories="Components AtomicCamera AtomicAnimation"))
class ATOMICSTARLINES_API AAtomicPlayerCharacter : public ACharacter {
	GENERATED_BODY()
	
	// Controller Yaw   = horizontal camera orbit
	// Controller Pitch = zoom-controlled camera angle
	// SpringArm        = follows controller rotation
	// Character body   = independent

public:
	AAtomicPlayerCharacter();
	virtual void BeginPlay() override;

	virtual void Tick(float DeltaSeconds) override;

	void Move(const FVector2D InputValue);
	//void Look(const FVector2D InputValue);
	void Zoom(const float InputValue);
	void JumpStart();

	
protected:
	UPROPERTY(Transient)
	TObjectPtr<AController> LocalController;
	
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	
	virtual void PawnClientRestart() override;
	
	void RefreshLocalControllerCache();
	
	//// COMPONENTS /////////////////////////////////////////////////////////
	UPROPERTY(VisibleAnywhere, Category=Components)
    TObjectPtr<UCameraComponent> CameraComponent;

    UPROPERTY(VisibleAnywhere, Category=Components)
    TObjectPtr<USpringArmComponent> CameraBoom;

	
	//// CAMERA /////////////////////////////////////////////////////////
	float InitialZoomAlpha;
	float TargetZoomAlpha;
    float CurrentZoomAlpha;
	UPROPERTY(EditDefaultsOnly, Category="AtomicCamera", meta=(DisplayPriority=0))
    float ZoomStep;
    UPROPERTY(EditDefaultsOnly, Category="AtomicCamera")
    float ZoomInterpSpeed;
	
	// Zoom-driven camera angle -- ZoomAlpha blends between ShoulderCam & TopDown Cam
	// X = Zoom Distance / Spring Arm Length, Y = Pitch Angle, Z = Vertical Socket/Camera Offset
	UPROPERTY(EditDefaultsOnly, Category="AtomicCamera")
	TObjectPtr<UCurveVector> ZoomCurve;

	void ApplyCameraZoom(float ZoomAlpha) const;
	bool TryApplyInitialCameraZoom();


	//// MONTAGES /////////////////////////////////////////////////////////
	UPROPERTY(EditDefaultsOnly, Category="AtomicAnimation")
	TObjectPtr<UAnimMontage> JumpMontage;
	
	UPROPERTY(EditDefaultsOnly, Category="AtomicAnimation")
	TObjectPtr<UAnimMontage> RunJumpMontage;


private:
	UPROPERTY()
	TObjectPtr<UCharacterMovementComponent> MovementComponent;
	
	bool bHasAppliedInitialCameraZoom = false;
};
