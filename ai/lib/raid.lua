-- The Stone Age raid of the owner's build order (memory boom-build-order, 2026-11-11; docs/AI_RESEARCH.md §8.50), over
-- lib.groups. require("lib.raid"). Generic: any script may use it; the numbers are options.
--
--   R.step(view, mem, opts)   every strategy step. opts: guard (the home guard's group), ready (true once the
--                             economy is developed: boom-econ's memory.phase == "stone_developed")
--   R.wanted(mem, n)          the soldiers the script should want this step, given its own count n: the raid force on
--                             top while it builds, the home reserve beside the raid while it is out
--   R.state(mem)              "none", "build", "sent", "done"
--
-- The owner's order: in the Stone Age, once the economy is developed, build to ~`size` soldiers and send them in ONE
-- raid on the nearest neighbour (its citizens first, or its army); on the dispatch train another ~`reserve` as the
-- home guard; then the Copper Age is saved for. "If the AI receives a raid it cannot handle, it's pretty much already
-- lost": the raid stalls the neighbour. The raid itself is a lib.groups raid (the enemy's gatherers away from its
-- towers, soldiers only near, flight at 1.5 x) whose legs follow ai.route round the known threat.
-- The trace: `raid sent`, then `raid done` with the time out, the soldiers lost and the enemy units it hit that were
-- gone afterwards (an estimate: a unit it attacked that is no longer seen while the raid stands within 6 tiles of
-- where it was).
local G = require("lib.groups")

local M = {}

M.opts = {
    size = 20,          -- the raid force
    reserve = 20,       -- the home guard trained on the dispatch
    epoch = 1,          -- the raid's epoch (the Stone Age)
    build_ms = 600000,  -- the force goes after this long building even if short, with half of size at least
    out_ms = 420000,    -- a raid out this long comes home
    route_avoid = 8,
}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

function M.state(mem)
    return mem.raid ~= nil and mem.raid.state or "none"
end

function M.wanted(mem, n)
    local r = mem.raid
    if r == nil then
        return n
    end
    if r.state == "build" then
        return n + M.opts.size
    elseif r.state == "sent" then
        return math.max(n, M.opts.reserve) + (r.out or 0)
    end
    return n
end

-- the nearest enemy with a known town
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

-- the enemy units the raid attacks (its aims) that are gone while the raid stands near where they were
local function count_kills(r, g)
    r.hit = r.hit or {}
    for _, tid in pairs(g.aims or {}) do
        local u = ai.unit(tid)
        if u ~= nil then
            r.hit[tid] = {x = u.x, y = u.y, c = u.category == "citizen" and 1 or 0}
        end
    end
    for _, tid in ipairs(ai.keys(r.hit)) do
        local h = r.hit[tid]
        if ai.unit(tid) == nil then
            if g.cx ~= nil and d2(g.cx, g.cy, h.x, h.y) <= 36 then
                if h.c == 1 then
                    r.kill_cit = (r.kill_cit or 0) + 1
                else
                    r.kill_sol = (r.kill_sol or 0) + 1
                end
            end
            r.hit[tid] = nil
        end
    end
end

function M.step(view, mem, opts)
    opts = opts or {}
    local o = M.opts
    local now = view.time
    mem.raid = mem.raid or {state = "none"}
    local r = mem.raid
    local guard = opts.guard
    if r.state == "none" then
        if view.epoch == o.epoch and opts.ready then
            r.state, r.t = "build", now
            print(string.format("raid build at=%d", now // 1000))
        end
        return
    end
    if r.state == "build" then
        if view.epoch ~= o.epoch then
            r.state = "done"   -- (the epoch went by: no Stone raid)
            print("raid skipped epoch=" .. view.epoch)
            return
        end
        local have = guard ~= nil and #guard.units or 0
        local e = target(view)
        local due = now - r.t >= o.build_ms
        if e ~= nil and guard ~= nil and (have >= o.size + o.reserve // 4 or (due and have >= o.size // 2 + 4)) then
            -- the force: the units of the guard beyond a few left home (the reserve is trained after the dispatch)
            local n = math.min(o.size, have - 4)
            local ids = {}
            for k = 1, n do
                ids[k] = guard.units[#guard.units - n + k]
            end
            local g = G.create(mem, {task = "raid", role = "stone_raid", target = e.player, x = -1, y = -1,
                                     home_x = view.town_x, home_y = view.town_y})
            G.add(mem, g, ids)
            g.route, g.route_avoid = true, o.route_avoid
            r.state, r.sent, r.out, r.n = "sent", now, n, n
            print(string.format("raid sent units=%d enemy=%d town=%d,%d after=%d", n, e.player, e.town_x, e.town_y,
                (now - r.t) // 1000))
        end
        return
    end
    if r.state == "sent" then
        local g = G.role(mem, "stone_raid")
        local alive = g ~= nil and #g.units or 0
        r.out = alive
        if g ~= nil then
            count_kills(r, g)
        end
        local home = g ~= nil and g.task == "defend"   -- (lib.groups: a raid that fled home defends)
        if g == nil or home or now - r.sent >= o.out_ms then
            if g ~= nil then
                if guard ~= nil then
                    G.add(mem, guard, g.units)
                end
                G.disband(mem, g)
            end
            r.state = "done"
            print(string.format("raid done ms=%d sent=%d back=%d lost=%d killed_citizens=%d killed_soldiers=%d",
                now - r.sent, r.n, alive, r.n - alive, r.kill_cit or 0, r.kill_sol or 0))
        end
    end
end

return M
