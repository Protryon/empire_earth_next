# Fix: dead fishing boats in the AI's gatherer lists

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (branch `rust-fishboat`, a declared rebaseline; PORT_LEARNINGS "Dead fishing boats leave the computer player's gatherer lists"): `EECPCivilizationManager::UnitLost` removes a lost gatherer of a citizen category that is no citizen (the fishing boats, the engineer) from the gatherer lists, as CitizenLost does a citizen. The sim oracle is unchanged (`ref` stays: no oracle game loses a boat); the late capture golden's seeds 2, 6 and 7 and `saveload_expect.txt`'s late seed 2 rows are re-recorded (the 1000 s save now parts on the units' active groups, which no save holds). Round 7's gatherer ghosts stay for older saves
