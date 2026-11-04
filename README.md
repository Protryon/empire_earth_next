# empire_earth_next

*Empire Earth: The Art of Conquest* (Stainless Steel Studios / Sierra, 2002), built from the source reconstruction of
the `empire_earth` project with a current toolchain: clang-cl and lld-link, 32-bit, the Microsoft C++ standard
library and the Universal C runtime. It starts from the reconstruction's `fixes-all` branch (the original game plus
every original-bug fix) and is free to diverge from the original from here on. `PORT.md` describes the port, and
`PORT_LEARNINGS.md` records every problem met on the way and its fix.

This repository contains no game data and no original binaries. You need a legal copy of the game to play anything
you build.

## Modules

| Module | Image | Source |
|---|---|---|
| `exe` | `EE-AOC.exe` (game; with libpng, zlib and libjpeg) | `src/Empire Earth/`, `src/RTS Engine/`, `src/ThirdParty/` |
| `lle` | `Low-Level Engine.dll` (engine core; with libjpeg and the DCL) | `src/Low-Level Engine/` |
| `default` | `Default.dll` (random map generators) | `src/Empire Earth/Random Map/Random Map Generators/Default/` |
| `dx7hr` | `DX7HRDisplay.dll` (renderer) | `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/` |
| `dx7hrtnl` | `DX7HRTnLDisplay.dll` (T&L renderer) | same folder |
| `tcp` | `TCP Protocol.dll` (network) | `src/Low-Level Engine/Network Engine/TCP Protocol/` |
| `mixer` | `Miles Sound System Mixer.dll` (sound) | `src/Low-Level Engine/Sound Engine/Miles Sound System Mixer/` |
| `language` | `Language.dll` (resources only) | `src/Empire Earth/Language/Language.rc` |

`EE-AOC.exe` still links stubs for the WON online library, whose source is not part of the reconstruction yet (the
online services no longer exist).

## Building

Requirements (Windows 10/11):

- LLVM (clang-cl, lld-link, llvm-lib, llvm-nm): `winget install LLVM.LLVM`
- Visual Studio 2022 Build Tools with the C++ workload, for the MSVC standard library, the UCRT and the Windows
  SDK: `winget install Microsoft.VisualStudio.2022.BuildTools --override "--quiet --wait --add
  Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Tools.x86.x64 --add
  Microsoft.VisualStudio.Component.Windows11SDK.22621"`
- Python 3.12 (standard library only) and 7-Zip (`winget install 7zip.7zip`)
- The DirectX 7 SDK headers the renderers compile against (sha256-pinned download from archive.org into
  `toolchain/`, which is not tracked): `python -I tools/fetch_dx7sdk.py`

Then:

```
python -I build.py                  # every image -> build/clang/<target>/
python -I build.py -j 8 lle exe     # some images (the others link against the engine's import library)
python -I tools/clang_build.py      # compile only, with a ranked error census
```

Each image comes with its `.map` and `.pdb`. Sources are staged under `C:\EEX\` before they are compiled (env
`EE_ORIG_SRC_ROOT` moves it). A full build takes about two minutes on a 12-core machine.

To play, copy the images over a copy of the game's install directory (keeping the game's own `Mss32.dll` and
`binkw32.dll`).

## Testing

`tools/harness/` is a headless live-game harness (ported from the reconstruction project): it stages a test build
(`python -I build.py --test-profile windowed-software`), drives the menus, plays AI games with a time shim and
records telemetry, all on a GPU-less VM. See `tools/harness/README.md`.

## Upstream

The `upstream` remote is the `empire_earth` reconstruction. Its `fixes-all` branch is merged in, never rebased (see
`PORT.md`).

## License

No license is granted. Empire Earth is the property of its rights holders. The third-party libraries under
`src/ThirdParty/` keep their own licenses.
