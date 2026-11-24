-- Age of AI port: upstream dll/ai/holy.c (THE HOLY: the priests' conversions and the prophets' calamities as their
-- own strike category, with the hit and run; UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.holy"), called from main.lua's step after ageofai.army's step; settings.lua's
-- `holy` (default off) switches it on.
--
-- The player 2026-11-16: "the ai needs to make use of religuess units, espacialy in the early ages they are very
-- powerfull offensive units. they probably should get their own cattegory like siege. prophets are spezial strike
-- units, they can acompany and push as siege (earthquake) or can strike on their own as hit and run, sneak in, spawn
-- a desaster in unprotected territory and run away again. priest can be used to snatch away citecens away from the
-- pack or lone soilders".
--
-- Neither is one of soldiers (a priest is of the class Land Priest, a prophet no land soldier): they are this
-- module's alone. Ever 2 s for each AI, with `holy` on:
--   TRAINING  - with a finished Temple making nothing: prophets up to HL_PROPHETS (one more every HL_PROPHET_EPOCHS
--               epochs; by the cap: HL_CAP_PROPHET), priests up to HL_PRIESTS and by the cap (HL_CAP_PRIEST places of
--               it, one at least), one at a time, when the bank holds the price (HL_BANK times it; nothing is saved
--               for them: the priority list knows them not) - a `train` order, so the verbs' pool makes it.
--   PROPHETS  - a power cast where it hits most (the calamities' own costs: earthquake 50, volcano 60, fire 75,
--               hurricane, plague, malaria 100): enemy buildings (HL_BUILDINGS within HL_CLUSTER of one: earthquake,
--               else fire, else volcano), enemy soldiers (HL_SOLDIERS: plague, else malaria), citizens (HL_CITIZENS:
--               plague, else malaria), ships (HL_SHIPS: hurricane) - what it knows within HL_REACH of it; an enemy
--               tower first (THE TOWERS), the earthquake a few tiles before it. The first prophet goes with the force
--               while it meets the enemy: HL_BEHIND tiles behind that place, as the siege. The others, ready for a
--               cast: A STRIKE - the nearest enemy building it knows within HL_STRIKE with no enemy soldiers near
--               (HL_STRIKE_GUARD), under no enemy defence it knows (ai.danger) or a tower itself, is walked to with up
--               to HL_ESCORT soldiers beside it, cast at, and it walks home again with them - AWAY AT ONCE: its cast
--               over, it turns home that look; hurt, or a cast it never got off (HL_CAST_LONG), it turns home too and
--               waits HL_CAST_READY before the next.
--   PRIESTS   - a conversion (the game's attack order: a priest's attack converts) of an enemy citizen or soldier
--               within HL_PRIEST_LOOK that stands alone (no other enemy soldier within HL_ALONE of it) and under no
--               danger (ai.danger), citizens first, the nearest, none near enemy soldiers that outnumber them. Idle:
--               with the force (HL_BEHIND behind where it meets the enemy), else at home. Hurt with enemies by: home.
-- THE LOST: a fall in the count is a loss (killed or converted); HL_LOSS_MAX of a kind lost within HL_LOSS_WINDOW -
-- none of that kind is trained until the oldest of them is out of it.
--
-- DEVIATIONS AND GAPS (each marked below): the prophet's POWER and a calamity's COST are not in the API (ai.unit has
-- no power, ai.techs no calamity cost) nor is the power's own check (calamity_check): a cast is ordered by the
-- module's own ready clock (HL_CAST_READY after a cast, its power's regeneration; after a cast that never happened
-- as well) and the engine decides; a cast that never happens (no power, an enemy Temple's protection) is given up at
-- HL_CAST_LONG and the prophet goes home (upstream read the power and skipped the target). "Its goal is a cast" is
-- the unit's action `cast` (no goal handle in the API). The kinds come from the class category and the class names
-- (ai.class has no family). The escort is the engine's idle soldiers, not the force's (upstream stations.c sn_escort),
-- it fights what it meets (ai.move is the attack-move: upstream's escort fought only with enemies by the prophet,
-- HL_ESC_FOE), and "the prophet stands for it" is an ai.stop every 2 s while the wait lasts. The ferry across the
-- water (ferry.c PROPHETS ACROSS) is not ported: prophets and priests stay on their land mass (ai.tile's continent
-- stands in for bk_land_of). THE REFUSED counts the engine's refusal of the order alone (upstream counted the power's
-- check). The log is one line a minute (upstream: g_autotest only).
local S = require("ageofai.settings")
local G = require("lib.groups")
local V = require("lib.view")

local M = {}

-- holy.c's constants
local HL_EVERY = 2000
local HL_TRAIN_EVERY = 20000
local HL_PROPHETS = 2           -- one with the force, one on strikes
local HL_PROPHET_EPOCHS = 4
local HL_PROPHETS_MAX = 4
local HL_PRIESTS = 3
local HL_CAP_PRIEST = 40        -- a priest for this many places of its population cap (one at least)
local HL_CAP_PROPHET = 150      -- ... a prophet (5 places each) for this many (none below)
local HL_BANK = 1
local HL_LOSSES = 8
local HL_LOSS_WINDOW = 600000   -- THE LOST: HL_LOSS_MAX of a kind lost within this - none of that kind meanwhile
local HL_LOSS_MAX = 3
local HL_ESCORT = 4             -- AN ESCORT: soldiers beside a prophet on a strike
local HL_ESC_NEAR = 6           -- ... the prophet walks on while half of them are within this of it
local HL_ESC_WAIT = 90000       -- ... and waits for them this long at most from the strike's start
local HL_ESC_AGAIN = 10000      -- the escort's order given again this often
local HL_CAUGHT = 4             -- A PARTING GIFT: this many enemy soldiers within HL_CAUGHT_R
local HL_CAUGHT_R = 6
local HL_HOME_R = 15            -- AT HOME: within this of a town of its
local HL_DEFEND = 25            -- ... the enemy soldiers within this of it
local HL_DEFEND_N = 4           -- ... this many in a group
local HL_FINISH_R = 10          -- THE POWERS: its own soldiers and towers within this of a group ...
local HL_FINISH_N = 5           -- ... this many - malaria (they finish them off)
local HL_VOLCANO_EVERY = 5      -- ... buildings: a volcano first one look in this many
local HL_SHIPS = 2              -- enemy ships in a group: a hurricane
local HL_REACH = 14
local HL_TOWER_LOOK = 16        -- THE TOWERS: an enemy tower within this
local HL_QUAKE_OFF = 3          -- ... gets the earthquake this many tiles before it
local HL_CLUSTER = 4
local HL_BUILDINGS = 2
local HL_SOLDIERS = 6
local HL_CITIZENS = 5
local HL_BEHIND = 8
local HL_STRIKE = 70
local HL_STRIKE_TRIES = 4       -- the nearest struck candidates tested a look (the raiders' scan costs)
local HL_STRIKE_GUARD = 10      -- enemy soldiers within this of a building make it guarded
local HL_STRIKE_OLD = 300000    -- a strike not cast this long after it was taken up is called off
local HL_PRIEST_LOOK = 12
local HL_ALONE = 5
local HL_HURT = 60              -- percent of its hit points (holy.c 0.6)
local HL_UNITS = 64
local HL_ORDER_WAIT = 20000     -- A GIVEN ORDER: not given again while the unit has a goal
local HL_CAST_READY = 60000     -- DEVIATION (the power is unreadable): the ready clock after a cast (its regeneration)
local HL_CAST_LONG = 240000     -- a cast order that has not become a cast by now is given up (the walk's cap)

-- THE POWERS: the powers' own techs in the data (holy.c's k_hl_tech: 1018 earthquake, 1013 fire, 1017 volcano,
-- 1019 plague, 1022 hurricane, 1016 malaria; the data calls malaria leprosy)
local QUAKE, FIRE, VOLCANO, PLAGUE, HURRICANE, MALARIA = 1, 2, 3, 4, 5, 6
local POWERS = {
    {tech = "zzzearthquake", name = "an earthquake"},
    {tech = "zzzfire_calamity", name = "a fire"},
    {tech = "zzzvolcano", name = "a volcano"},
    {tech = "zzzplague", name = "the plague"},
    {tech = "zzzhurricane", name = "a hurricane"},
    {tech = "zzzfamine_leprosy", name = "malaria"},
}
local HLP_N = #POWERS

-- THE POWERS' kinds of groups (holy.c HLK_*)
local KIND_BUILDINGS, KIND_SOLDIERS, KIND_CITIZENS, KIND_SHIPS = 1, 2, 3, 4
local KIND_NEED = {HL_BUILDINGS, HL_SOLDIERS, HL_CITIZENS, HL_SHIPS}
local KIND_ORDER = {KIND_SHIPS, KIND_SOLDIERS, KIND_CITIZENS, KIND_BUILDINGS}

-- the own buildings the holy uses (upstream's UT(B_*))
local TEMPLE = "b_temple"
local TOWNS = {b_town_center = true, b_capitol = true, b_settlement = true}

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
    print("holy " .. string.format(fmt, ...))
end

-- the engine's verbs the module needs: without them (an engine of an older stage, a data set without the powers) it
-- is inert, not an error
local function verbs_ok()
    return type(ai.cast) == "function" and type(ai.attack) == "function" and type(ai.seen) == "function"
        and type(ai.memory) == "function" and type(ai.units) == "function" and type(ai.get) == "function"
        and type(ai.techs) == "function" and type(ai.move) == "function" and type(ai.stop) == "function"
        and type(ai.tile) == "function" and type(ai.danger) == "function" and type(ai.class) == "function"
end

-- ---------------------------------------------------------------------------------------------------------------
-- The classes (holy.c's is_citizen_type / is_soldier / is_wall / defence_stats by the data's names and categories:
-- ai.class carries no family)

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
    t = t or ""
    return string.find(t, "wall", 1, true) ~= nil or string.find(t, "gate", 1, true) ~= nil
end

local function is_prophet(t)
    return t ~= nil and string.find(t, "prophet", 1, true) ~= nil
end

-- a priest, not a prophet and not a Priest Tower (the building priest is in the same category)
local function is_priest(t)
    return t ~= nil and t ~= "b_priest_tower" and string.find(t, "priest", 1, true) ~= nil
end

-- holy.c hl_kind: a building (not a wall's piece), a citizen, a ship, a land soldier
local function kind_of(t)
    local c = cls(t)
    if c == nil then
        return -1
    end
    if c.category == "building" then
        return is_wall(t) and -1 or KIND_BUILDINGS
    end
    if c.category == "citizen" then
        return KIND_CITIZENS
    end
    if c.category == "ship" then
        return KIND_SHIPS
    end
    if c.category == "soldier" then
        return KIND_SOLDIERS
    end
    return -1
end

-- holy.c defence_stats: an armed building (a tower)
local function is_defence(t)
    local c = cls(t)
    return c ~= nil and c.category == "building" and (c.attack or 0) > 0
end

-- hl_foe: an enemy player's, not nature's (ai.seen and ai.memory hold no nature and no ally)
local function foe_player(p)
    return p ~= nil and p > 0
end

-- ---------------------------------------------------------------------------------------------------------------
-- The small readers

local function land_of(x, y)
    local t = ai.tile(x, y)
    return t ~= nil and (t.continent or 0) or 0
end

-- the power's tech in the player's tree: holy.c's hl_cost/hl_cal read the calamity and its cost, which this API does
-- not expose; the node's existence is the same test ai.cast makes (a power the data has not is no power)
local function power_ok(k)
    local t = ai.techs{name = POWERS[k].tech}
    return t ~= nil and t[1] ~= nil
end

-- its soldiers and towers within r of (x, y) (holy.c hl_own_near): THE POWERS' malaria needs them to finish off
local function own_near(x, y, r)
    local n = #ai.units{category = "soldier", near = {x, y, r}}
    n = n + #ai.units{category = "building", armed = true, near = {x, y, r}}
    return n
end

-- enemy soldiers it sees within r of (x, y) (holy.c raiders_near)
local function raiders_near(x, y, r, limit)
    local f = {enemy = true, category = "soldier", near = {x, y, r}}
    if limit ~= nil then
        f.limit = limit
    end
    return #ai.seen(f)
end

-- the enemy units it sees within r, their records and kinds
local function enemies_near(px, py, r)
    local ids = ai.seen{enemy = true, near = {px, py, r}}
    local out = {}
    if #ids == 0 then
        return out
    end
    local c = ai.get(ids, {"type", "x", "y", "hp", "category", "inside"})
    for k = 1, #ids do
        local x, y, hp = c.x[k], c.y[k], c.hp[k]
        if x ~= nil and x >= 0 and y ~= nil and (hp or 0) > 0 and c.inside[k] == nil then
            out[#out + 1] = {id = ids[k], type = c.type[k], x = x, y = y, hp = hp,
                             category = c.category[k], kind = kind_of(c.type[k]), d = d2(x, y, px, py)}
        end
    end
    return out
end

-- the own towns (finished town centres, capitols and settlements, with their land mass), the newest temple, and
-- whether a temple of its makes something (holy.c holy_tick's first pass)
local function own_places()
    local town, temple, making = {}, nil, false
    local ids = ai.units{type = {"b_temple", "b_town_center", "b_capitol", "b_settlement"}, built = true}
    if #ids == 0 then
        return town, temple, making
    end
    local c = ai.get(ids, {"type", "x", "y", "queue"})
    for k = 1, #ids do
        local t, x, y = c.type[k], c.x[k], c.y[k]
        if x ~= nil and x >= 0 and y ~= nil then
            if t == TEMPLE then
                temple = {id = ids[k], x = x, y = y}
                if (c.queue[k] or 0) > 0 then
                    making = true   -- DEVIATION: a temple researching counts as making (upstream: object units only)
                end
            elseif TOWNS[t] ~= nil and #town < 64 then
                town[#town + 1] = {x = x, y = y, land = land_of(x, y)}
            end
        end
    end
    return town, temple, making
end

-- holy.c hl_behind: HL_BEHIND tiles from front towards from
local function behind(front_x, front_y, tx, ty)
    local dx, dy = tx - front_x, ty - front_y
    local d = isqrt(dx * dx + dy * dy)
    if d < 1 then
        return front_x, front_y
    end
    return front_x + dx * HL_BEHIND // d, front_y + dy * HL_BEHIND // d
end

-- the nearest town, on its own land first (holy.c's home: A PROPHET'S WAY's rule, without the ferry)
local function nearest_town(town, x, y, land)
    local best, bd, bs = nil, nil, nil
    for _, t in ipairs(town) do
        local d = d2(t.x, t.y, x, y)
        local same = land ~= 0 and t.land == land
        if best == nil or (same and not bs) or (same == bs and (bd == nil or d < bd)) then
            best, bd, bs = t, d, same
        end
    end
    if best == nil then
        return {x = x, y = y, land = land}
    end
    return best
end

-- holy.c hl_walk: on its way there (within 3 tiles, or its goal's place within 3 of it): nothing
local function walk(id, px, py, x, y, tx, ty)
    if d2(px, py, x, y) <= 9 then
        return false
    end
    if tx ~= nil and ty ~= nil and math.abs(tx - x) < 3 and math.abs(ty - y) < 3 then
        return false
    end
    return ai.move({id}, x, y, {forced = false}) == "ok"
end

-- holy.c hl_powers: the powers a group of that kind gets, in order (its own soldiers and towers near it give the
-- malaria first)
local function power_order(kind, finish, now)
    if kind == KIND_BUILDINGS then
        local v = (now // HL_EVERY) % HL_VOLCANO_EVERY == 0
        if v then
            return {VOLCANO, QUAKE, FIRE}
        end
        return {QUAKE, FIRE, VOLCANO}
    elseif kind == KIND_SOLDIERS then
        if finish then
            return {MALARIA, PLAGUE}
        end
        return {PLAGUE}
    elseif kind == KIND_CITIZENS then
        if finish then
            return {PLAGUE, MALARIA}
        end
        return {PLAGUE}
    end
    return {HURRICANE}
end

-- ---------------------------------------------------------------------------------------------------------------
-- The memory

local function state(memory)
    memory.holy = memory.holy or {casts = {}, t = 0}
    local h = memory.holy
    h.p = h.p or {}
    h.ord = h.ord or {}
    h.lost = h.lost or {{}, {}}
    h.casts = h.casts or {}
    h.last = h.last or {0, 0}
    return h
end

-- a new game (or a save from an older time): the counters again
local function reset(h, now)
    local casts = h.casts or {}
    h.t, h.train_t, h.log_t = 0, nil, nil
    h.p, h.ord = {}, {}
    h.lost = {{}, {}}
    h.last = {0, 0}
    for k = 1, HLP_N do
        if casts[k] == nil then
            casts[k] = 0
        end
    end
    h.casts = casts
    h.strikes, h.converts, h.trained_p, h.trained_c = 0, 0, 0, 0
    h.towers, h.refused, h.gifts, h.defends, h.aways, h.stuck = 0, 0, 0, 0, 0, 0
    h.start_t = now > 0 and now or 1
end

local function lost_add(h, k, n, now)
    local ring = h.lost[k]
    for _ = 1, n do
        if #ring >= HL_LOSSES then
            table.remove(ring, 1)
        end
        ring[#ring + 1] = now > 0 and now or 1
    end
end

-- hl_lost: those of kind k lost within HL_LOSS_WINDOW
local function lost_count(h, k, now)
    local n = 0
    for _, t in ipairs(h.lost[k]) do
        if now >= t and now - t < HL_LOSS_WINDOW then
            n = n + 1
        end
    end
    return n
end

-- ---------------------------------------------------------------------------------------------------------------
-- The orders

-- a cast (holy.c hl_cast): the power's tech by name, at a tile or on a unit. The engine's answer is the order's;
-- the power's own check (calamity_check) is not in the API - see the header.
local function cast_power(h, ps, u, k, x, y, target, now)
    if not power_ok(k) then
        h.refused = h.refused + 1
        return false
    end
    local code
    if target ~= nil then
        code = ai.cast({u.id}, POWERS[k].tech, target)
    else
        code = ai.cast({u.id}, POWERS[k].tech, x, y)
    end
    if code ~= "ok" then
        h.refused = h.refused + 1
        return false
    end
    h.casts[k] = (h.casts[k] or 0) + 1
    ps.ready = (now > 0 and now or 1) + HL_CAST_READY
    -- A GIVEN ORDER: the cast walks first; it is not given again while the unit has its goal
    h.ord[u.id] = {t = now > 0 and now or 1, seen = u.action == "cast"}
    u.action = "cast"
    return true
end

-- the first power of a kind that takes at a group's tile (holy.c's loop over out[]): true when one was cast
local function cast_kind(h, ps, u, kind, x, y, now, target)
    local finish = false
    if kind == KIND_SOLDIERS or kind == KIND_CITIZENS then
        finish = own_near(x, y, HL_FINISH_R) >= HL_FINISH_N
    end
    local order = power_order(kind, finish, now)
    for _, k in ipairs(order) do
        if cast_power(h, ps, u, k, x, y, target, now) then
            return true, k, finish
        end
    end
    return false
end

-- THE POWERS' groups of one kind among what it knows: the most of them within HL_CLUSTER of one another -> x, y, count
local function best_cluster(list, kind)
    local bx, by, bn = nil, nil, 0
    for _, a in ipairs(list) do
        if a.kind == kind then
            local c = 0
            for _, b in ipairs(list) do
                if b.kind == kind and d2(b.x, b.y, a.x, a.y) <= HL_CLUSTER * HL_CLUSTER then
                    c = c + 1
                end
            end
            if c > bn then
                bn, bx, by = c, a.x, a.y
            end
        end
    end
    return bx, by, bn
end

-- ---------------------------------------------------------------------------------------------------------------
-- The prophets

-- AN ESCORT (holy.c sn_escort): up to HL_ESCORT idle land soldiers beside a striking prophet, given again every
-- HL_ESC_AGAIN; the force's own are left to the force. ps.given / ps.by count for the wait.
local function escort(h, ps, px, py, now, force)
    if now - (h.esc_t or 0) < HL_ESC_AGAIN then
        return
    end
    h.esc_t = now > 0 and now or 1
    local ids = ai.units{category = "soldier", idle = true, near = {px, py, HL_ESC_NEAR + 6}, limit = HL_ESCORT * 3}
    local pick = {}
    for _, sid in ipairs(ids) do
        if not force[sid] and #pick < HL_ESCORT then
            pick[#pick + 1] = sid
        end
    end
    if #pick > 0 then
        ai.move(pick, px, py, {forced = false})
        ps.given = #pick
    end
    ps.by = #ai.units{category = "soldier", near = {px, py, HL_ESC_NEAR}}
end

local function prophet(h, u, now, town, known, fight, fx, fy, force, first, quake)
    local id, px, py = u.id, u.x, u.y
    local ps = h.p[id]
    if ps == nil then
        ps = {x = -1, y = -1, t = 0}
        h.p[id] = ps
    end
    local land = land_of(px, py)
    local home = nearest_town(town, px, py, land)
    local hurt = (u.hp_max or 0) > 0 and u.hp * 100 < HL_HURT * u.hp_max
    local ord = h.ord[id]
    local casting = u.action == "cast"
    local spent, stuck = false, false
    if ord ~= nil then
        if casting then
            ord.seen = true
        elseif ord.seen then
            spent = true   -- AWAY AT ONCE: it cast and the cast is over
        elseif now >= ord.t and now - ord.t >= HL_CAST_LONG then
            spent, stuck = true, true   -- the cast never happened (no power, or a target it cannot reach)
        end
    end
    if spent then
        h.ord[id] = nil
        ord, casting = nil, false
        if stuck then
            ps.ready = (now > 0 and now or 1) + HL_CAST_READY
            h.stuck = h.stuck + 1
            if S.holy_log then
                say("a prophet at %d,%d never cast - away home to %d,%d", px, py, home.x, home.y)
            end
        else
            h.aways = h.aways + 1
            if S.holy_log then
                say("a prophet at %d,%d has cast - away home to %d,%d", px, py, home.x, home.y)
            end
        end
        walk(id, px, py, home.x, home.y, u.tx, u.ty)
        return
    end
    ps.ready = ps.ready or 0
    local ready = not casting and ord == nil and now >= ps.ready
    local seen = nil
    if ready then
        seen = enemies_near(px, py, HL_DEFEND)
        -- A PARTING GIFT: HL_CAUGHT enemy soldiers within HL_CAUGHT_R of it, or hurt with any there
        local ns, sx, sy = 0, 0, 0
        for _, e in ipairs(seen) do
            if e.kind == KIND_SOLDIERS and e.d <= HL_CAUGHT_R * HL_CAUGHT_R then
                ns, sx, sy = ns + 1, sx + e.x, sy + e.y
            end
        end
        if ns >= HL_CAUGHT or (hurt and ns > 0) then
            local at_x, at_y = sx // ns, sy // ns
            local cast, k = cast_kind(h, ps, u, KIND_SOLDIERS, at_x, at_y, now)
            if cast then
                ps.x, ps.y, ps.t = -1, -1, 0
                h.gifts = h.gifts + 1
                casting = true
                if S.holy_log then
                    say("a prophet caught at %d,%d by %d enemy soldiers casts %s at %d,%d", px, py, ns,
                        POWERS[k].name, at_x, at_y)
                end
            end
        end
        -- AT HOME: within HL_HOME_R of a town, the biggest enemy group within HL_DEFEND gets its powers
        if not casting and ready and d2(px, py, home.x, home.y) <= HL_HOME_R * HL_HOME_R then
            local bx, by, bk, bn = nil, nil, -1, 0
            for _, kind in ipairs({KIND_SOLDIERS, KIND_SHIPS}) do
                local x, y, c = best_cluster(seen, kind)
                if c >= (kind == KIND_SHIPS and HL_SHIPS or HL_DEFEND_N) then
                    local is_s = kind == KIND_SOLDIERS
                    local have_s = bk == KIND_SOLDIERS
                    if bk < 0 or (is_s and not have_s) or (is_s == have_s and c > bn) then
                        bx, by, bk, bn = x, y, kind, c
                    end
                end
            end
            if bk >= 0 then
                local cast, k = cast_kind(h, ps, u, bk, bx, by, now)
                if cast then
                    ps.x, ps.y, ps.t = -1, -1, 0
                    h.defends = h.defends + 1
                    casting = true
                    if S.holy_log then
                        say("a prophet at home at %d,%d casts %s on %d enemy %s at %d,%d", px, py, POWERS[k].name, bn,
                            bk == KIND_SHIPS and "ships" or "soldiers", bx, by)
                    end
                end
            end
        end
        -- THE TOWERS: the nearest enemy tower within HL_TOWER_LOOK gets an earthquake HL_QUAKE_OFF tiles before it
        if not casting and ready and quake then
            local bd, at_x, at_y = nil, nil, nil
            for _, e in ipairs(seen) do
                if e.kind == KIND_BUILDINGS and is_defence(e.type) and e.d <= HL_TOWER_LOOK * HL_TOWER_LOOK then
                    if (land == 0 or land_of(e.x, e.y) == land) and (bd == nil or e.d < bd) then
                        local dx, dy = px - e.x, py - e.y
                        local dl = isqrt(dx * dx + dy * dy)
                        local sx, sy = e.x, e.y
                        if dl >= 1 then
                            sx = e.x + dx * HL_QUAKE_OFF // dl
                            sy = e.y + dy * HL_QUAKE_OFF // dl
                        end
                        bd, at_x, at_y = e.d, sx, sy
                    end
                end
            end
            if bd ~= nil and cast_power(h, ps, u, QUAKE, at_x, at_y, nil, now) then
                ps.x, ps.y, ps.t = -1, -1, 0
                h.towers = h.towers + 1
                casting = true
                if S.holy_log then
                    say("a prophet at %d,%d casts an earthquake at %d,%d before an enemy tower", px, py, at_x, at_y)
                end
            end
        end
        -- THE POWERS: what it knows within HL_REACH, ships, soldiers, citizens, buildings
        if not casting and ready then
            local list = {}
            for _, e in ipairs(seen) do
                if e.kind >= 0 and e.d <= HL_REACH * HL_REACH and (land == 0 or e.kind == KIND_SHIPS or land_of(e.x, e.y) == land) then
                    list[#list + 1] = e
                end
            end
            for _, m in ipairs(known) do   -- the remembered buildings (ai.memory holds enemies only)
                local k = kind_of(m.type)
                if k == KIND_BUILDINGS and d2(m.x, m.y, px, py) <= HL_REACH * HL_REACH
                        and (land == 0 or land_of(m.x, m.y) == land) then
                    list[#list + 1] = {type = m.type, x = m.x, y = m.y, kind = k}
                end
            end
            for _, kind in ipairs(KIND_ORDER) do
                local x, y, c = best_cluster(list, kind)
                if c >= KIND_NEED[kind] then
                    local cast, k = cast_kind(h, ps, u, kind, x, y, now)
                    if cast then
                        ps.x, ps.y, ps.t = -1, -1, 0
                        casting = true
                        if S.holy_log then
                            say("a prophet at %d,%d casts %s on %d of the enemy at %d,%d", px, py, POWERS[k].name, c, x, y)
                        end
                        break
                    end
                end
            end
        end
    end
    if not casting and ord == nil then
        -- A STRIKE called off: hurt, an enemy soldier by it (checked every HL_ESC_AGAIN), or too long ago
        if ps.x >= 0 then
            local old = ps.t > 0 and now >= ps.t and now - ps.t > HL_STRIKE_OLD
            if hurt or old then
                ps.x, ps.y, ps.t = -1, -1, 0
            elseif ps.chk == nil or now - ps.chk >= HL_ESC_AGAIN then
                ps.chk = now > 0 and now or 1
                if raiders_near(ps.x, ps.y, HL_STRIKE_GUARD, 1) > 0 then
                    ps.x, ps.y, ps.t = -1, -1, 0
                end
            end
        end
        -- WITH THE FORCE, ON ITS OWN: the first prophet goes with the force, the others strike
        if not casting and ready then
            local with = fight and first and ps.x < 0 and not hurt and quake
            if with then
                local bx, by = behind(fx, fy, home.x, home.y)
                if land ~= 0 and land_of(bx, by) ~= land_of(fx, fy) then
                    bx, by = fx, fy
                end
                walk(id, px, py, bx, by, u.tx, u.ty)
            elseif ps.x < 0 and not hurt and quake then
                -- A STRIKE: the nearest enemy building unguarded (holy.c's box scan over the memory's buildings)
                local best, bd = nil, nil
                for _, m in ipairs(known) do
                    if kind_of(m.type) == KIND_BUILDINGS and (m.hp or 0) > 0 then
                        local d = d2(m.x, m.y, px, py)
                        if d <= HL_STRIKE * HL_STRIKE and (land == 0 or land_of(m.x, m.y) == land)
                                and (bd == nil or d < bd) then
                            best, bd = m, d
                        end
                    end
                end
                if best ~= nil then
                    local tries = 0
                    local tower = is_defence(best.type)
                    while best ~= nil and tries < HL_STRIKE_TRIES
                            and (raiders_near(best.x, best.y, HL_STRIKE_GUARD, 1) > 0
                                 or (not tower and (ai.danger(best.x, best.y) or 0) > 0)) do
                        tries = tries + 1
                        -- the next nearest
                        local nb, nbd = nil, nil
                        for _, m in ipairs(known) do
                            if m ~= best and kind_of(m.type) == KIND_BUILDINGS and (m.hp or 0) > 0 then
                                local d = d2(m.x, m.y, px, py)
                                if d <= HL_STRIKE * HL_STRIKE and (land == 0 or land_of(m.x, m.y) == land)
                                        and (nbd == nil or d < nbd) then
                                    nb, nbd = m, d
                                end
                            end
                        end
                        best = nb
                        tower = best ~= nil and is_defence(best.type)
                    end
                    if best ~= nil and tries < HL_STRIKE_TRIES then
                        local sx, sy = best.x, best.y
                        if tower then   -- where it stands to cast: the tower's reach + 2 from it, on its side
                            local dx, dy = px - best.x, py - best.y
                            local dl = isqrt(dx * dx + dy * dy)
                            local c = ai.class(best.type)
                            local reach = (c ~= nil and (c.range or 0) // 100 or 4) + 2
                            if dl >= 1 then
                                sx, sy = best.x + dx * reach // dl, best.y + dy * reach // dl
                            end
                        end
                        ps.x, ps.y, ps.t = sx, sy, now > 0 and now or 1
                        ps.given, ps.by = nil, nil
                        h.strikes = h.strikes + 1
                        if S.holy_log then
                            say("a prophet at %d,%d strikes at the enemy building at %d,%d", px, py, sx, sy)
                        end
                    end
                end
            end
            -- AT ITS TARGET: within HL_REACH of it, the earthquake on it (a building seen within 2 of the place)
            if ps.x >= 0 and ready and not casting and d2(px, py, ps.x, ps.y) <= HL_REACH * HL_REACH then
                local bld = false
                for _, e in ipairs(seen) do
                    if e.kind == KIND_BUILDINGS and d2(e.x, e.y, ps.x, ps.y) <= 4 then
                        bld = true
                        break
                    end
                end
                local sx, sy = ps.x, ps.y
                if bld and cast_power(h, ps, u, QUAKE, sx, sy, nil, now) then
                    ps.x, ps.y, ps.t = -1, -1, 0
                    casting = true
                    if S.holy_log then
                        say("a prophet at %d,%d casts an earthquake at the building it struck at %d,%d", px, py, sx, sy)
                    end
                end
            end
        end
    end
    -- the escort and the walk
    if ps.x >= 0 and not hurt then
        escort(h, ps, px, py, now, force)
        if casting or ord ~= nil then
            return
        end
        local waited = ps.t > 0 and now >= ps.t and now - ps.t or 0
        local wait = waited < HL_ESC_WAIT and (ps.given ~= nil and ps.given > 0 and 2 * (ps.by or 0) < ps.given
                or (ps.given == nil and waited < 10000))
        if wait then
            if u.action ~= "idle" and now - (ps.held_t or 0) >= 2000 then
                ai.stop({id})
                ps.held_t = now > 0 and now or 1
            end
        else
            ps.held_t = nil
            walk(id, px, py, ps.x, ps.y, u.tx, u.ty)
        end
    elseif not casting and ord == nil then
        walk(id, px, py, home.x, home.y, u.tx, u.ty)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The priests

local function priest(h, u, now, town, fight, fx, fy)
    local id, px, py = u.id, u.x, u.y
    local home = nearest_town(town, px, py, land_of(px, py))
    local hurt = (u.hp_max or 0) > 0 and u.hp * 100 < HL_HURT * u.hp_max
    local list = enemies_near(px, py, HL_PRIEST_LOOK + HL_ALONE)
    local foes = 0
    for _, e in ipairs(list) do
        if e.kind == KIND_SOLDIERS and e.d <= (HL_ALONE + 1) * (HL_ALONE + 1) then
            foes = foes + 1
        end
    end
    if hurt and foes > 0 then
        walk(id, px, py, home.x, home.y, u.tx, u.ty)
        return
    end
    -- converting: its target still stands and is alone enough to keep at
    local tg = u.target
    if tg ~= nil and tg > 0 then
        local t = ai.unit(tg)
        if t ~= nil and (t.hp or 0) > 0 and foe_player(t.player) then
            local near = 0
            for _, e in ipairs(list) do
                if e.kind == KIND_SOLDIERS and e.id ~= tg and d2(e.x, e.y, t.x, t.y) <= HL_ALONE * HL_ALONE then
                    near = near + 1
                end
            end
            if near <= 1 then
                return   -- it converts
            end
        end
    end
    -- A GIVEN ORDER: on its way
    local ord = h.ord[id]
    if ord ~= nil and now >= ord.t and now - ord.t < HL_ORDER_WAIT and u.action == "attack" then
        return
    end
    -- the best: an enemy citizen or land soldier within HL_PRIEST_LOOK, alone, out of danger; citizens first
    local best, bs = nil, 0
    for _, e in ipairs(list) do
        local cit = e.kind == KIND_CITIZENS
        if (cit or e.kind == KIND_SOLDIERS) and e.d <= HL_PRIEST_LOOK * HL_PRIEST_LOOK then
            local others = 0
            for _, o in ipairs(list) do
                if o.kind == KIND_SOLDIERS and o.id ~= e.id and d2(o.x, o.y, e.x, e.y) <= HL_ALONE * HL_ALONE then
                    others = others + 1
                end
            end
            if others <= (cit and 0 or 1) and (ai.danger(e.x, e.y) or 0) == 0 then
                local sc = 100000 - e.d + (cit and 50000 or 0)
                if best == nil or sc > bs then
                    best, bs = e, sc
                end
            end
        end
    end
    if best ~= nil then
        if ai.attack({id}, best.id) == "ok" then
            h.converts = h.converts + 1
            h.ord[id] = {t = now > 0 and now or 1}
            if S.holy_log then
                say("a priest at %d,%d converts the enemy %s at %d,%d", px, py, best.type, best.x, best.y)
            end
        end
        return
    end
    if tg ~= nil and tg > 0 then
        return   -- its order still stands
    end
    local tx, ty = home.x, home.y
    if fight then
        tx, ty = behind(fx, fy, home.x, home.y)
    end
    walk(id, px, py, tx, ty, u.tx, u.ty)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The front (holy.c sn_need_at: where the force meets the enemy): army.lua's force out, else its front

local function read_front(memory)
    local mem = memory.army or {}
    local g = G.role(memory, "force")
    if mem.out == true and g ~= nil and (g.x or -1) >= 0 then
        return true, g.x, g.y
    end
    if (mem.front_x or -1) >= 0 then
        return true, mem.front_x, mem.front_y
    end
    return false, nil, nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (holy_tick)

function M.step(view, memory, orders)
    if S.holy ~= true or not verbs_ok() then
        return   -- off (default: until measured), or an engine without the verbs
    end
    local h = state(memory)
    local now = view.time
    if h.start_t == nil or now < h.start_t then
        reset(h, now)   -- a new game (or a save from an older time)
    end
    if h.t ~= nil and now >= h.t and now - h.t < HL_EVERY then
        return
    end
    h.t = now > 0 and now or 1
    local fight, fx, fy = read_front(memory)
    for k = 1, HLP_N do
        h.casts[k] = h.casts[k] or 0
    end
    -- its own: the priests and prophets, the towns, the temple
    local town, temple, making = own_places()
    local pro, pri = {}, {}
    local ids = ai.units{category = "priest"}
    if #ids > 0 then
        local c = ai.get(ids, {"type", "x", "y", "hp", "hp_max", "action", "inside", "tx", "ty", "target"})
        for k = 1, #ids do
            if c.x[k] ~= nil and c.x[k] >= 0 and (c.hp[k] or 0) > 0 and c.inside[k] == nil then
                local u = {id = ids[k], t = c.type[k], x = c.x[k], y = c.y[k], hp = c.hp[k], hp_max = c.hp_max[k],
                           action = c.action[k], tx = c.tx[k], ty = c.ty[k], target = c.target[k]}
                if is_prophet(u.t) then
                    if #pro < HL_UNITS then
                        pro[#pro + 1] = u
                    end
                elseif is_priest(u.t) then
                    if #pri < HL_UNITS then
                        pri[#pri + 1] = u
                    end
                end
            end
        end
    end
    -- THE LOST: a fall in the count is a loss
    for k = 1, 2 do
        local n = k == 1 and #pro or #pri
        local last = h.last[k] or 0
        if n < last then
            lost_add(h, k, last - n, now)
        end
        h.last[k] = n
    end
    -- TRAINING: one at a time, with a finished Temple making nothing, the bank the price
    if temple ~= nil and not making and (h.train_t == nil or now < h.train_t or now - h.train_t >= HL_TRAIN_EVERY) then
        h.train_t = now > 0 and now or 1
        local cap = view.population_cap or 0
        local wc = cap // HL_CAP_PRIEST
        local wp = HL_PROPHETS + (view.epoch or 0) // HL_PROPHET_EPOCHS
        if wp > HL_PROPHETS_MAX then
            wp = HL_PROPHETS_MAX
        end
        if wp > cap // HL_CAP_PROPHET then
            wp = cap // HL_CAP_PROPHET
        end
        if wc > HL_PRIESTS then
            wc = HL_PRIESTS
        end
        if wc < 1 then
            wc = 1
        end
        local want_p = #pro < wp and lost_count(h, 1, now) < HL_LOSS_MAX
        local want_c = #pri < wc and lost_count(h, 2, now) < HL_LOSS_MAX
        local t = nil
        if want_p then
            t = "prophet"
        elseif want_c then
            t = "priest"
        end
        if t == "prophet" and not power_ok(QUAKE) then
            t = want_c and "priest" or nil   -- no powers for it
        end
        local c = t ~= nil and V.cost(view, t) or nil
        if c ~= nil then
            for _, r in ipairs(V.resources) do
                if (c.cost[r] or 0) > 0 and (view.stock[r] or 0) < HL_BANK * c.cost[r] then
                    c = nil   -- the bank holds not the price
                    break
                end
            end
        end
        if c ~= nil then
            orders = orders or {}
            orders[#orders + 1] = {verb = "train", type = t, n = 1}
            if t == "prophet" then
                h.trained_p = h.trained_p + 1
            else
                h.trained_c = h.trained_c + 1
            end
        end
    end
    -- the remembered buildings (ai.memory holds the enemy's seen and remembered, never nature's or an ally's)
    local known = {}
    local force = {}
    local fg = G.role(memory, "force")
    local quake = false
    if #pro > 0 then
        known = ai.memory{buildings = true}
        quake = power_ok(QUAKE)
        if fg ~= nil then
            for _, sid in ipairs(fg.units) do
                force[sid] = true
            end
        end
    end
    for i, u in ipairs(pro) do
        prophet(h, u, now, town, known, fight, fx, fy, force, i == 1, quake)
    end
    for _, u in ipairs(pri) do
        priest(h, u, now, town, fight, fx, fy)
    end
    -- the dead's entries out (the memory small)
    local live = {}
    for _, u in ipairs(pro) do
        live[u.id] = true
    end
    for _, u in ipairs(pri) do
        live[u.id] = true
    end
    for id in pairs(h.p) do
        if not live[id] then
            h.p[id] = nil
        end
    end
    for id in pairs(h.ord) do
        if not live[id] then
            h.ord[id] = nil
        end
    end
    if S.holy_log and ((h.log_t or 0) == 0 or now >= h.log_t and now - h.log_t >= 60000) then
        h.log_t = now > 0 and now or 1
        say("%d prophets, %d priests, Temple %s | trained %d prophets and %d priests, casts quake %d fire %d volcano %d plague %d hurricane %d malaria %d, %d strikes, %d at towers, %d parting gifts, %d at home, %d refused, %d conversions, %d away, %d casts given up, lost in 10 minutes %d prophets %d priests",
            #pro, #pri, temple ~= nil and "1" or "0", h.trained_p, h.trained_c, h.casts[QUAKE], h.casts[FIRE],
            h.casts[VOLCANO], h.casts[PLAGUE], h.casts[HURRICANE], h.casts[MALARIA], h.strikes, h.towers, h.gifts,
            h.defends, h.refused, h.converts, h.aways, h.stuck, lost_count(h, 1, now), lost_count(h, 2, now))
    end
end

-- a new game (holy_new_game): the counters and the orders again
function M.clear(memory)
    memory.holy = nil
end

return M
