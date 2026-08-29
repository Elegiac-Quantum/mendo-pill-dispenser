"""Create lightweight, geometry-faithful SVG previews from binary or ASCII STL files."""

from __future__ import annotations

import math
import struct
import sys
from pathlib import Path


def read_stl(path: Path):
    data = path.read_bytes()
    if len(data) >= 84:
        count = struct.unpack_from("<I", data, 80)[0]
        if 84 + count * 50 == len(data):
            faces = []
            offset = 84
            for _ in range(count):
                values = struct.unpack_from("<12fH", data, offset)
                faces.append((values[3:6], values[6:9], values[9:12]))
                offset += 50
            return faces

    vertices = []
    for line in data.decode("utf-8", errors="ignore").splitlines():
        words = line.strip().split()
        if len(words) == 4 and words[0].lower() == "vertex":
            vertices.append(tuple(float(value) for value in words[1:]))
    return [tuple(vertices[index:index + 3]) for index in range(0, len(vertices) - 2, 3)]


def rotate(point):
    x, y, z = point
    azimuth = math.radians(-38)
    elevation = math.radians(58)
    x, y = x * math.cos(azimuth) - y * math.sin(azimuth), x * math.sin(azimuth) + y * math.cos(azimuth)
    y, z = y * math.cos(elevation) - z * math.sin(elevation), y * math.sin(elevation) + z * math.cos(elevation)
    return x, -z, y


def project_faces(faces, max_faces=1800):
    if len(faces) > max_faces:
        step = len(faces) / max_faces
        faces = [faces[int(index * step)] for index in range(max_faces)]
    projected = []
    for face in faces:
        points = [rotate(point) for point in face]
        projected.append((sum(point[2] for point in points) / 3, points))
    return sorted(projected, key=lambda item: item[0])


def render_panel(path: Path, x, y, width, height, label, color):
    faces = project_faces(read_stl(path))
    all_points = [point for _, face in faces for point in face]
    min_x = min(point[0] for point in all_points)
    max_x = max(point[0] for point in all_points)
    min_y = min(point[1] for point in all_points)
    max_y = max(point[1] for point in all_points)
    scale = min((width - 36) / max(max_x - min_x, 1), (height - 58) / max(max_y - min_y, 1))
    center_x = (min_x + max_x) / 2
    center_y = (min_y + max_y) / 2
    output = [f'<rect x="{x}" y="{y}" width="{width}" height="{height}" rx="18" fill="#f7faf9" stroke="#dce9e5"/>']
    light = (0.25, -0.4, 0.88)
    for _, face in faces:
        p0, p1, p2 = face
        ux, uy, uz = p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]
        vx, vy, vz = p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]
        nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        length = max(math.sqrt(nx * nx + ny * ny + nz * nz), 1e-9)
        shade = max(0.35, min(1.0, 0.58 + 0.42 * (nx * light[0] + ny * light[1] + nz * light[2]) / length))
        coords = " ".join(f"{x + width / 2 + (px - center_x) * scale:.1f},{y + (height - 22) / 2 + (py - center_y) * scale:.1f}" for px, py, _ in face)
        output.append(f'<polygon points="{coords}" fill="{color}" fill-opacity="{shade:.2f}" stroke="#173a34" stroke-opacity=".14" stroke-width=".45"/>')
    output.append(f'<text x="{x + 18}" y="{y + height - 16}" font-family="Segoe UI,Arial,sans-serif" font-size="16" font-weight="600" fill="#153d36">{label}</text>')
    return "\n".join(output)


def main():
    stl_dir = Path(sys.argv[1])
    output = Path(sys.argv[2])
    mode = sys.argv[3]
    if mode == "rotor":
        items = [(stl_dir / "03_rotor_15_cells.stl", "15-compartment rotary tray", "#172522")]
        width, height, columns = 1200, 760, 1
    else:
        names = [
            ("01_base_Mendo_EQ_Studio.stl", "Branded base"), ("02_fixed_deck.stl", "Fixed deck"),
            ("03_rotor_15_cells.stl", "Rotary tray"), ("04_servo_pinion.stl", "Servo pinion"),
            ("05_servo_cradle.stl", "Servo cradle"), ("06_medicine_cup.stl", "Medicine cup"),
            ("07_MT6701_adapter.stl", "MT6701 adapter"), ("08_screen_frame.stl", "Screen frame"),
            ("09_button_clamp.stl", "Button clamp"), ("10_dust_lid_body.stl", "Dust lid"),
            ("11_dust_lid_handle.stl", "Lid handle"), ("12_upper_shell.stl", "Upper shell"),
            ("13_upper_handle.stl", "Upper handle"),
        ]
        palette = ["#ec7d2d", "#50bfa5", "#172522"]
        items = [(stl_dir / filename, label, palette[index % len(palette)]) for index, (filename, label) in enumerate(names)]
        width, height, columns = 1500, 1320, 4
    rows = math.ceil(len(items) / columns)
    margin, gap = 28, 18
    panel_width = (width - margin * 2 - gap * (columns - 1)) / columns
    panel_height = (height - margin * 2 - gap * (rows - 1)) / rows
    panels = []
    for index, (path, label, color) in enumerate(items):
        column, row = index % columns, index // columns
        panels.append(render_panel(path, margin + column * (panel_width + gap), margin + row * (panel_height + gap), panel_width, panel_height, label, color))
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}"><rect width="100%" height="100%" fill="#eef6f3"/>{"".join(panels)}</svg>'
    output.write_text(svg, encoding="utf-8")


if __name__ == "__main__":
    main()
