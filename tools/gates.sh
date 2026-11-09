#!/usr/bin/env bash
# The game gates (Windows, x86_64): a tour (main menu, settings tabs, a random map, 120 s of play, input check) and a
# seeded 2-AI Hard game to the time cap, each on the binary data tree and on the open tree (text db, glTF, PNG).
# Build the test profile first: python -I build.py --test-profile windowed-software
#
# Default: the fixture driver (tools/harness/fixture_game.py): the game's own UI fixture and telemetry, its game log,
# the harness clock through PlatSim. It reads no memory by offset and hooks no symbol or import.
# Pass: each tour prints "RESULT PASS tour"; each AI game "RESULT PASS ai-game outcome time-cap" with both computer
# players alive (alive_computers [2, 3]); the script ends "GATES PASS".
#
# --legacy: the debugger harness (game_session.py: record layouts, mangled names, the time shim on the Win32
# imports), kept until it is retired. Pass: "STOP <windowed session complete> reached", outcome time-cap.
#
# Trees: EE_GATES_TREE_BINARY and EE_GATES_TREE_OPEN name them (no spaces in the path); else
# build/open_data/tree-binary and tree, built when missing. Logs: build/gates/.
cd "$(dirname "$0")/.." || exit 1
P="${PYTHON:-python}"
mkdir -p build/gates
BINARY="--tree binary"; OPEN="--tree open"
if [ -n "$EE_GATES_TREE_BINARY" ]; then BINARY="--data $EE_GATES_TREE_BINARY"; fi
if [ -n "$EE_GATES_TREE_OPEN" ]; then OPEN="--data $EE_GATES_TREE_OPEN"; fi

if [ "$1" = "--legacy" ]; then
  ai() {
    "$P" -I tools/harness/game_session.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard \
        --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40 --timeout 2400000 > "$1" 2>&1
    grep -E "^data:" "$1"
    E=$(grep -o "EVIDENCE .*" "$1" | cut -d' ' -f2)
    "$P" -I -c "import json,sys;s=json.load(open(sys.argv[1]))['summary'];print(sys.argv[2],s['outcome'],s['final_pt'],[p.get('units_lb14') for p in s['players']])" \
        "$E/ai_game.json" "$2" 2>&1 | tail -1
  }
  tour() {
    "$P" -I tools/harness/game_session.py --exercise tour --input-check --play-seconds 120 --timeout 900000 > "$1" 2>&1
    grep -E "^data:|STOP|FAILED|Error" "$1"
  }
  echo "== tour, binary tree (legacy)"; tour build/gates/tour_binary.log
  echo "== ai game, binary tree (legacy)"; ai build/gates/ai_binary.log "ai binary"
  export EE_HARNESS_TEXT_DB=1 EE_HARNESS_OPEN_ASSETS=1
  if [ -n "$EE_GATES_TREE_OPEN" ]; then export EE_HARNESS_OPEN_DATA="$EE_GATES_TREE_OPEN"; fi
  echo "== tour, open tree (legacy)"; tour build/gates/tour_open.log
  echo "== ai game, open tree (legacy)"; ai build/gates/ai_open.log "ai open"
  exit 0
fi

FAILED=0
tour() {   # log, tree
  "$P" -I tools/harness/fixture_game.py --exercise tour --input-check --play-seconds 120 $2 --timeout 900 \
      > "$1" 2>&1
  grep -E "^data:|INPUT CHECK|RENDER CHECK|^RESULT" "$1"
  grep -q "^RESULT PASS tour" "$1" || FAILED=1
}
ai() {   # log, tree
  "$P" -I tools/harness/fixture_game.py --exercise ai-game --ai-players 2 --map-size Small --difficulty Hard \
      --max-game-seconds 600 --time-factor 32 --no-render --seed 2 --pt-rate 40 $2 --timeout 2400 > "$1" 2>&1
  grep -E "^data:|^RESULT" "$1"
  grep -qE "^RESULT PASS ai-game outcome time-cap .*alive_computers \[2, 3\]" "$1" || FAILED=1
}
echo "== tour, binary tree"; tour build/gates/tour_binary.log "$BINARY"
echo "== ai game, binary tree"; ai build/gates/ai_binary.log "$BINARY"
echo "== tour, open tree"; tour build/gates/tour_open.log "$OPEN"
echo "== ai game, open tree"; ai build/gates/ai_open.log "$OPEN"
if [ "$FAILED" = 0 ]; then echo "GATES PASS"; else echo "GATES FAIL"; exit 1; fi
