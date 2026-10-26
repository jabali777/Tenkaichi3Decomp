# BT3 Low Spec · Linux

A launcher and OpenGL adapters for playing **Dragon Ball Z: Budokai Tenkaichi 3** on low-spec Linux systems, using the [Tenkaichi3Decomp port by z3xox](https://github.com/z3xox/Tenkaichi3Decomp).

Includes a profile menu, reduced internal rendering, simplified shading, and a session recorder that uses isolated copies of saves. This is an independent add-on; the port, installer, and game data must be obtained separately and are not included in the distribution.

## Quick start

Requirements: Linux x86-64, an OpenGL 3.3 driver, a C compiler (GCC or Clang), standard shell tools, and Python 3.9 or later for diagnostics and tests.

1. Download the Linux x86-64 release of the [original port](https://github.com/z3xox/Tenkaichi3Decomp/releases).
2. Extract it into `Tenkaichi3Decomp/`, next to `play.sh`.
3. Run its installer with your own USA **SLUS-21678** disc image. See the port's README for adding European edition languages.
4. Build the adapters and start the menu:

```sh
sh optimization/build.sh
sh play.sh
```

Expected layout:

```text
bt3-low-spec/
├── play.sh
├── play-fast.sh
├── play-diagnostics.sh
├── optimization/              # Source and locally compiled adapters
└── Tenkaichi3Decomp/          # Private installation, excluded from Git
    ├── Tenkaichi3Decomp
    ├── Tenkaichi3Decomp-setup
    ├── lib/
    └── gamedata/
```

Use a path without spaces or `:` so the `LD_PRELOAD` loader can find the adapters. The menu runs in a terminal; when opened from a file manager it attempts to start `xfce4-terminal` if available.

## Profiles

All profiles use a 640 × 480 window, disable effects, and enable frame skipping. Interpolation is enabled to present up to 60 FPS.

Combat simulation and logic still run at 30 FPS. Interpolation generates intermediate frames for presentation at up to 60 FPS; it does not double game speed. This experimental port feature may produce uneven results during camera changes. To restore the original 30 FPS rendering, use `BT3_INTERP=0 sh play-fast.sh`.

| Option | Internal image | Characters | Shadows under characters |
| --- | --- | --- | --- |
| 1 · Maximum smoothness | 256 × 224 | Simple shading | Omitted |
| 2 · Sharper image | 512 × 448 | Simple shading | Omitted |
| 3 · With shadows | 512 × 448 | Simple shading | Retained |
| 4 · Original shading | 512 × 448 | Original shading | Retained |

Option 5 toggles the FPS counter and option 6 opens diagnostics. Select a profile at each launch. On the GPU used for measurements, maximizing the window significantly reduces performance.

Direct launch and settings:

```sh
sh play-fast.sh
BT3_BAJA_RESOLUCION=0 sh play-fast.sh
BT3_SOMBREADO_SIMPLE=1 sh play-fast.sh
BT3_INTERP=0 sh play-fast.sh
BT3_METER=0 sh play-fast.sh
BT3_OPTIMIZACION_GL=0 sh play-fast.sh
```

The last option skips both adapters while retaining the rest of the fast profile. To open the port with its own settings:

```sh
cd Tenkaichi3Decomp
./Tenkaichi3Decomp
```

## Performance and compatibility

Previous local measurements on a **Pentium B950 with Intel HD Graphics 2000** showed 28.98–29.22 FPS during an ultimate attack sequence and 27.45–28.00 FPS during an effects-heavy sequence, at 256 × 224 in a 640 × 480 window. These are samples from one specific fight; load spikes of up to 171 ms remain. See [performance](docs/PERFORMANCE.md) for methodology, conditions, and limitations.

The adapters intercept OpenGL functions from the SDL3 loader. They depend on the port's renderer implementation and compatibility with all versions is not guaranteed. The exact tag of the local executable has not been identified; its available identifier is in [architecture](docs/ARCHITECTURE.md). Automated tests do not certify visual behavior or online play.

## Diagnostics and development

With the game closed, run `sh play-diagnostics.sh`, play a local fight, and close the game. Logs are stored in `performance-diagnostics/`; saves and settings are copied for the session. These files remain outside Git.

```sh
sh scripts/check.sh
CC=clang sh optimization/build.sh
```

The C tests simulate OpenGL calls, and Python checks save isolation and launcher behavior. These tests do not require the game.

## Creating a source archive

```sh
python3 scripts/export.py
```

This creates `dist/bt3-low-spec-src.tar.gz` from an explicit source file list. It excludes the local installation, saves, logs, compiled libraries, and backups.

## Credits and license

The port and decompilation belong to their respective contributors. See [NOTICE.md](NOTICE.md) for attribution and license scope. This add-on's code is distributed under **AGPL-3.0-only**; the full text is in [LICENSE](LICENSE). Game data is not covered by that license or distributed here.
