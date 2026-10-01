# Repository Guidelines

## Project Structure & Module Organization

`src/main.cpp` starts the firmware; `src/Application/` selects the active application. Code is organized by product family: `src/Ui/` (display models: scenes, drawing, encoder input, `UiApplication`), `src/Headless/` (Dron: `DronApplication`, setup server, capture switches, debug commands), `src/Modem/` (Standard modem stack), `src/Variants/` (per-model profiles in `Profiles/`, pinouts in `Pinouts/`, `ActiveProfile.h`, `Pinout.h`, `Legacy.h`, validation) and `src/Common/` (shared `Board/`, `Data/`, `Sensors/`, `Effectors/`, `Utility/`). `Ui/` and `Headless/` must not include each other; `scripts/check_family_boundaries.py` enforces it. Shared portable code lives in `include/Eolo/` and `lib/EoloCore/`. PlatformIO tests are under `test/test_*/`; Python script tests are `test/test_*.py`. Use `demos/` for isolated hardware sketches, `scripts/` for development tools, `web-server/` for the setup UI, `pinouts/` for GPIO references, and `docs/` for architecture and manuals.

## Build, Test, and Development Commands

- `pio run` builds the default `eolo_express` firmware.
- `pio run -e eolo_dron` builds a specific variant; available environments are defined in `platformio.ini`.
- `pio run -e eolo_dron -t upload` flashes a connected Dron board; `pio device monitor -b 115200` opens serial output.
- `./scripts/demo.py list` lists hardware demos; `./scripts/demo.py build AFM07 dron` builds one.
- `pio test -e native` runs portable `EoloCore` tests on the host.
- `python3 -m unittest discover -s test -p 'test_*.py'` runs Python tooling tests.
- `scripts/check_all.sh` builds every firmware environment and runs all hardware-free checks (`--with-hw-suites`, `--with-demos` for more).

## Coding Style & Naming Conventions

Firmware uses C++17 and four-space indentation. Follow the surrounding file's brace and include style. Use `PascalCase` for types, `camelCase` for methods and fields, and `UPPER_SNAKE_CASE` for build flags such as `EOLO_TARGET_DRON`. Keep variant-specific settings in `src/Variants/Profiles/` and pin assignments aligned with `src/Variants/Pinout.h` and `pinouts/`. No repository-wide formatter or linter is configured; keep changes focused and match existing conventions.

## Testing Guidelines

Add portable behavior tests to the Unity suite in `test/test_eolo_core_native/test_main.cpp`; name cases `test_<behavior>`. Hardware suites use `test/<suite>/test_main.cpp`. For a compile/link check without a board, run `pio test -e eolo_dron -f test_capture_switches --without-uploading --without-testing`. This does not execute the test. Run hardware-in-the-loop tests only with the matching board and locally configured `test_port`. Test changed firmware against each affected PlatformIO environment.

## Commit & Pull Request Guidelines

Recent commits use short, descriptive subjects, often in Spanish, with occasional `fix:` or `add:` prefixes; there is no enforced commit format. State the affected variant and behavior clearly. In pull requests, summarize the change, list affected environments and test commands/results, link relevant issues, and attach screenshots for setup UI changes or hardware evidence when behavior depends on a device.
