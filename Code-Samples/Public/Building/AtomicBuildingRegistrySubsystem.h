// Copyright (c) Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AtomicBuildingRegistrySubsystem.generated.h"

class UAtomicBeltDefinition;
class UAtomicBuildingDatabase;
class UAtomicBuildingDefinition;
/**
 * 
 */
UCLASS()
class ATOMICSTARLINES_API UAtomicBuildingRegistrySubsystem : public UGameInstanceSubsystem {
	GENERATED_BODY()
	
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	
	UAtomicBuildingDefinition* FindBuildingDefinition(const FName BuildingID) const;
	UAtomicBeltDefinition* FindBeltDefinition(const FName BeltID) const;
	
private:
	UPROPERTY()
	TObjectPtr<UAtomicBuildingDatabase> BuildingDatabase;
	
	// Runtime Cache
	UPROPERTY()
	TMap<FName, TObjectPtr<UAtomicBuildingDefinition>> BuildingDefinitionMap;
	
	UPROPERTY()
	TMap<FName, TObjectPtr<UAtomicBeltDefinition>> BeltDefinitionMap;
};


// Why use a global Registry Subsystem?
// global lookup from many systems
// async loading
// Asset Manager integration
// DLC/mod/plugin content
// multiple databases merged together
// validation at game startup
// hot reload/editor tooling
// unlock filtering by player/company state