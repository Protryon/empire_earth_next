"""Build a TEST-ONLY windowed EXE; never modify reconstructed source or canonical images.

python -I tools/boot/windowed_overlay.py INPUT --output build/windowed/EE-AOC.exe

The two original preference writers force fullscreen on. In the settings constructor,
AL is zero throughout the validated instruction range, so the one-byte ModRM change
selects AL rather than CL (one). Load's immediate one becomes zero. This also disables
the fullscreen splash before its thread starts; Start/Stop continue to agree.
Only fixed-layout, original-byte versions of both preference writers and the
audio initialization observer's complete function are accepted.
Generated images are test artifacts, never verifier inputs or byte-matching claims.
"""
import argparse
import hashlib
import json
import pathlib

import pefile

ROOT = pathlib.Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'orig-bin/aoc/EE-AOC.unwrapped.exe'
FUNCTIONS = [(0x1324F8, 0x13281E), (0x1328DD, 0x13505E), (0x143557, 0x14382B)]
PATCHES = [
    (0x13274B, bytes.fromhex('888ecf050000'), bytes.fromhex('8886cf050000'),
     'settings constructor: store zero AL instead of one CL at +0x5cf'),
    (0x132FBF, bytes.fromhex('c680d305000001'), bytes.fromhex('c680d305000000'),
     'preferences Load: keep settings +4+0x5cf false'),
]

# Separate opt-in TEST renderer profile for GPU-less VMs. Uses the real Windows
# D3D7 RGB device and system-memory surfaces; never stubs graphics calls as success.
SOFTWARE_PATCHES = [
    (0x9320, '2500000800', '2501000000', 'enumerate genuine FLOATTLVERTEX software capability'),
    (0x9325, '2d00000800', '2d01000000', 'compare software capability instead of hardware rasterization'),
    (0x93F6, '0f85ff000000', '909090909090', 'allow adapter without DDCAPS_3D; device enumeration still validates RGB'),
    (0x963F, '0f85ff000000', '909090909090', 'same adapter rule for extended enumeration'),
    (0x10688, 'e03de684aa46cf11816f0000c020156e', '605c66a47326cf11a31a00aa00b93356', 'request actual IID_IDirect3DRGBDevice instead of HAL'),
    (0xDFEE, '83780c10', '83780c20', 'probe a real enumerated 32-bit desktop mode'),
    (0xDA08, 'c745d000700010', 'c745d000380000', 'renderable textures in system memory'),
    (0xDA1E, 'c745d000100000', 'c745d000180000', 'non-renderable textures in system memory'),
    (0xDA28, '8945d4', '909090', 'software textures do not request hardware texture management'),
    (0xD964, '0508104000', '0508184000', 'mipmap textures in system memory'),
    (0xD979, '8945d4', '909090', 'software mipmaps do not request hardware texture management'),
    (0xDA5B, 'c745d000400210', 'c745d000080200', 'Z buffers in system memory'),
    (0xDAA4, '0540400010', '0540080000', 'offscreen render targets in system memory'),
    (0xDABB, '2500200000', '2500000000', 'desktop primary is a blit destination, not a 3D render target'),
    (0xDAC0, '0500420010', '0500020000', 'desktop primary without hardware video-memory requirement'),
]
# Only when the display offers no 800x600x32 mode (the VirtIO GPU DOD driver's smallest mode is 1024x768): the
# shell asks for 640x480 and Test32BitSupport for 800x600, both ESVERIFY/garbage without the mode. The windowed
# session never changes the display mode (DX7Screen::SetMode is a no-op unless fullscreen), so the mode list only
# has to name them: EnumVideoModeCallback relabels the two largest modes of that driver (5120x2880 -> 640x480,
# 4480x2520 -> 800x600) in a cave in .text's alignment padding.
MODE_HOOK = (0xDC6D, '8b45088b480c', 'e9be220000' + '90')   # mov eax,[ebp+8]; mov ecx,[eax+0xc] -> jmp cave
MODE_CAVE_RVA = 0xFF30
MODE_CAVE = ('8b4508'                      # mov eax, [ebp+8]          (the DDSURFACEDESC2)
             '81780c00140000' '7510'       # cmp [eax+0xc], 5120; jne next
             'c7400c80020000'              # mov [eax+0xc], 640        (dwWidth)
             'c74008e0010000' 'eb17'       # mov [eax+8], 480          (dwHeight); jmp done
             '81780c80110000' '750e'       # next: cmp [eax+0xc], 4480; jne done
             'c7400c20030000'              # mov [eax+0xc], 800
             'c7400858020000'              # mov [eax+8], 600
             '8b480c'                      # done: mov ecx, [eax+0xc]  (the replaced instructions)
             'e908ddffff')                 # jmp 0xdc73


def display_has_mode(width, height, bits):
    import ctypes
    from ctypes import wintypes

    class DEVMODEW(ctypes.Structure):
        _fields_ = [('dmDeviceName', wintypes.WCHAR * 32), ('dmSpecVersion', wintypes.WORD),
                    ('dmDriverVersion', wintypes.WORD), ('dmSize', wintypes.WORD),
                    ('dmDriverExtra', wintypes.WORD), ('dmFields', wintypes.DWORD), ('dmUnion', ctypes.c_byte * 16),
                    ('dmColor', ctypes.c_short), ('dmDuplex', ctypes.c_short), ('dmYResolution', ctypes.c_short),
                    ('dmTTOption', ctypes.c_short), ('dmCollate', ctypes.c_short),
                    ('dmFormName', wintypes.WCHAR * 32), ('dmLogPixels', wintypes.WORD),
                    ('dmBitsPerPel', wintypes.DWORD), ('dmPelsWidth', wintypes.DWORD),
                    ('dmPelsHeight', wintypes.DWORD), ('dmDisplayFlags', wintypes.DWORD),
                    ('dmDisplayFrequency', wintypes.DWORD)]
    mode = DEVMODEW(dmSize=ctypes.sizeof(DEVMODEW))
    i = 0
    while ctypes.windll.user32.EnumDisplaySettingsW(None, i, ctypes.byref(mode)):
        if (mode.dmPelsWidth, mode.dmPelsHeight, mode.dmBitsPerPel) == (width, height, bits):
            return True
        i += 1
    return False


def sha(data):
    return hashlib.sha256(data).hexdigest()


def test_output(source, output):
    roots = [ROOT/'build/windowed', ROOT/'build/boot/windowed_o', ROOT/'build/boot/windowed_r',
             ROOT/'build/boot/windowed_f',   # windowed_f: the full-link session (fulllink_locator.py)
             ROOT/'build/boot/play']         # play: manual play staging (stage_play.py, run_eeaoc.bat)
    # round 39: parallel instances stage in build/boot/windowed_{o,r,f}_K (game_session.py --instance K, K 1..32)
    roots += [ROOT/f'build/boot/windowed_{kind}_{k}' for kind in 'orf' for k in range(1, 33)]
    if source == output or not any(output.is_relative_to(root.resolve()) for root in roots):
        raise ValueError('Use a distinct test output below build/windowed/ or build/boot/windowed_{o,r,f}[_K]/')


def build_windowed_image(source, output, software_display=False):
    source, output = pathlib.Path(source).resolve(), pathlib.Path(output).resolve()
    test_output(source, output)
    original = source.read_bytes()
    reference = REFERENCE.read_bytes()
    pe = pefile.PE(data=original, fast_load=True)
    ref = pefile.PE(data=reference, fast_load=True)
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000:
        raise ValueError('Only the fixed-layout 0x400000 image is supported')
    functions = []
    for start, end in FUNCTIONS + ([(0x1444D0, 0x14494D)] if software_display else []):
        actual, expected = pe.get_data(start, end-start), ref.get_data(start, end-start)
        if len(actual) != end-start or actual != expected:
            raise ValueError(f'Unsupported function bytes at RVA {start:#x}; refusing overlay')
        functions.append(dict(rva=hex(start), size=end-start, original_sha256=sha(expected)))
    # The complete constructor equality above proves no intervening EAX write after this xor.
    if pe.get_data(0x132659, 2) != bytes.fromhex('33c0'):
        raise ValueError('Constructor AL-zero precondition failed')
    result = bytearray(original)
    changes = []
    patches = PATCHES + ([
        (0x14460A, bytes.fromhex('6a10'), bytes.fromhex('6a20'), 'software test policy: keep requested 32-bit render/texture format despite legacy HAL capability heuristic'),
        (0x144715, bytes.fromhex('c7057408930010000000'), bytes.fromhex('c7057408930020000000'), 'software test policy: 32-bit fallback mode matches the native desktop catalog'),
    ] if software_display else [])
    for rva, before, after, reason in patches:
        offset = pe.get_offset_from_rva(rva)
        if result[offset:offset+len(before)] != before:
            raise ValueError(f'Unexpected patch preimage at RVA {rva:#x}')
        result[offset:offset+len(before)] = after
        changes.append(dict(rva=hex(rva), file_offset=offset, before=before.hex(), after=after.hex(), reason=reason))
    differences = [i for i, (a, b) in enumerate(zip(original, result)) if a != b]
    expected_changes = 4 if software_display else 2
    if len(result) != len(original) or len(differences) != expected_changes:
        raise ValueError(f'Overlay must change exactly {expected_changes} bytes without changing file size')
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(result)
    manifest = dict(test_only=True, kind='native-windowed-preferences-v1', source=str(source),
                    output=str(output), input_sha256=sha(original), output_sha256=sha(result),
                    changed_bytes=expected_changes, software_display=software_display,
                    changed_file_offsets=differences, functions=functions, patches=changes,
                    source_purity='No src/, include/, orig-bin/, or canonical build changes; not matching credit')
    output.with_suffix('.windowed.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    if source.read_bytes() != original:
        raise RuntimeError('Input changed during overlay generation')
    return manifest


def build_software_display(source, output, rdp_modes=False):
    """rdp_modes (owner 2026-11-01): also rename the import "DDRAW.dll" to the same-length TEST proxy "DDRDP.dll"
    (tools/boot/rdp_modes.c), which adds missing 640x480/800x600 32-bit modes under an RDP session."""
    source, output = pathlib.Path(source).resolve(), pathlib.Path(output).resolve()
    test_output(source, output)
    data = source.read_bytes()
    reference = (ROOT/'orig-bin/aoc/retail/DX7HRDisplay.dll').read_bytes()
    if data != reference:
        raise ValueError('Software profile supports only the frozen retail DX7HRDisplay image')
    pe = pefile.PE(data=data, fast_load=True)
    result = bytearray(data); patches = []
    for rva, before, after, reason in SOFTWARE_PATCHES:
        before, after = bytes.fromhex(before), bytes.fromhex(after)
        offset = pe.get_offset_from_rva(rva)
        if result[offset:offset+len(before)] != before or len(before) != len(after):
            raise ValueError(f'Unexpected software-display preimage at {rva:#x}')
        result[offset:offset+len(before)] = after
        patches.append(dict(rva=hex(rva), before=before.hex(), after=after.hex(), reason=reason))
    if rdp_modes:
        rva, before, after = 0x123D2, b'DDRAW.dll\0', b'DDRDP.dll\0'   # the DDRAW.dll import descriptor's name
        offset = pe.get_offset_from_rva(rva)
        if result[offset:offset+len(before)] != before:
            raise ValueError(f'Unexpected DDRAW.dll import name at {rva:#x}')
        result[offset:offset+len(before)] = after
        patches.append(dict(rva=hex(rva), before=before.hex(), after=after.hex(),
                            reason='RDP test: load the DDRDP.dll mode-list proxy instead of DDRAW.dll (owner 2026-11-01)'))
    if not display_has_mode(800, 600, 32):
        text = pe.sections[0]
        cave, (hook_rva, before, after) = bytes.fromhex(MODE_CAVE), MODE_HOOK
        before, after = bytes.fromhex(before), bytes.fromhex(after)
        hook, at = pe.get_offset_from_rva(hook_rva), text.PointerToRawData + (MODE_CAVE_RVA - text.VirtualAddress)
        size_field = text.get_file_offset() + 8   # IMAGE_SECTION_HEADER.Misc.VirtualSize
        if (text.Name.rstrip(b'\0') != b'.text' or text.Misc_VirtualSize > MODE_CAVE_RVA - text.VirtualAddress
                or MODE_CAVE_RVA + len(cave) > text.VirtualAddress + text.SizeOfRawData
                or any(result[at:at+len(cave)]) or result[hook:hook+len(before)] != before):
            raise ValueError('Unexpected software-display mode-cave preimage')
        result[hook:hook+len(before)] = after
        result[at:at+len(cave)] = cave
        result[size_field:size_field+4] = text.SizeOfRawData.to_bytes(4, 'little')
        patches.append(dict(rva=hex(hook_rva), before=before.hex(), after=after.hex(),
                            reason='display has no 800x600x32 mode: EnumVideoModeCallback -> mode relabel cave'))
        patches.append(dict(rva=hex(MODE_CAVE_RVA), before='00' * len(cave), after=cave.hex(),
                            reason='5120x2880 -> 640x480, 4480x2520 -> 800x600 (windowed: never set as display mode)'))
        patches.append(dict(file_offset=hex(size_field), before=text.Misc_VirtualSize.to_bytes(4, 'little').hex(),
                            after=text.SizeOfRawData.to_bytes(4, 'little').hex(),
                            reason='.text VirtualSize covers the cave'))
    output.parent.mkdir(parents=True, exist_ok=True); output.write_bytes(result)
    manifest = dict(test_only=True, kind='native-d3d7-rgb-software-v1', source=str(source),
                    output=str(output), changed_bytes=sum(a != b for a, b in zip(data, result)),
                    input_sha256=sha(data), output_sha256=sha(result), patches=patches,
                    limitations='Different rendering backend; validates test execution, not hardware/byte equivalence')
    output.with_suffix('.software.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8')
    if source.read_bytes() != data:
        raise RuntimeError('Input changed during software overlay generation')
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=pathlib.Path)
    parser.add_argument('--output', type=pathlib.Path, required=True)
    parser.add_argument('--software-display', action='store_true', help='Also select the 32-bit software test format policy; DLL staging is handled by exe_boot.py')
    args = parser.parse_args()
    print(json.dumps(build_windowed_image(args.input, args.output, args.software_display), indent=2))


if __name__ == '__main__':
    main()
