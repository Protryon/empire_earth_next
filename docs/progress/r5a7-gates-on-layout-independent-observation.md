# R5a.7 gates on layout-independent observation

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

**Done, gate runs pending** (branch `rust-r5a-gates`): `tools/gates.sh` runs `fixture_game.py` (the game's UI fixture, its telemetry, stderr verdicts and game log; window captures; no layout, mangled name, debugger or import hook); the harness clock is `PlatSimSetTimeFactor` in `ee_plat_core` (clocks F x, sleeps/timers/wait timeouts 1/F); `--legacy` keeps `game_session.py`. Still on the layout: exe_boot.c's guards and hooks, the RNG trace, the human driver, `differential/`
