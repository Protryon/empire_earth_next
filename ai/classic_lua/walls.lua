-- EECPWallManager in Lua (the classic port; ORIGIN in this folder): from the Copper Age, a wall across the way from
-- each enemy's centre to each own town centre or capitol, nearest enemy first, one every 30 s once the last one's
-- gate stands. The way is cut at a fraction of its length (the size table: 0.45-0.55, 0.3-0.4 or 0.15-0.25 of the
-- way, a wall of 20, 30 or 40 tiles; the fractions in thousandths), each step looked at across (ai.wall_stretches: the open stretches between
-- forests, cliffs and water, a land building near blocks it), the shortest open level stretch kept; none: the next
-- size, the 9th size the stretch nearest a gate. A finished wall gets its gate in the middle (ai.gate).
-- require("classic_lua.walls").
--
-- DEVIATIONS: the way's points are ai.route's (8-tile cells), not the wall planner path's (each step refined as the
-- classic's, ai.wall_path); the enemy's
-- centre is the town the view knows (its capitol in the original); a wall piece lost is not queued again
-- (UnitLost's QueueBuildingAt); a stretch with units on it is not left out (EEGetUnitsOn).
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local C = T.C

local M = {}

local function U()
    return require("classic_lua.units")
end

local function B()
    return require("classic_lua.build")
end

local d2 = R.d2

local WALLS = {[2] = "b_wall_copper", [3] = "b_wall_bronze", [4] = "b_wall_bronze", [5] = "b_wall_middle",
               [6] = "b_wall_middle", [7] = "b_wall_imperial", [8] = "b_wall_imperial", [9] = "b_wall_ww1",
               [10] = "b_wall_ww1", [11] = "b_wall_ww1", [12] = "b_wall_digital", [13] = "b_wall_digital"}

-- the size table (EECPWallManager::Update's first call): two players or fewer on a map with water, the middle of
-- the way first; else the end nearest the enemy first
local function sizes(memory)
    local s = {}
    local fr = {{450, 550}, {300, 400}, {150, 250}}
    local computers = 0
    for _, p in ipairs(ai.players()) do
        if not p.human and p.id > 0 then
            computers = computers + 1
        end
    end
    if memory.t.waterless or computers > 2 then
        fr = {{150, 250}, {300, 400}, {450, 550}}
    end
    for _, f in ipairs(fr) do
        for _, len in ipairs({20, 30, 40}) do
            s[#s + 1] = {a = f[1], b = f[2], len = len}
        end
    end
    return s
end

local function state(memory)
    local w = memory.walls
    if w == nil then
        w = {queue = {}, next_time = 0, size = 1, runs = {}, plan = nil, gate_x = 0, gate_y = 0, seen = {}}
        memory.walls = w
    end
    return w
end

-- the enemy's centre (GetCenter: its capitol, else its first unit): the town the view knows, else the building of the
-- player the seat remembers (ai.memory). DEVIATION (fog): the classic reads every unit of the player
local function enemy_centre(view, player)
    for _, e in ipairs(view.enemies or {}) do
        if e.player == player and e.town_x ~= nil and e.town_x >= 0 then
            return e.town_x, e.town_y
        end
    end
    -- (its capitol first, then a town centre, then any of its buildings the seat remembers; a unit seen on the way
    -- is no centre)
    local best, rank
    for _, m in ipairs(ai.memory{player = player}) do
        local r = m.type == C.capitol and 0 or m.type == C.town_center and 1 or (R.class_of(m.type).building and 2 or nil)
        if r ~= nil and (best == nil or r < rank or (r == rank and m.id < best.id)) then
            best, rank = m, r
        end
    end
    if best ~= nil then
        return best.x, best.y
    end
    return nil
end

-- AddWalls: the unit's plans again, one an enemy with a known centre, the farthest first (taken from the back)
local function add_walls(view, memory, uid)
    local w = state(memory)
    local u = R.unit(uid)
    if u == nil then
        return
    end
    local keep = {}
    for _, q in ipairs(w.queue) do
        if q.unit ~= uid then
            keep[#keep + 1] = q
        end
    end
    local enemies = {}
    for _, e in ipairs(view.enemies or {}) do
        local ex, ey = enemy_centre(view, e.player)
        if ex ~= nil then
            enemies[#enemies + 1] = {player = e.player, d = d2(ex, ey, u.x, u.y)}
        end
    end
    table.sort(enemies, function(a, b)
        if a.d ~= b.d then
            return a.d > b.d
        end
        return a.player < b.player
    end)
    for _, e in ipairs(enemies) do
        keep[#keep + 1] = {unit = uid, enemy = e.player}
    end
    w.queue = keep
    w.next_time = view.time + 30000
end

-- the way from the unit to the enemy's centre, in tiles: ai.route's waypoints, each step refined as the classic
-- refines its wall planner path (ai.wall_path: the path manager's FindPath with the wall pather, searched from the
-- later point back to the earlier one; a step it cannot path is a straight line)
local function the_way(uid, fx, fy, tx, ty)
    local pts = ai.route(fx, fy, tx, ty, {}) or {}
    local way = {}
    table.insert(pts, 1, {fx, fy})
    pts[#pts + 1] = {tx, ty}
    for k = 2, #pts do
        local a, b = pts[k - 1], pts[k]
        local seg = ai.wall_path ~= nil and ai.wall_path(uid, b[1], b[2], a[1], a[2]) or {}
        if #seg < 2 then
            seg = ai.wall_line(a[1], a[2], b[1], b[2])
        end
        for j = 1, #seg - 1 do
            way[#way + 1] = seg[j]
        end
    end
    way[#way + 1] = {tx, ty}
    return way
end

-- the way from the wall planner's own path (ai.wall_route: the path manager's coarse request with the planner's
-- influence costs, from the enemy's centre to the town): reversed, its ends the unit and the centre, each step refined
-- with ai.wall_path from the later tile back to the earlier one (EECPWallManager::PlanWall)
local function route_way(uid, path, fx, fy, tx, ty)
    local pts = {}
    for k = #path, 1, -1 do
        pts[#pts + 1] = {path[k][1], path[k][2]}
    end
    if #pts < 2 then
        return nil
    end
    pts[1] = {fx, fy}
    pts[#pts] = {tx, ty}
    local way = {}
    for k = 2, #pts do
        local a, b = pts[k - 1], pts[k]
        local seg = ai.wall_path(uid, b[1], b[2], a[1], a[2])
        if #seg < 2 then
            seg = ai.wall_line(a[1], a[2], b[1], b[2])
        end
        for j = 1, #seg - 1 do
            way[#way + 1] = seg[j]
        end
    end
    way[#way + 1] = {tx, ty}
    return way
end

local function line_cost_ok(view, memory, ax, ay, bx, by)
    local class = WALLS[view.epoch]
    if class == nil then
        return false
    end
    local n = #ai.wall_line(ax, ay, bx, by)
    return U().can_afford(view, memory, U().cost(class), n, class)
end

local function gate_near(x, y, r2)
    for _, id in ipairs(ai.units{category = "building"}) do
        local u = R.unit(id)
        if u ~= nil and u.type:find("gate") ~= nil and d2(u.x, u.y, x, y) <= r2 then
            return true
        end
    end
    return false
end

local function wall_near(x, y, r2)
    for _, id in ipairs(ai.units{category = "building"}) do
        local u = R.unit(id)
        if u ~= nil and u.type:find("wall") ~= nil and d2(u.x, u.y, x, y) <= r2 then
            return true
        end
    end
    return false
end

-- IsRunClear: the line 0.75 drop-off distances from every resource centre (unless ignored), and across the way
local function run_clear(memory, ax, ay, bx, by, way_set, ignore_centres)
    local line = ai.wall_line(ax, ay, bx, by)
    if not ignore_centres then
        local r = T.b(memory, "drop_off_distance") * 3 // 4
        for _, c in ipairs(R.state(memory).centres) do
            for _, t in ipairs(line) do
                if d2(c.x, c.y, t[1], t[2]) <= r * r then
                    return false
                end
            end
        end
    end
    for _, t in ipairs(line) do
        if way_set[t[1] * 65536 + t[2]] then
            return true
        end
    end
    return false
end

-- PlanWall: the way's stretch of this size looked at across, step by step
local function plan_wall(view, memory, w, way, ux, uy)
    local sz = sizes(memory)[w.size]
    local half = sz.len // 2
    local way_set = {}
    for _, t in ipairs(way) do
        way_set[t[1] * 65536 + t[2]] = true
    end
    local first = #way * sz.a // 1000
    local last = #way * sz.b // 1000
    local runs = {}
    for i = math.max(1, first), last - 1 do
        local f, t = way[i], way[i + 1]
        if f == nil or t == nil then
            break
        end
        local sx, sy, ex, ey
        if f[1] == t[1] then
            sx, ex, sy, ey = f[1] - half, f[1] + half, f[2], f[2]
        elseif f[2] == t[2] then
            sx, ex, sy, ey = f[1], f[1], f[2] - half, f[2] + half
        else
            local endx, off
            if (f[1] < t[1] and f[2] < t[2]) or (t[1] < f[1] and t[2] < f[2]) then
                endx, off = half, -half
            else
                endx, off = -half, half
            end
            sx, sy = f[1] + off, f[2] + half
            ex, ey = f[1] + endx, f[2] - half
        end
        local blocked, stretches = ai.wall_stretches(sx, sy, ex, ey)
        if not blocked then
            local best, limit = nil, 99999
            for _, st in ipairs(stretches) do
                if st.len > 4 and st.len < limit and st.level and run_clear(memory, st.ax, st.ay, st.bx, st.by, way_set, false)
                        and not gate_near(st.ax, st.ay, 10) and not gate_near(st.bx, st.by, 10) then
                    best, limit = st, st.len
                end
            end
            if best ~= nil then
                runs[#runs + 1] = best
                w.runs = {}
            else
                for _, st in ipairs(stretches) do
                    local key = st.ax .. "," .. st.ay .. "," .. st.bx .. "," .. st.by
                    if run_clear(memory, st.ax, st.ay, st.bx, st.by, way_set, true) and not w.seen[key]
                            and not wall_near(st.ax, st.ay, 100) and not gate_near(st.ax, st.ay, 10) then
                        w.seen[key] = true
                        w.runs[#w.runs + 1] = st
                    end
                end
            end
        end
    end
    if #runs == 0 then
        return nil
    end
    -- ChooseRun: nearest the unit first (similar distances: shorter first), the first affordable
    local len2 = sz.len * sz.len
    table.sort(runs, function(a, b)
        local da = d2((a.ax + a.bx) // 2, (a.ay + a.by) // 2, ux, uy)
        local db = d2((b.ax + b.bx) // 2, (b.ay + b.by) // 2, ux, uy)
        if math.abs(da - db) > len2 then
            return da < db
        end
        if a.len ~= b.len then
            return a.len < b.len
        end
        return a.ax * 65536 + a.ay < b.ax * 65536 + b.ay
    end)
    for _, r in ipairs(runs) do
        if line_cost_ok(view, memory, r.ax, r.ay, r.bx, r.by) then
            return r
        end
    end
    return nil
end

-- ChooseRunNearGates: the affordable run nearest a gate, else nearest the town
local function run_near_gates(view, memory, runs)
    local tx, ty = R.town(view)
    local px, py = tx, ty
    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = R.unit(id)
        if u ~= nil and u.type:find("gate") ~= nil then
            px, py = u.x, u.y
            break
        end
    end
    if px == nil then
        return nil
    end
    table.sort(runs, function(a, b)
        local da, db = d2(a.ax, a.ay, px, py), d2(b.ax, b.ay, px, py)
        if da ~= db then
            return da < db
        end
        return a.ax * 65536 + a.ay < b.ax * 65536 + b.ay
    end)
    for _, r in ipairs(runs) do
        if line_cost_ok(view, memory, r.ax, r.ay, r.bx, r.by) then
            return r
        end
    end
    return nil
end

-- BuildWall: a builder for every 10 tiles (the nearest available citizens), the wall ordered
local function build_wall(view, memory, w, r)
    local n = math.max(1, math.ceil(math.sqrt(d2(r.ax, r.ay, r.bx, r.by)) / 10))
    local ids = {}
    for _ = 1, n do
        local b = B().find_builder(memory, r.ax, r.ay, C.house)
        if b == nil then
            break
        end
        B().used(b.id)
        ids[#ids + 1] = b.id
    end
    if #ids == 0 then
        return false
    end
    local code = ai.wall(ids, "wall", r.ax, r.ay, r.bx, r.by)
    print(string.format("classic_lua wall.build at=%d,%d-%d,%d builders=%d result=%s", r.ax, r.ay, r.bx, r.by, #ids,
        tostring(code)))
    if code ~= "ok" then
        return false
    end
    w.plan = {ax = r.ax, ay = r.ay, bx = r.bx, by = r.by, t = view.time}
    return true
end

-- the planned wall's pieces: built, still foundations
local function plan_pieces(w)
    local built, open = 0, 0
    for _, t in ipairs(ai.wall_line(w.plan.ax, w.plan.ay, w.plan.bx, w.plan.by)) do
        for _, id in ipairs(ai.units{category = "building", near = {t[1], t[2], 0}}) do
            local u = R.unit(id)
            if u ~= nil and u.type:find("wall") ~= nil then
                if u.built then
                    built = built + 1
                else
                    open = open + 1
                end
            end
        end
    end
    return built, open
end

-- AddGates (the planned wall finished): the gate on the tile nearest the wall's middle where one fits
local function place_gate(view, memory, w)
    local line = ai.wall_line(w.plan.ax, w.plan.ay, w.plan.bx, w.plan.by)
    local mid = line[#line // 2 + 1] or line[1]
    table.sort(line, function(a, b)
        local da, db = d2(a[1], a[2], mid[1], mid[2]), d2(b[1], b[2], mid[1], mid[2])
        if da ~= db then
            return da < db
        end
        return a[1] * 65536 + a[2] < b[1] * 65536 + b[2]
    end)
    for _, t in ipairs(line) do
        local code = ai.gate("gate", t[1], t[2])
        if code == "ok" then
            w.gate_x, w.gate_y = t[1], t[2]
            print(string.format("classic_lua wall.gate at=%d,%d", t[1], t[2]))
            return
        end
    end
    w.gate_x, w.gate_y = mid[1], mid[2]
end

-- StartNextWall + the finished request: the last plan's way, its wall, else the next size. The way is the wall
-- planner's path request (ai.wall_route, answered over the next ticks: the plan waits for it, up to a minute), else
-- ai.route's (the old way, also after a loaded game)
local ROUTE_WAIT_MS = 60000

local function next_wall(view, memory, w)
    local q = table.remove(w.queue)
    if q == nil then
        return
    end
    local u = R.unit(q.unit)
    local ex, ey = enemy_centre(view, q.enemy)
    if u == nil or ex == nil then
        return
    end
    local way = q.way   -- (the next size tries the same way, as the C++'s PlanWall the same request path)
    q.way = nil
    if way == nil and ai.wall_route ~= nil then
        if q.route == nil then
            q.route = ai.wall_route(q.enemy, ex, ey, u.x, u.y)
            q.route_t = view.time
            if q.route ~= nil then
                w.queue[#w.queue + 1] = q
                w.next_time = view.time + 1000
                return
            end
        else
            local path = ai.wall_route_result(q.route)
            if path == nil and view.time - (q.route_t or 0) < ROUTE_WAIT_MS then
                w.queue[#w.queue + 1] = q
                w.next_time = view.time + 1000
                return
            end
            q.route = nil
            if type(path) == "table" then
                way = route_way(q.unit, path, u.x, u.y, ex, ey)
            end
        end
    end
    local src = "route"
    if way == nil then
        way = the_way(q.unit, u.x, u.y, ex, ey)
        src = "cells"
    end
    if w.size == 1 then
        print(string.format("classic_lua wall.way %s tiles=%d", src, #way))
    end
    if #way < 10 then
        return
    end
    if B().find_builder(memory, u.x, u.y, C.house) == nil then
        w.queue[#w.queue + 1] = q
        w.next_time = view.time + 5000
        return
    end
    local r = plan_wall(view, memory, w, way, u.x, u.y)
    if r ~= nil then
        if build_wall(view, memory, w, r) then
            w.size = 1
            w.runs = {}
            add_walls(view, memory, q.unit)
        end
        return
    end
    w.size = w.size + 1
    if w.size > 9 then
        w.size = 1
        local rr = run_near_gates(view, memory, w.runs)
        if rr ~= nil and build_wall(view, memory, w, rr) then
            w.runs = {}
            add_walls(view, memory, q.unit)
        end
    else
        q.way = way
        w.queue[#w.queue + 1] = q
    end
end

-- Update: every step (the wall's own timers inside)
function M.update(view, memory)
    if view.epoch < 2 or WALLS[view.epoch] == nil or ai.wall_stretches == nil then
        return
    end
    local w = state(memory)
    -- the own town centres and capitols: their plans (AddGates of a town centre or capitol: AddWalls)
    for _, t in ipairs({C.town_center, C.capitol}) do
        for _, id in ipairs(ai.units{type = t, built = true}) do
            if not w.seen["u" .. id] then
                w.seen["u" .. id] = true
                add_walls(view, memory, id)
            end
        end
    end
    if w.plan ~= nil then
        -- (the planned wall's pieces under way: no new wall; all built: the gate)
        local built, open = plan_pieces(w)
        if open == 0 and (built > 0 or view.time - w.plan.t > 120000) then
            if built > 0 then
                place_gate(view, memory, w)
            end
            w.plan = nil
        else
            return
        end
    end
    if view.time < w.next_time then
        return
    end
    if #w.queue == 0 then
        -- (DEVIATION, fog: the classic queued its plans at the start, every enemy known; the port again for its
        -- town centres and capitols whenever its queue is empty, for the enemies it has met since)
        for _, t in ipairs({C.town_center, C.capitol}) do
            for _, id in ipairs(ai.units{type = t, built = true}) do
                add_walls(view, memory, id)
            end
        end
    end
    if #w.queue > 0 then
        w.next_time = view.time + 30000
        next_wall(view, memory, w)
    else
        w.next_time = view.time + 300000
    end
end

return M
