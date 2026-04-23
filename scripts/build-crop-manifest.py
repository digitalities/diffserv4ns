#!/usr/bin/env python3
"""Analyse thesis PDF layout and produce crop-manifest.json.

For each figure caption ("Figure N.M" or "Figure A.Na" variants) in the body
pages, compute a crop bbox in PDF points that encloses the figure art + caption.
Each manifest entry yields one cropped PNG named ``fig-<label>.png``.

Usage: build-crop-manifest.py
Writes: thesis/img/crop-manifest.json
"""
from __future__ import annotations

import json
import re
import subprocess
from pathlib import Path
from xml.etree import ElementTree as ET

REPO = Path(__file__).resolve().parent.parent
PDF = REPO / "thesis" / "Andreozzi-2001-thesis.pdf"
MANIFEST_OUT = REPO / "thesis" / "img" / "crop-manifest.json"

# Page dimensions are constant across the thesis (A4): 595 x 842 pt.
# Empirical content margins (from pdftotext bbox on body pages):
PAGE_W = 595.0
PAGE_H = 842.0
LEFT_MARGIN = 105.0   # slightly outside the 113.4 text left edge to catch figure art
RIGHT_MARGIN = 545.0  # slightly outside 538.6 text right edge
TOP_MARGIN = 60.0     # header zone; all body text lives below
BOTTOM_MARGIN = 780.0 # below typical last body line; page numbers sit below this

# How much vertical padding above the top of the figure region (to avoid cutting art)
FIG_TOP_PAD = 2.0
# How much padding below the caption (to catch descenders)
CAPTION_BOT_PAD = 6.0
# Gap between stacked figures on the same page
INTERFIG_GAP = 12.0
# Tolerance for grouping blocks onto the same caption line (same yMin ± this)
CAPTION_LINE_TOL = 3.0
# Left-anchor test: body text starts at xMin ≤ this. UML diagram labels,
# table cells, and figure callouts sit further right and are excluded.
BODY_TEXT_XMIN_MAX = 135.0

# Skip front-matter and toc pages: captions listed there are not actual figures.
FIRST_BODY_PAGE = 15
LAST_BODY_PAGE = 116

# Regex for figure identifier after "Figure": 1.1, 3.11, A.1, B.5a, C.4b, etc.
FIG_ID_RE = re.compile(r"^(?:[A-C]\.\d+|\d+\.\d+)[a-z]?$")


def run_pdftotext_bbox(page: int) -> str:
    return subprocess.check_output(
        ["pdftotext", "-bbox-layout", "-f", str(page), "-l", str(page), str(PDF), "-"],
        text=True,
    )


def parse_blocks(xml: str) -> list[dict]:
    """Return a list of dicts {x0, y0, x1, y1, text, words, is_caption, fig_id}."""
    # The pdftotext output is valid XHTML; strip the DOCTYPE and parse with ET.
    xml = re.sub(r"<!DOCTYPE[^>]+>", "", xml)
    xml = xml.replace('xmlns="http://www.w3.org/1999/xhtml"', "")
    root = ET.fromstring(xml)
    # blocks live under page > flow > block
    page = root.find(".//page")
    if page is None:
        return []
    blocks = []
    for bl in page.findall(".//block"):
        words = []
        for w in bl.findall(".//word"):
            if w.text:
                words.append(w.text)
        if not words:
            continue
        is_cap = (
            len(words) >= 2
            and words[0] == "Figure"
            and FIG_ID_RE.match(words[1]) is not None
        )
        blocks.append(
            {
                "x0": float(bl.attrib["xMin"]),
                "y0": float(bl.attrib["yMin"]),
                "x1": float(bl.attrib["xMax"]),
                "y1": float(bl.attrib["yMax"]),
                "text": " ".join(words),
                "words": words,
                "is_caption": is_cap,
                "fig_id": words[1] if is_cap else None,
            }
        )
    return blocks


def captions_on(page: int) -> list[dict]:
    return [b for b in parse_blocks(run_pdftotext_bbox(page)) if b["is_caption"]]


def compute_bboxes(blocks: list[dict]) -> list[tuple[str, list[float]]]:
    """Return [(fig_id, [x0,y0,x1,y1])...] for each caption on the page.

    The crop top is just below the last body-text block preceding the caption
    (or the page top when no body text comes before). The crop bottom is the
    yMax of the full caption line — multi-block captions ("Figure X.Y" label
    + continuation text) are grouped by shared yMin.
    """
    caps = [b for b in blocks if b["is_caption"]]

    # Group each caption label with sibling blocks on the same line
    # (label "Figure 4.1" + text "Scenario 1: test bed" share yMin)
    # Also absorb any continuation lines that sit immediately below the label,
    # left-aligned with the caption text column, into the caption bottom.
    caption_groups = []
    caption_block_ids: set[int] = set()
    for cap in caps:
        siblings = [b for b in blocks if abs(b["y0"] - cap["y0"]) < CAPTION_LINE_TOL]
        for s in siblings:
            caption_block_ids.add(id(s))
        y1 = max(b["y1"] for b in siblings)
        # Absorb any text block whose yMin is just below y1 and lies within the
        # caption text column (xMin >= 180) — these are caption continuation lines.
        while True:
            cont = [
                b for b in blocks
                if y1 < b["y0"] <= y1 + 12 and b["x0"] >= 180
                and id(b) not in caption_block_ids
            ]
            if not cont:
                break
            y1 = max(b["y1"] for b in cont)
            for c in cont:
                caption_block_ids.add(id(c))
        caption_groups.append({"fig_id": cap["fig_id"], "y0": cap["y0"], "y1": y1})
    caption_groups.sort(key=lambda c: c["y0"])

    body = [
        b for b in blocks
        if id(b) not in caption_block_ids and b["x0"] <= BODY_TEXT_XMIN_MAX
    ]

    out = []
    floor = TOP_MARGIN
    for cap in caption_groups:
        above = [
            b for b in body
            if b["y0"] >= floor - 1 and b["y1"] <= cap["y0"] + 1
        ]
        if above:
            top = max(b["y1"] for b in above) + FIG_TOP_PAD
        else:
            top = floor + (INTERFIG_GAP if out else FIG_TOP_PAD)
        bottom = cap["y1"] + CAPTION_BOT_PAD
        out.append((cap["fig_id"], [LEFT_MARGIN, top, RIGHT_MARGIN, bottom]))
        floor = bottom
    return out


def main() -> int:
    manifest = []
    label_to_pages: dict[str, list[int]] = {}

    for page in range(FIRST_BODY_PAGE, LAST_BODY_PAGE + 1):
        blocks = parse_blocks(run_pdftotext_bbox(page))
        caps = [b for b in blocks if b["is_caption"]]
        if not caps:
            continue
        bboxes = compute_bboxes(blocks)
        for fig_id, bbox in bboxes:
            safe = fig_id.replace(".", "-")
            manifest.append(
                {
                    "label": fig_id,
                    "page": page,
                    "output": f"fig-{safe}.png",
                    "bbox_pt": [round(v, 2) for v in bbox],
                }
            )
            label_to_pages.setdefault(fig_id, []).append(page)

    # Sanity: warn on duplicates
    dups = {k: v for k, v in label_to_pages.items() if len(v) > 1}
    if dups:
        print(f"[warn] duplicate labels: {dups}")

    MANIFEST_OUT.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"wrote {len(manifest)} entries -> {MANIFEST_OUT}")
    print(f"unique figure labels: {len(label_to_pages)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
