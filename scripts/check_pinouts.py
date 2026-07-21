#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PINOUT = ROOT / "src/Board/Pinout.h"
DEMO = ROOT / "demos/EoloDemoPinout.h"

TARGETS = {
    "dron": {
        "macro": "EOLO_TARGET_DRON",
        "demo": "EOLO_MODEL_DRON",
        "doc": ROOT / "pinouts/eolo-dron.md",
    },
    "standard": {
        "macro": "EOLO_TARGET_STANDARD",
        "demo": "EOLO_MODEL_STANDARD",
        "doc": ROOT / "pinouts/eolo-standard.md",
    },
    "express": {
        "macro": "EOLO_TARGET_EXPRESS",
        "demo": "EOLO_MODEL_EXPRESS",
        "doc": ROOT / "pinouts/eolo-express.md",
    },
    "express_legacy": {
        "macro": "EOLO_TARGET_EXPRESS_LEGACY",
        "demo": "EOLO_MODEL_EXPRESS_LEGACY",
        "doc": ROOT / "pinouts/eolo-express-legacy.md",
    },
}

DOC_DEFINES = {
    "SDA_PIN", "SCL_PIN", "SD_CS_PIN", "SD_MOSI_PIN", "SD_MISO_PIN", "SD_SCK_PIN",
    "WAIT_SW0_PIN", "WAIT_SW1_PIN", "DURATION_SW0_PIN", "DURATION_SW1_PIN",
    "NEOPIXEL_PIN", "NTC_PIN", "BATTERY_ADC_PIN", "RS485_RX_PIN", "RS485_TX_PIN",
    "RS485_DE_RE_PIN", "MOTOR_PWM_PIN_0", "MOTOR_PWM_PIN_1", "MOTOR_FG_PIN",
    "MOTOR_POWER_PIN", "PT_RX", "PT_TX", "PPH_PWR_PIN", "MODEM_PWR_PIN",
    "MODEM_RX_PIN", "MODEM_TX_PIN",
}

def eval_value(raw: str, values: dict[str, int]) -> int:
    raw = raw.strip()
    if raw == "EOLO_PIN_UNUSED":
        return -1
    if raw in values:
        return values[raw]
    return int(raw, 0)


def parse_defines(macro: str) -> dict[str, int]:
    result = subprocess.run(
        ["cc", "-E", "-dM", "-x", "c++", f"-D{macro}", str(PINOUT)],
        check=True,
        capture_output=True,
        text=True,
    )
    values = {"EOLO_PIN_UNUSED": -1}
    for name, raw in re.findall(r"#define\s+(\w+)\s+([^\s/]+)", result.stdout):
        try:
            values[name] = eval_value(raw, values)
        except ValueError:
            pass
    return values


def parse_doc(path: Path) -> dict[str, int]:
    out = {}
    for line in path.read_text().splitlines():
        m = re.search(r"`(GPIO\d+|EOLO_PIN_UNUSED)`\s*\|\s*`(\w+)`", line)
        if not m:
            continue
        raw, name = m.groups()
        if name in DOC_DEFINES:
            out[name] = -1 if raw == "EOLO_PIN_UNUSED" else int(raw[4:])
    return out


def main() -> int:
    demo_text = DEMO.read_text()
    errors = []

    if '#include "../src/Board/Pinout.h"' not in demo_text:
        errors.append(f"{DEMO}: no incluye la fuente autoritativa src/Board/Pinout.h")

    for target, cfg in TARGETS.items():
        pinout = parse_defines(cfg["macro"])
        doc = parse_doc(cfg["doc"])

        for name, doc_value in sorted(doc.items()):
            if name not in pinout:
                errors.append(f"{cfg['doc']}: {name} no existe en Pinout.h para {target}")
            elif pinout[name] != doc_value:
                errors.append(f"{cfg['doc']}: {name}={doc_value}, Pinout.h={pinout[name]}")

    if errors:
        print("Pinout sync check failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Pinout sync check OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
