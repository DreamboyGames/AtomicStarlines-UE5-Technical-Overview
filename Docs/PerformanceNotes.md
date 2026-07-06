# Performance Notes

Atomic Starlines is being designed with performance awareness from the start. The current prototype is not heavily optimized yet, but the architecture is intended to avoid common Unreal performance traps as the simulation grows.

## Performance Goals

- Keep per-frame work predictable
- Avoid unnecessary actor ticking
- Store grid data in cache-friendly structures where practical
- Prefer records and arrays for simulation state
- Keep visual actors separate from authoritative data
- Use profiling before making major optimization decisions

## Grid Data

The grid is a strong candidate for array-backed data.

Grid cells can be stored in a flat array and accessed through coordinate-to-index conversion. This is usually simpler and more cache-friendly than storing many independent UObject or Actor instances for each cell.

```text
Grid coordinate
  → array index
  → cell data
```

## Actor Cost

Actors are useful for presentation and interaction, but they are relatively expensive compared to lightweight data records.

The long-term direction is to use data records for authoritative state and actors/components for visuals and interaction.

## Tick Discipline

The project aims to avoid unnecessary Tick usage.

Useful patterns include event-driven updates, timers for lower-frequency systems, batched simulation, rebuilding caches only when state changes, and updating visuals only when relevant state changes.

## Profiling Mindset

Optimization should be driven by measurement.

Relevant Unreal tools include Unreal Insights, stat commands, CPU and GPU profiling, network profiling, collision debugging, logging, and visual debug overlays.

## Expected Future Hotspots

Likely performance-sensitive areas include large grid queries, conveyor/item simulation, building visual updates, replication of changing state, save/load, UI updates, and future routing logic.

## Current Trade-Off

The current priority is clean architecture over premature optimization. However, the code is being shaped so that expensive systems can later be batched, cached, replicated selectively, or moved into more data-oriented structures without throwing away the design.
