import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class RtcStartupTest(unittest.TestCase):
    def test_both_rtc_backends_preserve_valid_time_and_recover_safely(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "rtc-startup"
            for backend in (0, 1):
                with self.subTest(backend=backend):
                    subprocess.run([
                        "c++", "-std=c++17", f"-DEOLO_I2C_DIRECT_DRIVERS={backend}",
                        "-I", str(ROOT / "test/rtc_startup_stubs"),
                        "-I", str(ROOT / "lib/EoloCore/include"),
                        str(ROOT / "test/rtc_host/rtc_startup_host.cpp"), "-o", str(binary),
                    ], check=True)
                    subprocess.run([str(binary)], check=True)

    def test_direct_driver_separates_invalid_calendar_from_transport_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            binary = Path(directory) / "rtc-direct"
            subprocess.run([
                "c++", "-std=c++17",
                "-I", str(ROOT / "test/rtc_startup_stubs"),
                str(ROOT / "test/rtc_host/rtc_direct_host.cpp"), "-o", str(binary),
            ], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
