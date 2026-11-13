-- EECPCivilizationManager in Lua (the classic port; ORIGIN in this folder): the first update's requests, the epoch
-- advance timer and AttemptEpochAdvance (the SOL buildings, the epoch research; the personality's epoch pressure),
-- the building counts every 30 s, repairs every 31 s, the garrison and fortress (a settlement filled with citizens
-- into a town centre, a town centre into a capitol), a house every 59 s. require("classic_lua.civ").
--
-- Memory (memory.civ): the timers and flags of the C++ manager.
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local U = require("classic_lua.units")
local B = require("classic_lua.build")
local RS = require("classic_lua.research")
local C = T.C

local M = {}

function M.state(memory)
    local civ = memory.civ
    if civ == nil then
        civ = {first_done = false, schedule_advance = true, next_advance = 0, advancing = false,
               sol_requested = false, last_count_check = 0, counts_checked = false, next_repair = 0,
               next_garrison = 0, next_fortress = 0, next_morale = 0, enabled_advance = false, counts_enabled = false,
               morale_enabled = false, farm_type = C.farm}
        memory.civ = civ
    end
    return civ
end

-- ---------------------------------------------------------------------------------------------------------------
-- the epoch timer

-- the human players' latest epoch (GetMaxEpoch; ai.players shows the epoch where the UI does: the view's enemies
-- carry it). DEVIATION: the view's enemies' epochs stand for every human's
local function max_human_epoch(view)
    local m = 0
    local humans = {}
    for _, p in ipairs(ai.players()) do
        if p.human then
            humans[p.id] = true
        end
    end
    for _, e in ipairs(view.enemies or {}) do
        if humans[e.player] and (e.epoch or 0) > m then
            m = e.epoch
        end
    end
    return m
end

-- SetNextEpochAdvanceTime(t, actual): t itself, else a random delay of the epoch frequency (minutes) x its modifier,
-- with the catchup cheat lengthened when ahead of the humans and shortened behind (a quarter 3+ epochs behind)
function M.set_next_advance(view, memory, t, actual)
    local civ = M.state(memory)
    if actual then
        civ.next_advance = t
        return
    end
    local b = memory.t.behavior
    local minutes_k = T.random(b.epoch_frequency_min * 1000, b.epoch_frequency_max * 1000)
    minutes_k = minutes_k * T.epoch_frequency_modifier_k(memory) // 1000
    if T.cheat(memory, "catchup") then
        local scale = b.epoch_advance_time_scale_k
        local m = max_human_epoch(view)
        if scale > 0 then
            if view.epoch > m then
                minutes_k = minutes_k * scale // 1000
            elseif view.epoch < m then
                minutes_k = minutes_k * 1000 // scale
            end
        end
        if m - view.epoch >= 3 then
            minutes_k = minutes_k // 4
        end
    end
    civ.next_advance = t + minutes_k * 60
end

-- EECPEpochPressure in thousandths: the Prehistoric's schedule (the game time over stone_span minutes), else the
-- elapsed share of the epoch timer; plus the safe bonus from the Stone Age while no stronger army is near
function M.epoch_pressure_k(view, memory)
    local civ = M.state(memory)
    local p = memory.t.personality
    local bonus = 0
    if (p.safe_bonus_k or 0) > 0 and view.epoch >= 1 and not U.army_threat(view, memory) then
        bonus = p.safe_bonus_k
    end
    if view.epoch <= 0 and (p.stone_span_k or 0) > 0 then
        return view.time * 1000 // (p.stone_span_k * 60) + bonus
    end
    local span = T.epoch_timer_span(memory)
    local left = civ.next_advance - view.time
    local pressure = 1000 - left * 1000 // span + bonus
    return pressure < 0 and 0 or pressure
end

-- AttemptEpochAdvance: once the advance time (less the personality's early start) has come: the cheapest SOL
-- buildings when the next epoch's SOL is short, else the obsolete requests dropped and the epoch researched (held a
-- minute while ahead of every human with the catchup cheat)
local function attempt_advance(view, memory)
    local civ = M.state(memory)
    local t = memory.t
    if view.epoch >= t.epoch_end then
        return
    end
    local nxt = view.epoch + 1
    local e = ai.epoch(nxt)
    if e == nil then
        return
    end
    local due = civ.next_advance
    if not T.cheat(memory, "epochs") then
        local p = t.personality
        local early = (1000 - p.epoch_pressure_start_k) * T.epoch_timer_span(memory) // 1000
        due = due > early and due - early or 0
        if view.epoch <= 0 and (p.stone_span_k or 0) > 0 then
            due = p.epoch_pressure_start_k * p.stone_span_k * 60 // 1000
        end
        local bonus = 0
        if (p.safe_bonus_k or 0) > 0 and view.epoch >= 1 and not U.army_threat(view, memory) then
            bonus = p.safe_bonus_k
        end
        if bonus > 0 then
            local sooner = bonus * ((view.epoch <= 0 and (p.stone_span_k or 0) > 0) and p.stone_span_k * 60
                or T.epoch_timer_span(memory) // 1000)
            due = due > sooner and due - sooner or 0
        end
    end
    if view.time < due then
        return
    end
    local missing = e.required_sol - e.current_sol
    if missing > 0 then
        if not civ.sol_requested and civ.enabled_advance then
            RS.research_cheapest(view, memory, missing)
            civ.sol_requested = true
        end
    else
        civ.sol_requested = false
        U.drop_obsolete(view, memory, nxt)
        B.drop_obsolete(view, memory, nxt)
        if T.cheat(memory, "catchup") and view.epoch > max_human_epoch(view) then
            M.set_next_advance(view, memory, view.time + 60000, true)
        else
            RS.research(view, memory, e.id)
            civ.advancing = true
            print(string.format("classic_lua epoch.advance from=%d to=%d t=%d", view.epoch, nxt, view.time // 1000))
        end
    end
end

-- CatchUpEpoch (the catchup cheat; the engine calls it when a human reaches an epoch): the next advance pulled
-- forward to now + a random 1-2 minutes (WW1) or the catch-up minutes, when the human is ahead
local function catch_up(view, memory)
    local civ = M.state(memory)
    if not T.cheat(memory, "catchup") then
        return
    end
    local m = max_human_epoch(view)
    if civ.human_epoch == nil then
        civ.human_epoch = m
    end
    if m <= civ.human_epoch then
        return
    end
    civ.human_epoch = m
    local b = memory.t.behavior
    local lo, hi
    if m == 9 then
        lo, hi = 60000, 120000
    else
        hi = b.epoch_catch_up_max * 60000
        lo = b.epoch_catch_up_min * 60000
        if memory.t.deathmatch then
            hi = hi * b.epoch_catch_up_deathmatch_k // 1000
        elseif memory.t.difficulty == 0 then
            hi = hi * b.epoch_catch_up_easy_k // 1000
        elseif memory.t.difficulty == 2 then
            hi = hi * b.epoch_catch_up_hard_k // 1000
        end
    end
    local at = T.random(lo, hi) + view.time
    if m > view.epoch and (civ.next_advance < view.time or civ.next_advance > at) then
        M.set_next_advance(view, memory, at, true)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- CheckBuildingCounts: one building per units-per share of the units it serves, up to the maximum; military
-- buildings the difficulty's first count; no docks without water, no space docks off space maps, no airport before
-- the attacks started; the farms through their granary

-- the counts (AdjustUnitCount over the own units): a non-building counts for the farm, university, temple, hospital,
-- tower, space turret and priest tower; a non-citizen non-building for the building that trains it; a building (not
-- a farm or anti-air) for the anti-air
local function unit_counts(view, memory)
    local counts = {}
    for _, e in ipairs(T.COUNTED) do
        counts[e.key] = 0
    end
    local tree = U.tree(view)
    local population = {b_farm = true, b_university = true, b_temple = true, b_hospital = true, tower = true,
                        space_turret = true, b_priest_tower = true}
    local byType = {}
    for _, id in ipairs(ai.units{}) do
        local u = R.unit(id)
        if u ~= nil then
            byType[u.type] = (byType[u.type] or 0) + 1
        end
    end
    for ty, n in pairs(byType) do
        local c = R.class_of(ty)
        if c.building then
            if ty ~= C.farm and ty ~= C.robotic_farm and ty ~= "b_aa10_gun_88mm" and ty ~= "b_aa13_missile_tower"
                and ty ~= "b_aa15_missile_tower" then
                counts.anti_air = counts.anti_air + n
            end
        else
            for k in pairs(population) do
                counts[k] = counts[k] + n
            end
            if ty ~= C.citizen and ty ~= C.female_citizen then
                local t = tree.byClass[ty]
                for _, p in ipairs(t ~= nil and t.producers or {}) do
                    if counts[p] ~= nil and R.class_of(p).building then
                        counts[p] = counts[p] + n
                    end
                end
            end
        end
    end
    return counts
end

local function check_building_counts(view, memory)
    local civ = M.state(memory)
    local t = memory.t
    if not civ.counts_enabled then
        return
    end
    local counts = unit_counts(view, memory)
    local d = t.difficulty
    local b = t.behavior
    for _, e in ipairs(T.COUNTED) do
        local class = e.key
        if e.tower then
            class = T.tower_class(view.epoch)
        elseif e.anti_air then
            class = T.anti_air_class(view.epoch)
        elseif e.farm then
            class = civ.farm_type
        elseif e.space_turret then
            class = "b_space_turret"
        end
        local tech = class ~= nil and U.class_tech(view, class) or nil
        if tech ~= nil and not tech.obsolete and view.epoch >= tech.epoch and tech.available then
            local per = e.per ~= nil and b[e.per] or e.per_value
            local max = e.max ~= nil and b[e.max] or e.max_value
            if per ~= nil and per > 0 then
                local count = counts[e.key] or 0
                local wanted = (count + per - 1) // per
                if max > 0 and wanted > max then
                    wanted = max
                end
                if class == C.dock and wanted > 2 and t.personality.coast_fishing then
                    wanted = 2
                end
                local have, other = 0, 0
                if e.farm then
                    if R.state(memory).uses_fishing then
                        have = wanted
                    else
                        for _, id in ipairs(ai.units{type = class}) do
                            local u = R.unit(id)
                            if u ~= nil and u.built then
                                have = have + 1
                            else
                                other = other + 1
                            end
                        end
                    end
                else
                    have = #ai.units{type = class}
                end
                local build = 0
                if e.military then
                    local first
                    if t.deathmatch then
                        first = T.by_difficulty(memory, b.min_military_buildings_easy_dm, b.min_military_buildings_medium_dm,
                            b.min_military_buildings_hard_dm)
                    else
                        first = T.by_difficulty(memory, b.min_military_buildings_easy, b.min_military_buildings_medium,
                            b.min_military_buildings_hard)
                    end
                    if have == 0 then
                        build = first
                    elseif have < wanted then
                        build = t.deathmatch and wanted - have or 1
                    end
                    if t.waterless and (class == C.dock or class == C.navy_yard) then
                        build = 0
                    end
                    if not t.space and class == C.space_dock then
                        build = 0
                    end
                    if class == C.airport and not memory.cp.attack_started then
                        build = 0
                    end
                else
                    if wanted > 0 and have < wanted then
                        build = 1
                    end
                    if not t.space and class == "b_space_turret" then
                        build = 0
                    end
                end
                if build > 0 and not B.is_queued(memory, class) then
                    if e.farm and other == 0 then
                        if not B.is_queued(memory, C.granary) then
                            B.queue(view, memory, C.granary, true, false)
                        end
                    else
                        for _ = 1, build do
                            B.queue(view, memory, class, true, false)
                        end
                    end
                end
            end
        end
    end
    civ.counts_checked = true
end

-- ---------------------------------------------------------------------------------------------------------------
-- RepairBuildings: idle citizens to the buildings under 80% hit points (not farms), the nearest on the same land

local function repair_buildings(view, memory)
    local damaged = {}
    for _, id in ipairs(ai.units{category = "building"}) do
        local u = R.unit(id)
        if u ~= nil and u.hp_max ~= nil and u.hp_max > 0 and u.hp * 10 < u.hp_max * 8 and u.type ~= C.farm
            and u.type ~= C.robotic_farm and not u.pending and (u.built or true) then
            damaged[#damaged + 1] = u
        end
    end
    if #damaged == 0 then
        return
    end
    local workers = {}
    for _, id in ipairs(ai.units{category = "citizen", idle = true}) do
        local u = R.unit(id)
        if u ~= nil and u.inside == nil then
            workers[#workers + 1] = u
        end
    end
    for _, b in ipairs(damaged) do
        if #workers == 0 then
            break
        end
        local best, bd, bk
        for k, w in ipairs(workers) do
            local dd = R.d2(w.x, w.y, b.x, b.y)
            if (bd == nil or dd < bd) and (b.type == C.dock or ai.reach(w.x, w.y, b.x, b.y)) then
                best, bd, bk = w, dd, k
            end
        end
        if best ~= nil then
            table.remove(workers, bk)
            ai.repair({best.id}, b.id)
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the garrison and the fortress (ChooseGarrison / ChooseFortress / FillGarrison): a random settlement connected to the
-- town (economy.early_town_centers: none before the Bronze Age while that many centres stand) filled with idle
-- citizens, its upgrade to a town centre; a random town centre into a capitol (not before the Bronze Age with
-- early_town_centers). On island and planets maps a town centre hires its garrison (citizens trained for the room)
-- and any available citizen fills it, a gatherer too

local function fill_garrison(view, memory, id)
    local u = R.unit(id)
    if u == nil then
        return
    end
    local cls = R.class_of(u.type)
    local room = (cls.garrison or 0) - (u.garrisoned or 0)
    if room <= 0 then
        return
    end
    local island = memory.t.island or memory.t.planets
    if island and u.type == C.town_center then
        -- (island and planets maps: a town centre hires its garrison - the citizens trained for the free room,
        -- at once when affordable, else requested; they fill it as they come out)
        if U.can_afford(view, memory, U.cost(C.citizen), room, C.citizen) then
            ai.train(id, C.citizen, math.min(room, 5))
        elseif not U.is_requested(memory, C.citizen) then
            U.request(view, memory, C.citizen, room)
        end
    end
    local ids = {}
    for _, cid in ipairs(ai.units{category = "citizen", near = {u.x, u.y, 60}}) do
        if #ids >= room then
            break
        end
        local c = R.unit(cid)
        -- (idle, or a gatherer waiting for room at its resource: kActionWaitForOpenPosition; on island and planets
        -- maps any available citizen, a gatherer too: IsCitizenAvailable)
        if c ~= nil and (c.action == "idle" or (c.action == "gather" and (c.waiting or island))) and c.inside == nil
                and ai.reach(c.x, c.y, u.x, u.y) then
            ids[#ids + 1] = cid
        end
    end
    if #ids > 0 then
        ai.garrison(ids, id)
        local rm = R.state(memory)
        for _, cid in ipairs(ids) do
            R.remove_gatherer(rm, cid)
        end
    end
    -- the room left: AddGarrison(count + room) with no citizens (the classic's garrison cheat: the manifest's
    -- "fills_garrison"); a settlement filled becomes a town centre
    -- (only with citizens on their way: their arrival fills the garrison and starts the upgrade; with none the
    -- garrison would stand full with no one to finish it)
    if #ids > 0 and room > #ids and memory.t.fills_garrison and ai.garrison_fill ~= nil then
        ai.garrison_fill(id, room - #ids)
    end
end

local function choose_garrison(view, memory)
    local civ = M.state(memory)
    if civ.garrison ~= nil then
        local g = R.unit(civ.garrison)
        if g == nil or g.type ~= C.settlement then
            civ.garrison = nil
        else
            fill_garrison(view, memory, civ.garrison)
            return
        end
    end
    if U.is_requested(memory, C.citizen) or U.is_requested(memory, C.female_citizen) then
        return
    end
    local early = memory.t.personality.early_town_centers or 0
    if early > 0 and view.epoch < 3 then
        if #ai.units{type = C.town_center} + #ai.units{type = C.capitol} >= early then
            return
        end
    end
    local tx, ty = R.town(view)
    if tx == nil then
        return
    end
    local candidates = {}
    for _, id in ipairs(ai.units{type = C.settlement, built = true}) do
        local u = R.unit(id)
        if u ~= nil and ai.reach(u.x, u.y, tx, ty) then
            candidates[#candidates + 1] = id
        end
    end
    if #candidates == 0 then
        return
    end
    local id = candidates[ai.random(1, #candidates)]
    local u = R.unit(id)
    local room = (R.class_of(C.settlement).garrison or 0) - (u.garrisoned or 0)
    local needed = room * 5 // 2
    if U.workers(view, memory) < needed and not U.can_afford(view, memory, U.cost(C.citizen), room, C.citizen) then
        return
    end
    civ.garrison = id
    fill_garrison(view, memory, id)
end

local function choose_fortress(view, memory)
    local civ = M.state(memory)
    if civ.fortress ~= nil then
        local f = R.unit(civ.fortress)
        if f == nil or f.type ~= C.town_center then
            civ.fortress = nil
        else
            fill_garrison(view, memory, civ.fortress)
            return
        end
    end
    if U.is_requested(memory, C.citizen) or U.is_requested(memory, C.female_citizen) then
        return
    end
    if (memory.t.personality.early_town_centers or 0) > 0 and view.epoch < 3 then
        return
    end
    local candidates = ai.units{type = C.town_center, built = true}
    if #candidates == 0 then
        return
    end
    local id = candidates[ai.random(1, #candidates)]
    local u = R.unit(id)
    local room = (R.class_of(C.town_center).garrison or 0) - (u.garrisoned or 0)
    local needed = room * 5 // 2
    if U.workers(view, memory) < needed and not U.can_afford(view, memory, U.cost(C.citizen), room, C.citizen) then
        return
    end
    civ.fortress = id
    fill_garrison(view, memory, id)
end

-- QueueMoraleBuilding: a house by the first drop site whose morale is not full. DEVIATION: the morale effect's fill
-- is not readable; a drop site with fewer than 4 houses within 8 tiles counts as not full
local function queue_morale_building(view, memory)
    if B.is_queued(memory, C.house) then
        return
    end
    -- GetMoraleFill: the drop site's EEAEMoraleFromBuildings effect, (its level + the unfinished houses in its square)
    -- over its maximum level; only the town centre (dbareaeffect tc_morale_bonus_rng_10: 2 houses within 10 tiles)
    -- and the capitol (capitol_morale_bonus_rng_10: 4) carry one, so a settlement wants no houses. DEVIATION: the
    -- effect is not readable; the port counts the houses in the square (the level) from the data's numbers
    local MORALE = {[C.town_center] = 2, [C.capitol] = 4}
    for _, d in ipairs(R.drops(false)) do
        local du = R.unit(d.id)
        local max = du ~= nil and MORALE[du.type] or nil
        if d.built and max ~= nil then
            local houses = 0
            for _, id in ipairs(ai.units{type = C.house, near = {d.x, d.y, 15}}) do
                local h = R.unit(id)
                if h ~= nil and math.abs(h.x - d.x) <= 10 and math.abs(h.y - d.y) <= 10 then
                    houses = houses + 1
                end
            end
            if houses < max then
                local x, y = B.house_site(memory, d)
                if x ~= nil then
                    B.queue(view, memory, C.house, true, false, x, y)
                    return
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Update

function M.update(view, memory)
    local civ = M.state(memory)
    if not civ.first_done then
        -- the future research sentinel (dbtechtree 15020, the last epoch's): queued or requested at the start, it
        -- keeps a later epoch's request standing, which lets AttemptEpochAdvance run
        local s = ai.techs{id = 15020}[1]
        if s ~= nil then
            if s.makes ~= nil then
                if R.class_of(s.makes).building then
                    B.queue(view, memory, s.makes, false, false)
                else
                    U.request(view, memory, s.makes, 1)
                end
            else
                RS.research(view, memory, s.id)
            end
        end
        local wanted = U.wanted_citizens(view, memory)
        R.add_costs(memory, U.ledger_cost(C.citizen), wanted)
        civ.first_done = true
    end
    civ.farm_type = C.farm   -- (EEGetFarmType: the robotic farm with its upgrade; not ported, Nano and later)
    if civ.schedule_advance then
        M.set_next_advance(view, memory, view.time, false)
        civ.schedule_advance = false
    end
    catch_up(view, memory)
    -- the epoch's producer lost (UnitLost: the advance rescheduled): no town centre or capitol stands
    if civ.advancing and #R.of_type(C.town_center) == 0 and #R.of_type(C.capitol) == 0 then
        civ.advancing = false
        civ.schedule_advance = true
    end
    U.update(view, memory)
    require("classic_lua.econ").update(view, memory)
    B.update(view, memory)
    RS.update(view, memory)
    if not civ.advancing and (B.has_later_requests(view, memory) or RS.has_later_ids(view, memory)
            or U.has_later_requests(view, memory) or view.epoch < max_human_epoch(view)) then
        attempt_advance(view, memory)
    end
    if civ.counts_checked then
        civ.counts_checked = false
        civ.last_count_check = view.time
    elseif civ.last_count_check == 0 or view.time - civ.last_count_check > 30000 then
        check_building_counts(view, memory)
    end
    if view.time >= civ.next_repair then
        repair_buildings(view, memory)
        civ.next_repair = view.time + 31000
    end
    local b = memory.t.behavior
    if view.time >= civ.next_garrison then
        if civ.next_garrison > 0 then
            choose_garrison(view, memory)
        end
        civ.next_garrison = view.time + b.town_center_upgrade_timer * 60000
    end
    -- UnitAdded: a new citizen heads for the garrison or the fortress being filled (DEVIATION: every 2 s the idle
    -- citizens go, as fill_garrison takes them)
    if (civ.garrison ~= nil or civ.fortress ~= nil) and view.time >= (civ.next_route or 0) then
        civ.next_route = view.time + 2000
        -- (ReleaseGarrison / ReleaseFortress: the building upgraded or gone)
        local g = civ.garrison ~= nil and R.unit(civ.garrison) or nil
        if g == nil or g.type ~= C.settlement then
            civ.garrison = nil
        end
        local f = civ.fortress ~= nil and R.unit(civ.fortress) or nil
        if f == nil or f.type ~= C.town_center then
            civ.fortress = nil
        end
        local held = civ.garrison ~= nil and civ.fortress ~= nil and (ai.random(0, 1) == 0 and civ.fortress or civ.garrison)
            or civ.garrison or civ.fortress
        if held ~= nil then
            fill_garrison(view, memory, held)
        end
    end
    if view.time >= civ.next_fortress then
        if civ.next_fortress > 0 then
            choose_fortress(view, memory)
        end
        civ.next_fortress = view.time + b.capitol_upgrade_timer * 60000
    end
    if civ.morale_enabled and view.time >= civ.next_morale then
        if civ.next_morale > 0 then
            queue_morale_building(view, memory)
        end
        civ.next_morale = view.time + 59000
    end
end

-- the epoch reached (EpochReached -> the research manager's, EnterEpoch): the next advance scheduled
function M.epoch_reached(view, memory)
    local civ = M.state(memory)
    civ.advancing = false
    civ.schedule_advance = true
end

return M
