-- Drop sites that touch their resource (docs/AI_RESEARCH.md §8.51; the owner: every settlement and granary directly
-- touches its patch, mine or forest, so the gatherers never walk). The placement is policy, so it lives here, on the
-- per-unit interface: the resource units' tiles and footprints (ai.resources, ai.unit, ai.class), the game's own
-- legality (ai.can_place: no AI margin), and the build at that exact tile (ai.build); the C++ build verb's near=site
-- placement stays as it is for the scripts still on it.
--
--   M.spot(type, rids, hx, hy)   the anchor tile of the touching spot nearest home (gap 0), else gap 1; nil: none
--   M.request(view, mem, key, type, rids, x, y)
--                                one job per key: builders sent to the spot (a citizen walks first where the site is
--                                not explored); "building", "walking", "busy", "none"
--   M.step(view, mem)            the jobs: the builders released once the drop site stands (or after JOB_MS)
--   M.pending(mem)               the jobs under way (the settlements going up)
local M = {}

local JOB_MS = 180000       -- a job's builders are released after this, built or not
local WALK_MS = 60000       -- a walker stands at an unexplored site this long at most
local BAD_MS = 300000       -- a key whose spot failed waits this long
local TESTS = 24            -- ai.can_place calls per spot at most (300 instructions each)
local BUILDERS = 3          -- (C3 against the C++ verb: 4 settlements at minute 8 against 6, 6 at 12 against 7)
local NEAR = 20             -- builders from this near first (a wood gatherer 40 tiles off walked while the patch waited)

-- the building of a class standing (or going up) with its anchor exactly at x, y (nil: none; the build's own id is a
-- pending unit's until a builder reaches the tile, then the building's)
local function at_tile(type, x, y)
    for _, id in ipairs(ai.units{type = type, near = {x, y, 4}, limit = 8}) do
        local u = ai.unit(id)
        if u ~= nil and u.x == x and u.y == y then
            return id, u
        end
    end
    return nil
end

local sizes = {}            -- class -> footprint size (static data)
local function size_of(type)
    if sizes[type] == nil then
        local c = ai.class(type)
        sizes[type] = c ~= nil and c.size ~= nil and c.size > 0 and c.size or 1
    end
    return sizes[type]
end

-- the tiles between a footprint (x, y, s) and a unit's (ux, uy, us); 0 touching (or overlapping)
local function gap(x, y, s, ux, uy, us)
    local dx = math.max(ux - (x + s), x - (ux + us), 0)
    local dy = math.max(uy - (y + s), y - (uy + us), 0)
    return math.max(dx, dy)
end

local function overlaps(x, y, s, ux, uy, us)
    return x < ux + us and x + s > ux and y < uy + us and y + s > uy
end

function M.spot(type, rids, hx, hy, bad)
    local s = size_of(type)
    local units = {}
    for _, id in ipairs(rids) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            units[#units + 1] = {x = u.x, y = u.y, s = size_of(u.type)}
        end
    end
    if #units == 0 then
        return nil
    end
    for want = 0, 1 do
        local seen, cands = {}, {}
        for _, u in ipairs(units) do
            for x = u.x - s - want, u.x + u.s + want do
                for y = u.y - s - want, u.y + u.s + want do
                    local k = x * 100000 + y
                    if not seen[k] and x >= 0 and y >= 0 then
                        seen[k] = true
                        local best, clear = 1000, true
                        for _, v in ipairs(units) do
                            if overlaps(x, y, s, v.x, v.y, v.s) then
                                clear = false
                            end
                            best = math.min(best, gap(x, y, s, v.x, v.y, v.s))
                        end
                        if clear and best == want then
                            local dx, dy = x - hx, y - hy
                            cands[#cands + 1] = {x = x, y = y, d = dx * dx + dy * dy}
                        end
                    end
                end
            end
        end
        table.sort(cands, function(a, b)
            if a.d ~= b.d then
                return a.d < b.d
            end
            if a.x ~= b.x then
                return a.x < b.x
            end
            return a.y < b.y
        end)
        for i, c in ipairs(cands) do
            if i > TESTS then
                break
            end
            if not (bad ~= nil and bad[c.x .. "," .. c.y]) and ai.can_place(type, c.x, c.y) then
                return c.x, c.y, want
            end
        end
    end
    return nil
end

-- the builders for a spot: citizens near it the engine holds, the wood gatherers first, then the idle and the others
-- but food, gold and iron (the owner: never the food gatherers; a mine's gatherer pulled to build costs the mine its
-- gathering), the nearest first in each; food, gold or iron only when no other is near
local function builders(x, y, reach)
    local rank = {wood = 1, ["nil"] = 2, stone = 3, gold = 5, iron = 5, food = 6}
    local cands = {}
    for i, id in ipairs(ai.units{category = "citizen", held = "engine", near = {x, y, reach or NEAR}, limit = 16}) do
        local u = ai.unit(id)
        if u ~= nil and u.action ~= "build" and u.inside == nil then
            cands[#cands + 1] = {id = id, r = rank[tostring(u.gathers)] or 4, i = i}
        end
    end
    table.sort(cands, function(a, b)
        if a.r ~= b.r then
            return a.r < b.r
        end
        return a.i < b.i
    end)
    local picked = {}
    for _, c in ipairs(cands) do
        if #picked < BUILDERS and (c.r < 5 or #picked == 0) then
            picked[#picked + 1] = c.id
        end
    end
    if #picked == 0 and (reach or NEAR) < 40 then
        return builders(x, y, 40)
    end
    return picked
end

-- the player's drop sites a mine's or patch's gatherers can use
local DROPS = {"b_settlement", "b_town_center", "b_capitol"}

-- whether a drop site of the player's touches one of the resource units (gap 0): a home mine served by the town centre
-- 7 tiles off is not, and gets its own settlement
function M.touched(rids)
    return M.touched_units(rids) > 0
end

-- how many of the resource units a drop site of the player's touches (gap 0)
function M.touched_units(rids)
    local units = {}
    local cx, cy = 0, 0
    for _, id in ipairs(rids) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil then
            units[#units + 1] = {x = u.x, y = u.y, s = size_of(u.type)}
            cx, cy = cx + u.x, cy + u.y
        end
    end
    if #units == 0 then
        return 0
    end
    cx, cy = cx // #units, cy // #units
    local hit = {}
    for _, type in ipairs(DROPS) do
        local s = size_of(type)
        for _, id in ipairs(ai.units{type = type, near = {cx, cy, 10}, limit = 8}) do
            local b = ai.unit(id)
            if b ~= nil and b.x ~= nil then
                for i, u in ipairs(units) do
                    if gap(b.x, b.y, s, u.x, u.y, u.s) == 0 then
                        hit[i] = true
                    end
                end
            end
        end
    end
    local n = 0
    for i = 1, #units do
        if hit[i] then
            n = n + 1
        end
    end
    return n
end

function M.pending(mem)
    local n = 0
    for _, k in ipairs(ai.keys((mem.place or {}).jobs or {})) do
        n = n + 1
    end
    return n
end

-- a citizen walks to a site nobody has seen yet (its resource units are not listed until seen, so no spot can be
-- found there): the walker job, as request's for an unexplored spot
function M.walk(view, mem, key, x, y)
    mem.place = mem.place or {jobs = {}, bad = {}}
    local P = mem.place
    if P.jobs[key] ~= nil or (P.bad[key] or -1) > view.time then
        return "busy"
    end
    local w = builders(x, y, 40)
    if #w == 0 then
        return "none"
    end
    ai.take({w[1]})
    ai.move({w[1]}, x, y)
    P.jobs[key] = {walk = true, t = view.time, builders = {w[1]}}
    print(string.format("place walk key=%s to=%d,%d", key, x, y))
    return "walking"
end

function M.request(view, mem, key, type, rids, x, y)
    mem.place = mem.place or {jobs = {}, bad = {}}
    local P = mem.place
    if P.jobs[key] ~= nil then
        return "busy"
    end
    if (P.bad[key] or -1) > view.time then
        return "none"
    end
    local t = ai.tile(x, y)
    P.tiles = P.tiles or {}
    local bx, by, g = M.spot(type, rids, view.town_x, view.town_y, P.tiles)
    if bx == nil then
        if t ~= nil and not t.explored then
            -- (the game places a building only on tiles in sight: a citizen walks there first)
            local w = builders(x, y)
            if #w > 0 then
                ai.take({w[1]})
                ai.move({w[1]}, x, y)
                P.jobs[key] = {walk = true, t = view.time, builders = {w[1]}}
                return "walking"
            end
        end
        P.bad[key] = view.time + BAD_MS
        return "none"
    end
    local b = builders(bx, by)
    if #b == 0 then
        return "none"
    end
    local code, reason, _, id = ai.build(b, type, bx, by)
    if code ~= "ok" or id == nil then
        ai.release(b)
        P.bad[key] = view.time + BAD_MS
        print(string.format("place refused key=%s type=%s at=%d,%d gap=%d reason=%s", key, type, bx, by, g,
            tostring(reason)))
        return "none"
    end
    P.jobs[key] = {id = id, t = view.time, builders = b, type = type, x = bx, y = by}
    print(string.format("place build key=%s type=%s at=%d,%d gap=%d builders=%d", key, type, bx, by, g, #b))
    return "building"
end

function M.step(view, mem)
    local P = mem.place
    if P == nil then
        return
    end
    P.tiles = P.tiles or {}
    for _, t in ipairs({P.bad, P.tiles}) do
        for _, k in ipairs(ai.keys(t)) do
            if t[k] <= view.time then
                t[k] = nil
            end
        end
    end
    for _, key in ipairs(ai.keys(P.jobs)) do
        local j = P.jobs[key]
        local done = false
        if j.walk then
            done = view.time - j.t >= WALK_MS
            if not done then
                local u = ai.unit(j.builders[1])
                done = u == nil or (u.action ~= "move" and view.time - j.t >= 5000)
            end
        else
            -- (the foundation by its tile: the build's id is swapped for the building's once it is placed)
            local _, b = at_tile(j.type, j.x, j.y)
            if b ~= nil then
                done = b.built == true
            else
                -- (not placed yet: the foundation is a pending unit until a builder reaches the tile; it is dropped
                -- if the builders stop. While one still walks or builds it is waited for; once none does, the tile
                -- is passed over for a while and the next touching one comes)
                local busy = false
                for _, id in ipairs(j.builders or {}) do
                    local u = ai.unit(id)
                    if u ~= nil and (u.action == "move" or u.action == "build") then
                        busy = true
                    end
                end
                if not busy and view.time - j.t >= 15000 then
                    done = true
                    P.tiles[j.x .. "," .. j.y] = view.time + BAD_MS
                end
            end
            done = done or view.time - j.t >= JOB_MS
        end
        if done then
            local alive = {}
            for _, id in ipairs(j.builders or {}) do
                if ai.unit(id) ~= nil then
                    alive[#alive + 1] = id
                end
            end
            if #alive > 0 then
                ai.release(alive)
            end
            local id = j.x ~= nil and at_tile(j.type, j.x, j.y) or nil
            if id ~= nil then
                ai.release({id})   -- (the drop site is the engine's, as any other building)
            end
            P.jobs[key] = nil
        end
    end
end

return M
