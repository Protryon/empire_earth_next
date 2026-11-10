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
    min_workforce = 40,             -- the citizens before the early epochs are saved for (the copper plan; below it
                                    -- the citizens ignore the reserve and there is no early advance)
    -- copper_plan: "on" holds both early epochs' costs from min_workforce on; "next_epoch" (upstream's own option,
    -- the one its notes recommend) holds the next epoch's only and puts epoch_share of the income into an epoch
    -- account the citizens never touch. DEVIATION (forced by the epoch costs): "on" shipped, but our Copper Age
    -- costs 1,584 food / 844 gold / 844 iron after the variant's factor where upstream quotes 750 / 400 / 400, so
    -- "on" sent half the Prehistoric workers to gold and iron and put the Stone Age back 10-15 minutes (8.38)
    copper_plan = "next_epoch",
    epoch_share = 40,               -- percent of the income into the epoch account (copper_plan=next_epoch)
    copper_reserve_pct = nil,       -- the alternative for the epoch costs (8.38): {76, 47} holds, in the first two
                                    -- epochs, the reserve upstream's costs were (850 of our 1,122; 750 / 400 / 400 of
                                    -- our 1,584 / 844 / 844) with copper_plan = "on"; nil: the whole cost
    site_max_distance = 20,         -- tiles from the base within which the wild food (berries) counts before farms
    wild_food_share = 60,           -- percent of it the budget counts on (the rest walks, rots or goes to others)
                                    -- ENGINE: the wild food is counted as the gatherable places x a bush's food (this
                                    -- engine holds 6 to a patch, the owner's rule), and the food gatherers before
                                    -- farms are capped at those places; the original's manager sends any number to
                                    -- a bush, so upstream counts the sites' whole amount
    mineral_worker_margin = 300,    -- the minerals' demand beyond the reserve the workers follow at most
    food_floor = 18,                -- citizens on food at least before farms (food_share of the workers at most)
    food_share = 50,                -- percent of the workers on food at least before farms (half, up to food_floor)
    reserve_window_min = 5,         -- from farms: the reserve grows to the epoch's cost over these minutes
    -- epoch pacing: upstream "independent": the first two epochs as soon as they are paid (the copper plan), from
    -- the Copper Age the timer (50-70 minutes an epoch) with the game's catch-up when another player is ahead (an
    -- attempt catch_up_min..max minutes from now; no human here, so the human follow never applies); "rush" = as
    -- soon as paid in every epoch, "timer" = the timer without the catch-up
    epoch_pacing = "independent",
    epoch_min = 50, epoch_max = 70, -- minutes an epoch (timer pacing)
    catch_up_min = 1, catch_up_max = 8,   -- the game's epoch_catch_up row (1..12 minutes, Hard's modifier 70%)
    -- the early game (early.c)
    early_towers = true,            -- two towers by the town centre in the first two epochs
    direct_granary = true,          -- a granary through ai.build where the town's rings are full (an island town)
    replant_fields = true,          -- ai.replant lays such a granary's fields (both were off for a crash of the engine's
                                    -- footprint code at the map's edge, fixed: PORT_LEARNINGS, AI_RESEARCH 8.37)
    bunkers = true,                 -- a Fortress by the drop site nearest the base from the Copper Age once citizens
                                    -- were hurt three times in two minutes; the citizens hit shelter in it (bunker.c)
    tower_bait = true,              -- a citizen hit by a raider walks behind the nearest tower ...
    tower_bait_epoch = 3,           -- ... up to this epoch (upstream: up to the Bronze Age)
    early_core = 2, early_dist = 4,
    early_army = false,             -- off: the home guard from the first epoch (early.c's early_army, 0 upstream)
    early_soldiers = 0,             -- early.c early_soldiers (0 upstream)
    recover_economy = true,         -- econ.c RECOVERY: from the Copper Age, while the workforce is under
                                    -- min_workforce, two thirds of the workers on food, the citizens spend the whole
                                    -- stock, no soldiers unless enemies are in the base
    -- citizens (citizens.c)
    citizen_share = 45,             -- percent of the population cap the game wants as citizens
    citizen_army_share = 40,        -- from farms: the army's share of the cap the citizens' goal leaves
    train_queue = 3,                -- units queued at a producer at most
    -- home guard (defence.c)
    home_guard = 4,                 -- soldiers an epoch (6 + 4 x epoch) ...
    home_guard_share = 35,          -- ... or this percent of the military room (cap x (1 - citizen share))
    home_guard_cap = 166,           -- ENGINE: upstream's room counts unit_limit, the game's cap that follows the
                                    -- human's population (about 166 in its games: 0.35 x 166 x 0.55 = 32 soldiers);
                                    -- the view's cap is the lobby's limit (400: 77 soldiers from the first minute),
                                    -- so the room counts this cap at least and the own population when larger
    -- muster (defence.c)
    muster = true,
    muster_enemies = 8, muster_ratio = 200, muster_strike = 130, muster_out = 18,   -- defence.c MUSTER: at least 8
                                    -- enemies at twice the strength of soldiers and towers; the rally point 18 tiles
                                    -- beyond the base; the strike at 1.3 x
    -- defences (defence.c)
    defence_distance = 10,          -- the front's tower slots this far from the base
    defence_towers = true,
    defence_walls = true,           -- the wall in front of the towers, straight, with a gate in its middle (walls.lua)
    tower_stone_keep = 100,
    towers_max_epoch = 1000,        -- no cap (upstream has none): 4 + this x epoch towers at most
    -- attacks: upstream's first_attack / attack_interval 0 = the game's own times, which at Hard are the dbcpbehavior
    -- rows scaled by the difficulty (EEComputerPlayer::SetNextAttackTime: initial_attack_delay 20-40 minutes x the
    -- Hard modifier 30% = 6-12, attack_frequency 14-17 x 90% = 13-15; the port plays Hard's)
    attack_first_min = 6, attack_first_max = 12,
    attack_interval_min = 13, attack_interval_max = 15,
    -- the force (armies.c)
    ar_min_launch = 8,              -- soldiers a push needs at least
    ar_go_pct = 130,                -- a push: the force beats the enemy at the target by this
    ar_back_pct = 100,              -- out: under this twice running, home
    ar_judge_every = 4000,
    ar_done = 8,                    -- the destination reached: a member within this of it
    ar_home_r = 10,
    ar_raid_r = 14, ar_raid_reach = 60, ar_raid_band = 3,
    raid_answer = 100,              -- a raid answered when the force beats the enemy there by this (armies.c raid_answer 1.0)
    ar_route_avoid = 4, ar_route_avoid_lost = 4,   -- ai.route weights (AR_ROUTE_DANGER 40: a threat cell costs 4 cells)
    ar_bleed_pct = 0,               -- 0 upstream: home once the force has lost this much of the hit points it went
                                    -- out with (0: never for that)
    fight_memory_ms = 180000,       -- the enemy memory's life for soldiers (ai.config)
    -- the citizen militia (defence.c CITIZEN MILITIA)
    citizen_militia = true,         -- the citizens fight builders of a defence near them and, to the Bronze Age, the
                                    -- attackers of ours, at 3 x the enemies' strength within 6 tiles
    militia_estimate = false,       -- NOT PORTED (stats.c's estimate instead of hit points x attack; 0 upstream)
    militia_caught_share = 100,     -- (with militia_estimate only)
    -- the citizens' safety (attack.c citizens_safety, DAMAGE MEMORY, SEEN HURTS, raid_held)
    avoid_towers_citizens = true,   -- citizens sent back from raided sites and sites behind known towers
    citizen_threats = true,         -- the hurt spots: a site within 6 tiles of one (minerals: three) is dangerous for 2 min
    hurt_seen_short = true,         -- a hurt with an enemy soldier in sight counts 20 s only
    raid_held = 150,                -- percent: the town holds a raided site when its soldiers and towers there beat the raiders by this
    -- siege (attack.c attack_siege), settlement upgrades (garrison.c), counter-fire (counterfire.c)
    attack_siege = true,
    garrison_upgrades = true, gu_every = 20000, gu_wait = 60000, gu_batch = 10, gu_wood = 5000,
    counter_fire = true,
    -- scouting (armies.c ar_look, the game's scout)
    scout_at = 120000, scouts_max = 3,
    -- expand.c, towns.c
    -- expand_islands: the parts as bits (upstream's list: outposts 1, barracks 2, ally_islands 4, early 8,
    -- extra_citizens 16, more_islands 32, forward_outpost 64, hold_sea 128, fort 256, fort_guns 512)
    expand_islands = 0,             -- upstream's default: empty (off); the parts as bits above
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
    attack_retarget = false,        -- 0 upstream: the target out of buildings known, the living enemy whose
                                    -- building is nearest the force
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
    vassal_tribute_share = 0,       -- ... percent of each resource at most (0: off, as upstream's rules pay only
                                    -- the capitulation's promise; the 3 minutes / a quarter are the language model's bounds)
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
    -- defence.c (walls, enclose)
    defence_layout = true,          -- the defences like a player (the fronts' towers in econ.lua, the walls in walls.lua)
    game_walls = false,             -- ENGINE: the next engine never runs the classic wall manager (AI_LUA_PLAN §3);
                                    -- upstream skips its update for the mod's AIs (default 0)
    enclose = false,                -- enclose itself fully instead of a front: the coast of its own island, else a
                                    -- ring round the town with a gate on each side and the towers inside (0 upstream
                                    -- "until measured")
    enclose_citizens = 40,          -- with enclose: the ring only once it has this many citizens (the economy first)
    enclose_runs = 1,               -- with enclose: runs of wall (up to 12 pieces each) ordered every 20 s
    enclose_in_flight = 0,          -- with enclose: a new run only while fewer than this many stand unfinished (0 no limit)
    enclose_under_ships = false,    -- with enclose, on an island: only enemy land units near keep the builders off
    enclose_dock_gates = false,     -- with enclose, on an island: the coast by a dock open 4 tiles each way with a
                                    -- walled pocket and a gate towards the town (0: 2 tiles open, no wall)
    enclose_safe_tiles = 0,         -- with enclose, on an island: no coast wall with this many tiles beyond a ship's
                                    -- reach (9) of the sea (0: the coast always)
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
    fronts_log = false,             -- upstream's [autotest] fronts_log (0) - the fronts' trace line every
                                    -- 5 minutes is the port's only view of them
    -- packing.c, spoils.c, forces.c, bunker.c, early.c (leftovers)
    tight_build = true,             -- packing.c: the production buildings packed against their kind near the base
                                    -- (upstream's ini and rows.h: 1; the file's comment says "default 0 until
                                    -- measured" - the ini wins). Its numbers are upstream's defines (TB_R 32,
                                    -- TB_TOUCH 8, TB_BLOCK 14, TB_CLEAR_* 6/4/2/2) in packing.lua
    resign_spoils = true,           -- spoils.c: ENGINE - a player out of the game is this engine's defeat rule
                                    -- (EEWorld::CheckGameOver, defeat_without_recovery); nothing of the AI's to port
    bunker_reserve = true,          -- bunker.c RESERVE (upstream: a mask of players, 65535 = all): the map full,
                                    -- idle soldiers at home go into a fortress; out on losses or enemies near
    town_guards = false,            -- bunker.c TOWN GUARDS: off upstream; NOT PORTED (the front stations are
                                    -- fronts.c's, which is not ported)
    shelve_citizens = true,         -- bunker.c THE SHELF: at the cap with every resource at shelve_stock, idle
                                    -- citizens into fortresses (no new ones trained) ...
    shelve_stock = 5000,            -- ... food, wood, gold and iron each at this or more
    early_militia = true,           -- early.c / defence.c militia_answer: ENGINE - the engine's militia reflex
                                    -- (EECPRMMilitia, aipersonalities military.militia) stands in: citizens and the
                                    -- soldiers within 10 fight when, with the built towers within 12, they are 1.5 x
                                    -- the raiders, else they run to the nearest tower; upstream's stricter rule
                                    -- (under own tower cover or own soldiers, and no citizen lost on the estimate)
                                    -- has no switch (ai.reflex takes no militia key: an interface gap)
    citizens_home = true,           -- econ.c citizens_home part 2: an idle citizen 15+ tiles from its nearest
                                    -- finished town, idle twice running, walks to the nearest safe town (parts 1 and
                                    -- 3, the base point and the stacked foundations, are the original game's bugs)
}
