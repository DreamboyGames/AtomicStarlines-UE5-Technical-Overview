// Copyright (c) 2026 Dreamboy Games Pty Ltd. All rights reserved.

#pragma once
#include "Grid/AtomicGridTypes.h"
#include "AtomicBuildingTypes.generated.h"

USTRUCT(BlueprintType)
struct FAtomicBuildingPortDefinition {
	GENERATED_BODY()
	
	// Naming Format: "Input_A", "Output_A"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building|Port")
	FName PortID;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building|Port")
	EBuildingPortType PortType = EBuildingPortType::Input;
	
	// Which local footprint cell owns this port
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building|Port", meta=(ClampMax=3))
	FIntPoint LocalCellOffset = FIntPoint(0, 0);
	
	// Local side before building rotation is applied.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building|Port")
	EGridDirection LocalGridDirection = EGridDirection::East;
	
	// @todo: Prototype version. Later this could be a GameplayTag
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building|Port")
	FName AcceptedItemID;
};