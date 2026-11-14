# D9 models and textures at run time

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (branch `rust-d9-load`): glTF decoded straight into the model `GEModel::StoreLoaded` stores, PNG into the frames `GETexture::Load` stores; the `.cem` reader/writer, the `.sst`/`.tga` texture reading, `GETexture::Save` and the `.cem`/`.sst` writers deleted. Gates: `storegate.py` (the game's loaders over the whole open tree, 25,140 lines equal to the digests recorded from the pre-D9 loaders), `cppgate` (7,104 decoder cases equal to digests recorded with the old path checked alongside), the sim oracle
