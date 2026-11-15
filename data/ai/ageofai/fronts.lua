-- Age of AI port, FRONTS (upstream dll/ai/fronts.c; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age
-- of AI contributors). require("ageofai.fronts").
--
-- Where an AI's land meets land the enemy can walk over, and how much each stretch matters - the ground for standing
-- front armies (the player 2026-10-31: "1 or 2 front armies, they hold the front, push in, retreat into fortified
-- positions when losing; smaller defence forces in the backland" - and "it depends a lot on the map: in middle ocean
-- two armies for both ends of my reach, on flatland more to spread over my whole open border, with concentrations
-- only where I want to attack or fear a concentrated enemy push"). No count of armies is set: the border found here
-- decides - water, cliffs and the map's edge are no border, so the terrain makes the fronts.
-- Every 10 s per AI, the map in FR_CELL-tile cells (land: FR_CELL of its tiles walkable - a strip across it, so a
-- one-tile ford counts; the terrain doesn't change, so a cell is looked at once a game):
--   its territory - land cells within FR_TERR cells of its buildings, along land (not over water);
--   the enemy's - the same round the enemy buildings it knows (the memory, honest);
--   border - a cell of its territory beside a land cell that isn't, on the land mass of an enemy building it knows
--            (the enemy can walk there), or beside the enemy's territory;
--   fronts - border cells within 2 cells of each other, joined, a long one split in sections of FR_SECTION cells;
--            each one's width (cells), middle, threat (the enemy buildings and soldiers it knows within FR_REACH
--            tiles beyond, by count: a soldier twice a building), value (its own buildings within FR_REACH tiles
--            behind) and weight = width x (1 + threat) x (1 + value) / 4, its share of the weights its share of
--            the army to be.
-- THE STATIONS (fa_stations, every look): the heaviest front, and a second one weighing at least FA_SECOND of it;
-- each army's station its own tower nearest the front's middle (within FR_REACH; none: its building nearest),
-- FA_BEHIND tiles back towards its base - the fortified position the army holds and falls back to. Station 0 is the
-- force's home (armies.c ar_home: army.lua's station takes it when it is out of danger).
-- THE FRONT ARMY ([ai_base] front_army, 0 upstream: measured no gain head-to-head, "where the soldiers wait between
-- attacks matters little while the game sends nearly all of them on attacks at once"): the idle land soldiers that
-- are on no attack and stand more than FA_NEAR tiles from every station walk to the station whose army is the
-- furthest under its share, in groups of FA_GROUP or more, all to one station, none while the station lies where the
-- AI's fight is lost (the lost layer within FA_SAFE tiles), a unit sent at most once in FA_AGAIN.
-- DEVIATIONS (the engine): the walking regions (game/reachmap.c, gates open to the player) are the land masses
-- (ai.tile's continent, ai.reach); a cell's land is sampled on 16 of its 64 tiles (every other one each way, 2 land
-- of 16 for upstream's 8 of 64) and only explored tiles tell (fog: the terrain is not known before it is seen), so
-- the cells fill in as the map is explored, FR_SAMPLE a look; the port's land soldiers are one force (lib.groups): with
-- front_army on, those sent to a second station are walked back by the force's own defend task - a port gap, the
-- setting stays off as upstream.
local S = require("ageofai.settings")
local M = {}

local FR_CELL = 8
local FR_SIDE = 128          -- a 1024-tile map; a bigger one folds its far cells onto the edge
local FR_TERR = 3            -- territory: 3 cells (24 tiles) round its buildings
local FR_REACH = 48
local FR_MAX = 32
local FR_SECTION = 12        -- a front at most ~12 cells (96 tiles) of border; a longer border is split
local FR_SAMPLE = 96         -- DEVIATION: cells looked at per look while unknown (the charged budget)
local FR_EVERY = 10000
local FA_SECOND = 50         -- percent
local FA_BEHIND = 4
local FA_NEAR = 10
local FA_AGAIN = 60000
local FA_ARMIES = 2
local FA_SAFE = 6
local FA_GROUP = 5
local TOWNS = {b_town_center = true, b_capitol = true}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- the land id at a tile or next to it (a building's tile has none)
local function land_at(x, y)
    for _, d in ipairs({{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, -1}, {2, 0}, {0, 2}, {-2, 0}, {0, -2}}) do
        local t = ai.tile(x + d[1], y + d[2])
        if t ~= nil and (t.continent or 0) ~= 0 then
            return t.continent
        end
    end
    return 0
end

-- the cells' land, filled in as the map is explored (once a game per cell)
local function cells(view, fr)
    if fr.side == nil then
        local map = ai.map()
        local size = math.max(map.width or 0, map.height or 0)
        fr.size = size
        fr.side = math.min((size + FR_CELL - 1) // FR_CELL, FR_SIDE)
        fr.land, fr.area, fr.known = {}, {}, {}
        for i = 1, fr.side * fr.side do
            fr.land[i], fr.area[i], fr.known[i] = 0, 0, 0
        end
        fr.cursor, fr.unknown = 1, fr.side * fr.side
    end
    local side, n = fr.side, fr.side * fr.side
    local looked, i = 0, fr.cursor
    while fr.unknown > 0 and looked < FR_SAMPLE do
        if fr.known[i] == 0 then
            looked = looked + 1
            local x0, y0 = (i - 1) % side * FR_CELL, (i - 1) // side * FR_CELL
            local land, explored, area = 0, 0, 0
            for dy = 1, 7, 2 do
                for dx = 1, 7, 2 do
                    local t = ai.tile(x0 + dx, y0 + dy)
                    if t == nil then
                        explored = explored + 1   -- off the map (a folded cell)
                    elseif t.explored then
                        explored = explored + 1
                        if (t.continent or 0) ~= 0 then
                            land = land + 1
                            if area == 0 then
                                area = t.continent
                            end
                        end
                    end
                end
            end
            if land >= 2 then
                fr.land[i], fr.area[i], fr.known[i] = 1, area, 1
                fr.unknown = fr.unknown - 1
            elseif explored >= 16 then
                fr.known[i] = 1
                fr.unknown = fr.unknown - 1
            end
        end
        i = i % n + 1
        if i == fr.cursor then
            break
        end
    end
    fr.cursor = i
end

local function cell_of(fr, x, y)
    local side = fr.side
    local cx, cy = x // FR_CELL, y // FR_CELL
    if cx >= side then
        cx = side - 1
    end
    if cy >= side then
        cy = side - 1
    end
    if cx < 0 then
        cx = 0
    end
    if cy < 0 then
        cy = 0
    end
    return cy * side + cx + 1
end

-- territory: distance in cells from the seeds marked 1 in m, along land, up to FR_TERR (m: 0 none, else 1 + distance)
local function spread(fr, m)
    local side, n = fr.side, fr.side * fr.side
    local q, h = {}, 1
    for i = 1, n do
        if m[i] == 1 then
            q[#q + 1] = i
        end
    end
    while h <= #q do
        local i = q[h]
        h = h + 1
        local d = m[i]
        if d <= FR_TERR then
            local x, y = (i - 1) % side, (i - 1) // side
            for dy = -1, 1 do
                for dx = -1, 1 do
                    local nx, ny = x + dx, y + dy
                    if nx >= 0 and ny >= 0 and nx < side and ny < side then
                        local j = ny * side + nx + 1
                        if m[j] == 0 and fr.land[j] == 1 and (fr.area[i] == 0 or fr.area[j] == fr.area[i]) then
                            m[j] = d + 1
                            q[#q + 1] = j
                        end
                    end
                end
            end
        end
    end
end

local function look(view, memory, fr)
    cells(view, fr)
    local side, n = fr.side, fr.side * fr.side
    local own, foe = {}, {}
    for i = 1, n do
        own[i], foe[i] = 0, 0
    end
    -- its buildings
    local ids = ai.units{category = "building"}
    local blds = {}
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y", "type"})
        for k = 1, #ids do
            if cols.x[k] ~= nil and cols.x[k] >= 0 then
                blds[#blds + 1] = {x = cols.x[k], y = cols.y[k], type = cols.type[k]}
                own[cell_of(fr, cols.x[k], cols.y[k])] = 1   -- a cell of buildings may have no walkable tiles left: a seed all the same
            end
        end
    end
    -- the enemy buildings it knows, and their land masses
    local enemy = {}
    for _, p in ipairs(ai.players()) do
        if p.enemy and not p.me then
            enemy[p.id] = true
        end
    end
    fr.foe_area = fr.foe_area or {}
    local foe_areas, nfa = {}, {}
    local eb = {}
    for _, r in ipairs(ai.memory{buildings = true}) do
        if enemy[r.player] and r.x >= 0 and r.y >= 0 then
            eb[#eb + 1] = r
            local i = cell_of(fr, r.x, r.y)
            if fr.land[i] == 1 then
                foe[i] = 1
            end
            local a = fr.foe_area[i]
            if a == nil then
                a = land_at(r.x, r.y)
                fr.foe_area[i] = a
            end
            if a ~= 0 and not foe_areas[a] and #nfa < 64 then
                foe_areas[a] = true
                nfa[#nfa + 1] = a
            end
        end
    end
    spread(fr, own)
    spread(fr, foe)
    -- border cells: lab 0 not, -1 border (to be joined)
    local lab = {}
    local border, terr = 0, 0
    for i = 1, n do
        lab[i] = 0
        if own[i] > 0 then
            terr = terr + 1
            local x, y = (i - 1) % side, (i - 1) // side
            local b = false
            for dy = -1, 1 do
                for dx = -1, 1 do
                    if not b then
                        local nx, ny = x + dx, y + dy
                        if nx >= 0 and ny >= 0 and nx < side and ny < side then
                            local j = ny * side + nx + 1
                            if fr.land[j] == 1 then
                                if foe[j] > 0 then
                                    b = true
                                elseif own[j] == 0 and fr.area[j] ~= 0 and foe_areas[fr.area[j]] then
                                    b = true
                                end
                            end
                        end
                    end
                end
            end
            if b then
                lab[i] = -1
                border = border + 1
            end
        end
    end
    -- fronts: border cells within 2 cells of each other
    local fronts = {}
    for i = 1, n do
        if lab[i] == -1 and #fronts < FR_MAX then
            local l = #fronts + 1
            lab[i] = l
            local q, h = {i}, 1
            while h <= #q do
                local c = q[h]
                h = h + 1
                local x, y = (c - 1) % side, (c - 1) // side
                for dy = -2, 2 do
                    for dx = -2, 2 do
                        local nx, ny = x + dx, y + dy
                        if nx >= 0 and ny >= 0 and nx < side and ny < side then
                            local j = ny * side + nx + 1
                            if lab[j] == -1 then
                                lab[j] = l
                                q[#q + 1] = j
                            end
                        end
                    end
                end
            end
            -- a long border in sections of about FR_SECTION cells: seeds spread along it (the cell farthest from the
            -- seeds so far), each cell to its nearest seed, the seeds moved to their sections' middles once
            local cnt = #q
            local k = math.min((cnt + FR_SECTION - 1) // FR_SECTION, FR_MAX - #fronts)
            if k > 0 then
                local pts = {}
                for _, c in ipairs(q) do
                    pts[#pts + 1] = {(c - 1) % side, (c - 1) // side}
                end
                local seed = {pts[1]}
                for j = 2, k do
                    local most, bi = -1, 1
                    for qi, p in ipairs(pts) do
                        local d = nil
                        for _, s in ipairs(seed) do
                            local e = d2(p[1], p[2], s[1], s[2])
                            if d == nil or e < d then
                                d = e
                            end
                        end
                        if d > most then
                            most, bi = d, qi
                        end
                    end
                    seed[j] = pts[bi]
                end
                local sec_n, sec_x, sec_y = {}, {}, {}
                for _ = 1, 2 do
                    for m = 1, k do
                        sec_n[m], sec_x[m], sec_y[m] = 0, 0, 0
                    end
                    for _, p in ipairs(pts) do
                        local bm, bd = 1, nil
                        for m = 1, k do
                            local e = d2(p[1], p[2], seed[m][1], seed[m][2])
                            if bd == nil or e < bd then
                                bd, bm = e, m
                            end
                        end
                        sec_n[bm] = sec_n[bm] + 1
                        sec_x[bm] = sec_x[bm] + p[1]
                        sec_y[bm] = sec_y[bm] + p[2]
                    end
                    for m = 1, k do
                        if sec_n[m] > 0 then
                            seed[m] = {sec_x[m] // sec_n[m], sec_y[m] // sec_n[m]}
                        end
                    end
                end
                for m = 1, k do
                    if sec_n[m] > 0 then
                        fronts[#fronts + 1] = {width = sec_n[m], x = sec_x[m] * FR_CELL // sec_n[m] + FR_CELL // 2,
                                               y = sec_y[m] * FR_CELL // sec_n[m] + FR_CELL // 2, threat = 0, value = 0}
                    end
                end
            end
        end
    end
    -- threat beyond and value behind each front
    if #fronts > 0 then
        local r2 = FR_REACH * FR_REACH
        for _, r in ipairs(eb) do
            for _, f in ipairs(fronts) do
                if d2(f.x, f.y, r.x, r.y) <= r2 then
                    f.threat = f.threat + 1
                end
            end
        end
        for _, r in ipairs(ai.memory{since = math.max(view.time - 600000, 0)}) do
            if not r.building and enemy[r.player] then
                local c = ai.class(r.type)
                if c ~= nil and (c.attack or 0) > 0 then
                    for _, f in ipairs(fronts) do
                        if d2(f.x, f.y, r.x, r.y) <= r2 then
                            f.threat = f.threat + 2   -- a soldier counts twice a building
                        end
                    end
                end
            end
        end
        for _, b in ipairs(blds) do
            for _, f in ipairs(fronts) do
                if d2(f.x, f.y, b.x, b.y) <= r2 then
                    f.value = f.value + 1
                end
            end
        end
    end
    local total = 0
    for _, f in ipairs(fronts) do
        f.weight = f.width * (1 + f.threat) * (1 + f.value) * 100 // 4   -- x 100
        total = total + f.weight
    end
    for _, f in ipairs(fronts) do
        f.share = total > 0 and f.weight * 100 // total or 0   -- percent
    end
    fr.fronts, fr.border, fr.terr = fronts, border, terr
    fr.nland, fr.nown = 0, 0
    for i = 1, n do
        fr.nland = fr.nland + fr.land[i]
        if own[i] == 1 then
            fr.nown = fr.nown + 1
        end
    end
    return blds
end

-- the armies' stations (fa_stations): every look
local function stations(view, fr, blds)
    fr.stations = {}
    if #fr.fronts == 0 or view.town_x < 0 then
        return
    end
    local pick = {}
    for _ = 1, FA_ARMIES do
        local best = nil
        for j, f in ipairs(fr.fronts) do
            local used = false
            for _, p in ipairs(pick) do
                used = used or p == j
            end
            if not used and (best == nil or f.weight > fr.fronts[best].weight) then
                best = j
            end
        end
        if best == nil or (#pick > 0 and fr.fronts[best].weight * 100 < FA_SECOND * fr.fronts[pick[1]].weight) then
            break
        end
        pick[#pick + 1] = best
    end
    local armed = {}
    for _, id in ipairs(ai.units{category = "building", armed = true, built = true}) do
        armed[id] = true
    end
    local towers = {}
    for _, id in ipairs(ai.keys(armed)) do
        local u = ai.unit(id)
        if u ~= nil and not TOWNS[u.type] then
            towers[#towers + 1] = u
        end
    end
    local r2 = FR_REACH * FR_REACH
    for _, j in ipairs(pick) do   -- each army's station: its tower nearest the front (else its building nearest), a bit back
        local f = fr.fronts[j]
        local tower, td, bld, bd = nil, r2 + 1, nil, r2 + 1
        for _, u in ipairs(towers) do
            local d = d2(u.x, u.y, f.x, f.y)
            if d < td then
                tower, td = u, d
            end
        end
        for _, b in ipairs(blds) do
            local d = d2(b.x, b.y, f.x, f.y)
            if d < bd then
                bld, bd = b, d
            end
        end
        local anchor = tower or bld
        if anchor ~= nil then
            local ex, ey = view.town_x - anchor.x, view.town_y - anchor.y
            local el = math.floor(math.sqrt(ex * ex + ey * ey))
            local ax, ay = anchor.x, anchor.y
            if el >= 1 then
                ax, ay = anchor.x + FA_BEHIND * ex // el, anchor.y + FA_BEHIND * ey // el
            end
            local same = nil
            for q, st in ipairs(fr.stations) do
                if same == nil and d2(st.x, st.y, ax, ay) <= 4 * FA_NEAR * FA_NEAR then
                    same = q
                end
            end
            if same ~= nil then
                fr.stations[same].weight = fr.stations[same].weight + f.weight   -- one front for two sections that close
            else
                fr.stations[#fr.stations + 1] = {x = ax, y = ay, weight = f.weight, share = 0, have = 0}
            end
        end
    end
    local wsum = 0
    for _, st in ipairs(fr.stations) do
        wsum = wsum + st.weight
    end
    for _, st in ipairs(fr.stations) do
        st.share = wsum > 0 and st.weight * 100 // wsum or 100 // #fr.stations
    end
end

-- station k (1-based; upstream's 0) of this AI -> x, y or nil
function M.station(memory, k)
    local fr = memory.fronts
    local st = fr ~= nil and fr.stations ~= nil and fr.stations[k] or nil
    if st == nil then
        return nil
    end
    return st.x, st.y
end

-- THE FRONT ARMY (front_army): the idle land soldiers to the stations under their share
local function front_army(view, memory, fr)
    local fa = fr.fa
    if #fr.stations == 0 or (memory.army or {}).out then   -- the force out: its members are the attack's
        return
    end
    fa.sent_at = fa.sent_at or {}
    local ids = ai.units{category = "soldier", idle = true}
    if #ids == 0 then
        return
    end
    local cols = ai.get(ids, {"x", "y"})
    local free, total = {}, 0
    for _, st in ipairs(fr.stations) do
        st.have = 0
    end
    for k = 1, #ids do
        if cols.x[k] ~= nil then
            fa.idle = (fa.idle or 0) + 1
            total = total + 1
            local at = nil
            for q, st in ipairs(fr.stations) do
                if at == nil and d2(cols.x[k], cols.y[k], st.x, st.y) <= FA_NEAR * FA_NEAR then
                    at = q
                end
            end
            if at ~= nil then
                fr.stations[at].have = fr.stations[at].have + 1
            else
                free[#free + 1] = ids[k]
            end
        end
    end
    local go = {}   -- the free ones not sent lately: they go together, or not at all
    for _, id in ipairs(free) do
        local t = fa.sent_at[id]
        if t == nil or view.time < t or view.time - t >= FA_AGAIN then
            go[#go + 1] = id
        end
    end
    local k, most = 1, nil   -- the station furthest under its share
    for q, st in ipairs(fr.stations) do
        local under = st.share * total - st.have * 100
        if most == nil or under > most then
            most, k = under, q
        end
    end
    local st = fr.stations[k]
    local lost = ai.danger_box(st.x - FA_SAFE, st.y - FA_SAFE, st.x + FA_SAFE, st.y + FA_SAFE, "lost")
    local safe = (lost or 0) == 0
    if not safe then
        fa.unsafe = (fa.unsafe or 0) + 1
    end
    if #go < FA_GROUP or not safe then
        return
    end
    for _, id in ipairs(go) do
        local jx, jy = (id * 7) % 5 - 2, (id * 13) % 5 - 2   -- spread over a few tiles
        if ai.move({id}, st.x + jx, st.y + jy, {forced = false}) == "ok" then
            fa.sent_at[id] = view.time > 0 and view.time or 1
            st.have = st.have + 1
            fa.sent = (fa.sent or 0) + 1
        end
    end
    for _, id in ipairs(ai.keys(fa.sent_at)) do   -- the old marks
        if view.time - fa.sent_at[id] >= 2 * FA_AGAIN then
            fa.sent_at[id] = nil
        end
    end
end

-- every step: the look every FR_EVERY (fronts_tick)
function M.step(view, memory)
    memory.fronts = memory.fronts or {}
    local fr = memory.fronts
    fr.fa = fr.fa or {}
    if fr.t ~= nil and view.time >= fr.t and view.time - fr.t < FR_EVERY then
        return
    end
    fr.t = view.time
    fr.fronts = fr.fronts or {}
    fr.stations = fr.stations or {}
    local blds = look(view, memory, fr)
    stations(view, fr, blds)
    if S.front_army then
        front_army(view, memory, fr)
    end
    if S.fronts_log and (fr.log_t == nil or view.time < fr.log_t or view.time - fr.log_t >= 300000) then
        fr.log_t = view.time > 0 and view.time or 1
        local parts = {}
        for _, f in ipairs(fr.fronts) do
            parts[#parts + 1] = string.format("%d,%d w%d t%d v%d s%d%%", f.x, f.y, f.width, f.threat, f.value, f.share)
        end
        local st = {}
        for _, s in ipairs(fr.stations) do
            st[#st + 1] = string.format("%d,%d %d%%", s.x, s.y, s.share)
        end
        print(string.format("fronts map=%d side=%d land=%d unknown=%d own=%d terr=%d border=%d n=%d fronts=[%s] stations=[%s] sent=%d idle=%d unsafe=%d",
            fr.size or 0, fr.side or 0, fr.nland or 0, fr.unknown or 0, fr.nown or 0, fr.terr or 0, fr.border or 0, #fr.fronts,
            table.concat(parts, " | "), table.concat(st, " | "), fr.fa.sent or 0, fr.fa.idle or 0, fr.fa.unsafe or 0))
    end
end

return M
