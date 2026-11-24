# The world lane (Physics Thread, Random Map, Campaign, Data Pre-Loading Thread, Player)

The lane's state. Each round rewrites this file to the current state; the rounds' stories go in PORT_LEARNINGS.md
("The world lane, round 1", "The world lane, round 2"; round 3's leftovers went to "The game core lane, round 12", fork B).

**In Rust**
- The random map whole (round 1, ee_sim rmap `object.rs`): the generator state, a try's set-up, Generate, PlaceWall,
  the retries, the generator list's set-up and lookup. The generator list is Rust's since round 12 (was `std::vector<EERMGenerator>`
  of `UWideString`s); the C++ keeps `ee_rmap_cpp`. Default.cpp and its headers live on only as the rmap gate's adapter.
- `EEPhysics`' bodies (round 1, ee_game `physics.rs`) over the saved block and `ee_physcpp`.
- The physics model host (round 2, ee_sim units `pm_flight.rs`, `pm_note.rs`, `pm_host.rs`): the projectile and
  missile steps with their CBPA/CBPJ/CBPS/CBMA/CBMH records written in Rust at the C++'s points, the group-aware rates,
  the TEST ONLY PMST note, the models as statics and every multi-statement `ee_units_pm_host_*` (views or one-call
  shims). The sweeps' host (`unit_sweep.rs`, ops 10..47) and the AGPL record (`ag_sweep.rs`), the patrol group's
  and the ATC's hosts (`patrol_group.rs`, `atc.rs`).
- `EEDataPreLoading`'s bodies (round 1, ee_game `preload.rs`); the media objects (round 2, ee_game
  `preload_media.rs`): the graphic object table's dump and delete, FindTurretPart, the effects' links, DumpIDs,
  UnloadTable and GetMedia, the sounds' dump, unload and one-shot Play, the "New Music.txt" override and the epochs'
  songs, over `ee_preload_mediacpp`.
- `EECampaignProgress` whole and `EECampaign::Load` (round 1); EECampaign's editor side (round 2, ee_game
  `campaign.rs`): Save, SaveArchive, CreateScenarioList, CopyScenarioFiles, ListScenarioFiles, ListScenarioMedia,
  ListEffectSound(s), NotArchived, GetScenarioListFile, the password crypt (ee_base's codec), EECampaignFormManager's
  UpdateChecksum and GetBuiltInCampaign, and the progress' helpers (the user spec, the tag read, the list line).
- The player (round 2, ee_sim units `save_prim.rs`, `player_econ.rs`, `player_rest.rs`, `player_split_more.rs`,
  `town_split.rs`, `player_events_split.rs`, `player_save_split.rs`, `player_views.rs`): the save primitives, the
  economy and improvements with their PLEC record, about 30 bodies (ClearFootprint, ReloadTechTree, SetGameResult's
  decisions, AdoptTownPatrol, GetNearestArea, the wonder timers, the cloak chains, ...), every multi-statement case of
  the player, town-save, event-list and save hosts; 22 members only the hosts called are gone.

- Round 12 (game core lane, fork B): the media tables' loads and the sounds' and songs' media uses (`preload_media.rs`), EEDataPreLoading's ctor and dtor, the graphic object's and sound's dtors; EEPhysics' Load, Save, ctor body, the loaded game's fixup lists, the campaign win, the shift-key test and the JIT chat line (`physics.rs`); the generator list (rmap `object.rs`: EERMGenerator and sRandomMapGenerators gone); EECampaignFormManager::Get; EEPlayerStatistics' clears and the result chat line (`player_stats.rs`, `player_rest.rs`).

**Stays C++**
- Physics Thread: EEPhysics.cpp's structure (UpdateUnits' tick host, the after-load fixups, Load/Save's file host;
  99 logic lines), the host switch heads and data definitions (~50 in the other four files).
- Campaign: the class layouts, the ctors, the host switch (one-statement cases), the
  scenario list's stripped push (`EECampaignPushStripped`) and the built-in campaign tables.
- Data Pre-Loading Thread: the classes' object lifetimes (the table loads' remaining record readers
  `EEDbReadLeadRecords`/`EEDbReadRecords` with their lambdas, the `EEMediaAcquire`/`EEMediaRelease` lambdas, the gobj
  host's part lists).
- Player: EEPlayer.cpp (166: the hosts' prologues and context structs, EEPlayerAnnounceResult's chat text,
  EEPlayerShareBonus, the float-bits helpers), EEPlayer.h (427, almost all declarations; the tech tree's ~200 and
  RTSPlayer's belong to other lanes' .cpp), EEPlayerStatistics.cpp (51: the record's and history's resets).

**Gates of this lane**: the rmap gate (`tools/rmap_gate/rmap_gate.py check`, `check --reader`), the oracle and the
twelve capture profiles (missile and calamity check the flight steps and PMST; units and groups the sweeps; oracle
and late the player's PLEC/PLST), the saveload matrix, scenario_check (the campaign screens), the tour and gates.sh.

**What only a live game exercises**: the campaign editor's Save and file list (the scenario archive, the copies, the
media and sound lists), UpdateChecksum and GetBuiltInCampaign (the campaign screens), the progress' save after a won
scenario, a user campaign's RestoreCivilization, a passworded Load; the media objects' dumps (a media dump request),
the one-shot sound Play (EEUGUnloadTransport), the New Music override (a "Data\Music\New Music.txt"), GetMedia's
random graphic (kind-only effects: drawn only); the physics' autosave, OOS dump, the trample hit's effects, the
carrier deck walk, the ATC reroute; the player's SetGameResult chat line, the event list's alert sounds and mini-map
ping, DumpMedia, ReloadTechTree (the editor's load thread) and the 0.260-era saves' town matching.

**Next**: EEPhysics.cpp's tick host (UpdateUnits) once the units lane's moves settle; the player's host prologues and EEPlayer.h's declarations; the media objects' remaining record readers (EEDbReadRecords' lambdas).
