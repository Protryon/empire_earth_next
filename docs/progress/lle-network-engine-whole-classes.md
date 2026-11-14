# LLE Network Engine whole classes

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

**Started** (`ee_net`, `ee_netmsg` ne_records.rs; branches `rust-ne1` to `rust-ne7`; PORT_LEARNINGS "LLE Network
Engine, round 1" (with the census of every NE class) to "round 7"):

- whole in Rust: `NEMessageManager` (msgmgr.rs), `NELagLossSimulator`, `NERecordPlayback`, `NETimestampedMessage`
  (lagloss.rs, recplay.rs), `NESynchronousClient` (client.rs), `NESynchronousServer` (server.rs, tune.rs),
  NESynchronousCommunications' state (comms.rs), `NEProtocol` (ffi_protocol.rs, round 7);
- deleted: `NEPlayer`, `NEProtocolConfiguration`, the `NEConnection`/`NEServer` wrappers (the plug-in's handles),
  NEProtocol.cpp, the queue host's clock/event/thread ops and the ten thread trampolines (round 7);
- round 6: the 28 engine messages are one data class, `NEMRecord` (lle/NEMessage.h; layout and rules ee_netmsg
  `ne_records.rs`); the factory's table is Rust's.
- **round 7:** `NEProtocol` whole (the registry, the counters, every member over the TCP plug-in's C ABI; the C++
  class keeps the members the game calls as forwarders over the Rust object). The game's server and client answer
  their protocol, clock, event, thread and message-manager-send ops themselves (the `tcp` flag; native.rs); the
  nettest worlds still answer them (flag off), so their goldens kept those lines. The server's and client's message
  ops are NEMRecord member indexes (`MsgGet` -> `NEMRecord::Value`) and records by type (`MsgNew` ->
  `new NEMRecord(type, values, count)`): each answer is one line.

Net C++ round 1 -1,265, round 2 -963, round 3 -544, round 4 -918, round 5 -282, round 6 about -1,600, round 7 -573
(see PORT_LEARNINGS "LLE Network Engine, round 7"). The Network Engine's C++ (src + include/lle NE*) is 1,671 lines
after the merge of rust (dead code round 2 took 34 of them).

**Left** (thin by now; the rest is the game's types and its virtual seams):
- NEMessage's header (four bytes on `UReferenceCounter`; the game subclasses it) and NEMRecord's codec glue
  (CodecSlots, Export/Import: the nested inputs are C++ messages exported through their virtuals);
- the server's and client's member forwarders (the game holds the classes and overrides their virtuals) and their
  hosts: the virtual members, the message virtuals (type, flags, slots, export, references), the transfer files
  (FSFileSpec/FSFile), the message manager's NewResponse (it reads the base class's fields; moving it re-records the
  message manager's and server's goldens);
- NESynchronousCommunications' queue host: Verify, the FPU control, the message virtuals, the record files;
- untested by any gate: the game's refusal branches (EEServer::ProcessConnectionRequest's version, checksum, closed
  game, name and full-game refusals; EEClient's leave paths) and a server migration in a real game. Their NE side is
  in the goldens (refused connection responses and reconnection responses in the client's, reconnection requests and
  the reconnect wait in the server's); a driver over EEServer/EEClient needs the game's options, world, UI and lobby.
