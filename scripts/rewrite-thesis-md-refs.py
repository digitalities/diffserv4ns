#!/usr/bin/env python3
"""Rewrite thesis markdown image references.

Replaces every ``![... Figure X.Y ...](img/page-NNN.png)`` with
``![... Figure X.Y ...](img/fig-X-Y.png)``. Compound references that
bundled two figures ("Figure A.1 -- ...; Figure A.2 -- ...") are split
into separate image lines so that each figure points at its own tight
crop.
"""
from __future__ import annotations

import re
from pathlib import Path

THESIS = Path(__file__).resolve().parent.parent / "thesis"

FIG_ID_RE = re.compile(r"Figure\s+([A-C]\.\d+[a-z]?|\d+\.\d+[a-z]?)")
IMG_REF_RE = re.compile(r"!\[(.+?)\]\(img/page-\d+\.png\)")


def label_to_filename(label: str) -> str:
    return f"fig-{label.replace('.', '-')}.png"


def rewrite_line(line: str) -> str:
    m = IMG_REF_RE.search(line)
    if not m:
        return line
    alt = m.group(1)
    # Split compound alt text on "; " boundaries — each part begins with "Figure X.Y"
    parts = [p.strip() for p in alt.split(";")]
    parts = [p for p in parts if p]
    if len(parts) == 1:
        # Single ref: just swap the path
        label = FIG_ID_RE.search(alt).group(1)
        return IMG_REF_RE.sub(f"![{alt}](img/{label_to_filename(label)})", line)
    # Compound: emit one image ref per part, each on its own line
    new_lines = []
    for p in parts:
        lm = FIG_ID_RE.search(p)
        if not lm:
            new_lines.append(p)
            continue
        label = lm.group(1)
        new_lines.append(f"![{p}](img/{label_to_filename(label)})")
    # Preserve trailing whitespace / surrounding text structure
    prefix = line[: m.start()]
    suffix = line[m.end():]
    # Join split refs with blank lines so they render as separate figures
    joined = "\n\n".join(new_lines)
    return f"{prefix}{joined}{suffix}"


def main() -> int:
    changed = 0
    for md in sorted(THESIS.glob("*.md")):
        original = md.read_text()
        new = "".join(rewrite_line(ln) for ln in original.splitlines(keepends=True))
        if new != original:
            md.write_text(new)
            # Rough: count image refs that moved
            changed += len(re.findall(r"fig-[A-C0-9][-0-9a-z]+\.png", new))
            print(f"  {md.name}")
    print(f"rewrote {changed} refs across markdown files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
