#!/usr/bin/env python3
"""
scripts/device_firmware_backup.py - Respaldo de firmware instalado en ESP32 EOLO
y extracción de metadatos (modelo, versión, particiones, OTA, NVS y match local).
"""

import argparse
import datetime
import hashlib
import importlib.util
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import tarfile
import zlib
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BACKUPS_DIR = PROJECT_ROOT / "firmware_backups"
DEFAULT_FLASH_BACKUPS_DIR = PROJECT_ROOT / "flash_backups"

BOOTLOADER_OFFSET = 0x1000
PARTITION_TABLE_OFFSET = 0x8000
PARTITION_TABLE_SIZE = 0x1000
OTADATA_OFFSET = 0xE000
OTADATA_SIZE = 0x2000
DEFAULT_APP_OFFSET = 0x10000

ESP_IMAGE_MAGIC = 0xE9
ESP_APP_DESC_MAGIC = 0xABCD5432
PART_MAGIC = 0x50AA
PART_MD5_MAGIC = 0xEBEB

PART_TYPE_NAMES = {
    0x00: "app",
    0x01: "data",
}

APP_SUBTYPE_NAMES = {
    0x00: "factory",
    **{0x10 + i: f"ota_{i}" for i in range(16)},
    0x20: "test",
}

DATA_SUBTYPE_NAMES = {
    0x00: "ota",
    0x01: "phy",
    0x02: "nvs",
    0x03: "coredump",
    0x04: "nvs_keys",
    0x05: "efuse",
    0x06: "undefined",
    0x80: "esphttpd",
    0x81: "fat",
    0x82: "spiffs",
    0x83: "littlefs",
}

SPI_MODES = {0: "qio", 1: "qout", 2: "dio", 3: "dout"}
SPI_FREQS = {0x0: "40m", 0x1: "26m", 0x2: "20m", 0xF: "80m"}
FLASH_SIZES = {0x0: "1MB", 0x1: "2MB", 0x2: "4MB", 0x3: "8MB", 0x4: "16MB"}
OTA_STATES = {
    0x0: "NEW",
    0x1: "PENDING_VERIFY",
    0x2: "VALID",
    0x3: "INVALID",
    0x4: "ABORTED",
    0xFFFFFFFF: "UNDEFINED",
}

# Ordenado de más específico a más general para evitar colisiones (ej. Express FE vs Express).
EOLO_MODELS = [
    ("EOLO Express FE", "EOLO Express Rev.1", ["eolo_express_legacy", "eolo_express_legacy_spi"]),
    ("EOLO Express", "EOLO Express Rev.2", ["eolo_express"]),
    ("EOLO Standard", "EOLO-MP", ["eolo_standard"]),
    ("EOLO Dron Legacy", "EOLO Mini Rev.1", ["eolo_dron_legacy", "eolo_dron_legacy_low_power"]),
    ("EOLO Dron", "EOLO Mini Rev.2", ["eolo_dron"]),
]

SENSITIVE_NVS_KEY_RE = re.compile(
    r"(pass|pwd|psk|secret|token|auth|cred|apikey|api_key)",
    re.IGNORECASE,
)

SEMVER_CSTR_RE = re.compile(rb"\x00(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?)\x00")


def sanitize_name(name: str) -> str:
    """Convierte un texto en un nombre seguro para archivos y carpetas."""
    if not name:
        return ""
    clean = re.sub(r"[^\w\-\.]+", "_", name.strip())
    clean = re.sub(r"_+", "_", clean).strip("_")
    if not clean.strip("."):
        return ""
    return clean


def _esp_crc32_le_uint32(value: int) -> int:
    """Calcula el CRC32 usado por ESP-ROM en esp_ota_select_entry_t."""
    return zlib.crc32(struct.pack("<I", value & 0xFFFFFFFF), 0xFFFFFFFF) & 0xFFFFFFFF


def parse_esp32_image(buf: bytes) -> dict | None:
    """Analiza la cabecera de una imagen ESP32, su tamaño exacto y SHA-256."""
    if len(buf) < 24 or buf[0] != ESP_IMAGE_MAGIC:
        return None

    seg_count = buf[1]
    spi_mode_raw = buf[2]
    spi_speed_size = buf[3]
    entry_point = struct.unpack_from("<I", buf, 4)[0]
    hash_appended = buf[23]

    off = 24
    segments = []
    for _ in range(seg_count):
        if off + 8 > len(buf):
            return None
        load_addr, seg_len = struct.unpack_from("<II", buf, off)
        off += 8
        if off + seg_len > len(buf):
            return None
        segments.append({"load_addr": hex(load_addr), "size_bytes": seg_len})
        off += seg_len

    end_before_hash = ((off + 16) // 16) * 16
    total_size = end_before_hash + (32 if hash_appended == 1 else 0)
    if total_size > len(buf):
        return None

    sha_valid = None
    embedded_sha256 = None
    if hash_appended == 1:
        embedded_sha256 = buf[end_before_hash:total_size].hex()
        calc_sha = hashlib.sha256(buf[:end_before_hash]).hexdigest()
        sha_valid = embedded_sha256 == calc_sha

    flash_freq_nibble = spi_speed_size & 0x0F
    flash_size_nibble = (spi_speed_size >> 4) & 0x0F

    return {
        "valid": True,
        "segment_count": seg_count,
        "entry_point": hex(entry_point),
        "spi_mode": SPI_MODES.get(spi_mode_raw, f"0x{spi_mode_raw:02x}"),
        "flash_freq": SPI_FREQS.get(flash_freq_nibble, f"0x{flash_freq_nibble:x}"),
        "flash_size_header": FLASH_SIZES.get(flash_size_nibble, f"0x{flash_size_nibble:x}"),
        "hash_appended": hash_appended == 1,
        "sha256_valid": sha_valid,
        "embedded_sha256": embedded_sha256,
        "total_size_bytes": total_size,
        "image_sha256": hashlib.sha256(buf[:total_size]).hexdigest(),
    }


def parse_app_desc(app_bytes: bytes) -> dict | None:
    """Extrae esp_app_desc_t (offset 32 o búsqueda de magic 0xABCD5432)."""
    if len(app_bytes) < 288:
        return None

    idx = -1
    if struct.unpack_from("<I", app_bytes, 32)[0] == ESP_APP_DESC_MAGIC:
        idx = 32
    else:
        pos = app_bytes.find(struct.pack("<I", ESP_APP_DESC_MAGIC))
        if pos != -1 and pos + 256 <= len(app_bytes):
            idx = pos

    if idx < 0:
        return None

    secure_version = struct.unpack_from("<I", app_bytes, idx + 4)[0]
    sdk_version = app_bytes[idx + 16 : idx + 48].split(b"\x00", 1)[0].decode("utf-8", "replace")
    project_name = app_bytes[idx + 48 : idx + 80].split(b"\x00", 1)[0].decode("utf-8", "replace")
    compile_time = app_bytes[idx + 80 : idx + 96].split(b"\x00", 1)[0].decode("utf-8", "replace")
    compile_date = app_bytes[idx + 96 : idx + 112].split(b"\x00", 1)[0].decode("utf-8", "replace")
    idf_ver = app_bytes[idx + 112 : idx + 144].split(b"\x00", 1)[0].decode("utf-8", "replace")
    elf_sha256 = app_bytes[idx + 144 : idx + 176].hex()

    return {
        "offset": hex(idx),
        "secure_version": secure_version,
        "sdk_version": sdk_version,
        "project_name": project_name,
        "compile_date": compile_date,
        "compile_time": compile_time,
        "idf_ver": idf_ver,
        "app_elf_sha256": elf_sha256,
        "note": "En compilaciones Arduino-ESP32, sdk_version/idf_ver reflejan el core ESP-IDF/Arduino, no la versión de EOLO.",
    }


def parse_partition_table(table_bytes: bytes) -> dict:
    """Parsea una tabla de particiones ESP32 (0x50AA) y valida su MD5 (0xEBEB)."""
    partitions = []
    md5_present = False
    md5_valid = None

    for off in range(0, len(table_bytes) - 31, 32):
        entry = table_bytes[off : off + 32]
        magic, ptype, subtype, poff, psize = struct.unpack("<HBBII", entry[:12])
        if magic == PART_MAGIC:
            label = entry[12:28].split(b"\x00", 1)[0].decode("utf-8", "replace")
            flags = struct.unpack("<I", entry[28:32])[0]
            type_name = PART_TYPE_NAMES.get(ptype, f"0x{ptype:02x}")
            if ptype == 0x00:
                subtype_name = APP_SUBTYPE_NAMES.get(subtype, f"0x{subtype:02x}")
            elif ptype == 0x01:
                subtype_name = DATA_SUBTYPE_NAMES.get(subtype, f"0x{subtype:02x}")
            else:
                subtype_name = f"0x{subtype:02x}"
            partitions.append({
                "label": label,
                "type": type_name,
                "subtype": subtype_name,
                "offset": poff,
                "offset_hex": hex(poff),
                "size": psize,
                "size_hex": hex(psize),
                "flags": flags,
            })
        elif magic == PART_MD5_MAGIC:
            md5_present = True
            stored_md5 = entry[16:32]
            calc_md5 = hashlib.md5(table_bytes[:off]).digest()
            md5_valid = stored_md5 == calc_md5
            break
        elif entry == b"\xff" * 32:
            break
        else:
            break

    return {
        "valid": len(partitions) > 0 and (md5_valid is not False),
        "md5_present": md5_present,
        "md5_valid": md5_valid,
        "partitions": partitions,
    }


def parse_otadata(otadata_bytes: bytes, app_partitions: list[dict]) -> dict:
    """Determina la partición app activa a partir de los 2 sectores de otadata."""
    ota_parts = [p for p in app_partitions if p.get("subtype", "").startswith("ota_")]
    factory_part = next((p for p in app_partitions if p.get("subtype") == "factory"), None)
    default_part = factory_part or (ota_parts[0] if ota_parts else (app_partitions[0] if app_partitions else None))

    slots = []
    valid_candidates = []
    for idx in range(2):
        sec_off = idx * 4096
        if sec_off + 32 > len(otadata_bytes):
            break
        sec = otadata_bytes[sec_off : sec_off + 32]
        ota_seq, seq_label_raw, ota_state, crc = struct.unpack("<I20sII", sec)
        unwritten = ota_seq == 0xFFFFFFFF and ota_state == 0xFFFFFFFF and crc == 0xFFFFFFFF
        crc_ok = False if unwritten else (crc == _esp_crc32_le_uint32(ota_seq))
        usable = (not unwritten) and crc_ok and (ota_state not in (0x3, 0x4))
        slot_info = {
            "slot": idx,
            "ota_seq": None if unwritten else ota_seq,
            "ota_state": OTA_STATES.get(ota_state, f"0x{ota_state:08x}"),
            "crc_valid": None if unwritten else crc_ok,
            "unwritten": unwritten,
        }
        slots.append(slot_info)
        if usable and ota_parts:
            target = ota_parts[(ota_seq - 1) % len(ota_parts)]
            valid_candidates.append((ota_seq, target, slot_info))

    if valid_candidates:
        valid_candidates.sort(key=lambda item: item[0], reverse=True)
        _, active_part, _ = valid_candidates[0]
        return {
            "active_label": active_part["label"],
            "active_offset": active_part["offset"],
            "reason": "otadata_seq",
            "slots": slots,
        }

    return {
        "active_label": default_part["label"] if default_part else None,
        "active_offset": default_part["offset"] if default_part else None,
        "reason": "default_factory_or_ota0",
        "slots": slots,
    }


def parse_nvs_partition(nvs_bytes: bytes) -> dict:
    """Parsea páginas NVS (4 KB) ocultando cualquier clave sensible."""
    namespaces = {}
    entries = {}
    pages_active = 0

    for p_off in range(0, len(nvs_bytes) - 4095, 4096):
        page = nvs_bytes[p_off : p_off + 4096]
        state = struct.unpack_from("<I", page, 0)[0]
        if state in (0xFFFFFFFF, 0x00000000):
            continue
        pages_active += 1
        bitmap = page[32:64]
        i = 0
        while i < 126:
            st = (bitmap[i // 4] >> ((i % 4) * 2)) & 0x3
            if st != 0x2:
                i += 1
                continue
            off = 64 + i * 32
            ns_idx, etype, span, _chunk_idx, _crc = struct.unpack_from("<BBBBI", page, off)
            key = page[off + 8 : off + 24].split(b"\x00", 1)[0].decode("utf-8", "replace")
            raw_val = page[off + 24 : off + 32]

            if ns_idx == 0 and etype == 0x01:
                namespaces[raw_val[0]] = key
            else:
                ns_name = namespaces.get(ns_idx, f"ns_{ns_idx}")
                ns_bucket = entries.setdefault(ns_name, {})
                is_sensitive = bool(SENSITIVE_NVS_KEY_RE.search(key))
                if is_sensitive:
                    ns_bucket[key] = {"type": f"0x{etype:02x}", "value": "<redacted>", "redacted": True}
                elif etype == 0x01:
                    ns_bucket[key] = struct.unpack_from("<B", raw_val, 0)[0]
                elif etype == 0x11:
                    ns_bucket[key] = struct.unpack_from("<b", raw_val, 0)[0]
                elif etype == 0x02:
                    ns_bucket[key] = struct.unpack_from("<H", raw_val, 0)[0]
                elif etype == 0x12:
                    ns_bucket[key] = struct.unpack_from("<h", raw_val, 0)[0]
                elif etype == 0x04:
                    ns_bucket[key] = struct.unpack_from("<I", raw_val, 0)[0]
                elif etype == 0x14:
                    ns_bucket[key] = struct.unpack_from("<i", raw_val, 0)[0]
                elif etype == 0x08:
                    ns_bucket[key] = struct.unpack_from("<Q", raw_val, 0)[0]
                elif etype == 0x18:
                    ns_bucket[key] = struct.unpack_from("<q", raw_val, 0)[0]
                elif etype == 0x21:
                    slen = struct.unpack_from("<H", raw_val, 0)[0]
                    if off + 32 + slen <= 4096:
                        sdata = page[off + 32 : off + 32 + slen]
                        ns_bucket[key] = sdata.split(b"\x00", 1)[0].decode("utf-8", "replace")
                    else:
                        ns_bucket[key] = {"type": "string", "size_bytes": slen}
                elif etype in (0x41, 0x42, 0x48):
                    slen = struct.unpack_from("<H", raw_val, 0)[0]
                    ns_bucket[key] = {"type": "blob", "size_bytes": slen}
                else:
                    ns_bucket[key] = {"type": f"0x{etype:02x}"}
            i += max(1, span)

    return {
        "active_pages": pages_active,
        "namespaces": sorted(set(namespaces.values())),
        "entries": entries,
    }


def _infer_eolo_version_near_markers(app_bytes: bytes) -> str | None:
    """Infiere la versión de EOLO solo si aparece junto a marcadores propios de UI/firmware."""
    anchors = []
    for marker in (b"v%s\x00", b"Iniciando %s", b"AcercaDeScene"):
        pos = app_bytes.find(marker)
        if pos != -1:
            anchors.append(pos)

    if not anchors:
        return None

    for anchor in anchors:
        start = max(0, anchor - 8192)
        end = min(len(app_bytes), anchor + 8192)
        window = app_bytes[start:end]
        for match in SEMVER_CSTR_RE.finditer(window):
            candidate = match.group(1).decode("ascii", "ignore")
            # Evitar falsos positivos de versiones de ESP-IDF (4.x / 5.x)
            if candidate.startswith(("4.4.", "5.0.", "5.1.", "5.2.", "5.3.")):
                continue
            return candidate
    return None


def detect_eolo_heuristics(app_bytes: bytes) -> dict:
    """Detecta modelo, entorno candidato, versión probable y capacidades desde el binario."""
    matched_model = None
    product_name = None
    candidate_envs = []

    for model_id, prod_name, envs in EOLO_MODELS:
        token = model_id.encode("utf-8") + b"\x00"
        if token in app_bytes:
            matched_model = model_id
            product_name = prod_name
            candidate_envs = list(envs)
            break

    kind = "firmware"
    demo_or_test = None
    if matched_model is None:
        test_m = re.search(rb"test/(test_[A-Za-z0-9_]+)/test_main\.cpp", app_bytes)
        demo_m = re.search(rb"demos/([A-Za-z0-9_]+)/", app_bytes)
        if test_m:
            kind = "test_suite"
            demo_or_test = test_m.group(1).decode("ascii", "replace")
        elif demo_m:
            kind = "demo"
            demo_or_test = demo_m.group(1).decode("ascii", "replace")

    features = []
    if b"Iniciando pantalla..." in app_bytes:
        features.append("ui")
    if b"HeadlessSetupServer" in app_bytes or b"eolo.setup" in app_bytes:
        features.append("headless")
    if b"FS3000 inicializado" in app_bytes or b"FS3000.h" in app_bytes:
        features.append("flow_fs3000")
    if b"RS485 AFM07" in app_bytes or b"AFM07" in app_bytes:
        features.append("flow_afm07")
    if b"PlantowerTask" in app_bytes or b"Plantower.h" in app_bytes:
        features.append("plantower")
    if b"api-sensores.cmasccp.cl" in app_bytes or b"TinyGSM" in app_bytes:
        features.append("modem")
    if b"Anemometro" in app_bytes:
        features.append("anemometer")
    if b"NTC.h" in app_bytes or b"Kick bloqueado: AFM07 debe estar fresco y NTC valido." in app_bytes:
        features.append("ntc")
    if b"Directorio /EOLO" in app_bytes:
        features.append("sd_logging")

    # Refinar candidato entre d_legacy / low_power o express_legacy / spi si hay pista obvia
    if matched_model == "EOLO Dron Legacy" and b"STATUS_LED_LOW_POWER" in app_bytes:
        candidate_envs = ["eolo_dron_legacy_low_power"]

    inferred_version = _infer_eolo_version_near_markers(app_bytes)

    return {
        "kind": kind,
        "demo_or_test": demo_or_test,
        "model": matched_model,
        "product_name": product_name,
        "candidate_envs": candidate_envs,
        "version": inferred_version,
        "features": features,
    }


def match_local_firmware_backups(app_sha256: str, backups_dir: Path = DEFAULT_BACKUPS_DIR) -> list[dict]:
    """Busca coincidencias exactas por SHA-256 en firmware_backups/ (latest e history/*.tar.xz)."""
    if not app_sha256 or not backups_dir or not backups_dir.exists():
        return []

    matches = []

    def _record_if_match(meta: dict, source_path: Path):
        fw_sha = meta.get("files", {}).get("firmware", {}).get("sha256", "")
        if fw_sha and fw_sha.lower() == app_sha256.lower():
            matches.append({
                "target": meta.get("target"),
                "label": meta.get("label"),
                "version_tag": meta.get("version_tag"),
                "created_at": meta.get("created_at") or meta.get("timestamp"),
                "git": meta.get("git"),
                "backup_path": str(source_path),
            })

    for meta_file in sorted(backups_dir.rglob("build_info.json")):
        try:
            meta = json.loads(meta_file.read_text(encoding="utf-8"))
            _record_if_match(meta, meta_file.parent)
        except (OSError, ValueError, json.JSONDecodeError):
            continue

    for archive_path in sorted(backups_dir.rglob("*.tar.xz")):
        try:
            with tarfile.open(archive_path, "r:xz") as archive:
                member = next((m for m in archive.getmembers() if m.name.endswith("/build_info.json")), None)
                if not member:
                    continue
                extracted = archive.extractfile(member)
                if extracted is None:
                    continue
                meta = json.loads(extracted.read().decode("utf-8"))
                _record_if_match(meta, archive_path)
        except (OSError, ValueError, tarfile.TarError, json.JSONDecodeError):
            continue

    return matches


def _extract_embedded_metadata(app_bytes: bytes) -> dict | None:
    """Soporta detección opcional de EOLO_META_V1 si estuviera presente."""
    match = re.search(rb"EOLO_META_V1\|([^\x00]{1,512})\|END(?:\x00|$)", app_bytes)
    if not match:
        return None
    raw = match.group(1).decode("utf-8", "replace")
    parsed = {}
    for item in raw.split("|"):
        if "=" in item:
            k, v = item.split("=", 1)
            if k == "features":
                parsed[k] = [f for f in v.split(",") if f]
            else:
                parsed[k] = v
    return parsed


def analyze_binary(
    data: bytes,
    source_path: str = "",
    backups_dir: Path | None = DEFAULT_BACKUPS_DIR,
    chip_info: dict | None = None,
) -> dict:
    """Analiza un dump completo de flash ESP32 o un firmware.bin independiente."""
    is_full_flash = False
    partition_table = None

    if len(data) >= PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE:
        pt = parse_partition_table(data[PARTITION_TABLE_OFFSET : PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE])
        if pt["valid"]:
            is_full_flash = True
            partition_table = pt

    bootloader_info = None
    bootloader_bytes = b""
    partitions_bytes = b""
    otadata_info = None
    app_slots = []
    nvs_info = None
    nvs_bytes = b""

    active_app_bytes = b""
    active_app_image = None
    active_app_desc = None
    active_partition_label = None

    if is_full_flash and partition_table:
        image_type = "full_flash"
        partitions_bytes = data[PARTITION_TABLE_OFFSET : PARTITION_TABLE_OFFSET + PARTITION_TABLE_SIZE]
        if len(data) > BOOTLOADER_OFFSET:
            bl_parsed = parse_esp32_image(data[BOOTLOADER_OFFSET:PARTITION_TABLE_OFFSET])
            if bl_parsed:
                bootloader_info = bl_parsed
                bootloader_bytes = data[BOOTLOADER_OFFSET : BOOTLOADER_OFFSET + bl_parsed["total_size_bytes"]]

        app_parts = [p for p in partition_table["partitions"] if p["type"] == "app"]
        ota_part = next((p for p in partition_table["partitions"] if p["type"] == "data" and p["subtype"] == "ota"), None)
        if ota_part and ota_part["offset"] + ota_part["size"] <= len(data):
            otadata_info = parse_otadata(data[ota_part["offset"] : ota_part["offset"] + ota_part["size"]], app_parts)
        else:
            otadata_info = parse_otadata(b"", app_parts)

        active_partition_label = otadata_info.get("active_label")

        for part in app_parts:
            poff = part["offset"]
            psize = part["size"]
            if poff >= len(data):
                continue
            slot_slice = data[poff : min(len(data), poff + psize)]
            img_info = parse_esp32_image(slot_slice)
            desc_info = parse_app_desc(slot_slice) if img_info else None
            slot_entry = {
                "label": part["label"],
                "subtype": part["subtype"],
                "offset": part["offset_hex"],
                "size_bytes": psize,
                "has_valid_image": img_info is not None,
                "image": img_info,
                "app_desc": desc_info,
            }
            app_slots.append(slot_entry)

            if part["label"] == active_partition_label and img_info:
                active_app_bytes = slot_slice[: img_info["total_size_bytes"]]
                active_app_image = img_info
                active_app_desc = desc_info

        if not active_app_bytes:
            for slot in app_slots:
                if slot["has_valid_image"]:
                    part = next(p for p in app_parts if p["label"] == slot["label"])
                    active_partition_label = slot["label"]
                    active_app_bytes = data[part["offset"] : part["offset"] + slot["image"]["total_size_bytes"]]
                    active_app_image = slot["image"]
                    active_app_desc = slot["app_desc"]
                    break

        nvs_part = next((p for p in partition_table["partitions"] if p["type"] == "data" and p["subtype"] == "nvs"), None)
        if nvs_part and nvs_part["offset"] + nvs_part["size"] <= len(data):
            nvs_bytes = data[nvs_part["offset"] : nvs_part["offset"] + nvs_part["size"]]
            nvs_info = parse_nvs_partition(nvs_bytes)
    else:
        image_type = "app_image"
        img_info = parse_esp32_image(data)
        if img_info:
            active_app_bytes = data[: img_info["total_size_bytes"]]
            active_app_image = img_info
        else:
            active_app_bytes = data
        active_app_desc = parse_app_desc(active_app_bytes)

    embedded = _extract_embedded_metadata(active_app_bytes) if active_app_bytes else None
    heuristics = detect_eolo_heuristics(active_app_bytes) if active_app_bytes else {
        "kind": "unknown",
        "demo_or_test": None,
        "model": None,
        "product_name": None,
        "candidate_envs": [],
        "version": None,
        "features": [],
    }
    fw_sha256 = active_app_image["image_sha256"] if active_app_image else (
        hashlib.sha256(active_app_bytes).hexdigest() if active_app_bytes else None
    )
    local_matches = match_local_firmware_backups(fw_sha256, backups_dir) if (fw_sha256 and backups_dir) else []

    model = (embedded or {}).get("model") or heuristics.get("model")
    product_name = heuristics.get("product_name")
    if not product_name and model:
        for m_id, p_name, _ in EOLO_MODELS:
            if m_id == model:
                product_name = p_name
                break

    env = (embedded or {}).get("env")
    if not env and local_matches:
        env = local_matches[0].get("target")
    if not env and len(heuristics.get("candidate_envs", [])) == 1:
        env = heuristics["candidate_envs"][0]

    version = (embedded or {}).get("version")
    version_source = "embedded" if version else None
    if not version and local_matches and local_matches[0].get("version_tag"):
        version = local_matches[0]["version_tag"]
        version_source = "local_backup"
    if not version and heuristics.get("version"):
        version = heuristics["version"]
        version_source = "heuristics"

    git_commit = (embedded or {}).get("git")
    if not git_commit and local_matches:
        git_info = local_matches[0].get("git") or {}
        if git_info.get("commit") and git_info["commit"] != "unknown":
            git_commit = f"{git_info['commit']}{'-dirty' if git_info.get('dirty') else ''}"

    summary = {
        "image_type": image_type,
        "source_path": source_path,
        "model": model,
        "product_name": product_name,
        "kind": heuristics.get("kind"),
        "demo_or_test": heuristics.get("demo_or_test"),
        "env": env,
        "candidate_envs": heuristics.get("candidate_envs", []),
        "version": version,
        "version_source": version_source,
        "git_commit": git_commit,
        "features": (embedded or {}).get("features") or heuristics.get("features", []),
        "active_partition": active_partition_label,
        "firmware_size_bytes": len(active_app_bytes) if active_app_bytes else 0,
        "firmware_sha256": fw_sha256,
        "firmware_sha256_valid": active_app_image.get("sha256_valid") if active_app_image else None,
        "sdk_version": active_app_desc.get("sdk_version") if active_app_desc else None,
        "idf_ver": active_app_desc.get("idf_ver") if active_app_desc else None,
        "chip": (chip_info or {}).get("chip"),
        "mac": (chip_info or {}).get("mac"),
        "flash_size": (chip_info or {}).get("flash_size") or (
            f"{len(data) // (1024 * 1024)}MB" if is_full_flash and len(data) % (1024 * 1024) == 0 else None
        ),
        "matched_local_backup": local_matches[0] if local_matches else None,
    }

    return {
        "schema_version": 1,
        "analyzed_at": datetime.datetime.now().isoformat(),
        "summary": summary,
        "chip_info": chip_info,
        "bootloader": bootloader_info,
        "partition_table": partition_table,
        "otadata": otadata_info,
        "app_slots": app_slots,
        "active_app_desc": active_app_desc,
        "embedded_metadata": embedded,
        "heuristics": heuristics,
        "local_backup_matches": local_matches,
        "nvs": nvs_info,
        "_extracted_blobs": {
            "bootloader": bootloader_bytes,
            "partitions": partitions_bytes,
            "firmware": active_app_bytes,
            "nvs": nvs_bytes,
        },
    }


def find_esptool_command() -> list[str]:
    """Encuentra el ejecutable o módulo esptool disponible en el host."""
    pio_python = Path.home() / ".platformio" / "penv" / "bin" / "python"
    pio_esptool = Path.home() / ".platformio" / "packages" / "tool-esptoolpy" / "esptool.py"
    if pio_python.exists() and pio_esptool.exists():
        return [str(pio_python), str(pio_esptool)]

    for exe in ("esptool.py", "esptool"):
        found = shutil.which(exe)
        if found:
            return [found]

    if importlib.util.find_spec("esptool") is not None:
        return [sys.executable, "-m", "esptool"]

    pio_cmd = shutil.which("pio") or shutil.which("platformio")
    if pio_cmd:
        return [pio_cmd, "pkg", "exec", "-p", "tool-esptoolpy", "--", "esptool.py"]

    raise RuntimeError(
        "No se encontró esptool. Instala PlatformIO o esptool (pip install esptool)."
    )


def _parse_size_to_bytes(size_str: str) -> int:
    text = size_str.strip().upper()
    if text.startswith("0X"):
        return int(text, 16)
    if text.endswith("MB"):
        return int(float(text[:-2]) * 1024 * 1024)
    if text.endswith("KB"):
        return int(float(text[:-2]) * 1024)
    if text.endswith("B"):
        return int(text[:-1])
    return int(text)


def parse_esptool_flash_id_output(text: str) -> dict:
    """Extrae chip, MAC, features y tamaño de flash desde la salida de esptool flash_id."""
    info: dict = {}
    if m := re.search(r"Chip is\s+(.+)", text):
        info["chip"] = m.group(1).strip()
    if m := re.search(r"Features:\s+(.+)", text):
        info["features"] = [x.strip() for x in m.group(1).split(",") if x.strip()]
    if m := re.search(r"Crystal is\s+(.+)", text):
        info["crystal"] = m.group(1).strip()
    if m := re.search(r"MAC:\s+([0-9a-fA-F:]{17})", text):
        info["mac"] = m.group(1).lower()
    if m := re.search(r"Manufacturer:\s+(.+)", text):
        info["manufacturer"] = m.group(1).strip()
    if m := re.search(r"Device:\s+(.+)", text):
        info["device"] = m.group(1).strip()
    if m := re.search(r"Detected flash size:\s+(\S+)", text):
        flash_size_str = m.group(1).strip()
        info["flash_size"] = flash_size_str
        try:
            info["flash_size_bytes"] = _parse_size_to_bytes(flash_size_str)
        except ValueError:
            pass
    return info


def _build_esptool_base_cmd(esptool_cmd: list[str], port: str | None, baud: int) -> list[str]:
    cmd = list(esptool_cmd) + ["--chip", "esp32"]
    if port and port.lower() != "auto":
        cmd.extend(["--port", port])
    cmd.extend(["--baud", str(baud)])
    return cmd


def _write_restore_scripts(dest_dir: Path, summary: dict, has_full: bool, has_bl: bool, has_pt: bool, has_fw: bool, active_offset: int) -> None:
    label = summary.get("model") or summary.get("env") or "EOLO"
    ver = summary.get("version") or "desconocida"
    esptool_detect = """ESPTOOL_CMD=""
if [ -x "$HOME/.platformio/penv/bin/python" ] && [ -f "$HOME/.platformio/packages/tool-esptoolpy/esptool.py" ]; then
    ESPTOOL_CMD="$HOME/.platformio/penv/bin/python $HOME/.platformio/packages/tool-esptoolpy/esptool.py"
elif command -v esptool.py &> /dev/null; then
    ESPTOOL_CMD="esptool.py"
elif python3 -m esptool version &> /dev/null 2>&1; then
    ESPTOOL_CMD="python3 -m esptool"
elif command -v pio &> /dev/null; then
    ESPTOOL_CMD="pio pkg exec -p tool-esptoolpy -- esptool.py"
else
    echo "Error: no se encontró esptool.py ni PlatformIO."
    exit 1
fi"""

    if has_full:
        sh_full = f"""#!/usr/bin/env bash
# Restauración completa de flash para {label} (versión {ver})
set -e

PORT="${{1:-/dev/ttyUSB0}}"
BAUD="${{2:-921600}}"
SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"

{esptool_detect}

echo "Restaurando flash completa de {label} (v{ver}) en $PORT..."
$ESPTOOL_CMD --chip esp32 --port "$PORT" --baud "$BAUD" \\
    --before default_reset --after hard_reset write_flash -z \\
    0x0 "$SCRIPT_DIR/full_flash.bin"
"""
        full_sh = dest_dir / "restore_full_flash.sh"
        full_sh.write_text(sh_full, encoding="utf-8")
        full_sh.chmod(0o755)

        bat_full = f"""@echo off
rem Restauración completa de flash para {label} (versión {ver})
set PORT=%~1
if "%PORT%"=="" set PORT=COM3
set BAUD=%~2
if "%BAUD%"=="" set BAUD=921600

esptool.py --chip esp32 --port %PORT% --baud %BAUD% ^
    --before default_reset --after hard_reset write_flash -z ^
    0x0 "%~dp0full_flash.bin"
"""
        (dest_dir / "restore_full_flash.bat").write_text(bat_full, encoding="utf-8")

    if has_fw:
        parts_args = []
        if has_bl:
            parts_args.append(f'0x1000 "$SCRIPT_DIR/bootloader.bin"')
        if has_pt:
            parts_args.append(f'0x8000 "$SCRIPT_DIR/partitions.bin"')
        parts_args.append(f'{hex(active_offset)} "$SCRIPT_DIR/firmware.bin"')
        joined = " \\\n    ".join(parts_args)
        sh_app = f"""#!/usr/bin/env bash
# Restauración de aplicación/arranque sin sobrescribir NVS para {label} (versión {ver})
set -e

PORT="${{1:-/dev/ttyUSB0}}"
BAUD="${{2:-921600}}"
SCRIPT_DIR="$(cd "$(dirname "${{BASH_SOURCE[0]}}")" && pwd)"

{esptool_detect}

echo "Restaurando firmware de {label} (v{ver}) en $PORT (conservando NVS)..."
$ESPTOOL_CMD --chip esp32 --port "$PORT" --baud "$BAUD" \\
    --before default_reset --after hard_reset write_flash -z \\
    {joined}
"""
        app_sh = dest_dir / "restore_app_only.sh"
        app_sh.write_text(sh_app, encoding="utf-8")
        app_sh.chmod(0o755)


def export_backup_bundle(
    dest_dir: Path,
    analysis: dict,
    full_flash_bytes: bytes | None = None,
) -> dict:
    """Guarda los binarios extraídos, metadata.json y scripts de restauración."""
    dest_dir.mkdir(parents=True, exist_ok=True)
    blobs = analysis.get("_extracted_blobs", {})
    files_manifest = {}

    if full_flash_bytes:
        full_path = dest_dir / "full_flash.bin"
        full_path.write_bytes(full_flash_bytes)
        files_manifest["full_flash"] = {
            "file": "full_flash.bin",
            "offset": "0x0",
            "size_bytes": len(full_flash_bytes),
            "sha256": hashlib.sha256(full_flash_bytes).hexdigest(),
        }

    for key, filename, offset_hex in (
        ("bootloader", "bootloader.bin", "0x1000"),
        ("partitions", "partitions.bin", "0x8000"),
        ("firmware", "firmware.bin", hex((analysis.get("otadata") or {}).get("active_offset") or DEFAULT_APP_OFFSET)),
        ("nvs", "nvs.bin", "0x9000"),
    ):
        blob = blobs.get(key) or b""
        if not blob:
            continue
        out_path = dest_dir / filename
        out_path.write_bytes(blob)
        files_manifest[key] = {
            "file": filename,
            "offset": offset_hex,
            "size_bytes": len(blob),
            "sha256": hashlib.sha256(blob).hexdigest(),
        }

    active_offset = (analysis.get("otadata") or {}).get("active_offset") or DEFAULT_APP_OFFSET
    _write_restore_scripts(
        dest_dir,
        analysis.get("summary", {}),
        has_full=bool(full_flash_bytes),
        has_bl=bool(blobs.get("bootloader")),
        has_pt=bool(blobs.get("partitions")),
        has_fw=bool(blobs.get("firmware")),
        active_offset=active_offset,
    )

    clean_meta = {k: v for k, v in analysis.items() if k != "_extracted_blobs"}
    clean_meta["files"] = files_manifest
    (dest_dir / "metadata.json").write_text(
        json.dumps(clean_meta, indent=2, ensure_ascii=False),
        encoding="utf-8",
    )
    return clean_meta


def backup_from_device(
    port: str | None = None,
    baud: int = 921600,
    size: str = "auto",
    out_dir: Path | None = None,
    name: str = "",
    app_only: bool = False,
    backups_dir: Path | None = DEFAULT_BACKUPS_DIR,
) -> tuple[dict, Path]:
    """Lee la flash del dispositivo ESP32 conectado, extrae metadatos y guarda respaldo."""
    esptool_cmd = find_esptool_command()

    flash_id_cmd = _build_esptool_base_cmd(esptool_cmd, port, min(baud, 460800)) + ["flash_id"]
    res_id = subprocess.run(flash_id_cmd, capture_output=True, text=True, check=False)
    if res_id.returncode != 0:
        raise RuntimeError(f"Error ejecutando esptool flash_id:\n{res_id.stdout}\n{res_id.stderr}")

    chip_info = parse_esptool_flash_id_output(res_id.stdout + "\n" + res_id.stderr)
    if size.lower() == "auto":
        flash_size_bytes = chip_info.get("flash_size_bytes", 4 * 1024 * 1024)
    else:
        flash_size_bytes = _parse_size_to_bytes(size)

    import tempfile

    with tempfile.TemporaryDirectory() as tmp_dir_str:
        tmp_dir = Path(tmp_dir_str)
        if not app_only:
            dump_path = tmp_dir / "full_flash.bin"
            read_cmd = _build_esptool_base_cmd(esptool_cmd, port, baud) + [
                "read_flash",
                "0x0",
                hex(flash_size_bytes),
                str(dump_path),
            ]
            subprocess.check_call(read_cmd)
            full_bytes = dump_path.read_bytes()
            analysis = analyze_binary(
                full_bytes,
                source_path=str(dump_path),
                backups_dir=backups_dir,
                chip_info=chip_info,
            )
            extracted_full = full_bytes
        else:
            pt_path = tmp_dir / "partitions.bin"
            ota_path = tmp_dir / "otadata.bin"
            bl_path = tmp_dir / "bootloader.bin"
            app_path = tmp_dir / "firmware.bin"

            subprocess.check_call(
                _build_esptool_base_cmd(esptool_cmd, port, baud)
                + ["read_flash", hex(PARTITION_TABLE_OFFSET), hex(PARTITION_TABLE_SIZE), str(pt_path)]
            )
            pt = parse_partition_table(pt_path.read_bytes())
            app_parts = [p for p in pt["partitions"] if p["type"] == "app"]
            ota_part = next((p for p in pt["partitions"] if p["type"] == "data" and p["subtype"] == "ota"), None)
            ota_bytes = b""
            if ota_part:
                subprocess.check_call(
                    _build_esptool_base_cmd(esptool_cmd, port, baud)
                    + ["read_flash", hex(ota_part["offset"]), hex(ota_part["size"]), str(ota_path)]
                )
                ota_bytes = ota_path.read_bytes()
            ota_info = parse_otadata(ota_bytes, app_parts)
            active_off = ota_info.get("active_offset") or DEFAULT_APP_OFFSET
            active_part = next((p for p in app_parts if p["offset"] == active_off), None)
            read_len = active_part["size"] if active_part else 0x1E0000

            subprocess.check_call(
                _build_esptool_base_cmd(esptool_cmd, port, baud)
                + ["read_flash", hex(BOOTLOADER_OFFSET), hex(PARTITION_TABLE_OFFSET - BOOTLOADER_OFFSET), str(bl_path)]
            )
            subprocess.check_call(
                _build_esptool_base_cmd(esptool_cmd, port, baud)
                + ["read_flash", hex(active_off), hex(read_len), str(app_path)]
            )
            raw_app = app_path.read_bytes()
            analysis = analyze_binary(raw_app, source_path="device_app_only", backups_dir=backups_dir, chip_info=chip_info)
            bl_parsed = parse_esp32_image(bl_path.read_bytes())
            bl_bytes = bl_path.read_bytes()[: bl_parsed["total_size_bytes"]] if bl_parsed else bl_path.read_bytes()
            analysis["partition_table"] = pt
            analysis["otadata"] = ota_info
            analysis["summary"]["active_partition"] = ota_info.get("active_label")
            analysis["_extracted_blobs"]["bootloader"] = bl_bytes
            analysis["_extracted_blobs"]["partitions"] = pt_path.read_bytes()
            extracted_full = None

        if out_dir is None:
            stamp = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
            model_slug = sanitize_name(analysis["summary"].get("env") or analysis["summary"].get("model") or "")
            mac_slug = sanitize_name((chip_info.get("mac") or "").replace(":", "")[-6:])
            tag_slug = sanitize_name(name)
            parts = [p for p in (tag_slug, model_slug or f"esp32_{mac_slug or 'device'}", stamp) if p]
            out_dir = DEFAULT_FLASH_BACKUPS_DIR / "_".join(parts)

        clean_meta = export_backup_bundle(out_dir, analysis, full_flash_bytes=extracted_full)
        return clean_meta, out_dir


def print_human_report(meta: dict, out_dir: Path | None = None) -> None:
    """Imprime un resumen legible de la metadata extraída."""
    s = meta.get("summary", {})
    print("\n" + "=" * 68)
    print(" EOLO - Respaldo y Metadata de Firmware")
    print("=" * 68)
    print(f" Tipo de imagen   : {s.get('image_type')}")
    if s.get("model"):
        prod = f" ({s['product_name']})" if s.get("product_name") else ""
        print(f" Modelo EOLO      : {s['model']}{prod}")
    elif s.get("demo_or_test"):
        print(f" Tipo / Demo      : {s.get('kind')} ({s.get('demo_or_test')})")
    else:
        print(" Modelo EOLO      : No identificado")

    if s.get("env"):
        print(f" Entorno PlatformIO: {s['env']}")
    elif s.get("candidate_envs"):
        print(f" Entornos posibles: {', '.join(s['candidate_envs'])}")

    ver = s.get("version") or "desconocida"
    ver_src = f" [{s['version_source']}]" if s.get("version_source") else ""
    print(f" Versión EOLO     : {ver}{ver_src}")
    if s.get("git_commit"):
        print(f" Commit Git       : {s['git_commit']}")
    if s.get("features"):
        print(f" Capacidades      : {', '.join(s['features'])}")
    if s.get("active_partition"):
        print(f" Partición activa : {s['active_partition']}")

    fw_kb = (s.get("firmware_size_bytes") or 0) / 1024.0
    sha_status = "OK" if s.get("firmware_sha256_valid") is True else (
        "INVÁLIDO" if s.get("firmware_sha256_valid") is False else "sin hash"
    )
    print(f" Firmware         : {fw_kb:.1f} KB | SHA-256 ({sha_status}): {s.get('firmware_sha256')}")
    if s.get("sdk_version") or s.get("idf_ver"):
        print(f" Core SDK/IDF     : {s.get('sdk_version') or '-'} (IDF {s.get('idf_ver') or '-'})")

    if s.get("chip") or s.get("mac"):
        print(f" Hardware ESP32   : {s.get('chip') or '-'} | MAC: {s.get('mac') or '-'} | Flash: {s.get('flash_size') or '-'}")

    if s.get("matched_local_backup"):
        m = s["matched_local_backup"]
        print(f" Match local      : {m.get('target')} | tag={m.get('version_tag')} | {m.get('created_at')}")

    pt = meta.get("partition_table")
    if pt and pt.get("partitions"):
        print(f" Particiones ({len(pt['partitions'])})  : " + ", ".join(
            f"{p['label']}@{p['offset_hex']}" for p in pt["partitions"]
        ))

    nvs = meta.get("nvs")
    if nvs and nvs.get("namespaces"):
        print(f" NVS namespaces   : {', '.join(nvs['namespaces'])}")

    if out_dir is not None:
        print(f" Guardado en      : {out_dir}")
    print("=" * 68 + "\n")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Respaldar el firmware instalado en un EOLO y extraer su metadata.",
    )
    parser.add_argument("--from-file", default="", help="Analizar un archivo .bin existente sin conectar placa")
    parser.add_argument("--port", default="", help="Puerto serie del ESP32 (ej. /dev/ttyUSB0, COM3). Por defecto auto")
    parser.add_argument("--baud", type=int, default=921600, help="Baudrate para lectura flash (defecto: 921600)")
    parser.add_argument("--size", default="auto", help="Tamaño de flash a leer (auto, 4MB, 8MB, 0x400000)")
    parser.add_argument("--out", default="", help="Directorio de salida para el respaldo")
    parser.add_argument("--name", default="", help="Etiqueta opcional para el nombre de la carpeta de respaldo")
    parser.add_argument("--app-only", action="store_true", help="Leer solo bootloader, particiones, otadata y app activa")
    parser.add_argument("--extract", action="store_true", help="Con --from-file, extraer particiones y scripts en --out")
    parser.add_argument("--json", action="store_true", help="Imprimir metadata completa en formato JSON")
    parser.add_argument("--no-local-match", action="store_true", help="No buscar coincidencias SHA-256 en firmware_backups/")
    return parser.parse_args(argv)


def main(argv=None) -> int:
    args = parse_args(argv)
    backups_dir = None if args.no_local_match else DEFAULT_BACKUPS_DIR

    if args.from_file:
        src = Path(args.from_file)
        if not src.is_file():
            print(f"Error: no existe el archivo {src}", file=sys.stderr)
            return 1
        raw = src.read_bytes()
        analysis = analyze_binary(raw, source_path=str(src), backups_dir=backups_dir)

        out_dir = None
        if args.out or args.extract:
            if args.out:
                out_dir = Path(args.out)
            else:
                out_dir = src.parent / f"{src.stem}_extracted"
            is_full = analysis["summary"]["image_type"] == "full_flash"
            meta = export_backup_bundle(out_dir, analysis, full_flash_bytes=raw if is_full else None)
        else:
            meta = {k: v for k, v in analysis.items() if k != "_extracted_blobs"}

        if args.json:
            print(json.dumps(meta, indent=2, ensure_ascii=False))
        else:
            print_human_report(meta, out_dir)
        return 0

    try:
        out_dir = Path(args.out) if args.out else None
        meta, saved_dir = backup_from_device(
            port=args.port or None,
            baud=args.baud,
            size=args.size,
            out_dir=out_dir,
            name=args.name,
            app_only=args.app_only,
            backups_dir=backups_dir,
        )
    except Exception as exc:
        print(f"Error durante el respaldo: {exc}", file=sys.stderr)
        return 1

    if args.json:
        print(json.dumps(meta, indent=2, ensure_ascii=False))
    else:
        print_human_report(meta, saved_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
