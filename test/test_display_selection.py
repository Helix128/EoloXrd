import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/Variants/Displays.h"


@unittest.skipUnless(shutil.which("c++"), "requiere compilador C++")
class DisplaySelectionTest(unittest.TestCase):
    def compile_source(self, source):
        with tempfile.TemporaryDirectory() as temp_dir:
            source_path = pathlib.Path(temp_dir) / "display_selection.cpp"
            source_path.write_text(source)
            return subprocess.run(
                ["c++", "-std=c++17", "-fsyntax-only", str(source_path)],
                text=True,
                capture_output=True,
                check=False,
            )

    def test_profile_default_selects_i2c(self):
        source = f"""
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#include \"{HEADER}\"
static_assert(EOLO_DISPLAY_SPI == 0);
static_assert(EOLO_DISPLAY_SPI_CLOCK_HZ == 0UL);
static_assert(!EoloConfig::selectedDisplay.spi);
static_assert(EoloConfig::selectedDisplay.kind == EoloConfig::DisplayKind::SSD1309I2c);
static_assert(EoloConfig::selectedDisplay.spiClockHz == 0UL);
#include <string_view>
#define EOLO_STRINGIFY_IMPL(value) #value
#define EOLO_STRINGIFY(value) EOLO_STRINGIFY_IMPL(value)
static_assert(std::string_view(EOLO_STRINGIFY(EOLO_DISPLAY_MODEL)) ==
              "U8G2_SSD1309_128X64_NONAME0_F_HW_I2C");
"""
        result = self.compile_source(source)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_explicit_spi_selection(self):
        source = f"""
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#define EOLO_DISPLAY SSD1309_SW_SPI
#include \"{HEADER}\"
static_assert(EOLO_DISPLAY_SPI == 1);
static_assert(EOLO_DISPLAY_SPI_CLOCK_HZ == 8000000UL);
static_assert(!EOLO_DISPLAY_HW_SPI);
static_assert(EoloConfig::selectedDisplay.spi);
static_assert(!EoloConfig::selectedDisplay.hardwareSpi);
static_assert(EoloConfig::selectedDisplay.spiClockHz == 8000000UL);
#include <string_view>
#define EOLO_STRINGIFY_IMPL(value) #value
#define EOLO_STRINGIFY(value) EOLO_STRINGIFY_IMPL(value)
static_assert(std::string_view(EOLO_STRINGIFY(EOLO_DISPLAY_MODEL)) ==
              "U8G2_SSD1309_128X64_NONAME2_F_4W_SW_SPI");
"""
        result = self.compile_source(source)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_hardware_spi_uses_standard_clock(self):
        source = f"""
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#define EOLO_DISPLAY SSD1306_HW_SPI
#include \"{HEADER}\"
static_assert(EOLO_DISPLAY_SPI == 1);
static_assert(EOLO_DISPLAY_HW_SPI == 1);
static_assert(EOLO_DISPLAY_SPI_CLOCK_HZ == 4000000UL);
static_assert(EoloConfig::selectedDisplay.spiClockHz == 4000000UL);
#include <string_view>
#define EOLO_STRINGIFY_IMPL(value) #value
#define EOLO_STRINGIFY(value) EOLO_STRINGIFY_IMPL(value)
static_assert(std::string_view(EOLO_STRINGIFY(EOLO_DISPLAY_MODEL)) ==
              "U8G2_SSD1306_128X64_NONAME_F_4W_HW_SPI");
"""
        result = self.compile_source(source)
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_unknown_display_is_rejected(self):
        source = f"""
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#define EOLO_DISPLAY NOT_A_DISPLAY
#include \"{HEADER}\"
"""
        result = self.compile_source(source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("EOLO_DISPLAY debe ser", result.stderr)

    def test_legacy_spi_pinout_rejects_missing_display_pins(self):
        source = f"""
#define EOLO_TARGET_EXPRESS_LEGACY
#define EOLO_DEFAULT_DISPLAY SSD1309_SW_SPI
#include "{HEADER}"
#include "{ROOT / 'src/Variants/Pinout.h'}"
"""
        result = self.compile_source(source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("OLED SPI requiere", result.stderr)

    def test_express_spi_pinout_rejects_rs485_collision(self):
        source = f"""
#define EOLO_TARGET_EXPRESS
#define EOLO_DEFAULT_DISPLAY SSD1309_I2C
#define EOLO_DISPLAY SSD1309_SW_SPI
#include "{HEADER}"
#include "{ROOT / 'src/Variants/Pinout.h'}"
"""
        result = self.compile_source(source)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("OLED SPI duplica GPIO de RS485", result.stderr)

    def test_headless_spi_default_does_not_require_display_pins(self):
        source = f"""
#define EOLO_TARGET_DRON_LEGACY
#define FEATURE_HEADLESS
#define EOLO_DEFAULT_DISPLAY SSD1309_SW_SPI
#include "{HEADER}"
#include "{ROOT / 'src/Variants/Pinout.h'}"
static_assert(EOLO_DISPLAY_SPI == 1);
static_assert(EOLO_DISPLAY_SPI_CLOCK_HZ == 8000000UL);
static_assert(EOLO_DISPLAY_CS_PIN == EOLO_PIN_UNUSED);
"""
        result = self.compile_source(source)
        self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
