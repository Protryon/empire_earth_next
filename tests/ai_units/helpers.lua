-- TEST ONLY: the helpers (docs/AI_LUA_UNITS.md §4): ai.mark / ai.danger, ai.route around a marked danger, ai.find_site
-- and ai.build with the script's own citizens, the `built` event. `helpers.*` lines go to the trace.
local P = {}

local function walk_danger(path, sx, sy)
    -- the highest danger met on the straight legs between the waypoints (every tile)
    local worst, x, y = 0, sx, sy
    for _, w in ipairs(path) do
        local dx, dy = w[1] - x, w[2] - y
        local n = math.max(math.abs(dx), math.abs(dy))
        for k = 1, n do
            local d = ai.danger(x + dx * k // n, y + dy * k // n)
            worst = d > worst and d or worst
        end
        x, y = w[1], w[2]
    end
    return worst
end

function P.step(view, mem)
    if view.town_x < 0 then
        return {}
    end
    if mem.step == nil and view.time >= 20000 then
        -- a danger 12 tiles out, between the town and a point 24 tiles out on the same land
        local tx, ty = view.town_x, view.town_y
        local home = ai.tile(tx, ty).continent
        for _, d in ipairs({{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}, {1, -1}, {-1, 1}}) do
            local gx, gy = tx + d[1] * 24, ty + d[2] * 24
            local g = ai.tile(gx, gy)
            if g ~= nil and (g.continent == nil or g.continent == home) then
                local mx, my = tx + d[1] * 12, ty + d[2] * 12
                ai.mark(mx, my, 4, 50, 600000)
                local straight, c0 = ai.route(tx, ty, gx, gy, {avoid = 0})
                local around, c1 = ai.route(tx, ty, gx, gy, {avoid = 8})
                if straight ~= nil and around ~= nil then
                    print(string.format("helpers.route mark=%d,%d danger=%d straight=%d legs danger %d cost %d; around=%d legs danger %d cost %d",
                        mx, my, ai.danger(mx, my), #straight, walk_danger(straight, tx, ty), c0, #around,
                        walk_danger(around, tx, ty), c1))
                    mem.routed = true
                    break
                end
            end
        end
        if not mem.routed then
            local tx, ty = view.town_x, view.town_y
            local p1 = ai.route(tx, ty, tx + 3, ty, {avoid = 0})
            print(string.format("helpers.route none found: home=%s reach=%s short=%s map=%dx%d", tostring(ai.tile(tx, ty).continent),
                tostring(ai.reach(tx, ty, tx + 3, ty)), tostring(p1 and #p1), ai.map().width, ai.map().height))
        end
        mem.step = 1
    end
    if mem.step == 1 and view.time >= 25000 then
        local cits = ai.units{category = "citizen", limit = 3}
        local x, y = ai.find_site("b_barracks", view.town_x, view.town_y, {min = 4, max = 14, safe = true})
        if x ~= nil and #cits > 0 then
            local code, reason, n, id = ai.build(cits, "b_barracks", x, y)
            print(string.format("helpers.build site=%d,%d code=%s reason=%s n=%d id=%s", x, y, code, reason, n, tostring(id)))
            mem.building = id
        else
            print("helpers.build no site or no citizens")
        end
        mem.step = 2
    end
    if mem.step == 2 then
        for _, e in ipairs(ai.events()) do
            if e.kind == "built" and e.unit == mem.building then
                print(string.format("helpers.built t=%d id=%d type=%s", view.time, e.unit, e.type))
                mem.step = 3
            end
        end
    end
    if mem.step == 3 then
        -- the later helpers (docs/AI_LUA_UNITS.md §11): path_len, drop_site, shore, unexplored, the layers, config
        local tx, ty = view.town_x, view.town_y
        local len = ai.path_len(tx, ty, tx + 20, ty)
        local id, dist, dx, dy = ai.drop_site(tx + 10, ty + 10, "food")
        local sx, sy, sea, land = ai.shore(tx, ty, {max = 30})
        local ux, uy = ai.unexplored(tx, ty, {away = {tx, ty}})
        local lost = ai.danger(tx, ty, "lost")
        local infl = ai.danger(tx, ty, "influence")
        local imax, isum = ai.danger_box(tx - 8, ty - 8, tx + 8, ty + 8, "influence")
        local ms = ai.config{memory_ms = 120000}
        print(string.format("helpers.more path_len=%s drop=%s dist=%s at=%s,%s shore=%s,%s sea=%s land=%s unexplored=%s,%s lost=%d influence=%d box=%d/%d memory_ms=%d",
            tostring(len), tostring(id), tostring(dist), tostring(dx), tostring(dy), tostring(sx), tostring(sy), tostring(sea),
            tostring(land), tostring(ux), tostring(uy), lost, infl, imax, isum, ms))
        mem.step = 4
    end
    return {}
end

return P
