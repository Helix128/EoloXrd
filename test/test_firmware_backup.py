import json
import sys
import tempfile
import unittest
from contextlib import ExitStack
from pathlib import Path
from unittest import mock


PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

import firmware_backup as fb


class FirmwareBackupTests(unittest.TestCase):
    def sandbox(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name)
        stack = ExitStack()
        self.addCleanup(stack.close)
        stack.enter_context(mock.patch.object(fb, "PROJECT_ROOT", root))
        stack.enter_context(mock.patch.object(fb, "BACKUPS_DIR", root / "firmware_backups"))
        stack.enter_context(mock.patch.object(fb, "DEVICES_BACKUP_DIR", root / "firmware_backups" / "devices"))
        stack.enter_context(mock.patch.object(fb, "DEMOS_BACKUP_DIR", root / "firmware_backups" / "demos"))
        stack.enter_context(mock.patch.object(fb, "RELEASES_BACKUP_DIR", root / "firmware_backups" / "releases"))
        stack.enter_context(mock.patch.object(fb, "OTHER_BACKUP_DIR", root / "firmware_backups" / "other"))
        stack.enter_context(mock.patch.object(fb, "BUILD_TAG_FILE", root / ".build_tag"))
        return root

    def make_build(self, root):
        build_dir = root / "build"
        build_dir.mkdir()
        for name in ("firmware.bin", "bootloader.bin", "partitions.bin", "firmware.elf", "firmware.map"):
            (build_dir / name).write_bytes(("data:" + name).encode())
        return build_dir

    def test_name_sanitization_handles_spaces_slashes_and_special_characters(self):
        self.assertEqual(fb.sanitize_name("  Release Final/AFM@07  "), "Release_Final_AFM_07")
        self.assertEqual(fb.sanitize_name("///@@@"), "")
        self.assertEqual(fb.sanitize_name("..."), "")

    def test_named_backup_keeps_only_latest_with_flash_artifacts(self):
        root = self.sandbox()
        build_dir = self.make_build(root)
        with mock.patch.object(
            fb,
            "get_git_info",
            return_value={"commit": "abc1234", "branch": "main", "dirty": False, "tag": ""},
        ):
            dest = fb.create_backup(
                build_dir,
                "eolo_dron",
                root,
                custom_tag="Release especial/AFM@07",
            )

        self.assertEqual(dest.name, "latest")
        latest_files = {
            "EoloDron_latest_firmware.bin",
            "EoloDron_latest_bootloader.bin",
            "EoloDron_latest_partitions.bin",
            "build_info.json",
            "flash_firmware.sh",
            "flash_firmware.bat",
        }
        self.assertEqual({path.name for path in dest.iterdir()}, latest_files)
        metadata = json.loads((dest / "build_info.json").read_text(encoding="utf-8"))
        self.assertEqual(metadata["version_tag"], "Release_especial_AFM_07")
        for artifact in metadata["files"].values():
            self.assertIn("sha256", artifact)
            self.assertTrue((dest / artifact["file"]).is_file())
        script = (dest / "flash_firmware.sh").read_text(encoding="utf-8")
        self.assertIn('$SCRIPT_DIR/EoloDron_latest_firmware.bin', script)
        self.assertEqual(metadata["source_snapshot"], "Release_especial_AFM_07_" + metadata["timestamp"][:10] + "_" + metadata["timestamp"][11:19].replace(":", "-"))

    def test_daily_revision_uses_retained_metadata(self):
        root = self.sandbox()
        target = root / "target"
        latest = target / "latest"
        latest.mkdir(parents=True)
        (latest / "build_info.json").write_text(json.dumps({
            "timestamp": "2026-08-25T11:00:00", "version_tag": "gabc_r8"
        }), encoding="utf-8")
        self.assertEqual(fb.get_daily_revision(target, "2026-08-25"), 9)

    def test_rotation_keeps_nine_compressed_histories_without_touching_legacy(self):
        root = self.sandbox()
        target = root / "firmware_backups" / "devices" / "eolo_dron"
        for index in range(1, 11):
            stamp = f"2026-08-25_12-00-{index:02d}"
            snapshot = target / f"build_r{index}_{stamp}"
            snapshot.mkdir(parents=True)
            (snapshot / "firmware.bin").write_bytes(f"firmware-{index}".encode())
            metadata = {
                "target": "eolo_dron", "label": "EoloDron", "timestamp": f"2026-08-25T12:00:{index:02d}",
                "version_tag": f"build_r{index}", "files": {"firmware": {"file": "firmware.bin"}}
            }
            (snapshot / "build_info.json").write_text(json.dumps(metadata), encoding="utf-8")
            fb._archive_directory(snapshot, target / "history", snapshot.name)
        fb.update_latest_folder(target, target / "build_r10_2026-08-25_12-00-10", "EoloDron",
                                json.loads((target / "build_r10_2026-08-25_12-00-10" / "build_info.json").read_text()),
                                "firmware.bin", "bootloader.bin", "partitions.bin")
        fb.rotate_target_backups(target)
        archives = sorted((target / "history").glob("*.tar.xz"))
        self.assertEqual(len(archives), 9)
        self.assertTrue((target / "build_r1_2026-08-25_12-00-01").exists())
        self.assertEqual(
            {fb._metadata_from_archive(archive)["version_tag"] for archive in archives},
            {f"build_r{index}" for index in range(2, 11)},
        )
        self.assertTrue((target / "latest" / "build_info.json").is_file())

    def test_history_starts_only_after_policy_is_enabled(self):
        root = self.sandbox()
        target = root / "firmware_backups" / "devices" / "eolo_dron" / "latest"
        target.mkdir(parents=True)
        (target / "build_info.json").write_text(json.dumps({
            "target": "eolo_dron", "label": "EoloDron", "timestamp": "2026-08-25T10:00:00",
            "version_tag": "legacy", "source_snapshot": "legacy_2026-08-25_10-00-00",
        }), encoding="utf-8")
        build_dir = self.make_build(root)
        with mock.patch.object(fb, "get_git_info", return_value={"commit": "abc1234", "branch": "main", "dirty": False, "tag": ""}):
            fb.create_backup(build_dir, "eolo_dron", root, custom_tag="primero")
            self.assertFalse((target.parent / "history").exists())
            fb.create_backup(build_dir, "eolo_dron", root, custom_tag="segundo")
        archives = list((target.parent / "history").glob("*.tar.xz"))
        self.assertEqual(len(archives), 1)
        self.assertEqual(fb._metadata_from_archive(archives[0])["version_tag"], "primero")

    def test_all_named_targets_are_forwarded_to_platformio(self):
        calls = []

        def record(command, **kwargs):
            calls.append((command, kwargs))
            return 0

        targets = [
            "eolo_express",
            "eolo_express_legacy",
            "eolo_standard",
            "eolo_dron",
            "eolo_dron_low_power",
        ]
        with mock.patch.object(fb.shutil, "which", return_value="/usr/bin/pio"), mock.patch.object(
            fb.subprocess, "call", side_effect=record
        ):
            for target in targets:
                self.assertEqual(fb.build_and_backup(target, "Versión prueba"), 0)

        self.assertEqual([call[0][-1] for call in calls], targets)
        self.assertTrue(all(call[1]["env"]["EOLO_TAG"] == "Versión_prueba" for call in calls))

    def test_name_last_supports_current_and_historical_folders(self):
        for old_name in (
            "original_2026-08-25_12-34-56",
            "2026-08-25_12-34-56_original",
        ):
            with self.subTest(old_name=old_name):
                root = self.sandbox()
                target = root / "firmware_backups" / "devices" / "eolo_dron"
                source = target / old_name
                source.mkdir(parents=True)
                base = "EoloDron_2026-08-25_12-34-56_original"
                files = {
                    "firmware": f"{base}_firmware.bin",
                    "bootloader": f"{base}_bootloader.bin",
                    "partitions": f"{base}_partitions.bin",
                    "elf": f"{base}_firmware.elf",
                    "map": f"{base}_firmware.map",
                }
                for filename in files.values():
                    (source / filename).write_bytes(filename.encode())
                metadata = {
                    "target": "eolo_dron",
                    "label": "EoloDron",
                    "category": "devices",
                    "version_tag": "original",
                    "files": {key: {"file": filename} for key, filename in files.items()},
                }
                (source / "build_info.json").write_text(json.dumps(metadata), encoding="utf-8")

                self.assertTrue(fb.name_last_backup("Lista final"))
                renamed = target / "Lista_final_2026-08-25_12-34-56"
                self.assertTrue(renamed.is_dir())
                for filename in ("firmware.bin", "bootloader.bin", "partitions.bin", "firmware.elf", "firmware.map"):
                    self.assertTrue((renamed / filename).is_file())
                updated = json.loads((renamed / "build_info.json").read_text(encoding="utf-8"))
                self.assertEqual(updated["version_tag"], "Lista_final")
                self.assertEqual(updated["files"]["firmware"]["file"], "firmware.bin")


if __name__ == "__main__":
    unittest.main()
