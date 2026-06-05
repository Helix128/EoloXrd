#!/usr/bin/env python3
from pathlib import Path
import re
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

DEMO_TO_PINOUT = {
    "I2C_SDA_PIN": "SDA_PIN",
    "I2C_SCL_PIN": "SCL_PIN",
    "SD_CS_PIN": "SD_CS_PIN",
    "DIP_PIN_1": "WAIT_SW0_PIN",
    "DIP_PIN_2": "WAIT_SW1_PIN",
    "DIP_PIN_3": "DURATION_SW0_PIN",
    "DIP_PIN_4": "DURATION_SW1_PIN",
    "NEOPIXEL_PIN": "NEOPIXEL_PIN",
    "RS485_RX_PIN": "RS485_RX_PIN",
    "RS485_TX_PIN": "RS485_TX_PIN",
    "RS485_DE_RE_PIN": "RS485_DE_RE_PIN",
    "MOTOR_PWM_PIN": "MOTOR_PWM_PIN_0",
    "MOTOR_FG_PIN": "MOTOR_FG_PIN",
    "PT_RX_PIN": "PT_RX",
    "PT_TX_PIN": "PT_TX",
    "PPH_PWR_PIN": "PPH_PWR_PIN",
    "MODEM_PWR_PIN": "MODEM_PWR_PIN",
}


def eval_value(raw: str, values: dict[str, int]) -> int:
    raw = raw.strip()
    if raw == "EOLO_PIN_UNUSED":
        return -1
    if raw in values:
        return values[raw]
    return int(raw, 0)


def target_block(text: str, macro: str) -> str:
    start = text.index(macro)
    end = text.find("#elif", start + len(macro))
    else_pos = text.find("#else", start + len(macro))
    candidates = [p for p in (end, else_pos) if p != -1]
    return text[start:min(candidates) if candidates else len(text)]


def parse_defines(text: str, macro: str) -> dict[str, int]:
    values = {"EOLO_PIN_UNUSED": -1}
    for name, raw in re.findall(r"#define\s+(\w+)\s+([^\s/]+)", target_block(text, macro)):
        try:
            values[name] = eval_value(raw, values)
        except ValueError:
            pass
    return values


def parse_demo(text: str, macro: str) -> dict[str, int]:
    values = {"EOLO_PIN_UNUSED": -1}
    out = {}
    for name, raw in re.findall(r"static\s+const\s+int\s+(\w+)\s*=\s*([^;]+);", target_block(text, macro)):
        out[name] = eval_value(raw, values)
    return out


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
    pinout_text = PINOUT.read_text()
    demo_text = DEMO.read_text()
    errors = []

    for target, cfg in TARGETS.items():
        pinout = parse_defines(pinout_text, cfg["macro"])
        doc = parse_doc(cfg["doc"])
        demo = parse_demo(demo_text, cfg["demo"])

        for name, doc_value in sorted(doc.items()):
            if name not in pinout:
                errors.append(f"{cfg['doc']}: {name} no existe en Pinout.h para {target}")
            elif pinout[name] != doc_value:
                errors.append(f"{cfg['doc']}: {name}={doc_value}, Pinout.h={pinout[name]}")

        for demo_name, pinout_name in DEMO_TO_PINOUT.items():
            if demo_name not in demo:
                continue
            if pinout_name not in pinout:
                errors.append(f"{DEMO}: {demo_name} mapea a {pinout_name}, ausente en Pinout.h para {target}")
                continue
            if demo[demo_name] != pinout[pinout_name]:
                errors.append(f"{DEMO}: {target} {demo_name}={demo[demo_name]}, {pinout_name}={pinout[pinout_name]}")

    if errors:
        print("Pinout sync check failed:")
        for error in errors:
            print(f"- {error}")
        return 1

    print("Pinout sync check OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
