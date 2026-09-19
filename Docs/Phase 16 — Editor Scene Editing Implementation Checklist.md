# Phase 16 — Editor Scene Editing — Implementation Checklist complet

> **Status: IN DEVELOPMENT — AUTHORING CORE IMPLEMENTED; COMPLETION HARDENING ACTIVE**
>
> **Current development status:** `Docs/Phase 16 — Current Development Status.md`
>
> **Standard global:** `Docs/Production Engineering Standard.md`
>
> **Contract specific:** `Docs/Phase 16 — Professional Grade Implementation Contract.md`
>
> **Reflection contract:** `Docs/Phase 16 — Runtime Reflection Architecture Contract.md`
>
> **Handoff:** `Docs/Phase 16 — Editor Scene Editing Handoff.md`
>
> **Obiectiv:** transformăm editorul Phase 13/14/15 dintr-un validation shell cu obiecte fixe într-un scene-authoring editor real peste World/ECS Phase 15, fără a implementa prematur persistence Phase 17.
>
> **Important:** checkbox-urile acestui document rămân completion gates contractuale. Ele nu reprezintă singure statusul curent doar pentru că sunt încă `[ ]`. Pentru starea reală implementată/parțială/deschisă, consultă `Docs/Phase 16 — Current Development Status.md`. Phase 16 nu este încă COMPLETE.

## 1. Grounding și reguli de sursă

- [ ] Gregory §1.7.5 este folosit pentru tool architecture/shared runtime data.
- [ ] Gregory §15.4 este folosit pentru rolul world editor-ului.
- [ ] Gregory §15.4.1 este folosit pentru typical world-editor workflows.
- [ ] Gregory §15.4.1.7 este folosit pentru special transform/object placement tools.
- [ ] Gregory §15.4.1.10 este folosit pentru rapid iteration.
- [ ] Gregory §15.4.1.9 este recunoscut ca persistence requirement, dar implementarea este Phase 17.
- [ ] Nystrom Command / Undo and Redo este grounding pentru command history.
- [ ] Lengyel Vol. 2 §5.4.2 rămâne grounding pentru transform hierarchy.
- [ ] Nu inventăm page numbers.
- [ ] Orice politică Nocturne-specifică este etichetată **Design choice (not directly from the book)**.

---

# 2. Audit înainte de cod

- [ ] Confirmăm branch-ul Phase 16.
- [ ] Citim integral toate phase `.md`.
- [ ] Citim Production Engineering Standard.
- [ ] Citim Phase 16 Runtime Reflection Architecture Contract.
- [ ] Citim Phase 16 Professional Grade Implementation Contract.
- [ ] Citim Phase 15 Architecture / Implementation / Test & CI / Completion.
- [ ] Citim Phase 14 editor architecture/completion.
- [ ] Inspectăm `EditorShellV3.h/.cpp`.
- [ ] Inspectăm `EditorViewportController.h/.cpp`.
- [ ] Inspectăm `Apps/NocturneEditor/main.cpp`.
- [ ] Inspectăm `World.h/.cpp`.
- [ ] Inspectăm `EntityRegistry`.
- [ ] Inspectăm component systems și metadata.
- [ ] Inventariem toate UI stubs Phase 13/14 pentru Scene/Inspector/Undo/Redo.
- [ ] Inventariem `validationObjects_[4]`.
- [ ] Inventariem `selectedIndex_`.
- [ ] Inventariem row/index mapping din hierarchy.
- [ ] Inventariem hard-coded labels / counts / camera.
- [ ] Inventariem toate locurile unde editorul modifică direct World.
- [ ] Inventariem shortcut/focus routing.
- [ ] Inventariem Win32 child HWND ownership.
- [ ] Inventariem current scene tree din EditorShellV3 versus hierarchy rendering din viewport controller.
- [ ] Eliminăm riscul de a construi două Scene Hierarchy independente.
- [ ] Definim cine va fi owner-ul Scene Hierarchy UI final.
- [ ] Definim cine va fi owner-ul Inspector UI final.
- [ ] Nu începem feature coding până când authority boundaries sunt clare.

---

# 3. Phase 16 architecture document

Înainte sau odată cu primul milestone:

- [ ] Creăm `Docs/Phase 16 — Editor Scene Editing Architecture.md`.
- [ ] Documentăm ownership.
- [ ] Documentăm Editor Session.
- [ ] Documentăm selection model.
- [ ] Documentăm command/history model.
- [x] Documentăm transaction semantics.
- [ ] Documentăm snapshot semantics.
- [ ] Documentăm authored vs tool-owned entities.
- [ ] Documentăm hierarchy enumeration.
- [ ] Documentăm delete semantics.
- [ ] Documentăm duplicate semantics.
- [ ] Documentăm reparent semantics.
- [ ] Documentăm component add/remove.
- [ ] Documentăm Runtime Reflection architecture.
- [ ] Documentăm migration path ComponentRegistry → ReflectionRegistry.
- [ ] Documentăm Inspector ca reflection consumer.
- [ ] Documentăm Local/World gizmo orientation.
- [ ] Documentăm focus/input policy.
- [ ] Documentăm history memory budget.
- [ ] Documentăm threading.
- [ ] Documentăm Phase 17 persistence boundary.
- [ ] Documentăm temporary scaffolding removal.
- [ ] Marcăm toate design choices non-book.

---


# 3A. Runtime Reflection Core — BLOCKER înainte de Inspector

Autoritate:

`Docs/Phase 16 — Runtime Reflection Architecture Contract.md`

Ownership:
- [ ] `Engine` deține un singur `ReflectionRegistry`.
- [ ] Reflection init precede World init.
- [ ] Reflection shutdown este după World/consumers shutdown conform lifetime documentat.
- [ ] World/Editor sunt non-owning consumers.
- [ ] No global Reflection singleton.
- [ ] ComponentRegistry Phase 15 este migrat/facade, nu autoritate paralelă.

Identity:
- [ ] stable TypeId.
- [ ] invalid TypeId.
- [ ] stable PropertyId.
- [ ] invalid PropertyId.
- [ ] stable FunctionId/identity.
- [ ] IDs nu depind de registration order.
- [ ] IDs nu depind de RTTI pointer/address.
- [ ] duplicate IDs respinse.
- [ ] duplicate canonical names respinse.

Type system:
- [ ] TypeKind.
- [ ] primitive types.
- [ ] String.
- [ ] Enum.
- [ ] Struct.
- [ ] Component.
- [ ] Entity reference category.
- [ ] Resource reference category.
- [ ] fixed array.
- [ ] dynamic sequence/container adapter.
- [ ] Function.
- [ ] Opaque/custom seam.

Type metadata:
- [ ] canonical name.
- [ ] version.
- [ ] size/alignment.
- [x] flags.
- [ ] deterministic enumeration.
- [ ] registry-owned metadata/string lifetime.
- [ ] no temporary descriptor pointers.

Lifecycle/type ops:
- [ ] default construct.
- [ ] destruct.
- [ ] copy construct.
- [ ] move construct.
- [ ] copy assign.
- [ ] move assign.
- [ ] equality/compare policy.
- [ ] reset/default policy.
- [ ] non-trivial type tests.
- [ ] over-aligned type tests.
- [ ] no generic memcpy for non-trivial values.

Properties:
- [ ] PropertyMetadata.
- [ ] owner TypeId.
- [ ] value TypeId.
- [ ] flags.
- [ ] getter.
- [ ] setter/read-only.
- [ ] semantic setter path.
- [ ] optional safe direct-address path.
- [ ] validation adapter.
- [ ] default provider.
- [ ] no raw write bypass pentru Camera invariants.
- [ ] no raw write bypass pentru Transform hierarchy.

Attributes:
- [ ] typed attribute mechanism.
- [ ] display name.
- [ ] category.
- [ ] tooltip.
- [ ] numeric range/step.
- [ ] units.
- [ ] angle/color hints.
- [ ] resource type constraint.
- [ ] serialization/script aliases seam.
- [ ] no Win32 dependency.

Enums:
- [ ] enum TypeId.
- [ ] underlying type.
- [ ] value metadata.
- [ ] duplicate validation.
- [ ] flags enum support/policy.
- [ ] generic lookup/name conversion.

Nested structs:
- [ ] recursive property traversal.
- [ ] Vec2/Vec3/Vec4 policy.
- [ ] Quat policy.
- [ ] AABB policy.
- [ ] cycle/reference handling.

Containers:
- [ ] element TypeId.
- [ ] count.
- [ ] const access.
- [ ] mutable access policy.
- [ ] resize/insert/remove adapter seam.
- [ ] no STL types required by public API.
- [ ] fixed array tests.
- [ ] dynamic sequence synthetic test.

References:
- [ ] ResourceHandle reflected semantically.
- [ ] expected resource type constraint.
- [ ] EntityHandle reflected as transient reference.
- [ ] metadata seam pentru Phase 17 persistent translation.
- [ ] runtime index/generation never marked durable identity.

Components:
- [ ] reflected component operations Has/Add/Remove/Get.
- [x] generic reflected component enumeration per entity.
- [ ] Name reflected.
- [ ] Transform reflected.
- [ ] Renderable reflected.
- [ ] Camera reflected.
- [ ] internal caches/hierarchy links hidden/read-only appropriately.
- [ ] derived fields marked transient/read-only.

Functions:
- [x] FunctionMetadata.
- [x] stable function identity.
- [x] return TypeId.
- [x] parameters.
- [ ] flags.
- [x] invocation adapter.
- [x] type/count validation.
- [x] const/static/member semantics.
- [x] real engine function reflection proof.
- [x] mismatch/failure tests.
- [x] Phase 24 script exposure policy deferred, mechanism implemented.

Generic values:
- [ ] const reflected value view.
- [ ] mutable reflected value view.
- [ ] owned reflected value.
- [ ] allocator ownership explicit.
- [ ] alignment/lifecycle correct.
- [ ] copy/move non-trivial safe.
- [ ] no std::any public dependency.

Registry:
- [ ] deterministic registration.
- [ ] no static-init-order dependency.
- [ ] Building state.
- [ ] Freeze.
- [ ] Frozen read-only state.
- [ ] post-freeze registration rejected.
- [ ] schema validation pass.
- [ ] referenced TypeIds validation.
- [x] registry dump diagnostics.
- [ ] shutdown/leak tests.

Performance:
- [x] lookup by TypeId measured.
- [x] lookup by canonical name measured.
- [x] property lookup measured.
- [x] function lookup/invoke measured.
- [x] property enumeration measured.
- [x] reflected component enumeration measured.
- [x] no allocations on frozen hot lookups.
- [x] synthetic 1k reflected types.
- [x] 10k/100k lookup workloads unde util.
- [x] timings observations, correctness hard gate.

OCP acceptance:
- [ ] synthetic new reflected component.
- [ ] schema registration only pentru generic fields.
- [ ] generic Inspector model îl vede.
- [ ] generic property command îl poate edita.
- [x] debug reflection dump îl vede.
- [ ] NU modificăm central Inspector switch.
- [ ] NU modificăm central property-command switch.
- [ ] NU introducem serializer/scripting schema paralelă.

Reflection milestone:
- [ ] toate gate-urile din Reflection Architecture Contract sunt satisfăcute înainte de full Inspector implementation.

---

# 4. Editor Session

- [ ] Introducem un owner clar pentru editor-session state.
- [ ] Engine/World nu devine owner al UI state.
- [ ] Session deține selected EntityHandle.
- [ ] Session deține active tool.
- [ ] Session deține transform orientation.
- [ ] Session deține command history.
- [ ] Session deține scene dirty state dacă este adoptat.
- [x] Session deține transient transaction state.
- [ ] Session cunoaște tool-owned entities.
- [ ] Session nu deține copii autoritare ale componentelor.
- [ ] Session lifecycle este explicit Init/Shutdown.
- [ ] Shutdown anulează/finalizează tranzacția activă determinist.
- [ ] History este distrus înainte ca World target să devină invalid sau command destructors nu dereferențiază World.
- [ ] No globals.

---

# 5. Tool-owned editor camera

- [ ] Separăm camera de navigație editor de authored scene cameras.
- [ ] Editor camera nu apare ca authored entity în hierarchy.
- [ ] Editor camera nu poate fi delete.
- [ ] Editor camera nu poate fi duplicate.
- [ ] Editor camera nu poate fi reparent prin scene UI.
- [ ] Editor camera nu intră în transient authored snapshots.
- [ ] Editor camera rămâne sursa de view pentru viewport.
- [ ] Scene CameraComponents pot fi create/editate independent.
- [ ] Destroy/reset scene nu distruge accidental editor camera.
- [ ] Phase 17 va putea exclude tool camera din persistence fără hacks.

---

# 6. Selection identity

- [ ] Eliminăm `selectedIndex_` ca entity identity.
- [ ] Selection = `EntityHandle`.
- [ ] Default selection invalid.
- [ ] Selection validează `World::IsAlive`.
- [ ] Destroy selected entity clears/updates selection.
- [ ] Undo delete poate selecta restored entity.
- [ ] Duplicate selectează copia conform contractului.
- [ ] Reparent păstrează selection.
- [ ] Rename păstrează selection.
- [ ] Component mutation păstrează selection.
- [ ] Viewport click schimbă aceeași selection.
- [ ] Hierarchy click schimbă aceeași selection.
- [ ] Inspector citește aceeași selection.
- [ ] No row index stored as identity.
- [ ] No component pointer stored as identity.
- [ ] Stale selection nu produce crash/log spam.

---

# 7. Dynamic authored entity enumeration

- [ ] Eliminăm `kValidationObjectCount` din authoring logic.
- [ ] Eliminăm `validationObjects_[4]` din authoring authority.
- [ ] World enumeration este sursa pentru authored entity discovery.
- [ ] Tool-owned entities sunt filtrate explicit.
- [ ] Deterministic iteration order este documentat.
- [ ] Entity without required editor components are handled deterministically.
- [ ] Hierarchy refresh suportă entity count 0.
- [ ] Hierarchy refresh suportă 1 entity.
- [ ] Hierarchy refresh suportă 10k entities.
- [ ] Destroy during prior frame nu lasă stale rows.
- [ ] Structural version/change notification strategy este definită.
- [ ] Nu facem full rebuild inutil per-frame dacă nu există schimbare.

---

# 8. Scene Hierarchy UI — single authority

- [ ] Decidem dacă final hierarchy este controlul `EditorShellV3::sceneTree_` sau un replacement dedicat.
- [ ] Nu păstrăm simultan două hierarchy UIs autoritare.
- [ ] Row model conține EntityHandle.
- [ ] Row model nu conține component pointers persistente.
- [ ] Root/editor labels nu sunt confundate cu entities.
- [ ] Parent/child indentation reflectă Transform hierarchy.
- [ ] Entity fără parent apare root authored entity.
- [ ] Expand/collapse.
- [ ] Hover.
- [ ] Selected row.
- [ ] Keyboard navigation unde implementat.
- [ ] Scroll.
- [ ] Large hierarchy clipping/virtualization strategy dacă este necesar după baseline.
- [ ] Rename feedback.
- [ ] Component/type icons dacă există metadata suficientă.
- [ ] Duplicate names afișate corect.
- [ ] Invalid UTF-8 fallback diagnostic.
- [ ] No hard-coded `Runtime Objects (4)`.
- [ ] No hard-coded Cube_A/B/C/Ground rows.

---

# 9. Create Entity

**Design choice (not directly from the book): editor-created scene entity defaults to Name + Transform.**

- [ ] Add command pentru create.
- [ ] Create este undoable.
- [ ] Redo recreatează semantic entity.
- [ ] Nu presupunem același runtime handle la redo.
- [ ] Default name policy este documentată.
- [ ] Default transform identity.
- [ ] Parent target optional.
- [ ] Create under selected parent dacă UX decide asta.
- [ ] Parent invalid/stale => clear diagnostic / root fallback conform policy.
- [ ] Selection după create este predictibilă.
- [ ] History push doar după success.
- [ ] Allocation failure nu produce partial entity.
- [ ] Component-init failure rollback-ează entity.
- [ ] Tool camera nu este afectată.
- [ ] Test create 1/100/10k.

---

# 10. Rename Entity

- [ ] Rename folosește NameComponent.
- [ ] Rename este command.
- [ ] Undo restorează exact numele anterior.
- [ ] Redo reaplică numele nou.
- [ ] Duplicate names sunt acceptate.
- [ ] Empty name policy explicit.
- [ ] UTF-8 byte limit respectat.
- [ ] Too-long input rejected.
- [ ] Nu trunchiem silent.
- [ ] Rename in hierarchy și Inspector folosesc aceeași command.
- [ ] Focus loss/Enter semantics.
- [ ] Escape cancel.
- [ ] Invalid/stale entity => no history entry.
- [ ] Hierarchy refresh imediat.

---

# 11. Transient subtree snapshot

- [ ] Definim editor-only snapshot type.
- [ ] Snapshot copiază semantic data, nu component pointers.
- [ ] Snapshot copiază Name.
- [ ] Snapshot copiază Transform local TRS.
- [ ] Snapshot copiază Transform internal parent relationships.
- [ ] Snapshot copiază Renderable.
- [ ] Snapshot copiază Camera.
- [ ] Snapshot păstrează component presence.
- [ ] Snapshot nu păstrează runtime EntityHandle drept persistent identity.
- [ ] Internal snapshot references folosesc snapshot-local IDs/indices.
- [ ] Snapshot poate restaura subtree cu handles noi.
- [ ] Snapshot produce old→new remap.
- [ ] Snapshot excludes tool-owned entities.
- [ ] Snapshot allocation failure este explicit.
- [ ] Snapshot destructor/lifetime testat.
- [ ] Snapshot nu este expus ca scene serialization API.
- [ ] Snapshot format nu este numit stable/persistent.
- [ ] Test non-trivial hierarchy restore.
- [ ] Test missing optional components.
- [ ] Test duplicate names.
- [ ] Test camera component in subtree.
- [x] Leak tests.

---

# 12. Delete Entity/Subtree

**Design choice (not directly from the book): editor delete defaults to authored subtree delete.**

- [ ] Capture subtree snapshot înainte de mutation.
- [ ] Dacă snapshot eșuează, delete nu pornește.
- [ ] Delete children in safe order.
- [ ] Delete parent.
- [ ] World destroy failures handled.
- [ ] Compound rollback policy definită.
- [ ] Delete command = single history entry.
- [ ] Undo restorează entire subtree.
- [ ] Undo restorează internal parent relationships.
- [ ] Undo selection policy.
- [ ] Redo șterge restored subtree.
- [ ] Redo target mapping actualizat.
- [ ] Delete selected root.
- [ ] Delete selected child.
- [ ] Delete deep subtree.
- [ ] Delete wide subtree.
- [ ] Tool-owned entity delete rejected.
- [ ] Invalid/stale selection no-op + diagnostic.
- [ ] Keyboard Delete respectă text-edit focus.
- [x] Context menu Delete folosește aceeași command.

---

# 13. Duplicate Entity/Subtree

**Design choice (not directly from the book): duplicate defaults to authored subtree duplicate.**

- [ ] Capture source snapshot.
- [ ] Instantiate with new EntityHandles.
- [ ] Preserve component presence.
- [ ] Preserve local TRS.
- [ ] Preserve internal hierarchy.
- [ ] Parent copy policy explicit.
- [ ] Naming policy explicit.
- [ ] Select duplicated root.
- [ ] Duplicate este single command.
- [ ] Undo deletes duplicate subtree.
- [ ] Redo recreates duplicate subtree.
- [ ] No source mutation.
- [ ] Tool entity duplicate rejected.
- [ ] Stale source rejected.
- [ ] External references boundary documented.
- [ ] Test one entity.
- [ ] Test hierarchy.
- [ ] Test camera/renderable.
- [ ] Test 1k subtree stress.

---

# 14. Reparent / Unparent

- [ ] Reparent UI workflow definit.
- [ ] Drag/drop hierarchy sau equivalent explicit UX.
- [ ] Drop target highlight.
- [ ] Invalid target feedback.
- [ ] Self-parent reject.
- [ ] Descendant-cycle reject.
- [ ] Stale source reject.
- [ ] Stale target reject.
- [ ] Unparent/root drop.
- [ ] Reparent command.
- [ ] Undo restores original parent.
- [ ] Redo reapplies new parent.
- [ ] Selection preserved.
- [ ] Expanded state preserved where possible.

**Design choice (not directly from the book): editor reparent preserves world pose by default.**

- [ ] Capture pre-reparent world transform.
- [ ] Compute new local transform under new parent.
- [ ] Matrix inverse failure handled.
- [ ] TRS decomposition failure handled.
- [ ] Shear/non-representable transform policy documented.
- [ ] Non-uniform parent scale test.
- [ ] Rotated parent test.
- [ ] Deep hierarchy test.
- [ ] World pose epsilon verification test.

---

# 15. Command interface

- [ ] Command has clear ownership.
- [ ] Execute/apply semantics.
- [ ] Undo semantics.
- [ ] Redo semantics.
- [ ] Command display/debug name.
- [ ] Result/failure propagation.
- [ ] No raw dangling component pointers.
- [ ] Entity handles validated on each apply where relevant.
- [ ] Commands that recreate entities maintain remap.
- [ ] Destructor safe after World shutdown policy.
- [ ] No implicit global history.

---

# 16. Command History

Grounding: Nystrom — Command / Undo and Redo.

- [ ] Multiple undo levels.
- [ ] Multiple redo levels.
- [ ] Cursor semantics.
- [ ] New command after undo discards redo tail.
- [ ] Undo on empty history safe.
- [ ] Redo on end safe.
- [ ] Failed command not pushed.
- [ ] Failed undo leaves cursor consistent.
- [ ] Failed redo leaves cursor consistent.
- [ ] Clear history.
- [ ] History change notification for toolbar enabled state.
- [ ] Command labels pentru diagnostics.
- [ ] History max count/budget.
- [ ] Eviction policy.
- [ ] Large command policy.
- [ ] Allocation failure policy.
- [ ] Unit tests.
- [x] Stress 10k small commands.
- [x] Stress large subtree snapshots.
- [ ] Leak tests.

---

# 17. Transactions / compound commands

- [x] Transaction begin.
- [x] Transaction append.
- [x] Transaction commit.
- [x] Transaction cancel.
- [x] Nested transaction policy explicit.
- [x] Compound command order.
- [x] Undo reverse order.
- [x] Execute failure rollback.
- [x] Rollback failure diagnostic/assert policy.
- [x] Gizmo uses transaction/coalescing.
- [ ] Multi-field Inspector edit can group where UX requires.
- [x] Delete subtree uses one logical command.
- [x] Duplicate subtree uses one logical command.

---

# 18. Gizmo → history integration

- [x] Begin drag captures original local TRS.
- [ ] Mouse move preview does not push history.
- [x] Mouse up commits one command.
- [ ] No movement => no command.
- [ ] Escape reverts original.
- [ ] Capture loss policy.
- [ ] Tool switch during drag policy.
- [ ] Selection change during drag policy.
- [ ] Entity destroyed during drag policy.
- [ ] Undo exact original.
- [ ] Redo exact final.
- [ ] No per-mouse-move heap churn.
- [ ] Existing Phase 14 feel preserved.

---

# 19. Local / World transform orientation

- [ ] UI control visible.
- [ ] State stored in Editor Session.
- [ ] Local mode remains current baseline.
- [ ] World Move implemented.
- [ ] World Rotate implemented.
- [ ] Root entity tests.
- [ ] Rotated entity tests.
- [ ] Parented entity tests.
- [ ] Rotated parent tests.
- [ ] Non-uniform scale parent tests.
- [ ] Toggle during no active drag.
- [ ] Toggle during active drag policy.

Scale:
- [ ] Define Local scale behavior.
- [ ] Decide/define World scale behavior.
- [ ] Do not claim world-scale support if shear cannot be represented.
- [ ] UI communicates limitation if applicable.

---

# 20. Reflection-driven Inspector layer

Reflection schema este autoritatea; editor metadata este doar presentation extension.

- [ ] Inspector enumeră reflected components.
- [ ] Inspector enumeră reflected properties.
- [ ] Generic drawer registry keyed by reflected TypeId/attributes.
- [ ] Bool drawer.
- [ ] integer drawer.
- [ ] float drawer.
- [ ] Vec/struct drawer.
- [x] enum drawer.
- [ ] string drawer.
- [ ] resource reference drawer.
- [ ] readonly display.
- [ ] nested struct traversal.
- [ ] custom property drawer extension.
- [ ] custom component inspector extension.
- [ ] custom extensions referă reflection IDs și NU redefin canonical schema.
- [ ] CanAdd/CanRemove vine din component reflection/editor policy.
- [ ] Validation/apply folosește semantic reflected setters.
- [ ] Unknown reflected type are safe fallback/diagnostic.
- [ ] No giant hard-coded Inspector component switch.
- [ ] No central property-name switch.
- [ ] No Win32 types leak into engine runtime reflection headers.
- [ ] No STL restriction violation in public engine headers.
- [ ] Editor layer poate folosi STL intern.

---

# 21. Inspector selection lifecycle

- [ ] No selection state.
- [ ] Live selection state.
- [ ] Stale selection clears.
- [ ] Selection change destroys/rebinds edit controls safely.
- [ ] Structural component add/remove refreshes Inspector.
- [ ] Component pointer not retained across structural mutation.
- [ ] Tab/focus order.
- [ ] Scroll.
- [ ] Resize.
- [ ] Long values clipped/scrollable.
- [ ] Disabled/read-only field visuals.
- [ ] Error state visuals.

---

# 22. Name Inspector

- [ ] Name field.
- [ ] UTF-8 conversion.
- [ ] 63-byte payload contract.
- [ ] Empty input policy.
- [ ] Duplicate names.
- [ ] Enter commit.
- [ ] Focus-loss commit.
- [ ] Escape cancel.
- [ ] Undo/redo.
- [ ] Hierarchy live refresh.
- [ ] No partial invalid write.

---

# 23. Transform Inspector

- [ ] Position X/Y/Z.
- [ ] Rotation representation selected/documented.
- [ ] Scale X/Y/Z.
- [ ] Numeric parse robust.
- [ ] NaN reject.
- [ ] Inf reject.
- [ ] Locale/decimal behavior considered.
- [ ] Commit/cancel.
- [ ] Coalescing.
- [ ] Undo/redo.
- [ ] Gizmo synchronization.
- [ ] Parent changes reflected.
- [ ] Optional readonly world transform display.
- [ ] Degenerate scale policy.

Euler UI if used:
- [ ] Quaternion↔Euler conversion documented.
- [ ] Wrap/display conventions.
- [ ] Gimbal/discontinuity UX acknowledged.
- [ ] Runtime authority remains quaternion.

---

# 24. Renderable Inspector

- [ ] Add Renderable.
- [ ] Remove Renderable.
- [ ] Enabled.
- [ ] Mesh assignment.
- [ ] Existing resource system used.
- [ ] Invalid resource assignment rejected.
- [ ] Previous mesh retained on failed assignment.
- [ ] Bounds display/edit policy.
- [ ] Undo/redo.
- [ ] Viewport updates immediately.
- [ ] No asset previewer scope creep.

---

# 25. Camera Inspector

- [ ] Add Camera.
- [ ] Remove Camera.
- [ ] FOV edit.
- [ ] Aspect policy.
- [ ] Near edit.
- [ ] Far edit.
- [ ] Enabled edit.
- [ ] Runtime validation reused.
- [ ] Invalid lens rejected.
- [ ] Undo/redo.
- [ ] Scene Camera != Editor Camera.
- [ ] Removing authored active/runtime camera does not kill editor viewport camera.

---

# 26. Add Component UX

- [ ] Add Component button/menu.
- [ ] Enumerate editor-supported component descriptors.
- [ ] Hide already-present components.
- [ ] Required components excluded if already present.
- [ ] Add command.
- [ ] Undo removes added component.
- [ ] Redo re-adds.
- [ ] Defaults explicit.
- [ ] Allocation failure rollback.
- [x] Inspector refresh.
- [ ] Viewport refresh.
- [ ] Diagnostics.

---

# 27. Remove Component UX

- [ ] Remove action per removable component.
- [x] Required components cannot be removed.
- [ ] Remove command captures component state.
- [ ] Undo restores exact component state.
- [ ] Redo removes again.
- [ ] Camera active-state implications handled.
- [ ] Renderable removal updates viewport.
- [ ] Stale entity safe.
- [ ] Diagnostics.

---

# 28. Asset assignment integration

- [ ] Audit existing Content Browser asset identity.
- [ ] Audit ResourceManager handles/path mapping.
- [ ] Decide picker/drag-drop integration.
- [ ] No raw filesystem path stored in RenderableComponent dacă ResourceHandle este authority.
- [ ] Missing asset diagnostic.
- [ ] Wrong asset type diagnostic.
- [ ] Async load state handling dacă aplicabil.
- [ ] Assignment undoable.
- [ ] No Phase 23 previewer scope.

---

# 29. Scene dirty state

**Design choice (not directly from the book).**

- [ ] Decide dacă Phase 16 tracks dirty.
- [ ] Successful authoring command marks dirty.
- [ ] Undo/redo dirty semantics documented.
- [ ] New transient scene reset semantics.
- [ ] Exit warning semantics dacă dirty.
- [ ] Save nu este implementat fals.
- [ ] Dirty != serialized-state hash unless explicitly designed.
- [ ] Phase 17 can adopt/extend without rewrite.

---

# 30. New Scene transient workflow

Dacă Phase 16 îl implementează:

- [ ] Clar că este in-memory scene reset.
- [ ] Tool camera survives.
- [ ] Authored entities cleared.
- [ ] Selection cleared.
- [ ] History cleared.
- [ ] Dirty confirmation policy.
- [ ] No fake Save/Open.
- [ ] No disk I/O pretending to be Phase 17.

Dacă nu este implementat:
- [ ] Stub message updated to say persistence/session reset is deferred explicitly.

---

# 31. Prefab prototype seam

- [x] Nu introducem prefab file.
- [x] Reuse transient snapshot/instantiate mechanism.
- [x] Prototype scope explicitly named.
- [x] No persistent asset ID assumptions.
- [x] No serialized references.
- [x] No false compatibility guarantees.
- [x] Phase 17 handoff states what can be reused.

---

# 32. Input and shortcut policy

- [ ] Ctrl+Z.
- [ ] Ctrl+Y / Ctrl+Shift+Z policy.
- [x] Delete.
- [ ] Ctrl+D.
- [ ] F2 rename if adopted.
- [ ] Escape cancel current edit/drag.
- [ ] Enter commit edit.
- [ ] Shortcuts disabled/routed appropriately during text edit.
- [ ] Camera capture priority.
- [ ] Gizmo capture priority.
- [ ] Hierarchy drag priority.
- [ ] No accidental delete while typing.
- [ ] Toolbar buttons call same command pathways.

---

# 33. Context menus

- [x] Hierarchy context menu.
- [x] Create.
- [x] Rename.
- [x] Duplicate.
- [x] Delete.
- [x] Reparent/unparent where appropriate.
- [x] Add Component location policy.
- [x] Disabled states.
- [x] Same commands as keyboard/toolbar.
- [x] No duplicate mutation implementation.

---

# 34. Error handling matrix

- [ ] stale selected entity.
- [ ] stale command target.
- [ ] create allocation fail.
- [ ] snapshot allocation fail.
- [ ] restore allocation fail.
- [ ] duplicate component.
- [ ] absent component remove.
- [x] required component remove.
- [ ] invalid parent.
- [ ] self parent.
- [ ] cycle parent.
- [ ] non-invertible parent transform.
- [ ] TRS decomposition failure.
- [ ] invalid name.
- [ ] invalid numeric field.
- [ ] invalid camera lens.
- [ ] invalid asset/resource.
- [ ] history budget failure.
- [ ] compound rollback failure.
- [ ] editor tool camera misuse.
- [x] shutdown with active transaction.

Fiecare caz are:
- [ ] return/result policy.
- [ ] log/assert policy.
- [ ] user-visible diagnostic policy.
- [ ] state integrity verification.

---

# 35. Diagnostics / observability

- [ ] Editor console records command failures.
- [ ] Status bar can show relevant error/selection state.
- [ ] Debug command names.
- [ ] History depth/cursor counters.
- [ ] Current selection handle diagnostic.
- [ ] Current tool/orientation diagnostic.
- [ ] Hierarchy entity count.
- [ ] Snapshot entity/component count in debug logs.
- [ ] One-shot history dump optional.
- [ ] No per-frame spam.
- [ ] No per-mouse-move spam.

---

# 36. Threading contract

- [ ] Editor authoring mutations main-thread only.
- [ ] Structural ECS mutations main-thread only.
- [ ] Command execute/undo/redo main-thread only.
- [ ] Inspector commit main-thread.
- [ ] Hierarchy mutation main-thread.
- [ ] Async asset result application marshalled safely if required.
- [ ] No new locks in component storage.
- [ ] Shutdown/cancellation order explicit.

---

# 37. Allocation discipline

- [x] Hierarchy refresh allocations measured.
- [x] Inspector rebuild allocations measured.
- [x] Command allocation ownership explicit.
- [x] Snapshot allocation ownership explicit.
- [x] History memory tracked.
- [x] Gizmo mouse-move hot path no uncontrolled heap allocation.
- [x] Paint path no uncontrolled per-row allocations where avoidable.
- [x] Large hierarchy test.
- [x] Allocation failure tests where practical.

---

# 38. Performance baselines

Măsurăm, nu ghicim:

- [x] select entity latency.
- [x] hierarchy rebuild 100.
- [x] hierarchy rebuild 1k.
- [x] hierarchy rebuild 10k.
- [x] hierarchy traversal wide.
- [x] hierarchy traversal deep.
- [x] Inspector refresh.
- [x] command push.
- [x] undo.
- [x] redo.
- [x] create 1k.
- [x] delete subtree 1k.
- [x] undo delete subtree 1k.
- [x] duplicate subtree 1k.
- [x] reparent.
- [x] gizmo commit.
- [x] history memory after representative editing session.

- [x] Timings logged as observations.
- [x] No arbitrary CI timing threshold before stable baseline.
- [x] Correctness/leaks/allocation invariants remain hard pass/fail.

---

# 39. Unit tests — command history

- [ ] empty undo.
- [ ] empty redo.
- [ ] execute one.
- [ ] undo one.
- [ ] redo one.
- [ ] multiple commands.
- [ ] undo multiple.
- [ ] redo multiple.
- [ ] new command after undo clears redo.
- [ ] failed execute not pushed.
- [ ] failed undo cursor safety.
- [ ] failed redo cursor safety.
- [ ] clear.
- [ ] budget eviction.
- [ ] command destruction.
- [ ] compound command.
- [ ] transaction cancel.
- [ ] leak-free.

---

# 40. Integration tests — scene operations

- [x] create.
- [x] create under parent.
- [x] rename.
- [ ] duplicate leaf.
- [x] duplicate subtree.
- [ ] delete leaf.
- [x] delete subtree.
- [x] undo delete.
- [x] redo delete.
- [x] reparent.
- [x] unparent.
- [x] cycle reject.
- [x] preserve-world reparent.
- [x] add component.
- [x] remove component.
- [x] undo component add/remove.
- [x] inspector transform edit.
- [x] inspector camera validation.
- [ ] asset assignment.
- [x] selection invalidation.
- [x] tool camera protection.

---

# 41. Hierarchy tests

- [x] empty world.
- [x] one root.
- [x] many roots.
- [x] deep hierarchy.
- [x] wide hierarchy.
- [x] expand/collapse model.
- [x] duplicate names.
- [x] rename refresh.
- [x] reparent refresh.
- [x] delete refresh.
- [x] undo restore refresh.
- [x] stale row.
- [x] 10k entity stress.
- [x] deterministic row generation.

---

# 42. Inspector tests

- [x] no selection.
- [x] Name only.
- [x] Transform.
- [x] Renderable.
- [x] Camera.
- [x] multiple components.
- [x] add/remove refresh.
- [x] stale entity.
- [x] invalid text.
- [x] NaN/Inf.
- [x] too-long name.
- [x] invalid lens.
- [x] structural mutation while Inspector open.
- [x] pointer invalidation safety.

---

# 43. Gizmo tests

- [x] move local.
- [x] move world.
- [x] rotate local.
- [x] rotate world.
- [x] scale local.
- [x] parented transform.
- [x] rotated parent.
- [x] non-uniform scaled parent.
- [x] begin/cancel.
- [x] begin/commit.
- [x] undo.
- [x] redo.
- [x] no-op drag produces no history entry.
- [x] entity destroyed during transaction.
- [x] capture lost.
- [x] tool switch.

---

# 44. Manual UI regression

- [ ] EditorShellV3 visual baseline.
- [ ] Menu bar.
- [ ] Toolbar.
- [ ] Scene Hierarchy.
- [ ] Viewport.
- [ ] Inspector.
- [ ] Content Browser.
- [ ] Console.
- [ ] Build/Play panel.
- [ ] Status bar.
- [ ] DX12 viewport render.
- [ ] camera mouse.
- [ ] camera keyboard.
- [ ] picking.
- [ ] selection bounds.
- [ ] resize.
- [ ] create.
- [ ] delete.
- [ ] duplicate.
- [ ] rename.
- [ ] hierarchy drag reparent.
- [ ] add component.
- [ ] remove component.
- [ ] property edit.
- [ ] undo.
- [ ] redo.
- [ ] local/world toggle.
- [ ] no recurring stutter.
- [ ] no log spam.
- [ ] no crash during 15+ minute editing session.

---

# 45. CI

Phase 15 gates remain:

- [x] Host Debug x64.
- [x] Phase 15 tests.
- [x] Editor Debug x64.
- [x] Engine Development x64.
- [x] Host Development x64.
- [x] Editor Development x64.
- [x] solution Debug x64.

Phase 16 adds:

- [x] Runtime Reflection registry/type/property tests.
- [x] enum/struct/container/function reflection tests.
- [x] lifecycle/generic value tests.
- [x] semantic setter/invariant tests.
- [x] foundation component reflection tests.
- [x] OCP synthetic reflected component test.
- [x] reflection stress/perf tests.
- [x] Phase 16 command tests.
- [x] Phase 16 scene-edit integration tests.
- [x] Phase 16 stress/perf tests.
- [x] project paths trigger workflow.
- [x] zero new C++ errors.
- [x] warnings reviewed/no ignored new warnings.
- [x] no test requires interactive desktop unless explicitly manual-only.

---

# 46. Build/project hygiene

- [x] New source files in `.vcxproj`.
- [x] New source files in `.filters`.
- [x] Direct project build remains functional.
- [x] Solution build remains functional.
- [x] No stale historical editor source becomes authority.
- [x] No duplicate Scene Hierarchy implementation remains active.
- [x] No validation-array authority remains.
- [x] Generated editor cache dirs ignored.
- [x] No committed build artifacts.


Evidence note: `NocturneHost.vcxproj.filters` tracks the shared Phase 16 headers used by tests; the active Editor project has no `.filters` file. Historical `EditorShell` / `EditorControls` files remain in the repository for Phase 13 provenance but are no longer compiled by `NocturneEditor.vcxproj`; `EditorShellV3` is the sole active shell authority. Generated `DerivedDataCache/` and MSVC artifacts are ignored and tracked leftovers are removed by this gate. Host Debug `NOC_CONTENT_ROOT` is normalized from the historical machine-specific path to repository-relative `Data`.

---

# 47. Documentation

Înainte de completion:

- [ ] `Phase 16 — Runtime Reflection Architecture Contract.md` actualizat cu implementarea reală.
- [x] `Phase 16 — Editor Scene Editing Architecture.md`.
- [x] Implementation Checklist actualizat cu status.
- [x] Implementation Report.
- [x] Test and CI Validation Report.
- [ ] Completion Report.
- [ ] Phase 17 Handoff.
- [ ] Roadmap update.
- [x] Ownership documented.
- [x] Lifetime documented.
- [x] Selection documented.
- [x] Command/history documented.
- [x] Snapshot documented.
- [x] Reparent semantics documented.
- [x] Runtime Reflection model documented.
- [x] Reflection registry ownership/freeze documented.
- [x] Type/property/function/container schema documented.
- [x] semantic property access documented.
- [x] reflection-driven Inspector extension model documented.
- [x] Local/World gizmo semantics documented.
- [x] Known limitations documented.
- [x] Deferred scope assigned.
- [x] All design choices labeled.
- [x] No invented citations.

---

# 48. Phase 16 completion gate

Phase 16 poate fi marcată COMPLETE numai dacă:

- [x] Runtime Reflection completion gate este PASS.
- [x] Engine deține un singur ReflectionRegistry.
- [x] ComponentRegistry nu rămâne authority paralelă.
- [x] stable TypeId/PropertyId/function identity sunt implementate.
- [x] type/property/enum/function/container reflection este implementată.
- [x] lifecycle/type ops și generic reflected values sunt implementate.
- [x] semantic reflected setters protejează runtime invariants.
- [x] Name/Transform/Renderable/Camera sunt reflectate.
- [x] reflected component enumeration funcționează.
- [x] generic property command folosește reflection.
- [x] OCP synthetic component test trece fără central switch modifications.
- [x] Scene Hierarchy este reală și World-backed.
- [x] `validationObjects_[4]` nu mai este authoring authority.
- [x] selection identity este EntityHandle.
- [x] Inspector nu mai este placeholder.
- [x] create funcționează.
- [x] delete funcționează și este undoable.
- [x] duplicate funcționează și este undoable.
- [x] rename funcționează și este undoable.
- [x] reparent/unparent funcționează și este undoable.
- [x] cycle reparent este respins.
- [x] preserve-world semantics este validată sau alternativa documentată.
- [x] add/remove component funcționează și este undoable.
- [x] Name/Transform/Renderable/Camera sunt inspectabile conform scope.
- [x] gizmo changes intră în history ca o singură operație per drag.
- [x] Undo/Redo funcționează pentru toate operațiile Phase 16.
- [x] redo tail semantics este corect.
- [x] editor camera este protejată ca tool-owned state.
- [x] Local/World transform orientation este implementată conform contractului.
- [x] invalid input nu corupe World.
- [x] stale selection/commands nu corup World.
- [x] command/snapshot memory ownership este explicit.
- [x] performance baseline este măsurat.
- [x] 10k hierarchy workload este inspectat.
- [ ] Phase 13/14/15 regression trece.
- [x] CI complet trece.
- [x] documentation reflectă codul real.
- [x] Implementation Report există.
- [x] Test/CI Report există.
- [ ] Completion Report există.
- [x] Nu există structural TODO ascuns drept finished.
- [x] Phase 17 poate începe fără să înlocuiască authoring core-ul.

---

# 49. Explicit NU implementăm în Phase 16

- [x] **NU** scene-file persistence.
- [x] **NU** Save/Load real.
- [x] **NU** persistent Entity ID.
- [x] **NU** serialized entity reference fixups.
- [x] **NU** schema migration.
- [x] **NU** un al doilea reflection/schema registry pentru Inspector/serializer/scripting.
- [x] **NU** introspecție automată arbitrară a tuturor tipurilor third-party/C++ neînregistrate.
- [x] **NU** compiler/AST toolchain obligatoriu dacă explicit registration satisface contractul production-grade.
- [x] **NU** physics.
- [x] **NU** animation.
- [x] **NU** audio.
- [x] **NU** scripting/gameplay runtime.
- [x] **NU** AI/navigation.
- [x] **NU** PIE.
- [x] **NU** asset previewers.
- [x] **NU** multithreaded ECS scheduler.
- [x] **NU** networking.
- [x] **NU** shipping/install pipeline.

---

## Ordinea recomandată de execuție

**Audit → Architecture → Full Runtime Reflection Core → migrate ComponentRegistry authority → reflect foundation types/components → generic property command/OCP proof → Editor Session/Selection → Command History → Snapshot → Dynamic Hierarchy → Create/Rename/Delete/Duplicate → Reparent → reflection-driven Inspector → custom property/component extensions → Local/World gizmos → history integration → diagnostics → tests → stress/perf → regression → documentation → completion gate.**

Primul milestone intern obligatoriu este:

**Runtime Reflection Core**

Al doilea:

**Editor Session + EntityHandle Selection + Command History + Reflection-backed Transient Snapshot**

Nu construim full Inspector-ul sau hierarchy drag/drop înainte ca Reflection Core și command/selection foundations să fie testate.
