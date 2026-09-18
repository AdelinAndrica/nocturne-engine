# Nocturne Engine — Phase 16 Runtime Reflection Architecture Contract

> **Status: LOCKED FOR PHASE 16**
>
> **Subsystem:** Runtime Reflection
>
> **Scope:** engine-wide reflection pentru tipurile Nocturne înregistrate
>
> **Product role:** infrastructură comună pentru Editor Inspector, generic property editing, undo/redo, Phase 17 serialization/prefabs, Phase 24 scripting/bindings, debug tooling și viitoare sisteme data-driven.
>
> **Applies with:**
>
> - `Docs/Production Engineering Standard.md`
> - `Docs/Phase 16 — Professional Grade Implementation Contract.md`
>
> Reflection este implementată în Phase 16 **înainte** de Inspector-ul generic și înainte de scene serialization.

## 1. Decizia arhitecturală

Phase 16 introduce **full runtime reflection pentru modelul de tipuri înregistrate al Nocturne Engine**.

Aceasta nu este doar editor metadata.

Reflection devine infrastructură runtime centrală, engine-owned, folosită de mai mulți consumatori.

**Design choice (not directly from the book):** Nocturne adoptă un registry explicit de runtime reflection pentru toate tipurile engine/game care trebuie inspectate, editate, serializate, copiate generic, expuse scripting-ului sau adresate prin property identity.

Prin "full runtime reflection" în Nocturne înțelegem că, pentru orice tip declarat reflectable, runtime-ul poate determina și opera generic asupra:

- identității tipului;
- categoriei tipului;
- numelui canonic;
- size/alignment;
- version;
- lifecycle operations;
- proprietăților;
- tipurilor proprietăților;
- enum values;
- nested structs;
- array/sequence/container adapters;
- funcțiilor explicit reflectate;
- parametrilor și return type;
- attributes/flags;
- component membership operations unde tipul este componentă;
- semantic read/write/invoke operations.

Nu înseamnă că engine-ul trebuie să poată reflecta arbitrar orice construct C++ extern care nu a fost declarat reflectable.

Această limită nu reduce sistemul la "partial reflection"; definește universul reflectat ca fiind schema runtime Nocturne, nu întregul limbaj C++ și toate bibliotecile third-party.

## 2. Grounding din cărți

### Gregory — Game Engine Architecture, 3rd Edition

Grounding conceptual:

- §16.2.1.6 — pure component model și component lookup prin identity;
- §16.2.2 — property-centric architectures;
- §16.2.2.4 — avantajele data-driven/property-centric composition;
- §15.4 — world editor ca tool central;
- §15.4.1.7 — anumite properties necesită editing semantic/specializat;
- §16.3 — world data trebuie reprezentat într-o formă persistentă la nivelul fazei următoare;
- §16.9 — scripting este parte a gameplay foundation și va necesita un boundary între runtime types și script.

Gregory susține modelul data-driven/property-centric și avertizează și asupra complexității nejustificate.

Cartea nu prescrie API-ul concret Nocturne pentru runtime reflection.

Prin urmare structurile, IDs, registry-ul și callback-urile de mai jos sunt:

**Design choice (not directly from the book)**

## 3. Motivul pentru introducerea acum

Fără reflection centrală, fiecare consumator ar trebui să redefinească schema componentelor:

- Inspector;
- undo property commands;
- serialization;
- prefab overrides;
- scripting;
- debug inspector;
- copy/paste;
- diff;
- property animation;
- tooling validation.

Acest lucru ar produce mai multe surse de adevăr despre aceeași componentă.

Reflection trebuie să transforme operația normală:

> "adaug un nou component/proprietate"

dintr-o modificare în multe switch-uri centrale într-o extensie prin registration/schema.

Ținta arhitecturală este Open/Closed la nivel de extensie:

- infrastructura generică rămâne stabilă;
- tipurile noi își declară schema;
- custom behavior este înregistrat prin extension points;
- consumatorii generici nu trebuie modificați pentru fiecare component nou.

Referirea la Open/Closed Principle ca regulă explicită de proiect este **Design choice (not directly from the provided books)**.

## 4. Ownership

Reflection registry este engine-wide.

**Design choice (not directly from the book):**

`Engine` deține un singur `ReflectionRegistry`.

Ordinea conceptuală:

```text
Engine
 ├── ReflectionRegistry
 ├── Resources
 ├── Input
 ├── Render
 └── World
```

Reflection trebuie inițializată înainte de World.

World și Editor primesc acces non-owning la registry.

Nu există:

- reflection registry separat în Editor;
- serialization registry separat;
- scripting type registry paralel pentru aceleași tipuri native;
- component metadata registry concurent care poate diverge.

## 5. Migrarea ComponentRegistry Phase 15

Phase 15 are:

- `ComponentTypeId`;
- `ComponentTypeMetadata`;
- `ComponentRegistry`.

Phase 16 trebuie să păstreze compatibilitatea logică, dar să elimine autoritatea duplicată.

Direcția:

- `ComponentTypeId` rămâne stable component identity sau devine un wrapper/alias clar peste type identity conform arhitecturii finale;
- `ComponentTypeMetadata` este extins/integrat în reflection metadata;
- `ComponentRegistry` devine facade/component view peste `ReflectionRegistry` sau este migrat complet;
- World nu mai deține schema globală a tipurilor.

Nu menținem două registre sincronizate manual.

## 6. Stable TypeId

Reflection introduce stable `TypeId`.

Cerințe:

- invalid sentinel;
- explicit/stable values;
- zero/invalid rezervat;
- nu depinde de registration order;
- nu depinde de RTTI address;
- nu depinde de pointer;
- nu depinde de compiler-specific `typeid().hash_code()`;
- deterministic între run-uri și build-uri în cadrul contractului de compatibilitate;
- duplicate TypeId respins;
- duplicate canonical type name respins.

**Design choice (not directly from the book):** numeric explicit IDs sau un stable compile-time hash pot fi folosite doar după documentarea collision/compatibility policy.

Nu adoptăm accidental hash-uri fără collision policy.

## 7. Stable PropertyId

Fiecare property reflectată are `PropertyId` stabil în interiorul tipului owner.

Cerințe:

- invalid sentinel;
- explicit identity;
- nu depinde de memory offset;
- nu depinde de declaration order dacă schema persistence cere stabilitate;
- duplicate property ID respins;
- duplicate canonical property name respins;
- renamed property policy pregătită pentru Phase 17;
- removed/deprecated property poate păstra tombstone/migration metadata mai târziu.

PropertyId este destinat să fie reutilizat de:

- generic property commands;
- prefab overrides;
- serialization schema;
- scripting bindings;
- debug/property addressing.

## 8. TypeKind

Reflection trebuie să distingă semantic tipurile.

Minimum:

- Bool;
- signed integers;
- unsigned integers;
- floating point;
- String;
- Enum;
- Struct;
- Component;
- EntityHandle/reference category;
- ResourceHandle/reference category;
- Array/fixed sequence;
- Dynamic sequence/container;
- Function;
- Opaque/custom.

Tipurile matematice Nocturne precum:

- Vec2;
- Vec3;
- Vec4;
- Quat;
- Mat4;
- AABB;

pot fi reflectate ca Struct sau ca TypeKind specializat dacă arhitectura justifică.

Decizia trebuie documentată.

## 9. TypeMetadata

Un tip reflectat trebuie să poată expune cel puțin:

- TypeId;
- canonical name;
- display/debug name optional;
- TypeKind;
- version;
- size;
- alignment;
- flags;
- lifecycle ops;
- property count/properties;
- function count/functions;
- enum metadata unde aplicabil;
- container adapter unde aplicabil;
- component adapter unde aplicabil;
- base/interface metadata doar dacă este introdusă explicit.

Metadata lifetime este registry lifetime.

String ownership este explicit.

Nu returnăm pointers spre temporary registration descriptors.

## 10. Type lifecycle operations

Reflection completă trebuie să poată opera generic cu valori ale tipurilor reflectate.

Type operations includ, după caz:

- default construct;
- destruct;
- copy construct;
- move construct;
- copy assign;
- move assign;
- equality/compare;
- reset-to-default;
- optional hash.

Cerințe:

- operations absente sunt reprezentate explicit;
- non-trivial types sunt respectate;
- alignment este respectat;
- failure policy explicită;
- nu facem `memcpy` generic peste tipuri non-triviale.

Aceste operations vor susține:

- transient snapshots;
- generic property values;
- undo history;
- serialization staging;
- scripting marshalling.

## 11. PropertyMetadata

Fiecare property reflectată trebuie să expună minimum:

- PropertyId;
- canonical name;
- owner TypeId;
- value TypeId;
- flags;
- getter/read adapter;
- setter/write adapter sau readonly state;
- optional direct-address adapter pentru safe properties;
- optional default value provider;
- optional validation adapter;
- optional attributes.

Property metadata nu trebuie să depindă de UI toolkit.

## 12. Semantic property access

Aceasta este invariantă critică.

Reflection nu are voie să transforme toate fields în memory writes nevalidate.

Există două categorii:

### Plain data property

Poate permite direct read/write dacă:

- tipul este safe;
- nu există cross-field invariant;
- nu există side effects;
- structural mutation nu este implicată.

### Semantic property

Trebuie să folosească getter/setter callbacks care trec prin owner/system API.

Exemple:

- Camera lens → CameraSystem/World validation;
- Transform local TRS → TransformSystem/World;
- hierarchy parent → SetParent/reparent mechanism;
- ResourceHandle assignment → resource validation;
- active camera → camera activation mechanism.

Nu reflectăm private/internal hierarchy links ca user-editable fields doar pentru că există în struct.

## 13. Property flags

Core flags trebuie să poată exprima cel puțin:

- ReadOnly;
- Transient;
- Serializable;
- EditorVisible;
- ScriptVisible;
- Deprecated;
- Required;
- Hidden;
- ResourceReference;
- EntityReference.

Dacă unele flags sunt strict editor presentation, ele pot fi în attributes/editor metadata, nu în core flags.

Flags au semantics documentate și testate.

## 14. Attributes

Reflection trebuie să suporte typed attributes/annotations.

Exemple:

- display name;
- category;
- tooltip;
- numeric min/max;
- step;
- units;
- angle;
- color;
- multiline;
- resource type constraint;
- enum display labels;
- editor widget hint;
- serialization alias;
- scripting alias;
- readonly reason.

**Design choice (not directly from the book):** core reflection păstrează attributes într-o formă generică și nu depinde de Win32.

Editor interpretează doar attributes pe care le cunoaște.

## 15. Enum reflection

Enum metadata include:

- enum TypeId;
- underlying integer type;
- canonical enum name;
- value count;
- stable value identity/value;
- canonical value name;
- display label optional;
- flags-enum marker dacă este cazul.

Inspector, serializer și scripting folosesc aceeași enum metadata.

## 16. Struct reflection

Nested structs trebuie să fie reflectabile recursiv.

Exemple:

- Vec3;
- AABB;
- future gameplay structs.

Generic traversal trebuie să poată ajunge:

```text
Component
  -> Property
      -> Struct Type
          -> Nested Property
```

Cycle detection/policy pentru type graphs trebuie definită.

## 17. Container reflection

Full runtime reflection trebuie să poată reprezenta colecții fără să depindă de STL în public engine APIs.

Minimum abstractions:

- fixed-size array;
- dynamic sequence/container adapter.

Container metadata/ops trebuie să poată expune:

- element TypeId;
- count;
- const element access;
- mutable element access dacă permis;
- resize/insert/remove dacă supported;
- read-only state;
- capacity semantics unde relevant.

Phase 16 trebuie să implementeze infrastructura chiar dacă foundation components actuale folosesc puține containere reflectate.

Nu facem public reflection API dependent de `std::vector`.

## 18. Resource references

Resource references trebuie reflectate semantic.

Metadata trebuie să poată indica:

- resource reference flag/kind;
- expected resource type/category;
- getter;
- validated setter.

Acest lucru permite Inspector-ului și Phase 17 serializer-ului să trateze `ResourceHandle` coerent.

## 19. Entity references

Runtime `EntityHandle` este tranzitoriu.

Reflection poate descrie un field ca entity reference, dar:

- runtime accessor poate folosi EntityHandle;
- serializer Phase 17 nu va persista index/generation;
- reflection metadata trebuie să permită serializer-ului să știe că acel field necesită persistent reference translation.

Persistent entity identity rămâne Phase 17.

## 20. Component reflection

Un reflected component type trebuie să aibă component operations.

Minimum:

- Has(World, EntityHandle);
- Add(World, EntityHandle);
- Remove(World, EntityHandle);
- GetConst(World, EntityHandle);
- GetMutable/direct mutation doar dacă architecture permite;
- optional reset/default;
- component flags/policies.

Această layer permite generic:

- Inspector component enumeration;
- Add Component menu;
- Remove Component;
- generic snapshot capture;
- serializer component enumeration;
- scripting component lookup.

Nu hardcodăm în fiecare consumator:

```cpp
if Transform...
else if Camera...
else if Renderable...
```

## 21. Component enumeration per entity

Reflection singură nu este suficientă dacă World nu poate enumera generic component membership.

Phase 16 trebuie să introducă un mecanism determinist prin care se poate determina ce reflected components are o entitate.

Options pot include:

- query prin component adapters;
- compact component mask dacă ID model permite;
- World-level reflected component enumeration.

Alegerea este documentată după audit.

Nu introducem archetype ECS doar pentru acest requirement.

## 22. Function reflection

Full runtime reflection include funcții/metode explicit reflectate.

Function metadata trebuie să poată descrie:

- stable FunctionId în owner type/namespace;
- canonical name;
- return TypeId;
- parameter count;
- parameter metadata;
- flags;
- invocation adapter;
- const/static/member semantics unde aplicabil.

Nu toate C++ functions trebuie reflectate.

Doar functions declarate parte din runtime reflected schema.

Acest seam este necesar pentru scripting/debug tooling și evită un al doilea native-binding metadata system mai târziu.

## 23. Function invocation

Invocation generică trebuie:

- valida argument count;
- valida TypeIds;
- respecta constness;
- returna success/failure explicit;
- nu arunca exceptions peste boundaries dacă project policy nu le folosește;
- nu dereferenția invalid object/entity context;
- expune diagnostic pentru mismatch.

Phase 24 va decide ce reflected functions sunt script-exposed.

Reflection core implementează mecanismul; scripting policy rămâne Phase 24.

## 24. Generic value representation

Property commands, attributes, function invocation și tooling au nevoie de o reprezentare generică controlată.

Phase 16 trebuie să definească:

- non-owning typed value view;
- const value view;
- owned reflected value/storage pentru history/snapshots unde necesar.

Requirements:

- TypeId;
- pointer/data;
- size/alignment;
- lifecycle;
- copy/move safety;
- no unchecked raw byte copy pentru non-trivial types;
- allocator ownership explicit.

Nu introducem `std::any` în public engine API.

## 25. Reflection Registry

Registry trebuie să ofere:

- RegisterType;
- FindType(TypeId);
- FindTypeByName;
- deterministic TypeAt(index);
- Count;
- FindProperty;
- FindFunction;
- enum lookup;
- duplicate validation;
- startup diagnostics.

Registry registration este structural startup work, nu frame hot path.

## 26. Registration lifecycle

**Design choice (not directly from the book):**

Core engine types sunt registered în deterministic startup phase.

Registry poate avea:

- Building state;
- Frozen state;
- Shutdown state.

După freeze:

- metadata pointers rămân stabile;
- duplicate/mutation registration este respinsă;
- runtime reads sunt read-only.

Dacă plugin/hot-reload support va cere dynamic registration în viitor, contractul va fi extins explicit; nu compromitem invariants Phase 16 acum.

## 27. Registration mechanism

Phase 16 trebuie să aleagă și documenteze mecanismul dintre:

- explicit registration functions;
- templates/builders;
- macros;
- generated code;
- combinație.

Criterii:

- deterministic;
- debuggable;
- no hidden static initialization order dependency;
- no fragile linker magic;
- no RTTI-address identity;
- maintainable;
- future codegen-compatible.

**Design choice (not directly from the book):** preferăm explicit registration/builders la început dacă auditul nu justifică code generation imediat.

Dar public schema API nu trebuie să blocheze code generation ulterior.

## 28. No static initialization fiasco

Reflection registration nu se bazează pe ordinea nedefinită a global constructors între translation units.

Engine startup controlează registration order.

Tests trebuie să demonstreze deterministic registry independent de source/link order acolo unde este posibil.

## 29. Canonical names

Canonical reflected names sunt stable schema identity auxiliaries.

Exemple:

- `Nocturne.Transform`;
- `Nocturne.Camera`;
- `Nocturne.Vec3`.

Rules:

- unique;
- owned by registry;
- UTF-8 policy explicit;
- display labels pot diferi de canonical names;
- rename de display label nu schimbă schema identity.

## 30. Version metadata

Fiecare serializable reflected type are version.

Phase 16:

- stochează version metadata;
- validează non-zero/valid policy;
- expune version generic.

Phase 17 definește:

- compatibility;
- migration;
- rejection;
- file representation.

Nu implementăm migrations în Phase 16, dar nu proiectăm reflection fără version seam.

## 31. Reflection și Inspector

Inspector Phase 16 este generic-first.

Flow:

```text
Selected Entity
  -> enumerate reflected components
  -> TypeMetadata
  -> enumerate PropertyMetadata
  -> property editor factory by TypeId/attributes
  -> semantic getter/setter
  -> command transaction
```

Custom component inspectors sunt extension points pentru cazuri unde generic property UI nu este suficient.

Custom inspector nu redefinește schema.

## 32. Reflection și Undo/Redo

Simple property changes folosesc generic property identity:

- EntityHandle;
- Component TypeId;
- PropertyId;
- old reflected value;
- new reflected value.

Apply/undo trec prin semantic property setter.

Structural operations rămân specialized commands:

- create;
- delete;
- duplicate;
- add/remove component;
- reparent.

Reflection reduce command explosion fără a face structural changes "generic memcpy".

## 33. Reflection și transient snapshots

Transient subtree snapshot trebuie să folosească reflection pentru component discovery/copy unde component schema permite.

Custom snapshot adapters sunt permise pentru types care nu pot fi generic copied.

Snapshot-ul rămâne transient Phase 16 data, dar reflection core folosit aici este același care va servi Phase 17.

## 34. Reflection și Phase 17 Serialization

Phase 17 trebuie să reutilizeze:

- TypeId;
- PropertyId;
- TypeKind;
- property graph;
- component ops;
- enum metadata;
- container metadata;
- version;
- Serializable flags;
- semantic/custom serialization adapters unde necesar.

Phase 17 adaugă:

- file format;
- persistent entity identity;
- reference fixups;
- migrations;
- compatibility;
- atomic writes.

Nu reinventează schema tipurilor.

## 35. Reflection și Prefabs

Prefab property overrides vor putea adresa:

- component TypeId;
- PropertyId;
- reflected value.

Prefab persistence rămâne Phase 17.

Phase 16 poate valida modelul pe transient snapshots/overrides fără format pe disk.

## 36. Reflection și Phase 24 Scripting

Phase 24 trebuie să reutilizeze:

- reflected type identity;
- property metadata;
- function metadata;
- enum metadata;
- ScriptVisible flags;
- invocation adapters.

Phase 24 adaugă VM/language/binding policy.

Nu creează un al doilea native schema registry.

## 37. Reflection și Debug Tooling

Reflection trebuie să permită:

- generic entity/component inspector;
- type dump;
- property dump;
- registry dump;
- enum dump;
- function signature dump;
- schema validation diagnostics.

Acestea sunt utile pentru Phase 28 profiling/debug tooling și pentru development înainte de Phase 28.

## 38. Reflection și property animation

Nu implementăm property animation în Phase 16.

Dar stable TypeId + PropertyId + typed access permit viitorului animation/timeline tooling să adreseze properties fără alt identity system.

## 39. Public API constraints

Reflection este runtime engine API.

Regulile existente se aplică:

- namespace `noc::`;
- no STL in public engine headers;
- no Win32 types;
- no editor types;
- explicit ownership;
- explicit lifetime;
- allocator-aware owned storage;
- const-correct metadata reads;
- no hidden global singleton.

## 40. Threading

Registration:

- main/startup thread.

After freeze:

- metadata reads trebuie să fie read-only și safe pentru concurrent read dacă restul engine-ului le folosește astfel.

Property mutation:

- respectă owner system threading contract;
- reflection nu face structural ECS mutation de pe arbitrary threads.

Phase 16 editor writes rămân main-thread.

## 41. Performance

Reflection metadata lookup nu trebuie să devină frame-hot bottleneck.

Măsurăm:

- TypeId lookup;
- name lookup;
- property lookup by ID;
- property enumeration;
- generic get/set;
- enum lookup;
- function lookup/invoke;
- component enumeration;
- Inspector traversal;
- registry startup.

Representative counts:

- 100 types;
- 1k types synthetic;
- 10k property lookups;
- 100k property reads synthetic unde util.

Nu punem arbitrary timing thresholds înainte de baseline.

## 42. Allocation discipline

After registry freeze:

- lookup nu alocă;
- enumeration nu alocă;
- property get nu alocă implicit;
- function metadata lookup nu alocă;
- registration allocations sunt startup-only și măsurate.

Owned reflected values pot aloca prin allocator explicit.

## 43. Diagnostics

Reflection errors trebuie să fie explicabile:

- duplicate TypeId;
- duplicate name;
- duplicate PropertyId;
- duplicate property name;
- invalid property type;
- missing TypeId dependency;
- invalid lifecycle ops;
- invalid size/alignment;
- invalid getter/setter;
- invalid enum duplicate;
- invalid function signature;
- registration after freeze;
- invocation type mismatch.

Registry dump trebuie să permită inspectarea schema finală.

## 44. Schema validation

La freeze, registry rulează validation pass.

Minimum:

- all referenced TypeIds exist;
- all IDs valid;
- names non-null/non-empty conform policy;
- sizes/alignment valid;
- property owner/value types valid;
- enum underlying types valid;
- function parameters/return types valid;
- component ops complete conform flags;
- lifecycle ops consistent;
- Serializable fields do not reference unsupported type categories fără adapter;
- ScriptVisible functions/properties au suportul structural necesar.

Freeze eșuează dacă schema este invalidă.

## 45. Testing

Unit tests:

- TypeId;
- PropertyId;
- registration;
- duplicate rejection;
- canonical names;
- deterministic enumeration;
- lifecycle ops;
- non-trivial types;
- over-aligned types;
- property get/set;
- readonly;
- semantic setter rejection;
- nested struct;
- enum;
- container adapter;
- resource reference;
- entity reference metadata;
- component ops;
- function metadata/invocation;
- generic values;
- freeze;
- post-freeze mutation rejection;
- registry shutdown/leaks.

Integration tests:

- reflect Name/Transform/Renderable/Camera;
- generic component enumeration;
- generic Inspector model;
- generic property command;
- transient snapshot;
- world semantic setters;
- invalid camera property update rejected;
- transform parent cannot be bypassed via raw reflection.

## 46. Reflection coverage for Phase 15 components

Before Phase 16 Inspector milestone:

### NameComponent

Reflect:

- value semantic property.

### TransformComponent

Reflect authorable semantic properties:

- local translation;
- local rotation;
- local scale.

Do not expose internal hierarchy/cache fields ca editable generic properties:

- parent;
- firstChild;
- nextSibling;
- prevSibling;
- world cache;
- dirty internals.

Parenting este structural/editor operation, nu raw property edit.

### RenderableComponent

Reflect authorable semantic properties:

- mesh;
- local bounds dacă policy permite;
- enabled.

Derived world bounds sunt readonly/transient.

### CameraComponent

Reflect:

- FOV;
- aspect;
- near;
- far;
- enabled.

Derived matrices sunt readonly/transient sau hidden.

## 47. Function reflection initial coverage

Phase 16 trebuie să demonstreze function reflection cu un set real, nu doar synthetic tests.

Alegem câteva safe APIs/operations care pot fi reflectate fără a crea scripting policy prematur.

Scopul este validarea mechanismului.

Script-visible policy rămâne Phase 24.

## 48. OCP acceptance test

Adăugăm un synthetic/test component nou în tests.

Pentru a-l face vizibil în generic reflection/Inspector model trebuie să fie necesar:

- definirea componentului;
- registration/schema lui;
- optional custom editor only dacă property kinds nu sunt generic supported.

Nu trebuie modificat:

- central Inspector switch;
- central serializer switch placeholder;
- central property-command switch;
- central debug type switch.

Dacă trebuie modificate, reflection architecture nu și-a atins scopul.

## 49. Nu duplicăm metadata

Interzis:

- Inspector-owned canonical property schema;
- serializer-owned canonical property schema;
- scripting-owned canonical native type schema;
- prefab-owned canonical property schema.

Pot exista metadata/adapters specifice consumerului, dar ele referă stable reflection IDs.

## 50. Explicit în afara Reflection Phase 16

Reflection core NU înseamnă că implementăm în aceeași fază:

- scene file I/O;
- persistent entity IDs;
- migration executor;
- scripting VM;
- network replication;
- property animation;
- hot reload de DLL/types;
- arbitrary third-party C++ introspection;
- compiler/AST integration obligatorie;
- garbage collector.

Acestea sunt consumers sau extensii viitoare.

Reflection core trebuie însă să aibă seam-urile necesare ca acei consumers să nu inventeze alt schema system.

## 51. Completion gate — Reflection Core

Reflection milestone nu este COMPLETE până când:

- [ ] Engine deține un singur ReflectionRegistry.
- [ ] ComponentRegistry nu mai este autoritate paralelă.
- [ ] TypeId este stabil și testat.
- [ ] PropertyId este stabil și testat.
- [ ] TypeKind este implementat.
- [ ] TypeMetadata este implementat.
- [ ] PropertyMetadata este implementat.
- [ ] lifecycle/type ops sunt implementate.
- [ ] semantic getters/setters sunt implementate.
- [ ] readonly/transient/serializable/editor/script flags există.
- [ ] typed attributes există.
- [ ] enum reflection există.
- [ ] nested struct reflection există.
- [ ] container reflection seam există și este testat.
- [ ] resource reference reflection există.
- [ ] entity reference metadata există.
- [ ] component operations există.
- [ ] component enumeration există.
- [ ] function reflection există.
- [ ] generic function invocation este testată.
- [ ] generic reflected values sunt safe pentru non-trivial types.
- [ ] registry freeze/validation există.
- [ ] lookup după ID și name este deterministic.
- [ ] post-freeze hot lookups nu alocă.
- [ ] foundation components sunt reflectate.
- [ ] generic property command folosește reflection.
- [ ] Inspector model poate fi generat din reflection.
- [ ] OCP synthetic component acceptance test trece.
- [ ] stress/performance baseline există.
- [ ] negative tests există.
- [ ] docs reflectă codul final.

## 52. Ordinea de implementare Reflection Core

1. audit metadata Phase 15;
2. TypeId / PropertyId contract;
3. TypeKind + flags;
4. TypeMetadata + lifecycle ops;
5. ReflectionRegistry;
6. deterministic registration/freeze;
7. primitive/builtin types;
8. enum reflection;
9. struct/property reflection;
10. attributes;
11. generic value views/owned values;
12. semantic getter/setter adapters;
13. container adapters;
14. component ops;
15. component enumeration;
16. function metadata/invocation;
17. migrate Phase 15 component metadata;
18. reflect foundation components;
19. generic property command;
20. Inspector model proof;
21. OCP extension test;
22. stress/performance;
23. diagnostics/schema dump;
24. documentation.

Reflection milestone precede full Scene Inspector implementation.
