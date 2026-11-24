-- Age of AI port: upstream dll/ai/build.c (BUILDING BY TERRITORY: which building in which territory, the claims of
-- empty ground, the sinks, the foundations held until they show; UPSTREAM has the map and the licence, GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.build").
--
-- Upstream's build.c plays while the game's own computer player is off: its build manager, its plan and its repair
-- rounds don't run then, so nothing is built at all. Every BD_EVERY (5 s) it looks at the territory view and lays
-- ONE building, in this order (build.c build_tick):
--   FIELDS        the 8 places round each finished granary that hold no field (a field pays its wood when it starts);
--   A GRANARY     from the Copper Age, every field laid: by a town of the best territory (BACKLAND first), room for 6
--                 of its 8 fields; A GRANARY BEFORE THE NEXT asks garrison to fill a granary with room inside first;
--   RESEARCH      one University, one Hospital and one Temple as the game offers them, by a Town Center or Capitol,
--                 before the mines' buildings; THE PAIR: the first Hospital and the first Temple within 5 tiles;
--   PRODUCTION    the building produce.c asks for (a territory's first production, a hub's next kind, one more of a
--                 busy kind), 3 to 10 tiles from a town; its very first one right after the University (when
--                 produce.lua is wired it decides AND lays production itself - this module leaves it alone, as
--                 upstream's build.c laid what produce.c only asked for);
--   AN AIRPORT    airstrike.c's ask (a wing short of airports), by a town;
--   A MINE'S SETTLEMENT  every mine of a worked territory (or beside one) with no drop site of its, 2 to 6 tiles off,
--                 gold and iron a tile's gap at most; across the water the TOWER comes first;
--   A MINE'S TOWER       a mine with its drop site and no defence within 7 tiles;
--   A HERD'S SETTLEMENT  before farms, a settlement by a wild herd out of every drop site's hunting reach;
--   TO THE TREES ACROSS  idle citizens: a settlement by the trees on a land with no drop site of its;
--   A FISHING DOCK       citizens short on food: a dock on the shore by the fish nearest a drop site;
--   A CLAIM              a tower (a Barracks when no tower can be built or paid) in the middle of a territory nobody
--                 has a building in, beside one it holds with 3 soldiers there (claim_from_towers: with a tower);
--                 one every 2 minutes, the claims going up hold the next back; a claim's lone tower gets a Settlement
--                 before the next claim;
--   A WATCH TOWER        with nothing else to lay, over ground it rarely has in sight (watch_towers);
--   THE MORALE'S HOUSES  by a Town Center (2 levels) or a Capitol (4), a square 10 tiles each way, one at a time;
--   THE WOOD SINK        from 10 000 wood in the bank, a Hospital, a Temple, a University, an Airport or a production
--                 building in a quiet front territory (a backland when no front is quiet); the STONE SINK from 5 000
--                 stone for the kinds that cost stone.
-- HOLDING THE GROUND (hold_ground) is the one part that lays on its own: its idle citizens where the enemy is near put
-- up a tower, else a production building, and build it. A building it lays is one it may not pay yet: LAID, NOT PAID
-- holds its price until its foundation shows (BD_WAIT, the builder walks there first). A building it cannot place is
-- dropped after its tries; nothing is laid under raiders within 12 tiles, in the reach of an enemy tower it knows, or
-- in a territory being lost. Its builders, its wall pieces and its repairs are the engine's build runner here
-- (docs/AI_LUA_VERBS.md §2: AssignBuilders, ai.wall, the repairs every 31 s) - upstream had to send its own because
-- the game's rounds are off; that is an engine gap, not a decision of this file.
--
-- THIS PORT. The port's building decisions live in econ.lua (settlements, production, farms, towers, SOL) and towns.lua
-- (towns, outposts); this module is the territory-driven replacement, OFF unless S.build_by_territory is set, so the
-- shipped play is unchanged. What it replaces when it is on (and with which switch):
--   econ.lua settlements (drop sites by the claimed sites, the forest edge)  -> this file's mine/forest/herd passes
--   econ.lua production (a barracks, a range, a stable, army_plan BUILD)     -> this file's PRODUCTION
--   econ.lua farms (granaries and their fields, ai.replant)                  -> this file's FIELDS and A GRANARY
--   econ.lua towers (the core, the fronts, the outposts)                     -> NOT this file: territories.c BORDER
--     TOWERS is the territory system's own; this file's towers are the mines', the claims' and the watch's only
--   econ.lua sol (the next epoch's SOL building)                            -> untouched: a claimed territory's
--     buildings count toward the SOL by themselves (the epoch's rule is econ's, not the territory's)
--   towns.lua's drop site and towers inside a town's project                  -> untouched: a town of the script's
--     keeps its own sites (this file reads memory.eco.town_spots and keeps off them, as econ.lua does)
-- The wiring and the switches are in the report; build.step goes after econ's step in main.lua.
--
-- THE TERRITORIES. The territory model is ageofai.territories (the parallel port): the land and the seas, held, front
-- or backland, the guard. This module reads it when it is there and falls back to the continents and ai.tile when it is
-- not (or before its model is up). The API read (the parallel port's own, checked against it):
--   T.info(i)          a territory's record by its index (nil past the last), with x, y (its middle), land,
--                      role (0 none, 1 front, 2 back), state (0 quiet, 1 foes, 2 unsafe), hold (the finished
--                      buildings of its that shoot or train), bld (its buildings there), ebld (an enemy's it has
--                      seen), abld (an ally's), sold (its land soldiers), def (its defences), shore, fish
--   T.terr_at(x, y)    the index of the tile's territory (nil: none, or nothing known there)
--   T.state_of(x, y)   the being-lost state without the shelter setting's gate; T.unsafe(x, y) with it
--   T.list(view, memory) / T.at(x, y) / T.cost(x, y) / T.unsafe(x, y) are also accepted, for a module that grows
--                      work.c's wk_cost (the price of working a territory: 1 backland, 2 a front it holds, 3/6 its
--                      building or nobody's, 0 not to be worked) - without it this module derives the same price from
--                      the record (below, tv_from_module).
-- The fallback answers at()/cost()/unsafe() from ai.tile's continent ids, the own buildings and the enemy buildings of
-- ai.memory, and treats the whole of a continent as one territory (DEVIATION: on a one-continent map the backland is
-- the ground within S.backland_radius of the base with no enemy building known around it - the territory split is the
-- parallel port's job).
--
-- COST: one look a 5 s, one building a look, the census of its buildings and the territory list cached; the searches are
-- the engine's (ai.find_site, ai.can_place) or a ring of 8 tiles round a granary; nobody scans all units every step.
local V = require("lib.view")
local S = require("ageofai.settings")
local P = require("ageofai.priority")   -- [priority.c] the list every spender asks
local PK = require("ageofai.packing")   -- [packing.c] the packed production buildings
local M = {}

local okT, T = pcall(require, "ageofai.territories")   -- the parallel port; optional
if not okT then
    T = nil
end

-- the upstream defines (build.c)
local BD_EVERY = 5000
local BD_GRANARIES = 96      -- no limit of its own (one more whenever citizens wanted on food find no site)
local BD_DROP = 8            -- a mine's Settlement within this ...
local BD_DROP_METAL = 3      -- ... a gold or iron mine's within this
local BD_DSITES = 64         -- the drop sites remembered for the mine's test (upstream 2048)
local BD_GUARD = 7           -- a mine's defence within this (each way)
local BD_CLAIM_EVERY = 120000
local BD_CLAIM_GUARD = 3     -- its soldiers in the held territory a claim starts from
local BD_CLAIM_MINE = 2      -- a territory with a mine counts as this many times nearer to the base
local BD_CLAIM_OPEN = 2      -- its claims not holding yet that hold the next back
local BD_CLAIM_LONE = 360000 -- a claim's Settlement still missing holds the next back this long at most
local BD_HOUSES_TC = 2       -- the levels a Town Center's morale takes ...
local BD_HOUSES_CAPITOL = 4  -- ... a Capitol's
local BD_HOUSE_REACH = 9     -- a House's tile within this of the building's, each way (the square is 10)
local BD_HOUSE_EVERY = 60000
local BD_HOUSE_PEND = 180000
local BD_HERD_EVERY = 120000
local BD_FISH_EVERY = 120000
local BD_FISH_DOCKS = 4      -- while it has fewer docks (going up too)
local BD_FISH_NEAR = 30      -- the fish this near a drop site of its ...
local BD_FISH_R = 16         -- ... and no dock of its this near the fish
local BD_PAIR_R = 5          -- THE PAIR: its first Hospital and Temple this far apart at most
local BD_SINK = 10000        -- wood in the bank from which it builds unasked
local BD_SINK_EVERY = 20000
local BD_STONE_SINK = 5000   -- stone in the bank from which THE STONE SINK lays the kinds that cost stone
local BD_SINK_MAX = 3        -- of a kind in one territory
local BD_SINK_OPEN = 4       -- unfinished buildings
local BD_WATCH_EVERY = 120000
local BD_WATCH_NEAR = 8      -- no defence of its within this of the watch spot
local BD_HOLD_R = 12         -- HOLDING THE GROUND: enemy soldiers or buildings this near an idle citizen of its
local BD_HOLD_TOWER = 8
local BD_HOLD_PROD = 16
local BD_HOLD_EVERY = 20000
local BD_HOLD_N = 8
local BD_WAIT = 60000        -- LAID, NOT PAID: no other of its kind until its foundation shows (the builder walks)
local BD_GRAN_ASK = 180000
local BD_GRAN_PLAIN = 3      -- its first granaries are laid without the room's ask
local BD_FOREST_IDLE = 8     -- TO THE TREES ACROSS: idle citizens
local BD_FOREST_EVERY = 60000
local BD_NO_SITE_MS = 300000 -- a type the placement refused waits this long ...
local BD_DROP_TRIES = 3      -- ... and after this many tries is dropped ...
local BD_DROP_MS = 600000    -- ... for this long (upstream retry_dropped: 2 minutes, 5 tries)

local FARM_EPOCH = 2
local FAMILY_ANIMAL, FAMILY_FISH = 0x11, 0x12   -- ai.class(...).family (verbs.lua's constants)
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}
local RESEARCH = {"b_university", "b_hospital", "b_temple"}
local PROD_KINDS = {"b_barracks", "b_archery_range", "b_stable", "b_siege_factory", "b_tank_factory", "b_cyber_factory"}
local DROP_TYPES = {b_settlement = true, b_town_center = true, b_capitol = true}
local CENTRE_TYPES = {b_town_center = true, b_capitol = true}
-- the reach of its Hospital, Temple and University (AREA OF EFFECT: the game's circles' outer edge; upstream adds a
-- level of the range upgrades it has researched, which no ai.class field here carries - the outer edge stands in)
local AOE_REACH = {b_university = 18, b_hospital = 7, b_temple = 16}
-- the kinds a lay is booked under (LAID, NOT PAID's slots 1..6 are build.c wait[]'s; 7 house, 8 field, 9 dock)
local KIND = {b_granary = 1, b_granary_improved = 1, b_settlement = 2, b_town_center = 2, b_capitol = 2,
              b_university = 4, b_hospital = 4, b_temple = 4, b_airport = 6}
for _, t in ipairs(PROD_KINDS) do
    KIND[t] = 5
end

local function key(x, y)
    return x * 4096 + y
end

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function is_tower(t)
    for _, v in ipairs(TOWER_CLASSES) do
        if v == t then
            return true
        end
    end
    return false
end

local function is_wall(t)
    return string.sub(t, 1, 6) == "b_wall" or string.sub(t, 1, 7) == "b_gate_"
end

local function tower_class(view)
    for k = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[k]) ~= nil then
            return TOWER_CLASSES[k]
        end
    end
    return nil
end

local function zero()
    local r = {}
    for _, k in ipairs(V.resources) do
        r[k] = 0
    end
    return r
end

local function add_cost(view, t, into)
    local c = V.cost(view, t)
    if c == nil then
        return
    end
    for _, r in ipairs(V.resources) do
        into[r] = (into[r] or 0) + (c.cost[r] or 0)
    end
end

-- forward declarations (M.claim and the look's order use them)
local make_ctx, lay, spot_production, results

-- ---------------------------------------------------------------------------------------------------------------
-- The territories: the parallel port when it is there, the continents when it is not

local TV_FRESH = 10000

-- the parallel port's shape first (info/terr_at), this module's own names too (list/at)
local function module_tv()
    if T == nil then
        return false
    end
    if type(T.info) == "function" and type(T.terr_at) == "function" then
        return true
    end
    return type(T.list) == "function" and type(T.at) == "function"
end

local function role_of(r)
    local v = r.role
    if type(v) == "number" then
        return v == 1 and "front" or v == 2 and "back" or "none"
    end
    if v == "front" or v == "back" then
        return v
    end
    return "none"
end

local function state_of(r)
    local v = r.state
    if type(v) == "number" then
        return v == 2 and "unsafe" or v == 1 and "foes" or "quiet"
    end
    if v == "unsafe" or v == "foes" then
        return v
    end
    return "quiet"
end

local function norm(r)
    return {water = r.water == true, x = r.x or r.cx or 0, y = r.y or r.cy or 0,
            role = role_of(r), state = state_of(r), hold = r.hold or 0, bld = r.bld or 0, ebld = r.ebld or 0,
            abld = r.abld or 0, sold = r.sold or 0, def = r.def or 0, shore = r.shore or 0, fish = r.fish or 0,
            nb = r.nb}
end

local function tv_from_module(view, memory)
    local out = {list = {}, index = {}}
    if type(T.info) == "function" and type(T.terr_at) == "function" then
        -- ageofai.territories as it stands: info(i) until nil, terr_at(x, y); the price of working a territory is
        -- upstream's wk_cost read off the record (the module keeps no cost yet)
        for i = 1, 96 do
            local r = T.info(i)
            if r == nil then
                break
            end
            local n = norm(r)
            n.i = i
            if r.land == false then
                n.water = true
            end
            out.list[#out.list + 1] = n
            out.index[i] = n
        end
        if #out.list == 0 then
            return nil   -- (the model is not up yet: the continents stand in)
        end
        out.at = function(x, y)
            local i = T.terr_at(x, y)
            return i ~= nil and out.index[i] or nil
        end
        out.cost = function(x, y)
            local r = out.at(x, y)
            if r == nil or r.water then
                return 0
            end
            local st = type(T.state_of) == "function" and T.state_of(x, y) or r.state
            if st == 2 or st == "unsafe" then
                return 0
            end
            if r.role == "back" then
                return 1
            end
            if r.role == "front" then
                return 2
            end
            if r.bld >= 1 then
                return 3
            end
            if r.ebld > 0 or r.abld > 0 then
                return 0
            end
            return 6
        end
        out.unsafe = function(x, y)
            if type(T.unsafe) == "function" then
                return T.unsafe(x, y) == true
            end
            return type(T.state_of) == "function" and T.state_of(x, y) == 2 or false
        end
        if type(T.neighbours) == "function" then
            out.neighbours = function(i)
                return T.neighbours(i)
            end
        end
        return out
    end
    local list = T.list(view, memory)
    if type(list) ~= "table" then
        return nil
    end
    for k, r in ipairs(list) do
        if type(r) == "table" then
            local n = norm(r)
            n.i = k
            out.list[#out.list + 1] = n
            out.index[k] = n
        end
    end
    out.at = function(x, y)
        local i = T.at(x, y)
        if i == nil then
            return nil
        end
        return out.index[i] or out.index[i + 1]
    end
    out.cost = type(T.cost) == "function" and function(x, y) return T.cost(x, y) or 0 end
        or function(x, y)
            local r = out.at(x, y)
            if r == nil or r.water then
                return 0
            end
            return r.role == "back" and 1 or 2
        end
    out.unsafe = type(T.unsafe) == "function" and function(x, y) return T.unsafe(x, y) == true end
        or function(x, y)
            local r = out.at(x, y)
            return r ~= nil and r.state == "unsafe"
        end
    return out
end

-- the fallback (DEVIATION: a continent is a territory; on a one-continent map the backland is the ground near the
-- base with no enemy building known around it - see the header)
local function tv_fallback(view, memory)
    local out = {list = {}}
    local byc, foe_cont, enemy_near = {}, {}, {}
    local base_x, base_y = view.town_x, view.town_y
    local rad = S.backland_radius or 60
    local function cont(x, y)
        local t = ai.tile(x, y)
        return t ~= nil and (t.continent or 0) or 0
    end
    local function rec(c, x, y)
        local r = byc[c]
        if r == nil then
            r = {i = #out.list + 1, x = x, y = y, sx = x, sy = y, n = 1, water = false, role = "back",
                 state = "quiet", hold = 0, bld = 0, ebld = 0, abld = 0, sold = 0, def = 0, nb = {}}
            byc[c] = r
            out.list[#out.list + 1] = r
            return r
        end
        r.sx, r.sy, r.n = r.sx + x, r.sy + y, r.n + 1
        r.x, r.y = r.sx // r.n, r.sy // r.n
        return r
    end
    local ids = ai.units{category = "building"}
    if #ids > 0 then
        local cols = ai.get(ids, {"type", "x", "y", "armed"})
        for k = 1, #ids do
            local t, x, y = cols.type[k], cols.x[k], cols.y[k]
            if t ~= nil and x ~= nil and not is_wall(t) then
                local c = cont(x, y)
                if c ~= 0 then
                    local r = rec(c, x, y)
                    r.bld = r.bld + 1
                    if CENTRE_TYPES[t] or PK.is_type(t) or is_tower(t) or cols.armed[k] == true then
                        r.hold = r.hold + 1
                    end
                    if is_tower(t) or cols.armed[k] == true then
                        r.def = r.def + 1
                    end
                end
            end
        end
    end
    local eb = ai.memory{buildings = true, limit = 200}
    for _, b in ipairs(eb) do
        if b.x ~= nil then
            local c = cont(b.x, b.y)
            if c ~= 0 then
                foe_cont[c] = true
                local r = byc[c]
                if r ~= nil then
                    r.ebld = r.ebld + 1
                end
            end
        end
    end
    for c, r in pairs(byc) do
        local reach = (base_x ~= nil and cont(base_x, base_y) == c) and rad + 20 or rad
        local bld = ai.memory{buildings = true, near = {r.x, r.y, reach}, limit = 1}
        local foe = ai.seen{enemy = true, category = "soldier", near = {r.x, r.y, 25}, limit = 1}
        enemy_near[c] = #bld > 0 and d2(r.x, r.y, base_x or 0, base_y or 0) <= reach * reach
        r.role = (#bld > 0 or #foe > 0) and "front" or "back"
        r.state = ai.danger(r.x, r.y) ~= 0 and "unsafe" or "quiet"
    end
    out.at = function(x, y)
        local c = cont(x, y)
        return c ~= 0 and byc[c] or nil
    end
    out.cost = function(x, y)
        local t = ai.tile(x, y)
        if t == nil or t.explored ~= true or t.water == true or (t.continent or 0) == 0 then
            return 0
        end
        local r = byc[t.continent]
        if r == nil then
            return foe_cont[t.continent] and 0 or 6
        end
        if r.state == "unsafe" then
            return 0
        end
        if r.role == "front" and base_x ~= nil and not enemy_near[t.continent]
                and d2(x, y, base_x, base_y) <= rad * rad then
            return 1   -- (the one-continent map's backland: near home with no enemy known around it)
        end
        return r.role == "front" and 2 or 1
    end
    out.unsafe = function(x, y)
        local r = out.at(x, y)
        return r ~= nil and r.state == "unsafe"
    end
    return out
end

-- the territory view (cached for TV_FRESH; in memory.build)
function M.tv(view, memory)
    memory.build = memory.build or {t = 0, pending = {}}
    local B = memory.build
    if B.tvm ~= nil and view.time - (B.tv_t or 0) < TV_FRESH then
        return B.tvm
    end
    B.tv_t = view.time
    local tv = nil
    if module_tv() then
        local ok, v = pcall(tv_from_module, view, memory)
        if ok and v ~= nil then
            tv = v
        end
    end
    B.tvm = tv or tv_fallback(view, memory)
    return B.tvm
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look: what it has (build.c's first walk over its units)

local function census(view, memory, tv)
    local C = {n = {}, total = {}, fields = {}, houses = {}, centres = {}, drops = {}, defs = {}, prodp = {},
               rb = {0, 0, 0}, rb_at = {{}, {}, {}}, granaries = {}, gran_room = 0, open_kind = {0, 0, 0, 0, 0, 0},
               open_n = 0, lost = 0, open_fields = 0}
    local ids = ai.units{category = "building"}
    if #ids == 0 then
        return C
    end
    local cols = ai.get(ids, {"type", "x", "y", "built", "armed", "garrisoned"})
    for k = 1, #ids do
        local t, x, y = cols.type[k], cols.x[k], cols.y[k]
        if t ~= nil and x ~= nil and not is_wall(t) then
            local built = cols.built[k]
            if built == nil then
                built = true
            end
            local workable = tv.cost(x, y) > 0
            local going = not built and workable
            C.total[t] = (C.total[t] or 0) + 1
            if built then
                C.n[t] = (C.n[t] or 0) + 1
            else
                C.open_n = C.open_n + 1
                if not workable then
                    C.lost = C.lost + 1   -- LEFT ON LOST GROUND: it no longer counts as going up
                end
            end
            local kd = KIND[t]
            if kd ~= nil and kd <= 6 then
                C.open_kind[kd] = C.open_kind[kd] + (going and 1 or 0)
            end
            if t == "b_farm" then
                C.fields[key(x, y)] = true
                if not built then
                    C.open_fields = C.open_fields + 1
                end
            elseif t == "b_granary" or t == "b_granary_improved" then
                if built then
                    C.granaries[#C.granaries + 1] = {x = x, y = y}
                    local cls = ai.class(t)
                    local room = (cls ~= nil and cls.garrison or 0) - (cols.garrisoned[k] or 0)
                    if workable and room > 0 then
                        C.gran_room = C.gran_room + room
                    end
                end
            elseif t == "b_house" then
                C.houses[#C.houses + 1] = {x = x, y = y}
            end
            if built and CENTRE_TYPES[t] then
                C.centres[#C.centres + 1] = {x = x, y = y, max = t == "b_capitol" and BD_HOUSES_CAPITOL or BD_HOUSES_TC,
                                             cost = tv.cost(x, y)}
            end
            if DROP_TYPES[t] and (built or going) and #C.drops < BD_DSITES then
                C.drops[#C.drops + 1] = {x = x, y = y}
            end
            if (is_tower(t) or cols.armed[k] == true) and #C.defs < 512 then
                C.defs[#C.defs + 1] = {x = x, y = y}
            end
            if PK.is_type(t) and #C.prodp < 256 then
                C.prodp[#C.prodp + 1] = {x = x, y = y}
            end
            for j = 1, 3 do
                if t == RESEARCH[j] then
                    C.rb[j] = C.rb[j] + 1
                    if C.rb_at[j][1] == nil then
                        C.rb_at[j] = {x = x, y = y}
                    end
                end
            end
        end
    end
    return C
end

-- the enemy soldiers it knows where they stand (build.c raiders_near; one read a look)
local function enemies(ctx)
    if ctx.foes ~= nil then
        return ctx.foes
    end
    local list = {}
    local ids = ai.seen{enemy = true, armed = true, category = "soldier", limit = 64}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y"})
        for k = 1, #ids do
            if cols.x[k] ~= nil then
                list[#list + 1] = {x = cols.x[k], y = cols.y[k]}
            end
        end
    end
    ctx.foes = list
    return list
end

local function raiders(ctx, x, y, r)
    for _, f in ipairs(enemies(ctx)) do
        if d2(x, y, f.x, f.y) <= r * r then
            return true
        end
    end
    return false
end

-- not under raiders within 12, not in a territory being lost, not in the reach of an enemy tower it knows
local function safe_ground(ctx, x, y)
    return not raiders(ctx, x, y, 12) and not ctx.tv.unsafe(x, y) and ai.danger(x, y) == 0
end

local function near_any(list, x, y, r)
    local r2 = r * r
    for _, p in ipairs(list) do
        if d2(p.x, p.y, x, y) <= r2 then
            return true
        end
    end
    return false
end

-- each way (build.c's abs(dx) <= BD_GUARD && abs(dy) <= BD_GUARD)
local function near_box(list, x, y, r)
    for _, p in ipairs(list) do
        if math.abs(p.x - x) <= r and math.abs(p.y - y) <= r then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- Paying: the bank pays one of type with what is laid and not paid yet (com) left alone (build.c bd_pays)

local function pays(ctx, t)
    local c = V.cost(ctx.view, t)
    if c == nil then
        return false
    end
    for _, r in ipairs(V.resources) do
        local need = c.cost[r] or 0
        if need > 0 and ctx.view.stock[r] - (ctx.com[r] or 0) < need then
            return false
        end
    end
    return true
end

local function can_lay(ctx, t)
    local B = ctx.B
    local bad, drop = B.bad or {}, B.drop or {}
    if V.cost(ctx.view, t) == nil or (bad[t] or 0) > ctx.now or (drop[t] or 0) > ctx.now then
        return false
    end
    return pays(ctx, t) and P.refuses(ctx.view, ctx.memory, t) == nil
end

-- a tile it laid a building on lately (the census sees a foundation only once the engine placed it: a field's ring
-- would be ordered again every look meanwhile, and the engine refuses a taken tile)
local LAID_TILE_MS = 300000
local function recent_tile(ctx, x, y)
    return ((ctx.B.tiles or {})[key(x, y)] or 0) > ctx.now
end

-- LAID, NOT PAID: the price of what it laid whose foundation has not shown yet (BD_WAIT at most; a foundation that
-- starts is paid by its builder)
local function laid_account(view, B)
    local com = zero()
    for k = 1, 6 do
        local r = B.laid[k]
        if r ~= nil then
            local e = V.buildings(view, r.type)
            local started = e ~= nil and (e.unfinished or 0) > (r.open or 0)
            if started or view.time - r.t >= BD_WAIT then
                B.laid[k] = nil
            else
                add_cost(view, r.type, com)
            end
        end
    end
    B.com = com
    return com
end

-- for the integrator: the price of the foundations laid whose foundation has not shown yet, to add to the reserve
-- (priority.c's row "laid" would hold it from everybody; this module only leaves it out of its own spending)
function M.hold(view, memory)
    local B = memory.build
    if B == nil then
        return zero()
    end
    return laid_account(view, B)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The fields and the granary (build.c FIELDS, A GRANARY, A GRANARY BEFORE THE NEXT)

local function fields_due(C)
    local n = 0
    for _, g in ipairs(C.granaries) do
        for fx = -2, 2, 2 do
            for fy = -2, 2, 2 do
                if (fx ~= 0 or fy ~= 0) and not C.fields[key(g.x + fx, g.y + fy)]
                        and ai.can_place("b_farm", g.x + fx, g.y + fy) then
                    n = n + 1
                end
            end
        end
    end
    return n
end

local function lay_fields(ctx)
    if not S.build_fields or V.cost(ctx.view, "b_farm") == nil or ctx.C.open_fields >= 4 then
        return nil
    end
    local wood = V.cost(ctx.view, "b_farm").cost.wood or 0
    for _, g in ipairs(ctx.C.granaries) do
        for fx = -2, 2, 2 do
            for fy = -2, 2, 2 do
                if fx ~= 0 or fy ~= 0 then
                    local x, y = g.x + fx, g.y + fy
                    -- FARMS PAID FIRST: a field gets its builder (and its foundation) only while the wood in stock
                    -- pays it, one a field; nobody stands at an unstarted field without wood
                    if not ctx.C.fields[key(x, y)] and ai.can_place("b_farm", x, y) and not recent_tile(ctx, x, y)
                            and ctx.view.stock.wood - (ctx.com.wood or 0) >= wood and safe_ground(ctx, x, y) then
                        return "b_farm", x, y, 8, "a field of a granary"
                    end
                end
            end
        end
    end
    return nil
end

-- the food gatherers wanted that found no site (work.c food_short, as the port can read it)
local function food_short(ctx)
    local eco = ctx.memory.eco or {}
    local want = (eco.target or {}).food or 0
    return want > ctx.view.gatherers.food
end

local function lay_granary(ctx)
    local C, view, B = ctx.C, ctx.view, ctx.B
    if view.epoch < FARM_EPOCH or not S.more_granaries or V.cost(view, "b_granary") == nil then
        return nil
    end
    if C.open_kind[1] > 0 or C.open_fields > 0 or fields_due(C) > 0 or #C.granaries >= BD_GRANARIES
            or (#C.granaries > 0 and not food_short(ctx))
            or (B.laid[1] ~= nil and ctx.now - B.laid[1].t < BD_WAIT) or not can_lay(ctx, "b_granary") then
        return nil
    end
    -- A GRANARY BEFORE THE NEXT (garrison.c): one of its granaries has room inside - garrison is asked to fill it
    -- instead; asked for BD_GRAN_ASK without the room going, laid all the same
    if #C.granaries >= BD_GRAN_PLAIN and C.gran_room > 0 then
        if B.gran_ask_t == nil or C.gran_room ~= B.gran_room then
            B.gran_ask_t, B.gran_room = ctx.now, C.gran_room
        end
        if ctx.now - (B.gran_ask_t or ctx.now) < BD_GRAN_ASK then
            -- (upstream g_gu_gran: garrison.c is told to fill it; the port's tactics.garrison fills any building with
            -- room when it has spare citizens, so the ask is the note only)
            ctx.memory.eco = ctx.memory.eco or {}
            ctx.memory.eco.gu_gran = ctx.now
            return nil
        end
        B.gran_ask_t = nil
    end
    -- by a town of the best territory: backland first (1), then a front (2), then its own building's (3)
    for cost = 1, 3 do
        for _, c in ipairs(C.centres) do
            if c.cost == cost then
                local x, y = ai.find_site("b_granary", c.x, c.y, {min = 4, max = 12, safe = true})
                if x ~= nil then
                    local room = 0
                    for fx = -2, 2, 2 do
                        for fy = -2, 2, 2 do
                            if (fx ~= 0 or fy ~= 0) and ai.can_place("b_farm", x + fx, y + fy) then
                                room = room + 1
                            end
                        end
                    end
                    if room >= 6 then
                        return "b_granary", x, y, 1, "a granary for the fields"
                    end
                end
            end
        end
    end
    B.no_spot = (B.no_spot or 0) + 1
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- The research buildings and the first production building (build.c THE RESEARCH BUILDINGS, THE PAIR)

-- PRODUCTION's ask. produce.lua decides AND lays it (its own memory.pr and order): when it is wired this module leaves
-- production alone (upstream produce.c only asked build.c for the building). Without it, the port's stand-in: the
-- army's producer while its producers are busy (econ.lua's rule), else the first of produce.c's kinds it has nowhere.
-- src: "produce", "army" or "first" (the first one only is laid by the research loop)
local function production_ask(ctx)
    if ctx.memory.pr ~= nil then
        return nil, nil, "produce"
    end
    local plan = ctx.memory.army_plan
    if plan ~= nil and plan.producer ~= nil and plan.busy and V.cost(ctx.view, plan.producer) ~= nil then
        return plan.producer, nil, "army"
    end
    for _, t in ipairs(PROD_KINDS) do
        if V.cost(ctx.view, t) ~= nil and V.total(V.buildings(ctx.view, t)) == 0 then
            return t, nil, "first"
        end
    end
    return nil
end

local function lay_research(ctx)
    local C, view, B = ctx.C, ctx.view, ctx.B
    for k = 1, 4 do
        if k == 2 then
            -- THE FIRST PRODUCTION BUILDING, right after the University and saved for as a research building is (the
            -- player 2026-11-08: one production building until 2 farms)
            local t, at, src = production_ask(ctx)
            if t ~= nil and V.total(V.buildings(view, t)) == 0 and B.laid[5] == nil and C.open_kind[5] == 0 then
                if not can_lay(ctx, t) then
                    B.rs_want = t
                else
                    local x, y = at and at.x, at and at.y
                    if x == nil then
                        x, y = spot_production(ctx, t)
                    end
                    if x ~= nil then
                        return t, x, y, 5, "its first production building"
                    end
                    B.no_spot = (B.no_spot or 0) + 1
                end
            end
        else
            local j = k == 1 and 1 or (k - 1)
            if B.laid[4] == nil and C.rb[j] == 0 and V.cost(view, RESEARCH[j]) ~= nil then
                if not can_lay(ctx, RESEARCH[j]) then
                    B.rs_want = RESEARCH[j]
                else
                    -- THE PAIR: the first Hospital beside a Temple of its (whichever comes second), 3 to 5 tiles
                    local other = j == 2 and 3 or j == 3 and 2 or nil
                    local x, y = nil, nil
                    if other ~= nil and C.rb[other] > 0 and C.rb_at[other][1] ~= nil then
                        local o = C.rb_at[other][1]
                        x, y = ai.find_site(RESEARCH[j], o.x, o.y, {min = 3, max = BD_PAIR_R, safe = true})
                    end
                    if x == nil then
                        for cost = 1, 3 do
                            for _, c in ipairs(C.centres) do
                                if c.cost == cost then
                                    x, y = ai.find_site(RESEARCH[j], c.x, c.y, {min = 4, max = 10, safe = true})
                                    if x ~= nil then
                                        break
                                    end
                                end
                            end
                            if x ~= nil then
                                break
                            end
                        end
                    end
                    if x ~= nil then
                        local why = j == 3 and "a Temple" or j == 2 and "a Hospital" or "a University"
                        if other ~= nil and C.rb[other] > 0 then
                            why = why .. " beside its pair"
                        end
                        return RESEARCH[j], x, y, 4, why
                    end
                    B.no_spot = (B.no_spot or 0) + 1
                end
            end
        end
    end
    return nil
end

-- a production building's spot: packed against its kind (packing.c, tight_build) or the engine's search 3 to 10
-- tiles from a town (build.c PRODUCTION)
spot_production = function(ctx, t)
    local C, view = ctx.C, ctx.view
    if S.tight_build and PK.is_type(t) and view.time - ((ctx.B.pack_t or {})[t] or -100000) >= 10000 then
        ctx.B.pack_t = ctx.B.pack_t or {}
        ctx.B.pack_t[t] = view.time
        local x, y = PK.spot(view, ctx.B, t)
        if x ~= nil then
            return x, y
        end
    end
    for cost = 1, 3 do
        for _, c in ipairs(C.centres) do
            if c.cost == cost then
                local x, y = ai.find_site(t, c.x, c.y, {min = 3, max = 10, safe = true})
                if x ~= nil then
                    return x, y
                end
            end
        end
    end
    return nil
end

local function lay_production(ctx)
    local C, view, B = ctx.C, ctx.view, ctx.B
    local t, at, src = production_ask(ctx)
    if t == nil or src == "first" or V.total(V.buildings(view, t)) == 0 then
        return nil   -- (the very first one is the research loop's)
    end
    local most = 2 + view.citizens // 15   -- the port's cap of the stand-in (econ.lua's); produce.c's ask is its own
    if V.total(V.buildings(view, t)) >= most or C.open_kind[5] > 0
            or (B.laid[5] ~= nil and ctx.now - B.laid[5].t < BD_WAIT) or not can_lay(ctx, t) then
        return nil
    end
    local x, y = at and at.x, at and at.y
    if x == nil then
        x, y = spot_production(ctx, t)
    end
    if x == nil then
        B.no_spot = (B.no_spot or 0) + 1
        return nil
    end
    return t, x, y, 5, "a production building"
end

-- ---------------------------------------------------------------------------------------------------------------
-- The mines: their Settlements and their towers (build.c A MINE'S SETTLEMENT, A MINE'S TOWER)

local function mine_cands(ctx)
    local out = {settle = nil, tower = nil}
    local view, base_x, base_y = ctx.view, ctx.view.town_x, ctx.view.town_y
    local sbest, tbest = 1e18, 1e18
    for _, s in ipairs(view.sites) do
        local r = s.resource
        if (r == "stone" or r == "gold" or r == "iron") and s.units > 0 and s.x ~= nil and s.x >= 0 then
            local cost = ctx.tv.cost(s.x, s.y)
            if cost > 0 and safe_ground(ctx, s.x, s.y) then
                local metal = r == "gold" or r == "iron"
                local across = not ai.reach(base_x, base_y, s.x, s.y)
                local has_drop = near_any(ctx.C.drops, s.x, s.y, metal and BD_DROP_METAL or BD_DROP)
                local guard = near_box(ctx.C.defs, s.x, s.y, BD_GUARD)
                local score = cost * 1000000 + d2(s.x, s.y, base_x, base_y)
                if across and not guard then
                    -- TOWER FIRST across the water: its mine gets its tower before its Settlement
                    if score < tbest then
                        tbest, out.tower = score, {x = s.x, y = s.y}
                    end
                elseif not has_drop then
                    if score < sbest then
                        sbest, out.settle = score, {x = s.x, y = s.y, metal = metal}
                    end
                elseif not guard and score < tbest then
                    tbest, out.tower = score, {x = s.x, y = s.y}
                end
            end
        end
    end
    return out
end

local function lay_mine(ctx)
    local C, view, B = ctx.C, ctx.view, ctx.B
    if V.cost(view, "b_settlement") == nil then
        return nil
    end
    local c = mine_cands(ctx)
    if c.settle ~= nil and C.open_kind[2] == 0 and (B.laid[2] == nil or ctx.now - B.laid[2].t >= BD_WAIT)
            and can_lay(ctx, "b_settlement") then
        local reach = c.settle.metal and BD_DROP_METAL or 6
        local x, y = ai.find_site("b_settlement", c.settle.x, c.settle.y, {min = 2, max = reach, safe = true})
        if x ~= nil then
            return "b_settlement", x, y, 2, "a Settlement by a mine"
        end
        B.no_spot = (B.no_spot or 0) + 1
    end
    local tower = tower_class(view)
    if c.tower ~= nil and tower ~= nil and C.open_kind[3] == 0 and (B.laid[3] == nil or ctx.now - B.laid[3].t >= BD_WAIT)
            and can_lay(ctx, tower) then
        local x, y = ai.find_site(tower, c.tower.x, c.tower.y, {min = 2, max = 5, safe = true})
        if x ~= nil then
            return tower, x, y, 3, "a tower by a mine"
        end
        B.no_spot = (B.no_spot or 0) + 1
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- The herds, the trees across the water and the fish (build.c A HERD'S SETTLEMENT, TO THE TREES ACROSS, A FISHING DOCK)

local function herd_keep()
    local k = S.herd_keep
    if k == true then
        return 1
    elseif k == false or type(k) ~= "number" or k < 0 then
        return nil
    end
    return k
end

local function lay_herd(ctx)
    local view, B = ctx.view, ctx.B
    local keep = herd_keep()
    if keep == nil or view.epoch >= FARM_EPOCH or not food_short(ctx) or V.cost(view, "b_settlement") == nil
            or ctx.C.open_kind[2] > 0 or (B.herd_t ~= nil and ctx.now - B.herd_t < BD_HERD_EVERY) then
        return nil
    end
    local reach = S.food_hunt_range or 15
    local ids = ai.resources{kind = "food", near = {view.town_x, view.town_y, 120}, limit = 32}
    if #ids == 0 then
        return nil
    end
    local cols = ai.get(ids, {"type", "x", "y", "amount"})
    local animals = {}
    for k = 1, #ids do
        local c = cols.type[k] ~= nil and ai.class(cols.type[k]) or nil
        if c ~= nil and c.family == FAMILY_ANIMAL and (c.attack or 0) == 0 and (cols.amount[k] or 0) > 0
                and cols.x[k] ~= nil then
            animals[#animals + 1] = {x = cols.x[k], y = cols.y[k]}
        end
    end
    local best, best_score = nil, 1e18
    for _, a in ipairs(animals) do
        -- (upstream counts the herd's grown animals against herd_keep; the port counts the animals within 8 tiles, the
        -- herd's members, and wants more than it leaves)
        local n = 0
        for _, b in ipairs(animals) do
            if d2(a.x, a.y, b.x, b.y) <= 64 then
                n = n + 1
            end
        end
        local cost = ctx.tv.cost(a.x, a.y)
        local score = cost * 1000000 + d2(a.x, a.y, view.town_x, view.town_y)
        if n > keep and cost > 0 and score < best_score and not near_any(ctx.C.drops, a.x, a.y, reach)
                and safe_ground(ctx, a.x, a.y) then
            best, best_score = a, score
        end
    end
    if best == nil or not can_lay(ctx, "b_settlement")
            or (B.laid[2] ~= nil and ctx.now - B.laid[2].t < BD_WAIT) then
        return nil
    end
    local x, y = ai.find_site("b_settlement", best.x, best.y, {min = 2, max = 6, safe = true})
    if x == nil then
        B.no_spot = (B.no_spot or 0) + 1
        return nil
    end
    return "b_settlement", x, y, 2, "a Settlement by a herd"
end

local function lay_forest(ctx)
    local view, B = ctx.view, ctx.B
    if not S.forest_across or not S.expand_across or view.epoch < FARM_EPOCH
            or (view.idle or 0) < BD_FOREST_IDLE or V.cost(view, "b_settlement") == nil or ctx.C.open_kind[2] > 0
            or (B.forest_t ~= nil and ctx.now - B.forest_t < BD_FOREST_EVERY)
            or (B.laid[2] ~= nil and ctx.now - B.laid[2].t < BD_WAIT) then
        return nil
    end
    -- (work.c TO THE TREES ACROSS: the forest edge the engine picked on a land with no drop site of its; view.wood is
    -- that edge and has_drop its test)
    local w = view.wood
    if w == nil or w.ready or w.has_drop or w.x == nil or not safe_ground(ctx, w.x, w.y)
            or not can_lay(ctx, "b_settlement") then
        return nil
    end
    B.forest_t = ctx.now
    local x, y = ai.find_site("b_settlement", w.x, w.y, {min = 2, max = 10, safe = true})
    if x == nil then
        B.no_spot = (B.no_spot or 0) + 1
        B.forest_nospot = (B.forest_nospot or 0) + 1
        return nil
    end
    return "b_settlement", x, y, 2, "a Settlement by trees across the water"
end

local function lay_fish(ctx)
    local view, B = ctx.view, ctx.B
    if not S.sea_fishing or not food_short(ctx) or V.cost(view, "b_dock") == nil
            or (B.fdock_t ~= nil and ctx.now - B.fdock_t < BD_FISH_EVERY) then
        return nil
    end
    if (ctx.C.total.b_dock or 0) >= BD_FISH_DOCKS or #ctx.C.drops == 0 then
        return nil
    end
    local ids = ai.resources{kind = "food", limit = 32}
    if #ids == 0 then
        return nil
    end
    local cols = ai.get(ids, {"type", "x", "y", "amount"})
    local best, bd = nil, BD_FISH_NEAR * BD_FISH_NEAR + 1
    for k = 1, #ids do
        local c = cols.type[k] ~= nil and ai.class(cols.type[k]) or nil
        local x, y = cols.x[k], cols.y[k]
        if c ~= nil and c.family == FAMILY_FISH and x ~= nil and (cols.amount[k] or 0) > 0 then
            local dmin, drop = 1e18, nil
            for _, d in ipairs(ctx.C.drops) do
                local dd = d2(d.x, d.y, x, y)
                if dd < dmin then
                    dmin, drop = dd, d
                end
            end
            local docked = false
            for _, id in ipairs(ai.units{type = "b_dock", near = {x, y, BD_FISH_R}, limit = 1}) do
                docked = id ~= nil
            end
            if drop ~= nil and dmin < bd and not docked and not raiders(ctx, x, y, 12) and ai.danger(x, y) == 0 then
                bd, best = dmin, {x = x, y = y, drop = drop}
            end
        end
    end
    if best == nil or not can_lay(ctx, "b_dock") then
        return nil
    end
    -- a dock on the shore beside the fish, on that drop site's land (the game's placement says it laid a dock it
    -- didn't: upstream lays it as its own; here ai.shore finds the shore and ai.reach holds the land)
    local x, y = ai.shore(best.x, best.y, {max = 12})
    if x == nil or not ai.reach(best.drop.x, best.drop.y, x, y) or not ai.can_place("b_dock", x, y)
            or not safe_ground(ctx, x, y) then
        B.no_spot = (B.no_spot or 0) + 1
        return nil
    end
    B.fdock_t = ctx.now
    return "b_dock", x, y, 9, "a fishing dock by the fish"
end

-- ---------------------------------------------------------------------------------------------------------------
-- A CLAIM (build.c A CLAIM, CLAIMS GOING UP, THE CLAIM'S TURN, THE CLEARED GROUND)

local function mine_territories(ctx)
    if ctx.mine_r ~= nil then
        return ctx.mine_r
    end
    local set = {}
    for _, s in ipairs(ctx.view.sites) do
        local r = s.resource
        if (r == "stone" or r == "gold" or r == "iron") and s.x ~= nil and s.x >= 0 then
            local t = ctx.tv.at(s.x, s.y)
            if t ~= nil then
                set[t.i] = true
            end
        end
    end
    ctx.mine_r = set
    return set
end

-- HOLDING THE CLEARED GROUND: a push of its holds the territory it took (stations.c sn_holding). stations.lua is not
-- ported: when it is, it writes memory.stations.list = { {i, x, y, held = true}, ... }
local function held_ground(ctx)
    local st = ctx.memory.stations
    if st == nil or type(st.list) ~= "table" then
        return nil
    end
    for _, r in ipairs(st.list) do
        if r.held and r.x ~= nil then
            return r
        end
    end
    return nil
end

local function claim_lay(ctx)
    local view, B, C, tv = ctx.view, ctx.B, ctx.C, ctx.tv
    local tower = tower_class(view)
    local ctype = nil
    if tower ~= nil and can_lay(ctx, tower) then
        ctype = tower
    elseif V.cost(view, "b_barracks") ~= nil and can_lay(ctx, "b_barracks") then
        ctype = "b_barracks"   -- a Barracks claims it instead: it holds and costs wood
    end
    -- THE CLEARED GROUND first and at once: its soldiers stand there for the tower (no wait for the last claim)
    local held = held_ground(ctx)
    if ctype ~= nil and held ~= nil then
        local x, y = ai.find_site(ctype, held.x, held.y, {min = 0, max = 12, safe = true})
        if x ~= nil then
            B.claim_t = ctx.now
            B.claim_open = B.claim_open or {}
            B.claim_open[#B.claim_open + 1] = {r = held.i, t = ctx.now}
            if #B.claim_open > 8 then
                table.remove(B.claim_open, 1)
            end
            return ctype, x, y, ctype == tower and 3 or 5, "a claim on the ground its push cleared"
        end
    end
-- a claim's Settlement beside its tower (a lone tower: the one building of its there, and it holds) - before the next
    local lone = nil
    for _, r in ipairs(tv.list) do
        -- (a defence of its: the claim's tower; a home territory with a single Town Center is no claim's tower)
        if not r.water and r.role ~= "none" and r.state == "quiet" and r.bld == 1 and r.hold >= 1 and r.def >= 1 then
            lone = r
            break
        end
    end
    if lone ~= nil and B.claim_t ~= nil and ctx.now - B.claim_t >= BD_CLAIM_LONE then
        lone = nil   -- (one that found no spot holds them all: BD_CLAIM_LONE at most)
    end
    if lone ~= nil and B.laid[2] == nil and C.open_kind[2] == 0 and V.cost(view, "b_settlement") ~= nil
            and can_lay(ctx, "b_settlement") then
        local cx, cy = lone.x, lone.y
        for _, d in ipairs(C.defs) do
            local r = tv.at(d.x, d.y)
            if r ~= nil and r.i == lone.i then
                cx, cy = d.x, d.y
            end
        end
        if not raiders(ctx, cx, cy, 12) and ai.danger(cx, cy) == 0 then
            local x, y = ai.find_site("b_settlement", cx, cy, {min = 2, max = 8, safe = true})
            if x ~= nil then
                return "b_settlement", x, y, 2, "a claim's Settlement beside its tower"
            end
        end
    end
    if ctype == nil then
        B.claim_why = "no tower or Barracks to build or pay"
        return nil
    end
    if view.citizens < (S.min_workforce or 40) then
        B.claim_why = "too few citizens"
        return nil
    end
    if lone ~= nil then
        B.claim_why = "a claim's Settlement still missing"
        return nil
    end
    -- CLAIMS GOING UP: its own claims laid within BD_CLAIM_LONE whose territory it does not hold yet
    local going = 0
    for _, r in ipairs(B.claim_open or {}) do
        local t = tv.list[r.r]
        if t ~= nil and t.role == "none" and ctx.now - r.t < BD_CLAIM_LONE then
            going = going + 1
        end
    end
    if going >= BD_CLAIM_OPEN then
        B.claim_why = "claims still going up"
        return nil
    end
    if B.claim_t ~= nil and ctx.now - B.claim_t < BD_CLAIM_EVERY then
        B.claim_why = "the last one too recent"
        return nil
    end
    -- a territory nobody has a building in, beside one it holds with BD_CLAIM_GUARD of its soldiers there (a tower of
    -- its with claim_from_towers); the nearest to its base, a territory with a mine counted BD_CLAIM_MINE times nearer
    local mine_r = mine_territories(ctx)
    local best, best_d = nil, 1e18
    for _, t in ipairs(tv.list) do
        if not t.water and t.role == "none" and t.hold == 0 and t.ebld == 0 and t.abld == 0 and t.state == "quiet" then
            -- ("the border is secured first": the territories beside it, the module's neighbours when it has them)
            local nbs = tv.list
            if tv.neighbours ~= nil then
                nbs = {}
                for _, nb in ipairs(tv.neighbours(t.i) or {}) do
                    local o = tv.list[nb.o or nb]
                    if o ~= nil then
                        nbs[#nbs + 1] = o
                    end
                end
            end
            local guarded = false
            for _, o in ipairs(nbs) do
                if o.i ~= t.i and not o.water and o.role ~= "none" and o.state ~= "unsafe"
                        and (o.sold >= BD_CLAIM_GUARD or (S.claim_from_towers and o.def > 0)) then
                    guarded = true
                    break
                end
            end
            if guarded and not raiders(ctx, t.x, t.y, 12) and ai.danger(t.x, t.y) == 0 then
                local d = d2(t.x, t.y, view.town_x, view.town_y)
                if mine_r[t.i] then
                    d = d // (BD_CLAIM_MINE * BD_CLAIM_MINE)
                end
                if d < best_d then
                    best, best_d = t, d
                end
            end
        end
    end
    if best == nil then
        B.claim_why = "no territory to claim"
        return nil
    end
    local x, y = ai.find_site(ctype, best.x, best.y, {min = 0, max = ctype == tower and 8 or 12, safe = true})
    if x == nil then
        B.no_spot = (B.no_spot or 0) + 1
        B.claim_why = "no spot for the claim"
        return nil
    end
    B.claim_t = ctx.now
    B.claim_open = B.claim_open or {}
    B.claim_open[#B.claim_open + 1] = {r = best.i, t = ctx.now}
    if #B.claim_open > 8 then
        table.remove(B.claim_open, 1)
    end
    return ctype, x, y, ctype == tower and 3 or 5,
        ctype == tower and "a claim: a tower in a territory nobody holds"
            or "a claim: a Barracks in a territory nobody holds (no tower to build or pay)"
end

-- A CLAIM on its own: the integrator may wire it while the drop sites stay econ.lua's (do not call it in the same
-- step as build.step: both would lay)
function M.claim(view, memory, orders, budget)
    if not (S.build_by_territory or false) or ai == nil or view.town_x == nil or view.town_x < 0 then
        return nil
    end
    memory.build = memory.build or {t = 0, pending = {}}
    memory.build.pending = memory.build.pending or {}
    memory.build.laid = memory.build.laid or {}
    local ctx = make_ctx(view, memory, budget)
    results(ctx)
    local t, x, y, kind, why = claim_lay(ctx)
    if t == nil then
        return nil
    end
    return lay(ctx, orders, t, x, y, kind, why)
end

-- ---------------------------------------------------------------------------------------------------------------
-- A WATCH TOWER, THE MORALE'S HOUSES, the sinks (build.c)

local function lay_watch(ctx)
    local view, B, C, tv = ctx.view, ctx.B, ctx.C, ctx.tv
    if not S.watch_towers or (B.watch_t ~= nil and ctx.now - B.watch_t < BD_WATCH_EVERY)
            or view.citizens < (S.min_workforce or 40) then
        return nil
    end
    local tower = tower_class(view)
    if tower == nil or not can_lay(ctx, tower) then
        return nil
    end
    for _, r in ipairs(tv.list) do
        if not r.water and r.role ~= "none" and r.state ~= "unsafe" then
            -- (watch.c's DIM: the place longest out of sight; a held territory whose middle no defence of its watches
            -- stands in for the port's read of the fog - watch.lua, not ported, owns the real choice)
            if not near_any(C.defs, r.x, r.y, BD_WATCH_NEAR) and not raiders(ctx, r.x, r.y, 12)
                    and ai.danger(r.x, r.y) == 0 then
                local x, y = ai.find_site(tower, r.x, r.y, {min = 0, max = 4, safe = true})
                if x ~= nil then
                    B.watch_t = ctx.now
                    return tower, x, y, 3, "a watch tower over ground of its it rarely sees"
                end
            end
        end
    end
    return nil
end

local function lay_house(ctx)
    local view, B, C, tv = ctx.view, ctx.B, ctx.C, ctx.tv
    if #C.centres == 0 or V.cost(view, "b_house") == nil then
        return nil
    end
    if (C.total.b_house or 0) > (C.n.b_house or 0) then
        return nil   -- one going up at a time
    end
    if B.house_t ~= nil and ctx.now - B.house_t < BD_HOUSE_EVERY then
        return nil
    end
    if view.citizens < (S.min_workforce or 40) or not can_lay(ctx, "b_house") then
        return nil
    end
    local houses = {}
    for _, h in ipairs(C.houses) do
        houses[#houses + 1] = h
    end
    if B.house_at ~= nil and ctx.now - (B.house_t or 0) < BD_HOUSE_PEND then
        houses[#houses + 1] = B.house_at   -- the last one laid may not show yet: it counts
    end
    local bt, bs = nil, -1
    for _, t in ipairs(C.centres) do
        local have = 0
        for _, h in ipairs(houses) do
            if math.abs(h.x - t.x) <= BD_HOUSE_REACH and math.abs(h.y - t.y) <= BD_HOUSE_REACH then
                have = have + 1
            end
        end
        local r = tv.at(t.x, t.y)
        if have < t.max and tv.cost(t.x, t.y) > 0 and not raiders(ctx, t.x, t.y, 12)
                and ai.danger(t.x, t.y) == 0 then
            local sc = ((r ~= nil and r.role == "front") and 100 or 0) + (t.max - have)
            if sc > bs then
                bt, bs = t, sc
            end
        end
    end
    if bt == nil then
        return nil
    end
    B.house_t = ctx.now
    local x, y = ai.find_site("b_house", bt.x, bt.y, {min = 3, max = BD_HOUSE_REACH - 1, safe = true})
    if x == nil or (B.house_at ~= nil and math.abs(x - B.house_at.x) <= 3 and math.abs(y - B.house_at.y) <= 3) then
        B.no_spot = (B.no_spot or 0) + 1
        return nil
    end
    B.house_at = {x = x, y = y}
    return "b_house", x, y, 7, "a House for the morale of its Town Center or Capitol"
end

local function sink_kinds()
    local kinds = {"b_hospital", "b_temple", "b_university", "b_airport"}
    for _, t in ipairs(PROD_KINDS) do
        kinds[#kinds + 1] = t
    end
    return kinds
end

local function lay_sink(ctx)
    local view, B, tv = ctx.view, ctx.B, ctx.tv
    local wsink = (view.stock.wood or 0) >= BD_SINK
    local ssink = S.stone_sink and (view.stock.stone or 0) >= BD_STONE_SINK
    if not (wsink or ssink) or (B.sink_t ~= nil and ctx.now - B.sink_t < BD_SINK_EVERY) then
        return nil
    end
    local ids = ai.units{category = "building"}
    local open = 0
    if #ids > 0 then
        local cols = ai.get(ids, {"built"})
        for k = 1, #ids do
            if cols.built[k] == false then
                open = open + 1
            end
        end
    end
    if open >= BD_SINK_OPEN then
        return nil
    end
    -- AREA OF EFFECT: its middles of the kinds that overlap nothing
    local aoe = {}
    for _, t in ipairs({"b_hospital", "b_temple", "b_university"}) do
        aoe[t] = {}
        for _, id in ipairs(ai.units{type = t}) do
            local u = ai.unit(id)
            if u ~= nil and u.x ~= nil then
                aoe[t][#aoe[t] + 1] = {x = u.x, y = u.y}
            end
        end
    end
    for back = 0, 1 do
        for _, r in ipairs(tv.list) do
            if not r.water and r.state == "quiet" and r.role == (back == 1 and "back" or "front") then
                for _, t in ipairs(sink_kinds()) do
                    local c = V.cost(view, t)
                    if c ~= nil and (back == 0 or t ~= "b_airport")
                            and (ctx.C.total[t] or 0) < (t == "b_airport" and 1 or BD_SINK_MAX)
                            and (wsink or (AOE_REACH[t] ~= nil and (c.cost.stone or 0) > 0))
                            and not (AOE_REACH[t] ~= nil and near_any(aoe[t] or {}, r.x, r.y, AOE_REACH[t]))
                            and can_lay(ctx, t) then
                        if not raiders(ctx, r.x, r.y, 12) and ai.danger(r.x, r.y) == 0 then
                            local x, y = ai.find_site(t, r.x, r.y, {min = 3, max = 14, safe = true})
                            if x ~= nil then
                                local reach = AOE_REACH[t]
                                if reach ~= nil and near_any(aoe[t] or {}, x, y, reach) then
                                    B.sink_t = ctx.now   -- too near its own: the next look in BD_SINK_EVERY
                                    return nil
                                end
                                B.sink_t = ctx.now
                                -- (no LAID, NOT PAID slot: upstream's sink books none - it comes last in the look)
                                return t, x, y, nil,
                                    (wsink and "the wood sink: one more in a " or "the stone sink: one more in a ")
                                    .. (back == 1 and "backland" or "front territory")
                            end
                        end
                        B.no_spot = (B.no_spot or 0) + 1
                    end
                end
            end
        end
    end
    return nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- HOLDING THE GROUND (build.c): its idle citizens where the enemy is near lay a tower, else a production building,
-- and build it - anything better than waiting to be cleared off

local function lay_hold(ctx)
    local view, B, C, tv = ctx.view, ctx.B, ctx.C, ctx.tv
    if not S.hold_ground or (B.hold_t ~= nil and ctx.now - B.hold_t < BD_HOLD_EVERY) then
        return
    end
    local idle = ai.units{category = "citizen", idle = true, held = "engine"}
    if #idle == 0 then
        return
    end
    local cols = ai.get(idle, {"x", "y"})
    local hot, count, nl, sx, sy = {}, {}, 0, 0, 0
    for k = 1, #idle do
        local x, y = cols.x[k], cols.y[k]
        if x ~= nil then
            local r = tv.at(x, y)
            local near_foe = raiders(ctx, x, y, BD_HOLD_R)
                or (r ~= nil and not r.water and (r.ebld > 0 or r.state == "foes"))
            local t = ai.tile(x, y)
            local c = t ~= nil and (t.continent or 0) or 0
            if near_foe and c ~= 0 then
                hot[#hot + 1] = {x = x, y = y, c = c}
                count[c] = (count[c] or 0) + 1
            end
        end
    end
    local bn, land = 0, 0
    for c, n in pairs(count) do
        if n > bn then
            bn, land = n, c
        end
    end
    if bn == 0 then
        return
    end
    for _, h in ipairs(hot) do
        if h.c == land then
            sx, sy, nl = sx + h.x, sy + h.y, nl + 1
        end
    end
    if nl == 0 then
        return
    end
    local mx, my = sx // nl, sy // nl
    local tower = tower_class(view)
    local type, why = nil, nil
    if tower ~= nil and not near_any(C.defs, mx, my, BD_HOLD_TOWER) and can_lay(ctx, tower) then
        type, why = tower, "a tower"
    elseif near_any(C.defs, mx, my, BD_HOLD_TOWER) and not near_any(C.prodp, mx, my, BD_HOLD_PROD) then
        for _, t in ipairs(PROD_KINDS) do
            if V.cost(view, t) ~= nil and can_lay(ctx, t) then
                type, why = t, "a production building"
                break
            end
        end
    end
    local jx, jy = nil, nil
    if type ~= nil then
        for d = 2, 10 do
            local x, y = ai.find_site(type, mx, my, {min = d, max = d, safe = true})
            if x ~= nil then
                local t = ai.tile(x, y)
                if t ~= nil and (t.continent or 0) == land and ai.danger(x, y) == 0 then
                    jx, jy = x, y
                    break
                end
            end
        end
    end
    if jx ~= nil and not recent_tile(ctx, jx, jy) then
        -- laid as its own (ai.build): the citizens at it are the script's until it stands
        local b = {}
        for _, id in ipairs(idle) do
            if #b < BD_HOLD_N then
                b[#b + 1] = id
            end
        end
        B.hold_t = ctx.now
        local code, _, _, id = ai.build(b, type, jx, jy)
        ai.release(b)
        if code == "ok" and id ~= nil then
            B.tiles = B.tiles or {}
            B.tiles[key(jx, jy)] = ctx.now + LAID_TILE_MS
            B.holds = (B.holds or 0) + 1
            print(string.format("build hold_ground %s at=%d,%d idle=%d land=%d", type, jx, jy, nl, land))
        end
        return
    end
    -- nothing to lay: the nearest unfinished building of its on that land, out of the enemy towers' reach
    local best, bd = nil, BD_HOLD_PROD * BD_HOLD_PROD + 1
    local ids = ai.units{category = "building"}
    if #ids > 0 then
        local bcols = ai.get(ids, {"x", "y", "built"})
        for k = 1, #ids do
            local x, y = bcols.x[k], bcols.y[k]
            if x ~= nil and bcols.built[k] == false then
                local t = ai.tile(x, y)
                local d = d2(x, y, mx, my)
                if t ~= nil and (t.continent or 0) == land and d < bd and ai.danger(x, y) == 0 then
                    best, bd = {x = x, y = y}, d
                end
            end
        end
    end
    if best ~= nil then
        B.hold_t = ctx.now
        print(string.format("build hold_ground unfinished at=%d,%d idle=%d land=%d", best.x, best.y, nl, land))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look itself

local function choose(ctx)
    local t, x, y, kind, why
    -- HOLDING THE GROUND lays on its own (ai.build, its own citizens, every look as upstream; the look's one building
    -- follows)
    lay_hold(ctx)
    t, x, y, kind, why = lay_fields(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_granary(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    ctx.B.rs_want = nil
    t, x, y, kind, why = lay_research(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_production(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_mine(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_herd(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_forest(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_fish(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = claim_lay(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_watch(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    t, x, y, kind, why = lay_house(ctx)
    if t ~= nil then
        return t, x, y, kind, why
    end
    return lay_sink(ctx)
end

make_ctx = function(view, memory, budget)
    local B = memory.build
    B.laid = B.laid or {}
    B.pending = B.pending or {}
    B.bad = B.bad or {}
    B.drop = B.drop or {}
    B.tries = B.tries or {}
    B.tiles = B.tiles or {}
    local tv = M.tv(view, memory)
    local C = census(view, memory, tv)
    local com = laid_account(view, B)
    -- (budget is the economy's accounts, kept for the integrator and for produce.lua's ask; the priority list's
    -- savings are asked per type through P.refuses, as upstream's placement asked prio_refuses)
    return {view = view, memory = memory, B = B, tv = tv, C = C, com = com, now = view.time, budget = budget}
end

-- the lay through the verb layer (the engine's search from the tile it chose), recorded for LAID, NOT PAID and for
-- the order's result
lay = function(ctx, orders, t, x, y, kind, why)
    local B = ctx.B
    if recent_tile(ctx, x, y) then
        return nil
    end
    B.tiles = B.tiles or {}
    B.tiles[key(x, y)] = ctx.now + LAID_TILE_MS
    for k in pairs(B.tiles) do
        if B.tiles[k] <= ctx.now then
            B.tiles[k] = nil
        end
    end
    local at = #orders + 1
    orders[at] = {verb = "build", type = t, near = "tile", x = x, y = y}
    B.pending[#B.pending + 1] = {at = at, type = t, t = ctx.now}
    if kind ~= nil and kind <= 6 then
        local e = V.buildings(ctx.view, t)
        B.laid[kind] = {t = ctx.now, type = t, open = e ~= nil and (e.unfinished or 0) or 0}
    end
    B.want = {type = t, x = x, y = y, kind = kind, why = why, t = ctx.now}
    print(string.format("build %s at=%d,%d why=%s t=%d", t, x, y, tostring(why), ctx.now // 1000))
    return B.want
end

-- the results of the last look's orders: a building the placement refused is dropped after its tries (build.c: the
-- buildings it cannot place; econ.c retry_dropped for when it comes back)
results = function(ctx)
    local B = ctx.B
    local keep = {}
    for _, p in ipairs(B.pending or {}) do
        local r = ctx.view.results[p.at]
        if r == nil then
            keep[#keep + 1] = p   -- (the verb layer answers later: its queue)
        elseif r.code == "refused" and (r.reason == "no_site" or r.reason == "unsafe" or r.reason == "cannot_place") then
            B.bad[p.type] = ctx.now + BD_NO_SITE_MS
            B.tries = B.tries or {}
            B.tries[p.type] = (B.tries[p.type] or 0) + 1
            if B.tries[p.type] >= BD_DROP_TRIES then
                B.drop = B.drop or {}
                B.drop[p.type] = ctx.now + BD_DROP_MS
                B.tries[p.type] = 0
                print(string.format("build dropped %s (no place after %d tries)", p.type, BD_DROP_TRIES))
            end
        elseif r.code == "refused" and (r.reason == "not_in_epoch" or r.reason == "unknown_class") then
            B.drop = B.drop or {}
            B.drop[p.type] = ctx.now + BD_DROP_MS
        end
    end
    B.pending = keep
end

-- the step: one building a look, every BD_EVERY. Returns the last look's want ({type, x, y, kind, why, t}) or nil.
function M.step(view, memory, orders, budget)
    if not (S.build_by_territory or false) or ai == nil or view.town_x == nil or view.town_x < 0 then
        return nil
    end
    memory.build = memory.build or {t = 0, pending = {}}
    local B = memory.build
    B.pending = B.pending or {}
    B.laid = B.laid or {}
    if B.t ~= 0 and view.time - B.t < BD_EVERY then
        return B.want
    end
    B.t = view.time
    local ctx = make_ctx(view, memory, budget)
    results(ctx)
    local t, x, y, kind, why = choose(ctx)
    if t == nil then
        B.want = nil
        if view.time - (B.log_t or -100000) >= 120000 then
            B.log_t = view.time
            print(string.format("build none t=%d open=%d lost=%d claims=%d why=%s", view.time // 1000, ctx.C.open_n,
                ctx.C.lost, B.claims or 0, tostring(B.claim_why)))
        end
        return nil
    end
    if kind == 1 then
        B.granaries_n = (B.granaries_n or 0) + 1
    elseif kind == 2 then
        B.settlements = (B.settlements or 0) + 1
    elseif kind == 3 then
        B.towers = (B.towers or 0) + 1
    elseif kind == 4 then
        B.research_n = (B.research_n or 0) + 1
    elseif kind == 5 then
        B.production_n = (B.production_n or 0) + 1
    elseif kind == 8 then
        B.fields_n = (B.fields_n or 0) + 1
    end
    if why ~= nil and string.find(why, "claim", 1, true) ~= nil and string.find(why, "Settlement", 1, true) == nil then
        B.claims = (B.claims or 0) + 1
    end
    return lay(ctx, orders, t, x, y, kind, why)
end

-- the last look's choice (for the integrator's trace): {type, x, y, kind, why, t} or nil
function M.want(view, memory)
    local B = memory.build
    return B ~= nil and B.want or nil
end

return M
