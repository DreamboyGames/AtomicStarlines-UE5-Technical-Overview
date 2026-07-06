# Multiplayer-Aware Design

Atomic Starlines is being built as a multiplayer-aware prototype even while individual systems are developed locally first. The goal is not to add networking at the end, but to shape core systems so they can become authoritative and replicated without major rewrites.

## Design Principle

Client input and preview can be responsive, but final gameplay state should be server-authoritative.

This is especially important for construction, automation, inventory, and simulation systems.

## Intended Placement Model

```text
Client
  → local preview and input
  → request placement

Server
  → validates request
  → updates authoritative grid data
  → records placed build
  → replicates result

Clients
  → receive build record
  → update visuals and local caches
```

## Why This Matters

For a multiplayer construction game, the grid is shared state. If multiple players can build, remove, rotate, or modify objects, the server must be the final authority.

This avoids desynced occupancy, duplicate buildings, invalid placements, and clients disagreeing about automation paths.

## Local Preview vs Authoritative State

The client should be allowed to preview quickly. This keeps controls responsive.

However, preview state is not final state. The server should re-check building type, target grid, anchor coordinate, rotation, footprint, occupancy, permissions, and resources where applicable.

## Replication Direction

The project is moving toward replicating placed build records rather than replicating every raw grid cell directly.

This can be cleaner because clients can rebuild local occupancy and visuals from a smaller set of authoritative records.

Benefits include less replicated data, easier debugging, cleaner save/load, and stronger separation between state and presentation.

## Current Focus

The current focus is not complex network prediction. It is building systems with the correct ownership boundaries:

- Player components gather intent
- Grid components validate and own placement state
- Data records describe authoritative builds
- Visual systems respond to state

That foundation keeps multiplayer from becoming a late-game boss fight.
