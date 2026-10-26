import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

import build_flasher_site as site  # noqa: E402

PART_NAMES = [name for name, _ in site.PARTS]


def make_release(root, tag, envs, parts=PART_NAMES):
    release = root / tag
    release.mkdir(parents=True)
    (release / "release.json").write_text(
        json.dumps({"tag": tag, "date": "2026-10-26T12:00:00Z", "url": f"https://example/{tag}"})
    )
    slugs = {v["env"]: v for v in site.load_variants()}
    for env in envs:
        with zipfile.ZipFile(release / site.package_name(slugs[env], tag), "w") as zf:
            for name in parts:
                zf.writestr(name, f"{env}:{name}")


class BuildFlasherSiteTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.releases = self.root / "releases"
        self.out = self.root / "site"

    def tearDown(self):
        self.tmp.cleanup()

    def test_versions_sorted_newest_first(self):
        make_release(self.releases, "v1.2.0", ["eolo_express"])
        make_release(self.releases, "v1.10.0", ["eolo_express"])
        make_release(self.releases, "v1.9.1", ["eolo_express"])
        result = site.build(self.releases, self.out)
        self.assertEqual([v["tag"] for v in result["versions"]], ["v1.10.0", "v1.9.1", "v1.2.0"])

    def test_manifest_flashes_parts_without_touching_nvs(self):
        make_release(self.releases, "v1.0.0", ["eolo_dron_legacy"])
        site.build(self.releases, self.out)
        manifest = json.loads(
            (self.out / "firmware/v1.0.0/eolo_dron_legacy/manifest.json").read_text()
        )
        offsets = [part["offset"] for part in manifest["builds"][0]["parts"]]
        self.assertEqual(offsets, [0x1000, 0x8000, 0xE000, 0x10000])
        # La NVS vive en 0x9000-0xDFFF y ninguna parte la cubre.
        self.assertNotIn(0x9000, offsets)
        self.assertTrue(manifest["new_install_prompt_erase"])
        self.assertEqual(manifest["name"], "EOLO Mini Rev.1")
        for name in PART_NAMES:
            self.assertTrue((self.out / "firmware/v1.0.0/eolo_dron_legacy" / name).exists())

    def test_only_lists_envs_present_in_release(self):
        make_release(self.releases, "v1.0.0", ["eolo_standard"])
        result = site.build(self.releases, self.out)
        self.assertEqual(result["versions"][0]["envs"], ["eolo_standard"])

    def test_incomplete_package_fails(self):
        make_release(self.releases, "v1.0.0", ["eolo_express"], parts=["firmware.bin"])
        with self.assertRaises(SystemExit):
            site.build(self.releases, self.out)

    def test_site_without_releases_still_builds(self):
        result = site.build(self.releases, self.out)
        self.assertEqual(result["versions"], [])
        self.assertTrue((self.out / "index.html").exists())
        self.assertTrue((self.out / "site.json").exists())

    def test_every_platformio_env_has_a_variant(self):
        ini = (PROJECT_ROOT / "platformio.ini").read_text()
        envs = {line[5:-1] for line in ini.splitlines() if line.startswith("[env:")}
        envs.discard("native")
        self.assertEqual(envs, {v["env"] for v in site.load_variants()})


if __name__ == "__main__":
    unittest.main()
