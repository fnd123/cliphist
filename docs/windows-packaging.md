# Windows Packaging

## Prerequisites

- CMake 3.21+ (recommended, for runtime dependency collection).
- Qt5 (Widgets) development tools.
- SQLite3 development package.
- NSIS (`makensis`) if you need `.exe` installer output.

Important:
- Qt build and C++ toolchain must match.
- If you use MSYS2/MinGW Qt, run `cmake` inside the `MSYS2 MinGW 64-bit` shell.
- If you use official Qt for MSVC, run from a Visual Studio Developer Command Prompt and pass `-DCMAKE_PREFIX_PATH=<Qt install prefix>`.

The common Windows error `cannot find -lQt5Core -lQt5Gui -lstdc++-6 -lgcc_s_seh-1`
usually means one of these is true:
- you are mixing MSVC with a MinGW-built Qt package, or
- Qt is not visible through `CMAKE_PREFIX_PATH` / `Qt5_DIR`, or
- MinGW runtime DLLs are not being picked from the compiler's `bin` directory.

## Configure and Build

### MSYS2 / MinGW64

```bash
cmake -S . -B build-win -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCLIPHIST_ENABLE_X11=OFF \
  -DCMAKE_PREFIX_PATH=/mingw64
cmake --build build-win
```

Required packages:

```bash
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-qt5-base \
  mingw-w64-x86_64-sqlite3 \
  mingw-w64-x86_64-nsis
```

### MSVC + official Qt

```bat
cmake -S . -B build-win -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCLIPHIST_ENABLE_X11=OFF ^
  -DCMAKE_PREFIX_PATH=C:\Qt\5.15.2\msvc2019_64
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
- Packaging now runs recursive runtime dependency collection against the installed `cliphist.exe`, so Qt/MinGW/SQLite and their transitive DLLs are gathered automatically instead of relying on a hand-maintained DLL list.
