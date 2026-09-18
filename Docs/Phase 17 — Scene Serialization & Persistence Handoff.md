# Nocturne Engine — Phase 17 Scene Serialization & Persistence Handoff

> **Status:** DRAFT BOUNDARY NOTE — PHASE 16 STILL IN COMPLETION HARDENING
>
> **Purpose:** capture the reusable Phase 16 seams now; finalize this handoff only after the Phase 16 Completion Report passes.
>
> **Date:** 2026-09-18

---

## Reuse without replacement

Phase 17 must reuse the Phase 16 runtime reflection and authoring foundation:

- \`ReflectionRegistry\` as the canonical reflected schema;
- stable \`TypeId\`, \`PropertyId\` and reflected type metadata;
- reflected component enumeration;
- semantic reflected property read/write;
- \`OwnedReflectedValue\` lifetime and allocator semantics;
- \`ReflectedComponentSnapshot\`;
- \`ReflectedEntitySubtreeSnapshot\`;
- \`TransientEntityPrototype\` only as an in-memory capture/instantiate seam.

Phase 17 must not introduce a serializer-owned duplicate component/property schema.

---

## Prototype boundary inherited from Phase 16

**Design choice (not directly from the book):** \`TransientEntityPrototype\` is deliberately not a prefab asset.

It provides only:

\`source runtime subtree -> reflected transient template -> fresh runtime subtree instances\`

It does not provide:

- prefab files;
- persistent prefab asset IDs;
- persistent entity IDs;
- serialized EntityHandle references;
- disk compatibility guarantees;
- migration/version execution;
- prefab override persistence;
- atomic save/load.

Those are Phase 17 persistence concerns.

---

## What Phase 17 may reuse directly

\`ReflectedComponentSnapshot\` already demonstrates lifecycle-aware reflected value capture and semantic restore.

\`ReflectedEntitySubtreeSnapshot\` already demonstrates reflection-driven subtree capture, component reconstruction, new runtime handle creation, hierarchy reconstruction and rollback on failed instantiation.

\`TransientEntityPrototype\` demonstrates that the same transient reflected template can create multiple independent runtime instances without relying on source handles as identity.

These are implementation seams, not a serialized format contract.

---

## Phase 17 start gate

Before this draft becomes the final Phase 17 handoff:

- Phase 16 manual editor regression must pass;
- the 15+ minute authoring soak must pass;
- remaining repository/build hygiene must be reconciled;
- Phase 16 Test and CI Validation Report must pass;
- Phase 16 Completion Report must declare Phase 16 complete.
