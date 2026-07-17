// Copyright Dreamboy Games Pty Ltd. All rights reserved.

#include "ProjectTypes/AtomicPlacementPreviewTypes.h"

#include "Belts/AtomicBeltDefinition.h"
#include "Building/AtomicBuildingDefinition.h"


bool FAtomicPlacementSelection::IsValid() const
{
	switch (PlacementMode)
	{
	case EPlacementMode::Building:
		return !DefinitionID.IsNone() && BuildingDefinition != nullptr && BuildingDefinition->BuildingID == DefinitionID;	
			
	case EPlacementMode::Belt:
		return !DefinitionID.IsNone() && BeltDefinition != nullptr && BeltDefinition->BeltID == DefinitionID;	
			
	default:
		return false;
	}
}
