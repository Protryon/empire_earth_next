-- Age of AI port: upstream dll/ai/explore.c (citizens that find no food explore round their town until enough is
-- found, in the first age with no food at all as many as there are; the Canine Scout circles the base ring after
-- ring; the territory scouts walk the unexplored ground and the parts looked at longest ago; [ai_base] explore = 1;
-- UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.explore"), stepped from main.lua's step (once a game second).
--
-- Upstream's decisions, ported:
--   THE MAP UNEXPLORED - upstream counts an 8-tile cell's unexplored tiles (2048 tiles a look, a cell all explored
--     never counted again, a dark cell's edge is a goal). Here the map is read through the engine's own scout hook,
--     ai.unexplored (EECPNextArmy's scout; docs/AI_LUA_UNITS.md §4): the unexplored grid point of a land mass
--     nearest a tile on its 8-tile grid - upstream's edge tile without the per-tile count.
--   A GOAL (xp_goal) - the unexplored point of the unit's own land, the base's ring first (ai.unexplored's scoring is
--     the distance to the base less half the way from `away`, so the base's rings come out; upstream's base + half
--     the unit's own way); never within XP_SPREAD of another explorer's goal or of one given up lately (the engine
--     leaves out the points within its grid of a skip entry), never on a threat (ai.danger: upstream's wk_danger,
--     the enemy towers it has seen).
--   THE FOOD SEARCH - while the worker plan wants more food gatherers than the claimed sites have places
--     (econ.lua's food_want_raw less food_places = work.c food_short), that many more free citizens - the idle ones
--     first, then the woodcutters, the nearest to the drop site - explore from that drop site, XP_HOME_R tiles out at
--     most; in the Prehistoric age with no food gatherer at all as many as there are (XP_UNITS - 2), else XP_CIT_MAX.
--     A look with every food citizen placed gives half of them back to work.
--   THE BASE SCOUT - from XP_DOG_CITS citizens on, while unexplored land lies within XP_SCOUT_R of a town, one Canine
--     Scout (domestic_wolf: 50 food at a Town Center, faster and with a longer sight than a citizen) is ordered every
--     XP_DOG_EVERY till one comes (XP_DOG_WAIT after one was paid for) - the economy's money, not the army's: the
--     stock above the engine's reserve pays it. XP_DOG_TRIES orders without a dog (or no dog in this epoch): a citizen
--     stands in for it (the idle first, then a woodcutter, the nearest), XP_SCOUT_R out, back to work when one comes
--     or nothing is left. Every Canine Scout is an explorer of its own, based on the town, whatever it was doing; out
--     of goals it waits where it is.
--   THE NEXT GOAL AT ONCE (the player, 2026-11-20) - every step (upstream 250 ms) each explorer whose goal is
--     explored now (its sight took it in on the way) - or which stands at it and it stays dark - gets the next one
--     before it stops; a goal not reached in XP_GIVE_UP is given up (a cliff, a wall) and avoided for XP_FORGET; a
--     citizen with nothing left to explore goes back to work (ai.stop + ai.release: the verbs library's again); a
--     Canine Scout stays where it is.
--   THE TERRITORY SCOUTS (stations.c) took their goals here too: M.territory_goal - the unexplored ground of the
--     scout's land first, else the part looked at longest ago (XP_STALE), else nothing (the caller's middle).
--
-- The explorers are claimed (ai.take): the verbs library, the towns and the other modules leave them alone, as
-- upstream's claims.c ClW_EXPLORE routine claim did; a released citizen is the library's again at its next pass.
--
-- Deviations (the Lua budget, docs/AI_LUA_UNITS.md §2.1):
-- - the map: ai.unexplored per goal (about 17k of charge a search, 12 searches a look at most) instead of upstream's
--   per-cell tile count, so there is no per-cell `dark` count and no territory mask (xp_ter_dark): the goal search
--   itself is the "is there anything left for it" test;
-- - the goal is an unexplored grid point (a move into the fog is allowed, §2.4), not an explored tile at the edge;
--   the unit's own half of the way is the engine's `away` term, the base's ring the dominant one;
-- - XP_STALE: intel_looked_ago is not in the API, so M.stale reads the cells the explorers themselves stood in (a
--   cell of 8 tiles, stamped every look) - the explorers' own memory of a look, not the game's;
-- - the Canine Scout's base is a town (upstream: the nearest drop site that is no granary = the town in practice);
-- - the dog's training: ai.train at the Town Center/Capitol with the stock above view.reserve checked here (upstream
--   hook_train_units on the unit manager, past the Town Center queue's depth); "an order that got no dog" is counted
--   from a dog that did not appear after XP_EVERY, since the train verb answers at once.
-- Not ported: the log line ("# explore:", the land tiles unexplored of all, the per-kind counts), the [autotest]
-- variants, the scenario's Explore toggle (player_ai_toggle SM_AIT_EXPLORE: no per-scenario AI toggles in this
-- engine), the explorers' claim timeout (a claim here lasts until the release: XP_CLAIM_T has no counterpart).
local S = require("ageofai.settings")
local V = require("lib.view")

local M = {}

-- upstream's defines (explore.c)
local XP_EVERY = 2000        -- the look
local XP_GIVE_UP = 40000     -- a goal this old is given up (a cliff, a wall)
local XP_FORGET = 180000     -- a given-up goal is avoided this long
local XP_HOME_R = 40         -- the food search's citizens: this far from the drop site
local XP_SCOUT_R = 48        -- the base scout's citizen: this far from the town
local XP_DOG_EVERY = 30000   -- a Canine Scout is ordered this often till one comes
local XP_DOG_WAIT = 120000   -- ... and this long after one was paid for
local XP_DOG_CITS = 10       -- not before this many citizens (the food search explores meanwhile)
local XP_DOG_TRIES = 3       -- orders without a dog: a citizen stands in
local XP_CIT_MAX = 4         -- the food search's citizens after the first age
local XP_UNITS = 32          -- the explorers (a first-age food search: XP_UNITS - 2)
local XP_BAD = 16            -- the goals given up lately
local XP_STALE = 300000      -- a part looked at this long ago is looked at again (M.stale)
local XP_CELL = 8            -- the looked-at cells and the engine's grid (upstream's XP_CELL)
local XP_STEP_SEARCHES = 4   -- the goal searches a step at most (upstream found one per explorer, no cap: the Lua
                             -- budget - a search walks the map's grid)

-- the roles of an explorer (upstream's xp_unit kind)
local K_FOOD, K_SCOUT, K_DOG = 1, 2, 3

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- the stock above the engine's reserve pays what the view's costs say (the train verb's CanAfford, checked here since
-- the dog is a direct ai.train)
local function can_pay(view, cost)
    for _, r in ipairs(V.resources) do
        local c = cost[r] or 0
        if c > 0 and view.stock[r] - ((view.reserve or {})[r] or 0) < c then
            return false
        end
    end
    return true
end

-- the state: memory.explore
local function state(mem)
    mem.explore = mem.explore or {units = {}, bad = {}, looked = {}}
    local s = mem.explore
    s.units = s.units or {}
    s.bad = s.bad or {}
    s.looked = s.looked or {}
    return s
end

local function at(s, id)
    for k, e in ipairs(s.units) do
        if e.id == id then
            return k
        end
    end
    return nil
end

-- a goal given up lately: its place in the avoid list (upstream's xp_goals bad ring)
local function give_up(s, e, now)
    if e.gx == nil then
        return
    end
    local k = s.bad_at ~= nil and s.bad_at or 0
    if k >= XP_BAD then
        k = 0
    end
    s.bad[k + 1] = {x = e.gx, y = e.gy, t = now > 0 and now or 1}
    s.bad_at = (s.bad_at or 0) + 1
    s.gave_up = (s.gave_up or 0) + 1
end

-- a goal (xp_goal): the unexplored point of the unit's land nearest the base, off the other explorers' goals, the
-- goals given up lately and the threat layer (the engine leaves out the points within its grid of a skip entry:
-- 8 tiles, close to upstream's XP_SPREAD 10). nil: nothing left for it
local function search(s, id, x, y, bx, by, rmax, now)
    local t = ai.tile(x, y)
    local land = t ~= nil and t.continent or 0
    local skip = {}
    for _, e in ipairs(s.units) do
        if e.id ~= id and e.gx ~= nil then
            skip[#skip + 1] = {e.gx, e.gy}
        end
    end
    for _, b in ipairs(s.bad) do
        if b ~= nil and now - b.t < XP_FORGET then
            skip[#skip + 1] = {b.x, b.y}
        end
    end
    for _ = 1, 3 do
        local gx, gy = ai.unexplored(bx, by, {land = land, grid = XP_CELL, away = {bx, by}, skip = skip})
        if gx == nil then
            return nil
        end
        if rmax > 0 and d2(gx, gy, bx, by) > (rmax + XP_CELL) * (rmax + XP_CELL) then
            return nil   -- nothing to explore within that of the base
        end
        if ai.danger(gx, gy) <= 0 then
            return gx, gy
        end
        -- under an enemy tower it has seen (wk_danger): the next edge
        s.gave_up = (s.gave_up or 0) + 1
        skip[#skip + 1] = {gx, gy}
    end
    return nil
end

-- send an explorer to a fresh goal; false: nothing to explore for it (its `since` set: asked again at the next look)
local function send(s, e, now)
    if not e.x then
        local u = ai.unit(e.id)
        if u == nil then
            return false
        end
        e.x, e.y = u.x, u.y
    end
    local gx, gy = search(s, e.id, e.x, e.y, e.bx, e.by, e.rmax or 0, now)
    e.since = now > 0 and now or 1
    e.gx, e.gy = gx, gy
    if gx == nil then
        return false
    end
    ai.move(e.id, gx, gy, {forced = true})
    s.goals = (s.goals or 0) + 1
    return true
end

-- back to work, or where it stands for a dog (xp_release): the claim ends, the citizen is idle at once
local function release(s, k, stop)
    local e = s.units[k]
    table.remove(s.units, k)
    if stop then
        ai.stop(e.id)
    end
    ai.release(e.id)
    return e
end

-- the nearest free citizen (idle first, then a woodcutter) to (x, y), made an explorer of `kind` (xp_take)
local function take(view, s, kind, x, y, bx, by, rmax, now)
    if #s.units >= XP_UNITS then
        return false
    end
    for pass = 1, 2 do
        -- (the map is at most a few hundred tiles: a radius that covers it; a bigger one overflows the binding's sum)
        local filter = {category = "citizen", near = {x, y, 1000}}
        if pass == 1 then
            filter.idle = true
        else
            filter.action = "gather"
        end
        for _, id in ipairs(ai.units(filter)) do
            if at(s, id) == nil then
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil and u.held ~= "lua" and (pass == 1 or u.gathers == "wood") then
                    local e = {id = id, kind = kind, gx = nil, gy = nil, since = now > 0 and now or 1,
                               bx = bx, by = by, x = u.x, y = u.y, rmax = rmax}
                    s.units[#s.units + 1] = e
                    ai.take(id)
                    if not send(s, e, now) then
                        release(s, #s.units, false)   -- nothing to explore for it: it stays at work
                        return false
                    end
                    if kind == K_FOOD then
                        s.sent = (s.sent or 0) + 1
                    else
                        s.scouts = (s.scouts or 0) + 1
                    end
                    return true
                end
            end
        end
    end
    return false
end

-- THE NEXT GOAL AT ONCE: each explorer's goal reached (or explored by its sight), given up, or the next one
local function advance(view, s, now)
    if #s.units == 0 then
        return
    end
    local ids = {}
    for k, e in ipairs(s.units) do
        ids[k] = e.id
    end
    local c = ai.get(ids, {"x", "y", "action"})
    local searches = 0
    for k = #s.units, 1, -1 do
        local e = s.units[k]
        local x, y, action = c.x[k], c.y[k], c.action[k]
        if x == nil then
            table.remove(s.units, k)   -- dead, or converted (no longer ours)
        else
            e.x, e.y = x, y
            local edge, explored = false, false
            if e.gx ~= nil then
                local t = ai.tile(e.gx, e.gy)
                edge = t ~= nil and not t.explored
                explored = not edge
            end
            local idle = action == nil or action == "idle" or action == "inside"
            local there = e.gx ~= nil and d2(x, y, e.gx, e.gy) <= 4 and idle
            local next_one = false
            if e.gx == nil then
                -- none found lately: asked again at the next look (upstream: every XP_EVERY)
                next_one = not (e.since ~= nil and now - e.since < XP_EVERY)
            elseif explored then
                next_one = true   -- its sight took the edge in on the way: the next goal at once
            elseif edge and (there or now - e.since >= XP_GIVE_UP) then
                give_up(s, e, now)   -- stands there in the dark, or not reached in XP_GIVE_UP (a cliff, a wall)
                next_one = true
            elseif edge and idle and (e.re_t or 0) + XP_EVERY <= now then
                e.re_t = now > 0 and now or 1
                ai.move(e.id, e.gx, e.gy, {forced = true})   -- stopped short of it: the same goal again
            end
            -- (else it is on its way)
            if next_one then
                if searches >= XP_STEP_SEARCHES then
                    return   -- the step's search budget: the next step asks for it
                end
                searches = searches + 1
                if not send(s, e, now) and e.kind < K_DOG then
                    release(s, k, true)   -- nothing left for it: back to work (a Canine Scout waits)
                    s.back = (s.back or 0) + 1
                end
            end
        end
    end
end

-- the looked-at cells: the explorer's own cell is stamped each look (M.stale's memory; upstream reads intel.c's)
local function looked(s, x, y, now)
    local cx, cy = x // XP_CELL, y // XP_CELL
    s.looked[tostring(cx) .. "," .. tostring(cy)] = now > 0 and now or 1
end

-- the base a food search works round: the nearest own drop site that is no granary, else where it stands (xp_base)
local function food_base(view, x, y)
    local _, _, dx, dy = ai.drop_site(x, y)
    if dx ~= nil and dx >= 0 then
        return dx, dy
    end
    if view.town_x >= 0 then
        return view.town_x, view.town_y
    end
    return x, y
end

-- THE FOOD SEARCH and THE BASE SCOUT (the look)
local function food_and_dog(view, memory, s, now)
    local cits = view.citizens or 0
    local nfood = 0
    for _, e in ipairs(s.units) do
        if e.kind == K_FOOD then
            nfood = nfood + 1
        end
    end
    local eco = memory.eco or {}
    local short = 0
    if eco.food_places ~= nil then
        short = math.max((eco.food_want_raw or 0) - (eco.food_places or 0), 0)
    end
    if short > 0 and cits > 0 then
        -- in the first age with no food gatherer at all: as many as there are (the player's own start)
        local all = view.epoch == 0 and (view.gatherers.food or 0) == 0
        local cap = all and (XP_UNITS - 2) or XP_CIT_MAX
        if nfood < cap then
            local want = math.min(short, cap - nfood)
            for _ = 1, want do
                local ax, ay = food_base(view, view.town_x or 0, view.town_y or 0)
                if not take(view, s, K_FOOD, ax, ay, ax, ay, XP_HOME_R, now) then
                    break
                end
            end
        end
    elseif short <= 0 and nfood > 0 then
        -- every food citizen placed: half of them back to work
        local give = (nfood + 1) // 2
        for k = #s.units, 1, -1 do
            if give <= 0 then
                break
            end
            if s.units[k].kind == K_FOOD then
                release(s, k, true)
                s.back = (s.back or 0) + 1
                give = give - 1
            end
        end
    end

    -- THE BASE SCOUT: its Canine Scouts, each an explorer of its own round its town
    local dogs = 0
    for k = #s.units, 1, -1 do
        if s.units[k].kind == K_DOG then
            dogs = dogs + 1
        end
    end
    local town_x, town_y = view.town_x, view.town_y
    for _, id in ipairs(ai.units{type = "domestic_wolf", limit = XP_UNITS}) do
        local k = at(s, id)
        if k == nil and #s.units < XP_UNITS then
            local u = ai.unit(id)
            if u ~= nil and u.inside == nil then
                local bx, by = town_x, town_y
                if bx == nil or bx < 0 then
                    bx, by = food_base(view, u.x, u.y)
                end
                local e = {id = id, kind = K_DOG, gx = nil, gy = nil, since = now > 0 and now or 1,
                           bx = bx, by = by, x = u.x, y = u.y, rmax = 0}
                s.units[#s.units + 1] = e
                ai.take(id)
                dogs = dogs + 1
                s.dogs = (s.dogs or 0) + 1
            end
        end
    end
    for k = #s.units, 1, -1 do
        if dogs > 0 and s.units[k].kind == K_SCOUT then
            release(s, k, true)   -- a Canine Scout out: the citizen standing in for one back to work
            s.back = (s.back or 0) + 1
        end
    end
    if dogs > 0 then
        s.dog_asks = 0
    end
    if dogs > 0 or cits < XP_DOG_CITS or town_x == nil or town_x < 0 then
        return
    end
    local cost = V.cost(view, "domestic_wolf")
    local scouts = 0
    for _, e in ipairs(s.units) do
        if e.kind == K_SCOUT then
            scouts = scouts + 1
        end
    end
    -- a Canine Scout ordered every XP_DOG_EVERY till one comes (XP_DOG_WAIT after one was paid for)
    local ask = cost ~= nil and (s.dog_t or 0) + XP_DOG_EVERY <= now and (s.dog_ok or 0) + XP_DOG_WAIT <= now
    -- XP_DOG_TRIES orders refused in a row (or no dog in this epoch): a citizen meanwhile
    local stand = scouts == 0 and (cost == nil or (s.dog_asks or 0) >= XP_DOG_TRIES)
    if not ask and not stand then
        return
    end
    -- unexplored land within XP_SCOUT_R of a town (xp_goal(base, XP_SCOUT_R)): the search itself is the test
    local gx, gy = search(s, -1, town_x, town_y, town_x, town_y, XP_SCOUT_R, now)
    if gx == nil then
        return
    end
    if ask then
        -- the economy's money: the stock above the engine's reserve pays it
        local towns = {}
        for _, id in ipairs(ai.units{type = {"b_town_center", "b_capitol"}, built = true, limit = 8}) do
            towns[#towns + 1] = id
        end
        if #towns > 0 and can_pay(view, cost.cost) then
            s.dog_t = now > 0 and now or 1
            local _, _, got = ai.train(towns, "domestic_wolf", 1)
            -- the order is answered at once here: an order that got no dog is counted now (upstream counted the
            -- request's answer), one paid for holds the next ask for XP_DOG_WAIT
            if (got or 0) > 0 then
                s.dog_ok = now > 0 and now or 1
            else
                s.dog_asks = (s.dog_asks or 0) + 1
            end
        end
    end
    if stand then
        take(view, s, K_SCOUT, town_x, town_y, town_x, town_y, XP_SCOUT_R, now)
    end
end

-- the look (explore_tick): every XP_EVERY
local function look(view, memory, s, now)
    advance(view, s, now)
    -- the map of looks: the explorers' own cells, stamped every look (M.stale's memory); the old ones forgotten
    for _, e in ipairs(s.units) do
        if e.x ~= nil then
            looked(s, e.x, e.y, now)
        end
    end
    if view.town_x ~= nil and view.town_x >= 0 then
        looked(s, view.town_x, view.town_y, now)
    end
    if (s.prune_t or 0) + 60000 <= now then
        s.prune_t = now > 0 and now or 1
        for _, key in ipairs(ai.keys(s.looked)) do
            if now - s.looked[key] >= 2 * XP_STALE then
                s.looked[key] = nil
            end
        end
    end
    food_and_dog(view, memory, s, now)
    advance(view, s, now)
end

-- THE TERRITORY SCOUTS' goal (stations.c xp_stale's part, for a scout at x, y): the unexplored ground of its land,
-- else the cell of its land looked at longest ago (XP_STALE at least), else nil (the caller's middle)
function M.territory_goal(view, memory, x, y, cx, cy)
    local s = state(memory)
    local now = view.time
    local t = ai.tile(x, y)
    if t == nil then
        return nil
    end
    local land = t.continent or 0
    local gx, gy = search(s, -1, x, y, cx or x, cy or y, 0, now)
    if gx ~= nil then
        return gx, gy
    end
    return M.stale(view, memory, land)
end

-- the part of the land looked at longest ago (>= XP_STALE), by the cells the explorers stood in; nil: none
function M.stale(view, memory, land)
    local s = state(memory)
    local now = view.time
    local best, bx, by = nil, nil, nil
    for _, key in ipairs(ai.keys(s.looked)) do
        local px, py = key:match("^(-?%d+),(-?%d+)$")
        if px ~= nil then
            local qx, qy = tonumber(px) * XP_CELL + XP_CELL // 2, tonumber(py) * XP_CELL + XP_CELL // 2
            local t = ai.tile(qx, qy)
            if t ~= nil and (land == nil or land == 0 or t.continent == land) then
                local ago = now - s.looked[key]
                if ago >= XP_STALE and (best == nil or ago > best) then
                    best, bx, by = ago, qx, qy
                end
            end
        end
    end
    return bx, by
end

-- whether unexplored land of (x, y)'s land lies within r of (x, y) - the dog's test, and the navy's/outposts'
function M.dark_near(view, memory, x, y, r)
    local s = state(memory)
    local gx, gy = search(s, -1, x, y, x, y, r, view.time)
    return gx ~= nil
end

-- the step (explore_tick + explore_step in one): every game second
function M.step(view, memory)
    local s = state(memory)
    local now = view.time
    if S.explore == false then
        for k = #s.units, 1, -1 do
            release(s, k, false)
        end
        return
    end
    advance(view, s, now)
    if (s.t or 0) > 0 and now - s.t < XP_EVERY then
        return
    end
    s.t = now > 0 and now or 1
    look(view, memory, s, now)
    if (s.log_t or 0) + 60000 <= now and (#s.units > 0 or (s.sent or 0) > 0 or (s.scouts or 0) > 0) then
        s.log_t = now > 0 and now or 1
        local nfood, scout, dog = 0, 0, 0
        for _, e in ipairs(s.units) do
            nfood = nfood + (e.kind == K_FOOD and 1 or 0)
            scout = scout + (e.kind == K_SCOUT and 1 or 0)
            dog = dog + (e.kind == K_DOG and 1 or 0)
        end
        print(string.format("explore out=%d food=%d scout=%d dog=%d | sent=%d back=%d goals=%d gave_up=%d dogs=%d asks=%d",
            #s.units, nfood, scout, dog, s.sent or 0, s.back or 0, s.goals or 0, s.gave_up or 0, s.dogs or 0, s.dog_asks or 0))
    end
end

return M
