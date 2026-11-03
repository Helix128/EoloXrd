import importlib.util
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
CORE_INCLUDE = ROOT / "lib/EoloCore/include"

spec = importlib.util.spec_from_file_location("eolo_mirror", ROOT / "scripts/eolo_mirror.py")
eolo_mirror = importlib.util.module_from_spec(spec)
spec.loader.exec_module(eolo_mirror)

ENCODER = r"""
#include <Eolo/Core/Ui/DisplayMirrorFrame.h>
#include <cstdio>
int main() {
    uint8_t raw[1024];
    uint8_t frame[DisplayMirrorFrame::maxFrameSize(1024)];
    for (int i = 0; i < 1024; ++i) raw[i] = (i % 128 < 30) ? (uint8_t)(i * 7) : 0;
    size_t n = DisplayMirrorFrame::encode(raw, 16, 8, frame);
    fwrite(raw, 1, 1024, stdout);
    fwrite(frame, 1, n, stdout);
    uint8_t next[1024];
    for (int i = 0; i < 1024; ++i) next[i] = raw[i];
    next[128 * 3 + 9] ^= 0x5A;
    next[128 * 7] ^= 0x01;
    fwrite(next, 1, 1024, stdout);
    n = DisplayMirrorFrame::encodeDelta(next, raw, 16, 8, frame);
    fwrite(frame, 1, n, stdout);
}
"""


@unittest.skipUnless(shutil.which("c++"), "requiere compilador C++")
class MirrorProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as temp:
            source = pathlib.Path(temp) / "enc.cpp"
            binary = pathlib.Path(temp) / "enc"
            source.write_text(ENCODER)
            subprocess.run(
                ["c++", "-std=c++17", f"-I{CORE_INCLUDE}", str(source), "-o", str(binary)],
                check=True,
            )
            out = subprocess.run([str(binary)], check=True, capture_output=True).stdout
        cls.raw = out[:1024]
        # El firmware emite: crudo, trama completa, crudo siguiente, trama delta.
        delta_at = out.index(b"\xff\xe0\xf0\x0c")
        cls.frame = out[1024 : delta_at - 1024]
        cls.next_raw = out[delta_at - 1024 : delta_at]
        cls.delta = out[delta_at:]

    def test_decodes_firmware_frame(self):
        text, frames = eolo_mirror.FrameParser().feed(self.frame)
        self.assertEqual(text, b"")
        self.assertEqual(frames, [(128, 64, self.raw)])

    def test_separates_logs_and_survives_split_chunks(self):
        stream = b"[a.h:1] hola \xc3\xa1\n" + self.frame + b"fin\n"
        parser = eolo_mirror.FrameParser()
        text, frames = b"", []
        for i in range(0, len(stream), 7):
            t, f = parser.feed(stream[i : i + 7])
            text += t
            frames += f
        self.assertEqual(text, b"[a.h:1] hola \xc3\xa1\nfin\n")
        self.assertEqual(frames, [(128, 64, self.raw)])

    def test_corrupt_frame_is_dropped_and_next_one_decodes(self):
        bad = bytearray(self.frame)
        bad[20] ^= 0xFF
        _, frames = eolo_mirror.FrameParser().feed(bytes(bad) + self.frame)
        self.assertEqual(frames, [(128, 64, self.raw)])

    def test_delta_applies_on_top_of_full_frame(self):
        parser = eolo_mirror.FrameParser()
        _, frames = parser.feed(self.frame + self.delta)
        self.assertEqual(frames, [(128, 64, self.raw), (128, 64, self.next_raw)])

    def test_delta_without_base_is_ignored(self):
        _, frames = eolo_mirror.FrameParser().feed(self.delta)
        self.assertEqual(frames, [])

    def test_pixel_order_is_page_major_lsb_top(self):
        raw = bytearray(1024)
        raw[128 * 2 + 5] = 0b00000100  # pagina 2, x=5, bit 2 -> y=18
        pixels = eolo_mirror.to_pixels(128, 64, bytes(raw))
        self.assertEqual(sum(pixels), 1)
        self.assertEqual(pixels[18 * 128 + 5], 1)


if __name__ == "__main__":
    unittest.main()
