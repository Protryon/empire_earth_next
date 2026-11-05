# empire_earth_next

*Empire Earth: The Art of Conquest* (Stainless Steel Studios / Sierra, 2002), built from the source reconstruction of
the `empire_earth` project with a current toolchain: clang-cl and lld-link, 32-bit, the Microsoft C++ standard
library and the Universal C runtime. It starts from the reconstruction's `fixes-all` branch (the original game plus
every original-bug fix) and is free to diverge from the original from here on. `PORT.md` describes the port, and
`PORT_LEARNINGS.md` records every problem met on the way and its fix.

This repository contains no game data and no original binaries. You need a legal copy of the game to play anything
you build.

## What it builds

One statically linked `EE-AOC.exe` with the static C runtime. The original's DLLs are linked into it: the engine
(`Low-Level Engine.dll`, with libjpeg and the DCL), the TCP protocol, the mixer, the Default random map generator
and the DX7 hardware T&L renderer (the plug-ins are registered in `src/Low-Level Engine/PIMModule.cpp` instead of
being loaded from DLLs), and the language strings of `Language.dll`. The non-T&L renderer `DX7HRDisplay.dll` is
gone. Sound goes through miniaudio (vendored, statically linked). Only Windows system DLLs are imported, plus Bink
(`binkw32.dll`) until its replacement lands (`PORT.md`, "Plan").

| Module | Was | Source |
|---|---|---|
| `exe` | `EE-AOC.exe` (game; with libpng, zlib) | `src/Empire Earth/`, `src/RTS Engine/`, `src/ThirdParty/` |
| `lle`, `jpeg` | `Low-Level Engine.dll` (engine core, libjpeg, the DCL) | `src/Low-Level Engine/`, `src/ThirdParty/` |
| `default` | `Default.dll` (random map generators) | `src/Empire Earth/Random Map/Random Map Generators/Default/` |
| `dx7hrtnl` | `DX7HRTnLDisplay.dll` (T&L renderer) | `src/Low-Level Engine/Graphics Engine/DX7 Display Plug-ins/` |
| `tcp` | `TCP Protocol.dll` (network) | `src/Low-Level Engine/Network Engine/TCP Protocol/` |
| `mixer` | `Miles Sound System Mixer.dll` (sound; now on miniaudio) | `src/Low-Level Engine/Sound Engine/Mixer/` |
| (resources) | `Language.dll` | `src/Empire Earth/Language/Language.rc` |

The exe still links stubs for the WON online library, whose source is not part of the reconstruction yet (the
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
python -I build.py                  # -> build/clang/exe/EE-AOC.exe
python -I tools/clang_build.py      # compile only, with a ranked error census (--x64: the 64-bit census)
```

The exe comes with its `.map` and `.pdb`. Sources are staged under `C:\EEX\` before they are compiled (env
`EE_ORIG_SRC_ROOT` moves it). A full build takes about two minutes on a 12-core machine.

To play, copy `EE-AOC.exe` into a copy of the game's install directory (keeping the game's own `binkw32.dll`; the
other DLLs are no longer used).

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
