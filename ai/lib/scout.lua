-- The permanent scout and what it learns, over the per-unit interface (docs/AI_LUA_UNITS.md §3; docs/AI_RESEARCH.md
-- §8.40). require("lib.scout"). NOT WIRED YET: no script calls it.
-- (The next round wires it into boom and benches it.) Generic: any script may use it.
--
--   SC.step(view, mem, opts)   every strategy step: keeps one soldier watching the enemy's town (a manual lib.groups
--                              group of role "watch"): it circles the town at `ring` tiles, out of the reach of the
--                              towers it knows, a new point every `hold_ms`, and backs off when hit. A lost watcher
--                              is replaced after `replace_ms`. opts: from (a group to take it from: the guard)
--   SC.intel(view, mem, player)  what the player knows of an enemy (ai.memory, fog-honest; every 10 s): its soldiers
--                              (count, summed hit points and attack, by role), its towers near its town, its town and
--                              epoch; {soldiers, hp, atk, towers, epoch, x, y, roles = {ranged, pierce, shock, cavalry}}
--
-- [research §2.4, §4.4] the enemy's army, epoch and towers feed the guard's mix and the attack timing; [ageofai
-- army.lua scouts, lib.groups' scout] the land itself is explored by the script's own scouts (boom's), this one only
-- watches.
local G = require("lib.groups")
local GD = require("lib.guard")

local M = {}

M.opts = {
    from_epoch = 1,       -- the watcher from this epoch (the Stone Age: the guard is there to spare one)
    min_guard = 6,        -- and only while the guard keeps this many
    ring = 11,            -- tiles round the enemy's town the watcher circles
    tower_margin = 2,     -- tiles out of a known tower's reach
    hold_ms = 20000,      -- a point is held this long
    replace_ms = 90000,   -- a lost watcher is replaced after this long
    max_lost = 6,         -- watchers lost at most (then no more)
}

local DIRS = {{10, 0}, {7, 7}, {0, 10}, {-7, 7}, {-10, 0}, {-7, -7}, {0, -10}, {7, -7}}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

-- the enemy whose town is known nearest ours (nil: none known)
local function target(view)
    local best, bd = nil, nil
    for _, e in ipairs(view.enemies or {}) do
        if e.town_x ~= nil and e.town_x >= 0 then
            local d = d2(e.town_x, e.town_y, view.town_x, view.town_y)
            if bd == nil or d < bd then
                best, bd = e, d
            end
        end
    end
    return best
end

-- the known enemy towers (armed buildings in the memory) near a tile: {x, y, r}
local function towers_near(x, y, r, player)
    local out = {}
    for _, m in ipairs(ai.memory{buildings = true, player = player, near = {x, y, r}, limit = 40}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 and (c.range or 0) > 100 then
            out[#out + 1] = {x = m.x, y = m.y, r = c.range // 100}
        end
    end
    return out
end

function M.intel(view, mem, player)
    mem.intel = mem.intel or {}
    local key = player
    local i = mem.intel[key]
    if i ~= nil and view.time - i.t < 10000 then
        return i
    end
    local e = nil
    for _, x in ipairs(view.enemies or {}) do
        if x.player == player then
            e = x
        end
    end
    i = {t = view.time, soldiers = 0, hp = 0, atk = 0, towers = 0, epoch = e ~= nil and e.epoch or 0,
         x = e ~= nil and e.town_x or -1, y = e ~= nil and e.town_y or -1, ranged = 0, pierce = 0, shock = 0, cavalry = 0}
    for _, m in ipairs(ai.memory{player = player, category = "soldier", limit = 300}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            i.soldiers = i.soldiers + 1
            i.hp = i.hp + (m.hp or c.hp or 0)
            i.atk = i.atk + (c.attack or 0)
            local r = GD.role_of(m.type, c.range)
            i[r] = i[r] + 1
        end
    end
    if i.x >= 0 then
        i.towers = #towers_near(i.x, i.y, 22, player)
    end
    mem.intel[key] = i
    return i
end

function M.step(view, mem, opts)
    opts = opts or {}
    local o = M.opts
    local now = view.time
    local g = G.role(mem, "watch")
    if g == nil and mem.watch_id ~= nil then
        -- lost (lib.groups drops a group whose units are gone): the next one waits replace_ms
        mem.watch_id = nil
        mem.watch_lost = (mem.watch_lost or 0) + 1
        mem.watch_next = now + o.replace_ms
    end
    local e = target(view)
    if g == nil then
        if e == nil or view.epoch < o.from_epoch or (mem.watch_lost or 0) >= o.max_lost
                or now < (mem.watch_next or 0) then
            return
        end
        local from = opts.from
        if from == nil or #from.units <= o.min_guard then
            return
        end
        -- the fastest of the group (the first by id among equals)
        local c = ai.get(from.units, {"speed", "hp"})
        local best, bs = nil, nil
        for k, id in ipairs(from.units) do
            local s = c.speed[k]
            if s ~= nil and (bs == nil or s > bs) then
                best, bs = id, s
            end
        end
        if best == nil then
            return
        end
        g = G.create(mem, {task = "scout", role = "watch", manual = true, x = e.town_x, y = e.town_y})
        G.add(mem, g, {best})
        g.k, g.t, g.player = 1, 0, e.player
        mem.watch_id = g.id
        mem.watches = (mem.watches or 0) + 1
        print(string.format("watch new id=%d unit=%d enemy=%d town=%d,%d", g.id, best, e.player, e.town_x, e.town_y))
    end
    if #g.units == 0 then
        return
    end
    local u = ai.unit(g.units[1])
    if u == nil then
        return
    end
    if e == nil then
        return
    end
    g.player = e.player
    -- hit since the last step: back off to twice the ring and on to the next point
    local hit = g.hp ~= nil and u.hp < g.hp
    g.hp = u.hp
    if hit and now - (g.hit_t or 0) >= 5000 then
        g.hit_t = now
        g.k = g.k % #DIRS + 1
        local dx, dy = u.x - e.town_x, u.y - e.town_y
        local l = math.max(math.abs(dx), math.abs(dy), 1)
        ai.move(g.units, u.x + dx * 8 // l, u.y + dy * 8 // l, {forced = true})
        g.t = now
        return
    end
    if g.t ~= 0 and now - g.t < o.hold_ms then
        return
    end
    -- the next point of the ring out of the known towers' reach
    local towers = towers_near(e.town_x, e.town_y, o.ring + 12, e.player)
    local map = ai.map()
    for _ = 1, #DIRS do
        local d = DIRS[g.k]
        g.k = g.k % #DIRS + 1
        local px = e.town_x + d[1] * o.ring // 10
        local py = e.town_y + d[2] * o.ring // 10
        px = math.max(2, math.min(px, map.width - 3))
        py = math.max(2, math.min(py, map.height - 3))
        local safe = true
        for _, t in ipairs(towers) do
            local r = t.r + o.tower_margin
            if d2(px, py, t.x, t.y) <= r * r then
                safe = false
            end
        end
        local tile = ai.tile(px, py)
        if safe and tile ~= nil and tile.water ~= true then
            ai.move(g.units, px, py, {forced = true})
            g.t = now
            return
        end
    end
    g.t = now
end

return M
