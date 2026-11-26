import hashlib
import json
import struct
import subprocess
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

import device_firmware_backup as dfb


def _build_esp32_image(segments: list[tuple[int, bytes]], append_sha256: bool = True, corrupt_sha: bool = False) -> bytes:
    header = bytearray(24)
    header[0] = 0xE9
    header[1] = len(segments)
    header[2] = 0x00  # qio
    header[3] = 0x2F  # 4MB + 80MHz
    struct.pack_into("<I", header, 4, 0x400805F0)
    header[23] = 1 if append_sha256 else 0

    body = bytearray(header)
    for load_addr, payload in segments:
        body.extend(struct.pack("<II", load_addr, len(payload)))
        body.extend(payload)

    pad_len = ((len(body) + 16) // 16) * 16 - len(body)
    body.extend(b"\x00" * pad_len)

    if append_sha256:
        digest = hashlib.sha256(body).digest()
        if corrupt_sha:
            digest = bytes([digest[0] ^ 0xFF]) + digest[1:]
        body.extend(digest)

    return bytes(body)


def _build_app_desc(version: str = "esp-idf: v4.4.7", project: str = "arduino-lib-builder", idf_ver: str = "v4.4.7") -> bytes:
    desc = bytearray(256)
    struct.pack_into("<I", desc, 0, 0xABCD5432)
    struct.pack_into("<I", desc, 4, 0)
    desc[16 : 16 + len(version)] = version.encode()
    desc[48 : 48 + len(project)] = project.encode()
    desc[80 : 80 + 8] = b"12:00:00"
    desc[96 : 96 + 11] = b"Mar  5 2024"
    desc[112 : 112 + len(idf_ver)] = idf_ver.encode()
    desc[144:176] = bytes(range(32))
    return bytes(desc)


def _build_partition_table(entries: list[tuple[int, int, int, int, str]], include_md5: bool = True, corrupt_md5: bool = False) -> bytes:
    buf = bytearray()
    for ptype, subtype, poff, psize, label in entries:
        entry = bytearray(32)
        struct.pack_into("<HBBII", entry, 0, 0x50AA, ptype, subtype, poff, psize)
        raw_label = label.encode("utf-8")[:16]
        entry[12 : 12 + len(raw_label)] = raw_label
        buf.extend(entry)
    if include_md5:
        md5_entry = bytearray(32)
        struct.pack_into("<H", md5_entry, 0, 0xEBEB)
        md5_entry[2:16] = b"\xff" * 14
        digest = hashlib.md5(buf).digest()
        if corrupt_md5:
            digest = bytes([digest[0] ^ 0xAA]) + digest[1:]
        md5_entry[16:32] = digest
        buf.extend(md5_entry)
    while len(buf) < 0x1000:
        buf.extend(b"\xff" * 32)
    return bytes(buf[:0x1000])


def _build_otadata(slot0: tuple[int, int] | None = None, slot1: tuple[int, int] | None = None, corrupt_slot0_crc: bool = False) -> bytes:
    data = bytearray(b"\xff" * 0x2000)
    for idx, spec in ((0, slot0), (1, slot1)):
        if spec is None:
            continue
        seq, state = spec
        crc = dfb._esp_crc32_le_uint32(seq)
        if idx == 0 and corrupt_slot0_crc:
            crc ^= 0x12345678
        struct.pack_into("<I20sII", data, idx * 4096, seq, b"\xff" * 20, state, crc)
    return bytes(data)


def _build_nvs_page(ns_map: dict[int, str], items: list[tuple[int, int, str, bytes]]) -> bytes:
    page = bytearray(b"\xff" * 4096)
    struct.pack_into("<II", page, 0, 0xFFFFFFFE, 1)  # ACTIVE
    bitmap = bytearray(b"\xff" * 32)
    idx = 0

    def _write_entry(ns_idx: int, etype: int, span: int, key: str, raw8: bytes, extra: bytes = b""):
        nonlocal idx
        b_idx = idx // 4
        shift = (idx % 4) * 2
        bitmap[b_idx] = (bitmap[b_idx] & ~(0x3 << shift)) | (0x2 << shift)
        off = 64 + idx * 32
        struct.pack_into("<BBBBI", page, off, ns_idx, etype, span, 0xFF, 0)
        kb = key.encode("utf-8")[:15]
        page[off + 8 : off + 8 + len(kb)] = kb
        page[off + 8 + len(kb)] = 0
        page[off + 24 : off + 32] = raw8.ljust(8, b"\x00")[:8]
        if extra:
            page[off + 32 : off + 32 + len(extra)] = extra
        idx += span

    for ns_id, ns_name in ns_map.items():
        _write_entry(0, 0x01, 1, ns_name, bytes([ns_id]))

    for ns_id, etype, key, val_bytes in items:
        if etype == 0x21:  # string
            slen = len(val_bytes)
            span = 1 + ((slen + 31) // 32)
            raw8 = struct.pack("<HHI", slen, 0, 0)
            _write_entry(ns_id, etype, span, key, raw8, val_bytes)
        elif etype == 0x42:  # blob
            raw8 = struct.pack("<HHI", len(val_bytes), 0, 0)
            _write_entry(ns_id, etype, 1, key, raw8)
        else:
            _write_entry(ns_id, etype, 1, key, val_bytes)

    page[32:64] = bitmap
    return bytes(page)


class DeviceFirmwareBackupTests(unittest.TestCase):
    def test_parse_esp32_image_valid_and_corrupt_sha(self):
        img = _build_esp32_image([(0x3F400020, _build_app_desc() + b"\x00EOLO Express\x00")])
        parsed = dfb.parse_esp32_image(img)
        self.assertIsNotNone(parsed)
        self.assertTrue(parsed["sha256_valid"])
        self.assertEqual(parsed["total_size_bytes"], len(img))

        bad = _build_esp32_image([(0x3F400020, _build_app_desc())], corrupt_sha=True)
        parsed_bad = dfb.parse_esp32_image(bad)
        self.assertIsNotNone(parsed_bad)
        self.assertFalse(parsed_bad["sha256_valid"])

    def test_partition_table_md5_and_otadata_selection(self):
        pt_bytes = _build_partition_table([
            (0x01, 0x02, 0x9000, 0x5000, "nvs"),
            (0x01, 0x00, 0xE000, 0x2000, "otadata"),
            (0x00, 0x10, 0x10000, 0x20000, "app0"),
            (0x00, 0x11, 0x30000, 0x20000, "app1"),
        ])
        pt = dfb.parse_partition_table(pt_bytes)
        self.assertTrue(pt["valid"])
        self.assertTrue(pt["md5_present"])
        self.assertTrue(pt["md5_valid"])

        pt_bad = dfb.parse_partition_table(
            _build_partition_table([(0x00, 0x10, 0x10000, 0x20000, "app0")], corrupt_md5=True)
        )
        self.assertFalse(pt_bad["valid"])
        self.assertFalse(pt_bad["md5_valid"])

        app_parts = [p for p in pt["partitions"] if p["type"] == "app"]
        # Unwritten -> app0
        self.assertEqual(dfb.parse_otadata(_build_otadata(), app_parts)["active_label"], "app0")
        # seq 2 > seq 1 -> app1 (index (2-1)%2 = 1)
        self.assertEqual(dfb.parse_otadata(_build_otadata((1, 2), (2, 2)), app_parts)["active_label"], "app1")
        # seq 3 > seq 2 -> app0 (index (3-1)%2 = 0)
        self.assertEqual(dfb.parse_otadata(_build_otadata((3, 2), (2, 2)), app_parts)["active_label"], "app0")
        # corrupt slot0 falls back to slot1
        self.assertEqual(
            dfb.parse_otadata(_build_otadata((3, 2), (2, 2), corrupt_slot0_crc=True), app_parts)["active_label"],
            "app1",
        )

    def test_nvs_redacts_sensitive_keys_and_summarizes_blobs(self):
        nvs_page = _build_nvs_page(
            {1: "eolo_wifi", 2: "eolo_calib"},
            [
                (1, 0x21, "sta_ssid", b"MiRed\x00"),
                (1, 0x21, "sta_pass", b"supersecreto123\x00"),
                (1, 0x21, "api_token", b"tok_999\x00"),
                (2, 0x04, "afmDiv", struct.pack("<I", 10)),
                (2, 0x42, "pumpOrder", b"\x01\x02\x03\x04"),
            ],
        )
        parsed = dfb.parse_nvs_partition(nvs_page)
        self.assertEqual(parsed["entries"]["eolo_wifi"]["sta_ssid"], "MiRed")
        self.assertEqual(parsed["entries"]["eolo_wifi"]["sta_pass"]["value"], "<redacted>")
        self.assertEqual(parsed["entries"]["eolo_wifi"]["api_token"]["value"], "<redacted>")
        self.assertEqual(parsed["entries"]["eolo_calib"]["afmDiv"], 10)
        self.assertEqual(parsed["entries"]["eolo_calib"]["pumpOrder"], {"type": "blob", "size_bytes": 4})

    def test_model_disambiguation_features_and_version_safety(self):
        cases = [
            (b"\x00EOLO Express FE\x00FS3000 inicializado\x00Iniciando pantalla...\x00", "EOLO Express FE", "eolo_express_legacy"),
            (b"\x00EOLO Express\x00RS485 AFM07\x00PlantowerTask\x00Iniciando pantalla...\x00", "EOLO Express", "eolo_express"),
            (b"\x00EOLO Standard\x00Anemometro\x00api-sensores.cmasccp.cl\x00", "EOLO Standard", "eolo_standard"),
            (b"\x00EOLO Dron Legacy\x00EOLO Dron\x00HeadlessSetupServer\x00STATUS_LED_LOW_POWER\x00", "EOLO Dron Legacy", "eolo_dron_legacy_low_power"),
            (b"\x00EOLO Dron\x00Iniciando pantalla...\x00RS485 AFM07\x00", "EOLO Dron", "eolo_dron"),
        ]
        for payload, exp_model, exp_env in cases:
            h = dfb.detect_eolo_heuristics(payload)
            self.assertEqual(h["model"], exp_model)
            self.assertIn(exp_env, h["candidate_envs"])

        # Pre-v1.2.0 firmware sin marcador de version UI no debe reportar version del SDK/IDF
        pre_fw = _build_esp32_image([(0x3F400020, _build_app_desc("esp-idf: v4.4.7", idf_ver="v4.4.7") + b"\x00EOLO Express\x004.4.7\x00")])
        res_pre = dfb.analyze_binary(pre_fw, backups_dir=None)
        self.assertIsNone(res_pre["summary"]["version"])
        self.assertEqual(res_pre["summary"]["sdk_version"], "esp-idf: v4.4.7")

        # Firmware con marcador UI v%s y 1.2.0 cercano sí infiere 1.2.0
        v120_fw = _build_esp32_image([(0x3F400020, _build_app_desc() + b"\x00EOLO Express\x001.2.0\x00v%s\x00")])
        res_v120 = dfb.analyze_binary(v120_fw, backups_dir=None)
        self.assertEqual(res_v120["summary"]["version"], "1.2.0")
        self.assertEqual(res_v120["summary"]["version_source"], "heuristics")

    def test_local_backup_matching_latest_and_history(self):
        fw_bytes = _build_esp32_image([(0x3F400020, _build_app_desc() + b"\x00EOLO Standard\x00")])
        fw_sha = hashlib.sha256(fw_bytes).hexdigest()

        with tempfile.TemporaryDirectory() as tmp:
            bdir = Path(tmp) / "firmware_backups"
            latest = bdir / "devices" / "eolo_standard" / "latest"
            latest.mkdir(parents=True)
            (latest / "build_info.json").write_text(json.dumps({
                "target": "eolo_standard",
                "label": "EoloStandard",
                "version_tag": "v1.2.0_ok",
                "created_at": "2026-11-20 16:00:00",
                "git": {"commit": "8f8dc01", "dirty": False},
                "files": {"firmware": {"sha256": fw_sha}},
            }), encoding="utf-8")

            hist_dir = bdir / "devices" / "eolo_express" / "history"
            hist_dir.mkdir(parents=True)
            snap_src = Path(tmp) / "snap"
            snap_src.mkdir()
            (snap_src / "build_info.json").write_text(json.dumps({
                "target": "eolo_express",
                "label": "EoloExpress",
                "version_tag": "hist_r1",
                "created_at": "2026-11-19 10:00:00",
                "files": {"firmware": {"sha256": fw_sha}},
            }), encoding="utf-8")
            with tarfile.open(hist_dir / "hist_r1.tar.xz", "w:xz") as tar:
                tar.add(snap_src, arcname="hist_r1")

            res = dfb.analyze_binary(fw_bytes, backups_dir=bdir)
            self.assertEqual(len(res["local_backup_matches"]), 2)
            self.assertEqual(res["summary"]["version"], "v1.2.0_ok")
            self.assertEqual(res["summary"]["version_source"], "local_backup")

    def test_parse_esptool_flash_id_and_mock_device_backup(self):
        flash_id_txt = """
esptool.py v4.5.1
Detecting chip type... ESP32
Chip is ESP32-D0WDQ6 (revision v1.0)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
MAC: 08:3a:f2:b6:f1:e0
Manufacturer: ef
Device: 4016
Detected flash size: 4MB
"""
        chip = dfb.parse_esptool_flash_id_output(flash_id_txt)
        self.assertEqual(chip["chip"], "ESP32-D0WDQ6 (revision v1.0)")
        self.assertEqual(chip["mac"], "08:3a:f2:b6:f1:e0")
        self.assertEqual(chip["flash_size"], "4MB")
        self.assertEqual(chip["flash_size_bytes"], 4 * 1024 * 1024)

        # Construir un dump sintético de 512KB con bootloader, partitions, otadata, nvs y app0
        flash = bytearray(b"\xff" * 0x80000)
        bl = _build_esp32_image([(0x40078000, b"\x00" * 64)])
        pt = _build_partition_table([
            (0x01, 0x02, 0x9000, 0x5000, "nvs"),
            (0x01, 0x00, 0xE000, 0x2000, "otadata"),
            (0x00, 0x10, 0x10000, 0x40000, "app0"),
        ])
        ota = _build_otadata((1, 2), None)
        nvs = _build_nvs_page({1: "eolo_session"}, [(1, 0x04, "bootCount", struct.pack("<I", 7))])
        app0 = _build_esp32_image([(0x3F400020, _build_app_desc() + b"\x00EOLO Express\x001.2.0\x00v%s\x00")])

        flash[0x1000 : 0x1000 + len(bl)] = bl
        flash[0x8000 : 0x8000 + len(pt)] = pt
        flash[0x9000 : 0x9000 + len(nvs)] = nvs
        flash[0xE000 : 0xE000 + len(ota)] = ota
        flash[0x10000 : 0x10000 + len(app0)] = app0

        def fake_run(cmd, **_kwargs):
            return subprocess.CompletedProcess(cmd, 0, stdout=flash_id_txt, stderr="")

        def fake_check_call(cmd, **_kwargs):
            out_file = Path(cmd[-1])
            out_file.write_bytes(bytes(flash))
            return 0

        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp) / "backup_out"
            with mock.patch.object(dfb, "find_esptool_command", return_value=["esptool.py"]), \
                 mock.patch.object(dfb.subprocess, "run", side_effect=fake_run), \
                 mock.patch.object(dfb.subprocess, "check_call", side_effect=fake_check_call):
                meta, saved = dfb.backup_from_device(port="/dev/ttyUSB0", size="512KB", out_dir=out_dir, backups_dir=None)

            self.assertEqual(saved, out_dir)
            self.assertEqual(meta["summary"]["model"], "EOLO Express")
            self.assertEqual(meta["summary"]["version"], "1.2.0")
            self.assertEqual(meta["summary"]["mac"], "08:3a:f2:b6:f1:e0")
            self.assertEqual(meta["nvs"]["entries"]["eolo_session"]["bootCount"], 7)
            for fname in ("full_flash.bin", "bootloader.bin", "partitions.bin", "firmware.bin", "nvs.bin",
                          "metadata.json", "restore_full_flash.sh", "restore_full_flash.bat", "restore_app_only.sh"):
                self.assertTrue((out_dir / fname).is_file(), f"Falta {fname}")

    def test_eolo_cli_fw_info_json(self):
        fw = _build_esp32_image([(0x3F400020, _build_app_desc() + b"\x00EOLO Dron\x001.2.0\x00v%s\x00")])
        with tempfile.TemporaryDirectory() as tmp:
            fw_path = Path(tmp) / "test_fw.bin"
            fw_path.write_bytes(fw)
            proc = subprocess.run(
                [str(PROJECT_ROOT / "eolo"), "fw-info", str(fw_path), "--json", "--no-local-match"],
                capture_output=True,
                text=True,
                check=False,
            )
            self.assertEqual(proc.returncode, 0, proc.stderr)
            parsed = json.loads(proc.stdout)
            self.assertEqual(parsed["summary"]["model"], "EOLO Dron")
            self.assertEqual(parsed["summary"]["version"], "1.2.0")


if __name__ == "__main__":
    unittest.main()
