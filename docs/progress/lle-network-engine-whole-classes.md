# LLE Network Engine whole classes

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

**Started** (`ee_net`, `ee_netmsg` ne_records.rs; branches `rust-ne1` to `rust-ne6`; PORT_LEARNINGS "LLE Network
Engine, round 1" (with the census of every NE class) to "round 6"):

- whole in Rust: `NEMessageManager` (msgmgr.rs), `NELagLossSimulator`, `NERecordPlayback`, `NETimestampedMessage`
  (lagloss.rs, recplay.rs), `NESynchronousClient` (client.rs), `NESynchronousServer` (server.rs, tune.rs);
  NESynchronousCommunications' state (comms.rs); NEProtocol calls the Rust TCP plug-in directly;
- deleted: `NEPlayer`, `NEProtocolConfiguration`, the `NEConnection`/`NEServer` wrappers (the plug-in's handles);
- **round 6:** the 28 engine messages (`NEMAddPlayer` .. `NEMXferStatus`, `NEMPlayerInput`) are one data class,
  `NEMRecord` (lle/NEMessage.h): the fixed members packed in wire order; layout, flags, send-to-all-and-wait member,
  response rule and label from ee_netmsg `ne_records.rs`. The factory's table (`sConstructors[256]`) is Rust's
  (`ee_netmsg_register_builder` / `ee_netmsg_build`); EEMessage registers with `NEMessage::RegisterBuilder`. Every
  sender and reader (EEServer, EEClient, EEPhysics, the lobby, the actions, the capture, the server's and client's
  hosts) uses the record. Golden: `ne_message_test` (tests/net/golden_message.txt, made by the 28 classes).

Net C++ round 1 -1,265, round 2 -963, round 3 -544, round 4 -918, round 5 -282, round 6 about -1,600 (see
PORT_LEARNINGS "LLE Network Engine, round 6").

**Left:** NEMessage's header (four bytes on `UReferenceCounter`; the game subclasses it); NEProtocol (thin forwarders
to ee_net_tcp plus the byte counters: the Rust server and client reach it through their hosts' protocol ops, which
the goldens' worlds answer, so calling ee_net_tcp from Rust needs those ops kept as a seam); the server's and
client's hosts (their typed message ops could become field indexes into `NEMRecord`, which re-records the cargo
goldens); NESynchronousCommunications' queue host.
