#!/usr/bin/env python3
"""Arma el sitio de flasheo web (GitHub Pages) a partir de los releases.

Espera los paquetes descargados así:

    <releases>/<tag>/eolo-<env>-<tag>.zip
    <releases>/<tag>/release.json   {"tag", "date", "url"}

Cada zip trae bootloader.bin, partitions.bin, boot_app0.bin y firmware.bin.
El sitio queda en <out>/ con un manifest de ESP Web Tools por versión y env.
"""

import argparse
import json
import re
import shutil
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FLASHER_DIR = ROOT / "flasher"
LOGOS = [ROOT / "docs" / "assets" / n for n in ("logo-cplus.png", "logo-cplus-dark.png")]
REPO_URL = "https://github.com/Helix128/EoloXrd"

# Offsets de la tabla min_spiffs.csv. Se flashean por separado para no tocar
# la NVS (0x9000), donde viven la calibración, las presets y el Wi-Fi.
PARTS = [
    ("bootloader.bin", 0x1000),
    ("partitions.bin", 0x8000),
    ("boot_app0.bin", 0xE000),
    ("firmware.bin", 0x10000),
]


def version_key(tag):
    numbers = [int(n) for n in re.findall(r"\d+", tag)]
    return numbers or [0]


def load_variants():
    return json.loads((FLASHER_DIR / "variants.json").read_text(encoding="utf-8"))


def manifest(variant, tag):
    return {
        "name": variant["name"],
        "version": tag,
        "new_install_prompt_erase": True,
        "builds": [
            {
                "chipFamily": "ESP32",
                "parts": [{"path": name, "offset": offset} for name, offset in PARTS],
            }
        ],
    }


def extract_release(release_dir, out_dir, variants):
    meta = json.loads((release_dir / "release.json").read_text(encoding="utf-8"))
    tag = meta["tag"]
    available = []
    for variant in variants:
        env = variant["env"]
        package = release_dir / f"eolo-{env}-{tag}.zip"
        if not package.exists():
            continue
        target = out_dir / "firmware" / tag / env
        target.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(package) as zf:
            names = set(zf.namelist())
            missing = [name for name, _ in PARTS if name not in names]
            if missing:
                raise SystemExit(f"{package.name}: faltan {', '.join(missing)}")
            for name, _ in PARTS:
                (target / name).write_bytes(zf.read(name))
        shutil.copy2(package, target / package.name)
        (target / "manifest.json").write_text(
            json.dumps(manifest(variant, tag), indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8",
        )
        available.append(env)
    return {"tag": tag, "date": meta.get("date", ""), "url": meta.get("url", ""), "envs": available}


def build(releases_dir, out_dir):
    variants = load_variants()
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)

    versions = []
    if releases_dir.exists():
        for release_dir in sorted(p for p in releases_dir.iterdir() if p.is_dir()):
            entry = extract_release(release_dir, out_dir, variants)
            if entry["envs"]:
                versions.append(entry)
    versions.sort(key=lambda v: version_key(v["tag"]), reverse=True)

    for name in ("index.html", "app.js", "style.css"):
        shutil.copy2(FLASHER_DIR / name, out_dir / name)
    for logo in LOGOS:
        shutil.copy2(logo, out_dir / logo.name)
    site = {
        "repo": REPO_URL,
        "variants": [
            {**v, "manual": f"{REPO_URL}/blob/main/{v['manual']}"} for v in variants
        ],
        "versions": versions,
    }
    (out_dir / "site.json").write_text(
        json.dumps(site, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    return site


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--releases", type=Path, required=True, help="carpeta con <tag>/ descargados")
    parser.add_argument("--out", type=Path, required=True, help="carpeta de salida del sitio")
    args = parser.parse_args(argv)
    site = build(args.releases, args.out)
    print(f"Sitio en {args.out}: {len(site['versions'])} versiones")
    return 0


if __name__ == "__main__":
    sys.exit(main())
