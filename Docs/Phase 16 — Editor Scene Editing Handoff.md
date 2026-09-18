# What Phase 17 must reuse from Phase 16

Phase 17 scene persistence must build on the existing authoritative reflection and editor authoring seams rather than replace them:

- `ReflectionRegistry` and stable `TypeId` / `PropertyId`;
- reflected component enumeration and semantic property access;
- `OwnedReflectedValue` lifecycle/allocator semantics;
- `ReflectedComponentSnapshot`;
- `ReflectedEntitySubtreeSnapshot`;
- `TransientEntityPrototype` as an in-memory prototype seam only.

**Design choice (not directly from the book):** `TransientEntityPrototype` is not a persistent prefab representation. Phase 17 must define durable scene/prefab identity, file format, reference fixups, migration/version policy and atomic persistence independently while reusing reflection schema and semantic access.

# Handoff — Phase 16: Editor Scene Editing + Runtime Reflection

> **Current implementation note — 2026-09-18**
>
> Phase 16 este deja în dezvoltare avansată. Înainte de continuare citește `Docs/Phase 16 — Current Development Status.md`.
>
> Acest handoff păstrează contractul și ordinea inițială a fazei. Documentul de status consemnează ce este deja implementat, ce este parțial și ce completion gates rămân deschise. **Nu relua foundation work deja implementat și nu considera Phase 16 COMPLETE înainte de închiderea gate-urilor rămase.**

**Phase 15 — Entity / Component System este COMPLETE.**

Începem **Phase 16 — Editor Scene Editing + Runtime Reflection**.

## Instrucțiuni obligatorii la începutul chat-ului

Studiază **integral toate fișierele `.md` existente din fazele anterioare** înainte de a propune arhitectura sau de a modifica implementarea.

Acordă atenție specială următoarelor documente:

- `Docs/Production Engineering Standard.md`
- `Docs/Phase 16 — Runtime Reflection Architecture Contract.md`
- `Docs/Phase 16 — Professional Grade Implementation Contract.md`
- `Docs/Phase 16 — Editor Scene Editing Implementation Checklist.md`
- `Docs/Phase 15 — Entity Component System Architecture.md`
- `Docs/Phase 15 — Implementation Report.md`
- `Docs/Phase 15 — Test and CI Validation Report.md`
- `Docs/Phase 15 — Completion Report.md`
- `Docs/Phase 14 — Completion Report.md`
- `Docs/Phase 14 — Editor Rendering Viewport Architecture Guide.md`
- `Docs/nocturne_engine_architecture.md`

După studierea documentației, **auditează codul real actual** înainte de a proiecta sau implementa Phase 16.

Nu presupune că documentația istorică reprezintă perfect codul actual; verifică implementarea reală.

---

# Contracte arhitecturale care trebuie păstrate

- Engine language: **Modern C++20+**
- Platformă țintă: **Windows**
- Namespace engine: `noc::`
- Shell-ul activ al editorului rămâne:
  - `EditorShellV3`
- **Nu** folosi ca bază shell-urile istorice:
  - `EditorShell`
  - `EditorControls`
- Există un singur runtime `World` autoritar.
- Există un singur `MainLoop`.
- Editorul este client al aceluiași `World`.
- Renderer-ul consumă date extrase și nu deține state-ul componentelor.
- `EntityHandle` rămâne identitate runtime tranzitorie.
- Runtime entity `index/generation` **nu reprezintă persistent identity**.
- Structural ECS mutation rămâne main-thread-owned conform contractului Phase 15.
- Nu introducem STL în public engine headers fără o revizie arhitecturală explicită.
- Păstrăm baseline-ul vizual și interactiv validat în Phase 13/14/15.
- Nu introducem o a doua autoritate pentru scene/component data.

Orice decizie specifică Nocturne care nu este susținută direct de cărțile proiectului trebuie marcată:

**Design choice (not directly from the book)**

---

# Obiectivul principal Phase 16

Phase 16 nu este doar „facem Inspector-ul”.

Phase 16 are două obiective majore:

1. construirea unui **Full Runtime Reflection System production-grade**;
2. construirea unui **real Scene Editing workflow** peste ECS-ul Phase 15 și Reflection Core.

Reflection trebuie implementată **înainte de Inspector-ul generic** și trebuie tratată ca infrastructură centrală a engine-ului.

---

# Milestone 1 obligatoriu — Full Runtime Reflection Core

Implementăm un sistem engine-wide de runtime reflection pentru toate tipurile Nocturne declarate reflectable.

Reflection Core trebuie să fie suficient de complet pentru a deveni schema comună pentru:

- Inspector;
- generic property editing;
- undo/redo property commands;
- transient editor snapshots;
- copy/paste tooling;
- prefab property overrides;
- Phase 17 serialization;
- Phase 24 scripting;
- debug/runtime inspectors;
- viitoare property animation/tooling.

## Reflection Registry

`Engine` trebuie să dețină un singur:

`ReflectionRegistry`

Reflection este engine-wide, nu World-owned și nu Editor-owned.

Conceptual:

```text
Engine
 ├── ReflectionRegistry
 ├── Resource systems
 ├── Input
 ├── Render
 └── World
```

Reflection trebuie inițializată înainte de `World`.

Nu acceptăm registre paralele pentru:

- editor;
- serialization;
- scripting;
- prefabs.

---

# Reflection identity

Trebuie introduse identități stabile pentru:

- `TypeId`
- `PropertyId`
- reflected function identity / `FunctionId`

Aceste IDs:

- nu depind de registration order;
- nu depind de pointer addresses;
- nu depind de RTTI addresses;
- nu depind de memory offsets;
- trebuie să fie deterministic;
- duplicatele trebuie respinse.

`ComponentTypeId` și `ComponentRegistry` din Phase 15 trebuie integrate în noua arhitectură.

`ComponentRegistry` nu trebuie să rămână o a doua autoritate independentă.

Poate deveni:

- facade/view peste `ReflectionRegistry`; sau
- poate fi absorbit complet.

Alegerea trebuie documentată înainte de implementare.

---

# Reflection trebuie să acopere tipuri

Reflection Core trebuie să poată descrie minimum:

- bool;
- signed integers;
- unsigned integers;
- floating point;
- strings;
- enums;
- structs;
- components;
- entity references;
- resource references;
- fixed arrays;
- dynamic sequence/container adapters;
- opaque/custom types;
- reflected functions.

Tipurile matematice Nocturne trebuie integrate coerent:

- `Vec2`
- `Vec3`
- `Vec4`
- `Quat`
- `Mat4`
- `AABB`

---

# Type metadata

Un tip reflectat trebuie să poată expune:

- stable `TypeId`;
- canonical name;
- kind/category;
- version;
- size;
- alignment;
- flags;
- lifecycle operations;
- properties;
- functions;
- enum metadata unde este cazul;
- container metadata unde este cazul;
- component operations unde este cazul.

Metadata lifetime trebuie să fie explicit și stabil după registry freeze.

---

# Lifecycle/type operations

Reflection trebuie să poată opera corect cu tipuri non-triviale.

Minimum:

- default construct;
- destruct;
- copy construct;
- move construct;
- copy assign;
- move assign;
- equality/compare policy;
- reset/default policy.

Nu folosim generic `memcpy` pentru tipuri non-triviale.

Alignment trebuie respectat.

---

# Property Reflection

Fiecare reflected property trebuie să aibă:

- `PropertyId`;
- canonical name;
- owner `TypeId`;
- value `TypeId`;
- flags;
- getter;
- setter sau read-only state;
- validation adapter unde este necesar;
- optional attributes;
- optional default provider.

Reflection nu trebuie să fie doar:

```text
offset + memcpy
```

Trebuie să existe distincția dintre:

```text
Structural Reflection
"Ce proprietate există?"
```

și:

```text
Semantic Mutation
"Cum poate fi modificată legal?"
```

---

# Semantic property mutation

Reflection **nu are voie să bypass-eze invariants runtime**.

Exemple:

Camera properties nu trebuie modificate prin raw memory writes dacă asta bypass-ează:

- FOV validation;
- `near > 0`;
- `far > near`;
- derived projection state.

Transform properties trebuie aplicate prin mecanismele corecte ale `World` / `TransformSystem`.

Hierarchy parent nu trebuie editat generic prin memory offset.

Reparenting trebuie să treacă prin semantic operation.

Resource assignment trebuie validat prin resource mechanisms existente.

Flow-ul corect trebuie să fie aproximativ:

```text
Reflection metadata
        ↓
semantic getter / setter
        ↓
World / Component System
        ↓
validation
        ↓
mutation
```

---

# Property flags și attributes

Reflection trebuie să poată reprezenta lucruri precum:

- `ReadOnly`
- `Transient`
- `Serializable`
- `EditorVisible`
- `ScriptVisible`
- `Deprecated`
- `Required`
- `Hidden`
- `ResourceReference`
- `EntityReference`

Trebuie să existe și typed attributes/annotations pentru lucruri precum:

- display name;
- category;
- tooltip;
- min/max;
- step;
- units;
- angle;
- color;
- multiline;
- resource type constraints;
- editor widget hints;
- serialization alias;
- scripting alias.

Reflection core nu trebuie să depindă de Win32.

---

# Enum Reflection

Enum reflection trebuie să suporte:

- enum `TypeId`;
- underlying type;
- canonical enum name;
- values;
- canonical value names;
- optional display labels;
- flags enum policy.

Inspector, serializer și scripting trebuie să poată reutiliza aceeași metadata.

---

# Struct și nested reflection

Reflection trebuie să suporte recursive/nested types.

Conceptual:

```text
Component
 └── property
      └── Struct
           ├── property
           ├── property
           └── property
```

Trebuie documentată politica pentru recursive type graphs.

---

# Container Reflection

Trebuie să existe un abstraction layer pentru:

- fixed arrays;
- dynamic sequences/containers.

Fără STL în public reflection API.

Container reflection trebuie să poată expune:

- element `TypeId`;
- count;
- const element access;
- mutable element access dacă este permis;
- resize/insert/remove seam unde tipul suportă.

Nu este obligatoriu ca actualele componente Phase 15 să folosească intens containere; infrastructura trebuie însă să existe și să fie testată.

---

# Component Reflection

Reflection pentru componente trebuie să poată expune generic:

- Has;
- Add;
- Remove;
- const access;
- component policy;
- component flags.

Trebuie să putem enumera generic componentele reflectate ale unei entități.

Inspector-ul nu trebuie să conțină:

```cpp
if Transform...
else if Camera...
else if Renderable...
```

pentru fiecare component nou.

---

# Foundation components care trebuie reflectate

În Phase 16 trebuie reflectate complet componentele Phase 15:

## `NameComponent`

Reflectăm proprietatea semantică a numelui.

## `TransformComponent`

Reflectăm authorable properties:

- local translation;
- local rotation;
- local scale.

Nu expunem ca editable generic properties:

- internal parent links;
- child/sibling links;
- dirty internals;
- cached world transform.

Hierarchy este structural operation, nu raw property editing.

## `RenderableComponent`

Reflectăm minimum:

- mesh;
- enabled;
- local bounds dacă politica Phase 16 îl consideră authorable.

Derived world bounds trebuie să fie read-only/transient.

## `CameraComponent`

Reflectăm:

- FOV;
- aspect;
- near;
- far;
- enabled.

Derived matrices trebuie să fie hidden/read-only/transient.

---

# Function Reflection

Full runtime reflection trebuie să includă și funcții explicit reflectate.

Function metadata trebuie să poată descrie:

- stable function identity;
- name;
- return `TypeId`;
- parameters;
- flags;
- invocation adapter;
- const/static/member semantics.

Generic invocation trebuie să valideze:

- parameter count;
- parameter types;
- return type;
- object/context validity.

Phase 24 va decide ce funcții sunt expuse scripturilor.

Nu construim scripting VM în Phase 16.

Reflection mechanism-ul trebuie însă să existe acum, ca Phase 24 să nu inventeze un al doilea native function registry.

---

# Generic reflected values

Trebuie introduse safe abstractions pentru:

- const value view;
- mutable value view;
- owned reflected value.

Owned values trebuie să respecte:

- `TypeId`;
- size;
- alignment;
- lifecycle;
- allocator ownership;
- copy/move semantics.

Nu introducem `std::any` în public engine API.

---

# Reflection Registry lifecycle

Registry-ul trebuie să aibă lifecycle explicit:

```text
Building
   ↓
Freeze / Validate
   ↓
Frozen
   ↓
Shutdown
```

După freeze:

- metadata este read-only;
- metadata pointers sunt stabile;
- lookup-urile nu trebuie să aloce;
- registration nou trebuie respins.

Nu ne bazăm pe global/static initialization order între translation units.

Registration order trebuie controlat de engine startup.

---

# Schema validation

La freeze trebuie să existe un validation pass.

Minimum:

- duplicate IDs;
- duplicate canonical names;
- invalid size/alignment;
- missing referenced TypeIds;
- invalid property owner/value type;
- duplicate PropertyIds;
- invalid enum metadata;
- invalid function signatures;
- invalid lifecycle operations;
- missing component operations;
- invalid Serializable/ScriptVisible combinations;
- invalid getter/setter contracts.

Schema invalidă trebuie să facă freeze-ul să eșueze.

---

# Reflection performance

Trebuie măsurăm:

- registry startup;
- registry freeze;
- `TypeId` lookup;
- canonical-name lookup;
- property lookup;
- function lookup;
- property enumeration;
- generic get/set;
- reflected component enumeration;
- generic function invocation;
- Inspector reflection traversal.

Synthetic workloads:

- 100 reflected types;
- 1k reflected types;
- 10k property lookups;
- 100k reads/lookups unde este relevant.

Nu introducem praguri arbitrare înainte de baseline.

După freeze:

- lookup nu alocă;
- enumeration nu alocă;
- property get nu alocă implicit.

---

# OCP acceptance test

Trebuie creat un synthetic/test component nou.

De exemplu conceptual:

```cpp
struct ReflectionTestComponent
{
    float speed;
    bool enabled;
};
```

Pentru integrarea sa generică trebuie să fie necesare doar:

- definirea componentului;
- reflection registration/schema;
- eventual custom UI doar dacă folosește un property type special.

Nu trebuie modificat:

- central Inspector switch;
- central property-command switch;
- central debug reflection switch;
- viitor central serializer switch.

Dacă trebuie modificate aceste sisteme centrale, Reflection Core nu trece completion gate-ul.

---

# După Reflection Core — Editor Session

După ce Reflection Core este implementat și validat, introducem un `Editor Session` care deține doar editor-specific state:

- selected `EntityHandle`;
- active tool;
- transform orientation;
- command history;
- active transaction;
- hierarchy expanded state;
- editor camera/tool-owned entity state;
- scene dirty state;
- editor diagnostics.

Editor Session nu deține duplicate ale componentelor runtime.

---

# Tool-owned editor camera

Camera de navigație a viewport-ului trebuie tratată ca tool-owned editor state.

Nu trebuie să poată fi:

- ștearsă prin Scene Hierarchy;
- duplicată;
- reparentată prin scene workflow;
- salvată ulterior ca authored scene content.

Authored `CameraComponent` entities sunt separate de editor navigation camera.

Nu introducem un al doilea `World` pentru această separare.

---

# Selection

Selection identity devine:

`EntityHandle`

Nu:

- hierarchy row index;
- validation-array index;
- component pointer.

Selection trebuie să fie sincronizată între:

- Scene Hierarchy;
- Viewport;
- Inspector.

Stale handles trebuie eliminate sigur.

---

# Command / Transaction System

Grounding principal:

Bob Nystrom — **Command / Undo and Redo**.

Toate operațiile user-facing de authoring Phase 16 trebuie să treacă prin command/transaction layer.

Inclusiv:

- create;
- delete;
- duplicate;
- rename;
- reparent;
- unparent;
- add component;
- remove component;
- Inspector property edit;
- transform gizmo edits.

Simple property changes trebuie să poată folosi generic reflection identity:

```text
EntityHandle
Component TypeId
PropertyId
Old reflected value
New reflected value
```

Apply/Undo trebuie să treacă prin semantic reflected setter.

Structural operations rămân commands specializate.

---

# Undo / Redo

Obligatoriu:

- multiple undo levels;
- multiple redo levels;
- redo tail invalidation după command nou;
- failed command nu intră în history;
- compound commands;
- transactions;
- command memory ownership explicit;
- history memory budget;
- predictable eviction policy;
- allocation failure handling;
- stale-target handling;
- toolbar/shortcut enabled states.

Gizmo drag trebuie să producă o singură history entry.

---

# Transient snapshots

Delete/Duplicate/Undo pot folosi transient subtree snapshots.

Snapshot-urile trebuie să folosească Reflection Core pentru:

- component discovery;
- lifecycle-safe value copy;
- reflected component state;

unde schema permite.

Snapshot format:

- nu este scene-file format;
- nu este persistent serialization;
- nu este persistent entity identity;
- nu se scrie pe disk.

Reflection schema însă este permanentă și trebuie reutilizată de Phase 17.

---

# Scene Hierarchy

Eliminăm autoritatea:

- `validationObjects_[4]`;
- `selectedIndex_`;
- fixed hierarchy rows;
- hard-coded object count.

Scene Hierarchy trebuie să derive authored entities din `World`.

Trebuie să suporte:

- create;
- delete;
- duplicate;
- rename;
- reparent;
- unparent;
- expand/collapse;
- selection;
- hierarchy refresh;
- stale entity handling;
- large hierarchy workloads.

Scene Hierarchy trebuie să folosească `EntityHandle`.

---

# Editor-created entity policy

**Design choice (not directly from the book):**

O entitate creată prin Scene Editor primește implicit:

- `NameComponent`;
- `TransformComponent`.

Aceasta este editor policy, nu ECS restriction.

Runtime `World` rămâne capabil să creeze entity fără acestea.

---

# Delete / Duplicate semantics

**Design choice (not directly from the book):**

Delete pe un authored parent șterge întreg authored subtree.

Undo restaurează subtree-ul.

Duplicate copiază subtree-ul și relațiile interne.

Runtime handles noi sunt permise/expected după restore/duplicate.

Nu forțăm `EntityRegistry` să păstreze aceleași handles pentru undo.

Persistent identity va veni în Phase 17.

---

# Reparent / Unparent

Runtime `World::SetParent()` rămâne mechanism.

Editor layer trebuie să implementeze authoring semantics.

**Design choice (not directly from the book):**

Reparent/unparent din editor trebuie implicit să păstreze world-space pose.

Trebuie tratate:

- self-parent;
- cycle;
- stale handles;
- non-invertible parent transforms;
- non-representable TRS/shear;
- non-uniform scale cases.

Operația trebuie să fie undoable și atomică.

---

# Inspector

Inspector-ul trebuie să fie **reflection-driven și generic-first**.

Flow:

```text
Selected Entity
      ↓
enumerate reflected components
      ↓
TypeMetadata
      ↓
PropertyMetadata
      ↓
generic property drawer
      ↓
semantic setter
      ↓
Command / Transaction
      ↓
World
```

Generic drawers minimum:

- bool;
- integers;
- float;
- string;
- enum;
- nested structs;
- vectors;
- resource references;
- readonly fields.

Custom property drawers/component inspectors sunt permise pentru cazuri speciale.

Aceste custom extensions trebuie să refere reflection identities și nu pot redefini canonical schema.

---

# Component add/remove

Reflection component operations trebuie să alimenteze:

- Add Component;
- Remove Component;
- component enumeration.

Operations trebuie să fie:

- undoable;
- stale-safe;
- duplicate-safe;
- required-component aware.

---

# Transform Inspector

Trebuie să permită editarea:

- Position;
- Rotation;
- Scale.

Runtime authority rămâne:

- local TRS;
- quaternion rotation.

Dacă UI folosește Euler angles, conversia și conventions trebuie documentate.

Invalid:

- NaN;
- Inf;
- impossible scale values conform policy;

trebuie respinse.

---

# Local / World Gizmo

Phase 16 trebuie să introducă explicit:

- Local orientation;
- World/Global orientation.

Minimum:

- Move Local;
- Move World;
- Rotate Local;
- Rotate World.

Scale trebuie să aibă semantică documentată deoarece world-space non-uniform scale sub hierarchy poate necesita shear.

Nu pretindem suport pentru o operație care nu poate fi reprezentată corect de modelul TRS Phase 15.

---

# Input / Shortcuts

Trebuie definite minimum:

- Ctrl+Z;
- Ctrl+Y / Ctrl+Shift+Z;
- Delete;
- Duplicate;
- Rename;
- Escape;
- Enter.

Shortcuts trebuie să respecte focus-ul text controls.

Delete nu trebuie să șteargă entity atunci când userul editează un text field.

---

# Diagnostics

Trebuie să putem diagnostica clar:

Reflection:
- duplicate type;
- duplicate property;
- invalid schema;
- invalid function invocation;
- registration after freeze.

Editor:
- stale selection;
- invalid reparent;
- cycle;
- required component removal;
- invalid numeric input;
- invalid camera parameters;
- asset assignment failure;
- command failure;
- history exhaustion;
- snapshot failure;
- rollback failure.

Fără per-frame spam.

---

# CI și teste

Păstrăm toate Phase 15 gates.
Adăugăm Phase 16 tests pentru Reflection:

- type registry;
- property registry;
- enum reflection;
- nested struct reflection;
- container reflection;
- lifecycle ops;
- semantic getters/setters;
- generic values;
- component reflection;
- component enumeration;
- function reflection;
- function invocation;
- registry freeze;
- invalid-schema rejection;
- foundation components;
- OCP synthetic component;
- stress/performance.

Și Scene Editor tests pentru:

- command history;
- transactions;
- create;
- delete;
- duplicate;
- rename;
- reparent/unparent;
- add/remove component;
- generic property editing;
- selection invalidation;
- subtree snapshot;
- hierarchy enumeration;
- Local/World gizmo transactions.

CI trebuie să păstreze:

- Host Debug x64;
- Editor Debug x64;
- Engine Development x64;
- Host Development x64;
- Editor Development x64;
- solution Debug x64;
- Phase 15 tests;
- Phase 16 tests.

---

# Performance / Stress

Reflection:

- 100 / 1k types;
- 10k property lookups;
- 100k lookups/reads unde este util;
- no frozen-registry allocation on lookup.

Editor:

- 100 entities;
- 1k entities;
- 10k entities;
- wide hierarchy;
- deep hierarchy;
- 1k subtree delete/undo;
- 1k subtree duplicate;
- command-history stress;
- Inspector traversal;
- selection update;
- gizmo transaction commit.

Timings sunt baseline observations până avem suficiente date pentru budgets reale.

---

# Explicit NU implementăm în Phase 16

Reflection este implementat acum, dar **NU** implementăm încă:

- scene-file persistence;
- Save/Load real;
- persistent Entity IDs;
- serialized reference fixups;
- migration executor;
- scripting VM;
- networking/replication;
- property animation system;
- physics;
- animation system;
- audio system;
- AI/navigation;
- PIE;
- asset previewers;
- DLL/type hot reload;
- arbitrary introspection pentru toate tipurile C++/third-party neînregistrate.

Phase 17 trebuie să reutilizeze Reflection Core pentru serialization și prefabs.

Phase 24 trebuie să reutilizeze Reflection Core pentru scripting.

Nu acceptăm un al doilea schema system în fazele respective.

---

# Ordinea obligatorie de implementare

1. Audit complet cod + documentație.
2. Phase 16 architecture document.
3. **Full Runtime Reflection Core.**
4. `ReflectionRegistry`.
5. `TypeId` / `PropertyId` / function identity.
6. Type metadata + lifecycle operations.
7. Primitive/builtin type reflection.
8. Enum reflection.
9. Struct/property reflection.
10. Typed attributes.
11. Generic reflected values.
12. Semantic property access.
13. Container reflection.
14. Component reflection.
15. Generic component enumeration.
16. Function reflection/invocation.
17. Migrare `ComponentRegistry` Phase 15 către reflection authority unică.
18. Reflectare `Name`, `Transform`, `Renderable`, `Camera`.
19. Reflection diagnostics + registry freeze.
20. OCP synthetic component acceptance test.
21. Reflection stress/performance baseline.
22. Generic reflected property command proof.
23. Editor Session.
24. `EntityHandle` selection.
25. Command History / Transactions.
26. Reflection-backed transient snapshots.
27. Dynamic Scene Hierarchy.
28. Create/Rename/Delete/Duplicate.
29. Reparent/Unparent preserve-world.
30. Reflection-driven generic Inspector.
31. Custom property/component extensions unde sunt necesare.
32. Add/Remove components.
33. Local/World gizmos.
34. Gizmo transaction/history integration.
35. Input/shortcuts/context menus.
36. Diagnostics.
37. Phase 16 automated tests.
38. Stress/performance.
39. Phase 13/14/15 regression.
40. Implementation Report.
41. Test and CI Validation Report.
42. Completion Report.
43. Phase 17 handoff.

---

# Phase 16 completion gate

Phase 16 nu poate fi marcată COMPLETE decât dacă:

- Full Runtime Reflection completion gate este PASS;
- există un singur engine-wide `ReflectionRegistry`;
- `ComponentRegistry` nu rămâne authority paralelă;
- stable Type/Property/Function identities există;
- type/property/enum/function/container reflection există;
- lifecycle/type ops funcționează;
- non-trivial reflected types sunt safe;
- semantic property mutation protejează invariants;
- reflected component operations/enumeration funcționează;
- foundation components sunt reflectate;
- generic property commands folosesc reflection;
- OCP synthetic component test trece;
- Inspector-ul este reflection-driven;
- Scene Hierarchy este World-backed;
- selection = `EntityHandle`;
- create/delete/duplicate/rename funcționează;
- reparent/unparent funcționează;
- component add/remove funcționează;
- toate operațiile Phase 16 relevante sunt undoable;
- Local/World gizmo orientation funcționează conform contractului;
- invalid/stale input nu corupe World;
- reflection/editor performance baseline este măsurat;
- Phase 13/14/15 regression trece;
- CI complet trece;
- documentația reflectă codul real;
- nu există structural TODO ascuns drept finished.

---

## Primul milestone al Phase 16

**FULL RUNTIME REFLECTION CORE**

Nu începe Inspector-ul, Scene Hierarchy authoring sau alte sisteme mari înainte ca Reflection Core să fie proiectat, implementat și testat production-grade.

După reflection:

**Editor Session + EntityHandle Selection + Command History + Reflection-backed Transient Snapshot**

Abia apoi trecem la Scene Hierarchy, Inspector și authoring workflows.