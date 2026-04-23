#!/usr/bin/env python3
"""Re-render referenced thesis pages at 300 DPI and crop to figure+caption.

Reads crop-manifest.json. Each entry:
    {"label": "...", "page": <int>, "output": "<filename>",
     "bbox_pt": [x0, y0, x1, y1]}
where bbox_pt is in PDF points (72 pt/in), origin top-left.

Outputs land in thesis/img/.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

REPO = Path(__file__).resolve().parent.parent
PDF = REPO / "thesis" / "Andreozzi-2001-thesis.pdf"
MANIFEST = REPO / "thesis" / "img" / "crop-manifest.json"
OUT_DIR = REPO / "thesis" / "img"
DPI = 300
PTS_PER_INCH = 72.0


def render_page(page: int, dst_dir: Path) -> Path:
    prefix = dst_dir / f"r{page:03d}"
    subprocess.run(
        [
            "pdftocairo", "-png", "-r", str(DPI),
            "-f", str(page), "-l", str(page),
            str(PDF), str(prefix),
        ],
        check=True,
    )
    candidates = list(dst_dir.glob(f"r{page:03d}-*.png"))
    if not candidates:
        raise RuntimeError(f"pdftocairo produced nothing for page {page}")
    return candidates[0]


def crop_points(src: Path, bbox_pt, dst: Path) -> None:
    scale = DPI / PTS_PER_INCH
    with Image.open(src) as im:
        x0, y0, x1, y1 = bbox_pt
        box = (
            int(x0 * scale),
            int(y0 * scale),
            int(x1 * scale),
            int(y1 * scale),
        )
        im.crop(box).save(dst, optimize=True)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", nargs="+", help="Only render these labels (e.g., 4.1 3.6)")
    ap.add_argument("--out-dir", type=Path, default=OUT_DIR,
                    help="Override output directory (default: thesis/img)")
    args = ap.parse_args()

    entries = json.loads(MANIFEST.read_text())
    if args.only:
        wanted = set(args.only)
        entries = [e for e in entries if e["label"] in wanted]
        if not entries:
            print(f"[error] no labels matched {wanted}")
            return 1

    args.out_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        render_cache: dict[int, Path] = {}
        for e in entries:
            if not e.get("bbox_pt"):
                print(f"[skip] {e['label']} has no bbox_pt")
                continue
            page = e["page"]
            if page not in render_cache:
                render_cache[page] = render_page(page, td)
            out = args.out_dir / e["output"]
            crop_points(render_cache[page], e["bbox_pt"], out)
            print(f"  fig {e['label']:6s} page {page:3d} -> {out.name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
