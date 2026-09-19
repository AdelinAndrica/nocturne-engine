---
id: "noc.development.release-process"
doc_type: "standard"
canonical: true
status: "active"
subsystem: "Development"
phase_introduced: 16
description: "Canonical release packaging, artifact validation, manifest promotion and Downloads publication contract."
source_files: [".github/workflows/release-package.yml",".github/scripts/package-release.ps1","Schemas/release.schema.json","Website/src/data/releases.json","Website/src/data/release-schema.mjs","Website/src/pages/download.astro"]
source_docs: ["Docs/Production Engineering Standard.md","Docs/Web/Nocturne Website — Design & Technical Specification.md"]
book_grounding: ["Jason Gregory — Game Engine Architecture (3rd ed.), §1.7 Tools and the Asset Pipeline"]
---

# Release Process

## Purpose

The Nocturne release process defines when a build is allowed to become a public download.

A Visual Studio configuration is **not** evidence of public support.

A target becomes downloadable only after the pipeline has:

1. built the Ship configuration;
2. assembled the release bundle;
3. measured exact byte size;
4. generated a SHA-256 checksum;
5. published the archive;
6. promoted a validated release record into the website manifest.

This keeps the Downloads page aligned with actual artifacts rather than theoretical build configurations.

## Current validation state

Web 5 has validated the complete Ship build/package gate for:

- Windows `x86_64`;
- Windows `x86`.

Validation run:

`35468447675`

Both matrix jobs completed:

```text
Build Ship outputs         SUCCESS
Package release candidate  SUCCESS
Upload packaged candidate  SUCCESS
```

Candidate artifact names:

```text
nocturne-release-0.0.0-pr2-windows-x86_64
nocturne-release-0.0.0-pr2-windows-x86
```

Candidate package evidence:

| Architecture | Archive | Bytes | SHA-256 |
|---|---|---:|---|
| `x86_64` | `nocturne-engine-0.0.0-pr2-windows-x86_64.zip` | 2,762,266 | `c3812e289db7e3fb78f050256ac4a26c3ca8f06628855a16b1dc7402811bfdd2` |
| `x86` | `nocturne-engine-0.0.0-pr2-windows-x86.zip` | 2,555,997 | `77b123a464965002f4583ee7435f8bc092dc9d4a29ae74b061d97a3eef979869` |

These are ephemeral PR validation artifacts, not public releases.

## Current public state

There is currently no public Nocturne Engine release in the website manifest.

```text
Website/src/data/releases.json

schemaVersion: 1
latest: null
releases: []
```

Therefore `/download` exposes no download action.

This empty state is intentional.

## Build candidate pipeline

Release candidates are built by:

`.github/workflows/release-package.yml`

The workflow can run from:

- pull requests that change release-relevant build/package files;
- manual `workflow_dispatch`;
- semantic version tags beginning with `v`.

The architecture matrix is a **validation request**, not a public-support declaration.

An architecture is build/package-capable only if its matrix job completes all build and packaging steps successfully.

Web 5 currently proves this for `x86_64` and `x86`.

Public Downloads still require a real tagged publication plus reviewed manifest promotion.

## Ship build

The release gate builds:

- `NocturneEngine`;
- `NocturneHost`;
- `NocturneEditor`.

The required configuration is:

`Ship`

The current Windows build system uses MSBuild/Visual Studio project files.

**Design choice (not directly from the book):** release packaging currently overrides the CI toolset to the validated runner toolset while preserving the repository's project configuration semantics.

## Release bundle

The packager is:

`.github/scripts/package-release.ps1`

Each architecture bundle contains the current release outputs needed by the existing Ship runtime contract:

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

The exact bundle layout is **Design choice (not directly from the book)**.

It may evolve when Nocturne gains a dedicated SDK/install layout.

## Artifact identity

Current file naming:

```text
nocturne-engine-<version>-windows-<architecture>.zip
```

Examples of architecture identifiers accepted by the release metadata schema:

```text
x86_64
x86
arm64
```

Schema acceptance does **not** mean those architectures are currently supported.

Only published manifest entries are public Downloads truth.

## Checksums

Every packaged ZIP receives:

- exact byte size;
- SHA-256 digest;
- a sibling `.sha256` file.

The checksum embedded in release metadata must match the published archive.

## Release metadata

Machine-readable contract:

`Schemas/release.schema.json`

Executable website validation:

`Website/src/data/release-schema.mjs`

Validation command:

```powershell
cd Website
npm run validate:releases
```

Website development and production builds run this validation before Astro starts.

An invalid manifest fails the web build.

## Release manifest

Authored website data:

`Website/src/data/releases.json`

The manifest is ordered newest-first.

The `latest` value must be:

- `null` when there are no releases; or
- the version of the first release entry.

A release includes:

- semantic version;
- channel;
- publication date;
- release-notes URL;
- source commit SHA;
- one or more published build records.

A build record includes:

- platform;
- architecture;
- configuration;
- status;
- archive filename;
- exact byte count;
- SHA-256;
- HTTPS download URL.

## Channels

Current channel vocabulary:

| Channel | Meaning |
|---|---|
| `development` | development-facing release |
| `preview` | preview/testing release intended for broader evaluation |
| `stable` | release explicitly promoted as stable |

Channel labels are website/release policy.

**Design choice (not directly from the book).**

## Artifact states

Current build-record states:

| Status | UI meaning |
|---|---|
| `supported` | downloadable validated artifact |
| `experimental` | downloadable but explicitly experimental |
| `withdrawn` | historical record; no active download action |

A failed CI candidate is never added to the public manifest.

## Tagged release publication

When the release packaging workflow is triggered by a `v<version>` tag and all architecture jobs pass, the publish job creates a GitHub Release and attaches:

- packaged ZIP files;
- SHA-256 files;
- per-architecture release-record JSON files.

The website manifest is deliberately a separate reviewed source.

A GitHub Release existing by itself does not cause the website to claim support automatically.

Promotion tool:

```powershell
cd Website
npm run promote:release -- <release-record-x86_64.json> [additional-records...]
npm run validate:releases
npm run build
```

`Website/scripts/promote-release.mjs` verifies that all supplied records agree on version, channel, date, release notes and source commit. It rejects duplicate architecture tuples and refuses to overwrite an existing published version.

**Design choice (not directly from the book):** manifest promotion remains an explicit source-controlled action so a malformed/partial publication cannot silently change the public support matrix.

## Website publication

The Downloads page consumes only:

`Website/src/data/releases.json`

It does not infer support from:

- project configurations;
- solution platform names;
- source code architecture branches;
- untagged CI artifacts;
- failed package jobs.

The page displays, when available:

- version;
- channel;
- date;
- source commit;
- platform;
- architecture;
- Ship configuration;
- file size;
- SHA-256;
- release notes;
- download link.

## Architecture support rule

A target passes through three distinct states:

```text
Configured
    |
    v
CI build-capable
    |
    v
Published artifact
    |
    v
Downloads page
```

Do not collapse these states into one "supported" label.

This distinction is central to Web 5.

## Build-configuration defects discovered by Web 5

The first Ship validation runs exposed incomplete project configuration rather than runtime-feature defects.

The release gate found and repaired:

- missing C++20 language-standard / `NOC_SHIP` settings in the engine Ship configuration;
- incomplete Ship settings in `NocturneHost`;
- missing Unicode character-set configuration in the engine Ship configurations;
- engine Ship targets defaulting to executable output instead of `StaticLibrary`.

These were corrected in the existing Visual Studio project configuration so the Ship contract now matches the engine's actual C++20/static-library architecture.

**Design choice (not directly from the book):** Web 5 treats a configuration that exists in the solution but cannot complete the release gate as not release-capable.

## Failure handling

Release publication stops when:

- Ship compilation fails;
- a required output is missing;
- packaging fails;
- the archive cannot be hashed;
- release metadata is malformed;
- the website manifest violates its schema.

A release target that fails these gates is not represented as downloadable.

## Book grounding

Gregory §1.7 grounds the importance of reliable tooling and production pipelines around engine development.

The exact GitHub Actions workflow, semantic-version policy, ZIP layout, checksum contract, release schema and manifest-promotion process are **Design choice (not directly from the book)**.

## Related documentation

- [Production Engineering Standard](/docs/development/production-engineering-standard/)
- [Website Specification](/docs/development/website-specification/)
- [Downloads](/download)
