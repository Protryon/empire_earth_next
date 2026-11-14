# R1.1-R1.3 platform leaves, files, threads, sockets

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (`ee_plat_core`, `ee_plat_sys`). Windows PlatString still calls the CRT, whose behaviour depends on the locale. Main's later fixes to the deleted C++ (the settings file's replace retried, the simulation never shows a message box, POSIX stream sockets with `SO_REUSEADDR`) were ported into these crates at the merge of main 79be8619
