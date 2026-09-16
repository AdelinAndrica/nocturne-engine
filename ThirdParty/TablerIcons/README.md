# Tabler Icons subset for Nocturne Editor

Nocturne Editor vendors a small subset of **Tabler Icons 3.46.0** from https://github.com/tabler/tabler-icons.

Tabler Icons are MIT licensed. The upstream license is preserved in `LICENSE`.

The selected outline SVGs use the upstream 24x24 viewBox, 2px stroke, round line caps and round joins. Nocturne keeps SVG files as the source of truth and rasterizes/tints them at runtime for editor-only tooling chrome.

**Design choice (not directly from the book):** Tabler is the single primary icon vocabulary for Nocturne Editor. Custom icons should be limited to Nocturne-specific concepts such as the engine logo or domain-specific asset types that have no suitable Tabler equivalent.
