# Handoff — Phase 15: Entity / Component System

**Phase 14 este COMPLETĂ și merged în `master`.**

Începem **Phase 15 — Entity / Component System**.

Branch activ:

`phase-15-entity-component-system`

Head documentație Phase 15 la momentul handoff-ului:

`48c9fdefc66ea1adfe3485e0b941995ad3172c10`

## OBLIGATORIU la începutul chat-ului

Înainte de orice design nou sau modificare de cod:

1. Studiază integral toate fișierele `.md` existente pentru fazele anterioare.
2. Acordă atenție specială:
   - `Docs/Production Engineering Standard.md`
   - `Docs/Phase 15 — Entity Component System Handoff.md`
   - `Docs/Phase 14 — Completion Report.md`
   - `Docs/Phase 14 — Implementation Report.md`
   - `Docs/Phase 10 — Scene Representation.md`
   - `Docs/nocturne_engine_architecture.md`
3. Inspectează codul real actual din branch, în special:
   - `Engine/Runtime/World.h`
   - `Engine/Runtime/World.cpp`
   - `Engine/Runtime/SceneObject.h`
   - `Engine/Runtime/Engine.h/.cpp`
   - `Engine/Runtime/MainLoop.h/.cpp`
   - `Engine/Render/RenderQueue.h`
   - `Apps/NocturneEditor/EditorViewportController.h/.cpp`
   - orice cod care consumă `SceneObjectHandle`, transform, hierarchy, renderable sau camera data.
4. Nu presupune că documentația veche este identică cu codul actual. Reconcilierea trebuie făcută din sursa reală.
5. Nu scrie cod înainte să fie clar:
   - ce păstrăm;
   - ce migrăm;
   - ce eliminăm;
   - care sunt invariants;
   - ownership/lifetime;
   - invalidation rules;
   - failure semantics;
   - performance implications.

---

# Quality contract — NON-NEGOTIABLE

`Docs/Production Engineering Standard.md` este obligatoriu.

Nocturne Engine este un produs comercial, nu tutorial, school project, throwaway prototype sau proof of concept.

Tot ceea ce implementăm din Phase 15 înainte trebuie să fie:

**production-grade within the scope of the phase**

Asta înseamnă:

- ownership explicit;
- lifetime explicit;
- invariants documentate;
- invalid-state handling;
- negative-path handling;
- no silent corruption;
- single source of truth;
- API-uri curate și stabile;
- allocator/memory discipline;
- determinism unde contează;
- diagnostics;
- tests;
- stress tests;
- regression coverage;
- performance measurement pe hot paths;
- documentație care reflectă exact codul real;
- fără `TODO` structural ascuns drept soluție finală.

Professional-grade NU înseamnă feature creep.

Regula este:

> Implementăm complet și robust responsabilitatea fazei curente, astfel încât fazele următoare să poată construi peste ea fără să fie obligate să-i înlocuiască fundația.

---

# Primary source of truth

Folosește cărțile furnizate ca sursă principală pentru concepte și arhitectură.

Relevant pentru Phase 15:

## Jason Gregory — Game Engine Architecture, 3rd Edition

- §16.2 — Runtime Object Model Architectures
- §16.2.1.6 — Pure Component Models
- §16.2.2 — Property-Centric Architectures
- §16.5 — Object References and World Queries

## Bob Nystrom — Game Programming Patterns

- Component
- Data Locality

Pentru orice alegere care nu este clar prescrisă de cărți:

**Design choice (not directly from the book)**

Nu inventa citări sau pagini.

---

# Phase 15 objective

Înlocuim modelul temporar / hard-coded actual din `World` cu fundația reală Entity / Component a Nocturne Engine.

Ținta conceptuală:

```text
Entity
 ├── TransformComponent
 ├── RenderableComponent
 ├── CameraComponent
 ├── NameComponent
 └── future components
```

Arhitectura de pornire:

```cpp
EntityHandle
{
    uint32_t index;
    uint32_t generation;
};

EntityId

EntityRegistry
ComponentRegistry
ComponentStorage<T>
World
```

Separare obligatorie:

```text
EntityHandle = referință runtime rapidă, index + generation
EntityId     = identitate stabilă pentru editor / references / future serialization
```

Runtime handles NU se serializează.

---

# Architectural direction

**Design choice (not directly from the book):**

Nu introducem EnTT și nu alegem automat un archetype/chunk ECS doar pentru că este popular.

Nu construim un ECS „lightweight” în sensul de minimalist sau incomplet.

Construim un:

**production-grade component/property-centric runtime object model**

care evoluează din `World` existent.

Storage-ul poate fi dens per component type, dar implementarea finală trebuie decisă după auditul access patterns, invariants, lifetime și performance.

Nu presupune din start că `swap-and-pop`, sparse sets sau orice altă strategie este automat alegerea finală.

Justifică designul înainte de implementare.

---

# Phase 15 scope

## 1. Entity identity

Trebuie să existe o separare robustă între:

- transient runtime handle;
- persistent/stable entity identity.

Trebuie definite:

- creation;
- destruction;
- slot reuse;
- generation checking;
- stale-handle invalidation;
- generation overflow/wrap policy;
- duplicate ID handling;
- lookup by stable `EntityId`;
- behavior pentru invalid handle în Debug și Shipping.

---

## 2. EntityRegistry

Extragem lifecycle-ul entităților din `World`.

Responsabilități:

- create;
- destroy;
- alive/valid checks;
- generations;
- free-list / slot reuse;
- stable IDs;
- ID lookup;
- deterministic iteration unde este necesar;
- cleanup complet.

Entity destruction trebuie să elimine toate componentele asociate.

Nu acceptăm orphan components.

---

## 3. Component model

Introducem mecanisme production-grade pentru:

```cpp
Add<T>()
Remove<T>()
Has<T>()
Get<T>()
TryGet<T>()
```

sau API-ul final echivalent stabilit după design audit.

Pentru fiecare `ComponentStorage<T>` trebuie definite:

- layout;
- entity -> component mapping;
- component -> entity ownership;
- duplicate Add policy;
- Remove absent component policy;
- growth policy;
- compaction policy;
- relocation rules;
- pointer/reference invalidation;
- iteration guarantees;
- destruction semantics;
- allocator ownership.

Hot-path iteration nu trebuie să facă heap allocation per entity/component/per frame.

---

## 4. Components to introduce/migrate

### TransformComponent

Migrează actualul `TransformData`.

Trebuie păstrate și întărite:

- local translation;
- local rotation;
- local scale;
- world transform;
- hierarchy;
- dirty propagation.

Trebuie tratate explicit:

- self-parent rejection;
- ancestor-cycle rejection;
- reparent;
- unparent;
- invalid parent;
- parent destruction;
- child policy la parent destruction;
- dirty propagation;
- deterministic traversal;
- local/world consistency;
- non-uniform scaling.

Transform hierarchy nu are voie să poată intra în ciclu.

### RenderableComponent

Migrează actualul `RenderableData`.

Trebuie să păstreze:

- mesh/resource identity;
- local bounds;
- derived world bounds;
- render extraction;
- culling integration.

Renderer-ul trebuie să continue să consume render data, nu ECS/runtime internals direct.

### CameraComponent

Eliminăm camera special-case din `World` și o mutăm într-un model component-based coerent.

Trebuie păstrate:

- fov;
- aspect;
- near/far;
- active camera semantics necesare runtime-ului;
- Phase 14 editor-camera separation.

Nu trebuie să stricăm editor camera.

### NameComponent

Introduce identitate human-readable pentru editor/tooling.

Nu confunda `NameComponent` cu `EntityId`.

Name-ul nu este persistent identity.

---

# Component metadata

Introducem metadata minimă production-grade:

- stable component type ID;
- canonical type name;
- size;
- alignment;
- version;
- duplicate type-ID detection;
- duplicate registration detection;
- deterministic enumeration;
- regulă clară pentru type-ID stability între build-uri.

Scopul este să pregătim corect Phase 16/17.

NU implementăm încă:

- full reflection;
- generic inspector serialization;
- scene-file serializer.

Dar metadata de acum nu trebuie să ne forțeze la redesign în Phase 17.

---

# World responsibilities after Phase 15

`World` nu trebuie să rămână un container monolitic care doar ascunde aceleași arrays hard-coded.

După Phase 15, `World` trebuie să orchestreze:

- entity registry;
- component storages;
- transform update;
- world-level queries;
- render extraction;
- runtime camera selection/state unde este cazul.

`World` nu trebuie să devină din nou sursa unică pentru fiecare component type prin fields hard-coded.

---

# Query semantics

Minimum required:

- entity validity;
- lookup by stable `EntityId`;
- component presence;
- typed component access;
- entity iteration;
- component iteration;
- deterministic ordering unde editor/runtime correctness depinde de ordine.

Nu introduce generic query language dacă nu există nevoie concretă.

---

# Threading contract

**Design choice (not directly from the book):**

Phase 15 structural mutation rămâne single-thread-owned până când concurrency semantics sunt proiectate explicit.

Asta include:

- entity create/destroy;
- component add/remove;
- hierarchy mutation.

Nu adăuga multithreaded ECS mutation „pentru profesionalism”.

Concurrency fără ownership și synchronization contract nu este professional-grade.

---

# Performance contract

Nu optimiza orb.

Dar trebuie să existe măsurători înainte de completion.

Construiește workload reprezentativ pentru:

- entity create;
- entity destroy;
- repeated slot reuse;
- component add/remove;
- component lookup;
- entity/component iteration;
- transform hierarchy update;
- render extraction;
- allocation behavior.

Orice O(n²) evident pe hot path trebuie:

- eliminat;
- sau justificat explicit.

Nu accepta per-frame heap churn evident.

---

# Tests required

Trebuie testate minim:

- create valid entity;
- destroy valid entity;
- destroy invalid entity;
- destroy twice;
- slot reuse;
- stale handle after reuse;
- stable ID lookup;
- duplicate ID behavior;
- Add component;
- duplicate Add;
- Has/Get/TryGet;
- Remove;
- Remove absent component;
- entity destruction with many components;
- storage growth;
- storage compaction;
- relocation/invalidation semantics;
- shutdown with live entities;
- transform parent/child;
- reparent;
- unparent;
- self-parent rejection;
- deep hierarchy;
- cycle attempt rejection;
- parent destruction semantics;
- non-uniform scale;
- renderable extraction;
- camera behavior;
- metadata registration;
- duplicate component type registration.

Folosește unit/integration/stress tests după natura cazului.

---

# Phase 14 regression contract

Phase 15 nu este completă dacă ECS-ul funcționează, dar strică editorul.

Trebuie să continue să treacă:

- DX12 child-HWND viewport;
- resize;
- maximize/restore;
- minimize/zero-size safety;
- editor camera;
- RMB mouse look;
- WASD;
- Q/E;
- Shift speed;
- wheel speed;
- picking;
- nearest-hit picking;
- empty-click deselection;
- viewport ↔ hierarchy selection sync;
- selection outline;
- Ground_Plane outline;
- Move gizmo;
- Rotate gizmo;
- Scale gizmo;
- gizmo hover/active feedback;
- transforms after gizmo use;
- picking after transforms;
- DX12 debug-layer sanity;
- Phase 13 editor UI baseline;
- Nocturne Editor icon/resources.

---

# Hard boundaries

NU introduce în Phase 15:

- Phase 16 full scene editing;
- full generic Inspector;
- complete Add Component UI;
- prefab authoring;
- undo/redo system unless strictly needed by an owned Phase 15 operation;
- Phase 17 scene serialization;
- savegame;
- serializer implementation;
- Phase 18 physics/collision;
- production animation;
- production audio;
- scripting runtime;
- AI/navigation;
- universal event bus;
- archetype scheduler without demonstrated requirement;
- automatic system dependency graph;
- generic multithreaded ECS scheduler;
- PIE.

Nu construi sisteme doar pentru că sunt asociate generic cu termenul „ECS”.

---

# Phase 15 completion gate

Phase 15 NU este COMPLETE până când:

- [ ] architecture audit este documentat;
- [ ] final entity/component model este justificat;
- [ ] ownership/lifetime sunt explicite;
- [ ] `EntityId` și `EntityHandle` sunt separate corect;
- [ ] stale handles sunt imposibil de confundat cu entități reutilizate;
- [ ] generation overflow policy este definită;
- [ ] entity destruction elimină toate componentele;
- [ ] component storage invariants sunt documentate;
- [ ] Add/Remove/Has/Get/TryGet semantics sunt stabile;
- [ ] storage growth nu corupe mappings;
- [ ] compaction nu corupe reverse lookup;
- [ ] transform cycles sunt imposibile;
- [ ] invalid parenting este gestionat;
- [ ] transform hierarchy produce aceleași rezultate ca înainte;
- [ ] `RenderableComponent` produce același viewport;
- [ ] `CameraComponent` păstrează comportamentul runtime;
- [ ] `NameComponent` este integrat fără a deveni identity;
- [ ] metadata are stable type ID + version;
- [ ] duplicate metadata registration este detectată;
- [ ] hot paths nu au heap churn inutil;
- [ ] stress/performance baseline există;
- [ ] tests negative-path există;
- [ ] Debug diagnostics nu indică corruption/leaks în noul lifecycle;
- [ ] toate Phase 14 regressions trec;
- [ ] documentația reflectă exact codul;
- [ ] Implementation Report este actualizat;
- [ ] Completion Report este creat;
- [ ] toate cerințele aplicabile din `Docs/Production Engineering Standard.md` trec.

---

# How to start this chat

Primul răspuns NU trebuie să scrie cod imediat.

Mai întâi:

1. Citește integral toate `.md`-urile anterioare.
2. Citește `Docs/Production Engineering Standard.md`.
3. Inspectează implementarea curentă a `World`.
4. Inspectează toate call-site-urile `SceneObjectHandle`.
5. Inspectează Phase 14 editor ↔ runtime integration.
6. Desenează actualul ownership/data flow.
7. Identifică technical debt relevant pentru Phase 15.
8. Compară cel puțin:
   - actual model;
   - dense component storage;
   - sparse mapping / alternative relevantă;
   - pure component implications din Gregory.
9. Propune arhitectura finală Phase 15.
10. Definește:
    - invariants;
    - failure semantics;
    - invalidation;
    - memory layout;
    - update ordering;
    - API contract;
    - tests;
    - migration plan.
11. Abia după aprobarea/reconcilierea designului cu codul real, începe implementation step 1.

Nu presupune designul doar pentru că apare în handoff.

Dacă auditul arată că o parte trebuie schimbată pentru o fundație comercială mai solidă, explică exact de ce, citează sursa relevantă și marchează orice decizie proprie ca:

**Design choice (not directly from the book)**

Branch-ul de lucru este:

`phase-15-entity-component-system`