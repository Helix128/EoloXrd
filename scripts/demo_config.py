from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEMOS_DIR = PROJECT_ROOT / "demos"

MODELS = {
    "dron": "EOLO_MODEL_DRON",
    "standard": "EOLO_MODEL_STANDARD",
    "express": "EOLO_MODEL_EXPRESS",
    "express_legacy": "EOLO_MODEL_EXPRESS_LEGACY",
}

DEMO_CONFIG = {
    "AFM07": {
        "models": ["dron", "standard", "express", "express_legacy"],
        "lib_deps": ["4-20ma/ModbusMaster@^2.0.1"],
    },
    "Anemometer": {
        "models": ["standard", "express", "express_legacy"],
        "lib_deps": [],
    },
    "BME280": {
        "models": ["dron", "standard", "express", "express_legacy"],
        "lib_deps": [
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
        ],
    },
    "FS3000": {
        "models": ["dron", "standard", "express", "express_legacy"],
        "lib_deps": ["sparkfun/SparkFun_FS3000_Arduino_Library@^1.0.5"],
    },
    "Plantower": {
        "models": ["standard", "express", "express_legacy"],
        "lib_deps": [],
    },
    "DronFull": {
        "models": ["dron"],
        "lib_deps": [
            "adafruit/Adafruit NeoPixel@^1.12.5",
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
            "adafruit/RTClib@^2.1.4",
        ],
    },
    "DynamicMotorCalibration": {
        "models": ["dron"],
        "lib_deps": ["4-20ma/ModbusMaster@^2.0.1"],
    },
}


def env_name(demo_name, model):
    return f"demo_{demo_name.lower()}_{model}"


def demo_sketches():
    return sorted(DEMOS_DIR.glob("*/*.ino"))


def config_for(demo_name):
    return DEMO_CONFIG.get(demo_name, {
        "models": ["dron"],
        "lib_deps": [],
    })
