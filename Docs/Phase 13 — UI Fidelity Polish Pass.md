# Phase 13 — UI Fidelity Polish Pass

> **Status:** PLANNED
>
> **Branch:** `phase-13-editor-framework`
>
> **Visual target:** the generated ultra-modern Nocturne Editor mockup used for the Phase 13 modernization pass.

## 1. Objective

Bring the native Win32 Phase 13 editor shell significantly closer to the visual target. The current shell proves the architecture and layout, but several native-control artifacts still make it look like a themed Windows utility instead of a modern game editor.

This pass is strictly visual/tooling polish. It must not pull Phase 14 rendering, gizmos, scene editing, ECS editing or Play-In-Editor behavior into Phase 13.

## 2. Current visual gaps observed against the target

The following differences are visible in the current executable and must be addressed:

- Typography is too large and too heavy across menu, toolbar, tree, inspector, build/play and status areas.
- The editor feels crowded because font metrics, line heights and paddings are too large relative to panel sizes.
- Toolbar buttons still expose native Win32/Vista-era visual behavior: bright white outlines, focus rectangles and a rectangular default-button feel.
- Toolbar button spacing and icon/text proportions do not yet match the compact minimal target.
- Standard Win32 scrollbars are visually inconsistent with the dark theme and immediately break the custom-editor look.
- The Content Browser table is the largest visual mismatch: white/native column headers, old ListView chrome, native horizontal scrollbar and generic Windows table selection behavior.
- Native TreeView expansion glyphs and focus/selection treatment still look too much like stock Windows controls.
- Console scrollbar and edit control chrome remain native.
- Search field and other inputs still look like skinned EDIT controls instead of custom editor inputs.
- Panel chrome is close, but borders and spacing are still more rigid and dense than the target.
- The target uses more subtle separators, darker low-contrast borders and smaller text, while the current build uses stronger contrast and heavier framing.
- Build / Play controls need flatter input rows, smaller labels and more compact vertical rhythm.
- Status bar text and separators are too prominent relative to the target.

## 3. Visual language to lock

**Design choice (not directly from the book):** the exact visual styling in this section is a Nocturne Editor UI design decision, not a requirement from the game-engine architecture books.

### Typography

Use smaller, calmer typography throughout the editor:

- Base UI font: `Segoe UI Variable Text`.
- Base body size: approximately 13 px at 100% scale.
- Secondary/muted text: approximately 12 px.
- Panel title size: approximately 13–14 px semibold.
- Toolbar text: approximately 13 px semibold.
- Console: `Cascadia Mono`, approximately 12–13 px.
- Large placeholder branding in viewport remains larger but subdued.

Avoid bold body copy. Use semibold only for panel titles, selected toolbar tools, section headings and primary actions.

### Spacing

Reduce visual density without shrinking usable hit targets:

- Main panel gap: 7–8 px.
- Panel internal padding: 8–10 px.
- Toolbar button height: 34–36 px.
- Toolbar button horizontal padding: 12–14 px.
- Tree/list row height: 24–26 px.
- Inspector/build row height: 30–32 px.
- Status bar height: approximately 26 px.
- Panel header height: approximately 30–32 px.

Text becomes smaller, but hit areas should stay comfortable.

### Color behavior

Keep the existing charcoal/blue palette, but reduce contrast:

- Window background: near-black navy/charcoal.
- Panel surface: slightly lighter than window background.
- Input/table surface: slightly darker than panel body.
- Border: subtle blue-gray, low contrast.
- Primary text: off-white, never pure white.
- Muted text: cool desaturated gray-blue.
- Accent: Nocturne electric blue.
- Success/play: restrained green.
- Warning: muted amber.

Pure white should not be used for ordinary borders, outlines or table headers.

## 4. Button system overhaul

**Design choice (not directly from the book):** all editor toolbar and action buttons should be owner-drawn Nocturne controls rather than visually relying on native Windows button chrome.

### Required changes

- Remove native focus rectangle rendering from toolbar and primary action buttons.
- Remove bright white button outlines.
- Do not use the default Win32/Vista border appearance.
- Draw background, border, hover, pressed and active states manually.
- Use 1 px low-contrast borders only where necessary.
- Use rounded corners consistently, approximately 6–8 px radius.
- Active tool (`Select`, later Move/Rotate/Scale) should use accent fill/border without an OS focus ring.
- `Play` should use restrained green icon/text treatment, not a bright Windows button.
- `Build` should look like a neutral editor action, not a native push button.
- Keyboard focus must remain accessible, but use a custom subtle accent indication rather than the stock dotted rectangle.
- Hover state should be slightly lighter than idle, not a large color jump.
- Pressed state should be slightly darker and inset visually.

### Toolbar target

The toolbar should read as one integrated strip with groups separated by subtle vertical separators:

`New Open Save | Undo Redo | Select Move Rotate Scale | Play Stop Build`

Buttons should feel like compact editor tools, not independent Windows dialogs buttons.

## 5. Custom scrollbar system

**Design choice (not directly from the book):** stock Win32 scrollbars must no longer be visually exposed inside Nocturne Editor panels.

### Visual specification

- Track should be transparent or almost the same color as the panel background.
- Thumb should be a subtle medium blue-gray.
- Thumb hover state should become slightly brighter.
- Thumb pressed state should use the accent-adjacent blue-gray.
- Width: approximately 8–10 px.
- No arrow buttons.
- No bright Windows borders.
- Rounded thumb corners.
- Scrollbar should only become visually prominent on hover/scroll where practical.

### Architectural approach

Do not attempt to fully theme native non-client scrollbars with fragile system-theme hacks.

Preferred Phase 13 implementation:

1. Introduce a reusable `EditorScrollBar` custom child control.
2. Hide/remove native `WS_VSCROLL` / `WS_HSCROLL` where the control permits.
3. Synchronize custom scrollbar position with the content control.
4. Support mouse wheel, page movement, thumb dragging and resize.
5. Use the same scrollbar implementation for Content Browser, Console and future editor lists.

If a specific Win32 control cannot cleanly separate from its native scrollbar, subclass/replace that editor control rather than exposing a stock scrollbar.

## 6. Content Browser table overhaul

The Content Browser table is the highest-priority fidelity issue.

**Design choice (not directly from the book):** the Phase 13 asset table should become an owner-drawn/custom editor table rather than visually retaining the stock ListView header and scrollbar chrome.

### Remove

- White/native column header.
- Native ListView header gradients/borders.
- Stock horizontal scrollbar.
- Default Windows row focus rectangles.
- Strong gridlines.

### Target appearance

- Dark header integrated into the table surface.
- Header height approximately 28 px.
- Header text small, muted and semibold.
- `Asset` column visually dominant.
- `Type` column narrower and muted.
- Rows approximately 26 px high.
- Selected row uses a restrained blue selection fill.
- Hover row uses a very subtle lighter surface.
- No full boxed grid around every cell.
- Use separators only where they improve readability.
- Asset type may use a small colored/icon marker later; Phase 13 may use simple glyphs/placeholders.
- Empty content state should look deliberate instead of like an empty system table.

### Implementation direction

Preferred approach:

- Create an `EditorDataTable` / owner-drawn table abstraction for editor tooling.
- Paint header, rows, selection, hover and separators manually.
- Keep asset data source separate from rendering.
- Pair it with `EditorScrollBar`.

A fully custom table is preferred over increasingly complex attempts to skin the native header control.

## 7. Scene Hierarchy tree polish

The hierarchy must visually match the same system as the asset browser.

Required changes:

- Smaller row text.
- Row height around 24–26 px.
- Replace native focus treatment with custom selection highlight.
- Replace or custom-draw expansion arrows so they are subtle and modern.
- Add consistent indentation and icon spacing.
- Selected root row should use the same Nocturne accent system as the target mockup.
- Eliminate any bright/native borders.
- Tree background should visually merge into its panel.
- Custom vertical scrollbar when scrolling becomes necessary.

## 8. Console / Output polish

Required changes:

- Smaller `Cascadia Mono` font.
- Line height approximately 18–20 px.
- Darker console background than surrounding panel.
- Replace native vertical scrollbar with `EditorScrollBar`.
- Remove native EDIT focus border/chrome.
- Maintain selectable/copyable text.
- Keep timestamps muted.
- Keep `[Editor]` accent blue.
- Warnings use muted amber.
- Errors use muted red.
- Long messages should wrap or truncate consistently according to the final console design.

Future enhancement may replace the basic edit control with a richer custom log view, but this pass should first remove the native Windows visual artifacts.

## 9. Search/input field system

The Content Browser search bar and Build/Play fields should share one input style.

Required visual behavior:

- Flat dark input surface.
- 1 px subtle border.
- Rounded corners approximately 6 px.
- No stock `WS_EX_CLIENTEDGE` appearance.
- Smaller body font.
- Placeholder/muted text.
- Accent border on focus, not a Windows focus rectangle.
- Consistent vertical centering.

Introduce a reusable editor input styling/helper instead of one-off painting in each panel.

## 10. Build / Play panel polish

Required changes:

- Reduce heading/body font sizes.
- Tighten vertical spacing between Play Mode, Start Map and VSync.
- Replace native-looking value boxes with the same custom editor input/dropdown surface.
- VSync toggle should stay custom and compact.
- Primary `Play (F5)` button should be flatter and less saturated than the current build.
- `Build` should use the neutral secondary-action style.
- Ensure buttons align exactly and share height/radius.

## 11. Inspector polish

Required changes:

- Reduce title/body font size.
- Increase empty-state breathing room by reducing text size rather than growing margins.
- Muted helper copy should be lower contrast.
- Divider line should be thinner/darker.
- Tips section bullets and line spacing should be more compact.
- Future property rows must use the same input/table design language defined in this pass.

## 12. Panel chrome and global layout

Required changes:

- Reduce the sense of heavy boxes around every region.
- Lower border contrast.
- Keep approximately 8 px panel separation.
- Use one consistent panel radius/border rule.
- Panel titles should be smaller and vertically centered.
- Icons should be subdued and consistently sized.
- Central viewport should remain visually dominant.
- Bottom panels should feel secondary to the viewport and hierarchy/inspector.

Do not add decorative shadows that materially reduce clarity or complicate GDI painting. The target is minimalist, not glassy.

## 13. Viewport placeholder fidelity

The current viewport is close and should receive only subtle refinement:

- Reduce central label/subtitle font size slightly.
- Make watermark `N` more subdued.
- Reduce grid contrast.
- Keep axis gizmo small.
- Make `Perspective / Lit / Show` controls smaller and flatter.
- Future top-right viewport utility buttons should follow the same compact owner-drawn button system.
- Grid badge should be smaller with a subtler border.

No real DX12 viewport implementation belongs in this pass.

## 14. Menu bar

The custom menu bar should be refined to match the target:

- Smaller font.
- Remove strong white borders around menu items.
- Idle menu items should generally not appear boxed.
- Hover/open menu item may use a subtle surface highlight.
- Reduce horizontal padding slightly.
- Keep top bar visually integrated with the window/title bar.

## 15. Status bar

Required changes:

- Reduce text to approximately 12 px.
- Reduce bar height to approximately 26 px.
- Use subtle separators.
- Use muted icons/text except status indicators.
- `Ready` and `No Issues` may retain restrained green accents.
- Branch/object/version information should be visually secondary.
- No native status-bar bevels or borders.

## 16. Reusable UI primitives to introduce

**Design choice (not directly from the book):** avoid continuing to patch raw stock controls independently. Phase 13 should establish a minimal Nocturne tooling UI primitive layer over Win32.

Recommended primitives:

- `EditorTheme` — palette, fonts, metrics.
- `EditorButton` drawing/helper — idle/hover/pressed/active/focus states.
- `EditorInput` styling/helper.
- `EditorScrollBar` custom control.
- `EditorDataTable` custom/owner-drawn table.
- `EditorTreeView` subclass/custom-draw behavior.
- `EditorPanelHeader` painting helper.
- `EditorStatusBar` custom-painted implementation.

These are editor/tooling primitives only. They must not become runtime game UI dependencies.

## 17. Implementation order

Recommended order for the next code pass:

1. Typography/metrics reduction.
2. Button/focus-outline rewrite.
3. Menu bar simplification.
4. Custom scrollbar primitive.
5. Content Browser table replacement/custom draw.
6. Tree hierarchy polish.
7. Console scrollbar/chrome cleanup.
8. Input/search field styling.
9. Build/Play and Inspector compacting.
10. Status bar refinement.
11. Final panel spacing and viewport tuning.

This order attacks the most visually disruptive stock-Windows artifacts first.

## 18. Acceptance checklist

The UI fidelity pass is complete only when all of the following are true:

- [ ] No visible stock white/native button outline remains.
- [ ] Keyboard focus does not render a Vista/Windows-style dotted/default rectangle.
- [ ] No stock Windows scrollbar is visible in editor content areas.
- [ ] Content Browser has a fully dark custom-styled header and rows.
- [ ] No white ListView header/control chrome remains.
- [ ] No stock horizontal scrollbar is visible in the asset table.
- [ ] Scene Hierarchy selection and expand affordances match the Nocturne visual language.
- [ ] Console uses custom-themed scrolling and has no native edge/chrome.
- [ ] Search/inputs are flat dark Nocturne controls.
- [ ] UI font sizes are visibly smaller than the current implementation.
- [ ] Toolbar reads as a compact integrated tool strip.
- [ ] Panel headers and status bar are more subtle than the current build.
- [ ] Viewport remains the dominant visual region.
- [ ] No Phase 14 functionality was introduced.
- [ ] Debug x64 build succeeds with zero errors.
- [ ] Resize behavior remains correct.
- [ ] High-DPI scaling remains usable.

## 19. Common pitfalls

- Do not try to force the target look by stacking more colors on native controls; remove native chrome instead.
- Do not sacrifice mouse hit areas just because visible text and graphics become smaller.
- Do not rely on undocumented system theme hacks for core editor styling.
- Do not introduce a large third-party UI framework in this polish pass without a deliberate architecture decision.
- Do not let custom editor controls leak into runtime/game-facing modules.
- Do not overdraw every surface with borders; the target depends on subtle hierarchy and spacing.

## 20. Book grounding

The existence and role of editor facilities such as world hierarchy/selection, property inspection and integrated asset management remain grounded in Jason Gregory, *Game Engine Architecture (3rd Edition)*, Chapter 15.4 and its world-editor discussion.

The exact typography, color palette, custom scrollbars, owner-drawn controls, table presentation and visual polish described in this document are **Design choice (not directly from the book)**.

## 21. Next implementation handoff

Implement this document as **Phase 13 UI Fidelity Pass 2** on `phase-13-editor-framework`, beginning with typography, buttons, custom scrollbars and the Content Browser table. Do not begin Phase 14 until this acceptance checklist is visually satisfied.