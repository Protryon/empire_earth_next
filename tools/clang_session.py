"""TEST ONLY: run a clang-built EE-AOC.exe through the ee repository's headless live-game harness.

  python -I tools/clang_session.py [--image build/clang-test/exe/EE-AOC.exe] [game_session.py options ...]

e.g.  python -I tools/clang_session.py --exercise tour --timeout 300000
      python -I tools/clang_session.py --engine build/clang/lle/"Low-Level Engine.dll" --exercise tour

The harness (ee: tools/boot/game_session.py, env EE_REPO, default ../ee) stages a full-link exe by locating the
original image's windowed/software overlay sites by map name and proving each function body against the original
instruction by instruction (tools/boot/fulllink_locator.py). Clang's code differs everywhere, so that proof cannot
apply. The same test policy is built from source instead (tools/clang_link.py --test-profile windowed-software), and
this wrapper replaces only the locator step: it copies that test image into the stage and writes the locator's
manifest, with the telemetry globals and the EEInitializeSounds entry taken from the image's map by name. Everything
else (staging, retail DLLs and their test overlays, registry handling, exercises, evidence) is the harness's own.
"""
import hashlib
import json
import os
import pathlib
import shutil
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
EE = pathlib.Path(os.environ.get("EE_REPO", ROOT.parent / "ee")).resolve()
BOOT_TOOLS = EE / "tools" / "boot"
DEFAULT_IMAGE = ROOT / "build" / "clang-test" / "exe" / "EE-AOC.exe"
BASE = 0x400000
AUDIO_ENTRY = "?EEInitializeSounds@@YA_NXZ"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    argv = sys.argv[1:]
    image = DEFAULT_IMAGE
    if "--image" in argv:
        k = argv.index("--image")
        image = pathlib.Path(argv[k + 1])
        del argv[k:k + 2]
    image = image.resolve()
    if not image.exists():
        sys.exit(f"{image} is missing: python -I tools/clang_link.py --test-profile windowed-software lle exe")
    manifest_src = json.loads((image.parent / "test_profile.json").read_text()) if \
        (image.parent / "test_profile.json").exists() else None
    if not BOOT_TOOLS.exists():
        sys.exit(f"{BOOT_TOOLS} is missing (set EE_REPO to the ee checkout)")
    sys.path.insert(0, str(BOOT_TOOLS))
    sys.path.insert(1, str(EE / "tools"))
    os.chdir(EE)
    import fulllink_locator
    import windowed_overlay

    def build_prebuilt(source, output, map_path, software_display=False, hotfixes=()):
        source, output = pathlib.Path(source).resolve(), pathlib.Path(output).resolve()
        windowed_overlay.test_output(source, output)
        if hotfixes:
            raise ValueError("--hotfix needs the original-backed locator; not available for a clang image")
        if not software_display:
            raise ValueError("the clang test image is built with the software-display profile only")
        lm = fulllink_locator.LinkMap(map_path)
        data = source.read_bytes()
        output.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, output)
        globals_ = {name: dict(original_va=hex(va), va=hex(lm.address(name)))
                    for va, name in fulllink_locator.GLOBAL_NAMES.items()}
        manifest = dict(test_only=True, kind="clang-test-profile-prebuilt-v1", source=str(source), output=str(output),
                        map=str(pathlib.Path(map_path).resolve()), map_sha256=sha(pathlib.Path(map_path).read_bytes()),
                        input_sha256=sha(data), output_sha256=sha(data), changed_bytes=0,
                        software_display=software_display, patches=[], functions=[],
                        audio_entry_rva=hex(lm.address(AUDIO_ENTRY) - BASE), globals=globals_,
                        proof="none needed: the test policy is compiled in (empire_earth_next tools/clang_link.py "
                              "--test-profile windowed-software); the image is copied unchanged",
                        test_profile=manifest_src)
        output.with_suffix(".windowed.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        print(f"CLANG test image staged {source} (globals and audio entry from {pathlib.Path(map_path).name})",
              flush=True)
        return manifest

    fulllink_locator.build_fulllink_windowed_image = build_prebuilt

    # --engine: the harness requires every original engine export. A clang engine lacks exactly two kinds that no
    # retail or clang image imports (tools/clang_link.py clang_def): pooled string literals (??_C@) and a default
    # constructor closure (??_F). Anything else missing still refuses.
    import presentation_bridge
    inner_names = presentation_bridge.exported_names

    def exported_names(path):
        names = inner_names(path)
        if pathlib.Path(path).resolve().is_relative_to(EE / "orig-bin"):
            names = {n for n in names if not (n.startswith(b"??_C@") or n.startswith(b"??_F"))}
        return names
    presentation_bridge.exported_names = exported_names
    sys.argv = [str(BOOT_TOOLS / "game_session.py"), "--fulllink", str(image), *argv]
    import game_session
    game_session.main()


if __name__ == "__main__":
    main()
