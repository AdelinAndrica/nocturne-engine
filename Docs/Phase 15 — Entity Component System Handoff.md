# Phase 15 — Entity / Component System

> **Status:** READY TO START
>
> **Branch:** `phase-15-entity-component-system`
>
> **Starting baseline:** merged Phase 14 on `master`
>
> **Roadmap scope:** ECS or component model, serialization-ready data layout.
>
> **Hard boundary:** no Phase 16 full scene editing, no Phase 17 scene serialization/save-load, no Phase 18 physics.

## 1. Phase name + objective

**Phase 15 — Entity / Component System**

În roadmap, Phase 15 este explicit **„Entity / Component System — ECS or component model, serialization-ready data layout”**, poziționată intenționat între viewport/editor și Phase 16 scene editing / Phase 17 serialization.

Obiectivul fazei este simplu de formulat: **înlocuim modelul temporar din `World` cu fundația reală pentru obiectele jocului**. Vizual, editorul ar trebui să arate aproape identic la final; diferența importantă va fi arhitecturală.

La final vreau ca un obiect din Nocturne să nu mai fie „un slot în `World` cu Transform + Renderable hard-coded”, ci:

```text
Entity
 ├── TransformComponent
 ├── RenderableComponent
 ├── CameraComponent
 ├── NameComponent
 └── ...future components
```

Gregory descrie runtime object model-ul ca implementarea concretă a modelului de obiecte pe care îl vede editorul și prezintă atât arhitecturi object-centric, cât și property-centric/component-like.

Nystrom motivează Component pattern-ul prin separarea domeniilor: entity-ul devine un container, iar rendering, physics, AI etc. nu trebuie să fie îngrămădite într-o clasă gigantică.

## 2. Key concepts from the books

Gregory merge inclusiv până la un **pure component model**, unde obiectul logic este identificat printr-un ID, iar componentele sunt legate de acel ID; subliniază însă explicit că această arhitectură are trade-off-uri și necesită mecanisme bune de creare, lookup și comunicare.

Pentru Nocturne, vreau să păstrăm și ideea actuală de handle cu generație. Gregory discută problema stale handles și folosirea identității suplimentare pentru ca un handle vechi să nu înceapă accidental să refere un obiect nou care reutilizează același slot.

De asemenea, component data separat pe tip ne permite stocare contiguă și iterare cache-friendly; Gregory prezintă avantajele layout-urilor property-centric / struct-of-arrays în această direcție.

Surse primare:

- Jason Gregory, *Game Engine Architecture (3rd Edition)*, §16.2 — Runtime Object Model Architectures;
- Gregory §16.2.1.6 — Pure Component Models;
- Gregory §16.2.2 — Property-Centric Architectures;
- Gregory §16.5 — Object References and World Queries;
- Bob Nystrom, *Game Programming Patterns* — Component pattern;
- Bob Nystrom, *Game Programming Patterns* — Data Locality discussion where component storage and contiguous homogeneous data are relevant.

## 3. What we implement now

**Design choice (not directly from the book):** pentru Nocturne nu construim acum un archetype ECS complex și nu introducem EnTT. Construim un **lightweight component/property-centric ECS propriu**, care evoluează direct din `World` existent.

Asta se potrivește foarte bine cu ce avem deja. `World` are deja conceptual:

```text
generations[]
alive[]
TransformData[]
RenderableData[]
freeList[]
```

Deci, într-un sens, Phase 10 ne-a lăsat deja o versiune embrionară de ECS. Phase 15 o generalizează.

Ținta arhitecturală:

```cpp
EntityHandle
{
    uint32_t index;
    uint32_t generation;
};

EntityId               // identity persistentă/editor/serialization

EntityRegistry         // create/destroy/alive/generation
ComponentRegistry      // tipurile cunoscute de componente

ComponentStorage<T>
World
```

Primele componente reale:

```cpp
TransformComponent
RenderableComponent
CameraComponent
NameComponent
```

`TransformComponent` va păstra local TRS + hierarchy/runtime world transform.

`RenderableComponent` va păstra mesh/bounds.

`CameraComponent` va prelua datele care acum sunt special-case în `World`.

`NameComponent` ne pregătește pentru Scene Hierarchy din Phase 16.

Gregory cere identitate unică, queries și mecanisme sigure de referire la obiecte ca responsabilități fundamentale ale gameplay foundation.

Vom separa clar:

```text
EntityHandle = referință runtime rapidă, index + generation
EntityId     = identitate stabilă destinată editorului/serializării
```

**Design choice (not directly from the book):** `EntityId` va fi separat de handle-ul runtime, astfel încât Phase 17 să nu serializeze indici/generații temporare.

## 4. Implementation steps

1. Creăm branch-ul Phase 15 din Phase 14 complet și audităm integral toate `.md`-urile precedente, cu accent pe Phase 10 și Phase 14 Completion Report.

2. Extragem lifecycle-ul actual din `World` într-un `EntityRegistry`:
   - create;
   - destroy;
   - generation checking;
   - free-list;
   - persistent identity.

3. Introducem `ComponentStorage<T>` și un registry pentru tipuri de componente.

   **Design choice (not directly from the book):** storage dens per component type, cu lookup entity → component; nu archetype chunks.

4. Migrăm `TransformData` → `TransformComponent`, păstrând:
   - hierarchy;
   - dirty propagation;
   - world matrices.

5. Migrăm `RenderableData` → `RenderableComponent`.

   `BuildRenderQueue()` va itera componentele reale în locul array-urilor hard-coded din `World`.

6. Introducem `CameraComponent` și mutăm camera special-case către modelul de componente.

7. Introducem `NameComponent` și facem Phase 14 hierarchy/selection să lucreze cu entitățile reale.

   Validation scene rămâne, dar devine o scenă compusă din entities + components.

8. Introducem metadata minimă pentru componente:
   - stable component type ID;
   - nume;
   - size/alignment;
   - version.

   Asta ne face **serialization-ready**, dar nu scriem încă fișiere. Reflection/Inspector complet și scene files sunt Phase 16/17.

9. Migrăm `EditorViewportController` astfel încât gizmo/picking/selection să nu mai țină o copie paralelă inutilă a transformului obiectului; sursa autoritativă devine component data.

10. Adăugăm testele și facem regresie completă peste Phase 14.

Un principiu important: nu construim acum un generic `SystemScheduler` sofisticat. Transform update și render extraction pot rămâne explicit ordonate. Gregory atrage atenția că update-ul obiectelor și dependențele între subsisteme pot necesita ordine precisă; nu avem nevoie de un scheduler generic doar pentru a bifa termenul „ECS”.

## 5. Verification checklist

La finalul Phase 15 trebuie să putem bifa:

- [ ] create/destroy entity funcționează și reutilizează sloturile;
- [ ] stale `EntityHandle` devine invalid după destroy/reuse;
- [ ] fiecare entity are un `EntityId` stabil distinct de handle-ul runtime;
- [ ] Add / Remove / Has / Get component funcționează;
- [ ] component removal nu corupe dense storage;
- [ ] entity destruction elimină toate componentele sale;
- [ ] Transform hierarchy produce aceleași world matrices ca înainte;
- [ ] parenting/unparenting rămâne corect;
- [ ] render extraction din `Transform + Renderable` produce același viewport;
- [ ] `CameraComponent` produce aceeași cameră Phase 14;
- [ ] hierarchy/select/picking/gizmos din Phase 14 continuă să treacă;
- [ ] component types pot fi enumerate și au ID/version stabil pentru viitoarea serializare;
- [ ] nu există scene file I/O încă;
- [ ] nu există full editor Add Component/Create Entity UI încă;
- [ ] nu am introdus Phase 16/17/18 features prematur.

## 6. Common pitfalls

Cel mai mare pericol ar fi să transformăm Phase 15 într-un proiect de cercetare ECS:

- archetypes;
- chunk schedulers;
- automatic dependency graphs;
- multithreaded systems;
- reflection completă;
- event bus universal;
- query language;

toate simultan.

Gregory avertizează că nici pure component model-ul nu este automat superior celorlalte arhitecturi, iar Nystrom subliniază că Component pattern-ul introduce complexitate și indirection care trebuie justificate.

Al doilea pericol este să serializăm runtime handles.

**Nu facem asta.**

Gregory diferențiază necesitatea unui ID unic de mecanismul runtime de referire/handle.

Alte reguli:

- nu păstrăm două surse de adevăr pentru transform;
- nu lăsăm editorul să dețină storage separat de runtime;
- nu introducem un `GameObject` gigantic care doar mută problema din `World`;
- nu implementăm scene save/load în această fază;
- nu implementăm Inspector generic complet în această fază;
- nu introducem physics components înainte de Phase 18 doar pentru a demonstra ECS;
- nu rupem contractul Phase 14: child-HWND viewport, un singur `MainLoop`, picking/selection/gizmos validate.

## 7. Next chat handoff

În noul chat spune:

> **Phase 14 este COMPLETE. Începem Phase 15 — Entity / Component System. Studiază integral toate `.md`-urile fazelor anterioare, în special Phase 10 și `Docs/Phase 14 — Completion Report.md`. Pornește din head-ul final Phase 14 și proiectează component model-ul după cărțile furnizate înainte de a modifica codul. Nu introduce Phase 16 scene editing sau Phase 17 serialization înainte de vreme.**

La începutul acelui chat, primul pas este auditul complet al documentației și al actualului `World`, apoi stabilirea exactă a API-ului `EntityHandle / EntityId / EntityRegistry / ComponentStorage` înainte de primul commit de implementare.
