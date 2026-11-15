-- Age of AI port, THE ARMY AS ONE (upstream dll/ai/mass.c, docs/economy.md "The army as one"; UPSTREAM has the map
-- and the licence: GPL-3.0, Grille and the Age of AI contributors). require("ageofai.mass"), hooked from army.lua's
-- reinforcement path (the soldiers trained while the force is out) and its push.
--
-- The playtester: against the base game's nonstop waves a proper AI gathers its soldiers under cover (towers, Town
-- Center, production), lets the waves break on the towers and the massed army, strikes only when the mass is strong
-- enough against the enemy force it knows, and never feeds single soldiers into a fight - reinforcements gather
-- first and join as groups.
--
-- army_mass ([ai_base], bits; land or mixed maps only - navy_land: an enemy town known on the AI's own continent):
-- 1 GATHER: a reinforcement for the force out (a soldier trained meanwhile, or a member standing at home within 25
--   tiles while the force's pack is 30+ tiles from the base) is held at the rally point: by the own finished tower
--   within 14 tiles of the base nearest the point 6 tiles out toward the force, 2 tiles on the base's side of it
--   (else that point; chosen again every 20 s). The held go together once army_mass_group (5) wait, 3 for
--   army_mass_hold (120 s), or any after twice that; a released one is not held again for 3 minutes.
--   Upstream refuses the subgroup's orders to a held member (vf57); here the held are the "wait" group's, which the
--   force's orders never reach, so nothing is refused.
-- 2 WAIT: the attack time comes while the AI's land soldiers (hit points x attack) are below army_mass_ratio (130%)
--   of the strongest enemy's army it knows (the memory's soldiers seen within 10 minutes): the launch moves on by
--   20 s, for army_mass_wait (10) minutes at most.
-- 4 (was RETREAT, removed upstream; the bit does nothing).
-- 8 (with 1): GATHER only while the force fights; otherwise the reinforcements join at once (upstream's trickle).
-- 16 TETHER: a member of the force out picks no target more than army_mass_tether (12) tiles from the pack (the
--   middle of its members away from the base) unless it is within its own range + 2 (lib.groups' scorer); 128 (with
--   16): only buildings - enemy units are fought wherever they are. 32 REGROUP: a member out, tether + 4 tiles from
--   the pack while the force fights, attacking nothing, walks back to it. 64 JOIN (with 1): the released
--   reinforcements walk to the pack (else the force's next leg takes them).
-- Upstream's default: 0 (measured: GATHER lost fewer and killed more head-to-head and against the base AI without the
-- cheat, but killed fewer against the cheating one; docs/economy.md). Not ported: the "# mass:" / "# mass2:" autotest
-- measurement (the samples of soldiers alone, away, the stragglers, the deaths by place); a summary a minute instead.
local G = require("lib.groups")
local S = require("ageofai.settings")
local M = {}

local HOME_R = 25          -- at home: within this of the base
local OUT_R = 30           -- the pack out: its middle this far from the base
local GONE_MS = 180000     -- a released reinforcement is not held again for this long
local RALLY_MS = 20000
local TETHER_MS = 2000

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

local function state(mem)
    mem.mass = mem.mass or {gone = {}, held = 0, released = 0, waits = 0, waited = 0, goes = 0, tethered = 0,
                            regrouped = 0, joined = 0, rally_x = -1, rally_y = -1}
    return mem.mass
end

-- the plan's bits, 0 off: only with an enemy town known on the AI's own continent (navy_land; looked at once a minute)
function M.plan(view, mem)
    local plan = S.army_mass or 0
    if plan == 0 or view.town_x < 0 then
        return 0
    end
    local ms = state(mem)
    if ms.land_t == nil or view.time - ms.land_t >= 60000 then
        ms.land_t = view.time
        local home = ai.tile(view.town_x, view.town_y)
        local land = home ~= nil and home.continent or 0
        ms.land = false
        for _, e in ipairs(view.enemies or {}) do
            if e.town_x ~= nil and e.town_x >= 0 then
                local t = ai.tile(e.town_x, e.town_y)
                if t ~= nil and land ~= 0 and t.continent == land then
                    ms.land = true
                end
            end
        end
    end
    return ms.land and plan or 0
end

-- the pack: the middle of the force's members away from the base (nil: fewer than 2), and whether the force is out
-- (past forming, the pack OUT_R from the base; or none away any more while it marches or fights)
local function pack(view, force)
    local tx, ty = view.town_x, view.town_y
    local ax, ay, na = 0, 0, 0
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and d2(u.x, u.y, tx, ty) > HOME_R * HOME_R then
            ax, ay, na = ax + u.x, ay + u.y, na + 1
        end
    end
    if na == 0 then
        return nil, nil, 0, force.state == "moving" or force.state == "fighting"
    end
    ax, ay = ax // na, ay // na
    return ax, ay, na, force.state ~= "forming" and d2(ax, ay, tx, ty) > OUT_R * OUT_R
end

-- GATHER: the rally (every 20 s): the own finished tower within 14 tiles of the base nearest the point 6 tiles out
-- toward (fx, fy), 2 tiles on the base's side of it; else that point
local function rally(view, ms, fx, fy)
    local bx, by = view.town_x, view.town_y
    local dx, dy = fx - bx, fy - by
    local l = isqrt(dx * dx + dy * dy)
    if l < 1 then
        dx, dy, l = 1, 0, 1
    end
    local px, py = bx + 6 * dx // l, by + 6 * dy // l
    ms.rally_x, ms.rally_y = px, py
    local bd = nil
    for _, id in ipairs(ai.units{category = "building", armed = true, built = true, near = {bx, by, 14}}) do
        local u = ai.unit(id)
        if u ~= nil and u.type ~= "b_town_center" and u.type ~= "b_capitol" then
            local d = d2(u.x, u.y, px, py)
            if bd == nil or d < bd then
                bd = d
                local ex, ey = bx - u.x, by - u.y
                local el = isqrt(ex * ex + ey * ey)
                ms.rally_x = u.x + (el >= 1 and 2 * ex // el or 0)
                ms.rally_y = u.y + (el >= 1 and 2 * ey // el or 0)
            end
        end
    end
end

-- GATHER, every step while the force is out. `wait` is the reinforcements' group (created here when needed), `free`
-- the soldiers trained since the last step. Returns the wait group (nil: none).
function M.gather(view, memory, mem, force, wait, free)
    local ms = state(mem)
    local plan = M.plan(view, mem)
    local now = view.time
    local px, py, na, out = pack(view, force)
    local fighting = force.state == "fighting"
    local hold = (plan & 1) ~= 0 and out and ((plan & 8) == 0 or fighting)
    -- the released: forgotten after GONE_MS
    for _, id in ipairs(ai.keys(ms.gone)) do
        if now - ms.gone[id] >= GONE_MS then
            ms.gone[id] = nil
        end
    end
    if not hold then
        -- the port's own flow (no plan, or 8 and the force not fighting): the trained go with the force at once
        if plan ~= 0 and (plan & 1) ~= 0 then
            if #free > 0 then
                G.add(memory, force, free)
            end
            if wait ~= nil then
                G.add(memory, force, wait.units)
                G.disband(memory, wait)
            end
            return nil
        end
        if wait == nil and #free == 0 then
            return nil
        end
        if wait == nil then
            wait = G.create(memory, {task = "defend", role = "wait", x = mem.home_x, y = mem.home_y, radius = S.ar_home_r})
        end
        G.add(memory, wait, free)
        return wait
    end
    -- the members standing at home while the pack is out are held too (a replacement the force gave up on)
    local tx, ty = view.town_x, view.town_y
    local held = {}
    for _, id in ipairs(free) do
        if ms.gone[id] == nil then
            held[#held + 1] = id
        end
    end
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and d2(u.x, u.y, tx, ty) <= HOME_R * HOME_R and ms.gone[id] == nil then
            held[#held + 1] = id
        end
    end
    if #held > 0 or wait ~= nil then
        if wait == nil then
            wait = G.create(memory, {task = "defend", role = "wait", x = mem.home_x, y = mem.home_y, radius = 3})
            wait.hold_t = now
        end
        if #held > 0 then
            G.add(memory, wait, held)
            ms.held = ms.held + #held
            for _, id in ipairs(held) do
                wait.since = wait.since or {}
                if wait.since[id] == nil then
                    wait.since[id] = now
                end
            end
        end
    end
    -- the trained that were released lately go straight on
    local rest = {}
    for _, id in ipairs(free) do
        if ms.gone[id] ~= nil then
            rest[#rest + 1] = id
        end
    end
    if #rest > 0 then
        G.add(memory, force, rest)
    end
    if wait == nil or #wait.units == 0 then
        return wait
    end
    -- the rally, by the tower toward the pack
    if ms.rally_t == nil or now - ms.rally_t >= RALLY_MS then
        ms.rally_t = now
        rally(view, ms, px or force.x, py or force.y)
    end
    if wait.x ~= ms.rally_x or wait.y ~= ms.rally_y then
        wait.x, wait.y = ms.rally_x, ms.rally_y
        wait.way_t = 0
    end
    -- released together: army_mass_group wait, 3 for army_mass_hold, any after twice that
    wait.since = wait.since or {}
    local n, oldest = #wait.units, 0
    for _, id in ipairs(wait.units) do
        local t = wait.since[id] or now
        if now - t > oldest then
            oldest = now - t
        end
    end
    if n >= S.army_mass_group or (n >= 3 and oldest >= S.army_mass_hold * 1000) or oldest >= S.army_mass_hold * 2000 then
        local ids = {}
        for _, id in ipairs(wait.units) do
            ids[#ids + 1] = id
            ms.gone[id] = now
        end
        print(string.format("mass go n=%d held_min=%d join=%s rally=%d,%d", n, oldest // 60000,
            tostring((plan & 64) ~= 0 and na >= 2), ms.rally_x, ms.rally_y))
        G.add(memory, force, ids)
        if (plan & 64) ~= 0 and na >= 2 then   -- JOIN: to the pack
            ai.move(ids, px, py, {forced = false})
            ms.joined = ms.joined + #ids
        end
        ms.released = ms.released + n
        G.disband(memory, wait)
        return nil
    end
    return wait
end

-- the strongest enemy's army known: the memory's soldiers seen within 10 minutes, hit points x attack, by player
local function strongest(view)
    local foe = {}
    local enemies = {}
    for _, p in ipairs(ai.players()) do
        if p.enemy then
            enemies[p.id] = true
        end
    end
    for _, m in ipairs(ai.memory{since = view.time - 600000}) do
        if not m.building and enemies[m.player] then
            local c = ai.class(m.type)
            if c ~= nil and (c.attack or 0) > 0 then
                foe[m.player] = (foe[m.player] or 0) + m.hp * c.attack
            end
        end
    end
    local top = 0
    for _, p in ipairs(ai.keys(foe)) do
        if foe[p] > top then
            top = foe[p]
        end
    end
    return top
end

-- WAIT: at the attack time; true when the push waits (the attack time moves on by 20 s). `own` is the land army's
-- hit points x attack
function M.wait(view, mem, own)
    local ms = state(mem)
    if (M.plan(view, mem) & 2) == 0 then
        ms.wait0 = nil
        return false
    end
    local now = view.time
    local top = strongest(view)
    if ms.wait0 == nil then
        ms.wait0 = now
    end
    if top <= 0 or own * 100 >= top * S.army_mass_ratio or now - ms.wait0 >= S.army_mass_wait * 60000 then
        if now - ms.wait0 >= 20000 then
            ms.goes = ms.goes + 1
            print(string.format("mass goes waited_min=%d own=%d foe=%d", (now - ms.wait0) // 60000, own, top))
        end
        ms.wait0 = nil
        return false
    end
    if now - ms.wait0 < 1000 then
        ms.waits = ms.waits + 1
        print(string.format("mass wait own=%d foe=%d ratio=%d", own, top, S.army_mass_ratio))
    end
    ms.waited = ms.waited + 20
    return true
end

-- TETHER / REGROUP, every 2 s while the force is out: the pack's middle onto the force (lib.groups' scorer reads
-- force.tether_x/y, force.tether, force.tether_buildings); a member far from the pack and idle walks back
function M.tether(view, mem, force)
    local ms = state(mem)
    local plan = M.plan(view, mem)
    if force == nil then
        return
    end
    if (plan & 48) == 0 or not mem.out then
        force.tether = nil
        return
    end
    local now = view.time
    if ms.tether_t ~= nil and now - ms.tether_t < TETHER_MS then
        return
    end
    ms.tether_t = now
    local px, py, na, out = pack(view, force)
    if not out or na < 2 then
        force.tether = nil
        return
    end
    force.tether = (plan & 16) ~= 0 and S.army_mass_tether or nil
    force.tether_x, force.tether_y = px, py
    force.tether_buildings = (plan & 128) ~= 0
    if (plan & 32) == 0 or force.state ~= "fighting" then
        return
    end
    local t = S.army_mass_tether + 4
    local tx, ty = view.town_x, view.town_y
    local back = {}
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil and u.action ~= "attack" and d2(u.x, u.y, tx, ty) > HOME_R * HOME_R
                and d2(u.x, u.y, px, py) >= t * t and not (u.tx ~= nil and d2(u.tx, u.ty, px, py) <= 16) then
            back[#back + 1] = id
        end
    end
    if #back > 0 then
        ai.move(back, px, py, {forced = false})
        ms.regrouped = ms.regrouped + #back
    end
end

-- a summary a minute (the counters since the last line)
function M.report(view, memory, mem, force)
    local ms = mem.mass
    if ms == nil or M.plan(view, mem) == 0 or view.time % 60000 >= 1000 then
        return
    end
    local wait = G.role(memory, "wait")
    print(string.format("mass held=%d released=%d holding=%d rally=%d,%d waits=%d waited=%d goes=%d tethered=%d regrouped=%d joined=%d",
        ms.held, ms.released, wait ~= nil and #wait.units or 0, ms.rally_x, ms.rally_y, ms.waits, ms.waited, ms.goes,
        (force and force.tethered or 0), ms.regrouped, ms.joined))
    ms.held, ms.released, ms.waits, ms.waited, ms.goes, ms.regrouped, ms.joined = 0, 0, 0, 0, 0, 0, 0
    if force ~= nil then
        force.tethered = 0
    end
end

return M
