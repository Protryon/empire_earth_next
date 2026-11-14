# R5a.3 the renderer reads a snapshot

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

**Done for units, players, the map and the camera** (branch `rust-r5a-snap`; PORT_LEARNINGS "R5a.3", with the measured read table): FrameStarting under the existing read lock is the capture (`ee/EERenderSnapshot.h`, the render lists, the effects' `Queue`); after it the render phase reads no unit, player or physics state, and the unit picker holds ids, so the graphics thread takes no unit references. Left: the effects' own state (stepped by the tick while drawn), the terrain mesh's heights, the see-through flags written in the capture, the UI's CopyData. Sim oracle identical; no tour run
