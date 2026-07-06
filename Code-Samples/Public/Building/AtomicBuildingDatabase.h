// Copyright (c) Dreamboy Games Pty Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AtomicBuildingDatabase.generated.h"

// DeveloperSettings is the address book.
// The database is the library.
// The registry is the librarian.

class UAtomicBeltDefinition;
class UAtomicBuildingDefinition;
/**
 * 
 */
UCLASS()
class ATOMICSTARLINES_API UAtomicBuildingDatabase : public UDataAsset {
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TArray<TObjectPtr<UAtomicBuildingDefinition>> BuildingDefinitions;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Atomic|Building")
	TArray<TObjectPtr<UAtomicBeltDefinition>> BeltDefinitions;
};
