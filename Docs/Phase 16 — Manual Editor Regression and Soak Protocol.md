# Nocturne Engine — Phase 16 Manual Editor Regression and Soak Protocol

> **Status:** REQUIRED MANUAL COMPLETION GATE
>
> **Phase:** 16 — Editor Scene Editing + Runtime Reflection
>
> **Target branch:** \`phase-16-editor-scene-editing\`
>
> **Code baseline under manual test:** \`e2e09a1962b497d9c309a65107c7ec1933a2e3a4\` — \`phase16: close final headless authoring gates\`
>
> **Required green CI evidence:** run \`35443924446\`, job \`105899491478\`
>
> **Date:** 2026-09-19

---

## 1. Purpose

This protocol validates behavior that the headless Host tests and build-only CI cannot prove:

- preserved Phase 13 editor visual baseline;
- real Win32 focus/capture/input routing;
- DX12 viewport presentation;
- interactive picking/gizmo behavior;
- editor usability across repeated authoring operations;
- absence of recurring stutter, log spam and crash during an extended editing session.

Do not mark Phase 16 COMPLETE from CI alone.

---

## 2. Build/run target

Use code baseline `e2e09a19` from the Phase 16 branch and launch the Debug x64 NocturneEditor built from that baseline. Documentation-only commits after `e2e09a19` do not change the executable under test.

Before starting:

- CI for the current code baseline must be green;
- do not use historical \`EditorShell\` / \`EditorControls\`;
- active shell must be \`EditorShellV3\`;
- keep Console visible during authoring tests.

---

## 3. Phase 13 visual baseline regression

Verify without redesigning the accepted baseline:

- [ ] application window opens normally;
- [ ] menu bar layout/chrome is preserved;
- [ ] toolbar proportions/icons are preserved;
- [ ] Scene Hierarchy panel chrome is preserved;
- [ ] viewport panel chrome is preserved;
- [ ] Inspector panel chrome is preserved;
- [ ] Content Browser remains usable;
- [ ] Console remains usable;
- [ ] Build/Play panel baseline is preserved;
- [ ] status bar baseline is preserved;
- [ ] Tabler icon rendering has no obvious fallback/regression;
- [ ] resizing the main window does not corrupt layout.

Any visual change should be treated as a regression unless it is a documented Phase 16 authoring addition.

---

## 4. Phase 14 viewport regression

Verify:

- [ ] DX12 scene continues rendering in the editor viewport;
- [ ] viewport resize remains stable;
- [ ] right-mouse camera capture works;
- [ ] mouse-look works;
- [ ] keyboard camera navigation works;
- [ ] mouse wheel camera-speed behavior remains usable;
- [ ] clicking authored geometry updates the shared EditorSession selection;
- [ ] selection bounds follow the selected authored entity;
- [ ] tool/editor camera cannot be picked as authored content;
- [ ] losing mouse capture does not leave camera/gizmo input stuck.

---

## 5. Scene Hierarchy regression

Start from a small authored scene and exercise:

- [ ] empty scene hierarchy;
- [ ] create root entity;
- [ ] create child entity;
- [ ] duplicate names display as separate rows;
- [ ] expand/collapse;
- [ ] hierarchy scroll;
- [ ] row hover/selection;
- [ ] F2/inline rename workflow if routed by current shortcut policy;
- [ ] context-menu Create;
- [ ] context-menu Create Child;
- [ ] context-menu Rename;
- [ ] context-menu Duplicate;
- [ ] context-menu Delete;
- [ ] drag/drop reparent;
- [ ] unparent;
- [ ] invalid cycle attempt is rejected without corrupting hierarchy;
- [ ] deleting the selected entity does not leave a stale active row;
- [ ] Undo delete restores the subtree with working selection/hierarchy refresh.

---

## 6. Inspector regression

For authored entities, verify:

- [ ] no-selection state is stable;
- [ ] Name renders and edits;
- [ ] Transform renders and edits;
- [ ] Renderable renders;
- [ ] Camera renders and edits;
- [ ] multiple components display together;
- [ ] Inspector scroll works;
- [ ] nested vector axis editing works;
- [ ] nested AABB editing works;
- [ ] bool drawer works;
- [ ] enum drawer/menu works;
- [ ] angle presentation uses degrees where reflected metadata requires it;
- [ ] invalid numeric text does not mutate World;
- [ ] invalid Camera lens values are rejected;
- [ ] Add Component refreshes Inspector immediately;
- [ ] Remove Component refreshes Inspector immediately;
- [ ] component add/remove Undo/Redo refreshes correctly;
- [ ] text edit Enter/focus-loss commit behavior is coherent;
- [ ] Escape cancels the active edit where implemented;
- [ ] text editing does not trigger destructive scene shortcuts.

---

## 7. Resource assignment regression

Using the Renderable Inspector:

- [ ] mesh ResourceHandle picker opens;
- [ ] valid mesh assignment updates the authored Renderable;
- [ ] Undo restores the previous assignment;
- [ ] Redo restores the new assignment;
- [ ] invalid/missing selection does not crash;
- [ ] no Phase 23 asset-preview behavior is expected.

---

## 8. Gizmo regression

Test Move and Rotate in both Local and World orientation:

- [ ] Local Move X/Y/Z;
- [ ] World Move X/Y/Z;
- [ ] Local Rotate X/Y/Z;
- [ ] World Rotate X/Y/Z;
- [ ] Local Scale X/Y/Z;
- [ ] Scale remains Local when World orientation is selected;
- [ ] parented transform editing;
- [ ] rotated-parent editing;
- [ ] non-uniform-parent World Move;
- [ ] one completed drag produces one Undo step;
- [ ] Undo restores pre-drag transform;
- [ ] Redo restores final transform;
- [ ] no-op drag produces no meaningful history step;
- [ ] Escape during drag restores the starting transform;
- [ ] capture/focus loss follows the documented commit policy;
- [ ] switching tools during a live drag cancels/restores coherently;
- [ ] no stuck capture/cursor behavior remains after drag termination.

**Design choice (not directly from the book):** Phase 16 does not provide arbitrary World Scale because the resulting shear may not be representable by the current pure-TRS TransformComponent.

---

## 9. Authoring/history workflow regression

Exercise a mixed sequence:

1. Create root.
2. Create child.
3. Rename both.
4. Move/rotate child.
5. Add Camera.
6. Edit Camera lens.
7. Duplicate subtree.
8. Reparent duplicate.
9. Remove Camera.
10. Delete original subtree.

Then:

- [ ] Undo through every operation;
- [ ] Redo through every operation;
- [ ] issue a new command after partial Undo and verify redo tail disappears;
- [ ] selection remains coherent;
- [ ] hierarchy remains coherent;
- [ ] Inspector remains coherent;
- [ ] no stale-handle crash occurs.

---

## 10. Dirty state / New Scene / exit

Verify:

- [ ] successful authoring sets dirty state;
- [ ] New Scene warns before discarding dirty authored state;
- [ ] canceling the warning retains the current scene;
- [ ] accepting New Scene clears authored entities;
- [ ] tool camera survives New Scene;
- [ ] selection clears;
- [ ] history clears;
- [ ] new in-memory scene returns to clean state;
- [ ] exit warns when dirty;
- [ ] Open/Save still clearly state that persistence is Phase 17;
- [ ] no file is written pretending to be a Phase 17 scene file.

---

## 11. 15+ minute edit-session soak

Run the editor continuously for at least 15 minutes.

During the soak, repeatedly alternate:

- camera navigation;
- selection;
- hierarchy expand/collapse;
- create/create-child;
- rename;
- duplicate;
- delete + Undo;
- reparent/unparent;
- Transform edits;
- gizmo drags;
- add/remove Camera;
- Camera property edits;
- mesh assignment where a valid asset is available;
- Undo/Redo chains;
- New Scene only after deliberately testing dirty confirmation.

Acceptance:

- [ ] no crash;
- [ ] no hang;
- [ ] no recurring stutter introduced by authoring state;
- [ ] no increasing input latency;
- [ ] no stuck mouse capture;
- [ ] no persistent stale selection;
- [ ] no recurring error/log spam;
- [ ] viewport keeps rendering;
- [ ] editor remains responsive after repeated Undo/Redo and hierarchy rebuilds.

---

## 12. Result record

Record:

- code commit tested;
- build configuration;
- total soak duration;
- PASS/FAIL for Phase 13 visual regression;
- PASS/FAIL for Phase 14 viewport regression;
- PASS/FAIL for Phase 16 authoring regression;
- PASS/FAIL for 15+ minute soak;
- any accepted limitation;
- any defect with exact repro steps.

Phase 16 Completion Report may be produced only when every required manual item is PASS or an explicit contract-approved defer is documented.
