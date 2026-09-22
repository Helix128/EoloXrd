"""Inject the AFM07 poll gap for an explicitly selected build.

The production value remains in the firmware preprocessor contract.  A sweep
process can set EOLO_AFM07_POLL_GAP_MS for one PlatformIO invocation without
editing platformio.ini or exposing a runtime setter through the portal.
"""

import os
import re

Import("env")

raw = os.environ.get("EOLO_AFM07_POLL_GAP_MS", "").strip()
if raw:
    if not re.fullmatch(r"[0-9]+", raw):
        raise ValueError("EOLO_AFM07_POLL_GAP_MS debe ser un entero en milisegundos")

    gap = int(raw, 10)
    if gap < 50 or gap > 10000:
        raise ValueError("EOLO_AFM07_POLL_GAP_MS debe estar entre 50 y 10000 ms")

    env.Append(CPPDEFINES=[("EOLO_AFM07_POLL_GAP_MS", gap)])
    print("AFM07 poll gap de compilacion: %d ms" % gap)
