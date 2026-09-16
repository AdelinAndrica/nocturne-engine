# Phase 14 — Editor Rendering Viewport

## 1. Obiectivul fazei

Phase 14 va introduce primul viewport 3D real al Nocturne Editor, randat cu DX12 într-un **child `HWND` dedicat** aflat exclusiv în zona Viewport din `EditorShellV3`.

Regulile tale se potrivesc cu arhitectura deja stabilită: editorul rămâne client al runtime-ului, runtime-ul păstrează loop-ul autoritar, iar editorul nu devine un al doilea engine. Contractul existent spune explicit că editorul reutilizează rendering/resources/gameplay foundation și nu trebuie să înlocuiască sau să bifurce engine loop-ul. 

**Nu încep încă modificarea codului.** Motivul este concret: setul de fișiere accesibil în acest chat nu conține `Docs/Phase 13 — Completion Report.md`, `Docs/Phase 14 — Editor Rendering Viewport Handoff.md` și nici implementarea `EditorShellV3`. Căutarea după `EditorShellV3` nu găsește codul respectiv; documentul consolidat disponibil se oprește practic la implementările engine din fazele anterioare. Nu voi inventa ownership-ul Phase 13.

Totuși, ownership-ul runtime anterior poate fi stabilit suficient de precis pentru a fixa arhitectura corectă a Phase 14.

---

# 2. Architecture inspection — ce este confirmat acum

| Zonă | Ownership actual confirmat | Consecință pentru Phase 14 |
|---|---|---|
| `Engine` | deține `InputSystem`, `RenderSystem`, `World` | Editorul trebuie să consume aceste sisteme, nu să creeze duplicate |
| `RenderSystem` | `Impl` deține un `Dx12Renderer` | viewport-ul trebuie să intre prin renderer-ul existent |
| `Dx12Renderer` | deține `Dx12Device`, `Dx12SwapChain`, `Dx12FrameSync`, command allocators/list, descriptor heaps, deferred releases, passes | **nu creăm device/queue/fence #2** |
| Swap chain | `Dx12Renderer::AttachToWindow()` creează swap chain pentru HWND-ul primit | în editor acel HWND trebuie să fie child-ul Viewport, nu shell-ul top-level |
| Frame sync | fence-ul și frame index-ul sunt renderer-owned | resize-ul viewport-ului trebuie sincronizat prin același mecanism |
| `WinWindow` | deține HWND-ul platform window și redirecționează Raw Input | child viewport-ul are nevoie de lifecycle Win32 separat de top-level |
| Main loop | `Engine::BeginFrame → Tick → EndFrame` este fluxul autoritar | editor update intră ca hook/client al aceluiași loop |
| `World` | runtime-owned; stable handles, transforms, bounds, renderables și cameră internă | editor camera **nu trebuie introdusă ca scene object runtime** |
| Input | snapshot per frame + Raw Input + mouse deltas + key states | editor navigation trebuie doar să gate-uiască consumul inputului |
| Render handoff | `World::BuildRenderQueue()` produce date frame-local pentru Render | editorul trebuie să păstreze acest boundary, nu să lase renderer-ul să inspecteze `World` |

`Engine` deține explicit `RenderSystem` și `World`, iar `Engine::Tick()` actualizează input/resources/world; `Engine::EndFrame()` construiește `RenderQueue` și îl pasează renderer-ului.  

`RenderSystem` este un wrapper subțire peste `Dx12Renderer`, iar `Dx12Renderer` deține device-ul, swap chain-ul și frame synchronization-ul.  

### Problemă importantă descoperită în baseline-ul vechi

`Engine::EndFrame()` din implementarea disponibilă construiește coada cu:

```cpp
const uint32_t viewportW = 1280;
const uint32_t viewportH = 720;
```

Acest hard-code trebuie eliminat în Phase 14; dimensiunea trebuie să provină din render surface-ul efectiv. 

Mai important, actualul `MeshPass` dezactivează depth testing:

```cpp
pso.DepthStencilState.DepthEnable = FALSE;
```

și atașează numai RTV, fără DSV. Asta nu este suficient pentru viewport 3D real, grid/gizmos și selecție corectă. 

Luna tratează explicit swap chain-ul, CPU/GPU synchronization, RTV, depth/stencil, viewport și scissor ca părți distincte ale bootstrap-ului Direct3D 12, iar exemplele de draw folosesc atât RTV cât și DSV și tranzițiile back buffer-ului.  

---

# 3. Key concepts din cărți

Pentru această fază, fundamentele relevante sunt:

- **Game loop-ul rămâne orchestratorul central.** Rendering-ul este o etapă în frame, nu proprietarul aplicației. Gregory tratează separat rendering loop/game loop și arhitecturile de loop. 
- **Editor/debug camera trebuie separată de camera gameplay.** Gregory dedică explicit o secțiune debug cameras și tratează editorul de world ca tooling separat de runtime gameplay. 
- **Debug draw este engine/tool functionality legitimă**, nu gameplay/UI. Gregory îl tratează ca facilitate separată de debugging/development. 
- **Renderer-ul consumă geometrie vizibilă și configurează explicit state-ul GPU.** Gregory subliniază submission-ul primitivelor, command lists și evitarea state leaks. 
- **Projection/view/viewport sunt transformări distincte.** Lengyel descrie transformarea camera → clip → normalized device → viewport, esențială atât pentru rendering cât și pentru mouse picking. 
- **Rays sunt primitive geometrice naturale pentru queries/picking.** Lengyel Volume 1 tratează explicit lines and rays în geometria de bază. 
- DX12 cere lifecycle corect pentru **swap chain, descriptors, depth buffer, viewport/scissor, command submission și synchronization**. 

---

# 4. Ce implementăm acum

## A. Dedicated Viewport HWND

**Design choice (not directly from the book).**

Nu atașăm `Dx12SwapChain` la HWND-ul principal al editorului.

Structura țintă:

```text
Editor top-level HWND
└── EditorShellV3
    └── Viewport panel content area
        └── dedicated child HWND
            └── DXGI swap chain
                ├── back buffers / RTVs
                └── viewport depth target / DSV
```

Nu introducem multi-window rendering generic și nici un sistem arbitrar de N swap chains. Phase 14 are un singur viewport real; deci extensia minimă este **un singur swap chain existent, atașat child HWND-ului**.

Pentru jocul standalone, același renderer poate continua să fie atașat top-level game HWND.

### Platform boundary

Prefer:

```text
Engine/Platform/Win32/
    WinWindow.*          // top-level application window
    WinChildWindow.*     // generic child rendering window
```

și nu Win32 brut împrăștiat prin renderer.

**Design choice (not directly from the book).**

Nu aș transforma `WinWindow` într-o clasă plină de condiții `if (isChild)`, deoarece semantics pentru `WM_CLOSE`, quit, ownership și raw input diferă.

---

## B. Păstrăm exact un device / queue / synchronization domain

Nu:

```text
Editor renderer
Game renderer
```

ci:

```text
Engine
└── RenderSystem
    └── Dx12Renderer
        ├── Dx12Device
        ├── Direct Command Queue
        ├── Dx12FrameSync
        ├── command allocators/list
        ├── descriptor heaps
        └── Dx12SwapChain -> Viewport child HWND
```

Asta este cea mai mică schimbare compatibilă cu ownership-ul actual. 

---

## C. Resize lifecycle

Resize-ul **nu trebuie executat direct din `WM_SIZE`**.

**Design choice (not directly from the book).**

`WM_SIZE` doar publică dimensiunea dorită:

```cpp
renderer.RequestResize(width, height);
```

iar la un punct sigur de frame:

```text
BeginFrame
    ↓
apply pending viewport resize
    ↓
render
    ↓
present
```

Resize efectiv:

```text
ignore 0×0
↓
wait/flush GPU
↓
release old back-buffer references
↓
release old depth resource
↓
ResizeBuffers
↓
query new frame index
↓
recreate RTVs
↓
recreate depth texture + DSV
↓
update width/height
```

Nu recreăm device-ul, queue-ul sau renderer-ul.

---

## D. Depth target

Phase 14 trebuie să adauge:

```text
DXGI_FORMAT_D32_FLOAT
```

ca baseline simplu.

**Design choice (not directly from the book).**

Lifecycle-ul lui este identic dimensional cu viewport-ul:

```text
Attach viewport
    -> CreateDepth(width,height)

Resize viewport
    -> DestroyDepth()
    -> CreateDepth(newWidth,newHeight)

Shutdown
    -> DestroyDepth()
```

`MeshPass` devine:

```cpp
OMSetRenderTargets(1, &rtv, FALSE, &dsv);

ClearRenderTargetView(...);
ClearDepthStencilView(...);

DepthEnable = TRUE;
DepthWriteMask = ALL;
DepthFunc = LESS;
```

Această structură urmează pipeline-ul DX12 descris de Luna. 

---

## E. Editor Camera

Camera editorului stă în:

```text
Tools/Editor/Viewport/
    EditorCamera
    EditorCameraController
```

Nu în `World`.

**Design choice (not directly from the book).**

Exemplu de stare:

```cpp
struct EditorCamera
{
    Vec3 position;
    Quat orientation;

    float fovY;
    float nearZ;
    float farZ;

    Mat4 View() const;
    Mat4 Projection(float aspect) const;
};
```

`World` continuă să conțină camera runtime existentă. Editorul doar furnizează o **view override** pentru render-ul curent.

Asta păstrează principiul „camera produce view data, renderer consumes view data” deja documentat în arhitectura Nocturne. 

---

## F. Mică extensie la `World::BuildRenderQueue`

Astăzi `BuildRenderQueue()` reconstruiește camera internă a World-ului. 

Pentru editor nu trebuie să mutăm camera editorului în World.

Propun:

```cpp
struct RenderView
{
    Mat4 view;
    Mat4 projection;
    Mat4 viewProjection;

    Vec3 cameraPosition;

    uint32_t viewportWidth;
    uint32_t viewportHeight;
};
```

și:

```cpp
RenderQueue World::BuildRenderQueue(
    LinearArena& frameArena,
    const RenderView& view);
```

Runtime:

```text
World runtime camera
    -> RenderView
    -> BuildRenderQueue()
```

Editor:

```text
EditorCamera
    -> RenderView
    -> BuildRenderQueue()
```

Aceasta este **Design choice (not directly from the book)**, dar păstrează separarea renderer/world deja stabilită.

---

## G. MainLoop integration

Nu există:

```text
EditorLoop
EngineLoop
```

Trebuie să existe:

```text
noc::MainLoop
    ProcessMessages
    Engine::BeginFrame
    Engine::Tick
    Editor frame hook
    Engine::EndFrame
```

**Design choice (not directly from the book):** dacă Phase 13 nu are deja un mecanism echivalent, aș introduce un hook generic în `MainLoop`, nu o dependență pe `EditorShellV3`.

De exemplu:

```cpp
struct MainLoopHooks
{
    void* context = nullptr;
    void (*afterEngineTick)(void* context, Engine& engine) = nullptr;
};
```

Editorul implementează hook-ul.

Runtime-ul nu include niciun header din `Tools/Editor`.

Înainte de implementare trebuie însă verificat **cum face Phase 13 deja bridge-ul**; nu voi introduce acest hook dacă `EditorShellV3` are deja un mecanism echivalent.

---

## H. Input și editor navigation

Sistemul actual produce mouse delta și snapshots o dată pe frame. 

Nu avem nevoie de al doilea input system.

`EditorCameraController` consumă input **numai** când viewport interaction state permite:

```text
Viewport child focused
        +
RMB/navigation active
        ↓
mouse delta -> yaw/pitch
WASDQE     -> translation
```

Pentru picking:

```text
LMB inside viewport
+ not dragging gizmo
    ↓
pick ray
```

RMB navigation poate utiliza mouse capture pe child HWND până la release.

**Design choice (not directly from the book).**

Nu modificăm gameplay `ActionMap` pentru editor controls în această fază.

---

## I. Picking

Vom folosi exact reprezentarea de scenă existentă, nu Physics Phase 18 și nu ECS Phase 15.

API minim:

```cpp
struct WorldRayHit
{
    SceneObjectHandle object;
    float distance;
};

bool World::RaycastRenderableBounds(
    const Ray& ray,
    WorldRayHit& hit);
```

Algoritm:

```text
mouse client coordinates
        ↓
viewport normalized coordinates
        ↓
editor-camera world ray
        ↓
ray vs world AABB pentru fiecare renderable
        ↓
cel mai mic t >= 0
        ↓
SceneObjectHandle selected
```

`World` are deja `RenderableData::worldBounds`, stable handles și determinism. 

### Important

Phase 14 picking va fi **bounds picking**, nu per-triangle picking.

**Design choice (not directly from the book).**

Este suficient pentru single-object editor selection și evită introducerea prematură a collision/physics infrastructure.

---

## J. Selection ownership

Selection nu aparține World-ului.

```text
Tools/Editor/
    EditorSelection
        SceneObjectHandle selected_
```

Atât Viewport cât și Scene Hierarchy folosesc **aceeași instanță**:

```text
Viewport click ───────┐
                     ├── EditorSelection
Hierarchy click ─────┘
```

Apoi:

```text
EditorSelection
  ├── hierarchy highlight
  ├── selection bounds debug draw
  └── transform gizmo target
```

**Design choice (not directly from the book).**

Asta nu introduce ECS, inspector generalizat sau scene authoring.

---

## K. Transform Gizmos

Scope-ul Phase 14:

```cpp
enum class GizmoMode
{
    Translate,
    Rotate,
    Scale
};
```

și:

```cpp
class TransformGizmo
{
    GizmoMode mode_;
    GizmoAxis hovered_;
    GizmoAxis active_;
};
```

Toolbar-ul Phase 13 doar schimbă `mode_`.

Nu redesenăm toolbar-ul.

### API World minim necesar

Există deja:

```cpp
SetLocalTRS(...)
GetWorldMatrix(...)
```



Probabil va trebui adăugat strict:

```cpp
bool GetLocalTRS(
    SceneObjectHandle,
    Vec3& position,
    Quat& rotation,
    Vec3& scale) const;
```

și eventual:

```cpp
bool GetWorldBounds(SceneObjectHandle, AABB&) const;
```

Nu introducem component inspection sau generic component modification.

---

## L. Debug Draw

Gregory tratează debug drawing ca sistem de development distinct. 

Propun un mecanism generic sub Render:

```text
Engine/Render/Debug/
    DebugDraw.h
    DebugDraw.cpp

Engine/Render/DX12/
    DebugLinePass.*
```

Nu sub editor UI.

Editorul îl utilizează pentru:

```text
grid
world axes
selection AABB
translate gizmo
rotate gizmo
scale gizmo
ray/debug visualization dacă este necesar
```

Renderer-ul primește doar date POD pentru linii.

**Design choice (not directly from the book):** implementarea exactă ca dynamic line vertex buffer per frame.

---

# 5. Implementation steps

Ordinea recomandată este strictă:

1. **Finalizează inspection-ul Phase 13 real**
   - `EditorShellV3`;
   - editor/runtime bootstrap;
   - editor message pump;
   - Scene Hierarchy selection state;
   - viewport panel geometry;
   - existing toolbar command routing.

2. **Decuplează attach-ul input/render dacă Phase 13 încă folosește `Engine::AttachWindow()`**
   - păstrăm convenience path pentru runtime;
   - editor render target primește child HWND.

3. **Adaugă child viewport HWND**
   - create/destroy;
   - resize/move după content rect-ul existent;
   - zero redesign al shell-ului.

4. **Atașează swap chain-ul existent la child HWND**.

5. **Adaugă deferred resize lifecycle**.

6. **Adaugă depth resource + DSV**
   - enable depth în scene PSO;
   - clear color + depth;
   - resize-safe.

7. **Elimină viewport dimensions hard-coded**
   - dimensiunile vin din render surface.

8. **Introdu `RenderView` explicit**
   - runtime camera path continuă să funcționeze;
   - editor camera poate furniza view separat.

9. **Adaugă `EditorCamera` + controller**
   - RMB look;
   - WASD;
   - Q/E vertical;
   - focus/capture gating.

10. **Adaugă minimal World query API**
    - world bounds;
    - local TRS;
    - ray/AABB nearest-hit.

11. **Adaugă `EditorSelection` shared state**
    - viewport ↔ Scene Hierarchy.

12. **Adaugă `DebugDraw` + `DebugLinePass`**
    - grid;
    - axes;
    - selection bounds.

13. **Adaugă gizmo foundation**
    - toolbar modes;
    - axis rendering;
    - axis hit testing;
    - Translate/Rotate/Scale interaction limitată la obiectul selectat.

14. **Rulează DX12 debug layer**
    - resize repetat;
    - maximize/minimize;
    - panel foarte mic;
    - camera movement;
    - selection;
    - gizmo drag;
    - shutdown.

---

# 6. Verification checklist

Phase 14 nu este completă până când:

- [ ] DXGI swap chain este legat exclusiv la **Viewport child HWND**.
- [ ] Top-level `EditorShellV3` nu devine render target DX12.
- [ ] Nu există un al doilea `MainLoop`.
- [ ] Nu există al doilea DX12 device/queue/fence inutil.
- [ ] Redimensionarea panelului produce resize corect al back buffers.
- [ ] `0×0` / hidden/minimized nu produce DXGI errors.
- [ ] Depth buffer este recreat la fiecare resize relevant.
- [ ] Viewport/scissor urmează dimensiunea reală.
- [ ] Scene geometry are depth testing activ.
- [ ] Editor camera nu mută camera gameplay/runtime.
- [ ] Camera primește input numai în contextul viewport-ului.
- [ ] Click-ul selectează nearest renderable AABB hit.
- [ ] Click pe spațiu gol deselectează.
- [ ] Viewport selection și Scene Hierarchy arată același handle.
- [ ] Grid și axes sunt vizibile.
- [ ] Obiectul selectat are visualization clară.
- [ ] Translate / Rotate / Scale din toolbar schimbă gizmo mode fără redesign.
- [ ] Gizmo nu cere ECS.
- [ ] Nicio funcție Phase 16/17/27 nu a fost introdusă.
- [ ] DX12 debug layer raportează zero lifetime/state/synchronization errors.
- [ ] baseline-ul vizual Phase 13 rămâne pixel-identic în afara conținutului interior al Viewport-ului.

---

# 7. Common pitfalls

Cel mai periculos ar fi să facem `EditorShellV3` proprietar direct al `ID3D12Device` sau swap chain-ului. Ar crea două centre de ownership și ar încălca layering-ul deja stabilit.

Alte probleme de evitat: atașarea swap chain-ului la top-level editor HWND „doar temporar”; `ResizeBuffers()` din `WM_SIZE`; recrearea device-ului la resize; resetarea unui command allocator înainte ca fence-ul lui să fi terminat; păstrarea depth buffer-ului la vechea dimensiune; folosirea camerei `World` ca editor camera; introducerea unui al doilea input manager; picking prin Physics înainte de Phase 18; stocarea selection-ului separat în Hierarchy și Viewport; introducerea ECS doar pentru gizmos; și rescrierea toolbar/panel chrome din Phase 13.

---

# Decizia arhitecturală Phase 14

Ținta pe care aș fixa-o este:

```text
EditorShellV3
│
├── existing editor panels / chrome             UNCHANGED
│
└── ViewportPanel
    │
    ├── WinChildWindow
    │      └── HWND viewport
    │
    ├── EditorCamera
    ├── EditorCameraController
    ├── EditorSelection ───────── SceneHierarchy
    └── TransformGizmo
             │
             └── DebugDraw commands
                      │
                      ▼
noc::MainLoop
    │
    ├── Engine::BeginFrame
    ├── Engine::Tick
    ├── editor frame hook
    └── Engine::EndFrame
             │
             ▼
World::BuildRenderQueue(RenderView)
             │
             ▼
RenderSystem
    └── Dx12Renderer
        ├── ONE Dx12Device
        ├── ONE command queue
        ├── ONE Dx12FrameSync
        ├── Dx12SwapChain ───── viewport child HWND
        ├── depth target
        ├── MeshPass
        └── DebugLinePass
```

Aceasta este extensia pe care aș considera-o **cea mai mică fără compromis arhitectural**.

## Next chat handoff

Pentru următorul pas, adu exact:

> `Docs/Phase 13 — Completion Report.md`, `Docs/Phase 14 — Editor Rendering Viewport Handoff.md` și source-ul curent pentru `EditorShellV3`, editor bootstrap/main, `MainLoop`, `Engine`, `WinWindow`, `RenderSystem`, `Dx12Renderer`, `Dx12SwapChain`, `Dx12FrameSync`, `World` și `InputSystem`. Continuăm Phase 14 de la architecture inspection și implementăm numai după reconcilierea acestui design cu codul real.