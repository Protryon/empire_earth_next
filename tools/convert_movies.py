"""Convert the game's Bink movies (.bik) to MPEG-1 (.mpg) for the build's movie player (pl_mpeg).

  python -I tools/convert_movies.py <game directory or folder of .bik files> [--ffmpeg PATH] [--quality Q] [--force]

Every .bik under the given directory (the retail game keeps them in Data\\Movies\\; a campaign or scenario may bring
its own) gets an .mpg next to it: MPEG-1 video (mpeg1video, qscale Q, default 3, lower is better; 30 fps, as MPEG-1 allows no 15 fps, the
Bink movies' usual rate) and MP2 audio (224 kbit/s, 44.1 kHz) in an MPEG program stream, the format pl_mpeg plays. The game keeps asking for X.bik; the
player reads X.mpg. Needs ffmpeg (winget install Gyan.FFmpeg); ffmpeg decodes Bink, nothing of it is linked into the
game. Existing .mpg files are kept unless --force.
"""
import argparse
import os
import pathlib
import shutil
import subprocess
import sys


def find_ffmpeg(explicit):
    if explicit:
        return explicit
    found = shutil.which("ffmpeg")
    if found:
        return found
    winget = pathlib.Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    for p in sorted(winget.glob("Gyan.FFmpeg*/**/bin/ffmpeg.exe")) if winget.exists() else []:
        return str(p)
    sys.exit("ffmpeg not found: install it (winget install Gyan.FFmpeg) or pass --ffmpeg")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("directory", type=pathlib.Path)
    ap.add_argument("--ffmpeg")
    ap.add_argument("--quality", type=int, default=3, help="mpeg1video qscale (1 best .. 31)")
    ap.add_argument("--force", action="store_true", help="convert again when the .mpg exists")
    a = ap.parse_args()
    ffmpeg = find_ffmpeg(a.ffmpeg)
    movies = sorted(p for p in a.directory.rglob("*") if p.suffix.lower() == ".bik")
    if not movies:
        sys.exit(f"no .bik files under {a.directory}")
    failed = 0
    for bik in movies:
        mpg = bik.with_suffix(".mpg")
        if mpg.exists() and not a.force:
            print(f"keep     {mpg}")
            continue
        cmd = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-i", str(bik),
               "-c:v", "mpeg1video", "-q:v", str(a.quality), "-r", "30", "-pix_fmt", "yuv420p",
               "-c:a", "mp2", "-b:a", "224k", "-ar", "44100",
               "-f", "mpeg", str(mpg)]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode:
            failed += 1
            mpg.unlink(missing_ok=True)
            print(f"FAILED   {bik}: {r.stderr.strip()[-500:]}")
        else:
            print(f"wrote    {mpg} ({mpg.stat().st_size >> 10} KB)")
    print(f"{len(movies) - failed}/{len(movies)} movies converted or kept")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
