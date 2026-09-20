# Nocturne Engine — Phase 16 Basic Entities, Primitive Meshes, and Analytic Lights

> **Phase:** 16 — Editor Scene Editing + Runtime Reflection
>
> **Status:** IMPLEMENTED — automated validation required before this addendum is considered closed
>
> **Branch:** `phase-16-editor-scene-editing`
>
> **Scope:** Empty Entity, Static Mesh Entity, Camera, Directional Light, Point Light, Spot Light, Plane, Cube, Sphere, Cylinder
>
> **Phase 16 itself remains in completion hardening. This document does not replace the Phase 16 Completion Report gate.**

---

## 1. Objective

Phase 16 now exposes a compact set of reusable authored scene presets and built-in mesh primitives needed for ordinary level construction and for the editor's default graphics showcase.

The implementation deliberately does **not** introduce runtime entity subclasses such as `CubeEntity`, `SpotLightEntity`, or `CameraEntity`. The runtime source of truth remains:

```text
EntityHandle
+ reflected components
+ World semantic APIs
```

The editor names in this document are creation presets: they describe the initial component composition of a newly authored entity and then cease to be a separate runtime type.

---

## 2. Book grounding

The implementation is grounded in the project's primary references without inventing page numbers.

### Jason Gregory — Game Engine Architecture, 3rd Edition

Relevant concepts:

- game objects and runtime object models;
- component-based object composition;
- data-driven game engines;
- game-world editing;
- separation between runtime systems and editor tooling.

Applied here:

- authored objects are runtime entities composed from components;
- the editor creates/mutates the authoritative World instead of maintaining a second scene model;
- authoring operations continue through the existing command/history boundary.

### Frank D. Luna — Introduction to 3D Game Programming with DirectX 12

Relevant concepts:

- local/world/view/projection spaces;
- render items and per-object transforms;
- construction/use of basic geometric primitives;
- normals and lighting;
- directional, point, and spot lights;
- per-frame/per-object graphics data.

Applied here:

- built-in primitive meshes are real mesh resources with positions, normals, tangents, UVs, indices, and a submesh;
- scene geometry is submitted with a world transform;
- normals are transformed with the inverse-transpose linear transform for non-uniform scale;
- the forward preview path consumes directional, point, and spot lights.

### Eric Lengyel — Foundations of Game Engine Development, Volume 2: Rendering

Relevant concepts:

- coordinate spaces and transform hierarchy;
- vertex transformations and the graphics pipeline;
- normal/tangent-space data;
- lighting classes including point, spot, and infinite/directional lights.

Applied here:

- light position/orientation comes from the existing TransformComponent;
- render extraction is data-only and renderer-facing;
- mesh normals are real geometry data, not inferred from editor presentation state.

### Nocturne architecture and Phase 16 contracts

The implementation preserves:

- World/ECS as runtime authority;
- ReflectionRegistry as schema authority;
- EditorSession as editor-session authority;
- command-backed authored mutation;
- tool camera separation from authored Camera components;
- no persistence claims before Phase 17.

---

## 3. Authored creation presets

### Empty Entity

Initial composition:

```text
NameComponent
TransformComponent
```

Purpose:

- grouping;
- hierarchy pivots;
- authored transform parents;
- future gameplay/component composition.

### Static Mesh Entity

Initial composition:

```text
NameComponent
TransformComponent
RenderableComponent
```

A generic Static Mesh Entity starts with no assigned mesh. Mesh assignment uses the existing reflected ResourceHandle picker.

Mesh assignment now authors **mesh + localBounds atomically** through one compound history entry. Bounds are derived from decoded mesh positions before the command executes. A failed decode, non-finite vertex position, allocation failure, or semantic write leaves the previous mesh and bounds unchanged.

### Camera

Initial composition:

```text
NameComponent
TransformComponent
CameraComponent
```

The authored camera is distinct from the editor's tool-owned viewport camera. Existing CameraSystem validation and reflected Inspector editing remain authoritative.

### Directional Light / Point Light / Spot Light

Initial composition:

```text
NameComponent
TransformComponent
LightComponent
```

`LightComponent::type` selects:

- Directional;
- Point;
- Spot.

**Design choice (not directly from the book):** Nocturne represents these three analytic light classes with one reflected `LightComponent` plus a `LightType` enum, rather than three different component storage systems. This keeps one canonical component schema and avoids duplicated authoring/reflection code.

---

## 4. Light component contract

`LightComponent` owns authored light parameters:

```text
type
color
intensity
range
innerConeRadians
outerConeRadians
enabled
```

`TransformComponent` remains authoritative for:

```text
position
orientation
hierarchy/world transform
```

### Invariants

- `LightType` must be Directional, Point, or Spot.
- color channels must be finite and non-negative.
- intensity must be finite and >= 0.
- range must be finite and > 0.
- spot inner cone must satisfy `0 <= inner <= outer`.
- spot outer cone must be less than 90 degrees.
- structural mutation remains main-thread-only under the Phase 15/16 ECS contract.

**Design choice (not directly from the book):** spot cone fields are authored as half-angles in radians and Phase 16 constrains the outer half-angle below 90 degrees.

### Ownership

```text
World
  -> LightSystem
      -> ComponentStorage<LightComponent>
```

No editor object owns runtime light state.

---

## 5. Runtime reflection

The Engine-owned `ReflectionRegistry` now registers:

```text
Nocturne.LightType
Nocturne.Light
```

`Nocturne.LightType` is a reflected enum.

`Nocturne.Light` is a reflected component with semantic accessors for:

- type;
- color;
- intensity;
- range;
- inner cone;
- outer cone;
- enabled.

Inspector edits therefore flow through:

```text
Editor Inspector
-> SetReflectedPropertyCommand
-> reflection metadata
-> World semantic setter
-> LightSystem
```

Invalid values are rejected before state mutation.

**Design choice (not directly from the book):** the reflected non-component `LightType` value type uses a stable TypeId range separate from the 32-bit component compatibility IDs.

---

## 6. Built-in primitive mesh resources

The repository now ships:

```text
Data/Meshes/Primitives/plane.nmsh
Data/Meshes/Primitives/cube.nmsh
Data/Meshes/Primitives/sphere.nmsh
Data/Meshes/Primitives/cylinder.nmsh
```

All four use the existing Phase 11 `.nmsh` runtime format. There is no second primitive-only resource format.

Each contains:

- positions;
- normals;
- tangents;
- UVs;
- uint32 indices;
- one submesh.

Current deterministic geometry:

| Primitive | Vertices | Indices | Local bounds |
|---|---:|---:|---|
| Plane | 4 | 6 | (-1, 0, -1) to (1, 0, 1) |
| Cube | 24 | 36 | (-1, -1, -1) to (1, 1, 1) |
| Sphere | 561 | 2880 | approximately (-1, -1, -1) to (1, 1, 1) |
| Cylinder | 134 | 384 | (-1, -1, -1) to (1, 1, 1) |

**Design choice (not directly from the book):**

- Plane is a 2 x 2 unit XZ plane.
- Cube has half-extent 1 and duplicated vertices per face for hard normals.
- Sphere uses 32 slices and 16 stacks.
- Cylinder uses 32 radial slices with separate cap/side vertices so cap and wall normals stay discontinuous.

These dimensions/tessellation levels are Nocturne defaults, not architectural requirements from the books.

---

## 7. Resource-backed rendering

The old Phase 14 validation path rendered one hard-coded procedural cube for every RenderInstance and ignored `RenderInstance::mesh`.

That scaffolding has been removed from scene geometry rendering.

The current path is:

```text
RenderableComponent.mesh
-> World::BuildRenderQueue()
-> RenderInstance.mesh
-> ResourceManager::GetMesh()
-> IntermediateMesh
-> MeshPass GPU mesh cache
-> actual vertex/index buffers
-> DrawIndexedInstanced
```

### GPU mesh cache ownership

`MeshPass` owns the current DX12-side cache for its lifetime.

Cache key:

```text
ResourceHandle.index + ResourceHandle.generation
```

The cache stores:

- static vertex buffer;
- static uint32 index buffer;
- index count;
- ready/failure state.

A resource that has not finished loading is skipped for that frame. A resource that has failed loading emits a diagnostic instead of substituting unrelated geometry.

**Design choice (not directly from the book):** Phase 16 keeps uploaded mesh resources resident for the lifetime of `MeshPass`. General GPU residency/eviction and hot-reload cache invalidation remain later renderer/resource-management work.

---

## 8. Normal transformation

The resource-backed mesh path consumes mesh normals.

For each render instance it submits:

```text
world
normalWorld
```

where `normalWorld` is derived from the inverse transpose of the affine world transform's linear 3x3 portion.

This preserves correct normal transformation under non-uniform scale.

If the world transform cannot be inverted, the renderer falls back to identity normal transformation rather than producing NaN/Inf GPU data. The Transform authoring path already rejects important non-representable/singular hierarchy operations elsewhere in Phase 16.

---

## 9. Analytic light render extraction

`World::BuildRenderQueue()` now extracts enabled lights in deterministic entity-index order.

Each `RenderLight` contains renderer-facing data only:

- world position;
- normalized world direction;
- color;
- intensity;
- range;
- inner/outer cone cosine;
- light type.

The renderer does not read ECS components directly.

This preserves the existing runtime-to-renderer boundary:

```text
World / gameplay representation
-> RenderQueue
-> renderer
```

---

## 10. Forward preview lighting

The current `Basic.hlsl` supports:

- directional diffuse lighting;
- point diffuse lighting;
- spot diffuse lighting;
- vertex normals;
- world-space normal transform.

This is intentionally a Phase 16 preview-lighting path, not the final material renderer.

**Design choice (not directly from the book):**

- the Phase 16 forward path consumes at most 32 analytic lights per frame;
- World extraction itself remains uncapped;
- point/spot attenuation uses a smooth finite-range squared falloff;
- the preview surface uses one neutral base color plus a small ambient fill;
- the renderer logs once if the forward light budget is exceeded.

PBR materials, image-based lighting, production light attenuation policy, shadowing, and advanced light culling remain assigned to later rendering phases.

---

## 11. Editor creation UX

`Actor -> Create` now exposes:

```text
Create
  Empty Entity
  Static Mesh Entity
  Camera
  Light
    Directional Light
    Point Light
    Spot Light
  Primitive
    Plane
    Cube
    Sphere
    Cylinder
```

All authored creation uses the same `CreateEntityCommand` and `EditorCommandHistory`.

There is no direct UI-to-World mutation path for these creation actions.

`CreateEntityCommand` now accepts an `EditorEntityCreateKind` preset and rolls back the whole new entity if any required component creation fails.

Undo destroys the created entity.

Redo recreates it semantically with a fresh valid runtime handle, preserving the existing Phase 16 generational-handle contract.

---

## 12. Default opening showcase scene

The old Phase 14 bootstrap scene depended on a dummy binary mesh handle and the hard-coded validation cube renderer.

It has been replaced by a transient Phase 16 showcase:

```text
Ground_Plane
Showcase_Cube
Showcase_Sphere
Showcase_Cylinder
Directional Light
Point Light
Spot Light
<tool-owned editor camera>
```

The scene demonstrates in one viewport:

- multiple real mesh resources;
- flat and curved normals;
- hard cube edges;
- cylinder cap/side normal discontinuity;
- transforms and non-uniform scaling;
- directional lighting;
- finite-range point lighting;
- spot-cone lighting.

**Design choice (not directly from the book):** exact object placement, scale, light colors, intensities, and the showcase composition are editor presentation choices.

This is transient startup content only. It does not define the Phase 17 scene file format.

---

## 13. Error and rollback behavior

### Entity creation

Failure while adding any required preset component:

```text
destroy newly created entity
return failure
do not enter successful authored state
```

### Mesh picker

The picker rejects:

- invalid ResourceManager request;
- timeout/failure during validation;
- missing decoded mesh;
- mesh with no valid positions;
- non-finite positions;
- reflected semantic write failure;
- allocation failure.

Successful assignment is one compound history transaction:

```text
Set Renderable.mesh
+ Set Renderable.localBounds
```

If either child operation fails, the compound command uses the existing Phase 16 compensation semantics.

### Renderer

The renderer rejects/skips:

- invalid ResourceHandle;
- resource not ready;
- failed mesh resource;
- empty geometry;
- non-triangle index count;
- out-of-range indices;
- GPU upload failure.

It does not silently draw a fallback cube.

---

## 14. Allocation and performance policy

- primitive files are immutable runtime resources;
- per-frame lights and RenderInstances are extracted through FrameArena;
- GPU mesh data is uploaded once per ResourceHandle generation and cached;
- scene instance upload buffers grow geometrically per frame-in-flight;
- the old fixed 1024-instance truncation has been removed;
- no STL containers were added to the public `MeshPass` API; cache implementation remains private in `.cpp`;
- render and light extraction order is deterministic.

No arbitrary CI timing threshold is introduced. Existing Phase 16 performance and allocation gates remain authoritative.

---

## 15. Automated validation

The Phase 16 headless aggregate now includes light/preset/primitive validation.

Coverage includes:

### Light system

- add;
- duplicate add rejection;
- type;
- color validation;
- intensity validation;
- range validation;
- spot angle validation;
- reflected enum metadata;
- reflected component metadata;
- semantic reflected writes;
- invalid reflected write rejection;
- automatic removal on entity destruction.

### Creation presets

For Empty, Static Mesh, Camera, Directional Light, Point Light, and Spot Light:

- initial component composition;
- static mesh handle/bounds preservation;
- correct light type;
- Undo;
- Redo;
- fresh live entity after recreation;
- invalid preset kind rejection.

### Primitive assets

For Plane, Cube, Sphere, Cylinder:

- file present under content root;
- `.nmsh` decode succeeds;
- deterministic vertex/index counts;
- normals present;
- tangents present;
- UVs present;
- exactly one submesh;
- finite positions;
- expected local bounds;
- indices remain in range.

---

## 16. Explicit deferred scope

This implementation does not claim to complete:

- scene persistence / Save / Open;
- persistent entity IDs;
- prefab assets;
- material assignment workflow beyond existing resource foundation;
- PBR/metallic-roughness rendering;
- textures in the new preview shader;
- shadow maps;
- cascaded directional shadows;
- omnidirectional point-light shadows;
- spot-light shadows;
- clustered/deferred lighting;
- GPU residency eviction;
- asset hot reload;
- reflection probes;
- fog;
- decals;
- runtime gameplay lighting policy.

Those remain later-phase concerns unless the roadmap is explicitly revised.

---

## 17. Production Engineering Standard reconciliation

| Requirement | Result |
|---|---|
| One source of truth | World/ECS + ReflectionRegistry retained |
| Ownership/lifetime explicit | World owns LightSystem; MeshPass owns GPU mesh cache |
| Invalid states defined | light, mesh, preset, and picker validation added |
| Negative paths | headless tests cover invalid light/preset inputs and blob integrity |
| Narrow APIs | semantic World light API; private renderer cache implementation |
| Allocation discipline | FrameArena extraction; geometric per-frame instance capacity |
| Deterministic behavior | entity-index extraction order; deterministic primitive topology |
| Editor history | all user creation uses command history; mesh+bounds is compound |
| No editor second world | hierarchy/inspector continue to project authoritative World |
| Scaffolding removal | hard-coded validation cube scene rendering removed |
| Diagnostics | failed resources, GPU uploads, light overflow, picker failures logged |
| Regression | Phase 15/16 CI aggregate remains the automated gate |
| Deferred scope explicit | final rendering/persistence features listed above |

The Phase 16 manual regression and soak gates remain required before Phase 16 itself can receive a Completion Report.
