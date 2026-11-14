# R3.6 Sound Engine

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Logic done (`ee_sound`). `SESoundManager`: instance selection, limits, priorities, the paused queue, the scavenger's decisions, one-shots, random offsets (142 C++ golden scenarios). `SEConductor`: the score table and every decision (conductor.rs, 2,590 golden ops). `EEMusicManager`: the music thread's step, song changes, fades, intensity rules, timers (music.rs, 80 golden thread runs). `SEMixer`/`Sound Engine.cpp`: the mixers' registry, `UseMixer`, the start's plug-in choice, the shutdown (mixers.rs, 60 golden scenarios). C++ keeps the objects: nodes, samples, threads, events, locks, dmusic's loader/performance and `SEScore` (no decisions), the SPIM entry points, `sActiveMixer`. MCI CD audio (`SECD`) is dead on this port (never initialised) and not ported; DirectMusic is gone, the vendored dmusic stays. The positional sound (`EEComputeSoundParameters`) and the sound triggers are game code, mapped in PORT_LEARNINGS for the simulation lanes. No audio has been listened to
