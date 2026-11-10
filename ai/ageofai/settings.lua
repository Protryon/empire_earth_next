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
    -- the navy and landings (landing.c, navy.c)
    landing = true, landing_transports = 8, navy_share = 25,
    -- fishing and docks (fishing_boat=1, navy=1 upstream; the port's numbers)
    boats = {0, 6, 10}, docks = {0, 1, 2},
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
}
