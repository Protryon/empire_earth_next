-- Age of AI port: the settings ([ai_base] of upstream's config/ageofai.ini at the commit in UPSTREAM). See UPSTREAM
-- for the licence (GPL-3.0, Grille and the Age of AI contributors) and the per-file map.
-- Integers only (percent where upstream has a share). Where the port deviates, the line says so.
return {
    -- budget (econ.c)
    military_start_share = 30,      -- percent of the start stock into the military account
    military_income_share = 40,     -- percent of the income into it before farms ...
    military_income_share_farms = 65,   -- ... and from the Copper Age on
    hoard_cap = 3000,               -- stock above the reserve + this is free for the army too (food, wood)
    hoard_cap_mineral = 200,        -- ... stone, gold, iron
    min_workforce = 34,             -- the citizens before the epoch is saved for (the copper plan). DEVIATION: 40
                                    -- upstream; the owner's own build order saves from 32 (docs/AI_RESEARCH §8.37)
    stone_workforce = 60,           -- DEVIATION: the Stone Age's workforce before the Copper Age is saved for
    mineral_worker_margin = 300,    -- the minerals' demand beyond the reserve the workers follow at most
    food_floor = 24,                -- citizens on food at least before farms (food_share of the workers at most)
    food_share = 60,                -- DEVIATION (half upstream): percent of the workers on food at least before farms
    reserve_window_min = 5,         -- from farms: the reserve grows to the epoch's cost over these minutes
    -- epoch pacing: upstream "independent" (the timer, 50-70 minutes an epoch; a human's advance pulls the AIs
    -- behind along). DEVIATION: "rush", as soon as the epoch is paid (our games are 45 minutes long, the owner
    -- expects the port to beat boom's epoch pace); "timer" plays upstream's pacing
    epoch_pacing = "rush",
    epoch_min = 50, epoch_max = 70, -- minutes an epoch (timer pacing)
    -- the early game (early.c)
    early_towers = true,            -- two towers by the town centre in the first two epochs
    direct_granary = true,          -- a granary through ai.build where the town's rings are full (an island town)
    replant_fields = true,          -- ai.replant lays such a granary's fields (both were off for a crash of the engine's
                                    -- footprint code at the map's edge, fixed: PORT_LEARNINGS, AI_RESEARCH 8.37)
    bunkers = true,                 -- a Fortress by the drop site nearest the base from the Copper Age once citizens
                                    -- were hurt three times in two minutes; the citizens hit shelter in it (bunker.c)
    tower_bait = true,              -- a citizen hit by a raider walks behind the nearest tower ...
    tower_bait_epoch = 2,           -- ... up to this epoch (upstream: up to the Bronze Age; every citizen near the
                                    -- raider in every epoch lost 4 of 16 games at 90 minutes: the economy stood still)
    early_core = 2, early_dist = 4,
    early_army = true,              -- DEVIATION (off upstream): no home guard in the first epoch, towers and the
                                    -- militia hold the town (the owner's and upstream's playtester's opening); the
                                    -- home guard from the Stone Age ...
    early_soldiers = 4,             -- ... but upstream's alternative once enemy soldiers were seen: this many in the
                                    -- first epoch from minute 5 (the 90-minute games against classic Hard: a
                                    -- soldierless town with two towers was raided to death in the Prehistoric; four
                                    -- from the start cost 2.5 minutes of the Stone Age on quiet maps, AI_RESEARCH 8.37)
    recover_share = 75,             -- the saving stops while the citizens are under this percent of their most
                                    -- (recover_economy: a lost economy spends the whole stock on citizens)
    -- citizens (citizens.c)
    citizen_share = 45,             -- percent of the population cap the game wants as citizens
    citizen_army_share = 40,        -- from farms: the army's share of the cap the citizens' goal leaves
    train_queue = 3,                -- units queued at a producer at most
    -- home guard (defence.c)
    home_guard = 4,                 -- soldiers an epoch (6 + 4 x epoch) ...
    home_guard_share = 35,          -- ... or this percent of the military room (cap x (1 - citizen share))
    -- muster (defence.c)
    muster = true,
    muster_enemies = 4, muster_ratio = 130, muster_strike = 130, muster_out = 6,   -- DEVIATION: 8, 200 and 18 upstream:
                                    -- upstream's hold_town instead (the soldiers weaker than 1.2 x the enemy at the
                                    -- town pull back 6 tiles behind the base and hold under the towers)
    -- defences (defence.c)
    defence_distance = 10,          -- the front's tower slots this far from the base
    defence_towers = true,
    defence_walls = false,          -- DEVIATION: no front walls yet (no ai.wall helper)
    tower_stone_keep = 100,
    towers_max_epoch = 3,           -- DEVIATION (a cap upstream has not): 4 + this x epoch towers at most
    -- attacks (the game's timers upstream, dbcpbehavior: first 20-40 min, then every 20-25)
    attack_first_min = 20, attack_first_max = 40,
    attack_interval_min = 20, attack_interval_max = 25,
    -- the force (armies.c)
    ar_min_launch = 8,              -- soldiers a push needs at least
    ar_go_pct = 130,                -- a push: the force beats the enemy at the target by this
    ar_back_pct = 100,              -- out: under this twice running, home
    ar_judge_every = 4000,
    ar_done = 8,                    -- the destination reached: a member within this of it
    ar_home_r = 10,
    ar_raid_r = 14, ar_raid_reach = 60, ar_raid_band = 3,
    ar_route_avoid = 4, ar_route_avoid_lost = 4,   -- ai.route weights (AR_ROUTE_DANGER 40: a threat cell costs 4 cells)
    ar_bleed_pct = 50,              -- DEVIATION (0 upstream, measured as good on the fight): home once the force has
                                    -- lost this much of the hit points it went out with
    fight_memory_ms = 180000,       -- the enemy memory's life for soldiers (ai.config)
    -- siege (attack.c attack_siege), settlement upgrades (garrison.c), counter-fire (counterfire.c)
    attack_siege = true,
    garrison_upgrades = true, gu_every = 20000, gu_wait = 60000, gu_batch = 10, gu_wood = 5000,
    counter_fire = true,
    -- scouting (armies.c ar_look, the game's scout)
    scout_at = 120000, scouts_max = 3,
    -- expand.c, towns.c
    -- expand_islands: the parts as bits (upstream's list: outposts 1, barracks 2, ally_islands 4, early 8,
    -- extra_citizens 16, more_islands 32, forward_outpost 64, hold_sea 128, fort 256, fort_guns 512)
    expand_islands = 1,             -- DEVIATION (empty = off upstream: measured even against the base AI on the
                                    -- test maps, one free island at most): outposts on, so the port settles the free
                                    -- islands at all - the owner's direction that the AI must expand (towns.c SPREAD:
                                    -- "if the AI can't expand everything else becomes moot"); the other parts off as
                                    -- upstream has them
    expand_citizens = 8,            -- citizens a trip (3..12)
    expand_guard = 3,               -- land soldiers with them (0..4; 4 at least with hold_sea)
    expand_minute = 8,              -- not before this minute (and the Copper Age)
    expand_trips = 4,               -- trips to one island in all
    expand_max = 3,                 -- islands besides home with more_islands (1..4)
    rescue_citizens = true,         -- NOT PORTED (the stranded citizens' transport home): kept for the record
    -- towns: the parts as bits (resource_towns 1, settlement 2, forward_production 4, forward_base 8, spread 16,
    -- scout 32, grow 64, fort 128, take_mines 256, mine_raids 512); upstream "resource_towns, spread"
    towns = 17,
    towns_max = 3,                  -- resource towns at most (not with spread)
    towns_citizens = 6,             -- citizens that found a town (a forward base: + 2; 12 at most)
    towers_per_town = 2,            -- towers by a resource town (spread: 2 + one per 4 mines)
    towns_remove_unbuilt = false,   -- a town going up whose citizens are all dead: its foundations removed
    towns_care = true,              -- spread: the spot and its way safe (part 1; the other parts measured no gain)
    -- attack.c (weak spots, breaches), armies.c (detachments), forces.c
    avoid_towers = true,            -- ENGINE: ai.route over the threat layer (ar_route_avoid above) stands in for
                                    -- the route cost hook; avoid_towers_weight (2000 a step) has no counterpart
    avoid_towers_margin = 6,        -- tiles added to a known defence's range (threat_at, the leash)
    attack_weak_spots = true,       -- a goal inside known defences: the nearest uncovered enemy building instead
    attack_weak_spot_radius = 60,   -- tiles around the goal searched for it
    attack_leash = true,            -- no enemy inside known defences as a target unless the goal lies in them too
    attack_breach = true,           -- a known wall on the way: through its segment outside the defences
    attack_ignore_air = true,       -- aircraft beyond range + 3 are no target (lib.groups)
    attack_retarget = true,         -- DEVIATION (0 upstream): the target out of buildings known, the living enemy
                                    -- whose building is nearest the force (the port's flow took any enemy's nearest
                                    -- building before; kept)
    citizen_attacks = false,        -- not applicable: the force has no citizens (the game's groups took them)
    mine_raids = false,             -- armies.c MINE RAIDS / HIT AND RUN (towns=...|mine_raids, off upstream): the
                                    -- detachments are not ported (attack.c does not use them)
    siege_fire_every = 2000,        -- siege_tick: the shelling looked at every 2 s
    siege_fire_near = 40,           -- DEVIATION (upstream's waves go from home with escorts): the port's siege go
                                    -- with the force and shell a target within this of the force's middle
    -- navy.c, fleet.c, landing.c
    fishing_boat = true,            -- the newest fishing boat the dock can make (the game's pick is by epoch)
    fishing_boats_per_fish = 2,     -- DEVIATION: upstream leaves the boats' count to the game's field passes (+0xEFA);
    fishing_boats_max = 8,          -- this is the engine's own coast rule (EECPRMCoastBoatsWanted: 2 a fish within
    fishing_boats_max_water = 16,   -- fishing_reach of the town, 8 at most; 16 on a water map, where upstream fished
    fishing_reach = 45,             -- with 12-18 boats)
    navy = true,                    -- the navy account on a water map
    navy_share = 25,                -- percent of the wood, gold and iron income into it (x the map strength) ...
    navy_share_max = 50,            -- ... toward this while the fleet is below its size
    navy_fleet = "big",             -- the AI leads its warships and trains them up to navy_fleet_pop of the cap
    navy_fleet_pop = 15,            -- percent of the population cap: the fleet's size
    navy_fleet_mass = true,         -- mass under the towers while weaker than the enemy fleet known, strike at 1.25
    navy_fleet_boats = true, navy_fleet_coast = true, navy_fleet_raid = 130,   -- [general] test keys at their defaults
    fleet_cover = true,             -- cover.c fall_back fleet_cover: a warship under half its hit points to a dock until 80%
    transports_economy = false,     -- the engine's own transport orders are not in this path (no crossings of its own)
    landing = true, landing_transports = 8,   -- the most transports (the size follows the beach)
    landing_citizens = 10,          -- citizens along for the forward base (0 / 3 / 6 / 10 by the transports)
    landing_waves = 3,              -- waves after the first while the bridgehead holds
    -- diplo.c
    diplomacy = true,               -- the module runs (the port's own switch: everything below)
    answer_war = true,              -- whoever turns hostile towards an AI allied with him gets war back 3-6 s later
    ai_diplomacy = true,            -- the AIs' own initiative from minute 10: alliances against a leader (1.5 x),
                                    -- the betrayal of a weak ally, capitulation (the tribute 20% + caution/4 +
                                    -- (100 - greed)/4 of each resource); peace offers are always judged
    alliance_blocks = true,         -- the players linked by alliances are one block: peace inside it, a war inside
                                    -- it splits it (every other AI takes a side), the veto (a grudge: a war declared
                                    -- within pride/20 minutes, an opinion below -30)
    surrender = true,               -- no citizens for 60 s and none trainable (or 3 minutes at all), the army under
                                    -- half the strongest enemy's: it gives up. ENGINE GAP: no ai.resign - the
                                    -- decision is traced, the player stays in
    vassal_tribute_every = 3,       -- minutes between a vassal's tributes to its lord ...
    vassal_tribute_share = 25,      -- ... percent of each resource at most. DEVIATION: upstream's rules pay only the
                                    -- capitulation's promise; these are the language model's tribute bounds ([llm]
                                    -- llm_constrained=1: once in 3 minutes, a quarter of each resource) made a rule
    -- stance_lines ("yours"): the game's stance line - not applicable (the trace's `diplo stance` stands in);
    -- [general] auto_victory: the game's own end - not applicable
    -- mass.c, armies.c (detachments), skirmish.c
    -- army_mass: the parts as bits (upstream's list: rally 1, wait_stronger 2, only_fighting 8, tether 16,
    -- walk_back 32, to_pack 64, only_buildings 128; 4 was retreat, removed). Upstream: empty (0): rally lost fewer
    -- and killed more head-to-head and against the base AI without the cheat, but killed fewer against the cheating
    -- base AI (docs/economy.md "The army as one"). Only with an enemy town known on the own continent.
    army_mass = 0,
    army_mass_group = 5,            -- the held reinforcements go together once this many wait ...
    army_mass_hold = 120,           -- ... or 3 after this many seconds, any after twice that
    army_mass_ratio = 130,          -- wait_stronger: the push waits while the land army is under this percent of the
                                    -- strongest enemy army known ...
    army_mass_wait = 10,            -- ... for this many minutes at most
    army_mass_tether = 12,          -- tether: no target more than this many tiles from the pack
    -- mine_raids (above, towns' bit 512) switches armies.c's detachments on: MINE RAIDS, HIT AND RUN, THE RAIDERS,
    -- THE RIDERS' LOOK and THE SITES' MEMORY (detach.lua; off upstream). Their numbers are upstream's defines
    -- (AR_DET_SIZE 20, AR_DET_MIN 30, AR_DET_PAUSE 3 min, AR_DET_HOLD 15 min, AR_HIT_SIZE 12, AR_HIT_MIN 10 ...).
    -- skirmish: SK_ON's bits (1 the party, 2 the spotter); 0 upstream ("off until measured": level on the save
    -- "choke wolf" and on the wall test). Its numbers are upstream's defines (SK_N 8, SK_MIN 3, SK_RANGE 7 ...).
    skirmish = 0,
    -- airstrike.c, stats.c
    air_strike = true,              -- AS_ON (a compile-time switch upstream, no [ai_base] key; on since 2026-11-04):
                                    -- the bombers held as a wing, trained while a position needs them, the bombing
                                    -- run on the shooters' spot / the push's target / the target found too strong
    fight_estimate = false,         -- [ai_base] fight_estimate=0: the fight decisions keep summed hit points x attack
                                    -- (ai.strength); stats.lua's M.ratio is the estimate (PARTLY: no damage tables)
    -- metal.c, priority.c, fronts.c
    -- metal_plan: the parts as bits (upstream's list: best_per_metal 1, no_waiting 2, surplus_cheap 4,
    -- fleet_by_enemy 8, calm_navy_cap 16). Upstream: empty (0) - on the large island maps fleet_by_enemy (+ calm)
    -- gave 60-90 more soldiers trained and more kills - losses without the cheat, but against the cheating base AI
    -- the kills - losses fell on two maps; best_per_metal cost wins on small Continental (docs/economy.md "Gold and
    -- iron"). no_waiting is not applicable here (the port never saves for a refused soldier: metal.lua).
    metal_plan = 0,
    metal_cap = 250,                -- the bits act only from this population cap (the metal limits the army there)
    metal_fleet_pct = 250,          -- fleet_by_enemy: the fleet at most this percent of the enemy warships known + 6 ...
    metal_fleet_min = 20,           -- ... and at least this many x the map strength
    workers_to_metals = false,      -- army.c MIX (army_plan's part workers_to_metals, not in upstream's default list):
                                    -- workers from piled-up food to the wood, gold or iron the army lacks (metal.lua)
    army_mix_food = 500,            -- ... while the 2-minute low of food above the reserve is at least this
    spend_tiers = false,            -- priority.c: buildings wait for an outpost's Settlement and tower too
                                    -- (expand_hold; "default 0 until measured")
    farm_first = true,              -- econ.c THE SILOS' ROW: the first two Granaries and their 8 fields each are a row
                                    -- of the priority list, one at a time, from the Copper Age's research
    front_army = false,             -- fronts.c: the idle soldiers stand at the heaviest fronts between attacks
                                    -- (0 upstream: no gain measured; the port's force is one group, fronts.lua)
    fronts_log = true,              -- DEVIATION: upstream's [autotest] fronts_log (0) - the fronts' trace line every
                                    -- 5 minutes is the port's only view of them
}
