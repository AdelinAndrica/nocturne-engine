---
id: "noc.development.cpp-api"
doc_type: "reference"
canonical: true
status: "active"
subsystem: "Documentation"
phase_introduced: 16
description: "Canonical contract for generated Nocturne C++ API documentation, symbol links and Doxygen XML tooling output."
source_files: ["Docs/API/Doxyfile","Docs/API/MainPage.dox","Website/scripts/prepare-api.ps1","Website/scripts/validate-api.ps1"]
source_docs: ["Docs/Development/Documentation Model.md","Docs/Web/Nocturne Website — Design & Technical Specification.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §1.7 Tools and the Asset Pipeline"]
aliases: ["C++ API Reference","API Reference","Doxygen API"]
deprecated_aliases: []
api_symbols: ["noc::Engine","noc::ResourceManager","noc::RenderSystem","noc::World","nocturne::editor::EditorShellV3"]
---

# C++ API Reference

## Purpose

The generated C++ API reference answers a different question from the conceptual documentation:

> **What symbols exist in the current C++ header surface, and what members/types do they expose?**

Canonical Architecture, Systems and Development documents remain responsible for:

- ownership;
- subsystem responsibilities;
- lifecycle;
- dependency direction;
- rationale;
- design constraints;
- current architectural contracts.

The API reference is a structural/source companion to those documents, not a replacement for them.

## Human API route

Generated HTML is published at:

`/api/`

The Doxygen landing page is:

`/api/index.html`

The generated API uses the current Nocturne dark visual vocabulary while remaining a separate generated reference surface.

## Machine/tooling route

Doxygen XML is published at:

`/api-xml/index.xml`

The full XML compound set is available below `/api-xml/`.

A Doxygen tag file is also published at:

`/api/nocturne.tag`

These outputs are intended for tooling that needs symbol-level structure rather than conceptual prose.

## Stable symbol aliases

Web 7 generates stable redirect routes from qualified C++ names.

Examples:

```text
/api-symbol/noc.Engine.html
/api-symbol/noc.ResourceManager.html
/api-symbol/noc.RenderSystem.html
/api-symbol/noc.World.html
/api-symbol/nocturne.editor.EditorShellV3.html
```

The redirect target is derived from Doxygen XML, so conceptual documentation does not need to depend directly on Doxygen's internal `refid` filenames.

A generated machine index is published at:

`/api-symbols.json`

It records:

- fully qualified symbol name;
- compound kind;
- Doxygen refid;
- stable alias route;
- direct Doxygen HTML route.

## Extraction scope

Current Doxygen input:

```text
Engine/
Apps/NocturneEditor/
Apps/NocturneHost/
Docs/API/MainPage.dox
```

Header patterns:

```text
*.h
*.hpp
*.inl
```

The generator intentionally excludes:

- `ThirdParty/`;
- test directories;
- `Engine/Render/DX12/d3dx12.h`;
- legacy `Apps/NocturneEditor/EditorShell.h`;
- legacy `Apps/NocturneEditor/EditorControls.h`.

`EditorShellV3` is the current canonical editor shell.

## Undocumented existing code

The current repository did not previously contain a Doxygen documentation layer.

Web 7 therefore uses:

`EXTRACT_ALL = YES`

so the existing class/struct/function surface can be navigated immediately.

This does not imply that every member already has high-quality authored API prose.

Future subsystem work can add focused Doxygen comments where symbol-level contracts need more explanation.

## Public and internal members

Web 7 includes private/internal class members in the generated reference.

This is deliberate because Nocturne's immediate audience is engine development and maintenance, not a frozen third-party SDK surface.

**Design choice (not directly from the book):** when Nocturne later defines a supported external SDK, that SDK should get its own narrower public-reference policy rather than treating the current engine-internal reference as a compatibility promise.

## Conceptual page integration

Canonical system pages author an `api_symbols` list in frontmatter.

Example:

```yaml
api_symbols: ["noc::Engine","noc::MainLoop","noc::World"]
```

The documentation synchronizer generates the visible **C++ API reference** section from that metadata.

The same symbol names are also included in the machine-readable knowledge manifest.

This preserves one authored relationship instead of maintaining separate human and AI symbol mappings.

## Validation

Web 7 validates that generated output contains the current key symbols for:

- Runtime;
- Resources;
- Rendering;
- World & ECS;
- Editor.

Validation also rejects accidental inclusion of:

- the legacy `EditorControls` shell surface;
- vendored `d3dx12` helper code.

## Source precedence

For current behavior:

1. canonical Nocturne architecture/system/development contracts;
2. current source code and generated API structure for exact implementation detail;
3. completion/implementation reports;
4. historical Phase documentation.

The generated API cannot override a canonical architectural contract merely because an internal member currently exists.

## Book grounding

Gregory §1.7 supports reliable tooling and development pipelines as part of the engine ecosystem.

The choice of Doxygen, HTML/XML publication, symbol aliases, extraction policy and conceptual-to-symbol metadata are **Design choice (not directly from the book)**.

## Related documentation

- [Documentation Model](/docs/development/documentation-model/)
- [Architecture Overview](/docs/architecture/overview/)
- [Runtime](/docs/systems/runtime/)
- [Resources](/docs/systems/resources/)
- [Rendering](/docs/systems/rendering/)
- [World & ECS](/docs/systems/world-ecs/)
- [Editor](/docs/systems/editor/)
