-- Age of AI port, the navy (upstream dll/ai/navy.c the water map, the account, docks, the fishing boat, the fleet's
-- size and mix; dll/ai/landing.c the landing; dll/ai/fleet.c in ageofai.fleet. UPSTREAM has the map and the licence:
-- GPL-3.0, Grille and the Age of AI contributors). require("ageofai.navy"). On lib.navy (docs/AI_LUA_UNITS.md L6).
--
-- WATER MAP (navy_detect): the enemies with buildings known - across the water when none stands on the home
-- continent; the strength = the share of enemies across (1 islands, 0 land, between: mixed). Upstream reads every
-- enemy's buildings off the map once, 10 s in; this port is fog-honest, so it decides on the buildings it knows
-- and looks again every minute (until an enemy is known: a water map counts as islands after 3 minutes, so the sea
-- scout gets its boat).
-- NAVY ACCOUNT ([ai_base] navy, navy_share): navy_share % x the strength of every wood, gold and iron income goes
-- into the account (600 of each at most); below the fleet's target the share grows toward navy_share_max with what
-- is missing. Every other order sees the account as reserve (econ.lua M.reserve adds mem.navy_hold) - once a
-- warship the mix wanted could not be paid (within 2 minutes), that ship's whole cost. A warship takes any of its
-- resources up to the whole account (one pool: what it took beyond its own part is charged to the others, the
-- largest first). Fishing boats and a landing's transports are economy: the stock above the reserve pays them.
-- THE FLEET'S SIZE (stage 4): navy_fleet_pop % of the population cap x the strength; every 10 s while the warships
-- afloat and those ordered in the last 90 s are below it (a finished dock, population room) one warship of the type
-- the mix wants (navy_pick: the role furthest below its share - F 2 : L 1 : A 0.5, S 1 in place of A once a submarine
-- is available; + 0.5 F per enemy galley or submarine seen, + 0.25 L per frigate, + 0.75 A/S per battleship; AA
-- only with aircraft seen; L scaled by the non-submarine share of the enemy fleet and by 4 / (4 + coastal siege) -
-- payable now from the stock and the account, else the next role below its share that is; in the role the type
-- with the best factor x attack / (reload + 2 s) x hit points / cost^2). The account pays it (ai.train).
-- DOCKS (navy_docks, once a minute): 2, on islands 3 while a warship waits for the account or 12+ fishing boats share
-- them; with the fleet's target 2 + target / 8 (at most 5); one more when none is unfinished or queued (the fish
-- verb's dock by the fish). FISHING BOAT (fishing_boat): the newest fishing boat the dock can make, trained one at a
-- time (the game's pick here is by epoch: in the Atomic Age it asks for one it cannot build). The count is this
-- engine's coast rule (settings: fishing_boats_per_fish, fishing_boats_max): upstream leaves it to the game's field
-- passes.
-- LANDING (landing.c): every 30 s while idle, with an enemy known across the water, a finished dock and the force
-- at home: the weakest beach (lib.navy N.beach), the size - 1.5 x what holds the beach and what it knows within 40
-- tiles behind it (1.4 x each), 6 at least, from the soldiers at home it can spare (one kept per enemy soldier at
-- its door), on the transports it has or can pay (+1, up to landing_transports), planned when the available army
-- beats 1.2 x the need; the transports full, the citizens along by the size (0 / 3 / 6 / 10, landing_citizens at
-- most); the sea on the way checked (ld_sea_risk: the threats within 10 tiles of the line, each role's attack for a
-- minute less what the escorts stop, over the transports' hit points; over 0.4 it waits). Phase 1: the missing
-- transports trained (one every 20 s); after 3 minutes it goes with those it has if they carry the need, after 6 it
-- gives up (10 minutes). Up to 6 warships go along (those that hit the threats on the way first): the beach's
-- defences and warships, what shoots at them, the soldiers on the beach. Board (lib.navy): all aboard, 9 in 10
-- after a minute, or 3 minutes; the last look at the beach and the sea before the commit (3 minutes for the
-- escorts, then called off). Sail, unload (lib.navy). Ashore: one lib.groups attack; the citizens build the forward
-- base (ld_bridgehead: towers, a barracks, a town centre, a hospital, a factory by their number, 3/2/5/2/3 builders,
-- a project it cannot build or pay for a minute dropped, free citizens help, all done: the citizens back to the
-- engine); soldiers trained at the base join (ld_capture); the transports go back to the meeting point and bring the
-- soldiers at home it can spare (4 at least) over while it holds, landing_waves times (ld_wave); a quarter left or
-- nothing left to attack: released, the transports go home.
local V = require("lib.view")
local G = require("lib.groups")
local N = require("lib.navy")
local LA = require("lib.army")   -- [forces.c] home_threat
local S = require("ageofai.settings")
local F = require("ageofai.fleet")
local MT = require("ageofai.metal")      -- [metal.c] the fleet's size by the enemy's, the calm sea's account
local M = {}

local ACC_CAP = 600
local NAVY_RES = {"wood", "gold", "iron"}
local SHIP_PRODUCERS = {b_dock = true, b_navy_yard = true}
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_info", "b_guard_tower_space"}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end
local function ceil_div(a, b)
    return (a + b - 1) // b
end

-- ---------------------------------------------------------------------------------------------------------------
-- The water map

local function detect(view, mem)
    local now = view.time
    if now - (mem.detect_t or -100000) < 60000 then
        return
    end
    mem.detect_t = now
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return
    end
    local ht = ai.tile(tx, ty)
    local hc = ht ~= nil and ht.continent or 0
    local enemies, across, none = 0, 0, 0
    for _, p in ipairs(ai.players()) do
        if p.enemy and p.alive and not p.me then
            local known = ai.memory{player = p.id, buildings = true, limit = 8}
            if #known == 0 then
                none = none + 1
            else
                local home = false
                for _, b in ipairs(known) do
                    local t = ai.tile(b.x, b.y)
                    if t ~= nil and t.continent == hc then
                        home = true
                    end
                end
                enemies = enemies + 1
                if not home then
                    across = across + 1
                end
            end
        end
    end
    if enemies == 0 then
        if now < 180000 then
            return
        end
        -- nothing known yet: a water map counts as islands, a land map as land. The game's own flag (the map's
        -- island characteristic) is not in ai.map(): a sea within 20 tiles of the town stands in
        local water = M.water_near(view, mem)
        mem.kind, mem.strength = water and "islands" or "land", water and 100 or 0
    else
        mem.strength = across * 100 // enemies
        mem.kind = across == 0 and "land" or across == enemies and "islands" or "mixed"
    end
    mem.home = hc
    if mem.kind ~= mem.kind_said then
        mem.kind_said = mem.kind
        print(string.format("navy map kind=%s strength=%d across=%d enemies=%d unknown=%d home=%d", mem.kind, mem.strength, across, enemies, none, hc))
    end
end

-- a sea within 20 tiles of the town (looked for once)
function M.water_near(view, mem)
    if (mem.fish_near or 0) > 0 then
        return true
    end
    if not mem.water_near and view.town_x >= 0 and view.time - (mem.water_t or -100000) >= 60000 then
        mem.water_t = view.time
        local sx, sy, sea = ai.shore(view.town_x, view.town_y, {max = 20})
        mem.water_near = sea ~= nil and sea ~= 0
    end
    return mem.water_near or false
end

-- ---------------------------------------------------------------------------------------------------------------
-- Counts, the account, the fleet's target

local function count(view, mem)
    if mem.count_t ~= nil and view.time - mem.count_t < 10000 then
        return
    end
    mem.count_t = view.time
    local ids = ai.units{category = "ship", armed = true}
    local role = {[0] = 0, 0, 0, 0, 0, 0}
    local war = 0
    if #ids > 0 then
        local cols = ai.get(ids, {"type"})
        for k = 1, #ids do
            local r = F.role(cols.type[k])
            if r ~= 0 then
                war = war + 1
                role[r] = role[r] + 1
            end
        end
    end
    mem.war = war
    mem.role = role
    local d = V.buildings(view, "b_dock")
    mem.docks = V.total(d)
    mem.docks_built = d ~= nil and d.finished or 0
    mem.fish = #ai.units{category = "boat"}
end

local function fleet_builds(mem)
    return S.navy_fleet == "big" and (mem.strength or 0) > 0
end
-- the navy's share of the income in percent x 100 (navy_share): toward navy_share_max with the warships missing
local function share(mem)
    if not S.navy or mem.kind == nil then
        return 0
    end
    local sh = S.navy_share * 100
    local t, miss = mem.target or 0, (mem.target or 0) - (mem.war or 0)
    if t > 0 and miss > 0 and S.navy_share_max > S.navy_share and fleet_builds(mem) then
        sh = sh + (S.navy_share_max - S.navy_share) * 100 * miss // t
    end
    return sh * (mem.strength or 0) // 100
end
local function pool(mem)
    local a = mem.acc or {}
    return ((a.wood or 0) + (a.gold or 0) + (a.iron or 0)) // 100
end
local function account(view, mem)
    mem.acc = mem.acc or {wood = 0, gold = 0, iron = 0}
    local sh = share(mem)
    local dt = math.max(view.time - (mem.acc_t or view.time), 0)
    mem.acc_t = view.time
    if sh > 0 and dt > 0 and dt <= 5000 then
        for _, r in ipairs(NAVY_RES) do
            -- the account in hundredths (a step's share of a minute's income is under 1)
            mem.acc[r] = math.min(mem.acc[r] + view.income[r] * dt * sh // (60000 * 100), ACC_CAP * 100)
        end
    end
end
-- after a warship order: its own part first, then the others, the largest first (navy_paid)
local function paid(mem, cost)
    local a = mem.acc
    for _, r in ipairs(NAVY_RES) do
        local over = (cost[r] or 0) * 100
        for pass = 1, 3 do
            if over <= 0 then
                break
            end
            local q = r
            if pass > 1 then
                q = nil
                for _, x in ipairs(NAVY_RES) do
                    if q == nil or a[x] > a[q] then
                        q = x
                    end
                end
            end
            local take = math.min(a[q], over)
            a[q] = a[q] - take
            over = over - take
        end
    end
end
-- what the others may not touch (navy_hold): the account, or the whole cost of the warship the mix waits for
local function hold(view, mem, memory)
    local h = {wood = 0, gold = 0, iron = 0}
    if share(mem) > 0 then
        local want = mem.want_type ~= nil and view.time - (mem.want_t or 0) < 120000 and V.cost(view, mem.want_type) or nil
        local total = 0
        if want ~= nil then
            for _, r in ipairs(NAVY_RES) do
                total = total + want.cost[r]
            end
            if pool(mem) < total then
                want = nil
            end
        end
        for _, r in ipairs(NAVY_RES) do
            h[r] = math.max(mem.acc[r] // 100, want ~= nil and want.cost[r] or 0)
        end
    end
    memory.eco = memory.eco or {}
    memory.eco.navy_hold = h
end

-- ---------------------------------------------------------------------------------------------------------------
-- The fleet mix (navy_pick) and the fleet's own orders (fl_build)

local function candidates(view)
    local out = {}
    for _, c in ipairs(view.costs) do
        if not c.building and c.producer ~= nil and SHIP_PRODUCERS[c.producer] then
            local r = F.role(c.type)
            if r ~= 0 then
                out[#out + 1] = {cost = c, role = r}
            end
        end
    end
    return out
end
-- in its role: factor x attack / (reload + 2 s) x hp / cost^2 (x 1e6, an integer)
local function score(c, role)
    local cls = F.cls(c.type)
    local cost = V.sum(c.cost)
    local f
    if role == F.LINE then
        f = 100
    elseif role == F.AA then
        f = 120
    elseif role == F.FRIG then
        f = 100
    else
        f = F.factor(c.type, "s09_royal_battleship")
    end
    if cost <= 0 or f <= 0 or (cls.attack or 0) <= 0 then
        return 0
    end
    return f * cls.attack * 1000 // ((cls.reload or 0) + 2000) * (cls.hp or 1) * 100 // (cost * cost)
end
-- the enemy seen (intel's memory, 10 minutes) and the wanted shares, every 30 s
local function seen_shares(view, mem)
    if mem.seen_t ~= nil and view.time - mem.seen_t < 30000 then
        return
    end
    mem.seen_t = view.time
    local e = {[0] = 0, 0, 0, 0, 0, 0}
    local air, carriers, coast = 0, 0, 0
    for _, r in ipairs(ai.memory{since = math.max(view.time - 600000, 0)}) do
        if not r.building then
            local kind = F.kind(r.type)
            if r.type:find("carrier") then
                carriers = carriers + 1
            elseif kind == "ship" then
                e[F.role(r.type)] = e[F.role(r.type)] + 1
            elseif kind == "air" then
                air = air + 1
            elseif kind == "siege" then   -- hits battleships x3+ from the coast
                for _, d in ipairs({{0, 0}, {4, 0}, {-4, 0}, {0, 4}, {0, -4}}) do
                    local t = ai.tile(r.x + d[1], r.y + d[2])
                    if t ~= nil and (t.sea or 0) ~= 0 then
                        coast = coast + 1
                        break
                    end
                end
            end
        end
    end
    local afloat = e[F.FRIG] + e[F.LINE] + e[F.ANTI] + e[F.AA]
    local airn = air + 3 * carriers
    local w = {[0] = 0, 0, 0, 0, 0, 0}   -- x 100
    w[F.FRIG] = 200 + 50 * (e[F.ANTI] + e[F.SUB])
    w[F.LINE] = (100 + 25 * e[F.FRIG]) * (1 + afloat) // (1 + afloat + e[F.SUB]) * 4 // (4 + coast)
    w[F.ANTI] = 50 + 75 * e[F.LINE]
    w[F.SUB] = 100 + 75 * e[F.LINE]
    w[F.AA] = airn > 0 and math.min(50 + airn * 100 // 8, 200) or 0
    mem.seen = e
    mem.want = w
    mem.seen_air, mem.seen_coast = airn, coast
end
-- the type to train (navy_pick): nil when none; sets mem.want_type when the first choice cannot be paid
local function navy_pick(view, mem, budget)
    seen_shares(view, mem)
    local cands = candidates(view)
    local best, bs = {}, {}
    local now = view.time
    mem.bad = mem.bad or {}
    for _, c in ipairs(cands) do
        local t = c.cost.type
        if (mem.bad[t] or 0) <= now then
            local s = score(c.cost, c.role)
            if s > (bs[c.role] or 0) then
                bs[c.role], best[c.role] = s, c.cost
            end
        end
    end
    local w, have = {}, {}
    local wsum, all = 0, 0
    for r = 1, 5 do
        w[r] = best[r] ~= nil and mem.want[r] or 0
        have[r] = mem.role[r]
    end
    if best[F.SUB] ~= nil then
        w[F.ANTI] = 0   -- e10+: submarines take the galleys' place
    end
    mem.ring = mem.ring or {}
    for _, k in ipairs(ai.keys(mem.ring)) do
        if now - mem.ring[k].t < 60000 then
            have[mem.ring[k].role] = have[mem.ring[k].role] + 1
        else
            mem.ring[k] = nil
        end
    end
    for r = 1, 5 do
        wsum = wsum + w[r]
        all = all + have[r]
    end
    if wsum <= 0 then
        return nil
    end
    local p = pool(mem)
    local done = {}
    local first = nil
    for _ = 1, 5 do
        local r0, d0 = nil, nil
        for r = 1, 5 do
            if w[r] > 0 and not done[r] then
                local d = w[r] * (all + 1) * 100 // wsum - have[r] * 100
                if r0 == nil or d > d0 then
                    r0, d0 = r, d
                end
            end
        end
        if r0 == nil then
            break
        end
        done[r0] = true
        if first == nil then
            first = r0
        end
        local c = best[r0]
        local pay, total = true, 0
        for _, r in ipairs(V.resources) do
            -- [priority.c] a warship with a target sees the stock but the rows marked apart (budget.ship)
            if c.cost[r] > ((budget and budget.ship) and budget.ship[r] or view.stock[r]) then
                pay = false
            end
        end
        for _, r in ipairs(NAVY_RES) do
            total = total + c.cost[r]
        end
        if pay and total <= p then
            if r0 ~= first then
                mem.cheaper = (mem.cheaper or 0) + 1
            end
            return c, r0
        end
    end
    if first ~= nil then
        mem.want_type, mem.want_t = best[first].type, now
    end
    return nil
end
-- the fleet trains the missing warships itself: every 10 s while afloat + ordered in 90 s < target
local function fl_build(view, mem, budget)
    if mem.build_t ~= nil and view.time - mem.build_t < 10000 then
        return
    end
    mem.build_t = view.time
    local t = mem.target or 0
    local recent = 0
    mem.bring = mem.bring or {}
    for _, k in ipairs(ai.keys(mem.bring)) do
        if view.time - mem.bring[k] < 90000 then
            recent = recent + 1
        else
            mem.bring[k] = nil
        end
    end
    if t <= 0 or mem.shelter or (mem.war or 0) + recent >= t or (mem.docks_built or 0) == 0 or view.population >= view.population_cap then
        return
    end
    local c, role = navy_pick(view, mem, budget)
    if c == nil then
        return
    end
    local docks = ai.units{type = "b_dock", built = true}
    if #docks == 0 then
        return
    end
    local code, reason = ai.train(docks, c.type, 1)
    if code == "ok" then
        paid(mem, c.cost)
        mem.built = (mem.built or 0) + 1
        mem.bring_n = (mem.bring_n or 0) + 1
        mem.bring[mem.bring_n] = view.time
        mem.ring_n = (mem.ring_n or 0) + 1
        mem.ring[mem.ring_n] = {t = view.time, role = role}
        if mem.want_type == c.type then
            mem.want_type = nil
        end
        if (mem.built or 0) % 4 == 1 then
            print(string.format("navy train type=%s role=%s afloat=%d target=%d account=%d/%d/%d", c.type, F.ROLE_NAMES[role], mem.war or 0, t,
                mem.acc.wood // 100, mem.acc.gold // 100, mem.acc.iron // 100))
        end
    else
        mem.build_refused = (mem.build_refused or 0) + 1
        if reason ~= "cannot_afford" and reason ~= "pop_cap" and reason ~= "busy" then
            mem.bad[c.type] = view.time + 300000   -- not trainable here: skipped 5 minutes
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Docks and fishing boats

local function docks(view, mem, order)
    if mem.dock_t ~= nil and view.time - mem.dock_t < 60000 then
        return
    end
    mem.dock_t = view.time
    -- the fish within reach of the town (the engine's coast rule; the fishing boats' count follows it too)
    mem.fish_near = #ai.resources{source = "fish", near = {view.town_x, view.town_y, S.fishing_reach}, limit = 16}
    local want = 0
    if share(mem) > 0 then
        want = 2
        if mem.kind == "islands" and ((mem.fish or 0) >= 12 or (mem.want_type ~= nil and view.time - (mem.want_t or 0) < 120000)) then
            want = 3
        end
        if fleet_builds(mem) and (mem.target or 0) > 0 then
            want = math.max(want, math.min(2 + mem.target // 8, 5))
        end
    elseif (mem.fish_near or 0) > 0 or M.water_near(view, mem) then
        want = 1   -- the game's own plan: a dock where there is water
    end
    mem.docks_want = want
    if want == 0 or V.cost(view, "b_dock") == nil or (mem.docks or 0) >= want or (mem.docks_built or 0) < (mem.docks or 0) then
        return
    end
    order({verb = "fish", boats = 0, docks = want})
    mem.docks_asked = (mem.docks_asked or 0) + 1
    if mem.docks_said ~= want * 100 + (mem.docks or 0) then
        mem.docks_said = want * 100 + (mem.docks or 0)
        print(string.format("navy docks have=%d want=%d", mem.docks or 0, want))
    end
end
-- the newest fishing boat the dock makes
local function boat_class(view)
    local best = nil
    for _, c in ipairs(view.costs) do
        if not c.building and c.producer == "b_dock" and c.type:find("fishing") and (best == nil or c.type > best.type) then
            best = c
        end
    end
    return best
end
local function fishing(view, mem, budget, more)
    if not S.fishing_boat or view.epoch < 1 or (mem.docks_built or 0) == 0 then
        return
    end
    if mem.fish_t ~= nil and view.time - mem.fish_t < 10000 then
        return
    end
    mem.fish_t = view.time
    local max = (mem.strength or 0) > 0 and S.fishing_boats_max_water or S.fishing_boats_max
    local wanted = math.min((mem.fish_near or 0) * S.fishing_boats_per_fish, max) + more
    local c = boat_class(view)
    if c == nil or (mem.fish or 0) + c.in_production + c.requested >= wanted then
        return
    end
    for _, r in ipairs(V.resources) do
        if c.cost[r] > (budget.civil[r] or 0) then
            return
        end
    end
    local docks = ai.units{type = "b_dock", built = true}
    if #docks > 0 then
        local code = ai.train(docks, c.type, 1)
        if code == "ok" then
            mem.boats_trained = (mem.boats_trained or 0) + 1
            if mem.boats_trained % 5 == 1 then
                print(string.format("navy boat type=%s have=%d wanted=%d fish_near=%d", c.type, mem.fish or 0, wanted, mem.fish_near or 0))
            end
        end
    end
end

-- no tower covers the fleet's sea (fl_rally_tower): one asked for by the dock farthest from the enemy - a land tile
-- within 5 tiles of it with the sea within 3, placeable - the newest tower; every 2 minutes while none is queued,
-- 3 a game at most
local function rally_tower(view, mem, order)
    local fl = mem.fleet
    if fl == nil or (fl.rcover or 0) > 0 or (mem.rally_towers or 0) >= 3 or view.time - (mem.tower_t or -1000000) < 120000 then
        return
    end
    local s = F.last_scan()
    if s == nil or s.docks == nil or #s.docks == 0 or (fl.ocean or 0) == 0 then
        return
    end
    mem.tower_t = view.time
    local tower = nil
    for t = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[t]) ~= nil then
            tower = TOWER_CLASSES[t]
            break
        end
    end
    local tw = V.buildings(view, tower or "")
    if tower == nil or (tw ~= nil and tw.unfinished + tw.queued > 0) then
        return
    end
    local dk, bd = nil, -1
    for _, d in ipairs(s.docks) do
        local dd = s.eb ~= nil and d2(d.x, d.y, s.eb.x, s.eb.y) or 0
        if dd > bd then
            dk, bd = d, dd
        end
    end
    -- the builders: the two citizens nearest the dock (held three minutes, then the engine's again)
    mem.rally_builders = mem.rally_builders or {}
    for _, id in ipairs(ai.keys(mem.rally_builders)) do
        if view.time - mem.rally_builders[id] >= 180000 then
            if ai.unit(id) ~= nil then
                ai.release(id)
            end
            mem.rally_builders[id] = nil
        end
    end
    local c = V.cost(view, tower)
    for _, r in ipairs(V.resources) do
        if c.cost[r] > view.stock[r] then
            return
        end
    end
    for r = 2, 5 do
        local x, y = ai.find_site(tower, dk.x, dk.y, {min = r, max = r})
        if x ~= nil and F.coast(x, y, fl.ocean) then
            local builders = ai.units{category = "citizen", near = {x, y, 30}, held = "engine", limit = 2}
            if #builders == 0 then
                return
            end
            ai.take(builders)
            local code, reason = ai.build(builders, tower, x, y)
            print(string.format("fleet rally_tower type=%s at=%d,%d dock=%d,%d result=%s reason=%s", tower, x, y, dk.x, dk.y, code, tostring(reason)))
            if code == "ok" then
                mem.rally_towers = (mem.rally_towers or 0) + 1
                for _, id in ipairs(builders) do
                    mem.rally_builders[id] = view.time
                end
            else
                ai.release(builders)
            end
            return
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The landing

-- the enemy known across the water: the nearest known enemy building whose land mass differs from the town's
local function target_across(view)
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return nil
    end
    local list = ai.memory{buildings = true, near = {tx, ty, 300}, limit = 12}
    for _, m in ipairs(list) do
        if not ai.reach(tx, ty, m.x, m.y) then
            return m
        end
    end
    return nil
end
local function enemy_at(x, y, r)
    local str = 0
    for _, m in ipairs(ai.memory{near = {x, y, r}}) do
        local c = F.cls(m.type)
        if (c.attack or 0) > 0 then
            str = str + m.hp * c.attack
        end
    end
    return str
end
local function transport_class(view)
    local best = nil
    for _, c in ipairs(view.costs) do
        if not c.building and c.producer == "b_dock" then
            local cls = F.cls(c.type)
            if (cls.transport or 0) > 0 and (best == nil or c.type > best.type) then
                best = c
            end
        end
    end
    return best
end
-- citizens along by the landing's size (ld_cits_for): one transport none, two 3, three 6, four and more 10
local function cits_for(ships, cap)
    local c = ships <= 1 and 0 or ships == 2 and 3 or ships == 3 and 6 or 10
    return math.min(c, S.landing_citizens, cap)
end
-- how many of a cost the stock above the reserve (plus the account: a landing's orders are not held) pays now
local function affordable(c, budget, h)
    local n = 20
    for _, r in ipairs(V.resources) do
        if c.cost[r] > 0 then
            n = math.min(n, ((budget.civil[r] or 0) + (h[r] or 0)) // c.cost[r])
        end
    end
    return n
end
-- the sea on the way (ld_sea_threat / ld_sea_risk): the enemy ships seen within 10 tiles of the line from (fx, fy)
-- to the water (every 5 tiles) that hit transports, by role: hit points and attack; the escorts (records) against
-- each role they hit; the risk: each role's attack x 30 s less what the escorts stop, over the transports' hit points
local function sea_risk(fx, fy, wx, wy, escorts, tr_hp)
    local pts = {}
    local dx, dy = wx - fx, wy - fy
    local len = math.floor(math.sqrt(dx * dx + dy * dy))
    local steps = math.min(len // 5, 46)
    for i = 0, steps do
        pts[#pts + 1] = {fx + dx * i // math.max(steps, 1), fy + dy * i // math.max(steps, 1)}
        if steps == 0 then
            break
        end
    end
    local eh, ea, rep, n = {}, {}, {}, 0
    for _, id in ipairs(ai.seen{enemy = true, category = "ship", armed = true}) do
        local u = ai.unit(id)
        if u ~= nil then
            local role = F.role(u.type)
            if role ~= 0 and role ~= F.AA then
                local on = false
                for _, p in ipairs(pts) do
                    if d2(u.x, u.y, p[1], p[2]) <= 100 then
                        on = true
                        break
                    end
                end
                if on then
                    eh[role] = (eh[role] or 0) + u.hp
                    ea[role] = (ea[role] or 0) + (u.attack or F.cls(u.type).attack or 0)
                    rep[role] = u.type
                    n = n + 1
                end
            end
        end
    end
    if n == 0 then
        return 0, 0
    end
    local oh, oa = {}, {}
    for _, e in ipairs(escorts) do
        for role = 1, 5 do
            if rep[role] ~= nil and F.hits(e.type, rep[role]) then
                oh[role] = (oh[role] or 0) + e.hp
                oa[role] = (oa[role] or 0) + (F.cls(e.type).attack or 0)
            end
        end
    end
    local dmg = 0.0
    for role = 1, 5 do
        if rep[role] ~= nil then
            local es = math.sqrt(eh[role] * ea[role])
            local os = math.sqrt((oh[role] or 0) * (oa[role] or 0))
            dmg = dmg + ea[role] * (os >= es and 0.5 or 1.0 - 0.5 * os / es) * 30.0
        end
    end
    return tr_hp > 0 and dmg / tr_hp or 0, n
end
local function escort_recs(ids)
    local out = {}
    if #ids == 0 then
        return out
    end
    local cols = ai.get(ids, {"x", "y", "hp", "type", "action", "target"})
    for k = 1, #ids do
        if cols.x[k] ~= nil then
            out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], hp = cols.hp[k], type = cols.type[k], action = cols.action[k], target = cols.target[k]}
        end
    end
    return out
end
local function alive(ids)
    local out = {}
    for _, id in ipairs(ids or {}) do
        if ai.unit(id) ~= nil then
            out[#out + 1] = id
        end
    end
    return out
end
-- the home force: the force's members ashore within 25 tiles of its home
local function home_force(view, force)
    local out = {}
    if force == nil then
        return out
    end
    local fx, fy = force.home_x or view.town_x, force.home_y or view.town_y
    if fx < 0 then
        fx, fy = view.town_x, view.town_y
    end
    local cols = ai.get(force.units, {"x", "y", "inside"})
    for k, id in ipairs(force.units) do
        if cols.x[k] ~= nil and cols.inside[k] == nil and d2(cols.x[k], cols.y[k], fx, fy) <= 625 then
            out[#out + 1] = id
        end
    end
    return out
end
local function take_from_force(force, ids)
    local set = {}
    for _, id in ipairs(ids) do
        set[id] = true
    end
    local keep = {}
    for _, id in ipairs(force.units) do
        if not set[id] then
            keep[#keep + 1] = id
        end
    end
    force.units = keep
    ai.take(ids)
end
local function say(mem, view, key, text)
    mem.said = mem.said or {}
    if view.time - (mem.said[key] or -100000) >= 60000 then
        mem.said[key] = view.time
        print(text)
    end
end

-- the escorts' orders (ld_escorts), every 2 s while the landing is on: the threats to the transports off the beach,
-- on the way over, near itself; then the beach's defences and the warships off it, what shoots at it where it is,
-- the soldiers on the beach; else the water in front of the site
local function escorts_step(view, mem, L)
    if mem.esc_t ~= nil and view.time - mem.esc_t < 2000 then
        return
    end
    mem.esc_t = view.time
    L.escorts = alive(L.escorts or {})
    if #L.escorts == 0 then
        return
    end
    local recs = escort_recs(L.escorts)
    local attacks, order = {}, {}
    local function foes_near(x, y, r, want_ships, soldiers)
        local list = ai.seen{enemy = true, armed = true, near = {x, y, r}, limit = 12}
        local out = {}
        if #list == 0 then
            return out
        end
        local cols = ai.get(list, {"x", "y", "type", "category"})
        for k = 1, #list do
            local cat = cols.category[k]
            if cols.x[k] ~= nil then
                local ok = (want_ships and cat == "ship") or (not want_ships and (cat == "building" or cat == "ship")) or (soldiers and (cat == "soldier" or cat == "siege"))
                if ok then
                    out[#out + 1] = {id = list[k], x = cols.x[k], y = cols.y[k], type = cols.type[k], category = cat}
                end
            end
        end
        return out
    end
    local function nearest_hit(e, list, x, y)
        local best, bd = nil, nil
        for _, f in ipairs(list) do
            if F.hits(e.type, f.type) then
                local d = d2(f.x, f.y, x, y)
                if bd == nil or d < bd then
                    best, bd = f, d
                end
            end
        end
        return best
    end
    local mx, my = (L.water_x + L.board_wx) // 2, (L.water_y + L.board_wy) // 2
    local ships_water = foes_near(L.water_x, L.water_y, 16, true)
    local ships_mid = foes_near(mx, my, 14, true)
    local def_water = foes_near(L.water_x, L.water_y, 12, false)
    local def_site = foes_near(L.site_x, L.site_y, 12, false)
    local sol_site = foes_near(L.site_x, L.site_y, 10, false, true)
    for _, e in ipairs(recs) do
        local busy = e.action == "attack" and e.target ~= nil and ai.unit(e.target) ~= nil
        if not busy then
            local t = nearest_hit(e, ships_water, e.x, e.y) or nearest_hit(e, ships_mid, e.x, e.y)
            if t == nil then
                t = nearest_hit(e, foes_near(e.x, e.y, 12, true), e.x, e.y)
            end
            if t == nil then
                t = nearest_hit(e, def_water, e.x, e.y) or nearest_hit(e, def_site, e.x, e.y)
            end
            if t == nil then
                t = nearest_hit(e, foes_near(e.x, e.y, 8, false), e.x, e.y) or nearest_hit(e, sol_site, e.x, e.y)
            end
            if t ~= nil then
                attacks[t.id] = attacks[t.id] or {}
                local a = attacks[t.id]
                a[#a + 1] = e.id
                order[#order + 1] = t.id
            elseif d2(e.x, e.y, L.water_x, L.water_y) > 9 and not (e.action == "move") then
                ai.move(e.id, L.water_x, L.water_y, {forced = true})
            end
        end
    end
    table.sort(order)
    local last = nil
    for _, tid in ipairs(order) do
        if tid ~= last then
            last = tid
            ai.attack(attacks[tid], tid)
        end
    end
end

-- the forward base (ld_bridgehead), every 2 s while citizens are ashore
local KIND_CITS = {tower = 3, barracks = 2, center = 5, hospital = 2, factory = 3}
local function base_plan(n)
    local plan
    if n >= 8 then
        plan = {"tower", "barracks", "center", "hospital", "factory", "tower"}
    elseif n >= 5 then
        plan = {"tower", "barracks", "tower"}
    elseif n > 0 then
        plan = {"tower", "tower"}
    else
        plan = {}
    end
    local proj = {}
    for k, kind in ipairs(plan) do
        proj[k] = {kind = kind, cits = KIND_CITS[kind], state = 0, tries = 0, b = 0, type = "", x = -1, y = -1}
    end
    return proj
end
local function kind_type(view, kind)
    local list
    if kind == "tower" then
        for t = #TOWER_CLASSES, 1, -1 do
            if V.cost(view, TOWER_CLASSES[t]) ~= nil then
                return TOWER_CLASSES[t]
            end
        end
        return nil
    elseif kind == "center" then
        list = {"b_town_center", "b_settlement"}
    elseif kind == "factory" then
        list = {"b_cyber_factory", "b_tank_factory", "b_stable", "b_archery_range"}
    elseif kind == "barracks" then
        list = {"b_barracks"}
    else
        list = {"b_hospital"}
    end
    for _, t in ipairs(list) do
        local c = V.cost(view, t)
        if c ~= nil then
            local pay = true
            for _, r in ipairs(V.resources) do
                if c.cost[r] > view.stock[r] then
                    pay = false
                end
            end
            if pay then
                return t
            end
        end
    end
    return nil
end
local function bridgehead(view, mem, B)
    local now = view.time
    B.cits = alive(B.cits)
    B.jobs = B.jobs or {}
    if #B.cits == 0 then
        return false
    end
    local cols = ai.get(B.cits, {"x", "y", "inside", "action"})
    local n, cx, cy = 0, 0, 0
    local ashore = {}
    for k, id in ipairs(B.cits) do
        if cols.x[k] ~= nil and cols.inside[k] == nil then
            n = n + 1
            cx, cy = cx + cols.x[k], cy + cols.y[k]
            ashore[id] = {x = cols.x[k], y = cols.y[k], action = cols.action[k]}
        end
    end
    if n == 0 then
        return true
    end
    cx, cy = cx // n, cy // n
    -- going up: finished or destroyed - its builders free
    for j, p in ipairs(B.proj) do
        if p.state == 1 then
            local u = ai.unit(p.b)
            if u == nil or u.built then
                p.state = u ~= nil and 2 or 3
                print(string.format("landing base %s type=%s at=%d,%d %s", p.kind, p.type, p.x, p.y, p.state == 2 and "finished" or "destroyed"))
                for _, id in ipairs(ai.keys(B.jobs)) do
                    if B.jobs[id] == j then
                        B.jobs[id] = nil
                    end
                end
            end
        end
    end
    -- the next project: citizens free, 10 s after the last
    local free, busy = {}, 0
    for _, id in ipairs(B.cits) do
        if ashore[id] ~= nil and B.jobs[id] == nil then
            free[#free + 1] = id
        end
    end
    for _, p in ipairs(B.proj) do
        if p.state == 1 then
            busy = busy + 1
        end
    end
    if #free > 0 and now - (B.base_t or -100000) >= 10000 then
        for j, p in ipairs(B.proj) do
            if p.state == 0 then
                local need = math.min(p.cits, n)
                if busy > 0 and #free < need then
                    break   -- in order: wait for builders
                end
                local t = kind_type(view, p.kind)
                B.base_t = now
                if t == nil then   -- it cannot build or pay it: again for a minute, then dropped
                    p.tries = p.tries + 1
                    if p.tries >= 6 then
                        p.state = 3
                        print(string.format("landing base %s dropped why=cannot_build_or_pay", p.kind))
                    else
                        break
                    end
                else
                    local x, y = ai.find_site(t, cx, cy, {min = 2, max = 12, safe = true})
                    if x == nil then
                        x, y = ai.find_site(t, cx, cy, {min = 2, max = 12})
                    end
                    local builders = {}
                    table.sort(free, function(a, b)
                        local da, db = d2(ashore[a].x, ashore[a].y, x or cx, y or cy), d2(ashore[b].x, ashore[b].y, x or cx, y or cy)
                        if da ~= db then
                            return da < db
                        end
                        return a < b
                    end)
                    for k = 1, math.min(need, #free) do
                        builders[k] = free[k]
                    end
                    local code, reason, _, id = "refused", "no_site", 0, nil
                    if x ~= nil then
                        code, reason, _, id = ai.build(builders, t, x, y)
                    end
                    print(string.format("landing base %s type=%s at=%s,%s %s cits=%d free=%d", p.kind, t, tostring(x), tostring(y),
                        code == "ok" and "foundation" or ("no_room:" .. tostring(reason)), n, #free))
                    if code ~= "ok" or id == nil then
                        p.state = 3
                    else
                        p.b, p.type, p.x, p.y, p.state = id, t, x, y, 1
                        for _, b in ipairs(builders) do
                            B.jobs[b] = j
                        end
                    end
                    break
                end
            end
        end
    end
    -- orders: each on its building; the free ones help the first going up
    local helping, left = nil, 0
    for j, p in ipairs(B.proj) do
        if p.state == 1 and helping == nil then
            helping = j
        end
        if p.state < 2 then
            left = left + 1
        end
    end
    if left == 0 then
        print(string.format("landing base done cits=%d", #B.cits))
        ai.release(B.cits)
        B.cits = {}
        return false
    end
    for _, id in ipairs(B.cits) do
        local j = B.jobs[id] or helping
        local a = ashore[id]
        if a ~= nil and j ~= nil and B.proj[j].state == 1 and a.action ~= "build" and a.action ~= "repair" then
            ai.repair(id, B.proj[j].b)
        end
    end
    return true
end
-- soldiers trained at the forward base (a barracks, town centre or factory standing): within 8 tiles of it, the
-- engine's - they join the troops ashore
local function capture(memory, B, landed)
    if landed == nil then
        return
    end
    for _, p in ipairs(B.proj) do
        if p.state == 2 and p.kind ~= "tower" and p.kind ~= "hospital" then
            local ids = ai.units{category = "soldier", held = "engine", near = {p.x, p.y, 8}, limit = 10}
            if #ids > 0 then
                G.add(memory, landed, ids)
                B.captured = (B.captured or 0) + #ids
            end
        end
    end
end

local function release_landing(mem, L, why)
    if L.soldiers ~= nil and #L.soldiers > 0 then
        ai.release(alive(L.soldiers))
    end
    if L.cits ~= nil and #L.cits > 0 then
        ai.release(alive(L.cits))
    end
    mem.landing = nil
    mem.landing_t = ai.time()
end
-- the landing over: the transports go home and are released, the escorts go back to the fleet
local function landing_over(mem, why)
    local ships = alive(mem.ships or {})
    if #ships > 0 then
        if mem.board_wx ~= nil then
            ai.move(ships, mem.board_wx, mem.board_wy, {forced = true})
        end
        ai.release(ships)
    end
    mem.ships = nil
    mem.escorts = nil
    mem.base = nil
    mem.wave = nil
    mem.landing_t = ai.time()
    print(string.format("landing over why=%s landings=%d waves=%d", why, mem.landings or 0, mem.waves or 0))
end

-- the landing's state for the fleet's escort: phase 1 getting its transports, 2 boarding, 3 at sea, 4 ashore
local function landing_phase(mem)
    local L = mem.landing
    if L ~= nil then
        local ph = L.state == "board" and 2 or 3
        return {phase = ph, ocean = L.sea, water_x = L.water_x, water_y = L.water_y}
    end
    if mem.base ~= nil or mem.ships ~= nil then
        return {phase = 4, ocean = mem.sea or 0, water_x = mem.water_x or 0, water_y = mem.water_y or 0}
    end
    if mem.prep ~= nil then
        return {phase = 1, ocean = mem.prep.sea, water_x = mem.prep.water_x, water_y = mem.prep.water_y}
    end
    return nil
end

-- phase 0: the plan (ld_phase_plan), every 30 s
local function plan_landing(view, mem, budget, force, target, h)
    if view.time - (mem.plan_t or -100000) < 30000 then
        return
    end
    mem.plan_t = view.time
    if view.time < (mem.next_try or 0) then
        return
    end
    local home = home_force(view, force)
    local keep = LA.home_threat()   -- [forces.c] the enemy soldiers seen within 20 tiles of any own building
    local avail = #home - keep
    if avail < 6 then
        say(mem, view, "plan", string.format("landing none why=%s home=%d keep=%d", keep > 0 and "enemy_at_door" or "fewer_than_6", #home, keep))
        return
    end
    local tr = transport_class(view)
    if tr == nil then
        say(mem, view, "plan", "landing none why=no_transport")
        return
    end
    local cap = F.cls(tr.type).transport or 10
    local have = N.transports()
    local afford = math.min(#have + affordable(tr, budget, h) + 1, S.landing_transports)
    avail = math.min(avail, afford * cap)
    local per = math.max((force.strength or 0) // math.max(#force.units, 1), 1)
    local power = per * avail
    local sea, dock = N.dock_sea()
    if sea == nil then
        say(mem, view, "plan", "landing none why=no_sea_at_dock")
        return
    end
    local site_x, site_y, beach = N.beach({x = target.x, y = target.y}, sea)
    if site_x == nil then
        say(mem, view, "plan", "landing none why=no_beach")
        return
    end
    local need = beach * 14 // 10
    if power * 10 < need * 12 then
        say(mem, view, "plan", string.format("landing none why=beach_too_strong need=%d power=%d avail=%d site=%d,%d", need * 12 // 10, power, avail, site_x, site_y))
        return
    end
    local behind = enemy_at(site_x, site_y, 40)
    local big = behind * 14 // 10
    local n = math.max(ceil_div(15 * math.max(big, need), 10 * per), 6)
    n = math.min(n, avail)
    local ships = ceil_div(n, cap)
    local cits = cits_for(ships, cap)
    local water_x, water_y = site_x, site_y   -- the water in front comes with the plan (lib.navy); the beach stands in
    -- the sea on the way over, from the dock, against the warships it could take along
    local war = ai.units{category = "ship", armed = true, limit = 6}
    local risk, threats = sea_risk(dock.x, dock.y, water_x, water_y, escort_recs(war), (ships + 1) * (F.cls(tr.type).hp or 1000))
    if risk > 0.4 then
        say(mem, view, "plan", string.format("landing none why=sea_held threats=%d risk=%.2f", threats, risk))
        return
    end
    ships = math.min(ceil_div(n + cits, cap), afford)
    cits = cits_for(ships, cap)
    if ships * cap - cits < n then
        cits = math.max(ships * cap - n, 0)
    end
    n = math.min(ships * cap - cits, avail)
    mem.prep = {target = target.player, tx = target.x, ty = target.y, site_x = site_x, site_y = site_y, water_x = water_x, water_y = water_y,
                sea = sea, n = n, ships = ships, cits = cits, since = view.time, per = per, need = need, trained_t = 0, tr_type = tr.type, cap = cap}
    mem.tries = (mem.tries or 0) + 1
    print(string.format("landing plan target=p%d site=%d,%d sea=%d n=%d cits=%d ships=%d cap=%d home=%d keep=%d avail=%d power=%d need=%d big=%d have=%d",
        target.player, site_x, site_y, sea, n, cits, ships, cap, #home, keep, avail, power, need, big, #have))
end
-- phase 1 (ld_phase_ships): the transports trained, then the passengers, the citizens and the escorts: the plan
local function ships_phase(view, mem, budget, force, memory)
    local P = mem.prep
    local tr = V.cost(view, P.tr_type)
    local have = N.transports()
    local want = P.ships
    if #have < want and view.time - P.since >= 180000 and #have > 0 then
        local cits = cits_for(#have, P.cap)
        local n = math.min(#have * P.cap - cits, P.n)
        if P.per * n >= P.need then
            print(string.format("landing goes_with have=%d of=%d n=%d", #have, want, n))
            P.ships, P.n, P.cits, want = #have, n, cits, #have
        end
    end
    if #have < want then
        -- [priority.c] the landing row (landing_reserve, phase 1): the missing transports' cost held from the
        -- others; the landing's own order sees past it and the navy's account (g_landing_order)
        mem.landing_hold = {}
        for _, r in ipairs(V.resources) do
            mem.landing_hold[r] = tr ~= nil and (want - #have) * tr.cost[r] or 0
        end
        if view.time - P.since >= 360000 then
            mem.aborted = (mem.aborted or 0) + 1
            mem.next_try = view.time + 600000
            mem.prep = nil
            mem.landing_hold = nil
            print("landing given_up why=no_transports")
            return
        end
        if tr ~= nil and view.time - P.trained_t >= (P.tr_got and 120000 or 20000) then
            P.trained_t = view.time
            P.tr_got = false
            local PR = require("ageofai.priority")
            local vis = PR.visible(view, memory, PR.S_MIL | PR.S_ORDER | PR.S_LANDING_ORDER)
            if affordable(tr, {civil = vis}, {}) >= 1 and tr.in_production + tr.requested == 0 then
                local docks = ai.units{type = "b_dock", built = true}
                if #docks > 0 and ai.train(docks, tr.type, 1) == "ok" then
                    P.tr_got = true
                    mem.transports_asked = (mem.transports_asked or 0) + 1
                end
            end
        end
        say(mem, view, "ships", string.format("landing waits_transports have=%d want=%d", #have, want))
        return
    end
    mem.landing_hold = nil   -- [priority.c] the transports are there
    -- the passengers: the force's members at home, the nearest the dock first
    local home = home_force(view, force)
    if #home < 6 then
        mem.aborted = (mem.aborted or 0) + 1
        mem.next_try = view.time + 300000
        mem.prep = nil
        print("landing given_up why=too_few_passengers")
        return
    end
    local sea, dock = N.dock_sea()
    local cols = ai.get(home, {"x", "y"})
    local order = {}
    for k, id in ipairs(home) do
        order[#order + 1] = {id = id, d = d2(cols.x[k], cols.y[k], dock.x, dock.y)}
    end
    table.sort(order, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.id < b.id
    end)
    local soldiers = {}
    for k = 1, math.min(P.n, #order) do
        soldiers[k] = order[k].id
    end
    local carry = {}
    local cap = 0
    for _, t in ipairs(have) do
        if cap < P.n + P.cits then
            carry[#carry + 1] = t
            cap = cap + t.cap
        end
    end
    local plan, why = N.plan({x = P.tx, y = P.ty, player = P.target}, soldiers, carry, {site_x = P.site_x, site_y = P.site_y})
    if plan == nil then
        say(mem, view, "ships", string.format("landing no_plan why=%s", why))
        if view.time - P.since >= 360000 then
            mem.prep = nil
            mem.next_try = view.time + 300000
        end
        return
    end
    -- the citizens for the bridgehead: the nearest to the meeting point within 25 tiles
    local cits = {}
    if P.cits > 0 then
        cits = ai.units{category = "citizen", near = {plan.board_x, plan.board_y, 25}, limit = P.cits}
    end
    -- up to 6 warships along: those that hit the threats on the way first
    local war = ai.units{category = "ship", armed = true}
    local escorts = {}
    if #war > 0 then
        local recs = escort_recs(war)
        local threats = {}
        for _, id in ipairs(ai.seen{enemy = true, category = "ship", armed = true, limit = 16}) do
            local u = ai.unit(id)
            if u ~= nil then
                threats[#threats + 1] = u.type
            end
        end
        local function useful(e)
            for _, t in ipairs(threats) do
                if F.hits(e.type, t) then
                    return true
                end
            end
            return false
        end
        for pass = 1, 2 do
            for _, e in ipairs(recs) do
                if #escorts < 6 and e.id ~= mem.sea_scout and (pass == 2 or useful(e)) then
                    local dup = false
                    for _, id in ipairs(escorts) do
                        if id == e.id then
                            dup = true
                        end
                    end
                    if not dup then
                        escorts[#escorts + 1] = e.id
                    end
                end
            end
        end
    end
    take_from_force(force, soldiers)
    ai.take(plan.ships)
    if #cits > 0 then
        ai.take(cits)
        for _, id in ipairs(cits) do
            plan.soldiers[#plan.soldiers + 1] = id   -- loaded with the soldiers (lib.navy shares them out)
        end
    end
    if #escorts > 0 then
        ai.take(escorts)
    end
    plan.cits = cits
    plan.escorts = escorts
    plan.need = P.need
    mem.landing = plan
    mem.prep = nil
    mem.sea, mem.water_x, mem.water_y = plan.sea, plan.water_x, plan.water_y
    mem.board_wx, mem.board_wy = plan.board_wx, plan.board_wy
    memory.army.next_attack = view.time + (S.attack_interval_min + ai.random(0, S.attack_interval_max - S.attack_interval_min)) * 60000
    print(string.format("landing board n=%d cits=%d ships=%d escorts=%d site=%d,%d board=%d,%d target=%d,%d need=%d", #soldiers, #cits, #plan.ships,
        #escorts, plan.site_x, plan.site_y, plan.board_x, plan.board_y, P.tx, P.ty, P.need))
end
-- the sail check while boarding (ld_phase_board): the beach now against what is aboard, the sea on the way; the
-- escorts get 3 minutes, then it is called off
local function sail_check(view, mem, L)
    if L.state ~= "board" then
        return
    end
    local aboard = {}
    local cols = ai.get(L.soldiers, {"inside", "category"})
    for k, id in ipairs(L.soldiers) do
        if cols.inside[k] ~= nil and cols.category[k] ~= "citizen" then
            aboard[#aboard + 1] = id
        end
    end
    local pw = #aboard > 0 and ai.strength(aboard) or 0
    local need = enemy_at(L.site_x, L.site_y, 12) * 14 // 10
    local ships = alive(L.ships)
    local tr_hp = 0
    if #ships > 0 then
        local c = ai.get(ships, {"hp"})
        for k = 1, #ships do
            tr_hp = tr_hp + (c.hp[k] or 0)
        end
    end
    local risk, threats = sea_risk(L.board_wx, L.board_wy, L.water_x, L.water_y, escort_recs(alive(L.escorts or {})), tr_hp)
    local hold_why = nil
    if risk > 0.4 then
        hold_why = "sea_held"
    elseif pw < need then
        hold_why = "beach_too_strong"
    end
    L.hold = hold_why
    if hold_why ~= nil then
        say(mem, view, "hold", string.format("landing waits why=%s aboard=%d need=%d risk=%.2f threats=%d escorts=%d", hold_why, pw, need, risk, threats, #(L.escorts or {})))
    end
end
-- the waves (ld_wave): the transports back at the meeting point bring the soldiers at home it can spare, while the
-- bridgehead holds, landing_waves times
local function waves(view, mem, force, landed)
    local W = mem.wave
    if W == nil or mem.landing ~= nil then
        return
    end
    mem.ships = alive(mem.ships or {})
    if #mem.ships == 0 then
        return
    end
    if W.n >= S.landing_waves or landed == nil or view.time - (W.t or 0) < 20000 then
        return
    end
    W.t = view.time
    local home = home_force(view, force)
    local keep = LA.home_threat()   -- [forces.c] home_threat
    local trs = N.transports()
    local carry, cap = {}, 0
    for _, t in ipairs(trs) do
        for _, id in ipairs(mem.ships) do
            if t.id == id then
                carry[#carry + 1] = t
                cap = cap + t.cap
            end
        end
    end
    local want = math.min(#home - keep, cap)
    if want < 4 or #carry == 0 then
        return
    end
    local soldiers = {}
    for k = 1, want do
        soldiers[k] = home[k]
    end
    local plan, why = N.plan({x = mem.tx or view.town_x, y = mem.ty or view.town_y, player = landed.target}, soldiers, carry, {site_x = mem.site_x, site_y = mem.site_y})
    if plan == nil then
        say(mem, view, "wave", string.format("landing wave no_plan why=%s", why))
        return
    end
    take_from_force(force, soldiers)
    plan.wave = true
    plan.join = landed.id
    plan.escorts = mem.escorts or {}
    plan.cits = {}
    mem.landing = plan
    W.n = W.n + 1
    mem.waves = (mem.waves or 0) + 1
    print(string.format("landing wave k=%d n=%d ships=%d", W.n, #soldiers, #plan.ships))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

function M.step(view, memory, orders, budget, force)
    memory.navy = memory.navy or {}
    local mem = memory.navy
    memory.eco = memory.eco or {}
    local function order(o)
        orders[#orders + 1] = o
    end
    detect(view, mem)
    count(view, mem)
    if mem.prep == nil then
        mem.landing_hold = nil   -- [priority.c] no landing getting its transports
    end
    -- the fleet's size target (stage 4)
    if fleet_builds(mem) then
        mem.target = (view.population_cap * S.navy_fleet_pop * (mem.strength or 0) + 5000) // 10000
        -- [metal.c] fleet_by_enemy: at most metal_fleet_pct of the enemy warships known + 6, at least metal_fleet_min
        -- x the strength
        mem.target = MT.fleet_target(view, memory, mem.target, mem.strength or 0)
    else
        mem.target = 0
    end
    account(view, mem)
    -- [metal.c] calm_navy_cap: the sea calm for 3 minutes - the account keeps 100 gold and 100 iron at most
    MT.calm(view, memory, mem, share(mem) > 0)
    hold(view, mem, memory)
    docks(view, mem, order)
    fishing(view, mem, budget, memory.eco.more_boats or 0)   -- econ's: citizens idle for want of a food site
    fl_build(view, mem, budget)
    if view.time - (mem.budget_t or -1000000) >= 300000 and share(mem) > 0 then
        mem.budget_t = view.time
        print(string.format("navy budget share=%d account=%d/%d/%d warships=%d F%d L%d A%d S%d AA%d target=%d docks=%d/%d fishing=%d built=%d refused=%d want=%s seen=F%d L%d A%d S%d air=%d coast=%d",
            share(mem) // 100, mem.acc.wood // 100, mem.acc.gold // 100, mem.acc.iron // 100, mem.war or 0, mem.role[1], mem.role[2], mem.role[3], mem.role[4], mem.role[5],
            mem.target or 0, mem.docks_built or 0, mem.docks or 0, mem.fish or 0, mem.built or 0, mem.build_refused or 0, tostring(mem.want_type),
            mem.seen and mem.seen[1] or 0, mem.seen and mem.seen[2] or 0, mem.seen and mem.seen[3] or 0, mem.seen and mem.seen[4] or 0,
            mem.seen_air or 0, mem.seen_coast or 0))
    end
    if not S.landing then
        return
    end
    local target = target_across(view)
    mem.across = target ~= nil
    -- nothing of the enemy known and the land explored: a boat looks over the water (the landing's warships look at
    -- the beach first upstream; here the exploration itself, since nothing crosses the water otherwise)
    local known = ai.memory{buildings = true, limit = 1}
    if #known == 0 and view.time >= 300000 and view.time - (mem.sea_scout_t or 0) >= 120000 then
        local scout = mem.sea_scout ~= nil and ai.unit(mem.sea_scout) or nil
        if scout == nil then
            local ships = ai.units{category = "ship", armed = true, limit = 1}
            if #ships == 0 then
                ships = ai.units{category = "boat", limit = 1}
            end
            if #ships > 0 then
                mem.sea_scout = ships[1]
                scout = ai.unit(ships[1])
            end
        end
        if scout ~= nil then
            ai.take(scout.id)
            -- the nearest unexplored point of its water body, furthest from the town first; one reached is skipped
            mem.sea_skips = mem.sea_skips or {}
            if mem.sea_goal_x ~= nil and d2(scout.x, scout.y, mem.sea_goal_x, mem.sea_goal_y) <= 16 and #mem.sea_skips < 64 then
                mem.sea_skips[#mem.sea_skips + 1] = {mem.sea_goal_x, mem.sea_goal_y}
            end
            local gx, gy = ai.unexplored(scout.x, scout.y, {sea = true, grid = 8, away = {view.town_x, view.town_y}, skip = mem.sea_skips})
            if gx == nil then
                -- the water body's grid ran dry (its id differs from the boat's tile's near the coast): the unexplored
                -- tiles of a coarse grid the boat can sail to (ai.route over the sea knows the map under the fog)
                local map = ai.map()
                local best, bd = nil, nil
                for y = 6, map.height - 6, 12 do
                    for x = 6, map.width - 6, 12 do
                        local t = ai.tile(x, y)
                        if t ~= nil and not t.explored then
                            local skip = false
                            for _, sk in ipairs(mem.sea_skips) do
                                if d2(sk[1], sk[2], x, y) <= 64 then
                                    skip = true
                                end
                            end
                            if not skip then
                                local d = d2(x, y, scout.x, scout.y) // 4 - d2(x, y, view.town_x, view.town_y)
                                if bd == nil or d < bd then
                                    best, bd = {x, y}, d
                                end
                            end
                        end
                    end
                end
                if best ~= nil and ai.route(scout.x, scout.y, best[1], best[2], {sea = true, avoid = 0}) ~= nil then
                    gx, gy = best[1], best[2]
                elseif best ~= nil then
                    mem.sea_skips[#mem.sea_skips + 1] = best   -- no water way there
                end
            end
            -- a goal the boat made no way toward in 20 s is skipped (the water's edge it cannot pass)
            if gx ~= nil and gx == mem.sea_goal_x and gy == mem.sea_goal_y and mem.sea_at_x ~= nil
                    and d2(scout.x, scout.y, mem.sea_at_x, mem.sea_at_y) <= 4 and #mem.sea_skips < 64 then
                mem.sea_skips[#mem.sea_skips + 1] = {gx, gy}
                gx = nil
            end
            mem.sea_at_x, mem.sea_at_y = scout.x, scout.y
            if gx ~= nil then
                mem.sea_goal_x, mem.sea_goal_y = gx, gy
                ai.move(scout.id, gx, gy, {forced = true})
            else
                -- nothing left to look at over the water: the boat fishes again, the skips are forgotten in 10 minutes
                -- (no ai.explore: the engine's explore goal is not in the determinism trace's hash)
                ai.release(scout.id)
                mem.sea_scout = nil
                mem.sea_scout_t = view.time + 480000
                mem.sea_skips = {}
                print(string.format("navy sea_scout id=%d type=%s done=water_explored", scout.id, scout.type))
            end
            if gx ~= nil then
                mem.sea_scout_t = view.time - 100000   -- again in 20 s
            end
            mem.sea_scouts = (mem.sea_scouts or 0) + 1
            if mem.sea_scouts % 6 == 1 and gx ~= nil then
                print(string.format("navy sea_scout id=%d type=%s to=%d,%d", scout.id, scout.type, gx, gy))
            end
        end
    elseif mem.sea_scout ~= nil and #known > 0 then
        local scout = ai.unit(mem.sea_scout)
        if scout ~= nil then
            ai.release(scout.id)   -- the enemy found: the boat fishes again
        end
        mem.sea_scout = nil
    end
    -- the fleet (fleet.c), every 2 s: without the landing's escorts and the sea scout
    local exclude = {}
    local esc = mem.landing ~= nil and mem.landing.escorts or mem.escorts or {}
    for _, id in ipairs(esc) do
        exclude[#exclude + 1] = id
    end
    if mem.sea_scout ~= nil then
        exclude[#exclude + 1] = mem.sea_scout
    end
    mem.fleet = mem.fleet or {}
    mem.fleet.target = mem.target
    if share(mem) > 0 then
        F.step(view, mem.fleet, landing_phase(mem), exclude)
        mem.shelter = mem.fleet.shelter
        if S.navy_fleet_mass then
            rally_tower(view, mem, order)
        end
    elseif #exclude < #ai.units{category = "ship", armed = true, limit = 3} then
        N.fleet_step(mem, {reach = 24, skip = mem.sea_scout})   -- a land map: the warships guard the dock
    end
    local landed = G.role(memory, "landed")
    -- a landing under way
    if mem.landing ~= nil then
        local L = mem.landing
        L.escorts = alive(L.escorts or {})
        escorts_step(view, mem, L)
        sail_check(view, mem, L)
        local result, detail = N.landing_step(L)
        if result == "landed" then
            -- the citizens ashore are the bridgehead's, the soldiers the landed group's
            local cits, sold = {}, {}
            local cset = {}
            for _, id in ipairs(L.cits or {}) do
                cset[id] = true
            end
            for _, id in ipairs(detail) do
                if cset[id] then
                    cits[#cits + 1] = id
                else
                    sold[#sold + 1] = id
                end
            end
            print(string.format("landing ashore n=%d cits=%d at=%d,%d retries=%d wave=%s", #sold, #cits, L.site_x, L.site_y, L.retries or 0, tostring(L.wave or false)))
            if L.wave and landed ~= nil then
                G.add(memory, landed, sold)
            else
                local g = G.create(memory, {task = "attack", role = "landed", x = L.tx, y = L.ty, target = L.target,
                                            home_x = L.site_x, home_y = L.site_y})
                G.add(memory, g, sold)
                landed = g
                mem.landings = (mem.landings or 0) + 1
                mem.site_x, mem.site_y, mem.tx, mem.ty = L.site_x, L.site_y, L.tx, L.ty
                mem.wave = {n = 0, t = view.time}
            end
            if #cits > 0 then
                mem.base = {cits = cits, proj = base_plan(#cits), jobs = {}}
            end
            mem.ships = L.ships
            mem.escorts = L.escorts
            mem.landing = nil
        elseif result == "aborted" then
            print(string.format("landing aborted why=%s state=%s aboard=%d wave=%s", detail, L.state, L.aboard or 0, tostring(L.wave or false)))
            -- the transports unload where they are, at the boarding shore
            for _, id in ipairs(alive(L.ships)) do
                ai.unload(id, L.board_x, L.board_y)
            end
            mem.aborted = (mem.aborted or 0) + 1
            if L.wave then
                mem.ships = L.ships
                mem.escorts = L.escorts
                if L.hold == "sea_held" and mem.wave ~= nil then
                    mem.wave.n = S.landing_waves
                end
                ai.release(alive(L.soldiers))
                mem.landing = nil
            else
                mem.next_try = view.time + 600000
                mem.escorts = nil
                ai.release(alive(L.ships))
                ai.release(alive(L.escorts or {}))
                release_landing(mem, L, detail)
            end
        end
        return
    end
    -- ashore: the bridgehead, the captures, the waves; the landed group released when a quarter is left or it has
    -- nothing left to fight
    if mem.base ~= nil then
        if not bridgehead(view, mem, mem.base) then
            mem.base = nil
        else
            capture(memory, mem.base, landed)
        end
    end
    if landed ~= nil and (#landed.units * 4 <= (landed.started or 1) or landed.state == "arrived") then
        print(string.format("landing spent units=%d started=%d state=%s", #landed.units, landed.started or 0, landed.state))
        G.disband(memory, landed)
        landed = nil
        if mem.base ~= nil and #mem.base.cits > 0 then
            ai.release(alive(mem.base.cits))
        end
        landing_over(mem, "spent")
    elseif landed == nil and (mem.ships ~= nil or mem.escorts ~= nil) then
        landing_over(mem, "no_group")
    end
    if landed ~= nil then
        if mem.escorts ~= nil then
            escorts_step(view, mem, {escorts = mem.escorts, water_x = mem.water_x, water_y = mem.water_y, board_wx = mem.board_wx, board_wy = mem.board_wy,
                                     site_x = mem.site_x, site_y = mem.site_y})
            mem.escorts = alive(mem.escorts)
        end
        waves(view, mem, force, landed)
        return
    end
    if target == nil or view.time - (mem.landing_t or 0) < 60000 then
        return
    end
    -- the dock first
    if (mem.docks_built or 0) == 0 then
        if V.cost(view, "b_dock") ~= nil and (mem.docks or 0) == 0 and view.time - (mem.dock_ask_t or 0) >= 120000 then
            order({verb = "fish", boats = 0, docks = 1})
            mem.dock_ask_t = view.time
        end
        return
    end
    if force == nil or #force.units == 0 then
        return
    end
    if mem.prep ~= nil then
        ships_phase(view, mem, budget, force, memory)
        return
    end
    if view.time < (memory.army and memory.army.next_attack or 0) then
        return
    end
    plan_landing(view, mem, budget, force, target, memory.eco.navy_hold or {})
end

return M
