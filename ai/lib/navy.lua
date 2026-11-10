-- The navy on the per-unit interface (docs/AI_LUA_UNITS.md §8 L6): transports and landings, warships.
-- require("lib.navy"). Plain Lua over the primitives; the state is the caller's memory table.
--
-- A landing (N.landing_step) is a state machine in mem.landing:
--   plan   -> a site: the shore of the target's land mass (ai.shore from the target building) on a sea the own
--             dock touches; the transports needed for the soldiers given
--   board  -> the transports sail to the water in front of the boarding shore (the shore nearest the soldiers on
--             the own land), the soldiers walk there, each transport loads its share (ai.load); all aboard, or 9 in
--             10 after a minute, or 3 minutes: it sails with what is aboard (half at least, else it is given up)
--   sail   -> the transports move together to the water in front of the site (a sea route round the danger)
--   unload -> each transport unloads at its own shore tile (ai.unload), ordered again every 2 s while anyone is
--             still aboard (the engine's unload gives up when the spot is taken)
--   ashore -> the soldiers are handed back to the caller as a list (lib.groups makes a group of them); the empty
--             transports wait at the meeting point
-- N.fleet_step: the warships guard the water by the dock, attack enemy ships and boats they see within reach, and
-- fall back to the dock when the seen enemy ships outweigh them by 1.5.
local M = {}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function alive_on_map(ids)
    local out = {}
    for _, id in ipairs(ids or {}) do
        local u = ai.unit(id)
        if u ~= nil then
            out[#out + 1] = id
        end
    end
    return out
end

-- the water tile beside a shore tile on the sea (one of the four sides)
local function water_by(x, y, sea)
    for _, d in ipairs({{1, 0}, {-1, 0}, {0, 1}, {0, -1}, {2, 0}, {-2, 0}, {0, 2}, {0, -2}}) do
        local t = ai.tile(x + d[1], y + d[2])
        if t ~= nil and t.sea ~= nil and t.sea ~= 0 and (sea == nil or t.sea == sea) then
            return x + d[1], y + d[2]
        end
    end
    return nil
end

-- the own transports (ids) and the capacity of their class
function M.transports()
    local ids = ai.units{category = "transport"}
    local out = {}
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and u.inside == nil then
            local c = ai.class(u.type)
            if c ~= nil and (c.transport or 0) > 0 then
                out[#out + 1] = {id = id, cap = c.transport, cargo = u.cargo or 0, x = u.x, y = u.y}
            end
        end
    end
    return out
end

-- the sea an own dock stands on (nil: none)
function M.dock_sea()
    local docks = ai.units{type = "b_dock", built = true}
    for _, id in ipairs(docks) do
        local u = ai.unit(id)
        if u ~= nil then
            for r = 0, 2 do
                local sx, sy, sea = ai.shore(u.x, u.y, {max = 3 + r})
                if sea ~= nil then
                    return sea, u
                end
            end
        end
    end
    return nil
end

-- the enemy strength (hit points x attack of the memory's armed units) within r of a tile
local function enemy_at(x, y, r)
    local str = 0
    for _, m in ipairs(ai.memory{near = {x, y, r}}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            str = str + m.hp * c.attack
        end
    end
    return str
end

-- the weakest beach: of nine shores of the target's land mass on the sea round the target building, the one with
-- the least known enemy strength within 12 tiles (then the nearest). Returns x, y, strength or nil
function M.beach(target, sea)
    local tt = ai.tile(target.x, target.y)
    local land = tt and tt.continent or 0
    local best, bs, bd = nil, nil, nil
    for _, off in ipairs({{0, 0}, {8, 0}, {-8, 0}, {0, 8}, {0, -8}, {8, 8}, {-8, -8}, {8, -8}, {-8, 8}, {16, 0}, {-16, 0}, {0, 16}, {0, -16}}) do
        local sx, sy = ai.shore(target.x + off[1], target.y + off[2], {sea = sea, land = land ~= 0 and land or nil, max = 20})
        if sx ~= nil then
            local str = enemy_at(sx, sy, 12)
            local dist = d2(sx, sy, target.x, target.y)
            if bs == nil or str < bs or (str == bs and dist < bd) then
                best, bs, bd = {x = sx, y = sy}, str, dist
            end
        end
    end
    if best == nil then
        return nil
    end
    return best.x, best.y, bs
end

-- a landing plan: target {x, y, player} (a known enemy building), the soldiers (ids) to carry, the transports;
-- opts.site_x/y the beach (M.beach) when the caller chose one. Returns the plan or nil, reason
function M.plan(target, soldiers, transports, opts)
    opts = opts or {}
    if #soldiers == 0 or #transports == 0 then
        return nil, "nothing"
    end
    local sea, dock = M.dock_sea()
    if sea == nil then
        return nil, "no_dock"
    end
    -- the site: the weakest beach of the target's land mass on this sea (M.beach), or the caller's
    local best = nil
    if opts.site_x ~= nil then
        best = {x = opts.site_x, y = opts.site_y}
    else
        local bx, by = M.beach(target, sea)
        if bx ~= nil then
            best = {x = bx, y = by}
        end
    end
    if best == nil then
        return nil, "no_shore"
    end
    local wx, wy = water_by(best.x, best.y, sea)
    if wx == nil then
        return nil, "no_water"
    end
    -- the boarding shore: the own land's shore on this sea nearest the soldiers' centre
    local sx, sy, n = 0, 0, 0
    for _, id in ipairs(soldiers) do
        local u = ai.unit(id)
        if u ~= nil then
            sx, sy, n = sx + u.x, sy + u.y, n + 1
        end
    end
    if n == 0 then
        return nil, "nothing"
    end
    local cx, cy = sx // n, sy // n
    local ct = ai.tile(cx, cy)
    local bx, by = ai.shore(cx, cy, {sea = sea, land = ct and ct.continent or nil, max = 40})
    if bx == nil then
        bx, by = ai.shore(dock.x, dock.y, {sea = sea, max = 6})
    end
    if bx == nil then
        return nil, "no_board"
    end
    local bwx, bwy = water_by(bx, by, sea)
    if bwx == nil then
        return nil, "no_board_water"
    end
    local cap = 0
    local ships = {}
    for _, t in ipairs(transports) do
        if cap < #soldiers then
            ships[#ships + 1] = t.id
            cap = cap + t.cap
        end
    end
    return {
        state = "board", target = target.player, tx = target.x, ty = target.y, sea = sea,
        site_x = best.x, site_y = best.y, water_x = wx, water_y = wy,
        board_x = bx, board_y = by, board_wx = bwx, board_wy = bwy,
        ships = ships, soldiers = soldiers, started = ai.time(), state_t = ai.time(), aboard = 0,
    }
end

-- one step of a landing; returns "landed" with the soldiers ashore, "aborted" with a reason, or nil while it goes on
function M.landing_step(L, opts)
    opts = opts or {}
    local now = ai.time()
    L.ships = alive_on_map(L.ships)
    L.soldiers = alive_on_map(L.soldiers)
    if #L.ships == 0 then
        return "aborted", "no_ships"
    end
    local cols = ai.get(L.ships, {"x", "y", "cargo"})
    local aboard = 0
    for k = 1, #L.ships do
        aboard = aboard + (cols.cargo[k] or 0)
    end
    L.aboard = aboard
    if L.state == "board" then
        if now - L.state_t == 0 or (now - (L.order_t or 0)) >= 30000 then
            L.order_t = now
            ai.move(L.ships, L.board_wx, L.board_wy, {forced = true})
            -- the soldiers not aboard: each transport takes its share, nearest first
            local walkers = {}
            for _, id in ipairs(L.soldiers) do
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil then
                    walkers[#walkers + 1] = id
                end
            end
            local k = 1
            local per = math.max(1, (#walkers + #L.ships - 1) // #L.ships)
            for i = 1, #walkers, per do
                local batch = {}
                for j = i, math.min(i + per - 1, #walkers) do
                    batch[#batch + 1] = walkers[j]
                end
                ai.load(batch, L.ships[k])
                k = k % #L.ships + 1
            end
        end
        local waited = now - L.state_t
        local all = aboard >= #L.soldiers
        local most = waited >= 60000 and aboard * 10 >= #L.soldiers * 9
        if all or most or waited >= 180000 then
            if aboard * 2 < #L.soldiers and not all then
                return "aborted", "too_few_aboard"
            end
            L.state = "sail"
            L.state_t = now
            L.order_t = 0
        end
        return nil
    end
    if L.state == "sail" then
        if (now - (L.order_t or 0)) >= 15000 then
            L.order_t = now
            local sx, sy = cols.x[1], cols.y[1]
            local path = ai.route(sx, sy, L.water_x, L.water_y, {avoid = 6, sea = true})
            if path ~= nil then
                for k, w in ipairs(path) do
                    ai.move(L.ships, w[1], w[2], {queue = k > 1, forced = true, formation = true})
                end
            else
                ai.move(L.ships, L.water_x, L.water_y, {forced = true, formation = true})
            end
        end
        local near = 0
        for k = 1, #L.ships do
            if cols.x[k] ~= nil and d2(cols.x[k], cols.y[k], L.water_x, L.water_y) <= 36 then
                near = near + 1
            end
        end
        if near == #L.ships or (near > 0 and now - L.state_t >= 180000) then
            L.state = "unload"
            L.state_t = now
            L.order_t = 0
        elseif now - L.state_t >= 360000 then
            return "aborted", "sail_timeout"
        end
        return nil
    end
    if L.state == "unload" then
        -- each transport its own point along the shore; a transport whose unload ended with cargo still aboard (the
        -- engine gives up when the spot is taken or unreachable) is ordered again at once at the next shore tile
        -- (upstream's engine fix: the order renewed within 0.5 s at the nearest shore tile with room)
        L.tries = L.tries or {}
        local acts = ai.get(L.ships, {"action"})
        local offsets = {0, 4, -4, 8, -8, 12, -12, 3, -3, 6, -6, 10, -10}
        for k = 1, #L.ships do
            if (cols.cargo[k] or 0) > 0 then
                local idle = acts.action[k] ~= "unload"
                local due = (now - (L.order_t or 0)) >= 2000
                if idle or (due and (now - (L.try_t or 0)) >= 6000) then
                    L.tries[k] = (L.tries[k] or 0) + (idle and 1 or 0)
                    local off = offsets[(L.tries[k] + k - 1) % #offsets + 1]
                    local ux, uy = ai.shore(L.site_x + off, L.site_y + (L.tries[k] % 3 - 1) * 3, {sea = L.sea, max = 8})
                    if ux == nil then
                        ux, uy = L.site_x, L.site_y
                    end
                    ai.unload(L.ships[k], ux, uy)
                    L.order_t = now
                    if idle then
                        L.try_t = now
                        L.retries = (L.retries or 0) + 1
                    end
                end
            end
        end
        if aboard == 0 then
            local ashore = {}
            for _, id in ipairs(L.soldiers) do
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil then
                    ashore[#ashore + 1] = id
                end
            end
            L.state = "ashore"
            L.state_t = now
            ai.move(L.ships, L.board_wx, L.board_wy, {forced = true})
            return "landed", ashore
        elseif now - L.state_t >= 240000 then
            return "aborted", "unload_timeout"
        end
        return nil
    end
    return "aborted", "bad_state"
end

-- the warships' step: hold the water by the dock (or `at`), attack the enemy ships and boats seen within `reach`,
-- fall back when the seen enemy warships are 1.5 x their strength. mem.fleet holds the state
function M.fleet_step(mem, opts)
    opts = opts or {}
    local ships = {}
    for _, id in ipairs(ai.units{category = "ship", armed = true}) do
        if id ~= opts.skip then
            ships[#ships + 1] = id
        end
    end
    if #ships == 0 then
        return 0
    end
    local sea, dock = M.dock_sea()
    local hx, hy = opts.x, opts.y
    if hx == nil and dock ~= nil then
        hx, hy = water_by(dock.x, dock.y, sea)
    end
    if hx == nil then
        local u = ai.unit(ships[1])
        hx, hy = u.x, u.y
    end
    ai.take(ships)
    local cx, cy, n = 0, 0, 0
    for _, id in ipairs(ships) do
        local u = ai.unit(id)
        if u ~= nil then
            cx, cy, n = cx + u.x, cy + u.y, n + 1
        end
    end
    cx, cy = cx // n, cy // n
    local own = ai.strength(ships)
    local reach = opts.reach or 20
    local foes = ai.seen{enemy = true, near = {cx, cy, reach}}
    local war, targets = {}, {}
    for _, id in ipairs(foes) do
        local u = ai.unit(id)
        if u ~= nil and (u.category == "ship" or u.category == "boat" or u.category == "transport") then
            targets[#targets + 1] = u
            if u.category == "ship" and u.armed then
                war[#war + 1] = id
            end
        end
    end
    local foe = ai.strength(war)
    local now = ai.time()
    if foe * 2 > own * 3 then
        if (now - (mem.fleet_flee_t or 0)) >= 10000 then
            mem.fleet_flee_t = now
            ai.move(ships, hx, hy, {forced = true})
        end
        return #ships
    end
    if #targets > 0 then
        table.sort(targets, function(a, b)
            local pa = a.category == "ship" and 0 or a.category == "transport" and 1 or 2
            local pb = b.category == "ship" and 0 or b.category == "transport" and 1 or 2
            if pa ~= pb then
                return pa < pb
            end
            return a.id < b.id
        end)
        if (mem.fleet_aim or 0) ~= targets[1].id or (now - (mem.fleet_t or 0)) >= 10000 then
            mem.fleet_aim = targets[1].id
            mem.fleet_t = now
            ai.attack(ships, targets[1].id)
        end
        return #ships
    end
    mem.fleet_aim = 0
    if d2(cx, cy, hx, hy) > 64 and (now - (mem.fleet_t or 0)) >= 15000 then
        mem.fleet_t = now
        ai.move(ships, hx, hy, {forced = false})
    end
    return #ships
end

return M
