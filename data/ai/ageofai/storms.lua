-- Age of AI port: upstream dll/ai/storms.c (the prophet's hurricane and the Anti-Matter Storm: the AI's ships within
-- reach sail straight out of it whatever they were doing, and its squadrons send nobody in; [ai_base] storm_dodge = 1;
-- UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.storms"), stepped from main.lua's step AFTER ageofai.army (which runs the navy and the fleet: the
-- dodge's order is the one the ship carries out), and read by fleet.lua's scan for the danger zones.
--
-- The player, 2026-11-18: "ships dont avoid enemy storms yet i think, getting away from them should override
-- evrething else sinc ethey be sunk un seconds by it". Upstream's measurements: the hurricane is a unit of its
-- caster's (type hurricane, 9999 hit points, attack 0 - no warship to the fleet's scan, nothing at all to the AI
-- before this) that stands where it was cast for 40-60 s and hits every enemy unit within about 5 tiles of its
-- middle for 100 about every second, whatever its kind (a Good Hope frigate 1350 -> 250 in 10 s; the Middle Ages'
-- frigate 540 gone in 5 s); an ally's storm hurts nobody (the caster's stance decides). The Anti-Matter Storm is
-- taken as the same. Measured: ships lost in a storm 0.19 an AI with the dodge against 1.85 without, from epoch 9.
--
-- THE DODGE: every look each ship of the AI's (warships, transports, fishing boats - the sea's) within ST_R of an
-- enemy storm it SEES sails straight away from the storm's middle to open water of its sea ST_OUT out (the nearest
-- way out of the directions round that: straight ahead, then 30 degrees either way, +-60, ... up to 150), none into
-- another storm that hits it, whatever it was doing; the order is given again while a manager's took its place
-- (upstream ran storm_tick after every manager that ordered ships; here main.lua's step runs this after the army's
-- navy and fleet, and the look repeats every step). Once out, the managers have the ship back.
-- THE DANGER ZONES: the storms are danger zones of the fleet's scan (fleet.c fl_scan: the squadrons send nobody in,
-- sq_safe; the raids, sq_care takes out those inside). M.zones() hands them to fleet.lua's M.scan, M.danger tests a
-- point for any other reader.
-- THE COUNT: storms seen, dodge orders, ships caught in one at a look, ships lost in one (in reach at the last look
-- and gone at the next), a summary a minute.
--
-- Deviations: the look is every game second (upstream 250 ms; the step is 1 s here); the "caught" test also asks that
-- the ship be on water (a hurricane can be cast over land, and a land transport within 5 tiles of it is not a ship);
-- the storms are the ones seen now (upstream's seen_by), so a storm in the fog is not dodged from.
-- Not ported: nothing of the file is missing from the API (the claim's priority CLP_SURVIVE has no counterpart: this
-- module's order comes last in the step, which is the same effect), the [autotest] variant, the log's line format.
local S = require("ageofai.settings")

local M = {}

local ST_LOOK = 1000     -- the look (upstream 250 ms)
local ST_R = 5           -- the storm's reach: the ships inside this of its middle are dodging (upstream's about 5)
local ST_OUT = 12        -- sailed to this far from the storm's middle
local ST_MAX = 16        -- storms at a time
local ST_KNOWN = 8       -- the storms counted lately (the seen counter's ring)

-- the storm units (is_storm): the prophet's hurricane and the Anti-Matter Storm
local STORMS = {"hurricane", "anti_matter_storm"}

-- the storms seen at the last look: memory.storms.zones holds them; CUR is the same table for the per-call readers
-- (fleet.lua's scan asks M.zones() every 2 s and M.danger() is a per-ship test: no memory lookup)
local CUR = {}

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

-- the enemy storms it sees now: an array of {id, x, y, r}
function M.zones()
    return CUR
end

-- whether (x, y) lies in a storm's reach (+ m tiles): the fleet's and the squadrons' test (fleet.c's sq_safe)
function M.danger(x, y, m)
    m = m or 0
    for _, z in ipairs(CUR) do
        local r = z.r + m
        if d2(x, y, z.x, z.y) <= r * r then
            return true
        end
    end
    return false
end

-- the nearest storm a point is inside of (nil: none): its index in M.zones()
function M.inside(x, y)
    for k, z in ipairs(CUR) do
        if d2(x, y, z.x, z.y) <= z.r * z.r then
            return k
        end
    end
    return nil
end

-- a point of the sea oc ST_OUT from the storm's middle, the way from it to the ship first, then turning by 30 degrees
-- either way; none in reach of another storm that hits the AI (st_way_out)
local function way_out(z, ux, uy, oc)
    local dx, dy = ux - z.x, uy - z.y
    local len = isqrt(dx * dx + dy * dy)
    if len < 1 then
        dx, dy, len = 1, 0, 1
    end
    local a0 = math.atan(dy, dx)
    for i = 0, 10 do   -- 0, +30, -30, +60, -60 ... 150
        local turn = (i + 1) // 2
        local a = a0 + (i % 2 == 1 and turn * 0.5236 or -turn * 0.5236)
        local qx = z.x + math.floor(math.cos(a) * ST_OUT + 0.5)
        local qy = z.y + math.floor(math.sin(a) * ST_OUT + 0.5)
        local t = ai.tile(qx, qy)
        if t ~= nil and (t.sea or 0) == oc then
            local bad = false
            for _, o in ipairs(CUR) do
                if o.id ~= z.id and d2(o.x, o.y, qx, qy) <= ST_R * ST_R then
                    bad = true
                end
            end
            if not bad then
                return qx, qy
            end
        end
    end
    return nil
end

local function state(mem)
    mem.storms = mem.storms or {zones = {}, known = {}, caught = {}}
    local s = mem.storms
    s.zones = s.zones or {}
    s.known = s.known or {}
    s.caught = s.caught or {}
    return s
end

-- the step (storm_tick): every look
function M.step(view, memory)
    local s = state(memory)
    local now = view.time
    if (s.t or 0) > 0 and now - s.t < ST_LOOK then
        return
    end
    s.t = now > 0 and now or 1
    local dodge = S.storm_dodge ~= false
    -- the storms on the map it sees (st_find + st_hits: the owner is its enemy and it sees it now)
    local zones = {}
    for _, id in ipairs(ai.seen{enemy = true, type = STORMS, limit = ST_MAX}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            zones[#zones + 1] = {id = id, x = u.x, y = u.y, r = ST_R}
            if s.known[id] == nil then
                s.known[id] = true
                s.seen = (s.seen or 0) + 1
            end
        end
    end
    -- the counted ring of ST_KNOWN storms (upstream's g_stp.known[8]: a storm is counted once while it stands)
    if (s.known_prune or 0) + 60000 <= now then
        s.known_prune = now > 0 and now or 1
        local keys = ai.keys(s.known)
        for k = 1, math.max(#keys - ST_KNOWN, 0) do
            s.known[keys[k]] = nil
        end
    end
    s.zones = zones
    CUR = zones
    -- THE DODGE: each of its ships within ST_R of a storm sails straight out of it (with the dodge off, the zones
    -- are still the fleet's)
    local caught, orders = {}, 0
    local in_reach = {}
    for _, z in ipairs(zones) do
        for _, cat in ipairs({"ship", "boat", "transport"}) do
            for _, id in ipairs(ai.units{category = cat, near = {z.x, z.y, ST_R}}) do
                if not in_reach[id] then
                    local u = ai.unit(id)
                    local t = u ~= nil and u.x ~= nil and ai.tile(u.x, u.y) or nil
                    -- the sea's units only (a hurricane can stand over land: a land transport is no ship)
                    if u ~= nil and u.x ~= nil and t ~= nil and (t.sea or 0) ~= 0 then
                        in_reach[id] = true
                        s.caught_n = (s.caught_n or 0) + 1
                        caught[#caught + 1] = {id = id, x = u.x, y = u.y}
                        if dodge then
                            local qx, qy = way_out(z, u.x, u.y, t.sea)
                            -- on its way out already (goal_near, 2 tiles): left alone
                            if qx ~= nil and not (u.tx ~= nil and d2(u.tx, u.ty, qx, qy) <= 4) then
                                ai.move(id, qx, qy, {forced = true})
                                orders = orders + 1
                            end
                        end
                    end
                end
            end
        end
    end
    s.orders = (s.orders or 0) + orders
    -- LOST IN ONE: in reach at the last look, gone now (by id: no grid scan like upstream's)
    for _, c in ipairs(s.caught) do
        if ai.unit(c.id) == nil then
            s.lost = (s.lost or 0) + 1
        end
    end
    s.caught = caught
    if ((s.seen or 0) > 0 or (s.caught_n or 0) > 0) and (s.log_t or 0) + 60000 <= now then
        s.log_t = now > 0 and now or 1
        print(string.format("storms seen=%d dodge=%d caught=%d lost=%d zones=%d", s.seen or 0, s.orders or 0,
            s.caught_n or 0, s.lost or 0, #zones))
    end
end

return M
