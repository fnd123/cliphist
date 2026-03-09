# Repository Guidelines

## Project Structure & Module Organization
Core code lives in `src/`, split by responsibility: `app/` orchestrates flows, `cli/` parses commands and formats terminal output, `core/` holds retention and dedup logic, `db/` wraps SQLite access, `x11/` watches clipboard events, `ui/` provides the X11 viewer, and `util/` contains shared helpers. Tests are under `tests/unit` and `tests/integration`. Supporting material lives in `docs/` (`architecture.md`, `cli.md`) and `sql/schema.sql`. Keep generated files inside `build/`; do not commit build artifacts back into source folders.

## Build, Test, and Development Commands
Configure an out-of-tree build with `cmake -S . -B build`. Build the app and test binaries with `cmake --build build`. Run the full test suite with `ctest --test-dir build --output-on-failure`. Run the CLI locally with `./build/cliphist list --limit 20` or start the daemon with `./build/cliphist daemon --selection clipboard`. If CMake cannot find system packages, install the X11, Xfixes, Xft, and SQLite development headers first.

## Coding Style & Naming Conventions
This project uses C++17 and compiles with strict warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wnon-virtual-dtor -Wold-style-cast`. Match the existing style: 2-space indentation, opening braces on the same line, and include project headers with quoted paths such as `"app/Application.hpp"`. Use `PascalCase` for types and methods, `snake_case` for local variables and filenames, and keep enum constants in the existing `kName` style.

## Testing Guidelines
Add unit tests in `tests/unit/test_*.cpp` for isolated logic and integration tests in `tests/integration/test_*.cpp` for CLI or repository behavior. New features should add or update at least one executable test target in `CMakeLists.txt`. Prefer small self-checking programs that return nonzero on failure, consistent with the current tests. Run `ctest --test-dir build --output-on-failure` before opening a PR.

## Commit & Pull Request Guidelines
This workspace snapshot does not include `.git` history, so local commit conventions cannot be verified. Use short, imperative commit subjects such as `Add CLI filter validation` and keep each commit focused. Pull requests should describe behavior changes, list verification steps, and link any related issue. Include screenshots only for `ui` changes; CLI-only changes should include example commands and output when useful.
