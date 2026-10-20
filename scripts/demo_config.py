from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEMOS_DIR = PROJECT_ROOT / "demos"

MODELS = {
    "dron": "EOLO_MODEL_DRON",
    "dron_legacy": "EOLO_MODEL_DRON_LEGACY",
    "standard": "EOLO_MODEL_STANDARD",
    "express": "EOLO_MODEL_EXPRESS",
    "express_legacy": "EOLO_MODEL_EXPRESS_LEGACY",
}

DEMO_CONFIG = {
    "AFM07": {
        "models": ["dron_legacy", "standard", "express", "express_legacy"],
        "lib_deps": ["4-20ma/ModbusMaster@^2.0.1"],
    },
    "Anemometer": {
        "models": ["standard", "express", "express_legacy"],
        "lib_deps": [],
    },
    "BME280": {
        "models": ["dron_legacy", "standard", "express", "express_legacy"],
        "lib_deps": [
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
        ],
    },
    "FS3000": {
        "models": ["dron_legacy", "standard", "express", "express_legacy"],
        "lib_deps": ["sparkfun/SparkFun_FS3000_Arduino_Library@^1.0.5"],
    },
    "Plantower": {
        "models": ["standard", "express", "express_legacy"],
        "lib_deps": [],
    },
    "DronFull": {
        "models": ["dron_legacy"],
        "lib_deps": [
            "adafruit/Adafruit NeoPixel@^1.12.5",
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
            "adafruit/RTClib@^2.1.4",
        ],
    },
    "DronProdTest": {
        "models": ["dron_legacy"],
        "lib_deps": [
            "adafruit/Adafruit NeoPixel@^1.12.5",
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
            "adafruit/RTClib@^2.1.4",
            "plerup/EspSoftwareSerial@^8.2.0",
            "bblanchon/ArduinoJson@^6.20.0",
        ],
    },
    "DynamicMotorCalibration": {
        "models": ["dron_legacy"],
        "lib_deps": ["4-20ma/ModbusMaster@^2.0.1"],
    },
    "AFM07ProfPins": {"models": ["express"], "lib_deps": []},
    "AFM07Scan": {"models": ["express"], "lib_deps": []},
    "I2CDisplayDiag": {"models": ["express"], "lib_deps": ["olikraus/U8g2@^2.36.14"]},
    "MotorDirDiag": {"models": ["express"], "lib_deps": []},
    "SPIStandardDisplayDiag": {
        "models": ["standard"],
        "lib_deps": [
            "olikraus/U8g2@^2.36.14",
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
            "adafruit/RTClib@^2.1.4",
        ],
    },
    # Referencia validada en hardware del Dron base (V2): OLED SH1106, encoder
    # en Wire1, SD, RTC, BME280, bomba PWM, AFM07 y bateria 2S.
    "DronBase": {
        "models": ["dron"],
        "lib_deps": [
            "olikraus/U8g2@^2.36.14",
            "adafruit/Adafruit Unified Sensor@^1.1.15",
            "adafruit/Adafruit BME280 Library@^2.3.0",
            "adafruit/RTClib@^2.1.4",
            "4-20ma/ModbusMaster@^2.0.1",
        ],
    },
    # Reproduce la concurrencia OLED I2C + encoder en Wire1 + sensores y mide la
    # perdida de eventos del encoder (algoritmo actual de Input vs acumulado).
    "DronEncoderTest": {
        "models": ["dron"],
        "lib_deps": ["olikraus/U8g2@^2.36.14"],
    },
    # Diagnóstico de la microSD: niveles de MISO, CMD0/CMD8 manuales y montaje
    # a varias frecuencias.
    "DronSdDiag": {"models": ["dron"], "lib_deps": []},
    "DIPSwitchDiag": {"models": ["dron_legacy"], "lib_deps": []},
}


def env_name(demo_name, model):
    return f"demo_{demo_name.lower()}_{model}"


def demo_sketches():
    return sorted(DEMOS_DIR.glob("*/*.ino"))


def config_for(demo_name):
    return DEMO_CONFIG.get(demo_name, {
        "models": ["dron_legacy"],
        "lib_deps": [],
    })
