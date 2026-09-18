# Handoff — Phase 16: Editor Scene Editing

> **Phase 15 — Entity / Component System: COMPLETE**
>
> Phase 16 este guvernată obligatoriu de standardul:
>
> `Docs/Production Engineering Standard.md`
>
> și de contractul specific:
>
> `Docs/Phase 16 — Professional Grade Implementation Contract.md`

## 1. Punctul de pornire

Phase 15 a stabilit fundația runtime production-grade pentru entități și componente.

Modelul runtime canonic este acum:

- `noc::EntityHandle` generational și tranzitoriu;
- `EntityRegistry`;
- `ComponentStorage<T>` dense/sparse;
- `ComponentTypeMetadata` explicit;
- `TransformComponent`;
- `RenderableComponent`;
- `CameraComponent`;
- `NameComponent`;
- `World` ca owner/orchestrator;
- render extraction din component data;
- editor/runtime single source of truth.

Nu există un al doilea World autoritar în editor și nu există un model ECS concurent.

Renderer-ul consumă `RenderQueue` extras și nu deține component state mutabil.

## 2. Documente obligatorii la începutul Phase 16

Înainte de orice design sau cod nou, studiază integral toate fișierele `.md` din fazele anterioare.

Acordă atenție specială:

1. `Docs/Production Engineering Standard.md`
2. `Docs/Phase 16 — Professional Grade Implementation Contract.md`
3. `Docs/Phase 16 — Editor Scene Editing Implementation Checklist.md`
4. `Docs/Phase 15 — Entity Component System Architecture.md`
5. `Docs/Phase 15 — Implementation Report.md`
6. `Docs/Phase 15 — Test and CI Validation Report.md`
7. `Docs/Phase 15 — Completion Report.md`
8. `Docs/Phase 14 — Completion Report.md`
9. `Docs/Phase 14 — Editor Rendering Viewport Architecture Guide.md`
10. `Docs/nocturne_engine_architecture.md`

După documentație, inspectează codul real înainte de a propune arhitectura Phase 16.

## 3. Grounding principal din cărți

### Jason Gregory — Game Engine Architecture, 3rd Edition

- §1.7.5 — **Some Approaches to Tool Architecture**:
  editorul poate folosi framework-ul/runtime-ul comun pentru a evita două reprezentări independente ale acelorași date.
- §15.4 — **The Game World Editor**:
  world editor-ul este o parte importantă a unui engine comercial.
- §15.4.1 — **Typical Features of a Game World Editor**:
  vizualizare, navigație și authoring real al world-ului.
- §15.4.1.7 — **Object Placement and Alignment Aids**:
  position/orientation/scale sunt proprietăți tratate special de world editor prin manipulatoare dedicate.
- §15.4.1.10 — **Rapid Iteration**:
  timpul dintre editare și observarea rezultatului trebuie să fie scurt și potrivit frecvenței operației.
- §15.4.1.9 — **Saving and Loading World Chunks**:
  persistence este necesară unui editor complet, dar în roadmap-ul Nocturne implementarea fișierelor este Phase 17.

### Bob Nystrom — Game Programming Patterns

- **Command** — în special secțiunea **Undo and Redo**:
  comenzile pot reprezenta modificări concrete, pot implementa `undo()`, iar istoricul poate suporta multiple niveluri de undo/redo.
  Nystrom menționează explicit level editor-ele ca un caz în care undo este esențial.

### Eric Lengyel — Foundations of Game Engine Development, Volume 2

- §5.4.2 — **Transform Hierarchy**:
  relațiile parent/child formează arborele transformărilor și rămân fundamentul semanticii de reparenting.

Orice politică specifică Nocturne care nu este prescrisă direct de sursele de mai sus trebuie etichetată:

**Design choice (not directly from the book)**

## 4. Contracte arhitecturale care nu se negociază

- Shell-ul activ rămâne `EditorShellV3`.
- Nu readucem `EditorShell` / `EditorControls` istorice ca autoritate.
- Există un singur runtime `World`.
- Editorul citește și modifică același World.
- `EntityHandle` rămâne identitate runtime tranzitorie.
- Nu serializăm `index/generation`; persistent identity rămâne Phase 17.
- Renderer-ul continuă să consume date extrase.
- Structural ECS mutation rămâne main-thread-owned.
- Există un singur `MainLoop`.
- Nu introducem un al doilea editor loop.
- Nu folosim STL în public engine headers fără o revizie arhitecturală explicită.
- Phase 13/14 visual baseline trebuie păstrat în afara modificărilor pe care Phase 16 le deține explicit.

## 5. Obiectivul Phase 16

Transformăm editorul dintr-un viewport de validare cu obiecte fixe într-un **world editor real** peste modelul Phase 15.

La finalul fazei, utilizatorul trebuie să poată authora scena în memorie prin:

- create entity;
- delete entity;
- duplicate entity/subtree conform semanticii documentate;
- rename;
- reparent / unparent;
- add/remove componente suportate;
- editarea proprietăților componentelor;
- Scene Hierarchy reală;
- Inspector real;
- selecție sincronizată hierarchy ↔ viewport ↔ inspector;
- Move / Rotate / Scale integrate cu edit history;
- Local / World transform orientation conform contractului Phase 16;
- undo/redo pentru toate operațiile de authoring deținute de fază;
- diagnostics utile pentru operații respinse/eșuate.

Nu implementăm încă scene-file persistence.

## 6. Baseline-ul real care trebuie migrat

În codul actual există încă scaffolding Phase 14/15:

- `EditorViewportController::validationObjects_[4]`;
- `selectedIndex_`;
- mapping-uri hierarchy-row → validation-array index;
- Scene Hierarchy pictată pentru un set fix;
- `EditorShellV3::PopulateScene_()` cu rows de prezentare/stub;
- Inspector-ul este încă placeholder;
- toolbar Undo/Redo este încă stub;
- validation scene este creată programatic în `PrepareScene()`;
- editor camera este încă parte din validation setup.

Phase 16 trebuie să elimine autoritatea acestor constante fără să strice viewport-ul validat.

## 7. Editor session și authored world

**Design choice (not directly from the book):**

Phase 16 introduce conceptul de **Editor Session** ca owner al stării exclusiv editoriale:

- selection;
- active tool;
- transform orientation;
- command history;
- transient interaction state;
- set-ul mic de tool-owned entities;
- scene-dirty state;
- diagnostics/editor notifications.

Editor Session nu deține duplicate ale componentelor runtime.

World rămâne autoritatea pentru entity/component state.

## 8. Entități authored vs tool-owned entities

**Design choice (not directly from the book):**

Camera folosită pentru navigarea viewport-ului trebuie tratată ca **tool-owned editor camera**, nu ca authored scene entity pe care utilizatorul o poate șterge accidental.

Phase 16 trebuie să separe conceptual:

- entități authored ale scenei;
- entități/tool state necesare editorului.

Scene Hierarchy afișează authored entities, nu infrastructura internă a editorului.

Nu introducem un al doilea World pentru această separare.

## 9. Politica pentru entitățile create de editor

**Design choice (not directly from the book):**

O entitate creată prin workflow-ul Phase 16 primește implicit:

- `NameComponent`;
- `TransformComponent`.

Acestea formează baseline-ul scene-authoring Phase 16.

Runtime-ul rămâne generic și poate crea entități fără aceste componente; aceasta este o politică de editor, nu o restricție a ECS-ului.

Inspector-ul nu trebuie să permită eliminarea componentelor obligatorii de authoring fără ca arhitectura Phase 16 să definească explicit un workflow pentru entități non-spațiale.

## 10. Scene Hierarchy reală

Scene Hierarchy trebuie să fie derivată din World, nu din `validationObjects_`.

Trebuie să:

- enumere authored entities;
- folosească `EntityHandle` drept identitate;
- reflecte Transform parent/child;
- suporte expand/collapse;
- suporte rename;
- suporte create/delete/duplicate;
- suporte reparent/unparent;
- păstreze selecția sincronizată;
- rezolve stale handles fără crash;
- nu țină component pointers peste structural mutation;
- nu facă allocation churn necontrolat la fiecare frame.

## 11. Semantica delete/duplicate

**Design choice (not directly from the book):**

Pentru un world editor ierarhic, operațiile editoriale trebuie să aibă semantică de subtree documentată.

Direcția preferată pentru Phase 16:

- delete pe un parent șterge authored subtree-ul;
- undo restaurează subtree-ul;
- duplicate copiază subtree-ul și relațiile interne;
- noul subtree primește noi runtime EntityHandles;
- selecția se mută predictibil către copia/restaurarea relevantă.

Aceste snapshot-uri sunt transient editor data, nu scene serialization Phase 17.

Dacă auditul Phase 16 dovedește că o altă semantică este mai potrivită, decizia trebuie documentată înainte de implementare.

## 12. Reparenting

Runtime `World::SetParent()` are contractul Phase 15.

Editor authoring are nevoie de semantică UX explicită.

**Design choice (not directly from the book):**

Reparent/unparent din editor trebuie, implicit, să **preserve world-space pose** pentru obiectul mutat.

Editorul va calcula noul local TRS necesar sub noul parent.

Operația trebuie să:

- respingă self-parent;
- respingă cycle;
- respingă stale parent/child;
- fie atomică;
- fie undoable;
- păstreze world pose în limitele reprezentării TRS;
- raporteze clar când transformarea nu poate fi reprezentată fără shear/degeneracy.

Nu modificăm silent runtime hierarchy semantics pentru a obține UX-ul editorului.

## 13. Inspector

Inspector-ul devine un client al World.

Nu are voie să țină pointeri `TransformComponent*`, `RenderableComponent*` etc. peste structural mutation sau peste frame-uri dacă validitatea nu este garantată.

Inspector-ul trebuie să suporte cel puțin componentele Phase 15:

- Name;
- Transform;
- Renderable;
- Camera.

Trebuie să existe:

- editor descriptors/adapters pe `ComponentTypeId`;
- field validation;
- add/remove component workflow;
- readonly/required states;
- component-specific controls unde proprietatea nu poate fi tratată generic.

**Design choice (not directly from the book):**

Phase 16 nu introduce full runtime reflection doar pentru Inspector.

Se preferă un layer de editor component descriptors/adapters keyed by `ComponentTypeId`, suficient pentru Inspector și extensibil ulterior, fără să transforme Phase 16 în Phase 17 serialization/reflection.

## 14. Transform tools și coordinate space

Gregory §15.4.1.7 susține tratarea specială a translation/rotation/scale prin manipulatoare în viewport.

Phase 16 trebuie să introducă un control clar pentru:

- Local;
- World/Global.

**Design choice (not directly from the book):**

- Move și Rotate trebuie să suporte Local și World orientation.
- Scale trebuie să aibă semantică documentată; world-space non-uniform scale sub hierarchy poate necesita shear, iar Phase 15 Transform stochează TRS fără shear.
- Nu pretindem suport „Global Scale” dacă rezultatul nu poate fi reprezentat corect de modelul Transform.

Gizmo drag trebuie să fie o singură operație undoable, nu sute de intrări în history.

## 15. Command history și transactions

Nystrom, **Command / Undo and Redo**, este grounding-ul principal.

Toate operațiile de authoring deținute de Phase 16 trebuie să intre printr-un command/transaction model.

Exemple:

- create;
- delete;
- duplicate;
- rename;
- reparent;
- add/remove component;
- inspector property edit;
- transform gizmo commit.

Reguli:

- command care eșuează nu intră în history;
- compound command este atomic sau rollback-ează;
- undo/redo validează target-urile;
- o comandă nouă după undo invalidează redo tail;
- history are ownership/lifetime explicit;
- history are memory budget explicit;
- allocation failure nu transformă silent o operație destructivă într-una non-undoable;
- drag continuu este coalesced într-o singură comandă;
- Escape/cancel are semantică explicită.

## 16. Transient editor snapshots

**Design choice (not directly from the book):**

Undo pentru delete/duplicate poate folosi snapshot-uri transient in-memory ale componentelor suportate și hierarchy relations.

Aceste snapshot-uri:

- nu sunt format de scenă;
- nu sunt persistent entity identity;
- nu sunt API de serialization;
- nu se scriu pe disk;
- nu blochează Phase 17 să introducă schema persistentă corectă.

Snapshot-ul trebuie să copieze semantic data, nu pointeri interni în component storage.

## 17. Selection model

Selection identity devine `EntityHandle`, nu validation-array index.

Selection trebuie să:

- se invalideze când entity moare;
- fie sincronizată între hierarchy, viewport și inspector;
- evite dangling component pointers;
- păstreze focus/selection după rename/reparent;
- aibă semantică documentată după delete/undo/redo/duplicate.

**Design choice (not directly from the book):**

Phase 16 poate rămâne single-selection dacă acest lucru este documentat; API-ul nu trebuie să blocheze o extindere ulterioară la multi-selection.

## 18. Prefab prototype boundary

Roadmap-ul Phase 16 menționează prefab prototype.

**Design choice (not directly from the book):**

Phase 16 nu introduce încă prefab files.

Prototype-ul poate fi limitat la infrastructura transientă de subtree snapshot + instantiate, astfel încât Phase 17 să poată adăuga persistence/versioning fără a arunca authoring core-ul.

Orice UI numită „Prefab” trebuie să fie clar etichetată ca prototype până când Phase 17 definește persistence.

## 19. Performance și responsiveness

Gregory §15.4.1.10 pune accent pe rapid iteration.

Phase 16 trebuie să măsoare cel puțin:

- hierarchy rebuild/enumeration;
- selection update;
- inspector refresh;
- command execute/undo/redo;
- create/delete/duplicate subtree;
- reparent;
- large command-history memory;
- gizmo transaction commit.

Representative workloads trebuie să includă cel puțin:

- 1k authored entities;
- 10k authored entities pentru hierarchy/query scalability;
- wide hierarchy;
- deep hierarchy;
- bulk/subtree delete + undo;
- duplicate subtree;
- repeated inspector edits.

Nu stabilim praguri arbitrare înainte de baseline.

## 20. Diagnostics

Editorul trebuie să poată explica operațiile respinse.

Exemple:

- cycle reparent rejected;
- stale selection;
- component already present;
- required component cannot be removed;
- invalid camera parameters;
- invalid numeric Inspector input;
- command allocation failure;
- history budget exhaustion;
- snapshot restore failure;
- asset assignment failure.

Diagnostics trebuie să ajungă în console/status/editor feedback fără spam per-frame.

## 21. CI și regression

Phase 16 nu este completă doar pentru că editorul pornește.

CI trebuie să păstreze toate gate-urile Phase 15 și să adauge teste Phase 16 pentru:

- command history;
- undo/redo;
- create/delete/duplicate;
- subtree snapshot/restore;
- rename;
- reparent/unparent;
- cycle rejection;
- component add/remove;
- inspector mutation adapters;
- selection invalidation;
- hierarchy enumeration;
- stress/performance;
- Editor Debug/Development build;
- solution build.

Manual regression trebuie să includă Phase 13/14/15 behavior.

## 22. Ce este explicit în afara Phase 16

Nu implementăm în Phase 16:

- scene files;
- save/load persistent;
- persistent entity IDs;
- serialized reference fixups;
- schema migration;
- physics;
- animation;
- audio;
- scripting/gameplay;
- AI/navigation;
- PIE;
- generic multithreaded ECS scheduler;
- asset previewers;
- full shipping polish.

Save/Open pot rămâne inactive/stub până când Phase 17 introduce persistence reală.

## 23. Branch recomandat

După integrarea Phase 15 conform workflow-ului repo-ului:

`phase-16-editor-scene-editing`

**Design choice (not directly from the book).**

## 24. Ordinea recomandată pentru Phase 16

1. audit cod/editor state real;
2. architecture document;
3. Editor Session + selection model;
4. command/transaction/history foundation;
5. transient subtree snapshot;
6. dynamic Scene Hierarchy;
7. create/delete/duplicate/rename;
8. reparent/unparent preserve-world;
9. Inspector descriptor layer;
10. Name/Transform Inspector;
11. Renderable/Camera Inspector;
12. component add/remove;
13. Local/World gizmo orientation;
14. gizmo → transaction integration;
15. keyboard/menu/context workflows;
16. diagnostics;
17. tests;
18. stress/performance;
19. Phase 13/14/15 regressions;
20. implementation report;
21. completion report.

Nu sărim direct la UI înainte ca selection + command history + mutation semantics să fie definite.

## 25. Mesaj pentru chat-ul Phase 16

> **Phase 15 este COMPLETE. Începem Phase 16 — Editor Scene Editing. Studiază integral toate fișierele .md din fazele anterioare, cu atenție specială asupra Production Engineering Standard, Phase 16 Professional Grade Implementation Contract, Phase 16 Implementation Checklist, documentației finale Phase 15 și documentației editorului Phase 14. Auditează codul real înainte de a propune arhitectura Phase 16. Păstrează EditorShellV3, un singur World autoritar și contractul Phase 15 de single source of truth. Phase 16 trebuie implementată production-grade: command/transaction-based editing, undo/redo, Scene Hierarchy reală, Inspector real, create/delete/duplicate/reparent, component editing, selection synchronization, Local/World transform orientation, diagnostics, tests, CI și performance baselines. Nu implementa persistence Phase 17 prematur.**
