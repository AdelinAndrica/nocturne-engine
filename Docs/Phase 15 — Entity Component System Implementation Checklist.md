# Phase 15 — Entity / Component System — Checklist complet

**Obiectiv:** înlocuim modelul temporar în care `World` conține hard-coded `TransformData`, `RenderableData` și camera special-case cu fundația reală de entity/component pe care se vor baza editorul, serialization, physics, scripting și gameplay-ul.

**Grounding principal:** Gregory, *Game Engine Architecture* §16.2 — runtime object models / component & property-centric architectures și §16.5 — object references / world queries; Nystrom — *Component*, *Data Locality*, *Dirty Flag*; Lengyel Vol. 2 §5.4.2 — transform hierarchy.

**Design choice (not directly from the book):** pentru Nocturne, direcția potrivită pentru Phase 15 este un model **entity ID + generational handle + component storage per type**, cu storage dens pentru componente și fără archetype/chunk ECS în această fază.

---

## 1. Audit și cleanup înainte de implementare

- [ ] Confirmăm că branch-ul activ este `phase-15-entity-component-system`.
- [ ] Păstrăm `EditorShellV3` ca shell activ.
- [ ] Păstrăm dedicated child-HWND viewport din Phase 14.
- [ ] Păstrăm un singur `noc::MainLoop`.
- [ ] Păstrăm `Engine` ca owner al `World`.
- [ ] Păstrăm actualul runtime → `RenderQueue` → renderer flow.
- [ ] Păstrăm comportamentul vizual și interactiv validat în Phase 14.
- [ ] Inventariem toate utilizările lui `SceneObjectHandle`.
- [ ] Inventariem toate apelurile:
  - [ ] `World::CreateObject()`
  - [ ] `World::DestroyObject()`
  - [ ] `World::SetLocalTRS()`
  - [ ] `World::SetParent()`
  - [ ] `World::GetWorldMatrix()`
  - [ ] `World::SetRenderable()`
  - [ ] `World::SetCameraParams()`
  - [ ] `World::SetCameraFromObject()`
  - [ ] `World::AliveCount()`
- [ ] Inventariem dependențele editorului de structura temporară `ValidationObject`.
- [ ] Identificăm duplicate mutable state în `EditorViewportController`.
- [ ] Eliminăm ideea că `ValidationObject::t/r/s` poate rămâne autoritatea transformului.
- [ ] Runtime component data devine single source of truth.
- [ ] Verificăm `Engine/Runtime/VisibilitySystem.*`.
- [ ] Verificăm `Engine/Runtime/RenderPackets.h`.
- [ ] Confirmăm că acestea sunt actualmente dead/uncompiled code.
- [ ] Eliminăm sau reconciliem codul ECS vechi/incompatibil.
- [ ] Nu lăsăm două concepte diferite de `EntityHandle` în repository.
- [ ] Verificăm `Engine/Runtime/Transform.h`, care conține un model vechi/incompatibil de `Camera`.
- [ ] Eliminăm sau consolidăm header-ele runtime istorice care ar deveni ambigue.

---

# 2. Entity identity

## `EntityHandle`

- [ ] Introducem tipul runtime oficial `EntityHandle`.
- [ ] Handle-ul conține minimum:
  - [ ] `index`
  - [ ] `generation`
- [ ] Definim explicit valoarea invalidă.
- [ ] `EntityHandle{}` trebuie să fie invalid implicit.
- [ ] Implementăm:
  - [ ] `IsValid()`
  - [ ] `operator==`
  - [ ] `operator!=`
- [ ] Handle-ul nu conține pointer.
- [ ] Handle-ul nu depinde de adresa unui component.
- [ ] Handle-ul rămâne valid dacă storage-ul intern se realocă.
- [ ] Handle-ul stale nu poate identifica o entitate nouă care reutilizează același slot.
- [ ] Generația se incrementează la destroy/reuse.
- [ ] Definim comportamentul la overflow de generation.
- [ ] Nu permitem revenirea accidentală la generation invalidă.
- [ ] Development build detectează situațiile imposibile prin assert/log unde este util.
- [ ] Shipping build nu corupe memorie la handle invalid.

### Stable identity pentru Phase 17

- [ ] Facem distincția explicită între:
  - [ ] runtime transient `EntityHandle`;
  - [ ] future persistent entity identity.
- [ ] **Nu** serializăm `index`.
- [ ] **Nu** serializăm `generation`.
- [ ] **Nu** tratăm runtime handle-ul ca persistent scene ID.
- [ ] Pregătim API/data layout astfel încât Phase 17 să poată adăuga identitate persistentă fără rescrierea ECS-ului.

---

# 3. Entity registry / entity table

- [ ] `World` deține entity registry-ul.
- [ ] Entity registry-ul deține:
  - [ ] generation per slot;
  - [ ] alive/dead state;
  - [ ] free-list;
  - [ ] alive count.
- [ ] `CreateEntity()` alocă sau reutilizează un slot.
- [ ] `DestroyEntity()` invalidează handle-ul.
- [ ] Slot reutilizat primește generation nouă.
- [ ] `IsAlive(EntityHandle)` validează:
  - [ ] handle valid;
  - [ ] index în range;
  - [ ] slot alive;
  - [ ] generation match.
- [ ] Nu există acces direct la entity slots din afara `World`.
- [ ] Capacity growth este controlat.
- [ ] Growth nu invalidează `EntityHandle`.
- [ ] Entity creation are comportament determinist.
- [ ] Alive entity iteration are ordine documentată.
- [ ] Reuse al sloturilor nu produce duplicate în iteration.
- [ ] Destroy de două ori este tratat explicit.
- [ ] Destroy al unui stale handle este tratat explicit.
- [ ] Destroy al unui invalid handle este tratat explicit.
- [ ] `AliveCount()` rămâne corect în toate cazurile.
- [ ] Shutdown elimină toate entitățile și componentele fără leaks.

---

# 4. Component model

Gregory permite mai multe modele și spune explicit că nu există o variantă universal superioară. Nystrom motivează separarea domeniilor prin Component.

**Design choice (not directly from the book):** Nocturne Phase 15 folosește component pools separate per component type, nu `GameObject` cu pointeri hard-coded și nu archetype ECS.

Implementăm componentele foundation:

- [ ] `TransformComponent`
- [ ] `RenderableComponent`
- [ ] `CameraComponent`
- [ ] `NameComponent`

Fiecare componentă:

- [ ] aparține exact unei entități;
- [ ] poate fi găsită prin `EntityHandle`;
- [ ] are lifetime gestionat de `World`;
- [ ] nu stochează pointer stabil către alte componente;
- [ ] nu presupune adrese stabile în storage;
- [ ] poate fi adăugată/eliminată fără modificarea structurii `Entity`;
- [ ] poate fi iterată independent de celelalte componente.

---

# 5. Generic component storage foundation

**Design choice (not directly from the book):** folosim dense component storage + entity→component lookup.

Pentru fiecare component pool:

- [ ] dense array de componente.
- [ ] dense array de owner `EntityHandle` sau entity index.
- [ ] lookup entity → dense component index.
- [ ] invalid sentinel pentru entity fără componentă.
- [ ] `Add()`.
- [ ] `Remove()`.
- [ ] `Has()`.
- [ ] `Get()`.
- [ ] `TryGet()`.
- [ ] const/non-const access.
- [ ] `Count()`.
- [ ] deterministic iteration semantics.
- [ ] growth policy.
- [ ] shutdown/destruction.
- [ ] no uncontrolled allocation during normal frame iteration.

### Swap-remove

Dacă folosim swap-remove:

- [ ] componenta eliminată este destructed corect.
- [ ] ultima componentă este mutată în slotul liber.
- [ ] lookup-ul owner-ului mutat este actualizat.
- [ ] niciun pointer extern nu este considerat permanent.
- [ ] API-ul documentează invalidarea pointerilor/referințelor după mutation.
- [ ] teste specifice verifică swap-remove.

### Component duplicate rules

- [ ] O entitate poate avea maximum un `TransformComponent`.
- [ ] Maximum un `RenderableComponent`.
- [ ] Maximum un `CameraComponent`.
- [ ] Maximum un `NameComponent`.
- [ ] Duplicate add nu creează două componente.
- [ ] Duplicate add are policy explicit:
  - [ ] return failure;
  - [ ] Development diagnostic.
- [ ] Remove absent component este safe și documentat.

---

# 6. Component registration / type identity

Phase 15 trebuie să fie serialization-ready și editor-ready, fără a implementa serialization propriu-zis.

- [ ] Definim `ComponentTypeId`.
- [ ] Type ID este stabil în interiorul contractului runtime.
- [ ] Evităm type identity bazată pe:
  - [ ] pointer;
  - [ ] RTTI address;
  - [ ] component storage address.
- [ ] Fiecare component type foundation primește ID unic.
- [ ] Duplicate registration este detectată.
- [ ] Unknown type este tratat safe.
- [ ] Registration order este determinist sau independent de rezultat.

---

# 7. Component metadata

Trebuie să pregătească Phase 16 Inspector și Phase 17 serialization.

- [ ] Introducem metadata minimă per component type.
- [ ] Metadata include:
  - [ ] type ID;
  - [ ] canonical type name;
  - [ ] version;
  - [ ] size/alignment unde este relevant;
  - [ ] capabilities/flags necesare.
- [ ] Numele componentelor sunt stabile și unice.
- [ ] Component version începe explicit de la o versiune definită.
- [ ] Version nu este implicită sau dedusă din `sizeof`.
- [ ] Schema poate fi enumerată de editor în viitor.
- [ ] Phase 15 nu implementează încă full reflection framework dacă nu este necesar.
- [ ] Phase 15 nu implementează scene serialization.
- [ ] Phase 15 nu implementează migration logic de fișiere.

**Design choice (not directly from the book):** metadata registry-ul exact și schema de versioning sunt mecanisme Nocturne.

---

# 8. TransformComponent

Transform-ul este fundația spațială.

- [ ] `TransformComponent` stochează local translation.
- [ ] local rotation.
- [ ] local scale.
- [ ] cached world matrix.
- [ ] parent entity.
- [ ] child linkage sau structură echivalentă.
- [ ] dirty state.
- [ ] Nu stochează `SceneObjectHandle`.
- [ ] Folosește `EntityHandle`.

### Default state

- [ ] translation = zero.
- [ ] rotation = identity.
- [ ] scale = one.
- [ ] parent = invalid.
- [ ] world matrix = identity sau TRS default.
- [ ] dirty state corect.

---

# 9. Transform hierarchy

Lengyel §5.4.2: transform hierarchy trebuie să fie un arbore.

- [ ] Entitate fără parent este root.
- [ ] Parent trebuie să fie alive.
- [ ] Parent trebuie să aibă `TransformComponent`.
- [ ] Child trebuie să aibă `TransformComponent`.
- [ ] Nu permitem entity → itself parenting.
- [ ] Nu permitem cycle direct.
- [ ] Nu permitem cycle indirect.
- [ ] Cycle detection verifică întreg parent chain.
- [ ] Reparent la același parent este no-op.
- [ ] Reparent detach-uiește corect parent-ul vechi.
- [ ] Reparent atașează corect parent-ul nou.
- [ ] Reparent marchează subtree dirty.
- [ ] Parent transform change invalidează descendants.
- [ ] Transform update produce world transforms corecte top-down.
- [ ] Update order este determinist.
- [ ] Nu calculăm child înaintea parent-ului.
- [ ] Hierarchy traversal nu depinde de component storage order.

### Destroy parent semantics

Trebuie să existe o politică explicită.

**Design choice (not directly from the book):**

- [ ] Alegem și documentăm ce se întâmplă cu copiii când parent-ul este destroyed.
- [ ] Recomandare pentru Phase 15: children devin roots.
- [ ] Child local/world behavior după detach este explicit.
- [ ] Decidem dacă păstrăm local transform sau world transform.
- [ ] Scriem test pentru comportamentul ales.
- [ ] Nicio entitate child nu rămâne cu parent stale.

### Removing Transform

- [ ] Nu permitem remove Transform dacă ar lăsa hierarchy links corupte.
- [ ] Definim ce se întâmplă cu children.
- [ ] Renderable fără Transform este legal sau ilegal — contract explicit.
- [ ] Camera fără Transform este legală sau ilegală — contract explicit.
- [ ] Recomandat: Renderable/Camera depend logic de Transform pentru world use, dar component storage nu trebuie să se corupă dacă lipsește.

---

# 10. Dirty transform propagation

Nystrom — Dirty Flag.

- [ ] Mutarea local transform marchează entitatea dirty.
- [ ] Descendants ajung să fie recalculate.
- [ ] Un transform nemodificat nu este recalculat inutil.
- [ ] Nu facem permanent full-world recompute dacă nu este necesar.
- [ ] Dirty propagation nu produce infinite recursion.
- [ ] Deep hierarchy este testată.
- [ ] Wide hierarchy este testată.
- [ ] Iterative implementation este evaluată pentru stack safety.
- [ ] World matrices sunt valide înainte de render extraction.
- [ ] Bounds derivată din transform este actualizată după transform propagation.

---

# 11. RenderableComponent

- [ ] Separăm renderable data de entity identity.
- [ ] `RenderableComponent` conține minimum:
  - [ ] mesh resource handle;
  - [ ] local bounds;
  - [ ] enabled/visible state dacă este necesar.
- [ ] Derived world bounds nu devine duplicate authoritative transform data.
- [ ] World bounds este cache derivat.
- [ ] Mesh este `ResourceHandle`, nu raw pointer.
- [ ] Invalid/missing mesh este safe.
- [ ] Renderable poate fi adăugat după entity creation.
- [ ] Renderable poate fi eliminat.
- [ ] Removal elimină obiectul din render extraction.
- [ ] Disabled renderable nu este extras pentru renderer.
- [ ] Entity destroy elimină automat RenderableComponent.

---

# 12. CameraComponent

Actuala cameră special-case din `World::Impl` trebuie eliminată ca model special.

- [ ] `CameraComponent` devine component type normal.
- [ ] Conține:
  - [ ] FOV;
  - [ ] near plane;
  - [ ] far plane;
  - [ ] aspect policy/value necesar;
  - [ ] enabled/active relevant state.
- [ ] Camera view este derivată din `TransformComponent`.
- [ ] View/projection/viewProj nu devin alternate authoritative transform.
- [ ] Active camera identity este `EntityHandle`.
- [ ] `World` poate seta/selecta camera activă.
- [ ] Active camera stale după destroy este detectată.
- [ ] Destroy camera entity curăță active camera reference.
- [ ] Remove CameraComponent curăță active camera dacă este necesar.
- [ ] Viewport resize actualizează aspect corect.
- [ ] Phase 14 fly camera continuă să funcționeze identic.

---

# 13. NameComponent

- [ ] Introducem `NameComponent`.
- [ ] Name aparține runtime entity data.
- [ ] Nu folosim label-uri hard-coded ca autoritate în editor.
- [ ] Default name policy este explicit.
- [ ] Empty name este suportat sau respins explicit.
- [ ] Max length / storage policy este documentată.
- [ ] Rename API nu expune STL dacă păstrăm contractul public actual.
- [ ] Name component este pregătit pentru hierarchy și Inspector Phase 16.
- [ ] Entity identity nu depinde de name.
- [ ] Duplicate names sunt permise dacă nu există cerință contrară.

---

# 14. Public World API nou

World trebuie să devină orchestration API peste entities/components, nu structura monolitică hard-coded.

Checklist conceptual:

- [ ] `CreateEntity()`.
- [ ] `DestroyEntity()`.
- [ ] `IsAlive()`.
- [ ] `AliveCount()`.
- [ ] entity enumeration.
- [ ] component add.
- [ ] component remove.
- [ ] component has.
- [ ] component get/try-get.
- [ ] transform manipulation.
- [ ] parenting.
- [ ] active camera management.
- [ ] world update.
- [ ] render extraction.

Public API review:

- [ ] fără STL în public headers, conform contractului Nocturne.
- [ ] fără Win32 types.
- [ ] fără editor types.
- [ ] fără DX12 types.
- [ ] failure semantics clare.
- [ ] const-correctness.
- [ ] stale handle semantics clare.
- [ ] no hidden ownership.

---

# 15. Query model

Gregory §16.5 tratează world queries ca parte fundamentală a runtime object model.

Phase 15 nu are nevoie de un query DSL complex.

**Design choice (not directly from the book):** query API minimalist și determinist.

- [ ] Enumerare toate entities.
- [ ] Enumerare entities cu Transform.
- [ ] Enumerare entities cu Renderable.
- [ ] Enumerare Transform + Renderable intersection.
- [ ] Enumerare cameras.
- [ ] Lookup entity by handle.
- [ ] Lookup component by entity.
- [ ] Queries nu returnează destroyed entities.
- [ ] Queries nu returnează stale component slots.
- [ ] Mutation during iteration are policy explicit.
- [ ] Query result ordering este documentat.
- [ ] Query path hot nu alocă heap per frame.
- [ ] Render extraction nu construiește temporary STL collections per frame.

---

# 16. Deterministic behavior

- [ ] Entity creation determinist.
- [ ] Entity destruction determinist.
- [ ] Component iteration order determinist sau explicit unspecified.
- [ ] Transform propagation determinist.
- [ ] Render extraction determinist.
- [ ] Same world state → same render-instance order.
- [ ] Entity slot reuse nu schimbă aleator output-ul.
- [ ] Component swap-remove nu introduce nondeterminism necontrolat.
- [ ] Tests verifică ordering unde contractul îl cere.

---

# 17. Entity destruction cascade

La `DestroyEntity()`:

- [ ] validăm entity handle.
- [ ] gestionăm transform hierarchy.
- [ ] curățăm parent relation.
- [ ] curățăm children relation.
- [ ] eliminăm TransformComponent.
- [ ] eliminăm RenderableComponent.
- [ ] eliminăm CameraComponent.
- [ ] eliminăm NameComponent.
- [ ] curățăm active camera dacă este entity-ul distrus.
- [ ] curățăm alte world-owned transient references.
- [ ] incrementăm generation.
- [ ] marcăm slot dead.
- [ ] returnăm slot în free list.
- [ ] alive count scade exact o dată.
- [ ] nu rămân orphan component records.

---

# 18. Component destruction semantics

- [ ] Component destructors sunt apelate corect.
- [ ] Non-trivial components pot fi suportate de storage sau sunt explicit interzise.
- [ ] Move construction / move assignment requirements sunt documentate.
- [ ] Alignment este respectat.
- [ ] Storage realloc respectă alignment.
- [ ] `memcpy` nu este folosit pe tipuri non-trivially-copyable fără contract.
- [ ] Viitoarele components nu sunt forțate accidental să fie POD dacă arhitectura nu cere asta.

Acesta este un punct important deoarece vechiul `World` realocă arrays prin `memcpy`; asta nu trebuie generalizat orbește pentru componente C++ arbitrare.

---

# 19. Memory ownership

- [ ] `World` deține entity registry.
- [ ] `World` deține component storages.
- [ ] Storages folosesc allocator-ul engine.
- [ ] Shutdown order este explicit.
- [ ] Component memory nu este deținută de editor.
- [ ] Component memory nu este deținută de renderer.
- [ ] Component memory nu este deținută de ResourceManager.
- [ ] Renderer primește numai per-frame extracted data.
- [ ] External code nu face `delete` pe componente.
- [ ] External code nu păstrează pointers peste storage mutation fără contract.

---

# 20. Allocation discipline

- [ ] Create entity poate aloca doar la capacity growth.
- [ ] Add component poate aloca doar la component-pool growth.
- [ ] World update nu face heap allocation per entity.
- [ ] Transform update nu face heap allocation per frame.
- [ ] Render extraction folosește `FrameArena`.
- [ ] Queries hot nu fac allocations ascunse.
- [ ] Component iteration nu folosește temporary heap arrays.
- [ ] Capacity growth este geometric/controlled.
- [ ] Metrics pentru capacities/counts pot fi expuse pentru debugging.

---

# 21. Threading contract

Phase 15 nu trebuie să introducă concurență prematură.

**Design choice (not directly from the book):**

- [ ] World structural mutation este main-thread only.
- [ ] Entity create/destroy = main thread.
- [ ] Component add/remove = main thread.
- [ ] Transform parenting mutation = main thread.
- [ ] Component writes sunt main thread în Phase 15.
- [ ] Renderer nu citește direct mutable component storage.
- [ ] Renderer primește extracted immutable frame data.
- [ ] Nu introducem locks în fiecare component pool.
- [ ] Nu introducem parallel ECS scheduler.
- [ ] Documentăm clar că viitoarele jobified systems trebuie să respecte structural mutation barriers.

---

# 22. Render extraction migration

Actualul `World::BuildRenderQueue()` trebuie migrat de la:

`alive[] + xform[] + rend[]`

la component queries.

- [ ] Iterate entities/components cu Transform + Renderable.
- [ ] Skip disabled renderables.
- [ ] Obține world matrix din TransformComponent.
- [ ] Obține local/world bounds.
- [ ] Frustum test rămâne corect.
- [ ] Culling toggle rămâne funcțional.
- [ ] `WorldStats.visible` rămâne corect.
- [ ] `WorldStats.total` rămâne corect.
- [ ] `DebugRequestCullDump()` continuă să funcționeze sau este migrat curat.
- [ ] RenderQueue instance order rămâne deterministic.
- [ ] `FrameArena` continuă să dețină array-ul per-frame.
- [ ] Renderer nu primește Entity internals.
- [ ] Renderer nu devine dependent de component model.
- [ ] `RenderInstance` rămâne renderer-friendly POD.

---

# 23. Bounds migration

- [ ] Local bounds rămâne authored/renderable-owned data.
- [ ] World bounds este derived data.
- [ ] Transform change invalidează world bounds.
- [ ] Renderable local bounds change invalidează world bounds.
- [ ] World bounds este up-to-date înainte de culling.
- [ ] Non-uniform scale este tratată corect.
- [ ] Rotation produce conservative AABB corect.
- [ ] Missing Transform este gestionat safe.

---

# 24. Editor Phase 14 migration

Acesta este unul dintre cele mai importante sub-task-uri.

`EditorViewportController` nu trebuie să rămână cu propriul world model paralel.

## ValidationObject

- [ ] `ValidationObject` nu mai păstrează authoritative `t/r/s`.
- [ ] `EntityHandle` devine identity.
- [ ] Transform pentru pick/gizmo se citește din `TransformComponent`.
- [ ] Local bounds se citește din `RenderableComponent`.
- [ ] Name se citește din `NameComponent`.
- [ ] Selection este entity-based, nu index-based pe termen lung.

### Selection

- [ ] selected object devine `EntityHandle`.
- [ ] Stale selected entity după destroy este detectată.
- [ ] Selection clear funcționează.
- [ ] Viewport click selectează entity.
- [ ] Hierarchy selectează aceeași entity.
- [ ] Viewport ↔ hierarchy sync rămâne PASS.

### Picking

- [ ] Picking folosește transform-ul runtime real.
- [ ] Nu folosește mirror `object.t/r/s`.
- [ ] Inverse transform este calculat din component data.
- [ ] Bounds vin din RenderableComponent.
- [ ] Nearest hit rămâne corect.
- [ ] Picking după gizmo transform rămâne corect.

### Gizmos

- [ ] Pivot vine din TransformComponent.
- [ ] Rotate folosește TransformComponent rotation.
- [ ] Scale folosește TransformComponent scale.
- [ ] Drag modifică component data direct prin World API.
- [ ] Nu actualizăm un mirror local și apoi runtime separat.
- [ ] Debug selection world matrix este extrasă din World.
- [ ] Gizmo behavior rămâne identic cu Phase 14.

---

# 25. Scene Hierarchy bridge migration

Phase 16 va face hierarchy editor real; Phase 15 doar conectează UI-ul curent la world real.

- [ ] `AliveCount()` continuă să alimenteze status/hierarchy.
- [ ] Hierarchy names provin din NameComponent unde e relevant.
- [ ] Main Camera este entity real cu CameraComponent.
- [ ] Cubes/Ground sunt entities reale cu components.
- [ ] Nu implementăm încă generic create/delete UI.
- [ ] Nu implementăm drag/drop reparent UI.
- [ ] Nu implementăm component Inspector complet.
- [ ] Nu implementăm rename UI complet.
- [ ] Nu transformăm Phase 15 în Phase 16.

---

# 26. Camera editor migration

Actuala Phase 14 camera este un runtime world object; în Phase 15 devine entitate componentizată.

- [ ] Create camera entity.
- [ ] Add NameComponent `"Main Camera"` sau equivalent.
- [ ] Add TransformComponent.
- [ ] Add CameraComponent.
- [ ] Set active camera.
- [ ] Fly-camera modifică TransformComponent.
- [ ] Viewport resize modifică CameraComponent aspect.
- [ ] `BuildRenderQueue` extrage view din active camera.
- [ ] Destroy camera active este safe.
- [ ] Camera entity nu necesită RenderableComponent.

---

# 27. Validation scene migration

Cei 4 validation objects Phase 14:

- [ ] Cube_A → Entity.
- [ ] Cube_B → Entity.
- [ ] Cube_C → Entity.
- [ ] Ground_Plane → Entity.
- [ ] fiecare primește NameComponent.
- [ ] fiecare primește TransformComponent.
- [ ] fiecare primește RenderableComponent.
- [ ] camera primește Name + Transform + Camera.
- [ ] visual output rămâne același.
- [ ] procedural sky rămâne renderer/editor scaffolding și nu trebuie forțat într-o componentă dacă nu are sens în Phase 15.

---

# 28. Backward compatibility / migration seams

- [ ] Decidem dacă `SceneObjectHandle` este:
  - [ ] eliminat;
  - [ ] temporar alias către `EntityHandle`;
  - [ ] deprecated seam.
- [ ] Nu păstrăm două handle types cu aceeași semantică.
- [ ] Old `CreateObject()` este eliminat sau migrat deliberat.
- [ ] Old `SetRenderable()` este înlocuit de component API.
- [ ] Old camera special-case API este migrat.
- [ ] Editor compilează exclusiv pe API-ul nou.
- [ ] Runtime compilează exclusiv pe modelul nou.

---

# 29. Invalid-state handling

Trebuie teste pentru:

- [ ] invalid entity handle.
- [ ] stale entity handle.
- [ ] out-of-range entity index.
- [ ] wrong generation.
- [ ] double destroy.
- [ ] component add pe dead entity.
- [ ] component get pe dead entity.
- [ ] component remove pe dead entity.
- [ ] duplicate component add.
- [ ] remove absent component.
- [ ] parent = self.
- [ ] parent = descendant.
- [ ] parent stale.
- [ ] child stale.
- [ ] parent fără Transform.
- [ ] missing active camera.
- [ ] active camera destroyed.
- [ ] renderable fără valid mesh.
- [ ] name edge cases.
- [ ] zero entities.
- [ ] zero renderables.
- [ ] capacity growth.
- [ ] free-list reuse.
- [ ] storage growth.
- [ ] swap-remove.
- [ ] world shutdown cu entities încă alive.

---

# 30. Diagnostics

- [ ] Logs pentru World init/shutdown.
- [ ] Logs pentru unrecoverable ECS setup failure.
- [ ] Development diagnostics pentru invalid structural operation.
- [ ] Component registry duplicate diagnostic.
- [ ] Cycle-parenting diagnostic.
- [ ] Stale handle diagnostic unde este util.
- [ ] Counters:
  - [ ] alive entities;
  - [ ] Transform count;
  - [ ] Renderable count;
  - [ ] Camera count;
  - [ ] Name count.
- [ ] Debug dump opțional pentru entity/components.
- [ ] Diagnostics nu provoacă per-frame spam.

---

# 31. Unit tests — EntityHandle

- [ ] default handle invalid.
- [ ] created handle valid.
- [ ] alive handle resolves.
- [ ] destroy invalidates.
- [ ] reused slot gets different generation.
- [ ] old handle stays invalid after slot reuse.
- [ ] invalid index rejected.
- [ ] wrong generation rejected.
- [ ] thousands of create/destroy/reuse cycles.

---

# 32. Unit tests — component storage

Pentru fiecare component storage:

- [ ] add first component.
- [ ] add many components.
- [ ] has.
- [ ] get.
- [ ] const get.
- [ ] remove.
- [ ] remove first.
- [ ] remove middle.
- [ ] remove last.
- [ ] swap-remove lookup repair.
- [ ] duplicate add.
- [ ] absent remove.
- [ ] storage growth.
- [ ] no data corruption after growth.
- [ ] owners remain correct.
- [ ] destructors invoked if applicable.

---

# 33. Unit tests — transform

- [ ] root TRS → expected world.
- [ ] parent + child.
- [ ] 3-level hierarchy.
- [ ] multiple siblings.
- [ ] translation inheritance.
- [ ] rotation inheritance.
- [ ] scale inheritance.
- [ ] non-uniform scale.
- [ ] reparent.
- [ ] detach.
- [ ] parent move updates child.
- [ ] child local change updates child only where appropriate.
- [ ] self-parent rejected.
- [ ] child→ancestor cycle rejected.
- [ ] deep hierarchy.
- [ ] parent destroy semantics.
- [ ] remove Transform semantics.

---

# 34. Integration tests — rendering

- [ ] Entity + Transform + Renderable appears in queue.
- [ ] Entity fără Renderable nu apare.
- [ ] Renderable fără valid required transform este tratat conform contractului.
- [ ] remove Renderable removes draw.
- [ ] destroy entity removes draw.
- [ ] moving entity changes render matrix.
- [ ] bounds update.
- [ ] culling inside frustum.
- [ ] culling outside frustum.
- [ ] culling disabled.
- [ ] visible/total stats.
- [ ] deterministic render order.

---

# 35. Integration tests — camera

- [ ] Camera component produces valid viewProj.
- [ ] Moving camera changes view.
- [ ] Rotation changes view.
- [ ] aspect changes projection.
- [ ] near/far/FOV propagate.
- [ ] selecting another camera works.
- [ ] deleting active camera is safe.
- [ ] no active camera has explicit fallback/failure policy.

---

# 36. Editor regression tests

Toate cele validate în Phase 14 trebuie să continue să treacă:

- [ ] Editor launches.
- [ ] Engine init succeeds.
- [ ] viewport DX12 child renders.
- [ ] Cube_A visible.
- [ ] Cube_B visible.
- [ ] Cube_C visible.
- [ ] Ground_Plane visible.
- [ ] procedural sky visible.
- [ ] grid visible.
- [ ] resize works.
- [ ] maximize/restore.
- [ ] minimize/restore.
- [ ] camera RMB.
- [ ] WASD.
- [ ] Q/E.
- [ ] Shift speed.
- [ ] wheel speed.
- [ ] picking Cube_A.
- [ ] picking Cube_B.
- [ ] picking Cube_C.
- [ ] picking ground.
- [ ] empty clears selection.
- [ ] nearest hit.
- [ ] hierarchy → viewport sync.
- [ ] viewport → hierarchy sync.
- [ ] Select tool.
- [ ] Move X/Y/Z.
- [ ] Rotate X/Y/Z.
- [ ] Scale X/Y/Z.
- [ ] selection outline follows transform.
- [ ] picking after transform.
- [ ] gizmo after camera movement.
- [ ] no periodic shader/stutter regression.
- [ ] DX12 debug layer clean.

---

# 37. Stress tests

- [ ] 1 entity.
- [ ] 64 entities.
- [ ] 1,000 entities.
- [ ] 10,000 entities pentru registry/storage test.
- [ ] create all.
- [ ] add mixed components.
- [ ] remove randomized subset.
- [ ] destroy randomized subset.
- [ ] recreate entities using free-list.
- [ ] verify stale handles.
- [ ] verify component counts.
- [ ] verify no orphan components.
- [ ] verify no hierarchy corruption.
- [ ] repeated storage growth.
- [ ] repeated World init/shutdown where practical.

---

# 38. Performance baseline

Production Engineering Standard cere măsurare, nu presupuneri.

- [ ] Benchmark entity creation throughput.
- [ ] Benchmark entity destruction throughput.
- [ ] Benchmark component add/remove.
- [ ] Benchmark `IsAlive()`.
- [ ] Benchmark transform update pentru:
  - [ ] many roots;
  - [ ] wide hierarchy;
  - [ ] deep hierarchy;
  - [ ] mostly-clean transforms;
  - [ ] all-dirty transforms.
- [ ] Benchmark Renderable iteration/render extraction.
- [ ] Măsurăm allocations.
- [ ] Confirmăm zero unexpected per-frame heap allocations în core ECS paths.
- [ ] Capturăm rezultatele în Implementation Report.
- [ ] Nu inventăm arbitrary performance targets înainte de baseline.

---

# 39. Code organization

Un posibil layout:

```text
Engine/
└── Runtime/
    ├── Entity.h
    ├── ComponentType.h
    ├── ComponentRegistry.h/.cpp
    ├── ComponentStorage.h
    ├── Components/
    │   ├── TransformComponent.h
    │   ├── RenderableComponent.h
    │   ├── CameraComponent.h
    │   └── NameComponent.h
    ├── World.h
    └── World.cpp
```

**Design choice (not directly from the book):** exact file organization.

Checklist:

- [ ] separăm entity identity de World implementation.
- [ ] separăm component definitions.
- [ ] generic storage nu devine giant header spaghetti.
- [ ] public/private boundaries clare.
- [ ] project `.vcxproj` actualizat.
- [ ] `.vcxproj.filters` actualizat.
- [ ] dead runtime files eliminate dacă sunt obsolete.
- [ ] nu lăsăm duplicate headers cu concepte conflictuale.

---

# 40. Build/configuration validation

- [ ] NocturneEngine Development x64 build.
- [ ] NocturneHost Development x64 build.
- [ ] NocturneEditor Development x64 build.
- [ ] Debug/assert configuration unde există.
- [ ] Build direct `.vcxproj` încă funcționează.
- [ ] Solution build funcționează.
- [ ] Nu apar warnings noi importante.
- [ ] Nu apar stale include references.
- [ ] Dead ECS files nu sunt lăsate accidental în proiect.

---

# 41. Documentation

Înainte de completion:

- [ ] Actualizăm Phase 15 architecture documentation.
- [ ] Înregistrăm modelul de entity identity.
- [ ] Înregistrăm component storage model.
- [ ] Înregistrăm handle lifetime rules.
- [ ] Înregistrăm pointer/reference invalidation.
- [ ] Înregistrăm transform hierarchy invariants.
- [ ] Înregistrăm component duplicate semantics.
- [ ] Înregistrăm destroy semantics.
- [ ] Înregistrăm query semantics.
- [ ] Înregistrăm threading contract.
- [ ] Înregistrăm metadata/versioning contract.
- [ ] Înregistrăm persistent-ID boundary pentru Phase 17.
- [ ] Marcăm toate deciziile Nocturne cu:
  - **Design choice (not directly from the book)**
- [ ] Nu inventăm book citations.

---

# 42. Phase 15 Implementation Report

- [ ] Ce s-a implementat efectiv.
- [ ] Fișiere adăugate/modificate.
- [ ] API final.
- [ ] Data layout final.
- [ ] Ownership.
- [ ] Lifetime.
- [ ] Invariants.
- [ ] Error policies.
- [ ] Transform semantics.
- [ ] Editor migration.
- [ ] Render migration.
- [ ] Tests executate.
- [ ] Performance baseline.
- [ ] Known limitations.
- [ ] Deferred scope.

---

# 43. Phase 15 Completion Report

Phase 15 poate fi marcată COMPLETE numai dacă:

- [ ] `SceneObjectHandle` temporar nu mai este modelul autoritar.
- [ ] avem safe generational `EntityHandle`.
- [ ] stale handles sunt detectate.
- [ ] entity registry este robust.
- [ ] Transform este componentă.
- [ ] Renderable este componentă.
- [ ] Camera este componentă.
- [ ] Name este componentă.
- [ ] components pot fi adăugate/eliminate independent.
- [ ] component storages sunt production-grade în scope.
- [ ] transform hierarchy respinge ciclurile.
- [ ] entity destruction nu lasă orphan state.
- [ ] active camera lifecycle este safe.
- [ ] runtime rendering folosește component data.
- [ ] editorul folosește component data drept single source of truth.
- [ ] Phase 14 behavior rămâne intact.
- [ ] no hidden per-frame heap churn.
- [ ] stress tests trec.
- [ ] stale/invalid tests trec.
- [ ] component storage tests trec.
- [ ] transform tests trec.
- [ ] renderer integration tests trec.
- [ ] editor regression checklist trece.
- [ ] performance baseline este măsurat.
- [ ] documentația reflectă codul real.
- [ ] implementation report există.
- [ ] completion report există.
- [ ] nu există două ECS/world models concurente în repo.
- [ ] nu există structural TODO ascuns drept „finished”.

---

# 44. Explicit NU implementăm în Phase 15

- [ ] **NU** full Scene Hierarchy editing.
- [ ] **NU** generic create/delete/duplicate UI — Phase 16.
- [ ] **NU** Inspector complet pentru arbitrary components — Phase 16.
- [ ] **NU** undo/redo scene transactions — Phase 16.
- [ ] **NU** prefab authoring — Phase 16+.
- [ ] **NU** scene files — Phase 17.
- [ ] **NU** save/load.
- [ ] **NU** JSON/binary scene serialization.
- [ ] **NU** persistent reference fixups.
- [ ] **NU** serialization migrations.
- [ ] **NU** physics components/simulation — Phase 18.
- [ ] **NU** animation.
- [ ] **NU** audio.
- [ ] **NU** scripting.
- [ ] **NU** gameplay ECS systems.
- [ ] **NU** multithreaded ECS scheduler.
- [ ] **NU** network replication.
- [ ] **NU** archetype/chunk ECS doar pentru că „așa fac engine-urile moderne”.
- [ ] **NU** third-party ECS dependency fără motiv arhitectural demonstrat.

---

## Ordinea recomandată de execuție

Dacă o transformăm ulterior în pași de implementare, ordinea sigură este:

**Audit/cleanup → EntityHandle → EntityRegistry → generic ComponentStorage → component metadata → TransformComponent + hierarchy → Renderable → Camera → Name → World API → render extraction → editor migration → tests → stress/perf → cleanup dead code → documentation → final regression.**

Aș considera **EntityHandle + EntityRegistry + ComponentStorage** primul milestone intern; nu ar trebui să atingem încă editorul până când aceste trei fundații nu sunt testate solid.