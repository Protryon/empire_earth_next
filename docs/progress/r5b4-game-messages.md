# R5b.4 game messages

The lane's state (the game's network messages: `src/Empire Earth/Communications/EEMessage.cpp`, `ee/EEMessage.h`,
the capture `EEMessageCapture.cpp`; crate `ee_netmsg`). Each round rewrites this file to the current state; the
rounds' stories go in PORT_LEARNINGS.md ("R5b.4 game messages, round 1" .. "round 4").

**In Rust (ee_netmsg):** the wire format of every game and engine message (classes.rs, engine.rs, codec.rs: export,
size, import over the C++ members as slots); the box selection's compressed unit list (units.rs); the Execute
bodies' decisions (execute.rs: the selection id checks, the resources, the tribute, the trade, the disband refund);
the record types (records.rs): 100 of the 108 game message types have no C++ class of their own: one class,
`EEMRecord`, carries their fixed members in wire order in a byte record and their one string (if any) beside it;
their layout, the two flags (paused, may be sent while out) and the debug label are the Rust table's, by codec class
or (for a class that several types share, or 0x9d) by type; the engine messages likewise (ne_records.rs, `NEMRecord`,
the network engine lane's).

**Stays C++:**
- `EEMessage` (the base: the selection id, the codec through `GetCodecSlots`, `ExecuteMessage`/`ExecuteSelectionMessage`)
  and `EEMRecord` (the record, `Get<T>(i)`/`Set<T>(i)` by wire index, the factory's import, a sender's conversion).
- The record types' `Execute` bodies, as static functions over the record in a table by type: each body is world
  calls (players, units, the UI, the lobby, the scenario manager); the decisions in them are Rust already
  (execute.rs) or the units lane's (ee_units: path_to, attack_unit, build_unit, rally, patrol, ...).
- The eight types with a class: `EEMSelectUnits` and its two subclasses (the compressed blob and the decoded id
  list the scenario editor reads), `EEMSelectTheseUnits` (an id list without codec slots), `EEMCivUpgrades` and
  its two subclasses (a record list and a `SetName` virtual), `EEMLoadOrSaveGame` (`IsResponse`, the
  send-to-all-and-wait id, `LoadOrSave` from the physics), `EEMStartLoading` (the legacy debugger harness breaks at
  `?Execute@EEMStartLoading@@UEAAXXZ` and writes its `mSeed` from the PDB layout: build.py's HARNESS_HOOKS).
- The capture (`EEMessageCapture.cpp`: NMIM/NMEX/NMSW/NMDC/NMNE/NMCU) reads every message through its codec slots.

**Gates:** the oracle capture's NM* digests (equal to golden_cpp.txt with no re-recording since round 3); the sim
oracle; EE-SIM with the Lua AI (`--engine next`, the saveload matrix's `next` setting: its stack messages execute
in every game); `lan_game.py` (the only gate that encodes game messages on the wire: the lobby's options, slots and
chat, the start, the loading responses); the fixture's orders and box selection exercises (the UI's senders).

**Not reached by any gate:** the edict votes (EdictProposal/Vote/NewEdictEnacted: no UI sends them), End Cinematic
(the cinematic's skip button), the cheat's unit (HackANewUnit: the unit hack form and `ai.test_spawn`), the scenario
editor's click messages (the editor gate covers some), the civilization builder's upgrades.

**Next:** the eight classes above: `EEMSelectTheseUnits` and `EEMCivUpgrades*` as records need a record list kind in
`EEMRecord` (an id list, `EECivUpgrade`s); `EEMSelectUnits*` a blob kind and the decoded list (its constructor picks
the units: EEUnitPicker); `EEMLoadOrSaveGame`'s response matching as a record rule (the engine lane's `NEMRecord`
has the same shape). Then the Execute table itself: the bodies are world glue, so moving the switch to Rust only pays
with the bodies' world calls behind one host (as the units lane's EEUnitHost).
