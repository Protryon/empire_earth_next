# R5b player (src/Empire Earth/Player: EEPlayer, its statistics and events)

The lane's state. Each round rewrites this file to the current state; the round's story goes in PORT_LEARNINGS.md
("The player lane, round 1").

**Rust** (`ee_units`):
- `player_save.rs`, `player_tick.rs` (R5b.4, before this lane): `EeUnitsPlayerSaved` (the saved numbers, a record
  `EEPlayer` derives from), `Load`/`Save` over a host, `UpdatePlayer` and `Reset`'s part of the record. Since round 1
  the queue's save operations are answered in Rust and the tick walks the Rust queue; `LoadUnit`/`SaveUnit`
  (`load_unit`, `save_unit`) over the save's primitives and five unit operations.
- `player_stats.rs` (round 1): `EEPlayerStatistics`' 31 series as a Rust object (`mStats`): `Set`/`Reset`/`Lower`,
  `GetValue`/`GetLastValue`/`CountIncreases`, the copy, `CompactEpochHistory`, `Load`/`Save`; `GetSeries` copies.
- `player_events.rs` (round 1): the event list whole (`EEPPlayerEvent` is gone): the events, the current one, the next
  index, the ping, every decision, `Load`/`Save`; the C++ list keeps its alert sounds and answers the world's parts.
- `player.rs` (round 1): `EeUnitsPlayerFields` (every other plain member: `mHackUnitClass` .. `mJITManufacturing`,
  `mNumTeleporters`; a record `EEPlayer` derives from), the queued improvements (`mQueued`: a Rust map), and over one
  host (`EEPlayerHost`): `CanProduce`, `CanAfford`, `CanAffordScaled`, `CanAffordSingleResource`,
  `CanAffordTechnology`, `Pay`, `PayImprovement`, `RefundTechnology`, `RefundImprovement`, `GetImprovementLevel`,
  `CountQueuedImprovements`, `CanImprove`, `CanBuy`, `IsImprovementMaxed`, `QueueImprovement`,
  `RemoveQueuedImprovement`, `AdvanceImprovement`, `Erase`/`MoveQueuedImprovements`, the portrait's improvement and
  progress, `RepairPointers`' requeue, `Reset`'s member stores.

**C++ left** (`EEPlayer.cpp` about 3,850 lines with the 160-line test capture; `EEPPlayerEventList.cpp` 150;
`EEPlayerStatistics.cpp` 140): the constructors, destructor, `Reset`, `Clear` (groups, upgrades, the tech tree, the
air traffic: C++ objects); the unit bookkeeping (`AddUnit`, `UnitPlaced`, `UnitTaken`, `UnitCompleted`,
`RecountPopulation`, `RecountHeroesAndTeleporters`, `GetTotalPopulation`: world reads per unit); the towns and herds
(`AddUnitToTown`, `AddUnitToHerd`, `AssignTowns`, ...); the selection; diplomacy (`SetDiplomaticStance`, the sight
sharers and allies); the wonder powers (`mWonderPowers` is read by EEWorld.cpp); the class media; `RepairPointers`;
the save's host (`EEPlayerSaveOperation`) and the tick's; `ApplyImprovement` (the units' rescale).

**Next**: the unit bookkeeping's counts over one unit-info host (the PLST record pins them); `SetDiplomaticStance`'s
decisions; the wonder powers once EEWorld reads them through an accessor; `RepairPointers`' id lists.

**Gates**: PLST (each player's state at every `UpdatePlayer`: the statistics but the local player's reported series
0x14/0x15, the events, the queue, the plain members as digests, the resources 2..6 and counts) and PLEC (every economy
and improvement decision on the simulation thread with its arguments, result and the resources after), recorded from
the unmodified C++ at seeds 2-3 of the oracle, late, space and calamity profiles; TCST (the nodes' `GetCost`, in order);
the oracle; the saveload matrix (the statistics', events', queue's and units' saves).
**Tour only**: the events' sounds, mini-map ping and chat text (the local player's), the portrait's improvement
progress, the reason texts of `CanProduce`/`CanBuy` (the command buttons' tooltips).
