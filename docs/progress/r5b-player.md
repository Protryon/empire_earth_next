# R5b player (src/Empire Earth/Player: EEPlayer, its statistics and events)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The player lane, round 1", "The player lane, round 2").

**Rust** (`ee_units`):
- `player_save.rs`, `player_tick.rs` (R5b.4, before this lane): `EeUnitsPlayerSaved` (the saved numbers, a record
  `EEPlayer` derives from), `Load`/`Save` over a host, `UpdatePlayer` and `Reset`'s part of the record (since round 2
  all seven resource words, 0 and 1 included); the queue's save operations; `LoadUnit`/`SaveUnit`.
- `player_stats.rs`, `player_events.rs` (round 1): the statistics' 31 series and the event list, whole.
- `player.rs` (round 1): `EeUnitsPlayerFields` (every other plain member, a record `EEPlayer` derives from; since
  round 2 also `mWonderState`), the queued improvements, the economy and the improvements over one host
  (`EEPlayerHost`).
- `player_units.rs` (round 2): the unit bookkeeping (`AddUnit`, `UnitPlaced`, `UnitTaken`, `UnitCompleted`,
  `RecountPopulation`, `RecountHeroesAndTeleporters`, `GetTotalPopulation`, `GetNumHeroes`) over one unit-info
  operation (`EeUnitsPlayerUnit`); the wonders (`AddWonder`, `RemoveWonder`, the powers: `mWonderTypeCounts` and
  `mWonderPowers` are the Rust `EeUnitsPlayerWonders`); `EEWorld::ApplyShareBonus`/`RemoveShareBonus`
  (`EEPlayerShareBonus`); `ApplyImprovement`; `UpdateFreeUnitPower`.
- `player_diplomacy.rs` (round 2): `SetDiplomaticStance`, `AddSightSharer`, `RemoveSightSharer`, `SetSharedLOS`.
- `player_areas.rs` (round 2): `AddUnitToTown`, `AddUnitToHerd`, `RemoveUnitFromTown`, `RemoveUnitFromHerd`,
  `AssignTowns`, `AssignHerds` (the towns and herds stay C++ objects).

**C++ left** (`EEPlayer.cpp` 3,600 lines with the test capture and the host; `ee/EEPlayer.h`): the
constructors, destructor, `Reset`, `Clear` (groups, upgrades, the tech tree, the air traffic: C++ objects); the
host (`EEPlayerHost`, about 110 operations) and the save's and tick's operations; the selection (`SelectUnit`,
`TrySelectUnit`, `AddUnitsToSelectedGroup`, ..., only a tour or a LAN game runs them); the tile sight
(`AddSharedSight`, `RemoveSharedSight`, ... : per tile, a hot path left in C++); the lookups (`GetNearestTown`,
`GetTownOf`, `GetNearestArea`, `GetTownByID`, ...); `RepairPointers` and its id lists; `ClearFootprint`,
`RefreshUpgrades`, `ReloadTechTree`, `UnitsSwapped`, `RemovalsDone`, `PulseSight`, the camouflage power, the class
media (folded over `EEPlayerClassStats`).

**Next**: the lookups (towns, areas, construction sites) over the area operations; `RepairPointers`' id lists;
`ClearFootprint`; `EEWorld::RecordStatistics` (the post-game scores: 280 lines of decisions over the statistics,
with random draws; no gate reaches it, so a capture of its records first); EEWorld's teams (`AddTeam`,
`AddAlliance`, `RebuildTeams`, `BreakAlliance`: the world lane's file, coordinate).

**Gates**: PLST (each player's state at every `UpdatePlayer`: the counts this round moved), PLEC (every economy and
improvement decision, `GetTotalPopulation`, `GetNumHeroes` and `ApplyImprovement` among them), TCST, the oracle, the
saveload matrix. **Tour only**: the selection; the diplomacy's sight moves and the mini-map (a stance change needs a
diplomacy screen or a scenario; the random-map games never change one); the slavery power's sound.
