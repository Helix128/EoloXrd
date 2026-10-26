# Repository Guidelines

Firmware PlatformIO/ESP32 for the EOLO device family. Developer guide: `docs/firmware/desarrollo.md`. Docs index: `docs/README.md`.

## Layout

- `src/Ui/` (display models), `src/Headless/` (Dron = EOLO Mini; product names: Mini Rev.1/Rev.2, EOLO-MP, Express Rev.1/Rev.2), `src/Modem/`, `src/Variants/` (`Profiles/`, `Pinouts/`), `src/Common/`. `Ui/` and `Headless/` must not include each other (`scripts/check_family_boundaries.py`).
- Portable code: `include/Eolo/`, `lib/EoloCore/`. Tests: `test/test_*/` (PlatformIO), `test/test_*.py` (Python).
- Pinout references: `docs/hardware/pinouts/`. Demos: `demos/`. Setup web UI source: `web-server/`.

## Commands

- `pio run [-e <env>]` build (default `eolo_express`); envs in `platformio.ini`.
- `pio test -e native` host tests; `python3 -m unittest discover -s test -p 'test_*.py'` tooling tests.
- `scripts/check_all.sh` builds every env and runs all hardware-free checks.
- Compile-only suite check: `pio test -e eolo_dron_legacy -f test_capture_switches --without-uploading --without-testing`. Run HIL tests only with the matching board.

## Rules

- C++17, four-space indent, `PascalCase` types, `camelCase` members, `UPPER_SNAKE_CASE` flags. Variant settings go in `src/Variants/Profiles/`, never in `build_flags`.
- Never edit generated files: `src/Headless/HeadlessSetupWebPage.h`, `platformio.demos.ini`.
- Never commit `src/Headless/HeadlessSecrets.h` or Wi-Fi credentials.
- Add portable tests to `test/test_eolo_core_native/test_main.cpp` as `test_<behavior>`.
- Test changes against each affected env. Commits: Conventional Commits, Spanish description (`fix(modem): ...`), one change per commit.
