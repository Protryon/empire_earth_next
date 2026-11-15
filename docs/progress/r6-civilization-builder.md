# R6 civilization builder

The lane's state, as the other lanes' files in this folder. Each round rewrites this file to the current state;
the round's story goes in PORT_LEARNINGS.md ("R6: the civilization builder, round 1" and "round 2").

**Round 1** (`ee_screens`, branch `rust-civbuilder1`): the builder whole in Rust (`civ_whole.rs`, `civ_edit.rs`,
`civ_list.rs`, one host `ee_screenscpp_cb` in EECivilizationBuilder.cpp); EECBEditableCivilization and
EECBCivilizationCreator deleted; net C++ -1,806. The gate: `python -I tools/sim_editor.py --civbuilder` (3,913 lines).

**Round 2** (`ee_tech`, branch `rust-civbuilder2`): the civilizations' data and simulation.
- `civ.rs`: `EECivilizationManager` and `EECivPowersManager` whole: the tables (categories with their levels and ids,
  the special powers' category, the premade civilizations and eras, the powers' records) in one lock, the loads over
  the db tables' bytes, every query (levels, costs, keys both ways, legacy ids, premade checksums, power levels,
  `HasPower`). The C++ records (`EECivilizationCategoryEntry`, `EECivilizationEntry`, `EECivPower`) are layout twins
  of the Rust ones; `GetCategory`/`GetPowerRecord` hand out the Rust record in place. The rules are read in place
  from `EERules::mRust` (`EERules::RustRules()`, bound by the manager's ctor).
- `civ_apply.rs`: `ApplyUpgrade` (`EECBProcessUpgrade` over the unit set), `ApplyUpgradeLevel` (civbuilder.yaml's
  applies, `EECBUpgradeArmor`) and `ApplyPower` (civpowers.yaml's effects) over a host (`EECivHostCall`,
  EECivilizationManager.cpp): the stats come and go as a plain view (`EeTechCivStats`), one stat written back at a
  time (range and speed through the upgrade's `SetRange`/`SetMaxVelocity`).
- `civ_file.rs`: `EELoadCivilization`'s bytes (legacy binary or keyed text; FSMemoryFile read semantics),
  `EEApplyCivilizationFile` and `EECBRequestUpgrades` over a host (`EECivFileCall`, EECBCivilizationFiles.cpp).
- EECivPowersManager.cpp deleted (its forwarders in EECivilizationManager.cpp; link/exe/units.txt's row removed).
- Captures (TEST ONLY, ee/EECivCapture.h, EECivCapture.cpp): CVMG, CVAP, CVLD, CVAF, goldens of the unmodified C++
  at seeds 2-3 of the oracle, late, space, calamity and scenario profiles.

**Units of the folder now** (without the area effects): EECivilizationBuilder (the screens' host, 506),
EECBUIListBox (239), EECBCivilizationFiles (the file finding and the two hosts), EECivilizationManager (forwarders,
the db reads, the simulation's host, `FillCivilizationList`), EECivCapture (TEST ONLY).

**A person only**: multiplayer's `EECBRequestUpgrades` (the grant broadcast, the refusal's chat line; EE-SIM's civ
gate reaches the single-player grant through the game's accept); a keyed .civ file's error box; the premade list's
drawing.

**Next**: `EECivilization` (Empire Earth/EECivilization.cpp, 372: the player's points, purchases, costs and the
deferred upgrades) into Rust over a view (its numbers are this lane's data; its vectors are read by many lanes, so
the class keeps them); `FillCivilizationList` to `civ_whole.rs`; the builder host's remaining ops.
