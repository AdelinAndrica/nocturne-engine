# Nocturne Engine — Production Engineering Standard

> **Status:** LOCKED FOR PHASE 15 AND ALL SUBSEQUENT PHASES
>
> **Product target:** a sellable Windows game engine and a shippable first-person survival horror game.
>
> **Applies to:** engine runtime, editor, tools, asset pipeline, game systems, build/deployment, validation and shipping work.
>
> **Primary architectural sources:** the books provided with the Nocturne Engine project.
>
> **Project-level quality policy:** the completion gates in this document are **Design choice (not directly from the book)** unless a specific phase document explicitly ties an item to a book section.

## 1. Purpose

Nocturne Engine is not being built as a tutorial engine, school project, throwaway prototype or proof of concept.

Starting with Phase 15, every phase must be implemented to a **production-grade standard within the scope of that phase**.

That means:

- complete responsibility ownership;
- explicit lifetime rules;
- robust invalid-state handling;
- documented invariants;
- deterministic behavior where the architecture requires it;
- diagnostics for failures;
- tests and regression coverage;
- measured performance characteristics for hot paths;
- no silent corruption;
- no knowingly temporary architecture presented as final;
- no editor/runtime divergence that will require a rewrite later;
- no hidden dependence on validation-scene constants;
- no feature accepted solely because the happy path works.

Professional-grade does **not** mean pulling future roadmap features into the current phase. Scope remains phase-bounded. The rule is:

> Implement the current phase completely enough that later phases can build on it without first replacing its foundation.

## 2. Source-of-truth rule

The provided books remain the primary source of truth for concepts, terminology and architecture.

When a design decision is not directly prescribed by those sources, the documentation and code review notes must label it:

**Design choice (not directly from the book)**

No invented citations or page numbers are allowed.

Relevant recurring references include:

- Jason Gregory — *Game Engine Architecture, 3rd Edition*:
  - gameplay foundation and runtime object models;
  - object references and queries;
  - engine support systems;
  - tools/editor architecture;
  - runtime/update ordering;
- Bob Nystrom — *Game Programming Patterns*:
  - Component;
  - Data Locality;
  - event/decoupling patterns where they are actually applicable;
- Frank D. Luna — *Introduction to 3D Game Programming with DirectX 12* for DX12 resource/state/synchronization/rendering mechanics;
- Eric Lengyel and David Eberly for mathematics, rendering and geometric foundations where relevant.

## 3. Non-negotiable engineering qualities

### 3.1 Ownership and lifetime

Every substantial subsystem must define:

- who creates it;
- who owns it;
- who may reference it;
- when it is valid;
- how it shuts down;
- what happens when dependencies disappear;
- whether references survive relocation/reallocation;
- whether handles can become stale and how stale access is prevented.

Raw pointers may be used internally where appropriate, but ownership must never be ambiguous.

### 3.2 Invariants and invalid states

Each subsystem must document its invariants.

Examples:

- handle generation must match the live slot;
- a destroyed entity cannot still expose live components;
- a transform hierarchy cannot contain cycles;
- a GPU resource cannot be destroyed while still in-flight;
- a serialized reference cannot depend on a transient runtime address.

Invalid operations must have an explicit policy:

- reject;
- return failure;
- assert in development;
- log diagnostics;
- or perform a documented no-op.

Silent corruption is never an acceptable policy.

### 3.3 Error handling

Professional completion requires negative-path handling, not just successful execution.

Each relevant phase must test:

- invalid input;
- missing resources;
- duplicate registration;
- stale handles;
- out-of-range identifiers;
- capacity growth;
- allocation failure paths where practical;
- malformed or incompatible data when persistence/import is involved;
- device/resource errors when rendering is involved.

Shipping-safe failures should degrade or report cleanly where possible.

### 3.4 Data ownership and single source of truth

A datum must have one authoritative owner.

Editor mirrors, cached render data, derived bounds and presentation state are allowed only when their relationship to the authoritative data is explicit.

Do not maintain independent mutable copies of the same world state in:

- runtime;
- editor;
- renderer;
- serializer;
- physics;
- scripting.

Bridges may transform data, but they do not become alternate authorities.

### 3.5 API quality

Public engine APIs must be:

- narrow;
- intention-revealing;
- const-correct where applicable;
- explicit about ownership and failure;
- stable enough for the next roadmap phases;
- free of accidental Win32/editor dependencies in runtime layers;
- free of project-specific policy when the engine should expose a mechanism.

The existing project rule remains active:

- no STL types in public engine headers unless the architectural contract is deliberately revised and documented.

### 3.6 Memory and allocation discipline

Hot paths must not hide uncontrolled heap churn.

Each phase that introduces frequently-updated data must establish:

- storage ownership;
- growth policy;
- relocation behavior;
- invalidation rules;
- expected allocation frequency;
- whether frame arenas/pools/dense storage are appropriate.

Do not optimize blindly, but do not accept known per-frame allocations or synchronous compilation/loading in hot paths when the work can be moved out of them.

### 3.7 Determinism and ordering

Where system correctness depends on order, that order must be explicit and tested.

Examples:

- initialization/shutdown;
- transform propagation;
- simulation stages;
- component destruction;
- render extraction;
- fixed-timestep physics;
- serialization output ordering where deterministic output is required.

A generic scheduler is not automatically more professional than explicit ordering. Complexity must be justified by requirements.

### 3.8 Performance validation

Performance-sensitive systems must have a measured baseline before sign-off.

Do not approve a phase based on intuition alone when it introduces a hot loop, large storage, GPU work, streaming, pathfinding, animation evaluation or similar cost.

At minimum, capture:

- representative workload;
- timing or throughput measurement;
- allocation behavior where relevant;
- obvious algorithmic scaling;
- regression comparison after major fixes.

Exact performance budgets are phase-specific and should be set when the system requirements are known.

### 3.9 Diagnostics and observability

A sellable engine must help diagnose its own failures.

Relevant systems should expose development diagnostics such as:

- assertions;
- structured logging;
- debug names;
- counters;
- validation layers;
- one-shot dumps;
- profiler markers;
- editor-visible errors.

Diagnostics must not require invasive ad-hoc code changes each time a subsystem fails.

### 3.10 Tests and regression

A phase is not complete merely because the editor launches.

Required validation should include the appropriate combination of:

- unit tests for isolated algorithms/data structures;
- integration tests across subsystem boundaries;
- runtime interaction tests;
- stress/capacity tests;
- regression tests for previous phases;
- debug-layer/validation-layer checks for graphics;
- save/load round-trip tests for persistence phases;
- deterministic output checks for cooker/build phases.

Every phase must preserve previously validated contracts unless a deliberate architecture change is documented and migrated.

### 3.11 Tool/editor quality

Editor-facing systems must handle real authoring workflows, not only demonstration scenes.

Where applicable:

- actions must be undoable/redoable when the phase owns editing history;
- selection and inspection must remain synchronized;
- invalid assets/components must surface useful diagnostics;
- authoring operations must not corrupt runtime state;
- destructive operations need explicit semantics;
- long operations should not unnecessarily block the UI;
- editor state must not become a second authoritative game world.

### 3.12 Persistence and versioning

Whenever a phase introduces data that will survive process lifetime, it must define:

- stable identity;
- format version;
- compatibility policy;
- reference representation;
- migration or rejection behavior;
- deterministic write policy where required;
- atomicity/recovery expectations for writes where applicable.

Transient runtime handles, pointers and container indices must not become persisted identities.

### 3.13 Concurrency

Threading is introduced only when the system benefits from it and ownership is clear.

A production-grade subsystem must not be made “more advanced” by adding unsound concurrency.

When concurrency is introduced, define:

- thread ownership;
- mutation rules;
- synchronization;
- job lifetime;
- cancellation/shutdown behavior;
- visibility/memory-order requirements where applicable.

### 3.14 Security and trust boundaries

For tooling, asset import, serialization, scripting and external data, malformed inputs must not be assumed impossible.

Where relevant:

- validate sizes and ranges;
- reject invalid references;
- avoid unchecked buffer arithmetic;
- avoid executing untrusted paths/data as code;
- keep editor/tool parsing failures recoverable where possible.

## 4. Phase completion gate

Beginning with Phase 15, a phase may be marked COMPLETE only when all applicable items below are satisfied:

- [ ] architecture is documented before or alongside implementation;
- [ ] book grounding is recorded;
- [ ] design choices not directly from the books are labeled;
- [ ] ownership/lifetime rules are explicit;
- [ ] invariants are documented;
- [ ] invalid/stale/error cases are handled;
- [ ] no known structural TODO is being hidden as a final implementation;
- [ ] no duplicate authoritative state exists across layers;
- [ ] public APIs are reviewed for future-phase compatibility;
- [ ] hot-path allocation/performance behavior has been inspected;
- [ ] representative tests exist;
- [ ] previous-phase regression checks pass;
- [ ] diagnostics are sufficient to debug failures;
- [ ] documentation matches the actual code;
- [ ] an implementation report records what was really built;
- [ ] a completion report records final validation and deferred scope;
- [ ] deliberately deferred work is assigned to a later roadmap phase rather than left ambiguous.

## 5. Temporary scaffolding policy

Validation scaffolding is allowed when a phase needs it, but it must be clearly identified.

Examples:

- procedural validation geometry;
- temporary test scenes;
- synthetic benchmark entities;
- debug-only controls.

Scaffolding must not quietly become a permanent production API.

Every completion report must state whether temporary scaffolding remains and which later phase replaces it.

## 6. Refactoring policy

Refactoring is required when the current architecture cannot support the professional implementation of the current phase.

However:

- do not refactor unrelated systems for aesthetics alone;
- preserve validated behavior;
- keep migrations reviewable;
- add regression coverage before risky replacement work;
- document contract changes.

“Do not touch working code” is not a valid reason to preserve a known structural defect.

“Rewrite everything” is not a valid substitute for a bounded migration plan.

## 7. Definition of professional-grade for Nocturne

For this project, professional-grade means:

- suitable as a durable base for a commercial product;
- understandable and maintainable by an experienced engineer who did not write it;
- robust under invalid inputs and lifecycle edges expected for the subsystem;
- testable without manual guesswork;
- diagnosable when it fails;
- measurable when performance matters;
- compatible with the roadmap without requiring predictable near-term rewrites;
- intentionally scoped rather than superficially broad.

It does **not** mean:

- maximum abstraction;
- maximum genericity;
- implementing every known engine technique;
- adding concurrency everywhere;
- adding third-party frameworks by default;
- solving future phases prematurely.

## 8. Future-phase contract

Every Phase 15+ handoff must explicitly reference this document.

If a future phase proposes a shortcut that conflicts with this standard, the phase plan must either:

1. reject the shortcut; or
2. document a deliberate exception, its impact, and the concrete later migration point.

Untracked “we will fix it later” architecture is not accepted.
