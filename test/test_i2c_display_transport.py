import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HOST = ROOT / "test/i2c_display_transport_host"


@unittest.skipUnless(shutil.which("c++"), "requiere compilador C++")
class I2cDisplayTransportTest(unittest.TestCase):
    def test_frame_integrity_and_recovery(self):
        with tempfile.TemporaryDirectory() as temp:
            executable = pathlib.Path(temp) / "test_transport"
            subprocess.run(
                ["c++", "-std=c++17", "-I", str(HOST), str(HOST / "test.cpp"),
                 "-o", str(executable)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
