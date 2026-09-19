---
title: Nocturne Engine Documentation
description: Architecture, systems, editor, development standards and implementation history for Nocturne Engine.
tableOfContents: false
---

Nocturne Engine targets **Windows**, uses **Modern C++ (C++20+)**, and is being developed for a **first-person survival horror** game.

The documentation portal is built around one rule: **current canonical architecture must be distinguishable from historical implementation phases**.

## Start here

- [Architecture overview](/docs/architecture/overview/)
- [Production Engineering Standard](/docs/development/production-engineering-standard/)
- [Website design & technical specification](/docs/development/website-specification/)
- [Development history](/docs/history/)

## Documentation model

| Surface | Purpose |
|---|---|
| Architecture | Current global contracts and dependency rules |
| Systems | Current subsystem behavior as canonical pages are introduced |
| Development | Engineering standards and workflows |
| Development History | Phase-by-phase implementation record |

> Phase documents explain **how Nocturne was built**. Canonical Architecture/System pages describe **how Nocturne works now**.

## Source of truth

The authored Markdown remains in the repository-root `Docs/` directory. The website synchronizes those files into Starlight before local development and production builds. Generated copies are never edited by hand.
