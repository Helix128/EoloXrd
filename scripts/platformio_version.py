"""Inject EOLO_FIRMWARE_VERSION from lib/EoloCore/library.json into every build."""

import json
import os
import re

Import("env")

project_dir = env.subst("$PROJECT_DIR")
library_json = os.path.join(project_dir, "lib", "EoloCore", "library.json")

with open(library_json, "r", encoding="utf-8") as fh:
    data = json.load(fh)

version = str(data.get("version", "")).strip()
if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?", version):
    raise ValueError(f"Version invalida en {library_json}: {version!r}")

env.Append(CPPDEFINES=[("EOLO_FIRMWARE_VERSION", f'\\"{version}\\"')])
