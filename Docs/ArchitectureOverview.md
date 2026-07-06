# Architecture Overview

Atomic Starlines is structured around modular Unreal Engine gameplay systems. The current architecture prioritizes clear ownership of data, explicit validation paths, and systems that can evolve toward multiplayer without requiring a full rewrite.

## Core Design Goals

- Keep gameplay data separate from visual presentation
- Prefer reusable components over large monolithic actors
- Make placement validation deterministic and easy to debug
- Support client-side preview with server-authoritative placement
- Use data assets for building definitions and configuration
- Keep code readable and maintainable as the prototype grows

## High-Level Structure

The current grid/building system is organized around a ship grid actor with separate components for data, placement, and visuals.

```text
AAtomicShipGrid
  ├── UAtomicGridDataComponent
  ├── UAtomicGridPlacementComponent
  ├── UAtomicGridVisualComponent
  └── Trace / bounds components
```

## Component Responsibilities

### Grid Data Component

Owns the underlying grid state: grid cells, occupancy, placed build records, and rebuildable occupancy caches.

### Grid Placement Component

Owns placement validation and placement execution logic: footprint checks, rotation handling, build rules, and coordination with building definitions.

### Grid Visual Component

Owns debug and presentation-related grid visuals: overlays, placement feedback, visual variants, and debugging support.

## Player-Side Placement

Player placement logic is kept separate from the ship grid. The player component handles input, preview placement, rotation, and build intent. The grid remains responsible for validation and authoritative placement.

```text
Player input
  → local placement preview
  → placement request
  → grid placement validation
  → placed build record
  → visual/build actor update
```

## Current Architecture Direction

The project is moving toward a record-driven placement model where placed objects are represented by lightweight build records. Actors and visuals can then be spawned, rebuilt, or replicated from those records.

This supports save/load, multiplayer replication, cleaner debugging, and better separation between simulation data and visual actors.
