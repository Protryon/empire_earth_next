"""The build under test: where its images and maps are, its record layouts, and the values the harness derives.

ee's harness runs the original game or a VC6 full link next to the retail DLLs and binary-patches them for the
headless VM. This fork's harness runs one of its own clang builds, every module from source:

  stl "dinkum"   build/clang-test/       (clang_link.py --test-profile windowed-software)
  stl "msvc"     build/clang-test-msvc/  (clang_link.py --stl msvc --test-profile windowed-software)

The test profile compiles the windowed/software-display policy in (exe and renderer), so nothing is patched at
staging. Third-party DLLs come from the game install (Miles, Bink, and Dinkum's sxlrt233 for the dinkum profile);
Language.dll from the fork's VC6 build if present, else the install's.
"""
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import layout as L                                   # noqa: E402
from fulllink_locator import LinkMap                 # noqa: E402

INSTALL = pathlib.Path(r"C:\Program Files (x86)\Empire Earth\Empire Earth - The Art of Conquest")
TARGET_FILES = {"exe": "EE-AOC.exe", "lle": "Low-Level Engine.dll", "dx7hr": "DX7HRDisplay.dll",
                "dx7hrtnl": "DX7HRTnLDisplay.dll", "tcp": "TCP Protocol.dll", "default": "Default.dll",
                "mixer": "Miles Sound System Mixer.dll"}
THIRD_PARTY = {"dinkum": ["Mss32.dll", "binkw32.dll", "sxlrt233.dll"], "msvc": ["Mss32.dll", "binkw32.dll"]}
SHOWSCENE = "?ShowScene@DX7Rasterizer@@UAEJK@Z"
ACTIVATE = "?ActivateScreen@DX7Rasterizer@@UAEJKPAVGETextureManager@@PAVGEWindow@@K_NK@Z"


class Build:
    def __init__(self, stl="msvc", test_dir=None):
        self.stl = stl
        self.dir = pathlib.Path(test_dir or ROOT / "build" / ("clang-test" + ("-msvc" if stl == "msvc" else "")))
        self.layout = L.Layout(stl)
        for t, f in TARGET_FILES.items():
            if not (self.dir / t / f).exists():
                raise SystemExit(f"{self.dir / t / f} is missing: python -I tools/clang_link.py "
                                 f"{'--stl msvc ' if stl == 'msvc' else ''}--test-profile windowed-software")

    def image(self, target):
        return self.dir / target / TARGET_FILES[target]

    def map(self, target):
        return LinkMap(self.image(target).with_suffix(".map"))

    @property
    def exe(self):
        return self.image("exe")

    @property
    def exe_map_path(self):
        return self.exe.with_suffix(".map")

    def address(self, target, symbol):
        return self.map(target).address(symbol)

    def rva(self, target, symbol, base):
        return self.address(target, symbol) - base

    def stage_files(self):
        """{staged file name: source path}: the build's images, the third-party DLLs, Language.dll."""
        files = {TARGET_FILES[t]: self.image(t) for t in TARGET_FILES}
        for n in THIRD_PARTY[self.stl]:
            files[n] = INSTALL / n
        lang = ROOT / "build" / "vc6" / "language" / "Language.dll"
        files["Language.dll"] = lang if lang.exists() else INSTALL / "Language.dll"
        return files

    def bridge_defines(self):
        """-D values for presentation_bridge.c (its offsets and the renderer's ShowScene entry)."""
        lay = self.layout
        d = lay.constants({
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
        d["EE_SHOWSCENE_RVA"] = self.rva("dx7hr", SHOWSCENE, 0x10000000)
        return d

    def renderer_guards(self):
        """(ActivateScreen RVA, ShowScene RVA) in the staged renderer, for exe_boot.c's checkpoints."""
        return self.rva("dx7hr", ACTIVATE, 0x10000000), self.rva("dx7hr", SHOWSCENE, 0x10000000)
