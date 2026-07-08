#!/usr/bin/env python3
"""Convert a logo PNG into an LVGL C-array image for the boot splash.

Accepts *any* PNG (any size, with or without alpha), resizes it to the target
width while preserving aspect ratio, and emits an LVGL v9 C source file
(RGB565A8 by default) that is compiled directly from src/ui/assets/.

Usage:
    python3 tools/convert_logo.py                      # uses defaults below
    python3 tools/convert_logo.py --input assets/my_logo.png --width 220

Re-run this whenever you replace assets/boot_logo.png. Requires Pillow and
pypng (see tools/requirements.txt); LVGLImage.py is provided by the LVGL
library under .pio/libdeps (present after a dependency resolve / first build).
"""

import argparse
import glob
import subprocess
import sys
import tempfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def find_lvgl_image_script() -> Path:
    matches = glob.glob(
        str(PROJECT_ROOT / ".pio" / "libdeps" / "**" / "lvgl" / "scripts" / "LVGLImage.py"),
        recursive=True,
    )
    if not matches:
        sys.exit(
            "ERROR: LVGLImage.py not found under .pio/libdeps. Run a build first "
            "(so PlatformIO fetches the LVGL library), then re-run this script."
        )
    return Path(sorted(matches)[0])


def resize_png(input_path: Path, target_width: int, dest_path: Path) -> tuple[int, int]:
    try:
        from PIL import Image
    except ImportError:
        sys.exit("ERROR: Pillow is required (pip install pillow).")

    img = Image.open(input_path).convert("RGBA")
    w, h = img.size
    if w != target_width:
        target_height = max(1, round(h * target_width / w))
        img = img.resize((target_width, target_height), Image.LANCZOS)
    img.save(dest_path)
    return img.size


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--input", default="assets/boot_logo.png", help="Source PNG (any size)")
    parser.add_argument("--output-dir", default="src/ui/assets", help="Where to write the generated .c")
    parser.add_argument("--name", default="boot_logo", help="LVGL image symbol / file name")
    parser.add_argument("--width", type=int, default=240, help="Target width in px (aspect preserved)")
    parser.add_argument("--cf", default="RGB565A8", help="LVGL color format (keeps alpha for the black splash)")
    args = parser.parse_args()

    input_path = (PROJECT_ROOT / args.input).resolve()
    if not input_path.exists():
        sys.exit(f"ERROR: input image not found: {input_path}")

    output_dir = (PROJECT_ROOT / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    lvgl_script = find_lvgl_image_script()

    with tempfile.TemporaryDirectory() as tmp:
        # Name the temp file after --name so LVGLImage.py emits <name>.c
        resized = Path(tmp) / f"{args.name}.png"
        size = resize_png(input_path, args.width, resized)

        cmd = [
            sys.executable, str(lvgl_script),
            "--ofmt", "C",
            "--cf", args.cf,
            "--name", args.name,
            "-o", str(output_dir),
            str(resized),
        ]
        print(f"Resizing {input_path.name} -> {size[0]}x{size[1]} ({args.cf})")
        result = subprocess.run(cmd)
        if result.returncode != 0:
            sys.exit("ERROR: LVGLImage.py conversion failed")

    out_c = output_dir / f"{args.name}.c"
    print(f"Generated {out_c.relative_to(PROJECT_ROOT)}")


if __name__ == "__main__":
    main()
