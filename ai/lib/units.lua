-- Helpers over the per-unit interface (docs/AI_LUA_UNITS.md §11: ai.units, ai.unit, ai.move, ai.route ...).
-- require("lib.units"). Plain Lua on top of the primitives: groups are arrays of unit ids in the script's memory.
local M = {}

-- the behaviour table's rows (ai.stance takes the number; dbunitbehavior.csv)
M.STANCE = {scout = 0, stand_ground = 1, guard = 2, harass = 3, cautious = 4, aggressive = 5}

-- the squared distance of two records in centi-tiles
function M.d2(a, b)
    local dx, dy = a.px - b.px, a.py - b.py
    return dx * dx + dy * dy
end

-- the ids of a list that still answer ai.unit (alive, and seen when not the player's), in their order
function M.alive(ids)
    local out = {}
    for _, id in ipairs(ids or {}) do
        if ai.unit(id) ~= nil then
            out[#out + 1] = id
        end
    end
    return out
end

-- the mean tile of the units of a list that are on the map -> x, y (nil when none is)
function M.centre(ids)
    local sx, sy, n = 0, 0, 0
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil then
            sx, sy, n = sx + u.x, sy + u.y, n + 1
        end
    end
    if n == 0 then
        return nil
    end
    return sx // n, sy // n
end

-- the record of a list of ids nearest to a record (nil when none answers), and its squared distance
function M.nearest(u, ids)
    local best, bd
    for _, id in ipairs(ids) do
        local e = ai.unit(id)
        if e ~= nil then
            local d = M.d2(u, e)
            if bd == nil or d < bd or (d == bd and e.id < best.id) then
                best, bd = e, d
            end
        end
    end
    return best, bd
end

-- moves a group to (x, y) along ai.route from its centre: the first leg replaces their orders, the others are queued.
-- opts: avoid (the route's danger weight, default 4), forced, formation. -> the waypoints given (0: no route; then a
-- plain move)
function M.move_route(ids, x, y, opts)
    opts = opts or {}
    local cx, cy = M.centre(ids)
    if cx == nil then
        return 0
    end
    local path = ai.route(cx, cy, x, y, {avoid = opts.avoid or 4})
    if path == nil then
        ai.move(ids, x, y, {forced = opts.forced, formation = opts.formation})
        return 0
    end
    for k, w in ipairs(path) do
        ai.move(ids, w[1], w[2], {queue = k > 1, forced = opts.forced, formation = opts.formation})
    end
    return #path
end

return M
