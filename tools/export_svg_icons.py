#!/usr/bin/env python3
"""Export every framed icon in assets/icons/icons.svg at an arbitrary size.

The SVG is a drawing sheet, not one SVG file per icon. Each manifest entry
points to the transparent 50x50 frame surrounding one icon. Inkscape renders
the complete drawing but crops it to that frame, preserving artwork composed
from several sibling paths.

Usage:
    python tools/export_svg_icons.py --size 64
    python tools/export_svg_icons.py --size 32 --output build/icons-32
"""

import argparse
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DEFAULT_SVG = ROOT / "assets" / "icons" / "icons.svg"


def run(inkscape: str, *args: str) -> str:
    result = subprocess.run(
        [inkscape, *args], check=True, text=True, capture_output=True
    )
    return result.stdout.strip()


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--size", type=int, default=50, help="square PNG size in pixels (default: 50)")
    parser.add_argument("--output", type=Path, help="output root (default: build/exported-icons/<size>)")
    parser.add_argument("--svg", type=Path, default=DEFAULT_SVG)
    parser.add_argument("--inkscape", default=shutil.which("inkscape") or r"C:\Program Files\Inkscape\bin\inkscape.exe")
    args = parser.parse_args()

    if args.size <= 0:
        parser.error("--size must be positive")
    output = (args.output or ROOT / "assets" / "icons" / f"icons_{args.size}").resolve()
    root = ET.parse(args.svg).getroot()
    icons = []
    for element in root.iter():
        relative_name = element.get("data-icon-path")
        frame_id = element.get("id")
        if relative_name and frame_id:
            icons.append((relative_name, frame_id))
    if not icons:
        raise SystemExit(f"No data-icon-path frames found in {args.svg}")

    # Render the complete set before touching the checked-in output. A failed
    # Inkscape process must not leave a folder containing a mixture of old and
    # partially regenerated icons.
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"icons-{args.size}-", dir=output.parent) as temporary:
        staging = Path(temporary)
        for relative_name, frame_id in icons:
            destination = staging / relative_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            run(
                args.inkscape,
                str(args.svg.resolve()),
                # Without --export-id-only, Inkscape uses the frame object's
                # bounds as the crop while still rendering sibling artwork.
                f"--export-id={frame_id}",
                f"--export-width={args.size}",
                f"--export-height={args.size}",
                "--export-background-opacity=0",
                f"--export-filename={destination.resolve()}",
            )
            if not destination.is_file() or destination.stat().st_size == 0:
                raise RuntimeError(f"Inkscape did not produce {relative_name}")

        for relative_name, _ in icons:
            destination = output / relative_name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(staging / relative_name, destination)

    print(f"Exported {len(icons)} icons at {args.size}x{args.size} to {output}")


if __name__ == "__main__":
    main()
