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
    early_core = 2, early_dist = 4,
    early_army = true,              -- DEVIATION (off upstream): no soldiers in the first epoch, towers and the militia
                                    -- hold the town (the owner's and upstream's playtester's opening); the home guard
                                    -- from the Stone Age
    -- citizens (citizens.c)
    citizen_share = 45,             -- percent of the population cap the game wants as citizens
    citizen_army_share = 40,        -- from farms: the army's share of the cap the citizens' goal leaves
    train_queue = 3,                -- units queued at a producer at most
    -- home guard (defence.c)
    home_guard = 4,                 -- soldiers an epoch (6 + 4 x epoch) ...
    home_guard_share = 35,          -- ... or this percent of the military room (cap x (1 - citizen share))
    -- muster (defence.c)
    muster = true,
    muster_enemies = 8, muster_ratio = 200, muster_strike = 130, muster_out = 18,
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
}
