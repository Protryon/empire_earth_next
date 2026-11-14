-- Army groups over the per-unit interface (docs/AI_LUA_UNITS.md §8 L5: EECPNextArmy's groups as Lua data).
-- require("lib.groups"). A group is a table in the script's memory: its task, its soldiers' ids (held through
-- ai.take), its place and its state. G.step(view, mem) runs every group once a step: it drops the dead, gives each
-- unit its target (ai.attack) or the group its leg (ai.move), and keeps the state machine of EECPNextArmy.cpp:
--
--   attack   fight its way to a place (an enemy's town): soldiers within 12 tiles, then citizens within 20, towers,
--            buildings; at its place the target's nearest unit within 30 tiles, else its town; nothing: arrived
--   raid     the enemy's gatherers and fishing boats; soldiers only within 6 tiles; nothing within 9 tiles of a known
--            tower; flees home when the seen soldiers and towers within 10 tiles are 1.5 times its strength
--   defend   hold a place: the enemies within its radius of the place (or 6 tiles of the group); strays come back
--   retreat  a forced move home (no fighting), then it defends there
--   scout    forced moves to the unexplored grid point on its land nearest it and furthest from the town
--            (ai.unexplored); a place not reached in 45 s is skipped; done when nothing is left
--
-- States: forming (its soldiers gather, 20 s at most), moving, fighting, arrived, done. Melee units never take a
-- ship or boat as a target, and a group with only ships to fight keeps out of their reach (AI_RESEARCH §8.35).
-- Everything stored is an integer, a string or a boolean. Iteration goes through ai.keys (lockstep).
local U = require("lib.units")
local M = {}

M.ENGAGE = 12          -- attack, raid: the seen enemies this near the group's centre
M.RAID_SOLDIER = 6     -- a raid fights soldiers only this near
M.CITIZEN_RANGE = 20   -- an attack goes for the citizens it sees this near (before buildings)
M.BOAT_RANGE = 7       -- ranged units shoot at boats and ships this near the centre
M.TOWER_AVOID = 9      -- a raid leaves targets this near a known tower alone
M.SHIP_AVOID = 10      -- units that cannot shoot keep this far from a warship
M.ARRIVE = 6           -- a group this near its place has arrived
M.LEG = 14             -- a move goes this far ahead of the group's centre
M.FORM_MS = 20000      -- the longest a group forms
M.MOVE_MS = 10000      -- a move is given again after this long
M.SCOUT_STUCK_MS = 45000
M.SCOUT_GRID = 8
M.MAX_COUNT = 100
M.LEASH_MARGIN = 6     -- [attack.c] the leash: a known defence's range + this (avoid_towers_margin) ...
M.LEASH_SCAN = 45      -- ... among the defences known this near the group's centre
M.committed = {}       -- unit ids the script has committed elsewhere this step: the groups give them no order

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function near(x1, y1, x2, y2, r)
    return d2(x1, y1, x2, y2) <= r * r
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

-- ---------------------------------------------------------------------------------------------------------------
-- The groups in the memory

function M.init(mem)
    mem.groups = mem.groups or {}
    mem.group_next = mem.group_next or 1
end

-- every group, in id order
function M.all(mem)
    M.init(mem)
    local out = {}
    for _, k in ipairs(ai.keys(mem.groups)) do
        out[#out + 1] = mem.groups[k]
    end
    return out
end

function M.get(mem, id)
    M.init(mem)
    return id ~= nil and mem.groups[id] or nil
end

-- the group with a role (nil: none)
function M.role(mem, role)
    for _, g in ipairs(M.all(mem)) do
        if g.role == role then
            return g
        end
    end
    return nil
end

-- the group's living, on-map soldiers (records), their centre and strength this step
local function members(g)
    local recs, sx, sy, n, str = {}, 0, 0, 0, 0
    local keep = {}
    for _, id in ipairs(g.units) do
        local u = ai.unit(id)
        if u ~= nil then
            keep[#keep + 1] = id
            if u.inside == nil then
                recs[#recs + 1] = u
                sx, sy, n = sx + u.x, sy + u.y, n + 1
                str = str + u.hp * (u.attack or 0)
            end
        else
            g.lost = (g.lost or 0) + 1
            g.aims[id] = nil
        end
    end
    g.units = keep
    if n == 0 then
        return recs, nil, nil, 0
    end
    return recs, sx // n, sy // n, str
end

-- the land soldiers the engine holds (in no group, not held by the script), nearest a tile first
function M.free_soldiers(x, y, limit)
    local f = {category = "soldier", held = "engine"}
    if x ~= nil then
        f.near = {x, y, 1000}
    end
    if limit ~= nil then
        f.limit = limit
    end
    local ids = ai.units(f)
    local out = {}
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and (u.speed or 0) > 0 and u.inside == nil then
            out[#out + 1] = id
        end
    end
    return out
end

-- a new group: opts {task=, role=, x=, y=, radius=, target=(player), units={ids}, forced=}
function M.create(mem, opts)
    M.init(mem)
    local id = mem.group_next
    mem.group_next = id + 1
    local g = {
        id = id, task = opts.task, role = opts.role, units = {}, aims = {}, skips = {},
        x = opts.x or -1, y = opts.y or -1, radius = opts.radius or 0, target = opts.target or 0,
        state = "forming", state_t = ai.time(), way_x = -1, way_y = -1, way_t = 0, created = ai.time(),
        lost = 0, started = 0, home_x = opts.home_x or -1, home_y = opts.home_y or -1,
        -- [armies.c det, skirmish.c] a manual group: the module orders its members itself; the lib only keeps the
        -- list (drops the dead, the centre and the strength)
        manual = opts.manual == true,
    }
    mem.groups[id] = g
    if opts.units ~= nil and #opts.units > 0 then
        M.add(mem, g, opts.units)
    end
    return g
end

-- adds soldiers to a group (taken from the engine or from another group)
function M.add(mem, g, ids)
    if #ids == 0 then
        return 0
    end
    ai.take(ids)
    for _, other in ipairs(M.all(mem)) do
        if other.id ~= g.id then
            local keep = {}
            for _, id in ipairs(other.units) do
                local gone = false
                for _, n in ipairs(ids) do
                    if n == id then
                        gone = true
                    end
                end
                if not gone then
                    keep[#keep + 1] = id
                else
                    other.aims[id] = nil
                end
            end
            other.units = keep
        end
    end
    for _, id in ipairs(ids) do
        local have = false
        for _, u in ipairs(g.units) do
            if u == id then
                have = true
            end
        end
        if not have then
            g.units[#g.units + 1] = id
        end
    end
    if #g.units > (g.started or 0) then
        g.started = #g.units
    end
    -- a group forms at the soldier nearest its centre
    if g.state == "forming" then
        local _, cx, cy = members(g)
        if cx ~= nil then
            local best, bd = nil, nil
            for _, id in ipairs(g.units) do
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil then
                    local d = d2(u.x, u.y, cx, cy)
                    if bd == nil or d < bd then
                        best, bd = u, d
                    end
                end
            end
            if best ~= nil then
                g.way_x, g.way_y = best.x, best.y
                ai.move(g.units, best.x, best.y, {forced = true})
            end
        end
    end
    return #ids
end

-- the soldiers go back to the engine (the classic commander)
function M.disband(mem, g)
    M.init(mem)
    if #g.units > 0 then
        ai.stop(g.units)
        ai.release(g.units)
    end
    mem.groups[g.id] = nil
end

-- takes up to n free soldiers (the nearest to (x, y)), only of the classes named when any, or `from` another group
function M.recruit(mem, g, n, opts)
    opts = opts or {}
    n = math.min(n or 0, M.MAX_COUNT)
    if n <= 0 then
        return 0
    end
    local pool
    if opts.from ~= nil then
        pool = {}
        for _, id in ipairs(opts.from.units) do
            pool[#pool + 1] = id
        end
    else
        pool = M.free_soldiers(opts.x or g.x, opts.y or g.y)
    end
    local picked = {}
    local want = opts.classes
    for _, id in ipairs(pool) do
        if #picked >= n then
            break
        end
        local ok = true
        if want ~= nil and #want > 0 then
            ok = false
            local u = ai.unit(id)
            for _, c in ipairs(want) do
                if u ~= nil and u.type == c then
                    ok = true
                end
            end
        end
        if ok then
            picked[#picked + 1] = id
        end
    end
    return M.add(mem, g, picked)
end

-- ---------------------------------------------------------------------------------------------------------------
-- Targets

-- [attack.c] a wall piece or a gate: its own kind (an attack's last target; the breach segment is the group's focus)
local function is_wall(t)
    local p = string.sub(t or "", 1, 7)
    return p == "b_wall_" or p == "b_gate_"
end

-- the kinds of a seen enemy record
local function kind_of(u)
    if u.category == "building" then
        if is_wall(u.type) then
            return "wall"
        end
        return (u.armed and "tower" or "building")
    elseif u.category == "citizen" then
        return "citizen"
    elseif u.category == "boat" then
        return "boat"
    elseif u.category == "ship" then
        return "ship"
    elseif u.category == "transport" then
        return "ship"
    elseif u.category == "resource" or u.category == "animal" or u.category == "ambient" then
        return nil
    end
    return "soldier"
end

-- the priority of a kind for a task (lower first; nil: no target)
local function priority(task, kind, dist)
    if task == "raid" then
        if kind == "citizen" then return 1 end
        if kind == "boat" then return 2 end
        if kind == "soldier" and dist <= M.RAID_SOLDIER then return 0 end
        return nil
    end
    if task == "defend" or task == "retreat" then
        if kind == "soldier" then return 0 end
        if kind == "ship" then return 1 end
        if kind == "citizen" then return 2 end
        if kind == "tower" then return 3 end
        if kind == "boat" then return 4 end
        if kind == "building" then return 5 end
        return nil
    end
    -- attack
    if kind == "soldier" then return 0 end
    if kind == "ship" then return 1 end
    if kind == "citizen" and dist <= M.CITIZEN_RANGE then return 2 end
    if kind == "tower" then return 3 end
    if kind == "building" then return 4 end
    if kind == "boat" then return 5 end
    if kind == "wall" then return 6 end   -- [attack.c] walls score below everything else (the scorer's below zero)
    return nil
end

-- [attack.c] the known defences near a tile (for the leash): their tile, range (centi) and attack
local function defences_near(x, y, r)
    local out = {}
    for _, m in ipairs(ai.memory{buildings = true, near = {x, y, r}}) do
        if not is_wall(m.type) and string.sub(m.type, 1, 2) ~= "aa" then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 and (c.range or 0) > 100 then
                out[#out + 1] = {x = m.x, y = m.y, r = c.range // 100 + M.LEASH_MARGIN}
            end
        end
    end
    return out
end

local function defended(defs, x, y)
    for _, d in ipairs(defs) do
        if d2(x, y, d.x, d.y) <= d.r * d.r then
            return true
        end
    end
    return false
end

-- the known enemy towers (armed buildings in the memory) near a tile
local function tower_near(x, y, r)
    local t = ai.memory{buildings = true, near = {x, y, r}, limit = 8}
    for _, m in ipairs(t) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            return true
        end
    end
    return false
end

-- the candidates a group would fight now: records with kind and priority, nearest first
local function candidates(g, cx, cy, radius)
    local r = radius
    local f = {enemy = true, near = {cx, cy, r}}
    if g.task == "attack" or g.task == "raid" then
        if g.target ~= 0 then
            f.player = g.target
        end
    end
    local ids = ai.seen(f)
    local out = {}
    -- [attack.c] the leash (g.leash): an enemy inside known defences is no target unless the group's own place lies
    -- inside them too (defenders waiting by their towers drew the whole army in); the focus (g.focus_x/y, the breach
    -- segment): that wall piece first, its neighbours next
    local leash = nil
    if g.leash and g.task == "attack" and g.x >= 0 then
        leash = defences_near(cx, cy, M.LEASH_SCAN)
        if defended(leash, g.x, g.y) then
            leash = nil
        end
    end
    local fx, fy = g.focus_x or -1, g.focus_y or -1
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil then
            local kind = kind_of(u)
            if kind ~= nil then
                local dist = isqrt(d2(u.x, u.y, cx, cy))
                local p = priority(g.task, kind, dist)
                if p ~= nil and (g.task ~= "raid" or kind == "soldier" or not tower_near(u.x, u.y, M.TOWER_AVOID)) then
                    if (kind == "ship" or kind == "boat") and dist > M.BOAT_RANGE then
                        p = nil
                    end
                end
                if p ~= nil and kind == "wall" and fx >= 0 and math.abs(u.x - fx) <= 1 and math.abs(u.y - fy) <= 1 then
                    p = (u.x == fx and u.y == fy) and -2 or -1
                elseif p ~= nil and leash ~= nil and kind ~= "wall" and defended(leash, u.x, u.y) then
                    p = nil
                    g.leashed = (g.leashed or 0) + 1
                end
                if p ~= nil then
                    out[#out + 1] = {u = u, kind = kind, dist = dist, p = p}
                end
            end
        end
    end
    -- defend: the enemies within the radius of the place count too
    if g.task == "defend" and g.x >= 0 and (g.x ~= cx or g.y ~= cy) then
        local more = ai.seen{enemy = true, near = {g.x, g.y, g.radius}}
        for _, id in ipairs(more) do
            local have = false
            for _, c in ipairs(out) do
                if c.u.id == id then
                    have = true
                end
            end
            if not have then
                local u = ai.unit(id)
                local kind = u ~= nil and kind_of(u) or nil
                if kind ~= nil then
                    local dist = isqrt(d2(u.x, u.y, cx, cy))
                    local p = priority(g.task, kind, dist)
                    if p ~= nil and ((kind ~= "ship" and kind ~= "boat") or dist <= M.BOAT_RANGE) then
                        out[#out + 1] = {u = u, kind = kind, dist = dist, p = p}
                    end
                end
            end
        end
    end
    table.sort(out, function(a, b)
        if a.p ~= b.p then
            return a.p < b.p
        end
        if a.dist ~= b.dist then
            return a.dist < b.dist
        end
        return a.u.id < b.u.id
    end)
    return out
end

-- gives each member a target among the candidates: a unit keeps its aim while it is still a candidate; melee units
-- never take a ship or boat. Returns the members fighting, and whether a land target exists
local function fight(g, recs, cands)
    if #cands == 0 then
        return 0, nil
    end
    local by_target = {}
    local fighting = 0
    local land = nil
    for _, c in ipairs(cands) do
        if land == nil and c.kind ~= "ship" and c.kind ~= "boat" then
            land = c
        end
    end
    local taken = {}
    local hurts = {}
    for _, u in ipairs(recs) do
      if M.committed[u.id] then
        fighting = fighting + 1
      else
        local ranged = (u.range or 0) >= 150
        local aim = g.aims[u.id]
        local keep = nil
        if aim ~= nil then
            for _, c in ipairs(cands) do
                if c.u.id == aim and (ranged or (c.kind ~= "ship" and c.kind ~= "boat")) then
                    keep = c
                end
            end
        end
        if keep == nil then
            -- the best candidate by priority, then by distance to this unit (one already taken counts further)
            local best, bs = nil, nil
            -- [attack.c] aircraft beyond the unit's range + 3 are no target (the march waited for planes it chased);
            -- a ram takes no building while an enemy soldier guards it within 5 tiles (helpless against soldiers)
            local ram = string.find(u.type or "", "ram", 1, true) ~= nil and u.category == "soldier"
            local reach = (u.range or 0) // 100 + 3
            -- [mass.c] TETHER (g.tether, the pack at g.tether_x/y; army_mass 16): no target more than tether tiles
            -- from the pack unless within the unit's own range + 2; 128: buildings only
            local tether = g.tether
            local treach = (u.range or 0) // 100 + 2
            for _, c in ipairs(cands) do
                local skip = c.u.category == "air" and d2(u.x, u.y, c.u.x, c.u.y) > reach * reach
                if not skip and tether ~= nil and (not g.tether_buildings or c.kind == "building" or c.kind == "tower" or c.kind == "wall")
                        and d2(c.u.x, c.u.y, g.tether_x, g.tether_y) > tether * tether
                        and d2(c.u.x, c.u.y, u.x, u.y) > treach * treach then
                    skip = true
                    g.tethered = (g.tethered or 0) + 1
                end
                if not skip and ram and (c.kind == "building" or c.kind == "tower" or c.kind == "wall") then
                    for _, s in ipairs(cands) do
                        if s.kind == "soldier" and d2(s.u.x, s.u.y, c.u.x, c.u.y) <= 25 then
                            skip = true
                            break
                        end
                    end
                end
                -- g.can_hurt (opt-in): only a target the unit's weapon hurts (ai.can_hurt; the classic's submarines)
                if not skip and g.can_hurt and ai.can_hurt ~= nil then
                    local key = u.type .. ">" .. c.u.type
                    local h = hurts[key]
                    if h == nil then
                        h = ai.can_hurt(u.type, c.u.type)
                        hurts[key] = h
                    end
                    skip = not h
                end
                if not skip and (ranged or (c.kind ~= "ship" and c.kind ~= "boat")) then
                    local s = c.p * 10000 + isqrt(d2(u.x, u.y, c.u.x, c.u.y)) * 10 + (taken[c.u.id] or 0)
                    if bs == nil or s < bs then
                        best, bs = c, s
                    end
                end
            end
            keep = best
        end
        if keep ~= nil then
            taken[keep.u.id] = (taken[keep.u.id] or 0) + 15
            if g.aims[u.id] ~= keep.u.id or u.action ~= "attack" then
                by_target[keep.u.id] = by_target[keep.u.id] or {}
                local list = by_target[keep.u.id]
                list[#list + 1] = u.id
            end
            g.aims[u.id] = keep.u.id
            fighting = fighting + 1
        else
            g.aims[u.id] = nil
        end
      end
    end
    for _, tid in ipairs(ai.keys(by_target)) do
        ai.attack(by_target[tid], tid)
    end
    return fighting, land
end

-- the units not fighting that stand within SHIP_AVOID of a seen warship walk out of its reach
local function avoid_ships(g, recs, cands)
    local ships = {}
    for _, c in ipairs(cands) do
        if c.kind == "ship" then
            ships[#ships + 1] = c.u
        end
    end
    if #ships == 0 then
        return
    end
    local moves = {}
    for _, u in ipairs(recs) do
        if g.aims[u.id] == nil then
            local best, bd = nil, nil
            for _, sh in ipairs(ships) do
                local d = d2(u.x, u.y, sh.x, sh.y)
                if bd == nil or d < bd then
                    best, bd = sh, d
                end
            end
            if best ~= nil and bd <= M.SHIP_AVOID * M.SHIP_AVOID then
                local tx, ty = g.x, g.y
                if tx < 0 or d2(tx, ty, best.x, best.y) <= M.SHIP_AVOID * M.SHIP_AVOID then
                    local dx, dy = u.x - best.x, u.y - best.y
                    local d = isqrt(bd)
                    if d < 1 then
                        d, dx = 1, 1
                    end
                    tx = best.x + dx * (M.SHIP_AVOID + 2) // d
                    ty = best.y + dy * (M.SHIP_AVOID + 2) // d
                end
                if tx >= 0 and ty >= 0 then
                    local key = tx * 10000 + ty
                    moves[key] = moves[key] or {}
                    local l = moves[key]
                    l[#l + 1] = u.id
                end
            end
        end
    end
    for _, key in ipairs(ai.keys(moves)) do
        ai.move(moves[key], key // 10000, key % 10000, {forced = true})
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Movement

local function set_state(g, state)
    if g.state ~= state then
        g.state = state
        g.state_t = ai.time()
    end
end

-- [research §4.4, AI_RESEARCH §8.41] g.route (opt-in): the legs follow ai.route's waypoints round the danger map's
-- threat (known towers and soldiers), weighted g.route_avoid (default 8); the route is asked again for a new place or
-- after 30 s. -> the next waypoint at least 5 tiles from the centre, or the place itself
local function routed(g, cx, cy, x, y)
    local now = ai.time()
    if g.path == nil or g.path_tx ~= x or g.path_ty ~= y or now - (g.path_t or 0) >= 30000 then
        local path = ai.route(cx, cy, x, y, {avoid = g.route_avoid or 8})
        g.path, g.path_tx, g.path_ty, g.path_t, g.path_k = {}, x, y, now, 1
        if path ~= nil then
            for k, w in ipairs(path) do
                g.path[k] = {w[1], w[2]}
            end
        end
    end
    while g.path_k <= #g.path and near(cx, cy, g.path[g.path_k][1], g.path[g.path_k][2], 5) do
        g.path_k = g.path_k + 1
    end
    if g.path_k <= #g.path then
        return g.path[g.path_k][1], g.path[g.path_k][2]
    end
    return x, y
end

-- a leg of LEG tiles from the centre towards (x, y), given again when the leg moved 4 tiles or after MOVE_MS
local function move(g, cx, cy, x, y, forced)
    local now = ai.time()
    if g.route then
        x, y = routed(g, cx, cy, x, y)
    end
    local dist = isqrt(d2(cx, cy, x, y))
    local lx, ly = x, y
    if dist > M.LEG then
        lx = cx + (x - cx) * M.LEG // dist
        ly = cy + (y - cy) * M.LEG // dist
    end
    if g.way_t ~= 0 and now - g.way_t < M.MOVE_MS and near(lx, ly, g.way_x, g.way_y, 4) then
        return
    end
    g.way_x, g.way_y, g.way_t = lx, ly, now
    local idle = {}
    for _, id in ipairs(g.units) do
        if not M.committed[id] then
            idle[#idle + 1] = id
        end
    end
    if #idle > 0 then
        ai.move(idle, lx, ly, {forced = forced and true or false})
    end
end

local function scout_step(g, recs, cx, cy, view)
    if g.state == "done" then
        return
    end
    local now = ai.time()
    local need = g.x < 0
    if not need then
        local t = ai.tile(g.x, g.y)
        need = t == nil or t.explored or near(cx, cy, g.x, g.y, 3)
        if not need and now - g.state_t > M.SCOUT_STUCK_MS then
            if #g.skips < 64 then
                g.skips[#g.skips + 1] = {g.x, g.y}
            end
            need = true
        end
    end
    if not need then
        if g.way_t == 0 then   -- the first place the order named: on its way there
            g.way_t = now
            ai.move(g.units, g.x, g.y, {forced = true})
        end
        return
    end
    local lead = recs[1]
    local land = ai.tile(lead.x, lead.y).continent
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        tx, ty = cx, cy
    end
    local nx, ny = ai.unexplored(cx, cy, {land = land, grid = M.SCOUT_GRID, away = {tx, ty}, skip = g.skips})
    if nx == nil then
        g.x, g.y = tx, ty
        ai.move(g.units, tx, ty, {forced = true})
        set_state(g, "done")
        return
    end
    g.x, g.y = nx, ny
    g.state_t = now
    g.state = "moving"
    ai.move(g.units, nx, ny, {forced = true})
end

-- the enemy's gather spot last seen with no known tower near, else its town (view.enemies)
local function enemy_place(view, mem, player, not_x, not_y)
    local spots = (mem.gspots or {})[player]
    if spots ~= nil then
        local best = nil
        for _, k in ipairs(ai.keys(spots)) do
            local s = spots[k]
            if not (near(s.x, s.y, not_x, not_y, M.ARRIVE)) and not tower_near(s.x, s.y, M.TOWER_AVOID)
                    and (best == nil or s.t > best.t) then
                best = s
            end
        end
        if best ~= nil then
            return best.x, best.y
        end
    end
    for _, e in ipairs(view.enemies or {}) do
        if e.player == player and e.town_x >= 0 and not near(e.town_x, e.town_y, not_x, not_y, M.ARRIVE) then
            return e.town_x, e.town_y
        end
    end
    return nil
end

-- the enemies' gather spots (citizens or boats seen; one spot per 8 tiles, 8 per player; forgotten when seen empty
-- for a minute). Cheap: one ai.seen over citizens and boats a step
function M.note_spots(mem)
    mem.gspots = mem.gspots or {}
    local now = ai.time()
    local ids = ai.seen{enemy = true, category = "citizen"}
    local boats = ai.seen{enemy = true, category = "boat"}
    for _, b in ipairs(boats) do
        ids[#ids + 1] = b
    end
    local cols = ai.get(ids, {"x", "y", "player"})
    for k = 1, #ids do
        local x, y, p = cols.x[k], cols.y[k], cols.player[k]
        if x ~= nil then
            mem.gspots[p] = mem.gspots[p] or {}
            local spots = mem.gspots[p]
            local found = nil
            for _, key in ipairs(ai.keys(spots)) do
                if near(spots[key].x, spots[key].y, x, y, 8) then
                    found = spots[key]
                end
            end
            if found ~= nil then
                found.t = now
            else
                local keys = ai.keys(spots)
                if #keys >= 8 then
                    -- the oldest goes
                    local old = nil
                    for _, key in ipairs(keys) do
                        if old == nil or spots[key].t < spots[old].t then
                            old = key
                        end
                    end
                    spots[old] = nil
                end
                spots[x * 10000 + y] = {x = x, y = y, t = now}
            end
        end
    end
    -- forget a spot seen empty for a minute (its tile is in sight and nobody was seen there)
    for _, p in ipairs(ai.keys(mem.gspots)) do
        local spots = mem.gspots[p]
        for _, key in ipairs(ai.keys(spots)) do
            local s = spots[key]
            if now - s.t > 60000 then
                local t = ai.tile(s.x, s.y)
                if t ~= nil and t.visible then
                    spots[key] = nil
                end
            end
        end
    end
end

-- one group's step
local function group_step(g, view, mem)
    local recs, cx, cy, str = members(g)
    if #g.units == 0 then
        print(string.format("group gone id=%d task=%s lost=%d", g.id, g.task, g.lost or 0))
        mem.groups[g.id] = nil
        return
    end
    if cx == nil then
        return   -- every unit is carried or garrisoned
    end
    g.cx, g.cy, g.strength = cx, cy, str
    if g.manual then
        return   -- [armies.c det, skirmish.c] its module orders it
    end
    if g.task == "scout" then
        scout_step(g, recs, cx, cy, view)
        return
    end
    local tx, ty = view.town_x, view.town_y
    local home_x, home_y = g.home_x, g.home_y
    if home_x < 0 then
        home_x, home_y = tx, ty
    end
    if g.task == "retreat" then
        if near(cx, cy, g.x, g.y, M.ARRIVE + 2) then
            g.task = "defend"
            g.radius = (tx >= 0 and g.x == tx and g.y == ty) and view.claim_range or 12
            g.way_t = 0
            set_state(g, "arrived")
            return
        end
        move(g, cx, cy, g.x, g.y, true)
        return
    end
    -- a raid that meets more than it can fight goes home
    if g.task == "raid" and home_x >= 0 then
        local foes = ai.seen{enemy = true, armed = true, near = {cx, cy, 10}}
        local enemy = 0
        for _, id in ipairs(foes) do
            local u = ai.unit(id)
            if u ~= nil and (u.category == "soldier" or u.category == "building") then
                enemy = enemy + u.hp * (u.attack or 0)
            end
        end
        if enemy * 2 > str * 3 then
            g.task = "retreat"
            g.x, g.y = home_x, home_y
            g.way_t = 0
            g.aims = {}
            set_state(g, "moving")
            print(string.format("group flee id=%d enemy=%d own=%d", g.id, enemy, str))
            move(g, cx, cy, g.x, g.y, true)
            return
        end
    end
    local cands = candidates(g, cx, cy, g.task == "defend" and 6 or M.ENGAGE)
    local fighting, land = fight(g, recs, cands)
    if fighting > 0 then
        set_state(g, "fighting")
        local others = false
        for _, u in ipairs(recs) do
            if g.aims[u.id] == nil then
                others = true
            end
        end
        if others then
            if land ~= nil then
                move(g, cx, cy, land.u.x, land.u.y, false)
            else
                avoid_ships(g, recs, cands)
            end
        end
        return
    end
    g.aims = {}
    local ships = false
    for _, c in ipairs(cands) do
        if c.kind == "ship" then
            ships = true
        end
    end
    if ships then
        avoid_ships(g, recs, cands)
        return
    end
    if g.state == "fighting" then
        g.way_t = 0
    end
    if g.state == "forming" then
        local n, count = 0, 0
        for _, u in ipairs(recs) do
            count = count + 1
            if near(u.x, u.y, g.way_x, g.way_y, 6) then
                n = n + 1
            end
        end
        if ai.time() - g.state_t < M.FORM_MS and n * 5 < count * 4 then
            return
        end
        g.way_t = 0
        set_state(g, "moving")
    end
    if g.task == "defend" then
        if g.x < 0 then
            g.x, g.y = cx, cy
        end
        if near(cx, cy, g.x, g.y, M.ARRIVE) then
            set_state(g, "arrived")
            local strays = {}
            for _, u in ipairs(recs) do
                if not near(u.x, u.y, g.x, g.y, 10) and u.action == "idle" then
                    strays[#strays + 1] = u.id
                end
            end
            if #strays > 0 then
                ai.move(strays, g.x, g.y, {forced = false})
            end
            return
        end
        if g.state == "arrived" then
            set_state(g, "moving")
        end
        move(g, cx, cy, g.x, g.y, false)
        return
    end
    -- attack and raid
    if g.x < 0 or near(cx, cy, g.x, g.y, M.ARRIVE) then
        local nx, ny = nil, nil
        if g.task == "raid" then
            nx, ny = enemy_place(view, mem, g.target, g.x, g.y)
        else
            -- the target's nearest unit seen within 30 tiles, else its town as last seen
            local f = {enemy = true, near = {cx, cy, 30}}
            if g.target ~= 0 then
                f.player = g.target
            end
            local ids = ai.seen(f)
            for _, id in ipairs(ids) do
                local u = ai.unit(id)
                if u ~= nil and u.category ~= "boat" and u.category ~= "ship" and u.category ~= "resource"
                        and u.category ~= "animal" and u.category ~= "ambient" then
                    nx, ny = u.x, u.y
                    break
                end
            end
            if nx == nil then
                for _, e in ipairs(view.enemies or {}) do
                    if e.player == g.target and e.town_x >= 0 and not near(e.town_x, e.town_y, g.x, g.y, M.ARRIVE) then
                        nx, ny = e.town_x, e.town_y
                    end
                end
            end
        end
        if nx == nil then
            set_state(g, "arrived")
            return
        end
        g.x, g.y = nx, ny
        g.way_t = 0
        print(string.format("group next id=%d task=%s to=%d,%d", g.id, g.task, nx, ny))
    end
    set_state(g, "moving")
    move(g, cx, cy, g.x, g.y, false)
end

-- every group's step: call once a strategy step, after the economy's orders
function M.step(view, mem)
    M.init(mem)
    M.note_spots(mem)
    for _, g in ipairs(M.all(mem)) do
        group_step(g, view, mem)
    end
    if view.time % 30000 < 1000 then
        for _, g in ipairs(M.all(mem)) do
            print(string.format("group id=%d task=%s state=%s units=%d str=%d x=%d y=%d tx=%d ty=%d lost=%d", g.id, g.task,
                g.state, #g.units, g.strength or 0, g.cx or -1, g.cy or -1, g.x, g.y, g.lost or 0))
        end
    end
end

-- the enemy to fight: the one with the largest economy seen (the most citizens and boats seen at once), a known town
-- first; nil when none is known
function M.pick_enemy(view, mem)
    mem.econ = mem.econ or {}
    for _, e in ipairs(view.enemies or {}) do
        if (mem.econ[e.player] or 0) < e.citizens_seen then
            mem.econ[e.player] = e.citizens_seen
        end
    end
    local best, best_score = nil, -1
    for _, e in ipairs(view.enemies or {}) do
        local spots = (mem.gspots or {})[e.player]
        if e.town_x >= 0 or (spots ~= nil and #ai.keys(spots) > 0) then
            local score = (mem.econ[e.player] or 0) * 10 + e.epoch * 100 + (e.town_x >= 0 and 1 or 0)
            if score > best_score then
                best, best_score = e, score
            end
        end
    end
    return best
end

-- the strength and count of the groups of a task (nil: every group)
function M.strength(mem, task)
    local s, n = 0, 0
    for _, g in ipairs(M.all(mem)) do
        if task == nil or g.task == task then
            s = s + (g.strength or 0)
            n = n + #g.units
        end
    end
    return s, n
end

-- retreat every group but the scouts (or one group) to (x, y), the town by default
function M.retreat(view, mem, g, x, y)
    x = x or view.town_x
    y = y or view.town_y
    if x == nil or x < 0 then
        return
    end
    local list = g ~= nil and {g} or M.all(mem)
    for _, grp in ipairs(list) do
        if grp.task ~= "scout" then
            grp.task = "retreat"
            grp.x, grp.y = x, y
            grp.way_t = 0
            grp.aims = {}
            set_state(grp, "moving")
        end
    end
end

return M
