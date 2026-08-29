"""Regression check: published geometry previews must be real raster renders."""

from pathlib import Path
import struct


ROOT = Path(__file__).resolve().parents[1]
EXPECTED = (ROOT / "docs/images/gallery/rotary-tray-render.png",) + tuple(
    sorted((ROOT / "docs/images/gallery/parts-renders").glob("*.png"))
)


def png_size(path: Path):
    data = path.read_bytes()[:24]
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"not a PNG: {path.name}"
    return struct.unpack(">II", data[16:24])


def main():
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    assert "rotary-tray-render.svg" not in readme
    assert "printable-parts-render.svg" not in readme
    assert len(EXPECTED) == 14, f"expected tray plus 13 part renders, got {len(EXPECTED)}"
    for path in EXPECTED:
        assert path.exists(), f"missing raster render: {path.name}"
        width, height = png_size(path)
        minimum = (1200, 700) if path.name == "rotary-tray-render.png" else (600, 450)
        assert width >= minimum[0] and height >= minimum[1], f"render too small: {path.name} {width}x{height}"
    print("gallery raster renders: PASS")


if __name__ == "__main__":
    main()
