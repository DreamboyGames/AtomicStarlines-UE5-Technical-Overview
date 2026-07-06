# Data-Driven Architecture

Atomic Starlines uses data-driven design to keep gameplay systems configurable, extensible, and easier to iterate on.

The goal is to avoid hardcoding every building, belt, visual variant, and placement rule directly into gameplay classes.

## Why Data-Driven Design

Data-driven systems make it easier to add new content without rewriting core logic.

For Atomic Starlines, this is useful for building definitions, footprints, mesh references, placement categories, conveyor variants, costs, resource requirements, UI labels, and future simulation stats.

## Building Definitions

Buildings are intended to be described through definition assets rather than being hardcoded in placement logic.

A building definition may describe:

- Building ID
- Display name
- Actor class
- Preview mesh
- Footprint size
- Build category
- Rotation behavior
- Placement rules
- Visual data
- Future cost or unlock requirements

## Runtime Records vs Definitions

The project separates static definition data from runtime placement data.

```text
Building Definition
  → what this type of building is

Placed Build Record
  → one placed instance of that building in the world
```

This distinction keeps systems cleaner. A placed build record only needs to store the information that can change per instance, such as location, rotation, owner, and unique ID.

## Registry Direction

A building registry or database can map building IDs to their definitions.

This supports fast lookup during placement, UI build menus, save/load reconstruction, replication using IDs instead of heavy asset references, and cleaner content management.

## Editor-Friendly Workflow

Data assets make the project easier to work with in the Unreal Editor.

Designers or future collaborators should be able to add and tune content without modifying C++ every time.

## Benefits for Multiplayer

Data-driven definitions also help multiplayer.

Instead of replicating entire asset references, the server can replicate compact IDs and runtime state. Clients can resolve those IDs locally through their registry.

## Current Direction

The project is currently building the foundation for data asset building definitions, runtime placed build records, registry-based lookup, grid validation based on definition data, and visual spawning based on definition and record state.

The intended result is a system where adding a new building mostly means adding data, not rewriting gameplay code.
