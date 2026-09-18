# Nocturne Engine — Phase 16 Professional Grade Implementation Contract

> **Status: LOCKED FOR PHASE 16**
>
> **Product target:** editor și engine suficient de robuste pentru un produs comercial Windows și pentru dezvoltarea unui FPS survival-horror shippable.
>
> **Applies with:** `Docs/Production Engineering Standard.md`
>
> **Reflection contract:** `Docs/Phase 16 — Runtime Reflection Architecture Contract.md`
>
> **Phase:** 16 — Editor Scene Editing
>
> Acest document definește cerințele de calitate specifice Phase 16. Nu înlocuiește standardul global; îl specializează pentru scene authoring.

## 1. Principiul de bază

Phase 16 nu este un demo de UI.

La finalul fazei, Scene Hierarchy, Inspector, viewport tools și edit history trebuie să formeze un workflow de authoring coerent peste același runtime World introdus în Phase 15.

Regula de produs este:

> O operație de editor trebuie să fie predictibilă, undoable unde Phase 16 îi deține history-ul, să nu corupă World-ul la input invalid și să nu creeze o a doua autoritate asupra datelor runtime.

Aceasta este **Design choice (not directly from the book)** ca standard de produs.

## 2. Grounding din sursele proiectului

### Gregory — Game Engine Architecture, 3rd Edition

- §1.7.5: tool architecture poate partaja framework/runtime data structures; aceasta evită reprezentări duplicate, deși coupling-ul trebuie controlat.
- §15.4: world editor-ul este o componentă importantă a unui engine comercial.
- §15.4.1: editor-ele de world au workflows standard de visualization/navigation/authoring.
- §15.4.1.7: transform properties și asset linkages necesită editing specializat, nu doar property-grid generic.
- §15.4.1.10: rapid iteration este o cerință importantă a world editor-ului.
- §15.4.1.9: load/save este parte din editor complet, dar roadmap-ul Nocturne îl atribuie Phase 17.
- §16.2.1.6 și §16.2.2: pure component/property-centric object models și data-driven composition fundamentează necesitatea unei scheme coerente pentru tipurile și proprietățile runtime.
- §16.3 și §16.9: world data formats și scripting sunt consumatori viitori ai aceleiași runtime schema.

### Nystrom — Game Programming Patterns

- Command / Undo and Redo: comenzile encapsulează modificări concrete și pot implementa undo; istoricul multi-level și redo-tail invalidation sunt direct aplicabile unui level editor.

### Lengyel — Foundations Vol. 2

- §5.4.2 Transform Hierarchy: relațiile parent/child și transform propagation sunt fundația semanticii de reparenting.

## 3. Runtime Reflection este foundation obligatoriu

Phase 16 nu construiește Inspector-ul peste metadata ad-hoc.

Înainte de scene-authoring UI, implementăm:

`Docs/Phase 16 — Runtime Reflection Architecture Contract.md`

**Design choice (not directly from the book):** Nocturne introduce full runtime reflection pentru toate tipurile engine/game declarate reflectable.

Reflection include:

- engine-wide `ReflectionRegistry`;
- stable type/property/function identities;
- type/property/enum/function/container metadata;
- lifecycle/type operations;
- typed attributes;
- semantic getters/setters;
- component operations și generic component enumeration;
- registry validation/freeze;
- generic reflected values;
- foundation-component reflection.

Reflection este schema canonică reutilizată de:

- Inspector;
- property undo/redo;
- transient snapshots;
- Phase 17 serialization/prefabs;
- Phase 24 scripting;
- debug tooling.

`ComponentRegistry` Phase 15 nu poate rămâne autoritate paralelă.

---

## 4. Single source of truth

World și component storage rămân autoritatea.

Editorul poate păstra:

- selection handle;
- expanded-tree state;
- hover state;
- active tool;
- active transform orientation;
- text-edit buffers;
- command history;
- transient snapshots;
- transient drag state.

Editorul nu poate păstra o copie independentă autoritară de:

- entity component membership;
- local/world transform;
- name;
- renderable;
- camera;
- hierarchy parentage.

Caches/presentation models sunt permise doar dacă invalidation/rebuild este explicit.

## 5. Authoring mutation boundary

**Design choice (not directly from the book):**

După ce command system-ul Phase 16 este activ, toate operațiile user-facing de authoring deținute de Phase 16 trebuie să treacă prin command/transaction layer.

Excepția permisă este preview-ul tranzitoriu în timpul unui gest continuu, precum gizmo drag; acel preview trebuie:

- să pornească dintr-un snapshot exact;
- să poată fi cancel/revert;
- să se închidă într-o singură comandă commit-uită;
- să nu producă sute de undo entries.

Nu introducem cod UI care modifică World direct în zeci de locuri independente.

## 6. Atomicitate

Operațiile compuse trebuie să fie all-or-nothing din perspectiva utilizatorului.

Exemple:

- subtree duplicate;
- subtree delete;
- add component + initialize defaults;
- reparent preserve-world;
- compound Inspector edit.

Dacă pasul N eșuează, sistemul:

- rollback-ează pașii anteriori; sau
- nu comite operația.

Un state parțial fără diagnostic este defect blocker.

## 7. Undo/redo

Undo/redo este feature de bază al Phase 16, nu polish ulterior.

Cerințe:

- multiple undo levels;
- multiple redo levels;
- redo tail eliminat după comandă nouă;
- history clear/reset semantic documentat;
- stale-target handling;
- command failure nu avansează cursorul;
- command memory ownership explicit;
- compound commands;
- coalescing pentru editări continue;
- history budget;
- shortcuts integrate cu focus rules;
- UI enable/disable în funcție de disponibilitate.

History trebuie să testeze create/delete/duplicate/reparent/component edits, nu doar transform numeric edits.

## 8. Entity lifetime și undo

Runtime EntityHandles sunt tranzitorii.

Un command nu poate presupune că entity restaurată după undo primește același handle.

**Design choice (not directly from the book):**

Comenzile destructiv/restorative folosesc snapshot semantic și remapping al handle-urilor rezultate.

Selection/history trebuie actualizate la handle-ul nou rezultat.

Nu „rezervăm” sloturi EntityRegistry pentru a simula persistent identity.

Persistent identity real rămâne Phase 17.

## 9. Editor entity policy

**Design choice (not directly from the book):**

Editor-created scene entities au implicit Name + Transform.

Motivul este că Phase 16 este un scene authoring editor bazat pe hierarchy și transform tools.

Aceasta este policy de tool, nu constraint de World.

Dacă Phase 16 permite non-spatial entities, workflow-ul trebuie proiectat explicit; nu eliminăm Transform accidental și apoi lăsăm hierarchy/gizmo într-un state nedefinit.

## 10. Tool-owned camera

**Design choice (not directly from the book):**

Viewport navigation camera este infrastructură a editorului.

Ea trebuie protejată de:

- delete;
- duplicate;
- reparent;
- serialization viitoare;
- ordinary scene selection.

Scene CameraComponents create de utilizator sunt authored entities separate.

Tool camera poate rămâne în același World pentru render integration, dar editorul trebuie să știe că nu este authored scene content.

## 11. Selection

Selection este identificată prin `EntityHandle`.

Cerințe:

- stale handle auto-clear;
- no selection after destroyed target;
- predictable selection after create;
- predictable selection after duplicate;
- predictable selection after delete;
- undo restore can select restored entity;
- reparent preserves selection;
- hierarchy/viewport/inspector display the same selected identity.

Niciun subsystem nu păstrează un index de row ca identitate a entității.

## 12. Scene Hierarchy

Hierarchy este un projection/view al World-ului.

Cerințe:

- dynamic entity enumeration;
- parent/child ordering;
- stable traversal;
- no duplicate rows;
- no missing live authored entities;
- stale-safe;
- rename live;
- expand/collapse preserved across refresh where possible;
- drag/drop or equivalent explicit reparent workflow;
- feedback pentru invalid drop targets;
- keyboard delete/duplicate/rename where UX defines it.

Hierarchy refresh nu trebuie să re-creeze inutil toate resursele Win32 pe fiecare frame.

## 13. Reparenting UX

**Design choice (not directly from the book):**

Editor reparent păstrează world pose implicit.

Runtime `SetParent()` rămâne mechanism.

Editor layer calculează local TRS adecvat noului parent.

Cazurile cu scale degenerat, non-invertible transform sau shear nereprezentabil trebuie:

- detectate;
- respinse cu diagnostic; sau
- tratate printr-o semantică documentată.

Nu deformăm silent obiectul.

## 14. Inspector architecture

Inspector-ul nu trebuie implementat ca un singur `switch(ComponentTypeId)` gigant care devine imposibil de extins.

**Design choice (not directly from the book):**

Inspector-ul este generic-first și consumă `ReflectionRegistry`.

Pentru fiecare reflected component:

- component operations determină membership/add/remove;
- `TypeMetadata` descrie tipul;
- `PropertyMetadata` descrie proprietățile;
- reflected TypeId/attributes aleg generic property drawer;
- semantic getter/setter aplică schimbarea prin invariants runtime;
- generic property command înregistrează undo/redo.

Custom property drawers și custom component inspectors pot declara:

- specialized visualization;
- multi-property semantic UI;
- asset pickers;
- hierarchy-aware controls.

Acestea sunt extensions keyed by reflection identity și nu redefinește canonical schema.

Inspector-ul nu deține un al doilea registry de component/property metadata.

## 15. Property editing lifecycle

Un field edit trebuie să aibă:

- begin;
- validation;
- preview optional;
- commit;
- cancel;
- undo record.

Numeric fields trebuie să gestioneze:

- text gol;
- parse invalid;
- NaN/Inf;
- out-of-range;
- precision;
- focus loss;
- Enter/Escape.

Nu scriem invalid values temporar în World doar fiindcă userul încă tastează.

## 16. Transform Inspector

Transform Inspector trebuie să editeze local TRS.

Cerințe:

- position X/Y/Z;
- rotation UX documentat;
- scale X/Y/Z;
- invalid numeric rejection;
- scale degeneracy policy;
- transaction/coalescing;
- sync cu gizmo;
- hierarchy-derived world data readonly dacă este afișată.

Dacă rotation UI folosește Euler angles, conversia/quaternion ownership și discontinuity behavior trebuie documentate.

Aceasta este **Design choice (not directly from the book)**.

## 17. Renderable Inspector

Trebuie să suporte cel puțin:

- component presence;
- mesh assignment prin asset/resource mechanism existent;
- enabled;
- local bounds dacă acestea sunt authorable în Phase 16.

Nu introducem asset previewer Phase 23.

Asset assignment failure trebuie să păstreze component state anterior.

## 18. Camera Inspector

Trebuie să suporte:

- FOV;
- aspect policy;
- near;
- far;
- enabled.

Validation folosește aceleași invariants runtime; Inspector nu bypass-ează `CameraSystem` doar pentru că are component pointer.

Editor viewport camera nu este editată ca authored CameraComponent obișnuit.

## 19. Component add/remove

Add/remove trebuie să fie:

- undoable;
- atomic;
- metadata-aware;
- required-component-aware;
- stale-safe;
- duplicate-safe.

Remove Transform/Name trebuie să respecte editor entity policy.

Remove active authored Camera trebuie să nu afecteze editor tool camera.

## 20. Rename

Rename:

- este undoable;
- permite duplicate names, conform Phase 15;
- nu redefinește entity identity;
- respectă NameComponent byte limit;
- nu trunchiază silent;
- hierarchy se actualizează imediat;
- invalid UTF-8/conversion error are diagnostic.

## 21. Delete

Destructive operations trebuie să aibă semantică explicită.

**Design choice (not directly from the book):**

Delete hierarchy entity șterge subtree-ul authored.

Undo restaurează:

- component state;
- hierarchy structure;
- relative ordering relevant;
- selection target.

Tool-owned entities nu pot fi șterse prin scene workflow.

## 22. Duplicate

**Design choice (not directly from the book):**

Duplicate pe hierarchy entity copiază subtree-ul authored.

Copia:

- primește runtime handles noi;
- păstrează component values;
- păstrează internal hierarchy;
- are naming policy explicit;
- este selectată după success;
- este undoable ca o singură operație.

External entity references nu sunt încă rezolvate generic, deoarece persistent/reference serialization este Phase 17.

## 23. Transform gizmo transaction

Gizmo drag:

- capturează original local TRS;
- preview-ează update-uri;
- nu alocă command per mouse move;
- commit la mouse-up;
- cancel la Escape;
- capture-loss semantic explicit;
- undo revine exact la original;
- redo reaplică exact final state.

## 24. Local / World transform orientation

Gregory §15.4.1.7 justifică special handling pentru transforms.

Phase 16 trebuie să aibă un control vizibil pentru tool orientation.

**Design choice (not directly from the book):**

- Move: Local + World.
- Rotate: Local + World.
- Scale: semantică explicită; nu promitem world-space non-uniform scale dacă modelul TRS nu poate reprezenta rezultatul fără shear.

Testele trebuie să acopere:

- root object;
- rotated object;
- parented object;
- rotated parent;
- non-uniform parent scale;
- local/world mode switching.

## 25. Dirty/editor-session state

**Design choice (not directly from the book):**

Editor Session poate menține un `sceneDirty` flag pentru authoring changes.

Phase 16 nu implementează persistence, dar dirty state este util pentru:

- New Scene;
- exit warning;
- viitorul Save din Phase 17.

Dirty flag nu reprezintă o copie a scenei.

## 26. Persistence boundary

Nu există scene-file format în Phase 16.

Transient snapshots pentru undo/prefab prototype folosesc Reflection Core pentru component/property discovery și safe copy unde schema permite.

Snapshot format-ul:
- nu este scene-file format;
- nu este persistent identity;
- nu este salvat pe disk;
- nu folosește runtime handles ca durable references.

Reflection schema însă ESTE infrastructură runtime persistentă între faze și Phase 17 o reutilizează pentru serializer, prefab property addressing și version metadata.

Phase 17 definește file format, persistent Entity IDs, fixups și migration/compatibility policy.

## 27. Performance

Phase 16 este UI/tooling, dar performance rămâne parte din product quality.

Trebuie măsurate:

- ReflectionRegistry startup/freeze;
- TypeId/name/property/function lookups;
- reflected property enumeration/get/set;
- function invocation overhead;
- component enumeration;
- generic Inspector traversal;
- hierarchy model rebuild;
- hierarchy paint/update;
- Inspector refresh;
- selection change;
- command push/undo/redo;
- subtree snapshot;
- delete + undo;
- duplicate;
- reparent;
- gizmo commit.

Workloads:

- 100 entities;
- 1k entities;
- 10k entities;
- 1k siblings;
- deep hierarchy;
- large subtree history.

Niciun timer nu trebuie să forțeze rebuild complet dacă World-ul nu s-a modificat.

## 28. Memory/history budget

History trebuie să aibă limită.

**Design choice (not directly from the book):**

Se definește un budget în bytes și/sau command count.

Reguli:

- oldest history poate fi evicted predictibil;
- current/redo cursor rămâne valid;
- single command prea mare nu devine silent non-undoable;
- memory allocation failure are diagnostic și nu corupe scene state.

Valoarea finală se stabilește după baseline.

## 29. Input/focus

Shortcuts trebuie să respecte focus-ul Win32.

Exemple:

- Delete nu șterge entitate când userul editează text;
- Ctrl+Z într-un text edit trebuie să aibă policy explicită;
- Enter/Escape finalizează/anulează field edit conform controlului;
- gizmo capture nu interferează cu camera capture;
- context-menu commands folosesc aceeași command layer ca toolbar/keyboard.

## 30. Error handling

Failure paths obligatorii:

- stale selection;
- stale command target;
- invalid reparent;
- cycle;
- component duplicate;
- required component removal;
- snapshot allocation failure;
- invalid property input;
- invalid resource assignment;
- invalid camera lens;
- history corruption invariant;
- partial compound command failure.

Nu acceptăm silent state drift.

## 31. Diagnostics

Development diagnostics trebuie să includă:

- command name;
- selected entity index:generation;
- rejected operation reason;
- history depth/cursor;
- transaction begin/end/cancel;
- hierarchy rebuild counts/timing unde este util;
- snapshot entity/component counts pentru destructive commands.

Nu logăm fiecare mouse-move de gizmo.

## 32. Threading

Phase 16 editor authoring mutation rămâne main-thread.

Nu introducem locks în ECS doar pentru UI.

Dacă asset lookup existent este async, result application în World trebuie făcut prin contractul thread-safe deja existent sau marshalled pe main thread.

## 33. Testability

Scene editing core trebuie separat suficient de Win32 încât create/delete/undo/reparent logic să poată fi testată fără click simulation.

UI smoke/manual tests completează, nu înlocuiesc, testele core.

Aceasta este **Design choice (not directly from the book)**.

## 34. Build/CI

Phase 16 trebuie să păstreze toate gate-urile Phase 15.

În plus:

- Phase 16 runtime reflection unit/integration tests;
- schema freeze/validation tests;
- OCP synthetic reflected component extension test;
- Phase 16 headless editor-authoring tests;
- Debug x64 Host;
- Debug x64 Editor;
- Development x64 Engine/Host/Editor;
- solution build;
- warnings review;
- no stale Phase 14 validation code authority.

## 35. Regression

Manual final regression include minimum:

- editor launch;
- content browser;
- console;
- viewport render;
- camera navigation;
- picking;
- selection bounds;
- move/rotate/scale;
- resize;
- hierarchy;
- inspector;
- create/delete/duplicate;
- rename;
- reparent;
- add/remove component;
- undo/redo;
- Local/World transform orientation;
- long-running editor stability.

## 36. Completion rule

Phase 16 nu poate fi marcată COMPLETE dacă:

- full runtime Reflection Core nu trece completion gate-ul din contractul dedicat;
- `ReflectionRegistry` nu este engine-wide și unic;
- `ComponentRegistry`/editor/serializer-style metadata rămâne autoritate paralelă;
- foundation components nu sunt reflectate;
- Inspector necesită modificarea unui central component/property switch pentru fiecare component nou;
- semantic invariants pot fi bypass-ate prin raw reflected writes;
- hierarchy încă depinde autoritar de `validationObjects_[4]`;
- selection identity este încă row/index în loc de EntityHandle;
- Inspector este placeholder;
- undo/redo este stub;
- user-facing authoring mutations bypass-ează history;
- delete/duplicate/reparent nu au failure semantics;
- editor camera poate fi ștearsă ca scene content;
- tests acoperă doar happy path;
- CI nu păstrează Phase 15 gates;
- docs descriu altceva decât codul final.

## 37. Sellable-product criterion

Phase 16 este production-grade în scope atunci când Reflection Core este o infrastructură generică și extensibilă suficient de stabilă pentru Inspector/Phase 17/Phase 24, iar un designer poate lucra o sesiune de authoring reală fără validation-scene constants, fără pierdere accidentală de state și fără ca editorul să corupă World-ul prin operații de bază.

OCP acceptance criterion: un component reflectat nou cu proprietăți generice trebuie să poată apărea în generic Inspector/property tooling prin schema lui, fără modificarea switch-urilor centrale de Inspector/property command infrastructure.

Aceasta nu înseamnă că editorul este shipping-complete după Phase 16.

Persistence, asset previewers, PIE, profiling și shipping polish rămân în fazele lor.
