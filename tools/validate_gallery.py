"""Validate README image links and refresh the release checksum manifest."""

from __future__ import annotations

import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def main():
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    references = re.findall(r'(?:src="|!\[[^]]*\]\()([^"\)]+)', readme)
    missing = [reference for reference in references if not (ROOT / reference).exists()]
    if missing:
        raise SystemExit(f"Missing README images: {missing}")

    for svg in sorted((ROOT / "docs/images/gallery").glob("*.svg")):
        root = ET.parse(svg).getroot()
        polygons = root.findall("{http://www.w3.org/2000/svg}polygon")
        if not polygons:
            raise SystemExit(f"Render has no geometry: {svg.name}")
        print(f"{svg.name}: {len(polygons)} rendered faces")

    manifest = {
        str(path.relative_to(ROOT)).replace("\\", "/"): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in sorted(ROOT.rglob("*"))
        if path.is_file() and path.name != "SHA256SUMS.json" and ".git" not in path.parts
    }
    (ROOT / "SHA256SUMS.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"README images: {len(references)}; manifest files: {len(manifest)}")


if __name__ == "__main__":
    main()
