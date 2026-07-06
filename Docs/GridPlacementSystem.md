# Grid Placement System

The grid placement system is the current core technical feature in Atomic Starlines. It handles world-to-grid conversion, building footprint validation, rotation, occupancy, and placement records.

## Purpose

The system needs to answer simple questions reliably:

- Which cell is the player aiming at?
- What cells does this building occupy?
- Is the footprint valid?
- What rotation is being used?
- What runtime record should be created?
- What visuals should be shown?

## Grid Coordinates

Grid math is centralized in a shared grid library rather than scattered across actors.

Common responsibilities include:

- Converting world position to grid coordinate
- Converting grid coordinate to world position
- Checking whether a coordinate is inside the grid
- Converting grid coordinates to array indices
- Resolving footprint cells
- Rotating footprint offsets around a pivot

## Placement Flow

```text
1. Player enters placement mode
2. Placement preview finds target grid cell
3. Building definition provides footprint and preview data
4. Rotation is applied
5. Footprint cells are calculated
6. Placement component checks every required cell
7. Preview shows valid or invalid state
8. Player confirms placement
9. Server/grid validates again
10. Build record is added
11. Occupancy and visuals update
```

## Validation Rules

Placement validation checks whether each required cell is usable. Typical checks include whether the cell is inside the grid, has floor, is not blocked, is not reserved, and is not already occupied.

The exact rules can vary by build type. A conveyor belt, wall, machine, and room object may each use different constraints.

## Rotation and Footprints

Buildings use grid-based rotations, commonly represented as North, East, South, and West.

Footprints are resolved from a pivot and then rotated around that pivot. This allows non-square buildings, belts, and corner pieces to occupy predictable cells while keeping visual rotation consistent.

The project is currently refining pivot rules, corner belt rotation, flow direction, visual variant resolution, and footprint orientation for asymmetric shapes.

## Placement Records

Placed objects are intended to be represented by runtime records rather than relying only on spawned actors.

A placement record may include building ID, instance ID, anchor grid coordinate, rotation, owner/player info, occupied cells, and build category.

This record-driven approach is useful for replication, save/load, reconstruction, debugging, and long-term simulation.
