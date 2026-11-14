# Merge of main 79be8619 (the Lua AI)

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (branch `rust-mainmerge`, a declared rebaseline; PORT_LEARNINGS "The merge of main 79be8619"): main's classic-AI change of 32 coast fishing boats (AI_RESEARCH 8.56) parts seeds 2 and 3 of the oracle (CT 2881 and 2503); the merged build with the old 16 equals `ref` on seeds 2-7. New references from the merged build: the oracle run, the capture goldens of oracle and missile seeds 2-3 and late seeds 4-7 (late also by main's boat count fix); space, calamity and `saveload_expect.txt` unchanged. Save version 0.280
