# R5b.4 game messages

The lane's state (the game's network messages and, from round 6, the game's communications: `src/Empire
Earth/Communications/` EEMessage.cpp, EEMRecord.cpp, EEServer.cpp, EEClient.cpp, the capture EEMessageCapture.cpp;
crate `ee_netmsg`). Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("R5b.4 game messages, round 1" .. "round 6").

**In Rust (ee_netmsg):**
- the wire format of every game and engine message (classes.rs, engine.rs, codec.rs), the box selection's
  compressed unit list (units.rs);
- the records (records.rs): every game message type is an `EEMRecord` (layout, flags, rules, labels, the capture's
  sweep shapes); the engine messages' `NEMRecord` (ne_records.rs, the Network Engine lane's);
- the `Execute` bodies and the dispatch (bodies.rs, execute.rs) over EEMessage.cpp's host;
- round 6: the game's lock-step server and client (session.rs) over one host in EEServer.cpp (`EESessionHost`):
  the join request's refusals and acceptance, the boots and removals (server and client), the system messages
  (ping answers, the communications ticks' inputs, disconnections, reconnections), the filtered inputs (votes,
  pause, the host turned client), the local inputs' selection ids, the lobby inputs (player info, transfer status,
  the game start, slot info), the edict votes, the laggers, the slot info, the host migration's restart, the
  client's connection states, answers to the server and lost server;
- round 6: the capture's sweep payloads (capture.rs: NMSW, NMCU, NMNE, in the old order and random stream).

**Stays C++:**
- `EEMessage` and `EEMRecord` (EEMRecord.cpp since round 6: the codec over the slots, the record's members, the two
  whole-object senders); the factory, the Execute host and thirteen bodies (EEMessage.cpp, `kNetMsgBodies`);
  `EESendGroupCommand` (moved unchanged into EEMessage.cpp beside its callers);
- EEServer/EEClient: the classes, constructors, the engine's virtuals as one-line forwarders (`EESession`), the
  connection response senders, `TakeConnectionRequest`, `RefuseClient`, the small getters, `SendFullPlayerInformation`,
  `SendPingStat`, `HandleXferStatus`, `HandleSlotUpdate`, `CreateXferFile` (the transfer's file), the timeout warning;
  the host (one operation per world, lobby, UI or engine call);
- the capture's records (NMIM/NMEX/NMSW members, NMNE members, NMDC and its decision tables);
- EEWonNet.cpp (the game's side of WON): not touched; the WON lanes own the classes it calls.

**Goldens and gates:** `ee_session_test` (xbuild nettest; the server and client over a stand-in world, 6,000 random
calls, golden tests/net/golden_session.txt made by the original units, kept as `ee_session_orig` over
tests/net/orig_ee); the oracle capture's NM* digests (golden_cpp.txt, no re-recording); the sim oracle; EE-SIM with
the Lua AI; `lan_game.py` (the only gate that runs the wire: the connection's acceptance, the lobby, the loading, the
start).

**Not reached by any game gate (only the session golden):** the refusals (version, checksum, game closed, name in
use, game full) and the client's answers to them, the boots, the host's and a client's removal (booted, out of
sync), the edict votes, the laggers' mask, the slot info, the host migration (reconnect wait, restart, the client's
lost server and migration action), the host turned client. Not reached at all: End Cinematic, the cheat unit, the
scenario editor's lasso, the civilization builder's upgrades, the collision debug messages, playback.

**Next:** the session host's operations could shrink as the lobby (WONLobby lanes) and the UI move: most are one
call each. `EEMessage` and `EEMRecord` as one class. The thirteen C++ bodies with their callees. EEWonNet.cpp after
the WON lanes' rounds (it calls SmartRoutingConnection, AsyncOp and the lobby's windows).
