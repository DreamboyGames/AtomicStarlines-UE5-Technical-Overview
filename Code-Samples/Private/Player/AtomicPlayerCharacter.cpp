// Copyright Dreamboy Games Pty Ltd. All rights reserved.


#include "AtomicStarlines/Public/Player/AtomicPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Curves/CurveVector.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"

// Pawn/Character has player's OwningNetConnection (shares this with the OwningPlayerController, if currently possessed)
// Replicates to All Clients (if relevant)
// NetUpdateFrequency: (100 times/second) (Net Priority 3)
AAtomicPlayerCharacter::AAtomicPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	
	bNetLoadOnClient = true;			// true = Actor will load on clients during map load | false = Actor will only exist on the Server level
	bReplicates = true;					// Designates this Actor to be a replicated Actor, allows Variables to replicate (if marked for replication) (forces NetLoadOnClient true)
	SetReplicatingMovement(true);		// Allows movement data to replicate
	
	// Controller Yaw   = horizontal camera orbit
	// Controller Pitch = zoom-controlled camera angle
	// SpringArm        = follows controller rotation
	// Character body   = independent
	
	MovementComponent = GetCharacterMovement();

	// Rotation
	MovementComponent->bOrientRotationToMovement = true;
	MovementComponent->bUseControllerDesiredRotation = false;
	MovementComponent->RotationRate = FRotator(0.f, 500.0f, 0.f); // Try: 900, 1080, 1440
	
	// Ground Movement
	MovementComponent->MaxAcceleration = 3000.0f;					// Try: 3000, 4096, 6000
	MovementComponent->BrakingDecelerationWalking = 300.0f;		// Try 3000, 4096, 6000
	MovementComponent->GroundFriction = 8.0f;
	MovementComponent->BrakingFrictionFactor = 2.0f;

	// Jump / Falling
	MovementComponent->JumpZVelocity = 520.0f;						// Try: 500, 520, 480
	MovementComponent->GravityScale = 1.0f;							// Try: 1, 0.85, 1.15
	
	// Since jump is montage-driven, avoid held-jump complexity for now.
	JumpMaxHoldTime = 0.0f;
	
	// Air Control
	MovementComponent->AirControl = 0.45f;							// Try: 0.25, 0.45, 0.75
	MovementComponent->AirControlBoostMultiplier = 1.5f;
	MovementComponent->AirControlBoostVelocityThreshold = 250.0f;
	MovementComponent->FallingLateralFriction = 1.25f;				// Try: 0.5, 1.25, 2
	MovementComponent->BrakingDecelerationFalling = 768.0f;			// 256, 768, 1200


	// CAMERA
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComponent"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = true;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComponent"));
	CameraComponent->SetupAttachment(CameraBoom);
	CameraComponent->bUsePawnControlRotation = false;
	
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	
	InitialZoomAlpha = 0.2f;
	TargetZoomAlpha = InitialZoomAlpha;
	CurrentZoomAlpha = InitialZoomAlpha;
	ZoomStep = 0.1f;
	ZoomInterpSpeed = 8.0f;
}

void AAtomicPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	bHasAppliedInitialCameraZoom = false;
}

void AAtomicPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	bHasAppliedInitialCameraZoom = false;
}

void AAtomicPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	bHasAppliedInitialCameraZoom = false;
}

void AAtomicPlayerCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	bHasAppliedInitialCameraZoom = false;
}

bool AAtomicPlayerCharacter::TryApplyInitialCameraZoom()
{
	if (bHasAppliedInitialCameraZoom) return true;
	if (!IsLocallyControlled()) return false;
	if (!ZoomCurve || !CameraBoom) return false;
	LocalController = GetController();
	if (!LocalController) return false;

	CurrentZoomAlpha = InitialZoomAlpha;
	TargetZoomAlpha = InitialZoomAlpha;
	
	UE_LOG(LogTemp, Warning, TEXT("InitialZoomAlpha = %f"), InitialZoomAlpha);
	ApplyCameraZoom(InitialZoomAlpha);
	
	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// TICK
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (!bHasAppliedInitialCameraZoom)
	{
		bHasAppliedInitialCameraZoom = TryApplyInitialCameraZoom();
		return;
	}
	
	if (!FMath::IsNearlyEqual(CurrentZoomAlpha, TargetZoomAlpha))
	{
		CurrentZoomAlpha = FMath::FInterpTo(CurrentZoomAlpha, TargetZoomAlpha, DeltaSeconds, ZoomInterpSpeed);
		ApplyCameraZoom(CurrentZoomAlpha);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// ZOOM
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerCharacter::Zoom(const float InputValue)
{
	if (FMath::IsNearlyZero(InputValue)) return;
	
	const float ZoomDirection = FMath::Sign(InputValue);
	TargetZoomAlpha = FMath::Clamp(TargetZoomAlpha + ZoomDirection * ZoomStep, 0.0f, 1.0f);
}

void AAtomicPlayerCharacter::ApplyCameraZoom(const float ZoomAlpha) const
{
	UE_LOG(LogTemp, Warning, TEXT("ApplyCameraZoom"));
	
	// Run only on the locally controlled Pawn/Character
	if (!IsLocallyControlled() || !LocalController) return;
	if (!ensure(ZoomCurve && CameraBoom)) return;
	
	// Zoom-driven camera angle -- ZoomAlpha blends between ShoulderCam & TopDown Cam
	// X = Zoom Distance / Spring Arm Length, Y = Pitch Angle, Z = Vertical Socket/Camera Offset
	const FVector CurveValue = ZoomCurve->GetVectorValue(ZoomAlpha);

	CameraBoom->TargetArmLength = CurveValue.X;		// X = Spring Arm Length
	CameraBoom->SocketOffset.Z = CurveValue.Z;		// Z = Socket Height Offset

													// Y = Camera Pitch
	const FRotator CurrentRotation = LocalController->GetControlRotation();
	const FRotator TargetRotation(CurveValue.Y, CurrentRotation.Yaw, 0.0f);
	LocalController->SetControlRotation(TargetRotation);
	
	UE_LOG(LogTemp, Warning, TEXT("ApplyCameraZoom ZoomAlpha: %f"), ZoomAlpha);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// MOVE
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerCharacter::Move(const FVector2D InputValue)
{
	// Controller Rotation relative movement
	FRotator ControlRot = GetControlRotation();
	ControlRot.Pitch = 0.0f;
	
	// Forward/Back
	AddMovementInput(ControlRot.Vector(), InputValue.X);
	
	// Sideways
	const FVector RightDirection = ControlRot.RotateVector(FVector::RightVector);
	AddMovementInput(RightDirection, InputValue.Y);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
// JUMP
/////////////////////////////////////////////////////////////////////////////////////////////////////////
void AAtomicPlayerCharacter::JumpStart()
{
	if (!CanJump()) return;
	
	const bool bHasMoveIntent = GetLastMovementInputVector().SizeSquared2D() > FMath::Square(0.1f);
	PlayAnimMontage( bHasMoveIntent ? RunJumpMontage : JumpMontage);
}
