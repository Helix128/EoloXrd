#!/usr/bin/env python3
"""Verifica los limites entre familias de codigo en src/.

Familias: Ui, Headless, Modem, Variants, Common y Application (seleccion de
aplicacion). Reglas:

- Ui y Headless no se incluyen entre si.
- Headless no incluye Modem (Dron no tiene modem).
- Common no debe depender de una familia; las dependencias existentes estan en
  KNOWN_COMMON_DEPENDENCIES y solo pueden disminuir (pendientes de separar
  Context y Components por familia).
"""
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"
INCLUDE = re.compile(r'^\s*#\s*include\s*"([^"]+)"', re.M)
FAMILIES = ("Ui", "Headless", "Modem", "Variants", "Common", "Application")

FORBIDDEN = {
    ("Ui", "Headless"),
    ("Headless", "Ui"),
    ("Headless", "Modem"),
}

# (archivo que incluye, archivo incluido) relativos a src/.
KNOWN_COMMON_DEPENDENCIES = {
    ("Common/Data/Components.h", "Modem/Modem.h"),
    ("Common/Data/Components.h", "Modem/ModemService.h"),
    ("Common/Data/Components.h", "Modem/SensorAPI.h"),
    ("Common/Data/Components.h", "Ui/Input.h"),
    ("Common/Data/Context.h", "Ui/Drawing/SceneManager.h"),
    ("Common/Data/Context.h", "Ui/Drawing/Logos.h"),
    ("Common/Data/Context.h", "Headless/ContextHeadlessMotorCalibration.h"),
    ("Common/Data/ContextCaptureController.h", "Ui/Drawing/SceneManager.h"),
    ("Common/Utility/DebugConsole.h", "Headless/CaptureSwitchDebugCommands.h"),
    ("Common/Utility/DebugConsole.h", "Headless/DroneDebugCommands.h"),
    ("Common/Utility/DebugConsole.h", "Modem/ModemDebugCommands.h"),
    ("Common/Utility/DebugConsole.h", "Modem/ModemService.h"),
    ("Common/Data/RTCNetworkSync.h", "Modem/ModemService.h"),
    ("Common/Data/UploadService.h", "Modem/SensorAPI.h"),
}


def family(rel):
    head = rel.split("/", 1)[0]
    return head if head in FAMILIES else "Other"


def main():
    errors = []
    used_known = set()
    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".h", ".cpp"):
            continue
        rel = path.relative_to(SRC).as_posix()
        for inc in INCLUDE.findall(path.read_text(encoding="utf-8", errors="replace")):
            target = Path(os.path.normpath(path.parent / inc))
            if not target.is_relative_to(SRC) or not target.exists():
                continue
            trel = target.relative_to(SRC).as_posix()
            src_family, dst_family = family(rel), family(trel)
            if (src_family, dst_family) in FORBIDDEN:
                errors.append(f"{rel} incluye {trel} ({src_family} -> {dst_family})")
            elif src_family == "Common" and dst_family in ("Ui", "Headless", "Modem"):
                pair = (rel, trel)
                if pair in KNOWN_COMMON_DEPENDENCIES:
                    used_known.add(pair)
                else:
                    errors.append(f"{rel} incluye {trel}: Common no debe depender de {dst_family}")
    stale = KNOWN_COMMON_DEPENDENCIES - used_known
    for rel, trel in sorted(stale):
        errors.append(f"dependencia conocida ya inexistente, retirela de la lista: {rel} -> {trel}")
    if errors:
        print("\n".join(errors))
        return 1
    print(f"Limites entre familias OK ({len(used_known)} dependencias de Common pendientes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
