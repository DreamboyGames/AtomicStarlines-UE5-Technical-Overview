#pragma once

#define COLLISION_INTERACTION ECC_GameTraceChannel1
#define COLLISION_GRID_TRACE ECC_GameTraceChannel2

UENUM(BlueprintType)
enum class EAtomicPlacementFailReason : uint8 {
	Blocked,
};