"""Builds the Android APK without Gradle: javac, d8, aapt2, zip, zipalign, apksigner.

  python -I tools/android_apk.py                 -> build/android/ee-aoc.apk (debug-signed, game data bundled)
  python -I tools/android_apk.py --data DIR      the Data folder to bundle (default data/)
  python -I tools/android_apk.py --no-data       code only (quick builds; the data must then be pushed by hand)

The bundled data is the game's loose Data folder without the player's saved games, packed as assets/data.zip; the
launcher activity unpacks it into the app's external files folder on the first run. It is retail-derived content:
keep the APK for your own devices.

Needs: tools/xbuild.py game --target android-arm64 (libmain.so), toolchain/android-sdk (cmdline-tools'
sdkmanager: build-tools;34.0.0 platforms;android-34), toolchain/android-ndk-r27c (llvm-strip), a JDK.
SDL's Java glue (zlib licence) comes from toolchain/src/SDL3-<ver>/android-project; the activity and the touch
controls are android/java.
"""
import argparse
import hashlib
import pathlib
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
TC = ROOT / "toolchain"
SDK = TC / "android-sdk"
BT = SDK / "build-tools" / "35.0.0"
JAR = SDK / "platforms" / "android-34" / "android.jar"
NDK_BIN = TC / "android-ndk-r27c" / "toolchains" / "llvm" / "prebuilt" / "windows-x86_64" / "bin"
SDL_JAVA = TC / "src" / "SDL3-3.4.18" / "android-project" / "app" / "src" / "main" / "java"
OUT = ROOT / "build" / "android"
LIB = ROOT / "build" / "xbuild" / "android-arm64" / "game" / "libmain.so"
KEYSTORE = TC / "android-debug.keystore"
DATA_CACHE = ROOT / "build" / "android-data"
DEFAULT_DATA = ROOT / "data"
SKIP_TOP = ("Saved Games",)   # the player's saves (and their copies) never go into an APK


def data_files(data):
    for p in sorted(data.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(data)
        if rel.parts[0].startswith(SKIP_TOP):
            continue
        yield p, rel


def pack_data(data):
    """-> (data.zip path, version string, uncompressed bytes); rebuilt only when a file changed."""
    files = list(data_files(data))
    sig = hashlib.sha1()
    total = 0
    for p, rel in files:
        st = p.stat()
        sig.update(f"{rel.as_posix()}|{st.st_size}|{st.st_mtime_ns};".encode())
        total += st.st_size
    version = sig.hexdigest()[:16]
    DATA_CACHE.mkdir(parents=True, exist_ok=True)
    out = DATA_CACHE / f"data-{version}.zip"
    if not out.exists():
        for old in DATA_CACHE.glob("data-*.zip"):
            old.unlink()
        print(f"packing {len(files)} data files ({total // (1 << 20)} MB) ...", flush=True)
        tmp = out.with_suffix(".tmp")
        with zipfile.ZipFile(tmp, "w", zipfile.ZIP_DEFLATED, compresslevel=1) as z:
            for p, rel in files:
                z.write(p, "Data/" + rel.as_posix())
        tmp.rename(out)
    return out, version, total


def run(cmd, **kw):
    print("  " + " ".join(str(c) for c in cmd)[:220], flush=True)
    subprocess.run([str(c) for c in cmd], check=True, **kw)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", type=pathlib.Path, default=DEFAULT_DATA)
    ap.add_argument("--no-data", action="store_true")
    args = ap.parse_args()
    bundle = None
    if not args.no_data:
        if not args.data.is_dir():
            sys.exit(f"no Data folder at {args.data} (--data DIR, or --no-data)")
        bundle = pack_data(args.data)
    for p in (BT, JAR, LIB, SDL_JAVA):
        if not p.exists():
            sys.exit(f"missing {p}")
    if OUT.exists():
        shutil.rmtree(OUT)
    (OUT / "classes").mkdir(parents=True)
    sources = [*SDL_JAVA.rglob("*.java"), *(ROOT / "android" / "java").rglob("*.java")]
    run(["javac", "--release", "8", "-encoding", "UTF-8", "-Xlint:-options", "-classpath", JAR, "-d", OUT / "classes",
         *sources])
    classes = [str(p) for p in (OUT / "classes").rglob("*.class")]
    (OUT / "dex").mkdir()
    run([BT / "d8.bat", "--release", "--min-api", "26", "--lib", JAR, "--output", OUT / "dex", *classes])
    base = OUT / "base.apk"
    run([BT / "aapt2.exe", "link", "-o", base, "-I", JAR, "--manifest", ROOT / "android" / "AndroidManifest.xml",
         "--min-sdk-version", "26", "--target-sdk-version", "34"])
    stripped = OUT / "libmain.so"
    run([NDK_BIN / "llvm-strip.exe", "--strip-all", "-o", stripped, LIB])
    unaligned = OUT / "unaligned.apk"
    with zipfile.ZipFile(base) as src, zipfile.ZipFile(unaligned, "w", zipfile.ZIP_DEFLATED) as dst:
        for item in src.infolist():
            dst.writestr(item, src.read(item.filename))
        dst.write(OUT / "dex" / "classes.dex", "classes.dex")
        dst.write(stripped, "lib/arm64-v8a/libmain.so")
        cfg = ROOT / "android" / "assets" / "config.yaml"
        if cfg.exists():
            dst.write(cfg, "assets/config.yaml")
        ctl = ROOT / "android" / "assets" / "controls-default.json"   # the touch controls of a first run
        if ctl.exists():
            dst.write(ctl, "assets/controls-default.json")
        if bundle:   # one stored asset: it is already compressed, and the launcher streams it
            zpath, version, total = bundle
            dst.write(zpath, "assets/data.zip", compress_type=zipfile.ZIP_STORED)
            dst.writestr("assets/data.version", version)
            dst.writestr("assets/data.size", str(total))
    aligned = OUT / "aligned.apk"
    run([BT / "zipalign.exe", "-f", "-p", "4", unaligned, aligned])
    if not KEYSTORE.exists():   # a local debug key, only for sideloading onto the developer's own phone
        run(["keytool", "-genkeypair", "-keystore", KEYSTORE, "-alias", "debug", "-keyalg", "RSA", "-keysize", "2048",
             "-validity", "10000", "-storepass", "android", "-keypass", "android",
             "-dname", "CN=Android Debug,O=Android,C=US"])
    apk = OUT / "ee-aoc.apk"
    run([BT / "apksigner.bat", "sign", "--ks", KEYSTORE, "--ks-pass", "pass:android", "--key-pass", "pass:android",
         "--out", apk, aligned])
    print(f"built {apk} ({apk.stat().st_size // 1024} KB)")


if __name__ == "__main__":
    main()
