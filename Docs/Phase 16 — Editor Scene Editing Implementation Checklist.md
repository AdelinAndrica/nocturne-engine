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

- [x] Gregory §1.7.5 este folosit pentru tool architecture/shared runtime data.
- [x] Gregory §15.4 este folosit pentru rolul world editor-ului.
- [x] Gregory §15.4.1 este folosit pentru typical world-editor workflows.
- [x] Gregory §15.4.1.7 este folosit pentru special transform/object placement tools.
- [x] Gregory §15.4.1.10 este folosit pentru rapid iteration.
- [x] Gregory §15.4.1.9 este recunoscut ca persistence requirement, dar implementarea este Phase 17.
- [x] Nystrom Command / Undo and Redo este grounding pentru command history.
- [x] Lengyel Vol. 2 §5.4.2 rămâne grounding pentru transform hierarchy.
- [x] Nu inventăm page numbers.
- [x] Orice politică Nocturne-specifică este etichetată **Design choice (not directly from the book)**.

---

# 2. Audit înainte de cod

- [x] Confirmăm branch-ul Phase 16.
- [x] Citim integral toate phase `.md`.
- [x] Citim Production Engineering Standard.
- [x] Citim Phase 16 Runtime Reflection Architecture Contract.
- [x] Citim Phase 16 Professional Grade Implementation Contract.
- [x] Citim Phase 15 Architecture / Implementation / Test & CI / Completion.
- [x] Citim Phase 14 editor architecture/completion.
- [x] Inspectăm `EditorShellV3.h/.cpp`.
- [x] Inspectăm `EditorViewportController.h/.cpp`.
- [x] Inspectăm `Apps/NocturneEditor/main.cpp`.
- [x] Inspectăm `World.h/.cpp`.
- [x] Inspectăm `EntityRegistry`.
- [x] Inspectăm component systems și metadata.
- [x] Inventariem toate UI stubs Phase 13/14 pentru Scene/Inspector/Undo/Redo.
- [x] Inventariem `validationObjects_[4]`.
- [x] Inventariem `selectedIndex_`.
- [x] Inventariem row/index mapping din hierarchy.
- [x] Inventariem hard-coded labels / counts / camera.
- [x] Inventariem toate locurile unde editorul modifică direct World.
- [x] Inventariem shortcut/focus routing.
- [x] Inventariem Win32 child HWND ownership.
- [x] Inventariem current scene tree din EditorShellV3 versus hierarchy rendering din viewport controller.
- [x] Eliminăm riscul de a construi două Scene Hierarchy independente.
- [x] Definim cine va fi owner-ul Scene Hierarchy UI final.
- [x] Definim cine va fi owner-ul Inspector UI final.
- [x] Nu începem feature coding până când authority boundaries sunt clare.

---

# 3. Phase 16 architecture document

Înainte sau odată cu primul milestone:

- [x] Creăm `Docs/Phase 16 — Editor Scene Editing Architecture.md`.
- [x] Documentăm ownership.
- [x] Documentăm Editor Session.
- [x] Documentăm selection model.
- [x] Documentăm command/history model.
- [x] Documentăm transaction semantics.
- [x] Documentăm snapshot semantics.
- [x] Documentăm authored vs tool-owned entities.
- [x] Documentăm hierarchy enumeration.
- [x] Documentăm delete semantics.
- [x] Documentăm duplicate semantics.
- [x] Documentăm reparent semantics.
- [x] Documentăm component add/remove.
- [x] Documentăm Runtime Reflection architecture.
- [x] Documentăm migration path ComponentRegistry → ReflectionRegistry.
- [x] Documentăm Inspector ca reflection consumer.
- [x] Documentăm Local/World gizmo orientation.
- [x] Documentăm focus/input policy.
- [x] Documentăm history memory budget.
- [x] Documentăm threading.
- [x] Documentăm Phase 17 persistence boundary.
- [x] Documentăm temporary scaffolding removal.
- [x] Marcăm toate design choices non-book.

---


# 3A. Runtime Reflection Core — BLOCKER înainte de Inspector

Autoritate:

`Docs/Phase 16 — Runtime Reflection Architecture Contract.md`

Ownership:
- [x] `Engine` deține un singur `ReflectionRegistry`.
- [x] Reflection init precede World init.
- [x] Reflection shutdown este după World/consumers shutdown conform lifetime documentat.
- [x] World/Editor sunt non-owning consumers.
- [x] No global Reflection singleton.
- [x] ComponentRegistry Phase 15 este migrat/facade, nu autoritate paralelă.

Identity:
- [x] stable TypeId.
- [x] invalid TypeId.
- [x] stable PropertyId.
- [x] invalid PropertyId.
- [x] stable FunctionId/identity.
- [x] IDs nu depind de registration order.
- [x] IDs nu depind de RTTI pointer/address.
- [x] duplicate IDs respinse.
- [x] duplicate canonical names respinse.

Type system:
- [x] TypeKind.
- [x] primitive types.
- [x] String.
- [x] Enum.
- [x] Struct.
- [x] Component.
- [x] Entity reference category.
- [x] Resource reference category.
- [x] fixed array.
- [x] dynamic sequence/container adapter.
- [x] Function.
- [x] Opaque/custom seam.

Type metadata:
- [x] canonical name.
- [x] version.
- [x] size/alignment.
- [x] flags.
- [x] deterministic enumeration.
- [x] registry-owned metadata/string lifetime.
- [x] no temporary descriptor pointers.

Lifecycle/type ops:
- [x] default construct.
- [x] destruct.
- [x] copy construct.
- [x] move construct.
- [x] copy assign.
- [x] move assign.
- [x] equality/compare policy.
- [x] reset/default policy.
- [x] non-trivial type tests.
- [x] over-aligned type tests.
- [x] no generic memcpy for non-trivial values.

Properties:
- [x] PropertyMetadata.
- [x] owner TypeId.
- [x] value TypeId.
- [x] flags.
- [x] getter.
- [x] setter/read-only.
- [x] semantic setter path.
- [x] optional safe direct-address path.
- [x] validation adapter.
- [x] default provider.
- [x] no raw write bypass pentru Camera invariants.
- [x] no raw write bypass pentru Transform hierarchy.

Attributes:
- [x] typed attribute mechanism.
- [x] display name.
- [x] category.
- [x] tooltip.
- [x] numeric range/step.
- [x] units.
- [x] angle/color hints.
- [x] resource type constraint.
- [x] serialization/script aliases seam.
- [x] no Win32 dependency.

Enums:
- [x] enum TypeId.
- [x] underlying type.
- [x] value metadata.
- [x] duplicate validation.
- [x] flags enum support/policy.
- [x] generic lookup/name conversion.

Nested structs:
- [x] recursive property traversal.
- [x] Vec2/Vec3/Vec4 policy.
- [x] Quat policy.
- [x] AABB policy.
- [x] cycle/reference handling.

Containers:
- [x] element TypeId.
- [x] count.
- [x] const access.
- [x] mutable access policy.
- [x] resize/insert/remove adapter seam.
- [x] no STL types required by public API.
- [x] fixed array tests.
- [x] dynamic sequence synthetic test.

References:
- [x] ResourceHandle reflected semantically.
- [x] expected resource type constraint.
- [x] EntityHandle reflected as transient reference.
- [x] metadata seam pentru Phase 17 persistent translation.
- [x] runtime index/generation never marked durable identity.

Components:
- [x] reflected component operations Has/Add/Remove/Get.
- [x] generic reflected component enumeration per entity.
- [x] Name reflected.
- [x] Transform reflected.
- [x] Renderable reflected.
- [x] Camera reflected.
- [x] internal caches/hierarchy links hidden/read-only appropriately.
- [x] derived fields marked transient/read-only.

Functions:
- [x] FunctionMetadata.
- [x] stable function identity.
- [x] return TypeId.
- [x] parameters.
- [x] flags.
- [x] invocation adapter.
- [x] type/count validation.
- [x] const/static/member semantics.
- [x] real engine function reflection proof.
- [x] mismatch/failure tests.
- [x] Phase 24 script exposure policy deferred, mechanism implemented.

Generic values:
- [x] const reflected value view.
- [x] mutable reflected value view.
- [x] owned reflected value.
- [x] allocator ownership explicit.
- [x] alignment/lifecycle correct.
- [x] copy/move non-trivial safe.
- [x] no std::any public dependency.

Registry:
- [x] deterministic registration.
- [x] no static-init-order dependency.
- [x] Building state.
- [x] Freeze.
- [x] Frozen read-only state.
- [x] post-freeze registration rejected.
- [x] schema validation pass.
- [x] referenced TypeIds validation.
- [x] registry dump diagnostics.
- [x] shutdown/leak tests.

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
- [x] synthetic new reflected component.
- [x] schema registration only pentru generic fields.
- [x] generic Inspector model îl vede.
- [x] generic property command îl poate edita.
- [x] debug reflection dump îl vede.
- [x] NU modificăm central Inspector switch.
- [x] NU modificăm central property-command switch.
- [x] NU introducem serializer/scripting schema paralelă.

Reflection milestone:
- [x] Reflection Core authoring seams au precedat full Inspector; final perf/schema-dump proofs au fost închise ulterior în completion hardening.

---

# 4. Editor Session

- [x] Introducem un owner clar pentru editor-session state.
- [x] Engine/World nu devine owner al UI state.
- [x] Session deține selected EntityHandle.
- [x] Session deține active tool.
- [x] Session deține transform orientation.
- [x] Session deține command history.
- [x] Session deține scene dirty state dacă este adoptat.
- [x] Session deține transient transaction state.
- [x] Session cunoaște tool-owned entities.
- [x] Session nu deține copii autoritare ale componentelor.
- [x] Session lifecycle este explicit Init/Shutdown.
- [x] Shutdown anulează/finalizează tranzacția activă determinist.
- [x] History este distrus înainte ca World target să devină invalid sau command destructors nu dereferențiază World.
- [x] No globals.

---

# 5. Tool-owned editor camera

- [x] Separăm camera de navigație editor de authored scene cameras.
- [x] Editor camera nu apare ca authored entity în hierarchy.
- [x] Editor camera nu poate fi delete.
- [x] Editor camera nu poate fi duplicate.
- [x] Editor camera nu poate fi reparent prin scene UI.
- [x] Editor camera nu intră în transient authored snapshots.
- [x] Editor camera rămâne sursa de view pentru viewport.
- [x] Scene CameraComponents pot fi create/editate independent.
- [x] Destroy/reset scene nu distruge accidental editor camera.
- [x] Phase 17 va putea exclude tool camera din persistence fără hacks.

---

# 6. Selection identity

- [x] Eliminăm `selectedIndex_` ca entity identity.
- [x] Selection = `EntityHandle`.
- [x] Default selection invalid.
- [x] Selection validează `World::IsAlive`.
- [x] Destroy selected entity clears/updates selection.
- [x] Undo delete poate selecta restored entity.
- [x] Duplicate selectează copia conform contractului.
- [x] Reparent păstrează selection.
- [x] Rename păstrează selection.
- [x] Component mutation păstrează selection.
- [x] Viewport click schimbă aceeași selection.
- [x] Hierarchy click schimbă aceeași selection.
- [x] Inspector citește aceeași selection.
- [x] No row index stored as identity.
- [x] No component pointer stored as identity.
- [x] Stale selection nu produce crash/log spam.

---

# 7. Dynamic authored entity enumeration

- [x] Eliminăm `kValidationObjectCount` din authoring logic.
- [x] Eliminăm `validationObjects_[4]` din authoring authority.
- [x] World enumeration este sursa pentru authored entity discovery.
- [x] Tool-owned entities sunt filtrate explicit.
- [x] Deterministic iteration order este documentat.
- [x] Entity without required editor components are handled deterministically.
- [x] Hierarchy refresh suportă entity count 0.
- [x] Hierarchy refresh suportă 1 entity.
- [x] Hierarchy refresh suportă 10k entities.
- [x] Destroy during prior frame nu lasă stale rows.
- [x] Structural version/change notification strategy este definită.
- [x] Nu facem full rebuild inutil per-frame dacă nu există schimbare.

---

# 8. Scene Hierarchy UI — single authority

- [x] Decidem dacă final hierarchy este controlul `EditorShellV3::sceneTree_` sau un replacement dedicat.
- [x] Nu păstrăm simultan două hierarchy UIs autoritare.
- [x] Row model conține EntityHandle.
- [x] Row model nu conține component pointers persistente.
- [x] Root/editor labels nu sunt confundate cu entities.
- [x] Parent/child indentation reflectă Transform hierarchy.
- [x] Entity fără parent apare root authored entity.
- [x] Expand/collapse.
- [x] Hover.
- [x] Selected row.
- [x] Keyboard navigation unde implementat.
- [x] Scroll.
- [x] Large hierarchy clipping/virtualization strategy dacă este necesar după baseline.
- [x] Rename feedback.
- [x] Component/type icons dacă există metadata suficientă.
- [x] Duplicate names afișate corect.
- [ ] Invalid UTF-8 fallback diagnostic.
- [x] No hard-coded `Runtime Objects (4)`.
- [x] No hard-coded Cube_A/B/C/Ground rows.

---

# 9. Create Entity

**Design choice (not directly from the book): editor-created scene entity defaults to Name + Transform.**

- [x] Add command pentru create.
- [x] Create este undoable.
- [x] Redo recreatează semantic entity.
- [x] Nu presupunem același runtime handle la redo.
- [x] Default name policy este documentată.
- [x] Default transform identity.
- [x] Parent target optional.
- [x] Create under selected parent dacă UX decide asta.
- [x] Parent invalid/stale => clear diagnostic / root fallback conform policy.
- [x] Selection după create este predictibilă.
- [x] History push doar după success.
- [x] Allocation failure nu produce partial entity.
- [x] Component-init failure rollback-ează entity.
- [x] Tool camera nu este afectată.
- [x] Test create 1/100/10k.

---

# 10. Rename Entity

- [x] Rename folosește NameComponent.
- [x] Rename este command.
- [x] Undo restorează exact numele anterior.
- [x] Redo reaplică numele nou.
- [x] Duplicate names sunt acceptate.
- [x] Empty name policy explicit.
- [x] UTF-8 byte limit respectat.
- [x] Too-long input rejected.
- [x] Nu trunchiem silent.
- [x] Rename in hierarchy și Inspector folosesc aceeași command.
- [x] Focus loss/Enter semantics.
- [x] Escape cancel.
- [x] Invalid/stale entity => no history entry.
- [x] Hierarchy refresh imediat.

---

# 11. Transient subtree snapshot

- [x] Definim editor-only snapshot type.
- [x] Snapshot copiază semantic data, nu component pointers.
- [x] Snapshot copiază Name.
- [x] Snapshot copiază Transform local TRS.
- [x] Snapshot copiază Transform internal parent relationships.
- [x] Snapshot copiază Renderable.
- [x] Snapshot copiază Camera.
- [x] Snapshot păstrează component presence.
- [x] Snapshot nu păstrează runtime EntityHandle drept persistent identity.
- [x] Internal snapshot references folosesc snapshot-local IDs/indices.
- [x] Snapshot poate restaura subtree cu handles noi.
- [x] Snapshot produce old→new remap.
- [x] Snapshot excludes tool-owned entities.
- [x] Snapshot allocation failure este explicit.
- [x] Snapshot destructor/lifetime testat.
- [x] Snapshot nu este expus ca scene serialization API.
- [x] Snapshot format nu este numit stable/persistent.
- [x] Test non-trivial hierarchy restore.
- [x] Test missing optional components.
- [ ] Test duplicate names.
- [x] Test camera component in subtree.
- [x] Leak tests.

---

# 12. Delete Entity/Subtree

**Design choice (not directly from the book): editor delete defaults to authored subtree delete.**

- [x] Capture subtree snapshot înainte de mutation.
- [x] Dacă snapshot eșuează, delete nu pornește.
- [x] Delete children in safe order.
- [x] Delete parent.
- [x] World destroy failures handled.
- [x] Compound rollback policy definită.
- [x] Delete command = single history entry.
- [x] Undo restorează entire subtree.
- [x] Undo restorează internal parent relationships.
- [x] Undo selection policy.
- [x] Redo șterge restored subtree.
- [x] Redo target mapping actualizat.
- [x] Delete selected root.
- [x] Delete selected child.
- [x] Delete deep subtree.
- [x] Delete wide subtree.
- [x] Tool-owned entity delete rejected.
- [x] Invalid/stale selection no-op + diagnostic.
- [x] Keyboard Delete respectă text-edit focus.
- [x] Context menu Delete folosește aceeași command.

---

# 13. Duplicate Entity/Subtree

**Design choice (not directly from the book): duplicate defaults to authored subtree duplicate.**

- [x] Capture source snapshot.
- [x] Instantiate with new EntityHandles.
- [x] Preserve component presence.
- [x] Preserve local TRS.
- [x] Preserve internal hierarchy.
- [x] Parent copy policy explicit.
- [x] Naming policy explicit.
- [x] Select duplicated root.
- [x] Duplicate este single command.
- [x] Undo deletes duplicate subtree.
- [x] Redo recreates duplicate subtree.
- [x] No source mutation.
- [x] Tool entity duplicate rejected.
- [x] Stale source rejected.
- [x] External references boundary documented.
- [x] Test one entity.
- [x] Test hierarchy.
- [x] Test camera/renderable.
- [x] Test 1k subtree stress.

---

# 14. Reparent / Unparent

- [x] Reparent UI workflow definit.
- [x] Drag/drop hierarchy sau equivalent explicit UX.
- [x] Drop target highlight.
- [x] Invalid target feedback.
- [x] Self-parent reject.
- [x] Descendant-cycle reject.
- [x] Stale source reject.
- [x] Stale target reject.
- [x] Unparent/root drop.
- [x] Reparent command.
- [x] Undo restores original parent.
- [x] Redo reapplies new parent.
- [x] Selection preserved.
- [x] Expanded state preserved where possible.

**Design choice (not directly from the book): editor reparent preserves world pose by default.**

- [x] Capture pre-reparent world transform.
- [x] Compute new local transform under new parent.
- [x] Matrix inverse failure handled.
- [x] TRS decomposition failure handled.
- [x] Shear/non-representable transform policy documented.
- [x] Non-uniform parent scale test.
- [x] Rotated parent test.
- [ ] Deep hierarchy test.
- [x] World pose epsilon verification test.

---

# 15. Command interface

- [x] Command has clear ownership.
- [x] Execute/apply semantics.
- [x] Undo semantics.
- [x] Redo semantics.
- [x] Command display/debug name.
- [x] Result/failure propagation.
- [x] No raw dangling component pointers.
- [x] Entity handles validated on each apply where relevant.
- [x] Commands that recreate entities maintain remap.
- [x] Destructor safe after World shutdown policy.
- [x] No implicit global history.

---

# 16. Command History

Grounding: Nystrom — Command / Undo and Redo.

- [x] Multiple undo levels.
- [x] Multiple redo levels.
- [x] Cursor semantics.
- [x] New command after undo discards redo tail.
- [x] Undo on empty history safe.
- [x] Redo on end safe.
- [x] Failed command not pushed.
- [x] Failed undo leaves cursor consistent.
- [x] Failed redo leaves cursor consistent.
- [x] Clear history.
- [x] History change notification for toolbar enabled state.
- [x] Command labels pentru diagnostics.
- [x] History max count/budget.
- [x] Eviction policy.
- [x] Large command policy.
- [x] Allocation failure policy.
- [x] Unit tests.
- [x] Stress 10k small commands.
- [x] Stress large subtree snapshots.
- [x] Leak tests.

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
- [x] Multi-field Inspector grouping policy explicit: current Phase 16 field/nested edits commit one semantic property command; transaction API exists for future multi-command UX.
- [x] Delete subtree uses one logical command.
- [x] Duplicate subtree uses one logical command.

---

# 18. Gizmo → history integration

- [x] Begin drag captures original local TRS.
- [x] Mouse move preview does not push history.
- [x] Mouse up commits one command.
- [x] No movement => no command.
- [x] Escape reverts original.
- [x] Capture loss policy.
- [x] Tool switch during drag policy.
- [x] Selection change during drag policy.
- [x] Entity destroyed during drag policy.
- [x] Undo exact original.
- [x] Redo exact final.
- [x] No per-mouse-move heap churn.
- [ ] Existing Phase 14 feel preserved.

---

# 19. Local / World transform orientation

- [x] UI control visible.
- [x] State stored in Editor Session.
- [x] Local mode remains current baseline.
- [x] World Move implemented.
- [x] World Rotate implemented.
- [x] Root entity tests.
- [x] Rotated entity tests.
- [x] Parented entity tests.
- [x] Rotated parent tests.
- [x] Non-uniform scale parent tests.
- [x] Toggle during no active drag.
- [x] Toggle during active drag policy.

Scale:
- [x] Define Local scale behavior.
- [x] Decide/define World scale behavior.
- [x] Do not claim world-scale support if shear cannot be represented.
- [x] UI communicates limitation if applicable.

---

# 20. Reflection-driven Inspector layer

Reflection schema este autoritatea; editor metadata este doar presentation extension.

- [x] Inspector enumeră reflected components.
- [x] Inspector enumeră reflected properties.
- [x] Generic drawer registry keyed by reflected TypeId/attributes.
- [x] Bool drawer.
- [x] integer drawer.
- [x] float drawer.
- [x] Vec/struct drawer.
- [x] enum drawer.
- [x] string drawer.
- [x] resource reference drawer.
- [x] readonly display.
- [x] nested struct traversal.
- [ ] custom property drawer extension.
- [ ] custom component inspector extension.
- [x] custom extensions referă reflection IDs și NU redefin canonical schema.
- [x] CanAdd/CanRemove vine din component reflection/editor policy.
- [x] Validation/apply folosește semantic reflected setters.
- [x] Unknown reflected type are safe fallback/diagnostic.
- [x] No giant hard-coded Inspector component switch.
- [x] No central property-name switch.
- [x] No Win32 types leak into engine runtime reflection headers.
- [x] No STL restriction violation in public engine headers.
- [x] Editor layer poate folosi STL intern.

---

# 21. Inspector selection lifecycle

- [x] No selection state.
- [x] Live selection state.
- [x] Stale selection clears.
- [x] Selection change destroys/rebinds edit controls safely.
- [x] Structural component add/remove refreshes Inspector.
- [x] Component pointer not retained across structural mutation.
- [ ] Tab/focus order.
- [x] Scroll.
- [x] Resize.
- [x] Long values clipped/scrollable.
- [ ] Disabled/read-only field visuals.
- [ ] Error state visuals.

---

# 22. Name Inspector

- [x] Name field.
- [x] UTF-8 conversion.
- [x] 63-byte payload contract.
- [x] Empty input policy.
- [x] Duplicate names.
- [x] Enter commit.
- [x] Focus-loss commit.
- [x] Escape cancel.
- [x] Undo/redo.
- [x] Hierarchy live refresh.
- [x] No partial invalid write.

---

# 23. Transform Inspector

- [x] Position X/Y/Z.
- [x] Rotation representation selected/documented.
- [x] Scale X/Y/Z.
- [x] Numeric parse robust.
- [x] NaN reject.
- [x] Inf reject.
- [x] Locale/decimal behavior considered.
- [x] Commit/cancel.
- [x] Coalescing.
- [x] Undo/redo.
- [x] Gizmo synchronization.
- [x] Parent changes reflected.
- [x] Optional readonly world transform display.
- [ ] Degenerate scale policy.

Euler UI if used:
- [x] Quaternion↔Euler conversion documented.
- [x] Wrap/display conventions.
- [x] Gimbal/discontinuity UX acknowledged.
- [x] Runtime authority remains quaternion.

---

# 24. Renderable Inspector

- [x] Add Renderable.
- [x] Remove Renderable.
- [x] Enabled.
- [x] Mesh assignment.
- [x] Existing resource system used.
- [x] Invalid resource assignment rejected.
- [x] Previous mesh retained on failed assignment.
- [x] Bounds display/edit policy.
- [x] Undo/redo.
- [x] Viewport updates immediately.
- [x] No asset previewer scope creep.

---

# 25. Camera Inspector

- [x] Add Camera.
- [x] Remove Camera.
- [x] FOV edit.
- [x] Aspect policy.
- [x] Near edit.
- [x] Far edit.
- [x] Enabled edit.
- [x] Runtime validation reused.
- [x] Invalid lens rejected.
- [x] Undo/redo.
- [x] Scene Camera != Editor Camera.
- [x] Removing authored active/runtime camera does not kill editor viewport camera.

---

# 26. Add Component UX

- [x] Add Component button/menu.
- [x] Enumerate editor-supported component descriptors.
- [x] Hide already-present components.
- [x] Required components excluded if already present.
- [x] Add command.
- [x] Undo removes added component.
- [x] Redo re-adds.
- [x] Defaults explicit.
- [x] Allocation failure rollback.
- [x] Inspector refresh.
- [x] Viewport refresh.
- [x] Diagnostics.

---

# 27. Remove Component UX

- [x] Remove action per removable component.
- [x] Required components cannot be removed.
- [x] Remove command captures component state.
- [x] Undo restores exact component state.
- [x] Redo removes again.
- [x] Camera active-state implications handled.
- [x] Renderable removal updates viewport.
- [x] Stale entity safe.
- [x] Diagnostics.

---

# 28. Asset assignment integration

- [x] Audit existing Content Browser asset identity.
- [x] Audit ResourceManager handles/path mapping.
- [x] Decide picker/drag-drop integration.
- [x] No raw filesystem path stored in RenderableComponent dacă ResourceHandle este authority.
- [x] Missing asset diagnostic.
- [x] Wrong asset type diagnostic.
- [x] Async load state handling dacă aplicabil.
- [x] Assignment undoable.
- [x] No Phase 23 previewer scope.

---

# 29. Scene dirty state

**Design choice (not directly from the book).**

- [x] Decide dacă Phase 16 tracks dirty.
- [x] Successful authoring command marks dirty.
- [x] Undo/redo dirty semantics documented.
- [x] New transient scene reset semantics.
- [x] Exit warning semantics dacă dirty.
- [x] Save nu este implementat fals.
- [x] Dirty != serialized-state hash unless explicitly designed.
- [x] Phase 17 can adopt/extend without rewrite.

---

# 30. New Scene transient workflow

Dacă Phase 16 îl implementează:

- [x] Clar că este in-memory scene reset.
- [x] Tool camera survives.
- [x] Authored entities cleared.
- [x] Selection cleared.
- [x] History cleared.
- [x] Dirty confirmation policy.
- [x] No fake Save/Open.
- [x] No disk I/O pretending to be Phase 17.

Dacă nu este implementat:
- [x] Stub message updated to say persistence/session reset is deferred explicitly.

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

- [x] Ctrl+Z.
- [x] Ctrl+Y / Ctrl+Shift+Z policy.
- [x] Delete.
- [x] Ctrl+D.
- [x] F2 rename if adopted.
- [x] Escape cancel current edit/drag.
- [x] Enter commit edit.
- [x] Shortcuts disabled/routed appropriately during text edit.
- [x] Camera capture priority.
- [x] Gizmo capture priority.
- [x] Hierarchy drag priority.
- [x] No accidental delete while typing.
- [x] Toolbar buttons call same command pathways.

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

- [x] stale selected entity.
- [x] stale command target.
- [x] create allocation fail.
- [x] snapshot allocation fail.
- [x] restore allocation fail.
- [x] duplicate component.
- [x] absent component remove.
- [x] required component remove.
- [x] invalid parent.
- [x] self parent.
- [x] cycle parent.
- [x] non-invertible parent transform.
- [x] TRS decomposition failure.
- [x] invalid name.
- [x] invalid numeric field.
- [x] invalid camera lens.
- [x] invalid asset/resource.
- [x] history budget failure.
- [x] compound rollback failure.
- [x] editor tool camera misuse.
- [x] shutdown with active transaction.

Fiecare caz are:
- [x] return/result policy.
- [x] log/assert policy.
- [x] user-visible diagnostic policy.
- [x] state integrity verification.

---

# 35. Diagnostics / observability

- [x] Editor console records command failures.
- [x] Status bar can show relevant error/selection state.
- [x] Debug command names.
- [x] History depth/cursor counters.
- [x] Current selection handle diagnostic.
- [x] Current tool/orientation diagnostic.
- [x] Hierarchy entity count.
- [ ] Snapshot entity/component count in debug logs.
- [x] One-shot history dump: optional, not required for Phase 16 completion.
- [x] No per-frame spam.
- [x] No per-mouse-move spam.

---

# 36. Threading contract

- [x] Editor authoring mutations main-thread only.
- [x] Structural ECS mutations main-thread only.
- [x] Command execute/undo/redo main-thread only.
- [x] Inspector commit main-thread.
- [x] Hierarchy mutation main-thread.
- [x] Async asset result application marshalled safely if required.
- [x] No new locks in component storage.
- [x] Shutdown/cancellation order explicit.

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

- [x] empty undo.
- [x] empty redo.
- [x] execute one.
- [x] undo one.
- [x] redo one.
- [x] multiple commands.
- [x] undo multiple.
- [x] redo multiple.
- [x] new command after undo clears redo.
- [x] failed execute not pushed.
- [x] failed undo cursor safety.
- [x] failed redo cursor safety.
- [x] clear.
- [x] budget eviction.
- [x] command destruction.
- [x] compound command.
- [x] transaction cancel.
- [x] leak-free.

---

# 40. Integration tests — scene operations

- [x] create.
- [x] create under parent.
- [x] rename.
- [x] duplicate leaf.
- [x] duplicate subtree.
- [x] delete leaf.
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
- [x] asset assignment.
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

- [x] `Phase 16 — Runtime Reflection Architecture Contract.md` actualizat cu implementarea reală.
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
