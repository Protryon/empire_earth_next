"""The build under test: where its image and map are, its record layouts, and the values the harness derives.

ee's harness runs the original game or a VC6 full link next to the retail DLLs and binary-patches them for the
headless VM. This harness runs this repository's build: one statically linked EE-AOC.exe (engine, plug-ins and
runtime inside), built with the test profile:

  python -I tools/build.py --test-profile windowed-software            -> build/clang-test/

The test profile compiles the windowed/software-display policy in, so nothing is patched at staging. The game's
mixer runs on miniaudio's null device when the machine has no audio output.

The exe is x86_64 (checked from its PE header), and so are the harness's helpers (exe_boot.exe, the bridge and the
time shim, built with MSYS2 mingw64 gcc). The harness names the game's symbols by their x64 decorations, its
layout.json must be the x64 layout (checked), and every exe address it passes on is an RVA from the map's preferred
load address, so an ASLR image works too.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import layout as L                                   # noqa: E402
from fulllink_locator import LinkMap                 # noqa: E402

INSTALL = pathlib.Path(r"C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest")
TARGET_FILES = {"exe": "EE-AOC.exe"}
THIRD_PARTY = []                                     # every library is linked into the exe
BASE = 0x400000                                      # the exe's fixed base (the map gives its own)
SHOWSCENE = "?ShowScene@DX7Rasterizer@@UEAAJK@Z"
ACTIVATE = "?ActivateScreen@DX7Rasterizer@@UEAAJKPEAVGETextureManager@@PEAVGEWindow@@K_NK@Z"
NATIVE_ERROR = "?ESRecordNativeError@@YAKK@Z"
ABORT = "abort"                                     # the static runtime's
RNG_GENERATE = "?Generate@URandom@@QEAAMXZ"
RNG_NEXT = "?NextInSeries@URandom@@AEAAKXZ"
DEFAULT_DIR = ROOT / "build" / "clang-test"
GCC = pathlib.Path(r"C:\msys64\mingw64\bin\gcc.exe")   # MSYS2 mingw-w64-x86_64-gcc


def machine(path):
    """'x64' for an x86_64 PE image; refuses anything else."""
    data = pathlib.Path(path).read_bytes()[:0x400]
    pe = int.from_bytes(data[0x3c:0x40], "little")
    m = int.from_bytes(data[pe + 4:pe + 6], "little")
    if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0" or m != 0x8664:
        raise SystemExit(f"{path}: not an x86_64 PE image")
    return "x64"


class Build:
    def __init__(self, build_dir=None):
        self.dir = pathlib.Path(build_dir or DEFAULT_DIR).resolve()
        for t, f in TARGET_FILES.items():
            if not (self.dir / t / f).exists():
                raise SystemExit(f"{self.dir / t / f} is missing: python -I tools/build.py --test-profile "
                                 f"windowed-software" + (f" --out {self.dir}" if build_dir else ""))
        self.arch = machine(self.image("exe"))
        self.ptr = 8
        self.layout = L.Layout(self.dir)
        self._check_layout()

    def _check_layout(self):
        """layout.json must come from the same target as the exe: a std::vector's end pointer follows its first one."""
        try:
            step = self.layout.last("UIManager", "mFormStack") - self.layout.first("UIManager", "mFormStack")
        except KeyError:
            return
        if step != self.ptr:
            raise SystemExit(f"{self.dir / 'layout.json'} has {step}-byte pointers, the exe {self.ptr}: "
                             f"python -I tools/harness/layout_gen.py --out {self.dir}")

    @property
    def gcc(self):
        """The mingw64 gcc for the in-process helpers (bridge, time shim)."""
        return GCC

    @property
    def base(self):
        """The exe map's preferred load address (0x400000)."""
        return self.map("exe").base

    def image(self, target="exe"):
        return self.dir / target / TARGET_FILES[target]

    def map(self, target="exe"):
        if not hasattr(self, "_maps"):
            self._maps = {}
        if target not in self._maps:
            self._maps[target] = LinkMap(self.image(target).with_suffix(".map"))
        return self._maps[target]

    @property
    def exe(self):
        return self.image("exe")

    @property
    def exe_map_path(self):
        return self.exe.with_suffix(".map")

    def address(self, target, symbol):
        return self.map(target).address(symbol)

    def rva(self, target, symbol, base=None):
        return self.address(target, symbol) - (self.map(target).base if base is None else base)

    def stage_files(self):
        """{staged file name: source path}: the build's exe and the third-party DLLs."""
        files = {TARGET_FILES[t]: self.image(t) for t in TARGET_FILES}
        for n in THIRD_PARTY:
            files[n] = INSTALL / n
        return files

    def bridge_defines(self):
        """-D values for presentation_bridge.c (its offsets and the renderer's ShowScene entry)."""
        d = self.layout.constants({
            "EE_OFF_UIM_CRITICAL": ("field", "UIManager", "mCriticalSection"),
            "EE_OFF_UIM_FORMS_FIRST": ("first", "UIManager", "mFormStack"),
            "EE_OFF_UIM_FORMS_LAST": ("last", "UIManager", "mFormStack"),
            "EE_OFF_FORM_COUNT": ("field", "UIForm", "mControlCount"),
            "EE_OFF_FORM_ID": ("field", "UIForm", "mFormID"),
            "EE_OFF_FORM_CONTROLS": ("field", "UIForm", "mControls"),
            "EE_OFF_MOUSE_POS": ("field", "EEMouse", "mPosition"),
            "EE_OFF_MOUSE_BOUNDS": ("field", "EEMouse", "mBounds"),
            "EE_OFF_RAST_STATES_FIRST": ("first", "DX7Rasterizer", "mScreenStates"),
            "EE_OFF_STATE_SECONDARY": ("field", "GERScreenState", "mViewport.mSecondarySurface"),
            "EE_OFF_STATE_WINDOW": ("field", "GERScreenState", "mViewport.mWindow"),
            "EE_OFF_SURFACE_DD": ("field", "DX7Surface", "mSurface"),
        })
        d["EE_SHOWSCENE_RVA"] = self.rva("exe", SHOWSCENE)
        d["EE_SIZE_UWIDESTRING"] = self.layout.size("UWideString") if "UWideString" in self.layout.records else 0x14
        return d

    def renderer_guards(self):
        """(ActivateScreen RVA, ShowScene RVA) in the exe, for exe_boot.c's checkpoints."""
        return self.rva("exe", ACTIVATE), self.rva("exe", SHOWSCENE)

    def static_exe_args(self):
        """exe_boot.c's --static-exe arguments: the exe RVAs of the hooks the DLL builds armed in other modules."""
        return ["--static-exe"] + [f"{self.rva('exe', s):x}" for s in (NATIVE_ERROR, ABORT, RNG_GENERATE, RNG_NEXT)]
