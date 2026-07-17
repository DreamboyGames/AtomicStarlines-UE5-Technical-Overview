// Copyright (c) Dreamboy Games Pty Ltd. All rights reserved.


#include "Building/AtomicBuildingRegistrySubsystem.h"

#include "AtomicStarlines.h"
#include "Belts/AtomicBeltDefinition.h"
#include "Building/AtomicBuildingDatabase.h"
#include "Building/AtomicBuildingDefinition.h"
#include "Core/AtomicProjectSettings.h"

void UAtomicBuildingRegistrySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	const UAtomicProjectSettings* ProjectSettings = GetDefault<UAtomicProjectSettings>();
	if (!ProjectSettings) return;
	
	BuildingDatabase = ProjectSettings->BuildingDatabase.LoadSynchronous();
	if (!BuildingDatabase)
	{
		UE_LOG(LogGame, Error, TEXT("No Atomic Building Database assigned."));
		check(!BuildingDatabase);
		return;
	}

	for (UAtomicBuildingDefinition* Definition : BuildingDatabase->BuildingDefinitions)
	{
		if (!Definition) continue;
		
		BuildingDefinitionMap.Add(Definition->BuildingID, Definition);
	}
	
	for (UAtomicBeltDefinition* Definition : BuildingDatabase->BeltDefinitions)
	{
		if (!Definition) continue;
		
		BeltDefinitionMap.Add(Definition->BeltID, Definition);
	}
}

UAtomicBuildingDefinition* UAtomicBuildingRegistrySubsystem::FindBuildingDefinition(const FName BuildingID) const
{
	if (const TObjectPtr<UAtomicBuildingDefinition>* Definition = BuildingDefinitionMap.Find(BuildingID))
	{
		return Definition->Get();
	}
	
	return nullptr;
}

UAtomicBeltDefinition* UAtomicBuildingRegistrySubsystem::FindBeltDefinition(const FName BeltID) const
{
	if (const TObjectPtr<UAtomicBeltDefinition>* Definition = BeltDefinitionMap.Find(BeltID))
	{
		return Definition->Get();
	}
	
	return nullptr;
}



