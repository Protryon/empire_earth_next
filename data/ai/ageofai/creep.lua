-- Age of AI port: upstream dll/ai/creep.c (TOWER CREEP: towers laid at an enemy's buildings in the stalemate; UPSTREAM
-- has the map and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.creep"), called from
-- main.lua's step after ageofai.walls; settings.lua's `tower_creep` (off upstream) switches it on.
--
-- The player, 2026-11-16: "offensive towers and tower creep, espacialy in these prehistoric stealmates they shoudl
-- come handy. if an enemy territory seems mostly unguard walk up and place a tower just in range of its buildings,
-- greeping in, it can even be used to take down enemy towers, just bring enough citezens to out repair the other
-- towers demage while building, than add 2 or 3 more towers to overpower the enemy one."
--
-- THE STALEMATE: a creep begins only after CR_STARVED without a citizen of its gathering food (the view's food
-- gatherers) - before farms the land's food runs out and soldiers cannot be paid; its crew is CR_CREW_TOWER whatever
-- the target (its citizens have nothing else to do; fewer, down to CR_CREW_LOW, only when no more are left), no
-- workforce is kept back, and the owner's citizens end a creep only once they outnumber its crew.
-- One creep at a time per AI, every CR_EVERY a look:
--   THE TARGET  - a finished enemy building it knows (seen or remembered: ai.memory), not a wall's piece, the nearest
--                 to a finished building of its own first, however far; unguarded: no more than CR_FOES enemy
--                 soldiers in its territory or seen within CR_FOE_R, no more than CR_CITS of its citizens seen within
--                 CR_FOE_R. One where a creep was dropped not again for CR_BAD_WAIT.
--   THE SPOT    - a tile at the edge of its best tower's range of the target - the range with its upgrades
--                 (ai.class) - out of the reach of every enemy defence it knows or has explored (ai.danger), on the
--                 crew's land, the one nearest its own buildings. A tower as the target: from outside its reach where
--                 its own tower outranges it; else from in it, never where the enemy's outranges its own.
--   IN A TOWER'S REACH: the spot as far out as CR_EDGE past its own range but no farther than its sight, the farthest
--                 first, then the nearest its own (so the next towers go beside the first) - and its crew builds from
--                 the far side.
--   BEHIND      - the crew's place: CR_BEHIND tiles past the spot's far edge, straight away from the target, out of
--                 the reach of every enemy defence it knows.
--   THE CREW    - CR_CREW_TOWER of its citizens gathering or idle, the nearest, on the crew's land.
--   THE WALK    - the crew walks to its place behind the spot first; the first foundation is laid once half of it
--                 stands within CR_THERE tiles of it.
--   THE TOWERS  - laid by ai.build with the crew (the engine pays and places: upstream's place_foundation), the crew
--                 sent to build them from its place behind: a standing tower of the creep under CR_REPAIR of its hp
--                 first (ai.repair), else the one going up, else the most damaged. Against a building CR_TOWERS_B
--                 towers at most (the second when the first stands and the target still does after CR_SECOND),
--                 against a tower CR_TOWERS_T, each laid when the one before stands. A tower of it standing idle or on
--                 another building is ordered onto the target (ai.attack).
--   THE END     - the target gone: the next look picks the next target, nearest its buildings - its new towers among
--                 them, so it creeps on. Dropped (the target noted for CR_BAD_WAIT): enemy soldiers seen within CR_FOE_R
--                 of the spot while none of its towers there stands finished, as many of the owner's citizens as it has
--                 crew, fewer than CR_CREW_LOW of the crew left, no tower laid within CR_LAY_WAIT, its towers destroyed,
--                 CR_RELAY foundations destroyed, the target no worse CR_HIT_WAIT after a tower of it stood, CR_GIVEUP
--                 since it began; its towers going up are deleted then (the engine refunds them).
--   THE SCOUT   - no enemy building it knows: one free citizen walks to the nearest unexplored ground of its land
--                 (ai.unexplored; the look's place is kept, CR_SCOUT_WAIT).
--
-- DEVIATIONS AND GAPS (each marked below): the crew is held (ai.move/ai.build take it: the engine's economy leaves a
-- held citizen alone, where upstream's crew went back to work between two towers and the next look took it again); the
-- terrain is the engine's continent (ai.tile's continent stands in for bk_land_of and area_id); "out of every enemy
-- defence's reach" is ai.danger (the threat layer: seen armed units too, not only defences) and in the exposed case
-- (a tower taken from inside its reach) neither the spot's nor the crew's place is tested against the other defences
-- (ai.danger carries the target tower's own reach); a target out of sight is standing as last seen (upstream's
-- unit_by_id knew; the tile's visibility is the test here) and "the towers don't hurt it" is left out while it is out
-- of sight; a remembered building's foundation is taken for a finished one (the memory carries no built flag; a
-- foundation seen now is skipped); the crew's count and the citizen guard are the seen ones (ai.seen); "workforce" is
-- the view's citizens. The scout is ai.unexplored, not the territories' last-looked ages.
local S = require("ageofai.settings")
local V = require("lib.view")
local TV = require("ageofai.territories")

local M = {}

-- creep.c's constants
local CR_EVERY = 4000
local CR_REACH = 1000       -- no cap: the nearest first
local CR_FOE_R = 12
local CR_IN = 3             -- the spot this far inside its tower's range (tenths of a tile: creep.c 0.3)
local CR_BEHIND = 20        -- the crew's place: this far past the spot's far edge (2.0 tiles)
local CR_REPAIR = 60        -- percent: a standing tower under this share of its hp is repaired before the next
local CR_EDGE = 10          -- in a tower's reach: the spot this far past its own range at most (1.0 tile)
local CR_AT = 2             -- a crew member this near its place is there
local CR_CREW_TOWER = 12
local CR_CITS = 2
local CR_FOES = 2           -- THE LAST SOLDIERS: this many of the enemy's soldiers are taken on by the crew
local CR_RELAY = 3
local CR_CREW_MIN = 2
local CR_TOWERS_B = 2
local CR_TOWERS_T = 4
local CR_SECOND = 90000
local CR_LAY_WAIT = 120000  -- from the start: the walk, then the foundation
local CR_THERE = 5
local CR_GIVEUP = 900000
local CR_HIT_WAIT = 60000   -- a tower of it standing this long and the target no worse: dropped
local CR_BAD = 16
local CR_BAD_WAIT = 600000
local CR_AGAIN = 20000      -- after a creep ends or is dropped, the next look for a target
local CR_TRIES = 12         -- targets tried for a spot a look
local CR_STARVED = 180000   -- THE STALEMATE: this long without a citizen gathering food
local CR_CREW_LOW = 3       -- THE LAST CITIZENS: a crew this small against a building when no more are left
local CR_CREW_EXPOSED = 6   -- ... and in a tower's reach
local CR_SCOUT_WAIT = 180000   -- a scout's walk to one place at most
local CR_SEND = 30000       -- the crew's repair order given again this often
local CR_GUARD_MEMO = 60000 -- a candidate that failed the guard is not looked at again this soon
local CR_OWN = 1024
local CR_CAND = 512
local CR_FREE = 512

-- the tower classes (econ.lua's and territories.lua's; the newest the player can build is the creep's)
local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function isqrt(v)
    if v <= 0 then
        return 0
    end
    local r = math.floor(math.sqrt(v))
    while r * r > v do
        r = r - 1
    end
    while (r + 1) * (r + 1) <= v do
        r = r + 1
    end
    return r
end

local function say(fmt, ...)
    print("creep " .. string.format(fmt, ...))
end

-- the engine's verbs the module needs: without them it is inert, not an error
local function verbs_ok()
    return type(ai.units) == "function" and type(ai.get) == "function" and type(ai.unit) == "function"
        and type(ai.seen) == "function" and type(ai.memory) == "function" and type(ai.class) == "function"
        and type(ai.tile) == "function" and type(ai.danger) == "function" and type(ai.can_place) == "function"
        and type(ai.move) == "function" and type(ai.build) == "function" and type(ai.repair) == "function"
        and type(ai.attack) == "function" and type(ai.delete) == "function" and type(ai.release) == "function"
        and type(ai.unexplored) == "function"
end

local CLASS = {}

local function cls(t)
    if t == nil then
        return nil
    end
    local c = CLASS[t]
    if c == nil then
        c = ai.class(t) or false
        CLASS[t] = c
    end
    return c or nil
end

local function is_wall(t)
    local s = t or ""
    return string.find(s, "wall", 1, true) ~= nil or string.find(s, "gate", 1, true) ~= nil
end

-- creep.c defence_stats: an armed building (a tower)
local function is_defence(t)
    local c = cls(t)
    return c ~= nil and c.category == "building" and (c.attack or 0) > 0
end

local function tower_class(view)
    for k = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[k]) ~= nil then
            return TOWER_CLASSES[k]
        end
    end
    return nil
end

local function range_of(t)
    local c = ai.class(t)
    return c ~= nil and (c.range or 0) // 100 or 0
end

-- how far a tower of t hits what it does not see: its range, but no farther than its sight (creep.c cr_reach)
local function reach_of(t)
    local c = ai.class(t)
    if c == nil then
        return 0
    end
    local rng = (c.range or 0) // 100
    local sight = (c.sight or 0) // 100
    return sight > 0 and sight < rng and sight or rng
end

local function land_of(x, y)
    local t = ai.tile(x, y)
    return t ~= nil and (t.continent or 0) or 0
end

-- an unbuilt building seen now (a foundation: creep.c's "a foundation is no target")
local function seen_unbuilt(id)
    local u = ai.unit(id)
    return u ~= nil and u.built == false and u.pending ~= true
end

-- ---------------------------------------------------------------------------------------------------------------
-- The memory

local function state(memory)
    memory.creep = memory.creep or {}
    local c = memory.creep
    c.tow = c.tow or {}
    c.crew = c.crew or {}
    c.bad = c.bad or {}
    c.guard = c.guard or {}
    c.started, c.laid, c.down, c.dropped, c.sends, c.scouted = c.started or 0, c.laid or 0, c.down or 0,
        c.dropped or 0, c.sends or 0, c.scouted or 0
    return c
end

local function reset(c, now)
    local keep = {started = c.started, laid = c.laid, down = c.down, dropped = c.dropped, sends = c.sends,
                  scouted = c.scouted}
    for k in pairs(c) do
        c[k] = nil
    end
    for k, v in pairs(keep) do
        c[k] = v
    end
    c.tow, c.crew, c.bad, c.guard = {}, {}, {}, {}
    c.t, c.fed_t, c.end_t = 0, nil, nil
    c.start_t = now > 0 and now or 1
end

-- ---------------------------------------------------------------------------------------------------------------
-- The readers

local function foes_seen(x, y, r)
    return #ai.seen{enemy = true, category = "soldier", near = {x, y, r}}
end

local function cits_seen(x, y, r)
    return #ai.seen{enemy = true, category = "citizen", near = {x, y, r}}
end

-- alive: a tower or foundation of the creep's (its id, or the engine's forwarding, finds it)
local function alive(id)
    local u = ai.unit(id)
    return u ~= nil and ((u.hp or 0) > 0 or u.pending == true)
end

local function finished(id)
    local u = ai.unit(id)
    return u ~= nil and u.built == true
end

local function hp_of(id)
    local u = ai.unit(id)
    return u ~= nil and (u.hp or 0) or 0
end

local function hp_max_of(id)
    local u = ai.unit(id)
    if u == nil then
        return 0
    end
    if (u.hp_max or 0) > 0 then
        return u.hp_max
    end
    local c = ai.class(u.type)
    return c ~= nil and (c.hp or 0) or 1
end

-- the target still stands: seen now (its hit points), else the tile out of sight (the last sight of it), else gone
local function target_alive(c)
    local u = ai.unit(c.tid)
    if u ~= nil then
        return (u.hp or 0) > 0, (u.hp or 0), true
    end
    local t = ai.tile(c.tp_x, c.tp_y)
    if t ~= nil and t.visible == true then
        return false, 0, true   -- seen and not there
    end
    return true, c.hp_stood or 0, false   -- out of sight: as last seen
end

-- creep.c cr_is_bad
local function is_bad(c, id, now)
    for _, e in ipairs(c.bad) do
        if e.id == id and now >= e.t and now - e.t < CR_BAD_WAIT then
            return true
        end
    end
    return false
end

local function add_bad(c, id, now)
    while #c.bad >= CR_BAD do
        table.remove(c.bad, 1)
    end
    c.bad[#c.bad + 1] = {id = id, t = now > 0 and now or 1}
end

-- ---------------------------------------------------------------------------------------------------------------
-- The spot (creep.c cr_spot) and the crew's place behind it

-- a tile at the edge of its reach of the target, out of every enemy defence's reach it knows, on the crew's land,
-- nearest its own buildings (or, in the target tower's reach, farthest first); the crew's place behind it
local function spot(c, ttype, tx, ty, target_type, skip, land, own)
    local td, gd = ai.class(ttype), ai.class(target_type)
    if td == nil or gd == nil then
        return nil
    end
    local rng = (td.range or 0) // 100
    if rng <= 2 or rng > 30 then
        return nil
    end
    local sight = (td.sight or 0) // 100
    local half = (gd.size or 1) - 2
    if half < 0 then
        half = 0
    end
    local half10 = half * 5
    local r1
    if skip then
        local e = rng + CR_EDGE // 10
        r1 = (e < sight and e or sight) * 10
    else
        r1 = sight * 10 + half10 - CR_IN
    end
    local r0 = r1 - 15
    local own_half10 = (td.size or 1) * 5
    local box = r1 // 10 + 1
    local found, bsc, bx, by, bbx, bby = false, nil, nil, nil, nil, nil
    for ax = -box, box do
        for ay = -box, box do
            local d10 = isqrt((ax * ax + ay * ay) * 100)
            if d10 <= r1 and d10 >= r0 and d10 >= 10 then
                local qx, qy = tx + ax, ty + ay
                local ok = true
                local tq = ai.tile(qx, qy)
                if tq == nil or (land ~= 0 and (tq.continent or 0) ~= land) then
                    ok = false
                elseif not skip and (ai.danger(qx, qy) or 0) > 0 then
                    ok = false
                elseif not ai.can_place(ttype, qx, qy) then
                    ok = false
                end
                if ok then
                    local k = own_half10 + CR_BEHIND
                    local px, py = qx + ax * k // d10, qy + ay * k // d10
                    local tp = ai.tile(px, py)
                    if tp == nil or (land ~= 0 and (tp.continent or 0) ~= land) then
                        ok = false
                    elseif not skip and (ai.danger(px, py) or 0) > 0 then
                        ok = false
                    end
                    if ok then
                        local nearest = nil
                        for _, o in ipairs(own) do
                            local e = d2(o.x, o.y, qx, qy)
                            if nearest == nil or e < nearest then
                                nearest = e
                            end
                        end
                        nearest = nearest or 0
                        local sc = nearest
                        if skip then
                            sc = (r1 - d10) * 2 * 0x100000 + (nearest < 0xFFFFF and nearest or 0xFFFFF)
                        end
                        if not found or sc < bsc then
                            found, bsc, bx, by, bbx, bby = true, sc, qx, qy, px, py
                        end
                    end
                end
            end
        end
    end
    if not found then
        return nil
    end
    return bx, by, bbx, bby
end

-- a crew member to its place behind the spot; true when it is there
local function to_place(c, u)
    if d2(u.x, u.y, c.behind_x, c.behind_y) <= CR_AT * CR_AT then
        return true
    end
    if u.action == "move" and u.tx ~= nil and math.abs(u.tx - c.behind_x) < 2 and math.abs(u.ty - c.behind_y) < 2 then
        return false
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- The end of a creep

local function release(c)
    if #c.crew > 0 then
        ai.release(c.crew)
    end
    if #c.tow > 0 then
        ai.release(c.tow)
    end
end

local function end_(c, now, why, log)
    if why ~= nil then
        add_bad(c, c.tid, now)
        c.dropped = c.dropped + 1
        -- its towers going up removed, refunded (creep.c: unit_destroy)
        local going = {}
        for _, id in ipairs(c.tow) do
            if not finished(id) and alive(id) then
                going[#going + 1] = id
            end
        end
        if #going > 0 then
            ai.delete(going)
        end
    else
        c.down = c.down + 1
    end
    if log then
        say("the creep on %s at %d,%d %s after %d s - %d towers laid there (%d creeps, %d targets down, %d dropped, why: %s)",
            tostring(c.ttype), c.tp_x or -1, c.tp_y or -1, why ~= nil and "dropped" or "done, its target is gone",
            (now - (c.start_t or now)) // 1000, #c.tow, c.started, c.down, c.dropped, tostring(why))
    end
    release(c)
    c.target, c.tid, c.tow, c.crew, c.job = nil, nil, {}, {}, nil
    c.end_t = now > 0 and now or 1
end

-- ---------------------------------------------------------------------------------------------------------------
-- The creep going on (creep.c creep_tick's first half)

local function ongoing(view, c, now, ttype, log)
    -- its towers and its crew as they are
    local tow = {}
    for _, id in ipairs(c.tow) do
        if alive(id) then
            tow[#tow + 1] = id
        end
    end
    c.tow = tow
    local crew = {}
    for _, id in ipairs(c.crew) do
        if alive(id) then
            crew[#crew + 1] = id
        end
    end
    c.crew = crew
    local alive_t, hp, seen = target_alive(c)
    local stood, going = 0, nil
    for k, id in ipairs(c.tow) do
        if finished(id) then
            stood = stood + 1
        else
            going = k
        end
    end
    if stood > 0 and c.stood_t == nil then
        c.stood_t = now > 0 and now or 1
        c.hp_stood = seen and hp or nil
    end
    -- the owner's citizens in its towers' reach: they shoot those first - the target's minute starts again
    if c.stood_t ~= nil and cits_seen(c.spot_x, c.spot_y, reach_of(ttype) + 2) > 0 then
        c.stood_t = now > 0 and now or 1
        c.hp_stood = seen and hp or nil
    end
    if not alive_t then
        end_(c, now, nil, log)
        return
    end
    local why = nil
    if #c.crew < CR_CREW_MIN and stood == 0 then
        why = "its crew is gone"
    elseif stood == 0 and foes_seen(c.spot_x, c.spot_y, CR_FOE_R) > CR_FOES then
        why = "enemy soldiers came before a tower stood"
    elseif stood == 0 and #c.crew < (c.crew0 or 0) and cits_seen(c.spot_x, c.spot_y, CR_FOE_R) >= #c.crew then
        why = "the owner's citizens there outnumber its crew"
    elseif #c.tow == 0 and (c.lays or 0) >= CR_RELAY then
        why = "its foundations were destroyed"
    elseif #c.tow == 0 and c.laid_t ~= nil and now >= c.laid_t and now - c.laid_t >= CR_LAY_WAIT then
        why = (c.lays or 0) > 0 and "its towers were destroyed" or "no tower laid"
    elseif stood > 0 and c.stood_t ~= nil and now >= c.stood_t and now - c.stood_t >= CR_HIT_WAIT
            and seen and c.hp_stood ~= nil and hp >= c.hp_stood then
        why = "its towers don't hurt it"
    elseif now >= (c.start_t or now) and now - (c.start_t or now) >= CR_GIVEUP then
        why = "it took too long"
    end
    if why ~= nil then
        end_(c, now, why, log)
        return
    end
    -- THE WALK: to the place behind the spot, before the first foundation
    if (c.lays or 0) == 0 then
        local there = 0
        for _, id in ipairs(c.crew) do
            local u = ai.unit(id)
            if u ~= nil and d2(u.x, u.y, c.behind_x, c.behind_y) <= CR_THERE * CR_THERE then
                there = there + 1
            end
        end
        if 2 * there < #c.crew then
            local go = {}
            for _, id in ipairs(c.crew) do
                local u = ai.unit(id)
                if u ~= nil and not to_place(c, u) then
                    go[#go + 1] = id
                end
            end
            if #go > 0 then
                ai.move(go, c.behind_x, c.behind_y, {forced = true})
            end
            return
        end
    end
    -- the next tower: none going up, the last one stands, fewer than the most for its target
    local most = c.tower and CR_TOWERS_T or CR_TOWERS_B
    if going == nil and #c.tow < most and ttype ~= nil and V.cost(view, ttype) ~= nil
            and (#c.tow == 0 or (stood == #c.tow and (c.tower or (c.stood_t ~= nil and now >= c.stood_t
                and now - c.stood_t >= CR_SECOND)))) then
        local at_x, at_y, bh_x, bh_y = c.spot_x, c.spot_y, c.behind_x, c.behind_y
        local skip = c.exposed and c.tid or nil
        if #c.tow > 0 then   -- the next beside the first
            at_x, at_y, bh_x, bh_y = spot(c, ttype, c.tp_x, c.tp_y, c.ttype, skip, land_of(c.spot_x, c.spot_y),
                {{x = c.spot_x, y = c.spot_y}})
        end
        if at_x ~= nil then
            local code, reason, _, id = ai.build(c.crew, ttype, at_x, at_y)
            if code == "ok" and id ~= nil then
                c.behind_x, c.behind_y = bh_x, bh_y
                c.tow[#c.tow + 1] = id
                going = #c.tow
                c.lays = (c.lays or 0) + 1
                c.laid = c.laid + 1
                c.laid_t = now > 0 and now or 1
                c.sends = c.sends + 1
                if log then
                    say("tower %s laid at %d,%d against %s at %d,%d (%s, tower %d of the creep, %d of its crew)", ttype,
                        at_x, at_y, tostring(c.ttype), c.tp_x, c.tp_y, c.tower and "a defence" or "a building",
                        #c.tow, #c.crew)
                end
            end
        end
    end
    -- ITS TOWERS ON THE TARGET: one standing idle or on another building is ordered onto it
    for _, id in ipairs(c.tow) do
        if finished(id) then
            local u = ai.unit(id)
            if u ~= nil then
                local on = u.target
                local other = on ~= nil and on ~= c.tid and ai.unit(on) or nil
                local fam = other ~= nil and cls(other.type) or nil
                if u.action ~= "attack" or (on ~= nil and on ~= c.tid and fam ~= nil and fam.category == "building") then
                    ai.attack({id}, c.tid)
                end
            end
        end
    end
    -- the crew: onto a tower of its standing under CR_REPAIR of its hp, else the tower going up, else the most damaged
    local job, worst = nil, CR_REPAIR
    for _, id in ipairs(c.tow) do
        if finished(id) then
            local max = hp_max_of(id)
            local f = max > 0 and hp_of(id) * 100 // max or 100
            if f < worst then
                worst, job = f, id
            end
        end
    end
    if job == nil and going ~= nil then
        job = c.tow[going]
    end
    if job == nil then
        worst = 95
        for _, id in ipairs(c.tow) do
            local max = hp_max_of(id)
            local f = max > 0 and hp_of(id) * 100 // max or 100
            if f < worst then
                worst, job = f, id
            end
        end
    end
    if job ~= nil and not finished(job) then
        job = nil   -- the build order binds the crew to its foundation already
    end
    if job ~= nil and #c.crew > 0 and (c.job ~= job or now - (c.send_t or 0) >= CR_SEND) then
        c.job = job
        c.send_t = now > 0 and now or 1
        if ai.repair(c.crew, job) == "ok" then
            c.sends = c.sends + 1
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- A new creep (creep.c creep_tick's second half)

local function begin(view, memory, c, now, ttype, log)
    if (c.end_t ~= nil and now >= c.end_t and now - c.end_t < CR_AGAIN) then
        return
    end
    if c.fed_t == nil or now < c.fed_t or now - c.fed_t < CR_STARVED then
        return   -- THE STALEMATE: food is still gathered
    end
    if (view.citizens or 0) < CR_CREW_LOW then
        return
    end
    local cost = V.cost(view, ttype)
    if cost == nil then
        return
    end
    for _, r in ipairs(V.resources) do
        if (cost.cost[r] or 0) > 0 and (view.stock[r] or 0) < cost.cost[r] then
            return
        end
    end
    -- its own finished buildings (the creep starts from its ground and goes on from its towers)
    local own = {}    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil and u.x >= 0 and not is_wall(u.type) and #own < CR_OWN then
            own[#own + 1] = {x = u.x, y = u.y}
        end
    end
    if #own == 0 then
        return
    end
    -- the guard's memo: what failed lately is looked at again only after CR_GUARD_MEMO (the memory small)
    local keys = ai.keys(c.guard)
    if #keys > 64 then
        for _, id in ipairs(keys) do
            local t = c.guard[id]
            if t == nil or now >= t and now - t >= CR_GUARD_MEMO then
                c.guard[id] = nil
            end
        end
    end
    -- the crew's citizens: the free ones (gathering or idle)
    local free = {}
    local ids = ai.units{category = "citizen", limit = CR_FREE}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "action"})
        for k = 1, #ids do
            local a = cols.action[k]
            if cols.x[k] ~= nil and (a == "gather" or a == "idle") then
                free[#free + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], land = land_of(cols.x[k], cols.y[k])}
            end
        end
    end
    if #free < CR_CREW_LOW then
        return
    end
    -- THE TARGET: the nearest known enemy building to one of its own
    local cand = {}
    for _, m in ipairs(ai.memory{buildings = true}) do
        if not is_wall(m.type) and not is_bad(c, m.id, now) and #cand < CR_CAND then
            local d = nil
            for _, o in ipairs(own) do
                local e = d2(o.x, o.y, m.x, m.y)
                if d == nil or e < d then
                    d = e
                end
            end
            if d ~= nil and d <= CR_REACH * CR_REACH then
                cand[#cand + 1] = {id = m.id, type = m.type, x = m.x, y = m.y, d = d}
            end
        end
    end
    if #cand == 0 then
        -- THE SCOUT: no enemy building known - a free citizen to the nearest unexplored ground of its land
        if (c.scout == nil or not alive(c.scout)) or (c.scout_t ~= nil and now >= c.scout_t
                and now - c.scout_t >= CR_SCOUT_WAIT) then
            local from = free[1]
            local sx, sy = ai.unexplored(from.x, from.y, {land = from.land, away = {view.town_x, view.town_y}})
            if sx ~= nil then
                local best, bd = nil, nil
                for _, f in ipairs(free) do
                    local d = d2(f.x, f.y, sx, sy)
                    if bd == nil or d < bd then
                        best, bd = f, d
                    end
                end
                if best ~= nil and ai.move({best.id}, sx, sy, {forced = true}) == "ok" then
                    c.scout, c.scout_t = best.id, now > 0 and now or 1
                    c.scout_x, c.scout_y = sx, sy
                    c.scouted = c.scouted + 1
                    if log then
                        say("no enemy building known - a scout to %d,%d (%d so far)", sx, sy, c.scouted)
                    end
                end
            end
        end
        return
    end
    table.sort(cand, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.id < b.id
    end)
    local tries = 0
    for _, m in ipairs(cand) do
        if tries >= CR_TRIES then
            break
        end
        tries = tries + 1
        local memo = c.guard[m.id]
        if memo ~= nil and now >= memo and now - memo < CR_GUARD_MEMO then
            -- (the guard was not looked at lately)
        elseif seen_unbuilt(m.id) or foes_seen(m.x, m.y, CR_FOE_R) > CR_FOES or cits_seen(m.x, m.y, CR_FOE_R) > CR_CITS
                or ((TV.foes(TV.terr_at(m.x, m.y) or 0) or 0) > CR_FOES) then
            c.guard[m.id] = now > 0 and now or 1   -- guarded
        else
            -- its land: the nearest free citizen's
            local nk = nil
            for k, f in ipairs(free) do
                if nk == nil or d2(f.x, f.y, m.x, m.y) < d2(free[nk].x, free[nk].y, m.x, m.y) then
                    nk = k
                end
            end
            local land = nk ~= nil and free[nk].land or 0
            local at_x, at_y, bh_x, bh_y = spot(c, ttype, m.x, m.y, m.type, nil, land, own)
            local exposed = false
            if at_x == nil then
                if not is_defence(m.type) or range_of(ttype) < range_of(m.type)
                        or #free < CR_CREW_EXPOSED then
                    goto continue
                end
                at_x, at_y, bh_x, bh_y = spot(c, ttype, m.x, m.y, m.type, m.id, land, own)
                exposed = true
                if at_x == nil then
                    goto continue
                end
            end
            local want = #free < CR_CREW_TOWER and #free or CR_CREW_TOWER
            if want < CR_CREW_LOW or (view.citizens or 0) < want
                    or foes_seen(at_x, at_y, CR_FOE_R) > CR_FOES or cits_seen(at_x, at_y, CR_FOE_R) > CR_CITS then
                goto continue
            end
            -- the crew: the nearest to the spot, on its land
            local pick = {}
            local used = {}
            while #pick < want do
                local bk, bd = nil, nil
                for k, f in ipairs(free) do
                    if not used[k] and (land == 0 or f.land == land) then
                        local d = d2(f.x, f.y, at_x, at_y)
                        if bd == nil or d < bd then
                            bk, bd = k, d
                        end
                    end
                end
                if bk == nil then
                    break
                end
                used[bk] = true
                pick[#pick + 1] = free[bk].id
            end
            if #pick < CR_CREW_LOW then
                goto continue
            end
            c.target, c.tid = m.id, m.id
            c.ttype, c.tower, c.exposed = m.type, is_defence(m.type), exposed
            c.lays = 0
            c.tp_x, c.tp_y = m.x, m.y
            c.spot_x, c.spot_y, c.behind_x, c.behind_y = at_x, at_y, bh_x, bh_y
            c.start_t = now > 0 and now or 1
            c.laid_t, c.stood_t, c.hp_stood = c.start_t, nil, 0
            c.tow, c.crew, c.crew0, c.job, c.send_t = {}, {}, #pick, nil, nil
            for _, id in ipairs(pick) do
                c.crew[#c.crew + 1] = id
            end
            c.started = c.started + 1
            if log then
                say("a creep on %s at %d,%d (%s, %.0f tiles from its nearest building) from %d,%d, its crew behind at %d,%d, %d citizens, tower %s",
                    tostring(c.ttype), m.x, m.y, not c.tower and "a building" or (exposed and "a defence, in its reach"
                    or "a defence, from beyond its reach"), math.sqrt(m.d), at_x, at_y, bh_x, bh_y, #c.crew, ttype)
            end
            return
        end
        ::continue::
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (creep_tick)

function M.step(view, memory, orders)
    if S.tower_creep ~= true or not verbs_ok() then
        return   -- off (upstream's default), or an engine without the verbs
    end
    local c = state(memory)
    local now = view.time
    if c.start_t == nil or now < c.start_t then
        reset(c, now)   -- a new game (or a save from an older time)
    end
    if c.t ~= nil and now >= c.t and now - c.t < CR_EVERY then
        return
    end
    c.t = now > 0 and now or 1
    local log = S.creep_log == true
    if c.fed_t == nil or (view.gatherers.food or 0) > 0 then
        c.fed_t = now > 0 and now or 1   -- the first look: the stalemate's clock starts
    end
    local ttype = tower_class(view)
    if ttype == nil then
        return
    end
    if c.target ~= nil then
        ongoing(view, c, now, ttype, log)
    else
        begin(view, memory, c, now, ttype, log)
    end
    if log and (c.log_t == nil or (now >= c.log_t and now - c.log_t >= 60000)) then
        c.log_t = now > 0 and now or 1
        say("tower %s | creeps %d, targets down %d, dropped %d, towers laid %d | a creep on %s at %d,%d, %d towers, %d crew | food gathered %d s ago",
            ttype, c.started, c.down, c.dropped, c.laid, c.target ~= nil and tostring(c.ttype) or "none",
            c.target ~= nil and c.tp_x or -1, c.target ~= nil and c.tp_y or -1, #c.tow, #c.crew,
            (c.fed_t ~= nil and now >= c.fed_t) and (now - c.fed_t) // 1000 or -1)
    end
end

-- a new game (creep_new_game): nothing to give back
function M.clear(memory)
    memory.creep = nil
end

return M
