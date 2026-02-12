# Repository Guidelines

## Project Structure & Module Organization
`firmware/` is the active codebase for the ESP32-P4 device app.
- `firmware/main/main.c`: runtime logic (mic input, state transitions, LVGL UI updates).
- `firmware/main/*_0.c`, `*_1.c`, `*_2.c`: generated LVGL image frame data (large binary-like C sources).
- `firmware/main/pepper_frames.h`: shared frame declarations and LVGL type compatibility.
- `firmware/sdkconfig.defaults`, `firmware/partitions.csv`, `firmware/dependencies.lock`: target/build configuration.

`hardware/` stores fabrication files (for example DXF), and `docs/` / `media/` are for supporting documentation and assets.

## Build, Test, and Development Commands
Run commands from `firmware/`:
- `idf.py set-target esp32p4`: set the correct chip target (first setup or after cleaning).
- `idf.py build`: configure and compile firmware.
- `idf.py flash monitor`: flash to the board and open serial logs.
- `idf.py fullclean`: remove build artifacts before a clean rebuild.

Use an ESP-IDF 5.5.x environment (the lockfile currently pins IDF `5.5.2`).

## Coding Style & Naming Conventions
- Language: C (ESP-IDF + LVGL APIs), 4-space indentation, braces on the same line.
- Prefer `static` for file-local functions/consts and `snake_case` for functions/variables.
- Use ALL_CAPS for compile-time constants/macros (for example `RMS_WINDOW_MS`).
- Keep core behavior in `main.c`; treat frame files as generated data, not hand-edited logic.

No formatter/linter config is currently checked in; match the surrounding style in touched files.

## Testing Guidelines
There is no dedicated automated test suite yet. Minimum validation for changes:
- `idf.py build` must succeed with no new warnings of concern.
- `idf.py flash monitor` smoke test on hardware (UI renders, mic-driven state changes work).

When adding tests later, prefer `firmware/tests/` and name files by feature (for example `test_audio_levels.c`).

## Commit & Pull Request Guidelines
Recent commits use short, imperative summaries (for example `Update README.md`, `Revise BOM ...`).
- Keep subject lines concise and action-first.
- One logical change per commit.
- PRs should include: purpose, changed paths, hardware/firmware validation steps, and photos or log snippets when UI/device behavior changes.

## Configuration & Security Notes
- Do not commit secrets (Wi-Fi/API keys) in `sdkconfig` or source files.
- Keep `managed_components/` and `build/` untracked (see `firmware/.gitignore`).
