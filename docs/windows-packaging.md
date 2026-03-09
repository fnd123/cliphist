# Windows Packaging

## Prerequisites

- CMake 3.21+ (recommended, for runtime dependency collection).
- Qt5 (Widgets) development tools.
- SQLite3 development package.
- NSIS (`makensis`) if you need `.exe` installer output.

## Configure and Build

```bash
cmake -S . -B build-win -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCLIPHIST_ENABLE_X11=OFF
cmake --build build-win
```

## Create Installer/Archive

```bash
cpack --config build-win/CPackConfig.cmake -C Release
```

Default generators on Windows:
- `NSIS`: produces `.exe` installer.
- `ZIP`: produces portable zip package.

## Notes

- `daemon` and `desktop` commands require Linux X11 and are disabled in Windows builds.
- `list`, `stats`, and `ui` commands are available on Windows.
