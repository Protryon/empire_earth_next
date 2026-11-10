-- Age of AI port, the unit stats (upstream dll/ai/stats.c: the fight estimate, st_ratio). GPL-3.0: see UPSTREAM in
-- this folder for the licence and the per-file map. require("ageofai.stats").
--
-- [ai_base] fight_estimate (default 0, settings.fight_estimate = false): 1 = the fight decisions compare sides with
-- this estimate instead of summed hit points x attack (ai.strength). Upstream measured no gain (A/B 2026-10-17) and
-- left it off; the port keeps it off and offers M.ratio for the callers that would switch.
--
-- Upstream's damage of one shot: attack x the damage-class table [attacker class, target class] x the hit factor
-- [weapon kind, target family] (0 = cannot hit) - the target's armour against the attacker's class; rounded, at
-- least 1. Time between shots: the unit's reload (the class value with the owner's upgrades) + ST_SHOT_OVERHEAD
-- (2 s: picking the target, turning, stepping into range - measured in duels). By type: units of one type and
-- player share their stats, so the work grows with types x types, not units x units. RANGE: a unit that cannot
-- move never hits one that outranges it; one that is outranged walks the difference under fire first (st_close).
-- Minimum range: a mobile target that closed in cannot be hit by artillery. Area damage: the neighbours of a
-- target within the radius take the same, at most ST_AREA_MAX counted.
--
-- PARTLY (what ai.class exposes: hp, attack, range, speed, reload, with the player's own upgrades): the damage-class
-- table, the hit-factor table (weapon kind x target family), the armour per class, the minimum range and the area
-- effect are NOT exposed, so here a shot = attack (at least 1), every class can hit every other but for one rule by
-- name - anti-air hits aircraft only and only anti-air or aircraft hit aircraft (the game's hit factors; the towers
-- and the land units have 0 against air) -, no minimum range and no area damage. The enemy's stats are read from
-- the reader's own upgrades (ai.class has no player argument). The missing fields are an interface gap for the engine
-- side: ai.class(type).damage_class, .weapon, .family, .min_range, .area, .armor[6] and the two tables.
local M = {}

M.ST_SHOT_OVERHEAD = 2.0
M.ST_AREA_MAX = 2.0   -- (unused: no area effect is exposed)

local function is_aa(t)
    t = t or ""
    return string.sub(t, 1, 4) == "b_aa" or string.sub(t, 1, 2) == "aa" or string.find(t, "_aa_", 1, true) ~= nil
        or string.find(t, "missile_trooper", 1, true) ~= nil or string.find(t, "anti_missile", 1, true) ~= nil
end

-- the hit factor stand-in: 1 (can hit) or 0 (cannot), from the class names and categories (see the header)
local function factor(a, t)
    if t.category == "air" then
        return (is_aa(a.type) or a.category == "air") and 1 or 0
    end
    return is_aa(a.type) and 0 or 1
end

-- damage of one shot of group a at group t: 0 = cannot hurt it (st_shot_t without the tables and the armour)
local function shot(a, t)
    local att = a.cls.attack or 0
    if att <= 0 or factor(a, t) <= 0 then
        return 0
    end
    local r = math.floor(att + 0.5)
    return r < 1 and 1 or r
end

local function reload(g)   -- seconds between shots (the class value; 0 = 1 s)
    local ms = g.cls.reload or 0
    return ms > 0 and ms / 1000.0 or 1.0
end

local function range(g)
    return (g.cls.range or 0) / 100.0
end

local function speed(g)   -- tiles/s, 0 = cannot move
    return (g.cls.speed or 0) / 100.0
end

local function dps(a, t)
    return shot(a, t) / (reload(a) + M.ST_SHOT_OVERHEAD)
end

-- a unit that cannot move never hits one that outranges it
local function reaches(a, t)
    return speed(a) > 0 or range(a) + 0.5 >= range(t)
end

-- a side from unit ids (own, allied or seen; a remembered building's id too) or records {type=, hp=, category=}
local function side(list)
    local s = {groups = {}, n = 0, hp = 0}
    local by = {}
    for _, e in ipairs(list or {}) do
        local u = type(e) == "table" and e or ai.unit(e)
        if u ~= nil and u.type ~= nil then
            local g = by[u.type]
            if g == nil then
                local cls = ai.class(u.type) or {}
                g = {type = u.type, cls = cls, category = u.category or (cls.building and "building" or "soldier"), count = 0, hp = 0}
                by[u.type] = g
                s.groups[#s.groups + 1] = g
            end
            local hp = u.hp or g.cls.hp or 0
            g.count = g.count + 1
            g.hp = g.hp + hp
            s.hp = s.hp + hp
            s.n = s.n + 1
        end
    end
    return s
end

-- side a's damage per second at side b: each unit spreads its fire over the units of b it can hurt (by their
-- count). contact is kept for the callers' shape (no minimum range is exposed: the same either way)
local function side_dps(a, b, contact)
    local s = 0
    for _, u in ipairs(a.groups) do
        local num, den = 0, 0
        for _, t in ipairs(b.groups) do
            local d = reaches(u, t) and dps(u, t) or 0
            if d > 0 then
                num = num + t.count * d
                den = den + t.count
            end
        end
        u.part = den > 0 and u.count * num / den or 0
        s = s + u.part
    end
    return s
end

-- seconds side a walks under fire before it shoots: each mobile group's range short of side b's longest, over
-- its speed (weighted by what it deals)
local function close(a, b)
    local rb, t, w = 0, 0, 0
    for _, g in ipairs(b.groups) do
        if range(g) > rb then
            rb = range(g)
        end
    end
    for _, u in ipairs(a.groups) do
        local d, v, gap = 0, speed(u), rb - range(u)
        for _, g in ipairs(b.groups) do
            if d <= 0 then
                d = dps(u, g)
            end
        end
        if d > 0 and v > 0 then
            t = t + u.count * d * (gap > 0 and gap / v or 0)
            w = w + u.count * d
        end
    end
    return w > 0 and t / w or 0
end

-- time to destroy hp: first own closing (no fire), then free fire while the enemy closes, then contact fire
local function kill_time(hp, own_close, their_close, free_dps, contact_dps)
    if free_dps > 0 and hp <= free_dps * their_close then
        return own_close + hp / free_dps
    end
    hp = hp - free_dps * their_close
    return contact_dps > 0 and own_close + their_close + hp / contact_dps or 1e9
end

-- the estimate: time[1] = how long a needs to destroy b, time[2] b to destroy a
function M.estimate(a, b)
    local r = {dps_free = {}, dps_contact = {}, close = {}, time = {}}
    r.dps_contact[1] = side_dps(a, b, true)
    r.dps_contact[2] = side_dps(b, a, true)
    r.dps_free[1] = side_dps(a, b, false)
    r.dps_free[2] = side_dps(b, a, false)
    r.close[1] = close(a, b)
    r.close[2] = close(b, a)
    r.time[1] = kill_time(b.hp, r.close[1], r.close[2], r.dps_free[1], r.dps_contact[1])
    r.time[2] = kill_time(a.hp, r.close[2], r.close[1], r.dps_free[2], r.dps_contact[2])
    return r
end

-- how much stronger own is than foe, on the scale of summed hit points x attack: the square root of the time foe
-- needs to destroy own over the time own needs to destroy foe (twice the units of one type make the time ratio 4,
-- the root 2 - the scale of the old measure). Nothing to fight: very large / 0. own, foe: unit ids or records
function M.ratio(own, foe)
    local a, b = side(own), side(foe)
    if b.n == 0 or b.hp <= 0 then
        return 1e6
    end
    if a.n == 0 or a.hp <= 0 then
        return 0.0
    end
    local r = M.estimate(a, b)
    if r.time[1] >= 1e9 then
        return r.time[2] >= 1e9 and 1.0 or 0.0
    end
    if r.time[2] >= 1e9 then
        return 1e6
    end
    return math.sqrt(r.time[2] / (r.time[1] > 0.1 and r.time[1] or 0.1))
end

return M
