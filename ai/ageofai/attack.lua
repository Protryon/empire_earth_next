-- Age of AI port, the attack (upstream dll/ai/attack.c: the weak spots, the breaches, the siege's target and firing
-- position, the commander's patches that are decisions; forces.c enemy_objective; UPSTREAM has the map and the
-- licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.attack"). Called from army.lua's push
-- (the force's target and entry point) and from tactics.lua's siege (the shelling).
--
-- THE KNOWN DEFENCES (threat_refresh): the enemy buildings of the memory with a ground attack (not the AA guns),
-- range over 1 and up to 30 tiles. threat_at: the defences covering a tile within their range + avoid_towers_margin
-- (upstream's route penalty is the engine's ai.route over its threat layer; here the threat decides only the weak
-- spots, the breaches and the objective).
-- WEAK SPOT (attack_weak_spots): the push's goal inside known defences and an enemy building within 4 tiles of it:
-- the known enemy building (no wall) within attack_weak_spot_radius of the goal that no known defence covers, on the
-- force's land, the nearest the force and not too far from the goal (dist to the force + half the dist to the goal
-- least) - the economy behind the towers, the open flank.
-- BREACH (attack_breach): the straight line from the force to its goal crosses a known enemy wall: the segment of
-- that wall (8-connected, up to 60 tiles from the crossing) outside known defences with the shortest detour force ->
-- segment -> goal is the goal; the force attacks it and its neighbours first (lib.groups' focus).
-- THE SIEGE'S TARGET (siege_tick): the force's breach wall first, else the nearest known defence the siege outranges
-- (its range + 0.5 under the siege's least range); the firing position at the siege's range - 0.7 from it on the
-- side facing the siege, turned by 15-degree steps up to 90 either way until no other known defence reaches it
-- (range + 1.5). No trigonometry: the fixed tables, as upstream (lockstep).
-- OBJECTIVE (forces.c enemy_objective): the target's known building (no wall) on the land of a point, the nearest by
-- distance + 3 x the threat at it (a guard tower counts 1). wave_retarget (attack_retarget): the target player out
-- of buildings known: the living enemy whose known building is nearest the force's middle.
-- Not applicable: the game's attack subgroup hooks (the route cost, the subgroup's target record, the scorer): the
-- port's force is lib.groups over the per-unit interface; the leash, the aircraft out of reach and the rams held
-- back are lib.groups' marked blocks (g.leash), the breach segment first its focus.
local S = require("ageofai.settings")
local M = {}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- a wall piece or a gate (upstream's is_wall role)
function M.is_wall(t)
    local p = string.sub(t or "", 1, 7)
    return p == "b_wall_" or p == "b_gate_"
end

-- an anti-air building (defence_stats: the 88 mm gun and the missile tower shoot at aircraft only)
local function is_aa(t)
    return string.sub(t or "", 1, 2) == "aa"
end

-- ---------------------------------------------------------------------------------------------------------------
-- The known defences

-- the enemy memory's armed buildings with a ground attack (threat_refresh); near = {x, y, r} narrows the scan.
-- Each: id, x, y, r (range in centi-tiles, with the owner's upgrades as ai.class gives them), a (attack), type, player
function M.defences(near)
    local f = {buildings = true}
    if near ~= nil then
        f.near = near
    end
    local out = {}
    for _, m in ipairs(ai.memory(f)) do
        if not M.is_wall(m.type) and not is_aa(m.type) then
            local c = ai.class(m.type)
            local r = c ~= nil and (c.range or 0) or 0
            if c ~= nil and (c.attack or 0) > 0 and r > 100 and r <= 3000 then
                out[#out + 1] = {id = m.id, x = m.x, y = m.y, r = r, a = c.attack, type = m.type, player = m.player}
            end
        end
    end
    return out
end

-- the summed attack of the known defences covering a tile (their range + avoid_towers_margin); upstream counts a
-- guard tower as 1 (attack / 50): divide where it is weighed, compare > 0 where it decides
function M.threat_at(defs, x, y)
    local t = 0
    for _, d in ipairs(defs) do
        local r = d.r // 100 + S.avoid_towers_margin
        if d2(x, y, d.x, d.y) <= r * r then
            t = t + d.a
        end
    end
    return t
end

-- a known defence other than the one at (ex, ey) reaches (x, y) by its real range + 1.5 (covered)
function M.covered(defs, x, y, ex, ey)
    for _, d in ipairs(defs) do
        if d.x ~= ex or d.y ~= ey then
            local r = d.r / 100 + 1.5
            local dx, dy = x - d.x, y - d.y
            if dx * dx + dy * dy <= r * r then
                return true
            end
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- The weak spot

function M.weak_spot(defs, sx, sy, dx, dy)
    if not S.attack_weak_spots or M.threat_at(defs, dx, dy) == 0 then
        return nil
    end
    local list = ai.memory{buildings = true, near = {dx, dy, S.attack_weak_spot_radius}}
    -- only a goal at an enemy building is retargeted (building_at: within 4 tiles, no wall)
    local at = false
    for _, m in ipairs(list) do
        if not M.is_wall(m.type) and d2(m.x, m.y, dx, dy) <= 16 then
            at = true
            break
        end
    end
    if not at then
        return nil
    end
    local best, best_score = nil, nil
    for _, m in ipairs(list) do
        if not M.is_wall(m.type) and M.threat_at(defs, m.x, m.y) == 0 and ai.reach(sx, sy, m.x, m.y) then
            local score = math.sqrt(d2(m.x, m.y, sx, sy)) + 0.5 * math.sqrt(d2(m.x, m.y, dx, dy))
            if best_score == nil or score < best_score then
                best, best_score = m, score
            end
        end
    end
    return best
end

-- ---------------------------------------------------------------------------------------------------------------
-- The breach

-- the known enemy wall pieces by tile (key x * 10000 + y)
function M.walls()
    local out, n = {}, 0
    for _, m in ipairs(ai.memory{buildings = true}) do
        if M.is_wall(m.type) then
            out[m.x * 10000 + m.y] = m
            n = n + 1
        end
    end
    return out, n
end

local function wall_at(walls, x, y)
    if x < 0 or y < 0 then
        return nil
    end
    return walls[x * 10000 + y]
end

-- the first known wall tile on the straight line start -> dest (half-tile steps)
function M.wall_crossing(walls, sx, sy, dx, dy)
    local ddx, ddy = dx - sx, dy - sy
    local n = 2 * math.max(math.abs(ddx), math.abs(ddy))
    for i = 1, n - 1 do
        local x = sx + (ddx * i + (ddx >= 0 and n // 2 or -(n // 2))) // n
        local y = sy + (ddy * i + (ddy >= 0 and n // 2 or -(n // 2))) // n
        local w = wall_at(walls, x, y)
        if w ~= nil then
            return w
        end
    end
    return nil
end

-- follows the crossed wall (8-connected) up to 60 tiles from the crossing and takes the segment outside known
-- defences with the shortest detour start -> segment -> dest
function M.find_breach(defs, walls, sx, sy, dx, dy, cross)
    local MAXQ = 512
    local qx, qy, seen = {cross.x}, {cross.y}, {}
    seen[cross.x * 10000 + cross.y] = true
    local head, best, best_score = 1, nil, nil
    while head <= #qx do
        local x, y = qx[head], qy[head]
        head = head + 1
        local w = wall_at(walls, x, y)
        if w ~= nil then
            if M.threat_at(defs, x, y) == 0 then
                local score = math.sqrt(d2(x, y, sx, sy)) + math.sqrt(d2(x, y, dx, dy))
                if best_score == nil or score < best_score then
                    best, best_score = w, score
                end
            end
            for ex = -1, 1 do
                for ey = -1, 1 do
                    local nx, ny = x + ex, y + ey
                    local key = nx * 10000 + ny
                    if d2(nx, ny, cross.x, cross.y) <= 60 * 60 and #qx < MAXQ and not seen[key] and wall_at(walls, nx, ny) ~= nil then
                        seen[key] = true
                        qx[#qx + 1], qy[#qy + 1] = nx, ny
                    end
                end
            end
        end
    end
    return best
end

-- the wall segment to breach on the way start -> dest, or nil (and the crossing, for the trace)
function M.breach(defs, sx, sy, dx, dy)
    if not S.attack_breach then
        return nil
    end
    local walls, n = M.walls()
    if n == 0 then
        return nil
    end
    local cross = M.wall_crossing(walls, sx, sy, dx, dy)
    if cross == nil then
        return nil
    end
    return M.find_breach(defs, walls, sx, sy, dx, dy, cross), cross
end

-- ---------------------------------------------------------------------------------------------------------------
-- The entry point of a push (hook_asg_route's decisions, in its order: the weak spot, then the breach)

-- target: a memory record (the building the push goes for). Returns {x, y, id, player, weak, breach, focus_x,
-- focus_y}: where the force goes, what it attacks first (the breach segment: its focus), and notes the breach for the
-- siege in mem.at (breach_note)
function M.entry(mem, sx, sy, target)
    mem.at = mem.at or {}
    local at = mem.at
    local defs = M.defences()
    local e = {x = target.x, y = target.y, id = target.id, player = target.player, weak = false, breach = false,
               focus_x = -1, focus_y = -1}
    local t = M.threat_at(defs, e.x, e.y)
    local w = M.weak_spot(defs, sx, sy, e.x, e.y)
    if w ~= nil then
        at.weak_spots = (at.weak_spots or 0) + 1
        print(string.format("attack weak_spot goal=%d,%d threat=%d -> %s of p%d at %d,%d from=%d,%d", e.x, e.y, t // 50,
            w.type, w.player, w.x, w.y, sx, sy))
        e.x, e.y, e.id, e.player, e.weak = w.x, w.y, w.id, w.player, true
    end
    local b, cross = M.breach(defs, sx, sy, e.x, e.y)
    at.breach_id, at.breach_x, at.breach_y = 0, -1, -1
    if b ~= nil then
        at.breaches = (at.breaches or 0) + 1
        print(string.format("attack breach goal=%d,%d crossed=%d,%d threat=%d -> wall %s at %d,%d", e.x, e.y, cross.x, cross.y,
            M.threat_at(defs, cross.x, cross.y) // 50, b.type, b.x, b.y))
        e.x, e.y, e.breach = b.x, b.y, true
        e.focus_x, e.focus_y = b.x, b.y
        at.breach_id, at.breach_x, at.breach_y = b.id, b.x, b.y
    elseif cross ~= nil then
        at.breach_none = (at.breach_none or 0) + 1
    end
    return e
end

-- the breach is over (the force home, or its target changed): breach_note(-1)
function M.breach_forget(mem)
    if mem.at ~= nil then
        mem.at.breach_id, mem.at.breach_x, mem.at.breach_y = 0, -1, -1
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The objective and the re-targeting

-- forces.c enemy_objective: the target player's known building (no wall) on the land of (x, y), the nearest by
-- distance + 3 x the threat at it (a guard tower counts 1)
function M.objective(defs, player, x, y, r)
    local best, best_score = nil, nil
    for _, m in ipairs(ai.memory{buildings = true, player = player, near = {x, y, r or 200}}) do
        if not M.is_wall(m.type) then
            local score = math.sqrt(d2(m.x, m.y, x, y)) + 3 * M.threat_at(defs, m.x, m.y) / 50
            if (best_score == nil or score < best_score) and ai.reach(x, y, m.x, m.y) then
                best, best_score = m, score
            end
        end
    end
    return best
end

-- the force's next target once its building is down: the target's objective nearest the force's middle; the
-- target out of known buildings (or out of the game): with attack_retarget the living enemy whose known building
-- (no wall) is nearest the middle (wave_retarget)
function M.next_target(mem, player, cx, cy)
    local defs = M.defences()
    local alive = {}
    for _, p in ipairs(ai.players()) do
        alive[p.id] = p.alive and p.enemy
    end
    local b = nil
    if player ~= nil and player > 0 and alive[player] then
        b = M.objective(defs, player, cx, cy, 120)
    end
    if b == nil and S.attack_retarget then
        for _, m in ipairs(ai.memory{buildings = true, near = {cx, cy, 120}}) do
            if not M.is_wall(m.type) and alive[m.player] and ai.reach(cx, cy, m.x, m.y) then
                b = m
                break
            end
        end
        if b ~= nil and b.player ~= player then
            mem.at = mem.at or {}
            mem.at.retargets = (mem.at.retargets or 0) + 1
            print(string.format("attack retarget p%d -> p%d %s at %d,%d middle=%d,%d", player or 0, b.player, b.type, b.x, b.y, cx, cy))
        end
    end
    return b
end

-- ---------------------------------------------------------------------------------------------------------------
-- The siege's target and firing position (siege_tick)

local COS15 = {1.0, 0.96592583, 0.86602540, 0.70710678, 0.5, 0.25881905, 0.0}
local SIN15 = {0.0, 0.25881905, 0.5, 0.70710678, 0.86602540, 0.96592583, 1.0}

-- siege: the own siege units' records (x, y, range in centi-tiles). Returns the target (a memory record: id, x, y,
-- type), the firing position (tiles) and the siege's least range (centi), or nil
function M.shelling(mem, siege)
    if #siege == 0 then
        return nil
    end
    local sx, sy, srange = 0, 0, 9900
    for _, u in ipairs(siege) do
        sx, sy = sx + u.x, sy + u.y
        if (u.range or 0) > 0 and u.range < srange then
            srange = u.range
        end
    end
    sx, sy = sx / #siege, sy / #siege
    local at = mem.at or {}
    local defs = M.defences()
    -- the target: the force's breach wall (still known there), else the nearest known defence the siege outranges
    local t = nil
    if (at.breach_id or 0) ~= 0 then
        for _, m in ipairs(ai.memory{buildings = true, near = {at.breach_x, at.breach_y, 0}}) do
            if m.id == at.breach_id then
                t = m
            end
        end
    end
    if t == nil then
        local best = nil
        for _, d in ipairs(defs) do
            if d.r + 50 < srange then
                local dd = d2(d.x, d.y, sx, sy)
                if best == nil or dd < best then
                    best, t = dd, d
                end
            end
        end
    end
    if t == nil then
        return nil
    end
    -- the firing position: range - 0.7 from the target facing the siege, turned until no other defence reaches it
    local ux, uy = sx - t.x, sy - t.y
    local ul = math.sqrt(ux * ux + uy * uy)
    if ul > 0 then
        ux, uy = ux / ul, uy / ul
    else
        ux, uy = 1, 0
    end
    local r = srange / 100 - 0.7
    for k = 0, 12 do
        local st = (k + 1) // 2 + 1   -- 0, +-15, +-30 ... +-90 degrees
        local c = COS15[st]
        local s = (k % 2 == 1) and SIN15[st] or -SIN15[st]
        local px = t.x + 0.5 + r * (ux * c - uy * s)
        local py = t.y + 0.5 + r * (ux * s + uy * c)
        if not M.covered(defs, px, py, t.x, t.y) then
            return t, math.floor(px), math.floor(py), srange
        end
    end
    return nil
end

return M
