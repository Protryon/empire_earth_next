#!/usr/bin/env bash
# The game gates (Windows, x86_64): a tour (menus, settings, 120 s of play, input check) and a seeded 2-AI Hard game
# to the time cap, each on the binary data tree and on the open tree (text db, glTF, PNG).
# Build the test profile first: python -I build.py --test-profile windowed-software
# Logs: build/gates/. Pass: both tours print "STOP <windowed session complete> reached", both AI games end
# "time-cap" with both AI players alive.
cd "$(dirname "$0")/.." || exit 1
P="${PYTHON:-python}"
mkdir -p build/gates
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
echo "== tour, binary tree"; tour build/gates/tour_binary.log
echo "== ai game, binary tree"; ai build/gates/ai_binary.log "ai binary"
export EE_HARNESS_TEXT_DB=1 EE_HARNESS_OPEN_ASSETS=1
echo "== tour, open tree"; tour build/gates/tour_open.log
echo "== ai game, open tree"; ai build/gates/ai_open.log "ai open"
