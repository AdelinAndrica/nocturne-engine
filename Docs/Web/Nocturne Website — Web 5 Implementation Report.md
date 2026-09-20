# Nocturne Website — Web 5 Implementation Report

> **Status:** COMPLETE
>
> **Track:** Nocturne Website
>
> **Milestone:** Web 5 — Downloads
>
> **Branch:** `web-docs-foundation`
>
> **Specification:** `Docs/Web/Nocturne Website — Design & Technical Specification.md`
>
> **Validated implementation head before completion docs:** `f5f3f1fac3179091fd6719240d3b5152f3ca1590`
>
> **Website validation:** `35468447709` — SUCCESS
>
> **Release packaging validation:** `35468447675` — SUCCESS

---

## 1. Objective

Web 5 converts the Downloads placeholder into an artifact-driven release surface.

The milestone establishes a strict separation between:

```text
project configuration
        |
        v
CI build capability
        |
        v
Ship package capability
        |
        v
published release artifact
        |
        v
reviewed release manifest
        |
        v
/download
```

A target is not presented as downloadable merely because a Visual Studio configuration exists.

---

## 2. Book grounding

Jason Gregory, *Game Engine Architecture, 3rd Edition*, §1.7 grounds the importance of reliable tools and production pipelines around engine development.

The following Web 5 mechanisms are **Design choice (not directly from the book)**:

- semantic release versions/channels;
- JSON release manifests;
- JSON Schema;
- Zod validation;
- GitHub Actions release packaging;
- ZIP bundle layout;
- SHA-256 publication;
- tagged GitHub Releases;
- explicit source-controlled manifest promotion;
- exact Downloads UI/status language.

Web 5 does not introduce a new engine runtime subsystem.

---

## 3. Release metadata schema

Created:

`Schemas/release.schema.json`

Schema version:

`1`

The manifest requires:

```text
schemaVersion
latest
releases[]
```

Each release records:

- version;
- channel;
- publication date;
- release-notes URL;
- source commit;
- published builds.

Each published build records:

- platform;
- architecture;
- configuration;
- support state;
- archive filename;
- byte size;
- SHA-256;
- HTTPS download URL.

Accepted architecture identifiers include:

```text
x86_64
x86
arm64
```

Schema acceptance is vocabulary, not a support claim.

An architecture becomes public only when a validated artifact is promoted into the manifest.

---

## 4. Executable validation

Created:

`Website/src/data/release-schema.mjs`

The executable schema additionally enforces:

- unique release versions;
- unique platform/architecture/configuration tuples;
- `latest === null` when the manifest is empty;
- `latest` references an existing release;
- `latest` equals the first release;
- releases are ordered newest-first.

Created:

`Website/scripts/validate-releases.mjs`

Command:

```powershell
cd Website
npm run validate:releases
```

The command runs automatically before:

- `npm run dev`;
- `npm run build`;
- `npm run sync`.

Current production manifest result:

```text
Release manifest valid: 0 releases, 0 published builds.
```

---

## 5. Current public manifest

`Website/src/data/releases.json` remains:

```json
{
  "schemaVersion": 1,
  "latest": null,
  "releases": []
}
```

This is intentional.

Web 5 proves candidate build/package capability, but no public versioned Nocturne Engine release has been published and promoted yet.

Therefore `/download` currently exposes zero active download buttons.

---

## 6. Downloads UI

Updated:

`Website/src/pages/download.astro`

Created:

`Website/src/styles/download.css`

The page parses the release manifest through the executable schema at build time.

### Empty state

The production page currently states:

```text
PUBLIC RELEASE NOT PUBLISHED
0 published artifacts
Downloadable architectures: None
```

### Published-release state

When a real manifest record exists, each release displays:

- semantic version;
- latest marker;
- channel;
- publication date;
- source-commit short SHA;
- release-notes link.

Each build displays:

- status;
- platform;
- architecture;
- Ship configuration;
- exact file size;
- archive filename;
- shortened visible SHA-256 with full digest available in the element metadata;
- download action.

A `withdrawn` build has no active download action.

---

## 7. Release status semantics

Current channels:

| Channel | Meaning |
|---|---|
| `development` | development-facing release |
| `preview` | prerelease / broader testing |
| `stable` | explicitly promoted stable release |

Current build states:

| State | Downloads behavior |
|---|---|
| `supported` | active download |
| `experimental` | active download with warning state |
| `withdrawn` | historical record without active download |

These semantics are **Design choice (not directly from the book)**.

---

## 8. Windows release packaging

Created:

`.github/workflows/release-package.yml`

Created:

`.github/scripts/package-release.ps1`

The release workflow validates two Windows targets:

| MSBuild platform | Manifest architecture |
|---|---|
| `x64` | `x86_64` |
| `Win32` | `x86` |

Candidate jobs build:

```text
NocturneEngine
NocturneHost
NocturneEditor
```

using:

```text
Configuration = Ship
```

The package step requires:

```text
NocturneEngine.lib
NocturneHost.exe
NocturneEditor.exe
```

---

## 9. Release bundle

Current candidate layout:

```text
bin/
  NocturneEditor.exe
  NocturneHost.exe

lib/
  NocturneEngine.lib

Data/
  ...

release-info.json
```

Archive naming:

```text
nocturne-engine-<version>-windows-<architecture>.zip
```

The packager computes:

- exact ZIP byte size;
- SHA-256;
- `.sha256` sibling file;
- per-architecture `release-record-<architecture>.json`.

---

## 10. Ship configuration defects found by the gate

The first release-package runs failed before packaging.

This was useful evidence: declared Ship configurations were not yet complete enough to represent real release support.

### 10.1 Missing C++20 / build configuration

Initial errors included:

```text
std::string_view is not a member of std
nested-namespace-definition requires /std:c++17
No build configuration defined
```

Cause:

The engine Ship ItemDefinitionGroup did not carry the current C++20 and `NOC_SHIP` contract.

Fix:

- `LanguageStandard = stdcpp20`;
- `NOC_SHIP`;
- `_LIB`;
- current Engine include path;
- current conformance settings.

The Host Ship configuration received its corresponding C++20/`NOC_SHIP` setup.

### 10.2 Missing Unicode configuration

The next gate exposed:

```text
LoadCursorW ... cannot convert LPSTR to LPCWSTR
```

Cause:

The engine Ship configuration did not specify the Unicode character set used by the existing Win32 code.

Fix:

```text
CharacterSet = Unicode
```

for Ship Win32 and x64.

### 10.3 Wrong engine output type

The next gate exposed:

```text
LNK1561: entry point must be defined
```

The linker was trying to produce:

`NocturneEngine.exe`

instead of the engine static library.

Fix:

```text
ConfigurationType = StaticLibrary
UseDebugLibraries = false
```

for both Ship platforms.

These changes repair the existing build configuration; they do not add a new runtime feature.

---

## 11. Validated architecture candidates

Final release-package validation:

Run:

`35468447675`

Result:

`SUCCESS`

### Windows x86_64

Job:

`Ship x86_64`

Result:

`SUCCESS`

All gates passed:

```text
Build Ship outputs         SUCCESS
Package release candidate  SUCCESS
Upload packaged candidate  SUCCESS
```

Candidate archive:

`nocturne-engine-0.0.0-pr2-windows-x86_64.zip`

Exact package bytes:

`2,762,266`

SHA-256:

`c3812e289db7e3fb78f050256ac4a26c3ca8f06628855a16b1dc7402811bfdd2`

GitHub Actions artifact:

`nocturne-release-0.0.0-pr2-windows-x86_64`

Artifact ID:

`10592410335`

### Windows x86

Job:

`Ship x86`

Result:

`SUCCESS`

All gates passed:

```text
Build Ship outputs         SUCCESS
Package release candidate  SUCCESS
Upload packaged candidate  SUCCESS
```

Candidate archive:

`nocturne-engine-0.0.0-pr2-windows-x86.zip`

Exact package bytes:

`2,555,997`

SHA-256:

`77b123a464965002f4583ee7435f8bc092dc9d4a29ae74b061d97a3eef979869`

GitHub Actions artifact:

`nocturne-release-0.0.0-pr2-windows-x86`

Artifact ID:

`10592021644`

### Interpretation

Web 5 now proves **Ship build/package capability** for:

- Windows x86_64;
- Windows x86.

It does not yet claim either as a public downloadable release because no tagged publication has been promoted into `releases.json`.

ARM64 remains unvalidated and is not rendered as downloadable.

---

## 12. Tagged release publication

The release workflow supports semantic version tags:

```text
v*.*.*
```

The version itself is still validated by the semantic-version policy before build/package work is accepted.

Tag channel policy:

- version without prerelease suffix → `stable`;
- version with prerelease suffix → `preview`.

A successful tagged run publishes GitHub Release assets:

- ZIP archive(s);
- SHA-256 file(s);
- release-record JSON file(s).

The PR validation run skips the publish job by design.

---

## 13. Reviewed manifest promotion

Created:

`Website/scripts/promote-release.mjs`

Command:

```powershell
cd Website
npm run promote:release -- <release-record-x86_64.json> [additional-records...]
```

The promotion tool:

- validates each record;
- requires records to agree on version/channel/date/notes/source commit;
- rejects duplicate build tuples;
- rejects a version already present in the manifest;
- merges validated architecture builds;
- promotes the release to `latest`;
- validates the entire resulting manifest before writing it.

This intentionally keeps public support promotion source-controlled and reviewable.

---

## 14. CI coverage

Website CI now verifies the real empty public state.

It also creates an **ephemeral valid release fixture** during CI to exercise the otherwise-unreachable public-release render branch.

The fixture:

- is never committed;
- validates against the same executable schema;
- performs a full Astro build;
- verifies version rendering;
- verifies x86_64 architecture rendering;
- verifies formatted byte size;
- verifies archive filename rendering;
- restores the real empty manifest afterward.

This avoids weakening artifact honesty merely to test the release UI.

---

## 15. Canonical release documentation

Created:

`Docs/Development/Release Process.md`

Stable ID:

`noc.development.release-process`

The page documents:

- public-support precedence;
- candidate build pipeline;
- Ship bundle;
- checksum generation;
- channel/state vocabulary;
- tagged release publication;
- explicit manifest promotion;
- build/package evidence;
- discovered Ship build-config defects.

It is linked from the canonical Development navigation.

---

## 16. Verification checklist

- [x] versioned JSON Schema exists;
- [x] executable manifest validation exists;
- [x] invalid manifest stops website build;
- [x] empty manifest is valid and produces zero downloads;
- [x] non-empty release rendering is exercised in CI with an ephemeral fixture;
- [x] release version/channel/date/source commit render model exists;
- [x] size/checksum/file metadata exists;
- [x] release-notes link exists;
- [x] supported/experimental/withdrawn states exist;
- [x] manifest promotion tool exists;
- [x] tagged GitHub Release publication path exists;
- [x] Ship build gate exists;
- [x] packaging script requires engine/host/editor outputs;
- [x] ZIP + SHA-256 + release-record are produced;
- [x] Windows x86_64 Ship candidate passes;
- [x] Windows x86 Ship candidate passes;
- [x] Actions artifacts exist for both validated architectures;
- [x] ARM64 is not advertised;
- [x] public production manifest remains empty;
- [x] no fake public release was created;
- [x] Website remains independent from engine runtime dependencies.

---

## 17. Deferred scope

### First public Nocturne release

Web 5 establishes the mechanism but does not create a fake product release just to populate the page.

The first real release requires an intentional semantic version/tag and reviewed promotion of the published release records.

### ARM64

ARM64 is vocabulary-supported by the schema but has no validated Ship build/package gate.

It is therefore absent from Downloads.

### Web 6 — AI Knowledge Layer

Next milestone owns:

- `Knowledge/`;
- stable project/system manifest;
- terminology;
- schemas;
- generated `llms.txt`;
- generated canonical-only `llms-full.txt`;
- raw/copy-for-AI documentation affordances.

---

## 18. Completion statement

**Web 5 — Downloads is COMPLETE for its defined scope.**

Nocturne now has an artifact-driven Downloads architecture in which a public support claim can only appear after an actual build/package/publication chain produces the required evidence.

The release gate also improved the underlying project by exposing and repairing previously incomplete Ship build configuration.

The production Downloads manifest remains intentionally empty because no real public release has been published yet.

The next milestone is **Web 6 — AI Knowledge Layer**.

---

## 19. Next chat handoff

Say:

> Continue with **Web 6 — AI Knowledge Layer** on branch `web-docs-foundation`. Read the Website specification, `Docs/Development/Documentation Model.md`, and the Web 5 implementation report first. Build the machine-readable project/knowledge layer from the existing canonical `noc.*` documentation, generate `llms.txt` and canonical-only `llms-full.txt`, add terminology/manifest schemas and raw/copy-for-AI affordances. Historical Phase docs must not silently become current truth. Keep the website independent from engine/runtime dependencies and do not merge draft PR #2 automatically.
