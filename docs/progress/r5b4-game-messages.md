# R5b.4 game messages

The lane's state (the game's network messages: `src/Empire Earth/Communications/EEMessage.cpp`, `ee/EEMessage.h`,
the capture `EEMessageCapture.cpp`; crate `ee_netmsg`). Each round rewrites this file to the current state; the
rounds' stories go in PORT_LEARNINGS.md ("R5b.4 game messages, round 1" .. "round 5").

**In Rust (ee_netmsg):**
- the wire format of every game and engine message (classes.rs, engine.rs, codec.rs: export, size, import over the
  C++ members as slots); the box selection's compressed unit list (units.rs);
- the records (records.rs): every one of the 107 factory types and End Cinematic's 0x9d is an `EEMRecord`: its codec
  class's fixed members in wire order in a byte record, its one string (`mString`) and its one list or blob
  (`mList`: the unit ids, the civilization upgrades, the box selection's compressed buffer) beside it, members off
  the wire (the box selection's rectangle and unit type), its flags (paused, sent while out), its
  send-to-all-and-wait member and response rule (Load or Save), its debug label; the capture's sweep shapes (the
  game's and the engine's NMSW/NMNE payloads from the layouts);
- the `Execute` bodies (bodies.rs): the dispatch by type and every body's reads and decisions, over one C++ host
  (EEMessage.cpp's `EENetMsgHost`: `EE_NETMSG_OP_*`, each a world call: an object, a fact, an action); the physics'
  two framings (`ExecuteSelectionMessage`: the fast-selection lock, the local mark, the selection id and copy;
  `ExecuteMessage`: the command's selection id check); the decisions (execute.rs) recorded by the capture's NMDC
  record through the host; the unit orders through the units lane's bodies (ee_units, registered by `ee_rust_init`:
  ee_netmsg cannot depend on ee_units).

**Stays C++:**
- `EEMessage` (the base: the selection id, the codec through `GetCodecSlots`) and `EEMRecord` (the record,
  `Get<T>(i)`/`Set<T>(i)`/`GetList<T>()`, the factory's import, a sender's conversion with lists);
- the host (`EENetMsgHost`, `EENetMsgWorldAct`) and thirteen bodies that are world work with nothing to decide
  (`kNetMsgBodies`: the civilization upgrades, the cargo unload, the improvement, the main screen, the load or save
  request, the cheat class data, the migration slot, the saved group's selection, the flare, the diplomatic stance,
  the start (`EEStartLoading`, the legacy harness's seed pin), the formation);
- `EENewSelectUnits` (the picker, the compression, the fixture's hook), `EESelectUnitsIDs`, `EELoadOrSave`,
  `EENewFullGameOptions`, `EENewFullPlayerInformation`; the farm sorts for the units lane's host;
- the capture (`EEMessageCapture.cpp`: NMIM/NMEX/NMSW/NMDC/NMNE/NMCU).

**Gates:** the oracle capture's NM* digests (equal to golden_cpp.txt with no re-recording since round 3); the sim
oracle; EE-SIM with the Lua AI (`--engine next`: its stack messages execute in every game); `lan_game.py` (the only
gate that encodes game messages on the wire and runs the lobby's and the loading's bodies); the fixture's orders and
box selection exercises (the UI's senders, the selection framing); the legacy tour with `--seed` (the seed pin).

**Not reached by any gate:** the edict votes (no UI sends them), End Cinematic (the cinematic's skip button), the
cheat's unit (the unit hack form, `ai.test_spawn`), the scenario editor's lasso (its rectangle and unit type read
from the record), the civilization builder's upgrades, the collision sphere and collision detection debug messages,
the recorded game's playback (Record Frames, Zoom Info), host migration (Migration Slot Info, Game Restarting).

**Next:** the thirteen C++ bodies are strings, files and screens: they move with their callees (the lobby, the save
files, the chat log). `EEMessage` and `EEMRecord` could become one class (every game message is a record). The
senders' `new EEMRecord(type, {...})` calls could become Rust records built by the senders' lanes.
