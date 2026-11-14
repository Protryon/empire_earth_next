# D9 open formats only, the data side

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (branch `rust-d9-data`): the db tables are read only as text (no `.dat` fallback, the dead `dbrandommap.dat` load removed); the pointers, palette, volcano, scenario map and campaign background are PNG read by `GESurface::LoadPNG` (`ee_openformats` `png_to_picture`; 50/50 retail pictures and 2,666/2,666 tree PNGs pixel-equal to the `LoadTGA` path); `open_data.py` builds only the open tree (no binary form in it or in the release parts); the gates, the sim tools and the harness use it. Left: the model and texture loaders (another branch)
