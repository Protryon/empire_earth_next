-- The boom personality (docs/AI_LUA_PLAN.md §4 and "S3 as built"): the owner's own play from the Prehistoric to the
-- Copper Age. Food first, towers at the town and the worked sites, a small mobile defence that grows with the threat
-- it sees, fishing from the Stone Age's first minute, no attacks; the next epoch is saved for once the town has its
-- citizens and asked for only when it is paid. After the Copper Age it keeps growing on farms and holds (S5 extends
-- it).
--
-- One plan table per epoch (PLAN: 1 Prehistoric, 2 Stone, 3 Copper and later) and a planner that runs it every step.
-- Everything it decides comes from the view and the previous step's results. Each minute it prints a `plan` line (the
-- epoch, saving, the threat, army, citizens, towers, boats, claim and the gatherers wanted) into the AI trace.
local V = require("lib.view")
local A = require("lib.army")

local PLAN = {
    {   -- Prehistoric
        -- the owner's build order (2026-11-08): no soldiers before the Stone Age, all the food to citizens (about 18
        -- on food, 5-10 wood, 4 iron, 2 gold, 2 stone), then the advance as soon as it is paid
        citizens = 32,          -- trained with no reserve on food up to here ...
        citizens_max = 36,      -- ... and from the food above the reserve up to here
        save_at = 32,           -- the citizens from which the next epoch is reserved ...
        save_after = 18,        -- ... or from this many minutes into the epoch
        -- the gatherers on wood, stone, gold and iron: by what each lacks of the stock the plan spends soon
        -- (stock_targets: the reserve, the buildings, boats and soldiers to come, and `bank`), at least `floor` while
        -- it lacks anything (wood: always), at most `cap`
        floor = {wood = 5, stone = 2, gold = 2, iron = 4},
        cap = {wood = 10, stone = 2, gold = 2, iron = 4},
        bank = {wood = 400, stone = 0, gold = 250, iron = 300},   -- kept for the next epochs (Copper: gold, iron)
        food_share = 55,        -- percent of the citizens on food at least
        food = 18,              -- citizens on food at least (the fishing boats come on top)
        patches = 3,            -- forage patches claimed at most
        hunt = 6,               -- hunters at most (one an animal, within the claim range: view.hunt)
        soldiers = 0,           -- the mobile defence when nothing threatens (none: the militia and the towers)
        soldiers_max = 0,       -- under a threat
        towers = 3,
        settlements = 4,          -- (§8.35: one by each worked patch and mine, the owner)
        barracks = 1, ranges = 0,
        barracks_at = 28,       -- citizens: the first barracks stands when the Stone Age's army is trained
        boats = 0, docks = 0,
        claim_max = 30,
    },
    {   -- Stone
        -- first 20 soldiers (spearmen from two barracks, one defend group at home), then the citizens (18 food, 12
        -- gold, 12-18 iron, 6 stone, 10-15 wood, the boats on top), then the Copper Age
        citizens = 64, citizens_max = 90, save_at = 60, save_after = 10,
        floor = {wood = 10, stone = 6, gold = 12, iron = 12},
        cap = {wood = 15, stone = 6, gold = 12, iron = 18},
        bank = {wood = 150, stone = 0, gold = 100, iron = 100},
        food_share = 25,
        food = 18,
        patches = 5,
        hunt = 8,
        soldiers = 20, soldiers_max = 30,
        army_first = true,      -- the citizens wait (at the last epoch's count) until the soldiers are trained
        towers = 7,
        settlements = 8,
        barracks = 2, ranges = 1,
        barracks_at = 10,
        boats = 10, docks = 2,
        claim_max = 35,
    },
    {   -- Copper and later (S5 extends it)
        -- the same, with 2-3 granaries of farms; 12-18 gold, 12-18 iron, 6 stone, 15-20 wood
        citizens = 90, citizens_max = 100, save_at = 95, save_after = 25,
        floor = {wood = 15, stone = 6, gold = 12, iron = 12},
        cap = {wood = 20, stone = 6, gold = 18, iron = 18},
        bank = {wood = 300, stone = 0, gold = 200, iron = 200},
        food_share = 25,
        food = 18,
        patches = 5,
        hunt = 8,
        soldiers = 20, soldiers_max = 40,
        barracks_at = 10,
        towers = 10,
        settlements = 9,
        barracks = 2, ranges = 2,
        boats = 10, docks = 2,
        farms = 24, granaries = 3,
        claim_max = 45,
    },
}

local PASS_MS = 10000           -- the claims, the gatherers and the buildings every 10 s
local THREAT_MS = 90000         -- a threat seen keeps the town on guard this long
local THREAT_MIN = 2500         -- weaker than this (a scout) is left to the militia and the towers
local BACKOFF_MS = 60000        -- an order refused for want of a site or producer is not repeated for this long
local SITE_PLACES = 6           -- gatherers a mine unit holds (the mining limit); a forage patch holds 6 in all
local FAR_REACH = 45            -- tiles: how far a mineral with no site in the claim range is looked for (was 60:
                                -- on a Small map that is the other town, §8.35)
local UNKNOWN_REACH = 38        -- tiles: how far out a site is taken while no enemy town has been seen
local ENEMY_SHARE = 85          -- percent: a site is taken only this much nearer to the town than to an enemy town
local ENEMY_KEEP_OFF = 25       -- tiles: and never this near a seen enemy town
local HUNT_MAX = 6              -- hunters at most beside the patches (the owner: 6 hunt beside the patches)
local HUNTERS_PER_ANIMAL = 3    -- (as the C++ finder, kNextHuntersPerAnimal)
local WOOD_DROP_AT = 4          -- wood gatherers wanted from which the worked forest edge gets a settlement
local SCOUT_AT = 90000          -- the first scout (a soldier) from here
local SCOUTS = 3                -- scouts sent at most (a new one when the last was lost)
local TOWER_STRENGTH = 4000     -- what a tower counts for against a threat (about three clubmen)
local BOATS_LOST_MS = 300000   -- fishing boats sunk within this long ...
local BOATS_LOST_MAX = 3        -- ... this many times: none are replaced until fewer are
local ARMY_FIRST_MIN = 5        -- minutes into the epoch the citizens wait at most for the army_first soldiers
local PER_GATHERER = 30         -- what one gatherer is counted on to bring in over the planning horizon (~6 min)
local ESCORT_MIN = 2            -- soldiers an escort of a contested site takes at least (the guard keeps 2) ...
local ESCORT_MAX = 6            -- ... and at most
local ESCORT_WAIT = 120000      -- a new escort no sooner than this after the last one was sent

local MINERALS = {"stone", "gold", "iron"}
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle"}
-- never built for the SOL (as the classic ResearchCheapest), and houses: the boom has none
local NOT_FOR_SOL = {b_town_center = true, b_capitol = true, b_settlement = true, b_farm = true, b_granary = true,
                     b_house = true, b_dock = true}   -- (a dock for the SOL went 59 tiles out: the fish verb builds them, §8.35)
local TRAINERS = {"b_barracks", "b_archery_range"}

local M = {}

-- ---------------------------------------------------------------------------------------------------------------
-- view helpers

local function tower_class(view)
    for t = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[t]) ~= nil then
            return TOWER_CLASSES[t]
        end
    end
    return nil
end

local function tower_counts(view)
    local n, finished, busy = 0, 0, false
    for _, t in ipairs(TOWER_CLASSES) do
        local b = V.buildings(view, t)
        if b ~= nil then
            n = n + V.total(b)
            finished = finished + b.finished
            busy = busy or b.unfinished + b.queued > 0
        end
    end
    return n, finished, busy
end

local function going_up(entry)
    return entry ~= nil and entry.unfinished + entry.queued > 0
end

-- the soldier each trainer makes now: the first of PREFERRED it can make (the owner: "an army of spearmen waiting"),
-- else the cheapest (by the sum of its cost)
local PREFERRED = {inf04_phalanx = 1, inf02_spearman = 2}
local function soldier_classes(view)
    local best = {}
    for _, c in ipairs(view.costs) do
        if not c.building and c.military and c.producer ~= nil then
            for _, k in ipairs(TRAINERS) do
                if c.producer == k then
                    local s = V.sum(c.cost)
                    local rank = PREFERRED[c.type] or 100
                    if best[k] == nil or rank < best[k].rank or (rank == best[k].rank and s < best[k].sum) then
                        best[k] = {type = c.type, sum = s, cost = c, rank = rank}
                    end
                end
            end
        end
    end
    return best
end

-- the sites holding a resource, the home ones (the start resources by the town) first, then nearest first (patch:
-- forage patches only); a centre may hold two minerals
local function sites_of(view, resource, patch)
    local list = {}
    for _, s in ipairs(view.sites) do
        if s.holds[resource] > 0 and (not patch or s.patch) then
            list[#list + 1] = s
        end
    end
    table.sort(list, function(a, b)
        if a.home ~= b.home then
            return a.home
        end
        if a.distance ~= b.distance then
            return a.distance < b.distance
        end
        return a.index < b.index
    end)
    return list
end

-- ---------------------------------------------------------------------------------------------------------------
-- the parts of the plan

-- the stock of each resource but food that the plan will spend soon: the next epoch's cost while it is reserved,
-- the buildings, boats and soldiers still to come, and the plan's bank
local function stock_targets(view, plan, reserve, towers, soldiers_wanted)
    local t = {wood = plan.bank.wood, stone = plan.bank.stone, gold = plan.bank.gold, iron = plan.bank.iron}
    local function add(type, n)
        local c = V.cost(view, type)
        if c ~= nil and n > 0 then
            for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
                t[r] = t[r] + c.cost[r] * n
            end
        end
    end
    for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
        t[r] = t[r] + reserve[r]
    end
    add("b_barracks", math.min(plan.barracks - V.total(V.buildings(view, "b_barracks")), 1))
    add("b_archery_range", math.min(plan.ranges - V.total(V.buildings(view, "b_archery_range")), 1))
    add("b_settlement", math.min(plan.settlements - V.total(V.buildings(view, "b_settlement")), 2))
    add("b_dock", plan.docks - V.total(V.buildings(view, "b_dock")))
    local boat = V.cost(view, "s02_fishing_boat_stone")
    if boat ~= nil then
        add("s02_fishing_boat_stone", math.min(plan.boats - view.boats - boat.requested - boat.in_production, 4))
    end
    local tower = nil
    for i = #TOWER_CLASSES, 1, -1 do
        if tower == nil and V.cost(view, TOWER_CLASSES[i]) ~= nil then
            tower = TOWER_CLASSES[i]
        end
    end
    if tower ~= nil then
        add(tower, math.min(plan.towers - towers, 2))
    end
    if plan.farms ~= nil then
        add("b_farm", math.min(plan.farms - V.total(V.buildings(view, "b_farm")), 3))
    end
    -- the soldiers still wanted, at the cost of the soldiers trained (the cheapest of each trainer built, in turn)
    if soldiers_wanted > view.army_count then
        local kinds = {}
        for _, k in ipairs(TRAINERS) do
            local best = nil
            for _, c in ipairs(view.costs) do
                if not c.building and c.military and c.producer == k and c.producers > 0
                        and (best == nil or V.sum(c.cost) < V.sum(best.cost)) then
                    best = c
                end
            end
            if best ~= nil then
                kinds[#kinds + 1] = best.type
            end
        end
        local n = soldiers_wanted - view.army_count
        for i, k in ipairs(kinds) do
            add(k, (n + #kinds - i) // #kinds)
        end
    end
    return t
end

-- a site the gatherers can be sent to now: no enemy or danger near, not by another player's land (a claimed site not
-- seen yet is gathered from all the same: the first gatherer there explores it)
local function open_site(s)
    return s.blocked == nil or s.blocked == "unexplored"
end

-- a place the town may expand to (§8.35, the owner's fifth playtest: a settlement queued 50 tiles out in the other
-- town, 80 citizens lost walking and building there): the home sites always; else well nearer to the town than to
-- any enemy town seen (ENEMY_SHARE), never within ENEMY_KEEP_OFF of one, and while no enemy town has been seen not
-- past UNKNOWN_REACH
local function our_side(view, x, y, distance, home)
    if home then
        return true
    end
    local seen = false
    for _, e in ipairs(view.enemies) do
        if e.town_x ~= nil and e.town_x >= 0 then
            seen = true
            local dx, dy = x - e.town_x, y - e.town_y
            local d2 = dx * dx + dy * dy
            if d2 <= ENEMY_KEEP_OFF * ENEMY_KEEP_OFF
                    or distance * distance * 10000 > d2 * ENEMY_SHARE * ENEMY_SHARE then
                return false
            end
        end
    end
    return seen or distance <= UNKNOWN_REACH
end

local function safe_site(view, s)
    return our_side(view, s.x, s.y, s.distance, s.home)
end

-- the gatherers a site holds for a resource (the view's capacity: 6 a forage patch, the mining limit a mine unit)
local function places_of(s, resource)
    if s.capacity ~= nil and s.resource == resource then
        return s.capacity
    end
    if s.patch then
        return SITE_PLACES
    end
    return s.holds[resource] * SITE_PLACES
end

-- the claims of one resource: the nearest open sites whose places hold the gatherers wanted, at least one while any
-- are wanted, at most `most` (a claimed one under a passing threat is kept), are marked in `keep` and claimed. The
-- patches stay inside the claim range; the minerals are looked for out to FAR_REACH, nearest first (the claim range
-- holds a few small mines; the larger ones lie farther out). A mineral with no open site kept contests the nearest
-- one only enemy soldiers keep the gatherers off (not a tower, a danger zone or another player's land): it is kept
-- and claimed, and the escort holds it (§8.34). Only sites on the town's side (safe_site, §8.35); a claimed one under
-- a tower (or with one on the way) is not kept. Returns the places the kept sites hold, the contested site (nil: none)
-- and the places of the kept sites the gatherers can go to now (open).
local function claim_sites(view, order, keep, resource, patch, wanted, most, range)
    local want_sites = 0
    if wanted > 0 then
        want_sites = (wanted + SITE_PLACES - 1) // SITE_PLACES
    end
    if want_sites > most then
        want_sites = most
    end
    local reach = patch and range or FAR_REACH
    local kept, places, open, open_places = 0, 0, 0, 0
    local list = sites_of(view, resource, patch)
    for _, s in ipairs(list) do
        if kept < want_sites and (open_site(s) or (s.claimed and (s.blocked == "enemy_near" or s.blocked == "danger")))
                and s.distance <= reach and safe_site(view, s) then
            kept = kept + 1
            places = places + places_of(s, resource)
            keep[s.index] = true
            if open_site(s) then
                open = open + 1
                open_places = open_places + places_of(s, resource)
            end
            if not s.claimed then
                order("claim_" .. resource, {verb = "claim", site = s.index})
            end
        end
    end
    local contest = nil
    if open == 0 and want_sites > 0 and not patch then
        for _, s in ipairs(list) do
            if contest == nil and s.blocked == "enemy_near" and s.distance <= reach and safe_site(view, s) then
                contest = s
                if not keep[s.index] then
                    keep[s.index] = true
                    places = places + places_of(s, resource)
                    if not s.claimed then
                        order("claim_" .. resource, {verb = "claim", site = s.index})
                    end
                end
            end
        end
    end
    return places, contest, open_places
end

-- every claimed patch or mine the plan does not keep is released (the start-up claims far ones)
local function release_unkept(view, order, keep)
    for _, s in ipairs(view.sites) do
        if s.claimed and not keep[s.index]
                and (s.patch or s.holds.stone > 0 or s.holds.gold > 0 or s.holds.iron > 0) then
            order("release", {verb = "release", site = s.index})
        end
    end
end

function M.init(view, memory)
    memory.backoff = {}
    memory.tags = {}
    memory.threat_until = 0
    local parts = {}
    for _, t in ipairs({"citizen", "b_barracks", "b_settlement", "b_guard_tower_paleo", "inf01_clubman",
                        "inf01_rock_thrower"}) do
        local c = V.cost(view, t)
        if c then
            parts[#parts + 1] = string.format("%s=%d/%d/%d/%d/%d", t, c.cost.food, c.cost.wood, c.cost.stone,
                c.cost.gold, c.cost.iron)
        end
    end
    local n = view.next_epoch_cost
    print(string.format("boom init next=%d/%d/%d/%d/%d sol=%d %s", n.food, n.wood, n.stone, n.gold, n.iron,
        view.sol_missing, table.concat(parts, " ")))
    return {}
end

function M.step(view, memory)
    local orders, tags, spots = {}, {}, {}
    -- an order, with the tag its refusals are remembered by (and the spot, for a building at a site)
    local function order(tag, o, spot)
        if (memory.backoff[tag] or 0) > view.time then
            return false
        end
        orders[#orders + 1] = o
        tags[#tags + 1] = tag
        spots[#spots + 1] = spot or ""
        return true
    end

    -- the previous step's refusals: an order with no site or producer waits a minute; a site with no room for a
    -- building is passed over for five
    memory.bad_spots = memory.bad_spots or {}
    for i, r in ipairs(view.results) do
        local tag = memory.tags[i]
        if tag ~= nil and r.code == "refused" and tag == "improve" then
            -- a tower improvement: no level left waits for the next tower class, else a minute
            if r.reason == "requirements" or r.reason == "bad_argument" or r.reason == "not_in_epoch" then
                memory.improved = memory.improve_class
            end
            memory.backoff[tag] = view.time + BACKOFF_MS
        elseif tag ~= nil and r.code == "refused"
                and (r.reason == "no_site" or r.reason == "no_producer" or r.reason == "unsafe") then
            local spot = memory.spots ~= nil and memory.spots[i] or ""
            if spot ~= "" then
                memory.bad_spots[spot] = view.time + 5 * BACKOFF_MS
            else
                memory.backoff[tag] = view.time + BACKOFF_MS
            end
            if tag == "granary" and r.reason == "no_site" then
                memory.grow_claim = true   -- a granary (and its farms) finds room only round the claimed land
            end
        end
    end
    for _, e in ipairs(view.events) do
        if e.kind == "no_site" and (e.type == "b_granary" or e.type == "b_farm") then
            memory.grow_claim = true
        end
        if e.kind == "unit_lost" and string.find(e.type, "fishing_boat", 1, true) then
            memory.boats_lost = memory.boats_lost or {}
            for _ = 1, e.count do
                memory.boats_lost[#memory.boats_lost + 1] = view.time
            end
        end
    end
    -- the boats sunk in the last BOATS_LOST_MS (warships on the fishing waters: no new boats for them to sink)
    local sunk = {}
    for _, t in ipairs(memory.boats_lost or {}) do
        if view.time - t < BOATS_LOST_MS then
            sunk[#sunk + 1] = t
        end
    end
    memory.boats_lost = sunk
    for _, k in ipairs(ai.keys(memory.bad_spots)) do
        if memory.bad_spots[k] <= view.time then
            memory.bad_spots[k] = nil
        end
    end
    local function spot_of(site)
        return site.x .. "," .. site.y
    end

    if view.town_x < 0 then
        memory.tags, memory.spots = tags, spots
        return orders   -- no town centre: nothing to build on
    end
    local age = view.epoch < 2 and view.epoch + 1 or 3
    local plan = PLAN[age]
    local citizen = V.cost(view, "citizen")
    local in_epoch = (view.time - view.epoch_start) // 60000
    local towers, towers_built, towers_busy = tower_counts(view)

    -- the threat: enemy soldiers on the claimed land stronger than a scout, kept in mind for a while; what the towers
    -- and the army cannot hold is what the army must grow by
    if view.threat_strength >= THREAT_MIN then
        memory.threat_until = view.time + THREAT_MS
        memory.threat = math.max(memory.threat or 0, view.threat_strength)
    elseif view.time > (memory.threat_until or 0) then
        memory.threat = 0
    end
    local threatened = view.time <= (memory.threat_until or 0) and (memory.threat or 0) > 0
    -- the strongest army seen of an enemy in the last five minutes (what scouting there is)
    local seen = 0
    for _, e in ipairs(view.enemies) do
        if e.last_seen > 0 and view.time - e.last_seen <= 300000 and e.seen_army > seen then
            seen = e.seen_army
        end
    end

    -- the army wanted: the light defence, more for an army seen, more again under a threat
    local per = view.army_count > 0 and view.army_strength // view.army_count or 1000
    if per < 500 then
        per = 500
    end
    local soldiers_wanted = plan.soldiers + seen // 2   -- (S4: the scout sees the army coming)
    local short_strength = 0
    if threatened then
        short_strength = memory.threat * 3 // 2 - view.army_strength - towers_built * TOWER_STRENGTH
        if short_strength > 0 then
            soldiers_wanted = math.max(soldiers_wanted, view.army_count + (short_strength + per - 1) // per)
        end
    end
    if soldiers_wanted > plan.soldiers_max then
        soldiers_wanted = plan.soldiers_max
    end
    local army_short = view.army_count < soldiers_wanted
    -- the reserve gives way to the army under a threat it cannot hold, or with less than the light army (towers do not
    -- stand in for an empty army: they cover only the land round them)
    local scramble = threatened and plan.soldiers_max > 0 and (short_strength > 0 or view.army_count < plan.soldiers)

    -- saving for the next epoch: once the town has its citizens (or the epoch has run long) the reserve holds the
    -- advance's cost, and the advance is asked for only when it is paid. From the Stone Age the gold and iron are kept
    -- from the epoch's start (nothing but soldiers spends them)
    local last = view.epoch >= view.last_epoch
    local saving = not last and not view.advancing
        and (view.citizens >= plan.save_at or in_epoch >= plan.save_after)
    local cost = view.next_epoch_cost
    -- the light army comes before the reserve: what its missing soldiers cost is left out of it
    local missing = plan.soldiers - view.army_count
    local soldier_cost = nil
    for _, c in ipairs(view.costs) do
        if not c.building and c.military and c.producer == "b_barracks"
                and (soldier_cost == nil or V.sum(c.cost) < V.sum(soldier_cost)) then
            soldier_cost = c.cost
        end
    end
    -- the epoch's tower upgrade (cheap) before the reserve
    local upgrade = V.tower_upgrade(view, memory)
    if upgrade ~= nil and order("tower_upgrade", upgrade) then
        memory.tower_up_at = #orders
    end
    -- a resource of the advance the army never spends from the reserve: one nearly paid (70%), or one with no site left
    -- to gather it (a playtest: a raid's scramble spent the Copper Age's iron, 433 to 22, and the last safe iron mine ran
    -- out: the Stone Age for the rest of the game)
    local no_site = memory.no_site or {}
    local function guarded(r)
        return cost[r] > 0 and (view.stock[r] * 10 >= cost[r] * 7 or no_site[r] == true)
    end
    local reserve, differs = {}, false
    for _, r in ipairs(V.resources) do
        local want = 0
        local kept = not last and not view.advancing and (saving or (age >= 2 and (r == "gold" or r == "iron")))
        if kept then
            want = cost[r]
            if missing > 0 and soldier_cost ~= nil and soldier_cost[r] > 0 and not guarded(r) then
                -- the missing light army is never locked out by the reserve: at most the stock less what it costs
                -- (in steps of 100, so the reserve changes seldom)
                local cap = (view.stock[r] - missing * soldier_cost[r]) // 100 * 100
                want = math.max(math.min(want - missing * soldier_cost[r], cap), 0)
            end
        end
        if scramble and r ~= "stone" and not (kept and guarded(r)) then
            want = 0
        end
        reserve[r] = want
        differs = differs or reserve[r] ~= view.reserve[r]
    end
    if differs then
        order("reserve", {verb = "reserve", food = reserve.food, wood = reserve.wood, stone = reserve.stone,
                          gold = reserve.gold, iron = reserve.iron})
    end
    local paid = true
    for _, r in ipairs(V.resources) do
        paid = paid and view.stock[r] >= cost[r]
    end
    if saving and view.sol_missing == 0 and paid then
        order("advance", {verb = "advance"})
    end

    -- citizens, never held back: two at a time per town centre up to the plan, then from the food above the reserve
    local making = citizen and citizen.in_production or 0
    local waiting = citizen and citizen.requested or 0
    local planned = view.citizens + making + waiting
    local cap = plan.citizens
    if planned >= cap and view.stock.food >= reserve.food + 100 then
        cap = plan.citizens_max
    end
    if plan.army_first then
        -- the soldiers first: while the army and the soldiers coming are short of the plan, the citizens are only
        -- replaced up to the last epoch's count (for ARMY_FIRST_MIN minutes at most)
        local coming = 0
        for _, c in ipairs(view.costs) do
            if not c.building and c.military then
                coming = coming + c.requested + c.in_production
            end
        end
        if view.army_count + coming < plan.soldiers and in_epoch < ARMY_FIRST_MIN then
            cap = math.min(cap, PLAN[age - 1].citizens)
        end
    end
    -- (§8.35, the owner: "if there is only 12 spots for a resource available, use only that 12"): in the Prehistoric no
    -- more citizens than the patches, the game near the town, the planned wood and the mines' places can take (from
    -- the Stone Age the claims grow with the town: a cap there kept iron at one site and the Copper Age 7 min later)
    if age == 1 and memory.useful ~= nil and cap > memory.useful then
        cap = memory.useful
    end
    -- the soldiers' share (§8.35, the owner: "~20 spearmen in the Stone Age so it can stop a raid"): past the
    -- soldiers-first window, while the light army is short and the food gatherers fill the food places, a citizen is
    -- trained only from the food above what two soldiers cost, when the soldiers' other resource is in stock (the
    -- citizens took every 50 food as it came in, and the army was 5 at 30 minutes)
    if plan.army_first and army_short and view.army_count < plan.soldiers and soldier_cost ~= nil and citizen
            and view.gatherers.food >= ((memory.want or {}).food or 0) then
        local soldiers_payable = true
        for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
            if soldier_cost[r] > 0 and view.stock[r] - reserve[r] < 2 * soldier_cost[r] then
                soldiers_payable = false
            end
        end
        if soldiers_payable and view.stock.food - reserve.food < 2 * soldier_cost.food + citizen.cost.food then
            cap = math.min(cap, planned)
        end
    end
    if citizen and waiting == 0 and making < 2 * math.max(citizen.producers, 1) and planned < cap
            and view.population < view.population_cap then
        order("citizen", {verb = "train", type = "citizen", n = math.min(cap - planned, 2)})
    end

    local tower = tower_class(view)

    if view.time - (memory.last_pass or 0) >= PASS_MS or memory.last_pass == nil then
        memory.last_pass = view.time
        -- the gatherers: each of wood, stone, gold and iron by what it lacks of the stock the plan will spend soon
        -- (the next epoch's cost once it is reserved, the buildings, boats and soldiers still to come, a bank for the
        -- next epoch's first minutes), within the plan's floor and cap; food takes the rest and never less than its
        -- share (less once the advance's food is paid: food is finite until the farms)
        local land = view.citizens
        local target = stock_targets(view, plan, reserve, towers, soldiers_wanted)
        local want = {}
        local others = 0
        for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
            local lack = target[r] - view.stock[r]
            local n = 0
            if lack > 0 then
                n = (lack + PER_GATHERER - 1) // PER_GATHERER
                if n < plan.floor[r] then
                    n = plan.floor[r]
                end
            elseif r == "wood" then
                n = plan.floor[r]
            end
            if n > plan.cap[r] then
                n = plan.cap[r]
            end
            want[r] = n
            others = others + n
        end
        local share = plan.food_share
        if saving and view.stock.food >= reserve.food + 200 then
            share = share - 15
        end
        local food_min = math.min(math.max((land * share + 99) // 100, plan.food or 0), land)
        -- food short of its minimum: the resource with the most gatherers beyond what its lack needs gives one up
        -- (with the plan's floors larger than the town, iron gave way first while gold piled up past 1000)
        local needed = {}
        for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
            needed[r] = math.max(target[r] - view.stock[r], 0) // PER_GATHERER
        end
        while others > 0 and land - others < food_min do
            local big, big_n = nil, nil
            -- (while the army is short the soldiers' iron gives way only when nothing else is left: the Copper Age's
            -- gold counted as the larger lack, so iron went to 0 in a third of the bench's Stone Age minutes and the
            -- spearmen came one per 30 iron gathered, §8.35)
            for _, r in ipairs({"iron", "gold", "stone", "wood"}) do
                local spare = want[r] - needed[r]
                if army_short and r == "iron" then
                    spare = -1000000
                end
                if want[r] > 0 and (big_n == nil or spare > big_n) then
                    big, big_n = r, spare
                end
            end
            want[big] = want[big] - 1
            others = others - 1
        end
        want.food = land - others
        if want.food < 0 then
            want.food = 0
        end
        -- claims: forage patches for the food gatherers (the hunters take the rest), the minerals' nearest sites for
        -- theirs; the claim grows only when no patch is left in it, and never past the plan's reach. A mineral with no
        -- site the gatherers can reach gets none: they would wander between resources
        local range = view.claim_range
        local keep = {}
        -- the food places (§8.35): the open patches kept (6 each), the game near the town (HUNT_MAX at most, one
        -- hunter an animal) and, from the Copper Age, the farms
        local hunt_places = math.min((view.hunt or 0) * HUNTERS_PER_ANIMAL, plan.hunt or HUNT_MAX)
        local farm_places = V.total(V.buildings(view, "b_farm"))
        local food_wanted = want.food
        local _, _, patch_places = claim_sites(view, order, keep, "food", true, want.food, plan.patches, range)
        if (patch_places + hunt_places < want.food - farm_places or memory.grow_claim)
                and range < math.min(plan.claim_max, view.claim_range_max) then
            order("claim_range", {verb = "claim_range", range = range + view.claim_range_step})
        end
        memory.grow_claim = nil
        -- no_site (the reserve keeps such a resource from the army): wanted, and no open site the gatherers can reach
        -- (none at all, or only a contested one they go to once its escort has cleared it)
        memory.no_site = {}
        local contest = nil
        local mineral_places = {}
        for _, r in ipairs(MINERALS) do
            local places, c = claim_sites(view, order, keep, r, false, want[r], 3, range)
            memory.no_site[r] = want[r] > 0 and (places == 0 or c ~= nil)
            mineral_places[r] = places
            if c ~= nil and (contest == nil or c.distance < contest.distance) then
                contest = c
            end
            if want[r] > places then
                want.food = want.food + want[r] - places
                want[r] = places
            end
        end
        -- no more on food than it has places for (§8.35: 16 on two patches of 6, the rest waiting; the owner: "if
        -- there is only 12 spots ... use only that 12"): the rest go to the mines' places (iron, gold, stone: the
        -- advance and the soldiers spend them), then to wood up to the plan's cap, and the citizens are not trained past
        -- what has work (memory.useful)
        local food_cap = patch_places + hunt_places + farm_places
        if want.food > food_cap then
            local spill = want.food - food_cap
            want.food = food_cap
            for _, r in ipairs({"iron", "gold", "stone"}) do
                local n = math.min(spill, math.max(math.min(plan.cap[r], mineral_places[r]) - want[r], 0))
                want[r] = want[r] + n
                spill = spill - n
            end
            local n = math.min(spill, math.max(plan.cap.wood - want.wood, 0))
            want.wood = want.wood + n
        end
        local useful = food_cap + plan.cap.wood + 2
        for _, r in ipairs(MINERALS) do
            useful = useful + math.min(plan.cap[r], mineral_places[r])
        end
        memory.useful = useful
        memory.food_cap = food_cap
        release_unkept(view, order, keep)
        memory.contest = contest ~= nil and {x = contest.x, y = contest.y} or nil

        local tower_wanted = tower ~= nil and towers < plan.towers
        local boats_changed = (memory.boats_seen or -1) ~= view.boats
        memory.boats_seen = view.boats
        for _, r in ipairs(V.resources) do
            if want[r] ~= view.wanted[r] or (r == "food" and boats_changed) then
                order("gather_" .. r, {verb = "gather", resource = r, n = want[r]})
            end
        end
        memory.want = want

        -- a settlement at a claimed patch or worked mine with no drop site at hand, the nearest first, one at a time
        local settlements = V.buildings(view, "b_settlement")
        if not going_up(settlements) and V.total(settlements) < plan.settlements
                and V.cost(view, "b_settlement") ~= nil then
            local best = nil
            for _, s in ipairs(view.sites) do
                if keep[s.index] and not s.has_drop and s.danger == 0 and memory.bad_spots[spot_of(s)] == nil
                        and (best == nil or s.distance < best.distance) then
                    best = s
                end
            end
            if best ~= nil then
                order("settlement", {verb = "build", type = "b_settlement", near = "site", site = best.index},
                      spot_of(best))
            else
                -- (§8.35: every wood trip was a far one) by the forest edge the wood gatherers work, when it has no
                -- drop site at hand
                local w = view.wood
                if w ~= nil and not w.has_drop and want.wood >= WOOD_DROP_AT and w.distance > 6
                        and our_side(view, w.x, w.y, w.distance, false)
                        and memory.bad_spots["w" .. w.x .. "," .. w.y] == nil then
                    order("settlement_wood", {verb = "build", type = "b_settlement", near = "tile", x = w.x, y = w.y},
                          "w" .. w.x .. "," .. w.y)
                end
            end
        end

        -- (§8.35, the owner: "upgrading tower range is worth it for AI") the towers' range, a level at a time, once
        -- three towers stand, from the Stone Age, never out of the reserve (the verb keeps it)
        if age >= 2 and tower ~= nil and towers_built >= 3 and not scramble and memory.improved ~= tower then
            memory.improve_class = tower
            order("improve", {verb = "improve", type = tower, what = "range"})
        end

        -- towers: the first at the town, then at the worked sites (the nearest unguarded first), one at a time; under a
        -- threat one more at the town
        if tower_wanted and not towers_busy and view.citizens >= 10 then
            if towers == 0 or (threatened and towers < 2) then
                order("tower_town", {verb = "build", type = tower, near = "town"})
            else
                local best = nil
                for _, s in ipairs(view.sites) do
                    if keep[s.index] and not s.has_tower and s.danger == 0
                            and memory.bad_spots["t" .. spot_of(s)] == nil
                            and (best == nil or s.distance < best.distance) then
                        best = s
                    end
                end
                if best ~= nil then
                    order("tower_site", {verb = "build", type = tower, near = "site", site = best.index},
                          "t" .. spot_of(best))
                else   -- (the classic tower placement: the worked centres nearest the town)
                    order("tower_town", {verb = "build", type = tower, near = "town"})
                end
            end
        end

        -- the next epoch's SOL: one building at a time that counts toward it and is not built yet, the cheapest first
        if saving and view.sol_missing > 0 then
            local best, best_cost, building = nil, 0, false
            for _, c in ipairs(view.costs) do
                if c.building and c.counts_toward_epoch and not NOT_FOR_SOL[c.type] then
                    local have = V.buildings(view, c.type)
                    building = building or going_up(have)
                    if V.total(have) == 0 then
                        local s = V.sum(c.cost)
                        if best == nil or s < best_cost then
                            best, best_cost = c, s
                        end
                    end
                end
            end
            if best ~= nil and not building then
                order("sol", {verb = "build", type = best.type, near = "town"})
            end
        end

        -- fishing: from the Stone Age's first minute, two docks and the boats (they pay for themselves in food)
        if plan.boats > 0 then
            local boats = plan.boats
            if #memory.boats_lost >= BOATS_LOST_MAX then
                boats = math.min(boats, view.boats)   -- the boats left fish on; none are replaced for now
            end
            order("fish", {verb = "fish", boats = boats, docks = plan.docks})
        end

        -- the trainers: a barracks once the town has ten citizens, then an archery range, then the second of each as
        -- the town grows or a threat comes
        local barracks = V.buildings(view, "b_barracks")
        local ranges = V.buildings(view, "b_archery_range")
        if view.citizens >= (plan.barracks_at or 10) and V.total(barracks) < 1 then
            order("barracks", {verb = "build", type = "b_barracks", near = "town"})
        elseif age >= 2 and V.cost(view, "b_archery_range") ~= nil and not going_up(barracks)
                and not going_up(ranges) then
            if plan.army_first and V.total(barracks) < plan.barracks then
                order("barracks", {verb = "build", type = "b_barracks", near = "town"})   -- the army's second barracks
            elseif V.total(ranges) < 1 and plan.ranges > 0 then
                order("range", {verb = "build", type = "b_archery_range", near = "town"})
            elseif (threatened or view.citizens >= 55) and V.total(barracks) < plan.barracks then
                order("barracks", {verb = "build", type = "b_barracks", near = "town"})
            elseif (threatened or view.citizens >= 65) and V.total(ranges) < plan.ranges then
                order("range", {verb = "build", type = "b_archery_range", near = "town"})
            end
        end

        -- Copper and later: a granary, then farms as the patches run out
        if plan.farms ~= nil and V.cost(view, "b_farm") ~= nil then
            local granaries = V.buildings(view, "b_granary")
            local farms = V.buildings(view, "b_farm")
            local patch_room = 0
            for _, s in ipairs(sites_of(view, "food", true)) do
                if s.claimed then
                    patch_room = patch_room + s.room
                end
            end
            if V.total(granaries) < 1 and V.cost(view, "b_granary") ~= nil then
                order("granary", {verb = "build", type = "b_granary", near = "town"})
            elseif (farms == nil or farms.queued < 2) and V.total(farms) < plan.farms
                    and patch_room < food_wanted - view.gatherers.food + 4 then
                order("farm", {verb = "build", type = "b_farm", near = "town"})
            elseif V.total(farms) >= 8 and V.total(granaries) < plan.granaries then
                order("granary", {verb = "build", type = "b_granary", near = "town"})
            end
        end
    end

    -- the soldiers, from each trainer in turn, while the army is short (the reserve keeps them off the advance's
    -- resources unless the town scrambles)
    if army_short then
        local classes = soldier_classes(view)
        local queued = 0
        for _, k in ipairs(TRAINERS) do
            local c = classes[k]
            if c ~= nil then
                queued = queued + c.cost.requested + c.cost.in_production
            end
        end
        local need = soldiers_wanted - view.army_count - queued
        for _, k in ipairs(TRAINERS) do
            local c = classes[k]
            if need > 0 and c ~= nil and c.cost.producers > 0 and c.cost.requested == 0 then
                local n = math.min(need, scramble and 3 or 2)
                order("soldier_" .. k, {verb = "train", type = c.type, n = n})
                need = need - n
            end
        end
    end

    -- the groups (lib.army; docs/AI_LUA_PLAN.md "S4 as built"): the soldiers guard the town together (a defence of the
    -- claim range: they fight what comes onto the claimed land and come back), and one of them scouts, the nearest
    -- claimed site not explored yet first (the gatherers go only to what the player has seen), then the land beyond
    A.collect(view, memory)
    local home = A.role(view, memory, "home")
    local scout = A.role(view, memory, "scout")
    if scout ~= nil and scout.state == "done" then
        order("scout_done", {verb = "disband", group = scout.id})
    end
    if scout == nil and view.time >= SCOUT_AT and (memory.scouts or 0) < SCOUTS
            and (view.free_soldiers > 0 or (home ~= nil and home.units > 1)) then
        local o = {verb = "scout", n = 1}
        local first = nil
        for _, s in ipairs(view.sites) do
            if s.claimed and not s.explored and (first == nil or s.distance < first.distance) then
                first = s
            end
        end
        if first ~= nil then
            o.x, o.y = first.x, first.y
        end
        if view.free_soldiers == 0 then
            o.from = home.id
        end
        if order("scout", o) then
            A.track(memory, orders, "scout")
            memory.scouts = (memory.scouts or 0) + 1
        end
    end
    if view.free_soldiers > 0 then
        if home ~= nil then
            order("guard", {verb = "defend", group = home.id, n = view.free_soldiers})
        elseif order("guard", {verb = "defend", n = view.free_soldiers}) then
            A.track(memory, orders, "home")
        end
    end

    -- the escort (§8.34): a contested mineral site (the claims' memory.contest) is held by a defend group from the
    -- home guard until the soldiers that kept the gatherers off it are gone; disbanded when the site is open again or
    -- the town is threatened (its soldiers go back to the guard). A lost escort is sent again after ESCORT_WAIT.
    local escort = A.role(view, memory, "escort")
    local contested = memory.contest
    if escort == nil and contested ~= nil and not threatened and home ~= nil and home.units >= ESCORT_MIN + 2
            and view.time >= (memory.escort_next or 0) then
        if order("escort", {verb = "defend", n = math.min(home.units - 2, ESCORT_MAX), x = contested.x,
                            y = contested.y, radius = 8, from = home.id}) then
            A.track(memory, orders, "escort")
            memory.escort_next = view.time + ESCORT_WAIT
        end
    elseif escort ~= nil and (contested == nil or threatened or escort.tx ~= contested.x or escort.ty ~= contested.y) then
        order("escort_done", {verb = "disband", group = escort.id})
    end

    -- the plan line, once a minute
    if view.time % 60000 < 1000 then
        local w = memory.want or {}
        print(string.format("plan e=%d save=%s paid=%s threat=%d seen=%d army=%d/%d cit=%d/%d towers=%d boats=%d "
            .. "claim=%d want=%d/%d/%d/%d/%d", view.epoch, tostring(saving), tostring(paid), memory.threat or 0, seen,
            view.army_count, soldiers_wanted, view.citizens, cap, towers, view.boats, view.claim_range, w.food or 0,
            w.wood or 0, w.stone or 0, w.gold or 0, w.iron or 0))
    end
    -- every five minutes the nearest patches and mines: index, distance, C claimed, E explored, D drop site, H home,
    -- why the gatherers would pass it over (ok: they would not)
    if view.time % 300000 < 1000 then
        local parts = {}
        for _, r in ipairs({"food", "stone", "gold", "iron"}) do
            local n = 0
            for _, s in ipairs(sites_of(view, r, r == "food")) do
                if n < 4 then
                    parts[#parts + 1] = string.format("%s:%d:d%d:%s%s%s%s:%s", r, s.index, s.distance,
                        s.claimed and "C" or "-", s.explored and "E" or "-", s.has_drop and "D" or "-",
                        s.home and "H" or "-", s.blocked or "ok")
                end
                n = n + 1
            end
        end
        print("sites " .. table.concat(parts, " "))
    end
    memory.tags, memory.spots = tags, spots
    return orders
end

return M
