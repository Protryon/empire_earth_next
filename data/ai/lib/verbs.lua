-- The coarse verbs in Lua (docs/AI_LUA_VERBS.md; AI_RESEARCH §8.92): the order tables an AI returns
-- ({verb = "gather", resource = "wood", n = 8}, build, train, fish, claim, ...) carried out with the per-unit commands,
-- as the C++ managers did (EECPUnitManager, EECPResourceManager, EECPBuildManager, EECPResearchManager and the
-- civilization manager's repairs). An AI module is wrapped once, at its return:
--
--     return require("lib.verbs").wrap(M)
--
-- Each step the runners go first (the production queue every step, the research queue, the build queue every 3 s, the
-- gatherers every 10 s, the repairs every 31 s, the order EECPNextUpdate kept), then the view gets the fields the
-- managers filled (gatherers, idle, wanted, claim_range, reserve, advancing, hunt, hunters, buildings[].queued,
-- costs[].requested, sites[].claimed, the no_site events), then the script steps, then its economy orders are carried
-- out here. Any other order (the army's group verbs) goes on to C++, and every result comes back in the order given.
-- The library holds the seat's citizens and boats (ai.take): no C++ runner touches them. Its state is memory.verbs.
local T = require("classic_lua.tune")

local M = {}
M.TRACE = false   -- TEST ONLY: print lines per economy order, build start and builder

-- the bindings this library stands in front of (below: who holds a citizen)
local RAW = {take = ai.take, release = ai.release, units = ai.units, unit = ai.unit, get = ai.get}
M.RAW = RAW
local cur = nil   -- the seat's state while its script runs (set by the wrapper)

local RES = {"food", "wood", "stone", "gold", "iron"}
local LISTS = {"idle", "food", "wood", "stone", "gold", "iron"}   -- (the C++ lists' order: idle first)
local FAMILY_ANIMAL, FAMILY_FISH = 0x11, 0x12

-- the C++ engine's numbers (ee/EECPNext.h, EECPTrace.h)
local CLAIM_MIN, CLAIM_STEP, CLAIM_MAX = 25, 5, 45
local DROP_REACH, DROP_SLACK, WOOD_SLACK = 3, 1, 2
local PATCH_PLACES = 6
local HUNT_RANGE, HUNT_MAX, HUNTERS_PER_ANIMAL = 24, 30, 3
local EXPLORE_MS = 90000
local DROP_BUILDERS = 4
local PATCH_DROP_RANGE = 3
local MAX_COUNT = 100
local REPAIR_RATIO_K = 800

local C = T.C
local DROP_TYPES = {C.town_center, C.capitol, C.settlement}

-- ---------------------------------------------------------------------------------------------------------------
-- small helpers

local function d2(ax, ay, bx, by)
    local dx, dy = ax - bx, ay - by
    return dx * dx + dy * dy
end

-- e into a list kept in its entries' epoch order (e.e; ties: first in, first out; an epoch advance first)
local function insert_by_epoch(list, e)
    local k = #list + 1
    while k > 1 and (list[k - 1].e > e.e or (list[k - 1].e == e.e and e.epoch and not list[k - 1].epoch)) do
        k = k - 1
    end
    table.insert(list, k, e)
end

local function remove_value(list, v)
    for k = #list, 1, -1 do
        if list[k] == v then
            table.remove(list, k)
        end
    end
end

-- the class records of this game (not saved: static data)
local sClass = {}
local function cls(t)
    local c = sClass[t]
    if c == nil then
        c = ai.class(t) or false
        sClass[t] = c
    end
    return c or nil
end

local function half(t)
    local c = cls(t)
    return c ~= nil and (c.size or 0) // 2 or 0
end

local function is_tower(t)
    return string.find(t, "tower", 1, true) ~= nil
end

local function is_wall(t)
    return string.find(t, "wall", 1, true) ~= nil and not is_tower(t)
end

-- the step's caches: unit records, drop sites, gatherers sent (not saved)
local S = {time = -1}

local function begin_cache(view)
    S = {time = view.time, units = {}, memo = {}, assigned = nil, view = view}
end

local function U(id)
    local u = S.units[id]
    if u == nil then
        u = ai.unit(id) or false
        S.units[id] = u
    end
    return u or nil
end

local function touch(id)
    S.units[id] = nil
end

local function memo(key, fn)
    local v = S.memo[key]
    if v == nil then
        v = fn()
        S.memo[key] = v
    end
    return v
end

-- the gatherers sent to each resource (ai.gatherers once a step, raised as this step sends more)
local function assigned(id)
    if S.assigned == nil then
        S.assigned = ai.gatherers()
    end
    return S.assigned[id] or 0
end

local function add_assigned(id)
    assigned(id)
    S.assigned[id] = (S.assigned[id] or 0) + 1
end

-- the own finished drop sites: {x, y (the middle), half, id}; food: the granaries too
local function drops(food)
    return memo(food and "drops_food" or "drops", function()
        local out = {}
        local types = {C.town_center, C.capitol, C.settlement}
        if food then
            types[#types + 1] = C.granary
        end
        for _, t in ipairs(types) do
            local h = half(t)
            for _, id in ipairs(ai.units{type = t, built = true}) do
                local u = U(id)
                if u ~= nil then
                    out[#out + 1] = {x = u.x + h, y = u.y + h, half = h, id = id}
                end
            end
        end
        return out
    end)
end

-- EECPNextDropReady: a finished drop site within kNextDropReach + slack + its half size of a tile
local function drop_ready(x, y, food, slack)
    for _, d in ipairs(drops(food)) do
        local reach = DROP_REACH + slack + d.half
        if d2(x, y, d.x, d.y) <= reach * reach then
            return true
        end
    end
    return false
end

local function unit_drop_ready(u)
    return drop_ready(u.x, u.y, u.kind == "food", DROP_SLACK)
end

function M.state(memory)
    local vs = memory.verbs
    if vs == nil then
        vs = {wanted = {}, food_land = 0, prefer = "patch", exact = 0, granary_farms = -1, claim_range = CLAIM_MIN,
              claims = {}, reserve = {}, lists = {}, requests = {}, waiting = {}, builds = {}, placed = {},
              research = {}, advancing = false, explore = {}, events = {}, pending = {}, next_assign = -1,
              next_check = -1, next_build = 0, next_builder_check = -1, next_repair = 0, t = nil,
              sh = {}, lib = {}, builder = {}}
        for _, r in ipairs(RES) do
            vs.wanted[r] = 0
            vs.reserve[r] = 0
        end
        for _, l in ipairs(LISTS) do
            vs.lists[l] = {}
        end
        memory.verbs = vs
    end
    return vs
end

-- what the seat reads from the rules once (ai.tuning): the mining limit, the moves a cycle, the claim range rule
local function tuning(vs)
    if vs.t == nil then
        local t = ai.tuning() or {}
        local b, p = t.behavior or {}, t.personality or {}
        vs.t = {mining_limit = math.max(1, b.mining_limit or 1), moves = p.moves_per_cycle or 3,
                claim_rule = p.claim_range == true, cheat_epochs = (t.cheats or {}).epochs == true}
    end
    return vs.t
end

local function town(view)
    if view.town_x == nil or view.town_x < 0 then
        return nil
    end
    return view.town_x, view.town_y
end

local function in_claim(view, vs, x, y)
    local tx, ty = town(view)
    if tx == nil then
        return true
    end
    return d2(x, y, tx, ty) <= vs.claim_range * vs.claim_range
end

local function hunt_reach(vs)
    return vs.claim_range > HUNT_RANGE and vs.claim_range or HUNT_RANGE
end

local function cost_of(view, t)
    for _, c in ipairs(view.costs) do
        if c.type == t then
            return c
        end
    end
    return nil
end

-- EECPNextCanAfford: the stock pays n, and the reserve stays (the epoch advance spends the reserve)
local function can_afford(view, vs, cost, n, epoch)
    for _, r in ipairs(RES) do
        local c = (cost[r] or 0) * n
        if c > 0 then
            local stock = (S.stock or view.stock)[r] or 0
            if stock < c or (not epoch and stock - c < (vs.reserve[r] or 0)) then
                return false
            end
        end
    end
    return true
end

-- what this step spent (the next affordability tests see it)
local function spend(view, cost, n)
    S.stock = S.stock or {food = view.stock.food, wood = view.stock.wood, stone = view.stock.stone,
                          gold = view.stock.gold, iron = view.stock.iron}
    for _, r in ipairs(RES) do
        S.stock[r] = S.stock[r] - (cost[r] or 0) * n
    end
end

local function event(vs, kind, t)
    for _, e in ipairs(vs.events) do
        if e.kind == kind and e.type == t then
            e.count = e.count + 1
            return
        end
    end
    vs.events[#vs.events + 1] = {kind = kind, type = t, count = 1}
end

local function result(verb, code, reason, count)
    return {verb = verb, code = code, reason = reason or "none", count = count or 0}
end

-- ---------------------------------------------------------------------------------------------------------------
-- the gatherer lists (AddGatherer / RemoveGatherer / AddIdleGatherer)

local function unlist(vs, id)
    for _, l in ipairs(LISTS) do
        remove_value(vs.lists[l], id)
    end
end

local function list(vs, id, key)
    unlist(vs, id)
    local l = vs.lists[key]
    l[#l + 1] = id
end

local function is_boat(u)
    return u ~= nil and u.category == "boat"
end

local function hunting(u)
    if u == nil or u.target == nil then
        return false
    end
    local t = U(u.target)
    return t ~= nil and t.kind == "food" and (cls(t.type) or {}).family == FAMILY_ANIMAL
end

-- the resource kind a citizen's gather goal works (its target's; a farm and game: food); nil when it gathers nothing
-- (record.gathers is what it carries, which lags behind a new target)
local function work_kind(u)
    if u == nil or u.target == nil or (u.gathers == nil and u.action ~= "gather") then
        return hunting(u) and "food" or nil
    end
    local t = U(u.target)
    if t == nil then
        return u.gathers
    end
    if t.kind ~= nil then
        return t.kind
    end
    if t.type == C.farm or t.type == C.robotic_farm then
        return "food"
    end
    return u.gathers
end

-- a citizen on its way to build (or building): its goal's target is a building other than a farm, or a foundation
-- not placed yet (the C++ runners left these alone: mCitizenBuildQueue)
local function building(u)
    if u == nil or u.target == nil then
        return false
    end
    if u.action == "build" or u.action == "repair" then
        return true
    end
    local t = U(u.target)
    return t ~= nil and t.category == "building" and t.type ~= C.farm and t.type ~= C.robotic_farm
end

local function is_idle(u)
    return u ~= nil and u.inside == nil and (u.action == "idle" or u.waiting == true)
end

-- the lists each step: the dead out, the new citizens and boats onto the idle list (and the seat's: ai.take); a
-- citizen the militia or a script took off its work goes to the idle list (the militia's AddIdleGatherer), one the
-- game moved on to another resource to that resource's list
local function refresh_lists(view, vs)
    local ids = RAW.units{category = "citizen"}
    local boats = RAW.units{category = "boat"}
    local alive = {}
    for _, id in ipairs(ids) do
        alive[id] = true
    end
    for _, id in ipairs(boats) do
        alive[id] = true
    end
    for _, k in ipairs(ai.keys(vs.lib)) do
        if not alive[k] then
            vs.lib[k] = nil
            vs.sh[k] = nil
        end
    end
    local known = {}
    for _, l in ipairs(LISTS) do
        local list_ = vs.lists[l]
        for k = #list_, 1, -1 do
            if not alive[list_[k]] or vs.sh[list_[k]] then
                table.remove(list_, k)
            else
                known[list_[k]] = l
            end
        end
    end
    local take = {}
    for _, group in ipairs({ids, boats}) do
        for _, id in ipairs(group) do
            if not vs.lib[id] then
                -- (a citizen the script held before this library took the seat stays the script's)
                local u = RAW.unit(id)
                if u ~= nil and u.held == "lua" then
                    vs.sh[id] = true
                else
                    take[#take + 1] = id
                end
                vs.lib[id] = true
            end
            if known[id] == nil and not vs.sh[id] then
                list(vs, id, "idle")
            end
        end
    end
    if #take > 0 then
        RAW.take(take)
    end
    vs.builder = vs.builder or {}
    for _, b in ipairs(ai.keys(vs.builder)) do
        local u = RAW.unit(b)
        local f = U(vs.builder[b])
        if u == nil or f == nil or not f.pending or u.target ~= vs.builder[b] then
            if M.TRACE then
                print(string.format("verbs.builder %d f=%d alive=%s fnd=%s pending=%s target=%s action=%s", b, vs.builder[b],
                    tostring(u ~= nil), tostring(f ~= nil), tostring(f and f.pending), tostring(u and u.target),
                    tostring(u and u.action)))
            end
            vs.builder[b] = nil
        end
    end
    S.citizens, S.boats = {}, {}
    for _, id in ipairs(ids) do
        if not vs.sh[id] then
            S.citizens[#S.citizens + 1] = id
        end
    end
    for _, id in ipairs(boats) do
        if not vs.sh[id] then
            S.boats[#S.boats + 1] = id
        end
    end
end

local function count_land(vs, key)
    local n = 0
    for _, id in ipairs(vs.lists[key]) do
        local u = U(id)
        if u ~= nil and not is_boat(u) then
            n = n + 1
        end
    end
    return n
end

-- SendToGather: onto the resource's list
local function send(vs, id, res)
    local code
    if res.type == C.farm or res.type == C.robotic_farm then
        code = ai.farm({id}, res.id)
    else
        code = ai.gather({id}, res.id)
    end
    touch(id)
    if code ~= "ok" then
        return false
    end
    add_assigned(res.id)
    list(vs, id, res.kind or "food")
    return true
end

local function stop(vs, id)
    list(vs, id, "idle")
    ai.stop({id})
    touch(id)
end

-- ---------------------------------------------------------------------------------------------------------------
-- who holds a citizen. The library holds the seat's citizens and boats in the engine (ai.take), standing where the
-- C++ runners stood: the script's own ai.take / ai.release / held filters see them as the engine's, and a citizen the
-- script takes leaves the library's lists until the script releases it (EECPUnitsTake / EECPUnitsRelease's rules)


local function lib_owned(id)
    return cur ~= nil and cur.lib[id] == true and not cur.sh[id]
end

local function vheld(id, raw)
    if cur == nil then
        return raw
    end
    if cur.sh[id] then
        return "lua"
    end
    if cur.builder ~= nil and cur.builder[id] then
        -- (a builder walking to a foundation this library placed: the foundation is not on the map until it arrives,
        -- and a script's order would drop it; the script sees it held)
        return "lua"
    end
    if cur.lib[id] then
        return "engine"
    end
    return raw
end

ai.take = function(ids, ...)
    if cur ~= nil and type(ids) == "table" then
        for _, id in ipairs(ids) do
            if cur.lib[id] then
                cur.sh[id] = true
                unlist(cur, id)
            end
        end
    end
    return RAW.take(ids, ...)
end

ai.release = function(ids, opts, ...)
    if cur == nil or type(ids) ~= "table" then
        return RAW.release(ids, opts, ...)
    end
    local others = {}
    for _, id in ipairs(ids) do
        if cur.lib[id] then
            if cur.sh[id] then
                cur.sh[id] = nil
                local u = RAW.unit(id)
                touch(id)
                if u ~= nil then
                    if u.category == "citizen" and work_kind(u) ~= nil then
                        list(cur, id, work_kind(u))
                    elseif u.category == "citizen" and hunting(u) then
                        list(cur, id, "food")
                    else
                        list(cur, id, "idle")
                    end
                end
            end
        else
            others[#others + 1] = id
        end
    end
    if #others > 0 then
        return RAW.release(others, opts, ...)
    end
    return "ok", "none", #ids
end

ai.units = function(f, ...)
    if cur == nil or type(f) ~= "table" or f.held == nil then
        return RAW.units(f, ...)
    end
    local g = {}
    for _, k in ipairs(ai.keys(f)) do
        if k ~= "held" and k ~= "limit" then
            g[k] = f[k]
        end
    end
    local all = RAW.units(g)
    g.held = f.held
    local raw = {}
    for _, id in ipairs(RAW.units(g)) do
        raw[id] = true
    end
    local out = {}
    for _, id in ipairs(all) do
        if f.limit ~= nil and #out >= f.limit then
            break
        end
        local h = vheld(id, raw[id] and f.held or (f.held == "lua" and "engine" or "lua"))
        if h == f.held then
            out[#out + 1] = id
        end
    end
    return out
end

ai.unit = function(id, ...)
    local u = RAW.unit(id, ...)
    if u ~= nil and u.held ~= nil and cur ~= nil then
        u.held = vheld(id, u.held)
    end
    return u
end

ai.get = function(ids, fields, ...)
    local cols = RAW.get(ids, fields, ...)
    if cur ~= nil and cols ~= nil and cols.held ~= nil and type(ids) == "table" then
        for k, id in ipairs(ids) do
            if cols.held[k] ~= nil then
                cols.held[k] = vheld(id, cols.held[k])
            end
        end
    end
    return cols
end

-- ---------------------------------------------------------------------------------------------------------------
-- the finders (FindMines, FindFarm, FindResourceTarget, the wood target, FindFish)

-- the bush's patch: the site holding it (sites by id, rebuilt each step)
local function patch_of(view, id)
    local map = memo("patch_of", function()
        local m = {}
        for _, s in ipairs(view.sites) do
            for _, u in ipairs(s.ids or {}) do
                m[u] = s
            end
        end
        return m
    end)
    return map[id]
end

-- EECPNextHasRoom: fewer sent than the mining limit, and a patch holds kNextPatchPlaces in all
local function has_room(view, vs, u)
    local limit = tuning(vs).mining_limit
    if assigned(u.id) >= limit then
        return false
    end
    if u.type ~= C.forage_bush then
        return true
    end
    local s = patch_of(view, u.id)
    if s == nil then
        return true
    end
    local n = 0
    for _, id in ipairs(s.ids) do
        local b = U(id)
        if b ~= nil and b.type == C.forage_bush then
            n = n + assigned(id)
        end
    end
    return n < PATCH_PLACES
end

local function site_safe(u)
    local cache = memo("site_safe", function() return {} end)
    local ok = cache[u.id]
    if ok == nil then
        local why = ai.site_blocked(u.id)
        ok = why == nil or why == "unexplored"
        cache[u.id] = ok
    end
    return ok
end

local function reaches(a, b)
    return a == nil or b == nil or ai.reach(a.x, a.y, b.x, b.y)
end

local function claimed(view, vs, s)
    local c = vs.claims[s.id]
    if c == nil then
        return s.cpp_claimed == true
    end
    return c
end

-- FindMines(n): the game within the hunt reach of the town on its land, no enemy near, fewer than 3 hunters an animal
local function find_mines(view, vs, n)
    local out = {}
    local tx, ty = town(view)
    if tx == nil or n <= 0 then
        return out
    end
    local reach = hunt_reach(vs)
    for _, id in ipairs(ai.resources{kind = "food", near = {tx, ty, reach}}) do
        if #out >= n then
            break
        end
        local u = U(id)
        local c = u ~= nil and cls(u.type) or nil
        if c ~= nil and c.family == FAMILY_ANIMAL and (c.attack or 0) == 0 and (u.amount or 0) > 0
                and ai.reach(tx, ty, u.x, u.y) and not ai.enemy_near(u.x, u.y)
                and assigned(id) < HUNTERS_PER_ANIMAL then
            out[#out + 1] = u
        end
    end
    return out
end

-- FindFarm: the first patch (the sites' order) whose bush with room has a finished drop site at hand, on the unit's
-- land, safe, within the claim range with that rule
local function find_farm(view, vs, unit)
    if town(view) == nil then
        return nil
    end
    local tx, ty = town(view)
    for _, s in ipairs(view.sites) do
        local first
        for _, id in ipairs(s.ids or {}) do
            local u = U(id)
            if u ~= nil and u.kind == "food" and (u.amount or 0) > 0 then
                first = u
                break
            end
        end
        if first ~= nil and first.type == C.forage_bush then
            local bush
            for _, id in ipairs(s.ids) do
                local u = U(id)
                if u ~= nil and u.kind == "food" and (u.amount or 0) > 0 and has_room(view, vs, u) and unit_drop_ready(u) then
                    bush = u
                    break
                end
            end
            if bush ~= nil and (unit ~= nil and reaches(unit, bush) or unit == nil and ai.reach(tx, ty, bush.x, bush.y))
                    and site_safe(bush) and (not tuning(vs).claim_rule or in_claim(view, vs, bush.x, bush.y)) then
                return bush
            end
        end
    end
    return nil
end

-- FindResourceTarget(r, unit, backwards): the claimed sites from the last, a unit of r with room (else the first),
-- explored or by a drop, safe, by a finished drop, the claim range first with that rule, on the unit's land
local function find_target(view, vs, r, unit, claim_pass)
    if not claim_pass and tuning(vs).claim_rule then
        local near = find_target(view, vs, r, unit, true)
        if near ~= nil then
            return near
        end
    end
    for k = #view.sites, 1, -1 do
        local s = view.sites[k]
        if claimed(view, vs, s) and (s.holds[r] or 0) > 0 then
            local first, room
            for _, id in ipairs(s.ids or {}) do
                local u = U(id)
                if u ~= nil and u.kind == r and (u.amount or 0) > 0 then
                    first = first or u
                    if has_room(view, vs, u) and unit_drop_ready(u) then
                        room = u
                        break
                    end
                end
            end
            local u = room or first
            if u ~= nil and has_room(view, vs, u) and site_safe(u) and unit_drop_ready(u)
                    and (not claim_pass or in_claim(view, vs, u.x, u.y)) and reaches(unit, u) then
                return u
            end
        end
    end
    return nil
end

-- the wood target (EECPRMNextWoodTarget): the tree nearest the town when a finished drop site is at hand, else the
-- tree nearest each finished drop site (the drop nearest the town first) within its reach; nil: none
local function wood_target(view, unit)
    local w = memo("wood_target", function()
        local tx, ty = town(view)
        if tx == nil then
            return false
        end
        local t = ai.resources{kind = "wood", near = {tx, ty, 60}, limit = 1}
        local tree = t[1] ~= nil and U(t[1]) or nil
        if tree ~= nil and drop_ready(tree.x, tree.y, false, WOOD_SLACK) then
            return tree
        end
        local order = {}
        for _, d in ipairs(drops(false)) do
            order[#order + 1] = d
        end
        table.sort(order, function(a, b)
            local da, db = d2(a.x, a.y, tx, ty), d2(b.x, b.y, tx, ty)
            if da ~= db then
                return da < db
            end
            return a.id < b.id
        end)
        for _, d in ipairs(order) do
            local reach = DROP_REACH + WOOD_SLACK + d.half
            local near = ai.resources{kind = "wood", near = {d.x, d.y, reach}, limit = 1}
            if near[1] ~= nil then
                local u = U(near[1])
                if u ~= nil then
                    return u
                end
            end
        end
        return false
    end)
    if not w then
        return nil
    end
    if unit ~= nil and not reaches(unit, w) then
        return nil
    end
    return w
end

local function own_docks()
    return memo("docks", function()
        local out = {}
        for _, id in ipairs(ai.units{type = C.dock, built = true}) do
            local u = U(id)
            if u ~= nil then
                out[#out + 1] = u
            end
        end
        return out
    end)
end

local function warships()
    return memo("warships", function()
        local out = {}
        for _, id in ipairs(ai.seen{enemy = true, armed = true, category = "ship"}) do
            local u = U(id)
            if u ~= nil then
                out[#out + 1] = u
            end
        end
        return out
    end)
end

-- FindFish(dock): the fish nearest the dock with room, none of the warships seen within 12 tiles, away from the own
-- docks (8 tiles) first, else any with room
local function find_fish(view, vs, dock)
    local list_ = {}
    for _, id in ipairs(ai.resources{kind = "food", near = {dock.x, dock.y, 40}}) do
        local u = U(id)
        if u ~= nil and (cls(u.type) or {}).family == FAMILY_FISH and (u.amount or 0) > 0
                and assigned(id) < tuning(vs).mining_limit then
            local near = false
            for _, w in ipairs(warships()) do
                if d2(w.x, w.y, u.x, u.y) <= 144 then
                    near = true
                end
            end
            if not near then
                list_[#list_ + 1] = u
            end
        end
    end
    for _, u in ipairs(list_) do
        local free = true
        for _, d in ipairs(own_docks()) do
            if d2(d.x, d.y, u.x, u.y) <= 64 then
                free = false
            end
        end
        if free then
            return u
        end
    end
    return list_[1]
end

-- ---------------------------------------------------------------------------------------------------------------
-- AssignGatherers (every 10 s): Rebalance, the crowds shed, the idle citizens to the short resources, then to the
-- drop sites going up, then by the most under-served; the idle boats to fish

local function rebalance(view, vs)
    local short = false
    for _, r in ipairs(RES) do
        if #vs.lists[r] < vs.wanted[r] then
            short = true
        end
    end
    if not short then
        return
    end
    local moves, max = 0, tuning(vs).moves
    local slack = vs.exact ~= 0 and 0 or 1
    for _, r in ipairs(RES) do
        local l = vs.lists[r]
        while moves < max and #l > vs.wanted[r] + slack do
            local id = l[#l]
            local u = U(id)
            if u == nil or is_boat(u) or building(u) then
                break
            end
            stop(vs, id)
            moves = moves + 1
        end
    end
end

local function shed(view, vs)
    local limit = tuning(vs).mining_limit
    local on_unit, on_patch = {}, {}
    local out = {}
    local wood_ready
    for k = 2, #LISTS do
        for _, id in ipairs(vs.lists[LISTS[k]]) do
            local u = U(id)
            if u ~= nil and not is_boat(u) and u.target ~= nil and work_kind(u) ~= nil and not building(u) then
                local t = U(u.target)
                if t ~= nil then
                    if t.kind == "wood" then
                        if not drop_ready(t.x, t.y, false, WOOD_SLACK) then
                            if wood_ready == nil then
                                wood_ready = wood_target(view, nil) ~= nil
                            end
                            if wood_ready then
                                out[#out + 1] = id
                            end
                        end
                    else
                        local fam = (cls(t.type) or {}).family
                        if fam ~= FAMILY_ANIMAL and fam ~= FAMILY_FISH and t.type ~= C.farm and t.type ~= C.robotic_farm then
                            on_unit[t.id] = (on_unit[t.id] or 0) + 1
                            local over = on_unit[t.id] > limit
                            if t.type == C.forage_bush then
                                local s = patch_of(view, t.id)
                                if s ~= nil then
                                    on_patch[s.id] = (on_patch[s.id] or 0) + 1
                                    over = over or on_patch[s.id] > PATCH_PLACES
                                end
                            end
                            if over then
                                out[#out + 1] = id
                            end
                        end
                    end
                end
            end
        end
    end
    for _, id in ipairs(out) do
        stop(vs, id)
    end
end

-- whether a unit is walking to see a site (§8.36: kept where it looks)
local function exploring(view, vs, id)
    for _, e in ipairs(vs.explore) do
        if e.unit == id and view.time - e.t <= EXPLORE_MS then
            return true
        end
    end
    return false
end

local function pop_front(l)
    return table.remove(l, 1)
end

-- GetIdleFarms / GetUnfinishedFarms
local function farms(view, built, max)
    local out = {}
    for _, t in ipairs({C.farm}) do
        for _, id in ipairs(ai.units{type = t, built = built}) do
            if #out >= max then
                break
            end
            local u = U(id)
            if u ~= nil and (not built or assigned(id) == 0) then
                out[#out + 1] = u
            end
        end
    end
    return out
end

-- EECPRMBuildsFields: the idle food gatherers build the granaries' fields unless gather(granary_farms = n) said
-- otherwise (0 none, n at most n finished farms a finished granary)
local function builds_fields(vs)
    local per = vs.granary_farms
    if per < 0 then
        return true
    end
    if per == 0 then
        return false
    end
    local granaries = #ai.units{type = C.granary, built = true}
    local farms_ = #ai.units{type = C.farm, built = true}
    return farms_ < per * granaries
end

local function fill_food(view, vs, idle, boats, want)
    local function fill(finder)
        local target = finder(idle[1])
        if target == nil then
            return
        end
        local n = math.min(want, #idle)
        while n > 0 and #idle > 0 do
            if has_room(view, vs, target) then
                if send(vs, idle[1], target) then
                    want = want - 1
                end
                pop_front(idle)
                n = n - 1
            else
                target = finder(idle[1] and U(idle[1]) or nil)
                if target == nil then
                    break
                end
            end
        end
    end
    -- hunt first: gather(food, n, prefer = "hunt")
    if vs.prefer == "hunt" then
        local hunters = 0
        for _, id in ipairs(vs.lists.food) do
            local u = U(id)
            if u ~= nil and not is_boat(u) and u.target ~= nil then
                local t = U(u.target)
                if t ~= nil and (cls(t.type) or {}).family == FAMILY_ANIMAL then
                    hunters = hunters + 1
                end
            end
        end
        if hunters < vs.wanted.food then
            local more = math.min(vs.wanted.food - hunters, want, #idle)
            for _, m in ipairs(find_mines(view, vs, more)) do
                if #idle == 0 or want <= 0 then
                    break
                end
                if send(vs, idle[1], m) then
                    want = want - 1
                end
                pop_front(idle)
            end
        end
    end
    local function farm_finder(id)
        return find_farm(view, vs, id ~= nil and U(id) or nil)
    end
    fill(function(u)
        return find_farm(view, vs, type(u) == "number" and U(u) or u)
    end)
    if want <= 0 or #idle == 0 then
        return
    end
    for _, f in ipairs(farms(view, true, math.min(want, #idle))) do
        if #idle == 0 then
            break
        end
        if send(vs, idle[1], f) then
            want = want - 1
        end
        pop_front(idle)
    end
    if want <= 0 or #idle == 0 then
        return
    end
    local function mines(n)
        for _, m in ipairs(find_mines(view, vs, n)) do
            if #idle == 0 or want <= 0 then
                break
            end
            local u = U(idle[1])
            if u ~= nil and ai.reach(u.x, u.y, m.x, m.y) then
                if send(vs, idle[1], m) then
                    want = want - 1
                end
                pop_front(idle)
            end
        end
    end
    mines(math.min(want, #idle))
    if want <= 0 or #idle == 0 then
        return
    end
    fill(function(u)
        return find_farm(view, vs, type(u) == "number" and U(u) or u)
    end)
    if want <= 0 or #idle == 0 then
        return
    end
    mines(math.min(want, #idle))
    if want <= 0 or #idle == 0 then
        return
    end
    if cost_of(view, C.farm) ~= nil and builds_fields(vs) then
        for _, f in ipairs(farms(view, false, math.min(want, #idle))) do
            if #idle == 0 then
                break
            end
            local id = pop_front(idle)
            ai.repair({id}, f.id)
            touch(id)
            want = want - 1
        end
    end
    -- (the boats: AssignIdleBoats below)
    local _ = farm_finder
end

-- EECPRMHelpBuildDrops: up to 4 builders to each settlement and granary going up (safe), the nearest first
local function help_build_drops(view, vs, idle)
    local sites = {}
    for _, t in ipairs({C.settlement, C.granary}) do
        for _, id in ipairs(ai.units{type = t, built = false}) do
            local u = U(id)
            if u ~= nil and u.x ~= nil and u.x >= 0 and ai.build_unsafe(u.x, u.y) == nil then
                sites[#sites + 1] = u
            end
        end
    end
    if #sites == 0 then
        return
    end
    for _, b in ipairs(sites) do
        if #idle == 0 then
            break
        end
        local builders = 0
        for _, id in ipairs(S.citizens or {}) do
            local c = U(id)
            if c ~= nil and c.target == b.id and (c.action == "build" or c.action == "repair") then
                builders = builders + 1
            end
        end
        while builders < DROP_BUILDERS and #idle > 0 do
            local best, bk, bd
            for k, id in ipairs(idle) do
                local c = U(id)
                if c ~= nil then
                    local dd = d2(c.x, c.y, b.x, b.y)
                    if bd == nil or dd < bd then
                        best, bk, bd = id, k, dd
                    end
                end
            end
            if best == nil then
                break
            end
            table.remove(idle, bk)
            ai.repair({best}, b.id)
            touch(best)
            builders = builders + 1
        end
    end
end

-- AssignIdleGatherers: each idle one to the most under-served of gold, iron, stone, wood (exact = 2: none past its
-- count)
local function assign_idle(view, vs, idle)
    for _, id in ipairs(idle) do
        local order = {"gold", "iron", "stone", "wood"}
        for i = 2, 4 do
            for k = i, 2, -1 do
                local a, b = order[k - 1], order[k]
                if vs.wanted[b] - #vs.lists[b] > vs.wanted[a] - #vs.lists[a] then
                    order[k - 1], order[k] = b, a
                end
            end
        end
        local u = U(id)
        local target
        for _, r in ipairs(order) do
            if target ~= nil then
                break
            end
            if vs.exact < 2 or #vs.lists[r] < vs.wanted[r] then
                if r == "wood" then
                    target = wood_target(view, u)
                else
                    target = find_target(view, vs, r, u)
                end
            end
        end
        if target ~= nil then
            send(vs, id, target)
        end
    end
end

local function assign_boats(view, vs, boats)
    for _, id in ipairs(boats) do
        local u = U(id)
        if u ~= nil then
            local best, bd
            for _, d in ipairs(own_docks()) do
                local dd = d2(d.x, d.y, u.x, u.y)
                if bd == nil or dd < bd then
                    best, bd = d, dd
                end
            end
            if best ~= nil then
                local fish = find_fish(view, vs, best)
                if fish ~= nil then
                    send(vs, id, fish)
                end
            end
        end
    end
end

local function assign_gatherers(view, vs)
    rebalance(view, vs)
    shed(view, vs)
    -- the idle-listed that gather (or hunt) go onto their resource's list
    local moved = {}
    for _, id in ipairs(vs.lists.idle) do
        local u = U(id)
        if u ~= nil and not is_boat(u) and work_kind(u) ~= nil then
            moved[#moved + 1] = {id, work_kind(u)}
        end
    end
    for _, m in ipairs(moved) do
        list(vs, m[1], m[2])
    end
    local idle, boats = {}, {}
    for _, l in ipairs(LISTS) do
        for _, id in ipairs(vs.lists[l]) do
            local u = U(id)
            if u ~= nil then
                if is_boat(u) then
                    if l == "idle" and is_idle(u) then
                        boats[#boats + 1] = id
                    end
                elseif is_idle(u) and not exploring(view, vs, id) then
                    idle[#idle + 1] = id
                end
            end
        end
    end
    if #idle > 0 then
        for _, r in ipairs(RES) do
            if #idle == 0 then
                break
            end
            local listed = #vs.lists[r]
            if vs.wanted[r] > listed then
                local want = math.min(vs.wanted[r] - listed, #idle + #boats)
                if want > 0 then
                    if r == "food" then
                        fill_food(view, vs, idle, boats, want)
                    elseif r == "wood" then
                        local target = wood_target(view, U(idle[1]))
                        if target ~= nil then
                            local n = math.min(want, #idle)
                            while n > 0 and #idle > 0 do
                                send(vs, pop_front(idle), target)
                                n = n - 1
                            end
                        end
                    else
                        local target = find_target(view, vs, r, U(idle[1]))
                        local n = math.min(want, #idle)
                        while target ~= nil and n > 0 and #idle > 0 do
                            if has_room(view, vs, target) then
                                send(vs, pop_front(idle), target)
                                n = n - 1
                            else
                                target = find_target(view, vs, r, U(idle[1]))
                            end
                        end
                    end
                end
            end
        end
        if #idle > 0 then
            help_build_drops(view, vs, idle)
        end
        if #idle > 0 then
            assign_idle(view, vs, idle)
        end
    end
    if #boats > 0 then
        assign_boats(view, vs, boats)
    end
end

-- every 15 s: the idle citizens back on the idle list (RequeueIdleGatherers)
local function requeue_idle(view, vs)
    for _, id in ipairs(S.citizens or {}) do
        local u = U(id)
        if u ~= nil and u.action == "idle" and u.inside == nil then
            list(vs, id, "idle")
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the builders (FindBuilder, the build queue, AssignBuilders)

-- IsCitizenAvailable: alive, on the map, idle or gathering
local function available(view, vs)
    return memo("available", function()
        local out = {}
        for _, id in ipairs(S.citizens or {}) do
            local u = U(id)
            if u ~= nil and u.inside == nil and u.x ~= nil and u.x >= 0 and (u.action == "idle" or u.action == "gather")
                    and not exploring(view, vs, id) then
                out[#out + 1] = u
            end
        end
        return out
    end)
end

local function used(id)
    local l = S.memo.available
    if l ~= nil then
        for k = #l, 1, -1 do
            if l[k].id == id then
                table.remove(l, k)
            end
        end
    end
    touch(id)
end

-- FindBuilder: the nearest available citizen on the site's land (a dock or tower: one that reaches its shore); a
-- farm's builder is no farmer
local function find_builder(view, vs, x, y, t)
    -- (a dock's or tower's site: the land tiles by its footprint, one a land mass; a citizen reaching one of them)
    local lands
    if t == C.dock or t == C.navy_yard or is_tower(t) then
        lands = memo("lands_" .. t .. "_" .. x .. "_" .. y, function()
            local size = math.max(1, (cls(t) or {}).size or 1)
            local out, seen = {}, {}
            for ty = y - 1, y + size do
                for tx = x - 1, x + size do
                    local lx, ly, _, mass = ai.shore(tx, ty, {max = 0})
                    if lx ~= nil and mass ~= nil and not seen[mass] then
                        seen[mass] = true
                        out[#out + 1] = {lx, ly}
                    end
                end
            end
            return out
        end)
    end
    local best, bd
    for _, u in ipairs(available(view, vs)) do
        local dd = d2(u.x, u.y, x, y)
        if bd == nil or dd < bd or (dd == bd and u.id < best.id) then
            local ok = ai.reach(u.x, u.y, x, y)
            for _, l in ipairs(not ok and lands or {}) do
                ok = ok or ai.reach(u.x, u.y, l[1], l[2])
            end
            if ok and t == C.farm and u.action == "gather" and u.target ~= nil then
                local g = U(u.target)
                ok = g == nil or g.type ~= C.farm
            end
            if ok then
                best, bd = u, dd
            end
        end
    end
    return best
end

local function epoch_of(t, view)
    local c = cls(t)
    return c ~= nil and c.epoch or view.epoch
end

local function queue_build(view, vs, t, x, y)
    local e = epoch_of(t, view)
    insert_by_epoch(vs.builds, {type = t, x = x, y = y, e = e})
end

-- the foundations this library placed that the builders have not reached yet (not on the map: ai.units leaves them
-- out, and so did the C++ view)
local function pending_builds(vs, t)
    local n = 0
    for _, id in ipairs(vs.placed) do
        local u = U(id)
        if u ~= nil and u.pending and (t == nil or u.type == t) then
            n = n + 1
        end
    end
    return n
end

local function queued_builds(vs, t)
    local n = 0
    for _, b in ipairs(vs.builds) do
        if b.type == t then
            n = n + 1
        end
    end
    return n
end

-- the classes that are done with no builder to be had (StartBuilding's list)
local SKIP_NO_BUILDER = {[C.temple] = true, [C.university] = true, [C.hospital] = true, [C.house] = true,
                         [C.farm] = true, [C.robotic_farm] = true, [C.granary] = true, [C.priest_tower] = true}

-- StartBuilding: -> done (started or dropped)
local function start_building(view, vs, req)
    if ai.build_unsafe(req.x, req.y) ~= nil then
        event(vs, "no_site", req.type)
        return true
    end
    if #available(view, vs) == 0 then
        return false
    end
    local w = find_builder(view, vs, req.x, req.y, req.type)
    if M.TRACE then
        print(string.format("verbs.start %s @%d,%d builder=%s avail=%d", req.type, req.x, req.y, tostring(w and w.id),
            #available(view, vs)))
    end
    if w == nil then
        return SKIP_NO_BUILDER[req.type] == true
    end
    if req.type == C.farm or req.type == C.robotic_farm then
        for _, id in ipairs(ai.units{type = req.type, built = false}) do
            local f = U(id)
            if f ~= nil and f.x == req.x and f.y == req.y then
                ai.repair({w.id}, id)
                used(w.id)
                return true
            end
        end
        return true
    end
    local code, _, _, id = ai.build({w.id}, req.type, req.x, req.y)
    if M.TRACE then
        print(string.format("verbs.placed %s @%d,%d -> %s %s", req.type, req.x, req.y, tostring(code), tostring(id)))
    end
    used(w.id)
    if code == "ok" and id ~= nil and id ~= 0 then
        vs.placed[#vs.placed + 1] = id
        vs.builder[w.id] = id
        S.placed_now = S.placed_now or {}
        S.placed_now[id] = true   -- (placed after the view was made: the decoration counts it)
        local c = cost_of(view, req.type)
        if c ~= nil then
            spend(view, c.cost, 1)
        end
    end
    return true
end

local function build_update(view, vs)
    local attempts, started = 0, false
    local keep = {}
    local skip_epoch = {}
    local left = vs.builds
    vs.builds = {}
    for _, req in ipairs(left) do
        local done = false
        if started or attempts >= 4 or skip_epoch[req.e] or req.e > view.epoch then
            keep[#keep + 1] = req
        else
            local c = cost_of(view, req.type)
            if c == nil then
                if req.e > view.epoch then
                    keep[#keep + 1] = req
                end   -- (else obsolete: dropped)
            elseif not can_afford(view, vs, c.cost, 1, false) then
                keep[#keep + 1] = req
                skip_epoch[req.e] = true   -- (as the original: the next epoch's queue)
            else
                attempts = attempts + 1
                done = start_building(view, vs, req)
                if done then
                    started = true
                else
                    keep[#keep + 1] = req
                end
            end
        end
    end
    -- (the requests ordered while this ran stay after)
    for _, req in ipairs(vs.builds) do
        keep[#keep + 1] = req
    end
    local sorted = {}
    for _, req in ipairs(keep) do
        insert_by_epoch(sorted, req)
    end
    vs.builds = sorted
end

-- the foundations placed: a builder for each that lost its own (BuilderLost); the stalled ones cancelled
local function tend_foundations(view, vs, full)
    local keep = {}
    local building = {}
    for _, id in ipairs(S.citizens or {}) do
        local u = U(id)
        if u ~= nil and u.target ~= nil and (u.action == "build" or u.action == "repair" or u.goal ~= nil) then
            building[u.target] = (building[u.target] or 0) + 1
        end
        if u ~= nil and u.stalled and u.target ~= nil then
            if M.TRACE then
                print(string.format("verbs.stalled %d f=%d", id, u.target))
            end
            ai.delete({u.target})
            local tx, ty = town(view)
            if tx ~= nil then
                ai.move({id}, tx, ty)
            end
            touch(id)
        end
    end
    local list_ = vs.placed
    if full then
        -- (AssignBuilders: the foundations this library placed and the engine's own, e.g. a granary's fields; not
        -- the script's)
        list_ = {}
        local seen = {}
        for _, id in ipairs(vs.placed) do
            list_[#list_ + 1] = id
            seen[id] = true
        end
        for _, id in ipairs(RAW.units{category = "building", built = false, held = "engine"}) do
            if not seen[id] then
                list_[#list_ + 1] = id
            end
        end
    end
    for _, id in ipairs(list_) do
        local b = U(id)
        if b ~= nil and not b.built then
            if not full then
                keep[#keep + 1] = id
            end
            local why = b.x ~= nil and ai.build_unsafe(b.x, b.y) or nil
            if why ~= nil then
                for _, cid in ipairs(S.citizens or {}) do
                    local c = U(cid)
                    if c ~= nil and c.target == id then
                        ai.stop({cid})
                        touch(cid)
                    end
                end
                if why == "tower" and (b.hp or 0) <= 1 then
                    if M.TRACE then
                        print(string.format("verbs.unsafe_cancel %s %d", b.type, id))
                    end
                    ai.delete({id})
                    event(vs, "no_site", b.type)
                end
            elseif (building[id] or 0) == 0 and b.x ~= nil and b.x >= 0 then
                local w = find_builder(view, vs, b.x, b.y, b.type)
                if w ~= nil then
                    ai.repair({w.id}, id)
                    used(w.id)
                    building[id] = 1
                end
            end
        end
    end
    if full then
        local still = {}
        for _, id in ipairs(vs.placed) do
            local b = U(id)
            if b ~= nil and not b.built then
                still[#still + 1] = id
            end
        end
        vs.placed = still
    else
        vs.placed = keep
    end
end

-- RepairBuildings (every 31 s): the nearest available citizen on the same land to each land building under 80%
local function repair(view, vs)
    local damaged = {}
    for _, id in ipairs(ai.units{category = "building"}) do
        local b = U(id)
        if b ~= nil and b.hp ~= nil and b.hp_max ~= nil and b.hp_max > 0 and b.hp * 1000 < REPAIR_RATIO_K * b.hp_max
                and b.built and b.type ~= C.farm and b.type ~= C.robotic_farm and b.x ~= nil and b.x >= 0
                and not (is_wall(b.type) and b.hp <= 1) then
            damaged[#damaged + 1] = b
        end
    end
    for _, b in ipairs(damaged) do
        local list_ = available(view, vs)
        if #list_ == 0 then
            return
        end
        local best, bd
        for _, u in ipairs(list_) do
            local dd = d2(u.x, u.y, b.x, b.y)
            if (bd == nil or dd < bd) and (b.type == C.dock or b.type == C.navy_yard or ai.reach(u.x, u.y, b.x, b.y)) then
                best, bd = u, dd
            end
        end
        if best ~= nil then
            ai.repair({best.id}, b.id)
            used(best.id)
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- production (UnitManager: the request FIFOs by epoch) and research (ResearchManager's queues)

-- the classes that make a class (its tech node's producers; static, cached for the game)
local sProducers = {}
local function producers_of(t)
    local p = sProducers[t]
    if p == nil then
        local c = cls(t)
        local n = c ~= nil and c.tech ~= nil and ai.techs{id = c.tech}[1] or nil
        p = n ~= nil and n.producers or {}
        sProducers[t] = p
    end
    return p
end

-- the own units of the classes that make a class (built or not)
local function has_producer(t)
    local p = producers_of(t)
    if #p == 0 then
        return false
    end
    return #ai.units{type = p, limit = 1} > 0
end

local function request(view, vs, t, n)
    vs.waiting[t] = (vs.waiting[t] or 0) + n
    for _, r in ipairs(vs.requests) do
        if r.type == t then
            r.n = r.n + n
            return
        end
    end
    insert_by_epoch(vs.requests, {type = t, n = n, e = epoch_of(t, view)})
end

-- StartProduction: the built producers of the class, the least busy first, n in turn -> made
local function start_production(view, vs, r, n)
    local b = {}
    local p = producers_of(r.type)
    for _, id in ipairs(#p > 0 and ai.units{type = p, built = true} or {}) do
        local u = U(id)
        if u ~= nil and (cls(u.type) or {}).building then
            b[#b + 1] = u
        end
    end
    if #b == 0 then
        return 0
    end
    local made = 0
    for _ = 1, n do
        table.sort(b, function(x, y)
            local qx, qy = x.queue or 0, y.queue or 0
            if qx ~= qy then
                return qx < qy
            end
            return x.id < y.id
        end)
        local code, _, k = ai.train({b[1].id}, r.type, 1)
        if code ~= "ok" or (k or 0) < 1 then
            break
        end
        b[1].queue = (b[1].queue or 0) + 1
        touch(b[1].id)
        made = made + 1
    end
    if made > 0 then
        vs.waiting[r.type] = math.max(0, (vs.waiting[r.type] or 0) - made)
        S.in_production = (S.in_production or 0) + made
        local c = cost_of(view, r.type)
        if c ~= nil then
            spend(view, c.cost, made)
        end
    end
    return made
end

local function in_production(view)
    local n = 0
    for _, c in ipairs(view.costs) do
        if not c.building then
            n = n + (c.in_production or 0)
        end
    end
    return n
end

local function production(view, vs)
    S.in_production = 0
    local room = view.population_cap - in_production(view) - view.population
    if room <= 0 then
        return
    end
    local citizens_asked = (vs.waiting.citizen or 0) > 0 or (vs.waiting.female_citizen or 0) > 0
    local keep = {}
    for _, r in ipairs(vs.requests) do
        local c = cost_of(view, r.type)
        if r.n <= 0 then
            vs.waiting[r.type] = 0   -- (done)
        elseif c == nil and r.e <= view.epoch then
            vs.waiting[r.type] = 0   -- (obsolete: dropped)
        elseif c == nil or r.e > view.epoch or room - S.in_production < 1 then
            keep[#keep + 1] = r
        else
            local size = math.max(1, (cls(r.type) or {}).pop or 1)
            local left = room - S.in_production
            local all = left >= r.n * size
            if not all then
                if size < left and can_afford(view, vs, c.cost, 1, false) then
                    r.n = r.n - start_production(view, vs, r, 1)
                end
            elseif not can_afford(view, vs, c.cost, r.n, false) then
                if can_afford(view, vs, c.cost, 1, false) then
                    local wait = false
                    if r.type ~= C.citizen and r.type ~= C.female_citizen and citizens_asked then
                        local workers = #(S.citizens or {}) + #(S.boats or {})
                        wait = workers < view.population - workers
                    end
                    if not wait then
                        r.n = r.n - start_production(view, vs, r, 1)
                    end
                end
            else
                r.n = r.n - start_production(view, vs, r, r.n)
            end
            if r.n > 0 then
                keep[#keep + 1] = r
            end
        end
    end
    vs.requests = keep
end

-- a tech's node (ai.techs by name or id, once a step)
local function tech(id)
    return memo("tech_" .. tostring(id), function()
        local list_ = type(id) == "number" and ai.techs{id = id} or ai.techs{name = id}
        return list_[1] or false
    end) or nil
end

local function is_epoch_tech(tn)
    return tn ~= nil and tn.kind == 0 and tn.name ~= nil and string.sub(tn.name, 1, 6) == "epoch_" or false
end

-- TryResearch: an idle built producer that can afford it (one producer: built); an epoch at the town centre
local function try_research(view, vs, entry)
    local tn = tech(entry.id)
    if tn == nil or not tn.available then
        return false
    end
    local units = {}
    for _, p in ipairs(tn.producers or {}) do
        if p ~= C.citizen and p ~= C.female_citizen then
            for _, id in ipairs(ai.units{type = p}) do
                units[#units + 1] = id
            end
        end
    end
    if #units == 0 then
        if entry.epoch then
            vs.advancing = false
        end
        return false
    end
    for _, id in ipairs(units) do
        local u = U(id)
        if u ~= nil and u.built and (#units == 1 or u.action == "idle" or (u.queue or 0) == 0) then
            if not can_afford(view, vs, tn.cost or {}, 1, entry.epoch) and not (entry.epoch and tuning(vs).cheat_epochs) then
                return false
            end
            local code = ai.research({id}, tn.id)
            touch(id)
            if code == "ok" then
                spend(view, tn.cost or {}, 1)
                return true
            end
            return false
        end
    end
    return false
end

local function research_update(view, vs)
    for k, entry in ipairs(vs.research) do
        if entry.e <= view.epoch and try_research(view, vs, entry) then
            table.remove(vs.research, k)
            return
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the verbs

local function v_gather(view, vs, o)
    local r, n = o.resource, o.n
    local prefer = o.prefer or "patch"
    if vs.wanted[r] == nil or type(n) ~= "number" or n < 0 or n > 1000 or (prefer ~= "patch" and prefer ~= "hunt") then
        return result("gather", "refused", "bad_argument")
    end
    if o.exact ~= nil and o.exact >= 0 then
        vs.exact = math.min(o.exact, 2)
    end
    if o.granary_farms ~= nil and o.granary_farms >= -1 and r == "food" then
        vs.granary_farms = o.granary_farms
    end
    local wanted = n
    if r == "food" then
        for _, l in ipairs(LISTS) do
            for _, id in ipairs(vs.lists[l]) do
                if is_boat(U(id)) then
                    wanted = wanted + 1
                end
            end
        end
        vs.food_land = n
        vs.prefer = prefer
    end
    vs.wanted[r] = wanted
    if n > 0 and not (r == "food" and prefer == "hunt") then
        local places = 0
        if r == "wood" then
            places = wood_target(view, nil) ~= nil and MAX_COUNT or 0
        else
            local limit = tuning(vs).mining_limit
            for _, s in ipairs(view.sites) do
                if claimed(view, vs, s) and s.ready[r] == 1 and (s.holds[r] or 0) > 0 then
                    places = places + (r == "food" and PATCH_PLACES or s.holds[r] * limit)
                end
            end
        end
        if places < n then
            return result("gather", "queued", "no_dropoff", n)
        end
    end
    return result("gather", "done", "none", n)
end

local function v_claim(view, vs, o, on)
    local verb = on and "claim" or "release"
    local s = type(o.site) == "number" and view.sites[o.site] or nil
    if s == nil then
        return result(verb, "refused", "bad_argument")
    end
    if claimed(view, vs, s) == on then
        return result(verb, "done", "none", 0)
    end
    vs.claims[s.id] = on
    s.claimed = on
    return result(verb, "done", "none", 1)
end

local function v_claim_range(view, vs, o)
    local r = o.range
    if type(r) ~= "number" or r < CLAIM_MIN or r > CLAIM_MAX then
        return result("claim_range", "refused", "bad_argument")
    end
    vs.claim_range = r
    view.claim_range = r
    return result("claim_range", "done", "none", r)
end

-- the dock by the fish near home (EECPNextDockByFish)
local function dock_by_fish(view, vs)
    local c = cost_of(view, C.dock)
    if c == nil then
        return result("build", "refused", "not_in_epoch")
    end
    if town(view) == nil then
        return result("build", "refused", "no_site")
    end
    if queued_builds(vs, C.dock) > 0 or pending_builds(vs, C.dock) > 0 then
        return result("build", "refused", "busy")
    end
    local x, y = ai.place(C.dock, "dock")
    if x == nil then
        return result("build", "refused", "no_site")
    end
    if ai.build_unsafe(x, y) ~= nil then
        return result("build", "refused", "unsafe")
    end
    queue_build(view, vs, C.dock, x, y)
    return result("build", "queued", "none", 1)
end

-- the explore walker of a site not seen yet (§8.36): one alive walker a site
local function walker(view, vs, x, y)
    local keep = {}
    local have = false
    for _, e in ipairs(vs.explore) do
        local u = U(e.unit)
        if view.time - e.t <= EXPLORE_MS and u ~= nil then
            keep[#keep + 1] = e
            if e.x == x and e.y == y then
                have = true
            end
        end
    end
    vs.explore = keep
    if have then
        return 1
    end
    local best, bd
    for _, id in ipairs(S.citizens or {}) do
        local u = U(id)
        if u ~= nil and u.inside == nil and u.x ~= nil and u.x >= 0 and u.action ~= "build" and u.action ~= "repair"
                and u.action ~= "move" then
            local dd = d2(u.x, u.y, x, y)
            if bd == nil or dd < bd then
                best, bd = id, dd
            end
        end
    end
    if best == nil then
        return 0
    end
    ai.move({best}, x, y)
    touch(best)
    vs.explore[#vs.explore + 1] = {x = x, y = y, t = view.time, unit = best}
    return 1
end

local function arrived(view, vs, x, y)
    for _, e in ipairs(vs.explore) do
        local u = U(e.unit)
        if e.x == x and e.y == y and u ~= nil and u.x ~= nil and d2(u.x, u.y, x, y) <= 9 then
            return true
        end
    end
    return false
end

local function v_build(view, vs, o)
    local t, near = o.type, o.near or "town"
    local c = type(t) == "string" and cost_of(view, t) or nil
    if c == nil then
        return result("build", "refused", cls(t or "") ~= nil and "not_in_epoch" or "unknown_class")
    end
    if not c.building and t ~= "b_fortress" or is_wall(t) then
        return result("build", "refused", "bad_argument")
    end
    if near ~= "town" and near ~= "site" and near ~= "tile" then
        return result("build", "refused", "bad_argument")
    end
    local tx, ty = town(view)
    if tx == nil then
        return result("build", "refused", "no_site")
    end
    if near == "town" and t == C.dock then
        return dock_by_fish(view, vs)
    end
    local x, y, gap
    local tower = is_tower(t)
    if near == "town" then
        x, y = ai.place(t, "classic")
    elseif near == "site" then
        local s = type(o.site) == "number" and view.sites[o.site] or nil
        if s == nil or s.x == nil or s.x < 0 or s.resource == nil then
            return result("build", "refused", "bad_argument")
        end
        local sx, sy = s.x, s.y
        if t == C.settlement or t == C.granary then
            local units, nodrop = {}, {}
            for _, id in ipairs(s.ids or {}) do
                local u = U(id)
                if u ~= nil and (u.amount or 0) > 0 then
                    units[#units + 1] = id
                    if not unit_drop_ready(u) then
                        nodrop[#nodrop + 1] = u
                    end
                end
            end
            if #nodrop > 0 and #nodrop < #units then
                units = {}
                for _, u in ipairs(nodrop) do
                    units[#units + 1] = u.id
                end
                sx, sy = nodrop[1].x, nodrop[1].y
            end
            x, y, gap = ai.place(t, "adjacent", {x = sx, y = sy, last = PATCH_DROP_RANGE + 4, units = units})
            if (x == nil or gap > 2) and not arrived(view, vs, sx, sy) then
                if ai.build_unsafe(sx, sy) ~= nil then
                    return result("build", "refused", "unsafe")
                end
                return result("build", "queued", "unexplored", walker(view, vs, sx, sy))
            end
        else
            x, y = ai.place(t, "ring", {x = sx, y = sy, first = 2, last = 7})
        end
        if x == nil and t == C.settlement then
            x, y = ai.place(t, "ring", {x = sx, y = sy, first = PATCH_DROP_RANGE + 3, last = PATCH_DROP_RANGE + 5})
            if x == nil then
                local cx, cy = ai.place(t, "centre", {site = o.site})
                if cx ~= nil and d2(cx, cy, sx, sy) <= (PATCH_DROP_RANGE + 6) * (PATCH_DROP_RANGE + 6) then
                    x, y = cx, cy
                end
            end
        end
    else
        if type(o.x) ~= "number" or type(o.y) ~= "number" or o.x < 0 or o.y < 0 then
            return result("build", "refused", "bad_argument")
        end
        if t == C.settlement then
            x, y, gap = ai.place(t, "adjacent", {x = o.x, y = o.y, last = 7, units = {}})
        else
            x, y = ai.place(t, "ring", {x = o.x, y = o.y, first = 0, last = 6})
        end
    end
    if x == nil and tower and near ~= "tile" then
        if near == "site" then
            x, y = ai.place(t, "classic")
        end
        if x == nil then
            local h = half(C.town_center)
            x, y = ai.place(t, "tower", {x = tx + h, y = ty + h, first = 3, last = 12})
        end
    end
    if x == nil or (x == 0 and y == 0) then
        return result("build", "refused", "no_site")
    end
    if ai.build_unsafe(x, y) ~= nil then
        return result("build", "refused", "unsafe")
    end
    if near == "site" then
        local keep = {}
        for _, e in ipairs(vs.explore) do
            if d2(e.x, e.y, x, y) > 100 then
                keep[#keep + 1] = e
            end
        end
        vs.explore = keep
    end
    queue_build(view, vs, t, x, y)
    return result("build", "queued", "none", 1)
end

local function v_fish(view, vs, o)
    local boats, docks = o.boats, o.docks or 1
    if type(boats) ~= "number" or boats < 0 or boats > MAX_COUNT or docks < 0 or docks > 10 then
        return result("fish", "refused", "bad_argument")
    end
    local built = #ai.units{type = C.dock, built = true}
    local have_docks = #ai.units{type = C.dock} + queued_builds(vs, C.dock) + pending_builds(vs, C.dock)
    local code = "done"
    if have_docks < docks then
        local d = dock_by_fish(view, vs)
        if d.code == "refused" and built == 0 then
            return result("fish", "refused", d.reason)
        end
        code = "queued"
    end
    if built == 0 then
        return result("fish", code, code == "queued" and "none" or "no_producer", 0)
    end
    local boat = T.fishing_boat_class(view.epoch)
    if cost_of(view, boat) == nil then
        return result("fish", "refused", "not_in_epoch")
    end
    local have = #ai.units{category = "boat"} + (cost_of(view, boat).in_production or 0) + (vs.waiting[boat] or 0)
    if have < boats then
        request(view, vs, boat, boats - have)
        return result("fish", "queued", "none", boats - have)
    end
    return result("fish", code, "none", 0)
end

local function v_train(view, vs, o)
    local t, n = o.type, o.n
    local c = type(t) == "string" and cost_of(view, t) or nil
    if c == nil then
        return result("train", "refused", cls(t or "") ~= nil and "not_in_epoch" or "unknown_class")
    end
    if c.building or type(n) ~= "number" or n <= 0 or n > MAX_COUNT then
        return result("train", "refused", "bad_argument")
    end
    if not has_producer(t) then
        return result("train", "refused", "no_producer")
    end
    request(view, vs, t, n)
    return result("train", "queued", "none", n)
end

local function v_reserve(view, vs, o)
    for _, r in ipairs(RES) do
        local v = o[r] or 0
        if v < 0 or v > 1000000 then
            return result("reserve", "refused", "bad_argument")
        end
    end
    for _, r in ipairs(RES) do
        vs.reserve[r] = o[r] or 0
    end
    view.reserve = {food = vs.reserve.food, wood = vs.reserve.wood, stone = vs.reserve.stone, gold = vs.reserve.gold,
                    iron = vs.reserve.iron}
    return result("reserve", "done", "none", 0)
end

local WHAT = {attack = 1, hit_points = 9, range = 11, speed = 14}

local function v_improve(view, vs, o)
    local t, what = o.type, WHAT[o.what or ""]
    if cost_of(view, t or "") == nil then
        return result("improve", "refused", cls(t or "") ~= nil and "not_in_epoch" or "unknown_class")
    end
    if what == nil then
        return result("improve", "refused", "bad_argument")
    end
    local ic = ai.improve_cost(t, o.what)
    if ic == nil then
        return result("improve", "refused", "bad_argument")
    end
    if ic.level > ic.max then
        return result("improve", "refused", "requirements")
    end
    if not can_afford(view, vs, ic, 1, false) then
        return result("improve", "refused", "cannot_afford")
    end
    local units = ai.units{type = t, limit = 1}
    if #units == 0 then
        return result("improve", "refused", "no_producer")
    end
    local code = ai.improve(units[1], what)
    if code ~= "ok" then
        return result("improve", "refused", "cannot_afford")
    end
    spend(view, ic, 1)
    return result("improve", "done", "none", 1)
end

local function queued_tech(vs, id)
    for _, e in ipairs(vs.research) do
        if e.id == id then
            return true
        end
    end
    return false
end

local function in_queue(tn)
    for _, p in ipairs(tn.producers or {}) do
        for _, id in ipairs(ai.units{type = p}) do
            local u = U(id)
            if u ~= nil and (u.producing == tn.id) then
                return true
            end
            for _, q in ipairs(u ~= nil and u.queued or {}) do
                if q == tn.id then
                    return true
                end
            end
        end
    end
    return false
end

local function v_research(view, vs, o)
    local tn = type(o.tech) == "string" and tech(o.tech) or nil
    if tn == nil then
        return result("research", "refused", "unknown_class")
    end
    if tn.makes ~= nil or not tn.plain_producer or is_epoch_tech(tn) or tn.id == 0x7eb then
        return result("research", "refused", "bad_argument")
    end
    if tn.researched then
        return result("research", "done", "none", 0)
    end
    if not tn.available or tn.obsolete then
        return result("research", "refused", "not_in_epoch")
    end
    if queued_tech(vs, tn.id) or in_queue(tn) then
        return result("research", "refused", "busy")
    end
    local any = false
    for _, p in ipairs(tn.producers or {}) do
        any = any or #ai.units{type = p, limit = 1} > 0
    end
    if not any then
        return result("research", "refused", "no_producer")
    end
    insert_by_epoch(vs.research, {id = tn.id, e = tn.epoch or view.epoch})
    return result("research", "queued", "none", 1)
end

local function advancing(view, vs)
    for _, e in ipairs(vs.research) do
        if e.epoch then
            return true
        end
    end
    return false
end

local function v_advance(view, vs, o)
    local nxt = view.epoch + 1
    if view.epoch >= view.last_epoch then
        return result("advance", "refused", "requirements")
    end
    if view.sol_missing > 0 then
        return result("advance", "refused", "requirements")
    end
    if advancing(view, vs) or view.advancing then
        return result("advance", "refused", "busy")
    end
    local ep = ai.epoch(nxt)
    if ep == nil then
        return result("advance", "refused", "requirements")
    end
    if not tuning(vs).cheat_epochs and not can_afford(view, vs, ep.cost or {}, 1, true) then
        return result("advance", "refused", "cannot_afford")
    end
    -- (as AttemptEpochAdvance: the requests the next epoch makes obsolete go; the queues drop them as they run)
    insert_by_epoch(vs.research, {id = ep.id, e = view.epoch, epoch = true})
    vs.advancing = true
    view.advancing = true
    return result("advance", "queued", "none", 1)
end

local VERBS = {
    gather = v_gather,
    claim = function(view, vs, o) return v_claim(view, vs, o, true) end,
    release = function(view, vs, o) return v_claim(view, vs, o, false) end,
    claim_range = v_claim_range,
    build = v_build,
    fish = v_fish,
    train = v_train,
    reserve = v_reserve,
    improve = v_improve,
    research = v_research,
    advance = v_advance,
}
M.VERBS = VERBS

-- ---------------------------------------------------------------------------------------------------------------
-- the view's fields

local function decorate(view, vs)
    -- the merged results: this library's, and C++'s for the orders it passed on, in the order given
    local cpp = view.results or {}
    local merged, j = {}, 1
    for _, p in ipairs(vs.pending) do
        if p.cpp then
            merged[#merged + 1] = cpp[j] or result(p.verb, "refused", "bad_argument")
            j = j + 1
        else
            merged[#merged + 1] = p
        end
    end
    vs.pending = {}
    view.results = merged
    -- the lists
    for _, r in ipairs(RES) do
        view.gatherers[r] = count_land(vs, r)
        view.wanted[r] = r == "food" and vs.food_land or vs.wanted[r]
    end
    view.idle = count_land(vs, "idle")
    view.claim_range = vs.claim_range
    view.reserve = {food = vs.reserve.food, wood = vs.reserve.wood, stone = vs.reserve.stone, gold = vs.reserve.gold,
                    iron = vs.reserve.iron}
    if advancing(view, vs) then
        view.advancing = true
    end
    -- the build queue and the requests
    for _, b in ipairs(view.buildings) do
        b.queued = 0
    end
    for _, q in ipairs(vs.builds) do
        local entry
        for _, b in ipairs(view.buildings) do
            if b.type == q.type then
                entry = b
            end
        end
        if entry == nil then
            entry = {type = q.type, finished = 0, unfinished = 0, queued = 0, busy = 0, queue = 0}
            view.buildings[#view.buildings + 1] = entry
        end
        entry.queued = entry.queued + 1
    end
    for _, id in ipairs(vs.placed) do
        local u = U(id)
        if u ~= nil and not u.built and (u.pending or (S.placed_now ~= nil and S.placed_now[id])) then
            local entry
            for _, b in ipairs(view.buildings) do
                if b.type == u.type then
                    entry = b
                end
            end
            if entry == nil then
                entry = {type = u.type, finished = 0, unfinished = 0, queued = 0, busy = 0, queue = 0}
                view.buildings[#view.buildings + 1] = entry
            end
            entry.unfinished = entry.unfinished + 1
        end
    end
    for _, c in ipairs(view.costs) do
        c.requested = vs.waiting[c.type] or 0
    end
    -- the game in reach and the hunters
    local hunt = vs.hunt or 0
    local tx, ty = town(view)
    if tx ~= nil and view.time >= (vs.next_hunt or 0) then
        vs.next_hunt = view.time + 5000
        hunt = 0
        for _, id in ipairs(ai.resources{kind = "food", near = {tx, ty, hunt_reach(vs)}}) do
            if hunt >= HUNT_MAX then
                break
            end
            local u = U(id)
            local c = u ~= nil and cls(u.type) or nil
            if c ~= nil and c.family == FAMILY_ANIMAL and (c.attack or 0) == 0 and (u.amount or 0) > 0 and site_safe(u) then
                hunt = hunt + 1
            end
        end
    end
    vs.hunt = hunt
    view.hunt = hunt
    local hunters = 0
    for _, id in ipairs(vs.lists.food) do
        local u = U(id)
        if u ~= nil and not is_boat(u) and hunting(u) then
            hunters = hunters + 1
        end
    end
    view.hunters = hunters
    -- the events
    for _, e in ipairs(vs.events) do
        view.events[#view.events + 1] = e
    end
    vs.events = {}
end

-- ---------------------------------------------------------------------------------------------------------------
-- the step

local function runners(view, vs)
    if S.ran == view.time then
        return
    end
    S.ran = view.time
    production(view, vs)
    if vs.next_assign < 0 then
        vs.next_assign = view.time + 10000
        vs.next_check = view.time + 15000
        vs.next_builder_check = view.time + 120000
    end
    if view.time >= vs.next_assign then
        assign_gatherers(view, vs)
        vs.next_assign = view.time + 10000
    end
    if view.time >= vs.next_check then
        requeue_idle(view, vs)
        vs.next_check = view.time + 15000
    end
    if view.time >= vs.next_build then
        build_update(view, vs)
        local full = view.time >= vs.next_builder_check
        tend_foundations(view, vs, full)
        if full then
            vs.next_builder_check = view.time + 120000
        end
        vs.next_build = view.time + 3000
    end
    research_update(view, vs)
    if view.time >= vs.next_repair then
        repair(view, vs)
        vs.next_repair = view.time + 31000
    end
end

function M.begin(view, memory)
    local vs = M.state(memory)
    cur = vs
    if S.time ~= view.time or S.view ~= view then
        begin_cache(view)
        tuning(vs)
        ai.reflex{held = true}
        -- the claims (C++'s grouping sets a centre's first state; the script's claims stand by its id)
        for _, st in ipairs(view.sites) do
            st.cpp_claimed = st.claimed
            if vs.claims[st.id] ~= nil then
                st.claimed = vs.claims[st.id]
            end
        end
        refresh_lists(view, vs)
        runners(view, vs)
        decorate(view, vs)
        if view.time % 60000 < 1000 then
            local l = vs.lists
            print(string.format("verbs t=%d lists=%d/%d/%d/%d/%d/%d want=%d/%d/%d/%d/%d held=%d builds=%d req=%d res=%d claim=%d",
                view.time // 1000, #l.idle, #l.food, #l.wood, #l.stone, #l.gold, #l.iron, vs.wanted.food, vs.wanted.wood,
                vs.wanted.stone, vs.wanted.gold, vs.wanted.iron, #ai.keys(vs.sh), #vs.builds, #vs.requests, #vs.research,
                vs.claim_range))
        end
    end
    return vs
end

-- the orders: the economy verbs here, the others passed on to C++ (their results come back next step, merged)
function M.run(view, memory, orders)
    local vs = M.state(memory)
    if orders == nil then
        return nil
    end
    local out = {}
    for _, o in ipairs(orders) do
        local fn = type(o) == "table" and VERBS[o.verb] or nil
        if fn ~= nil then
            local r = fn(view, vs, o)
            vs.pending[#vs.pending + 1] = r
            if M.TRACE and o.verb ~= "reserve" then
                print(string.format("verbs.order %s %s near=%s site=%s -> %s %s %d", o.verb, tostring(o.type or o.tech or o.resource or ""),
                    tostring(o.near), tostring(o.site or o.x), r.code, r.reason, r.count))
            end
        else
            vs.pending[#vs.pending + 1] = {cpp = true, verb = type(o) == "table" and o.verb or "?"}
            out[#out + 1] = o
        end
    end
    return out
end

function M.wrap(A)
    local W = {}
    for k, v in pairs(A) do
        W[k] = v
    end
    W.init = function(view, memory)
        M.begin(view, memory)
        local orders = A.init ~= nil and A.init(view, memory) or nil
        return M.run(view, memory, orders)
    end
    W.step = function(view, memory)
        M.begin(view, memory)
        local orders = A.step(view, memory)
        return M.run(view, memory, orders)
    end
    if A.micro ~= nil then
        W.micro = function(info, memory)
            cur = M.state(memory)
            return A.micro(info, memory)
        end
    end
    if A.configure ~= nil then
        W.configure = function(opts)
            A.configure(opts)
            return W
        end
    end
    return W
end

return M
