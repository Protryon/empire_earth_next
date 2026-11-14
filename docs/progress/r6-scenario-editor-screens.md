# R6 scenario editor screens

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md.

**Started** (`ee_screens`, branch `rust-editor1`; PORT_LEARNINGS "R6: the scenario editor screens, round 1"):
`src/Empire Earth/Scenario Editor` (46 units, 23.3k lines) measured per unit (non-forwarder body lines, what the
editor gate reaches: 8,310 of 12,871); the editor gate dumps every control of the editor form and the players
(`form`, `players`; the load thread drained first; 5,560 lines); the player tab whole in Rust
(`editor_players.rs`: `EESEPlayerScreen` with its player rows and game options page, each control a record the
host `ee_screenscpp_sepl` makes; net -1,321 C++ lines). Next: the editor form (`EEUIFScenarioEditor`),
`EESETSObject`, `EESEUnit`, `EESETSETPlayer`, `EESETSTrigger`, sharing the control records.
