#!/usr/bin/env bash
# TEST ONLY: a fresh headless Linux machine (Ubuntu 24.04 LTS, x86_64) made ready for the AI benches (README.md
# "Testing": "Benches on a Linux machine"). Run from the checkout's root, as a user with sudo:
#
#   bash tools/setup_linux_bench.sh [-j N]
#
# 1. packages: clang/lld/cmake/ninja (the build), SDL3's development files (its build only: the simulation runs on
#    SDL's dummy video driver, no X or Wayland), Mesa's lavapipe (the Vulkan adapter the renderer still creates on a
#    machine without a GPU), the gh CLI (the data releases);
# 2. the TCP ports: ephemeral ports from 45000 up, below them the benches' fixed pairs (33334 + offset, 64 per slot);
# 3. wgpu-native, SDL3's sources, the fonts (toolchain/), then the build: build/xbuild/linux-x86_64/game/ee-sim;
# 4. the data: the newest release parts into data/ without the 4x textures (gh must be logged in: gh auth login),
#    and the checkout's AI scripts into it;
# 5. a smoke game: seed 2, 5 game minutes, its report in build/sim/smoke.json.
set -euo pipefail
JOBS=$(nproc)
if [ "${1:-}" = "-j" ]; then JOBS=$2; fi
cd "$(dirname "$0")/.."

sudo apt-get update
sudo apt-get install -y --no-install-recommends git python3 clang lld llvm cmake ninja-build pkg-config \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libxfixes-dev libxtst-dev \
  libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev libgl1-mesa-dev libdbus-1-dev \
  mesa-vulkan-drivers libvulkan1 gh
python3 -c 'import sys; assert sys.version_info >= (3, 12), "Python 3.12 or newer (Ubuntu 24.04)"'

# the benches bind 33334/33335 + 2000 + 64 k for slot k (sim_bench.py --port-base); Linux's ephemeral range starts
# at 32768, so an outgoing connection could hold one of them
echo "net.ipv4.ip_local_port_range = 45000 65000" | sudo tee /etc/sysctl.d/60-ee-sim-ports.conf >/dev/null
sudo sysctl -q -p /etc/sysctl.d/60-ee-sim-ports.conf

python3 -I tools/fetch_posix.py wgpu sdl3 fonts
python3 -I tools/xbuild.py sim -j "$JOBS"

gh auth status >/dev/null 2>&1 || { echo "gh is not logged in: gh auth login, then run this again"; exit 1; }
python3 -I tools/fetch_data.py data --without textures-4x
python3 -I tools/open_data.py ai data

mkdir -p build/sim
python3 -I tools/sim.py --data open --slot smoke -- --seed 2 --game-seconds 300 --report build/sim/smoke.json
python3 -c 'import json; r = json.load(open("build/sim/smoke.json")); print("smoke:", r["outcome"], r["game_ms"] // 1000, "game s")'
echo "ready: python3 -I tools/sim_bench.py --name NAME --data open --ai-scripts ai --seeds 2-33 --jobs $(nproc)"
