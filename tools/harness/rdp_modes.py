"""Build the TEST-ONLY RDP display-mode proxy (rdp_modes.c, staged as DDRDP.dll) and decide whether a run needs it.

needed(): True when the primary display does not enumerate both 640x480x32 and 800x600x32 (an RDP session: the Remote
Display Adapter offers only the client's desktop size). The software-display profile then renames the staged
DX7HRDisplay.dll's import "DDRAW.dll" to "DDRDP.dll" and stages this proxy (owner, 2026-11-01; see rdp_modes.c)."""
import ctypes
import hashlib
import json
import os
import pathlib
import subprocess
import sys
from ctypes import wintypes

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from windowed_overlay import ROOT, test_output

PROXY_NAME = 'DDRDP.dll'
SIZES = [(640, 480), (800, 600), (1024, 768)]   # keep in step with kSizes in rdp_modes.c


class DEVMODEW(ctypes.Structure):
    _fields_ = [('dmDeviceName', wintypes.WCHAR * 32), ('dmSpecVersion', wintypes.WORD), ('dmDriverVersion', wintypes.WORD),
                ('dmSize', wintypes.WORD), ('dmDriverExtra', wintypes.WORD), ('dmFields', wintypes.DWORD),
                ('dmPositionX', wintypes.LONG), ('dmPositionY', wintypes.LONG), ('dmDisplayOrientation', wintypes.DWORD),
                ('dmDisplayFixedOutput', wintypes.DWORD), ('dmColor', ctypes.c_short), ('dmDuplex', ctypes.c_short),
                ('dmYResolution', ctypes.c_short), ('dmTTOption', ctypes.c_short), ('dmCollate', ctypes.c_short),
                ('dmFormName', wintypes.WCHAR * 32), ('dmLogPixels', wintypes.WORD), ('dmBitsPerPel', wintypes.DWORD),
                ('dmPelsWidth', wintypes.DWORD), ('dmPelsHeight', wintypes.DWORD), ('dmDisplayFlags', wintypes.DWORD),
                ('dmDisplayFrequency', wintypes.DWORD), ('rest', ctypes.c_byte * 64)]


def primary_modes():
    modes, i = set(), 0
    while True:
        m = DEVMODEW(); m.dmSize = ctypes.sizeof(DEVMODEW)
        if not ctypes.windll.user32.EnumDisplaySettingsW(None, i, ctypes.byref(m)):
            return modes
        modes.add((m.dmPelsWidth, m.dmPelsHeight, m.dmBitsPerPel)); i += 1


def needed():
    modes = primary_modes()
    return any((w, h, 32) not in modes for w, h in SIZES)


def build_proxy(directory):
    directory = pathlib.Path(directory).resolve()
    output = directory/PROXY_NAME
    test_output(directory/'DX7HRDisplay.dll', output)
    work = directory/'rdp-modes-build'
    work.mkdir(exist_ok=True)
    sys.path.insert(0, str(ROOT/'tools'))
    from ee import CLANG, LLD_LINK, RUNTIME_LIBS, lib_paths, modern_dirs
    msvc, sdk, _, _ = modern_dirs()
    env = dict(os.environ, INCLUDE=';'.join(str(p) for p in [ROOT/'toolchain/dx7sdk/include', msvc/'include', sdk/'ucrt',
                                                            sdk/'um', sdk/'shared', ROOT/'include/winsdk']))
    source = pathlib.Path(__file__).with_suffix('.c')
    obj = work/'rdp_modes.obj'
    subprocess.run([str(CLANG), '--target=x86_64-pc-windows-msvc', '/nologo', '/c', '/O1', '/MT', '/GS-', '-Wno-everything',
                    str(source), '/Fo'+str(obj)], env=env, cwd=work, check=True, capture_output=True)
    definition = work/'rdp_modes.def'
    definition.write_text('LIBRARY "DDRDP"\nEXPORTS\nDirectDrawCreateEx\nDirectDrawEnumerateA\nDirectDrawEnumerateExA\n',
                          encoding='ascii')
    subprocess.run([str(LLD_LINK), '/nologo', '/DLL', '/NODEFAULTLIB', '/MACHINE:X64', '/ENTRY:DllMain',
                    '/OUT:'+str(output), '/DEF:'+str(definition), str(obj), 'user32.lib', *RUNTIME_LIBS, *lib_paths()],
                   cwd=work, check=True, capture_output=True)
    manifest = dict(test_only=True, kind='rdp-mode-proxy-v1', source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    output_sha256=hashlib.sha256(output.read_bytes()).hexdigest(), modes=sorted(primary_modes()),
                    synthesized_if_missing=['%dx%dx32' % s for s in SIZES],
                    owner='2026-11-01: test-only mode-list shim for RDP sessions (windowed profile; no display-mode change)',
                    limitations='Adds missing 640x480/800x600/1024x768 32-bit entries to EnumDisplayModes (filtered calls only when the filter allows them); '
                                'real modes pass through unchanged; never a reconstructed module')
    output.with_suffix('.rdp.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    return manifest


if __name__ == '__main__':
    print('needed:', needed(), 'primary modes:', sorted(primary_modes()))
    if len(sys.argv) > 1:
        print(json.dumps(build_proxy(sys.argv[1]), indent=2))
