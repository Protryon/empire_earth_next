-- Age of AI port, the fleet (upstream dll/ai/fleet.c, docs/navy.md stages 3-5; UPSTREAM has the map and the licence:
-- GPL-3.0, Grille and the Age of AI contributors). require("ageofai.fleet"). ageofai.navy runs it (F.step).
--
-- The warships led as one (navy_fleet=big, navy_fleet_mass=1):
-- - every own warship and submarine afloat, not a landing's escort and not the sea scout, every 2 s;
-- - danger: the known enemy defences (their range + 2) and the siege and long-range units remembered 5 minutes;
--   no ship sits, sails or attacks inside - a ship inside sails out, a target inside is dropped, a way that crosses a
--   zone goes round (ai.route over the sea, the engine's threat layer) or not at all;
-- - strength: hit points x damage a second (attack x 1000 / (reload + 2000)) x the hit factor by role (docs/navy.md
--   1: frigates hit everything, battleships galleys x0.25 and submarines not at all, galleys battleships x3.8 and
--   catapult ships x4.5, submarines battleships x3.5, cruisers no ship), the ratio sqrt(own hp x own dps / (enemy hp
--   x enemy dps)); this engine has no hit table call, so the roles come from the class names and the factors from
--   upstream's table;
-- - MASSING (navy_fleet_mass): while the fleet is weaker than the enemy fleet known (ratio < 1) or under its
--   critical mass (a quarter of the target, 4 at least; both with a margin of 3/4) every ship waits at the rally -
--   the water tile inside the reach of the most own towers (range - 1), else by the dock farthest from the enemy -
--   and fights there whatever comes into the cover (ratio 0.5 with towers, 0.8 without), never chasing out of it
--   (the leash: the cover's radius + 3). It strikes as one at ratio 1.25 with the critical mass. No tower covers the
--   sea: one is asked for by that dock (every 2 minutes, 3 a game);
-- - ONE FLEET (stage 3): a landing under way - the ships it did not take stay off its beach; enemy ships within 15
--   tiles of the guard point, the buildings or the fishing boats attacked at ratio 0.8; a landing getting its
--   transports - its sea route cleared at ratio 1; else the hunt at ratio 1.3 (navy_fleet_raid) with 60% of the hit
--   points: gathered first (within 8 tiles, 60 s at most), then the enemy's warships (at 1.3), its fishing boats
--   and transports, its coast, the water off its nearest known dock (skipped 3 minutes once there); retreat at half
--   the hit points of the start or outmatched near (0.8) for 60 s; the guard point meanwhile (5 tiles from the
--   fishing boats toward the nearest known enemy building, on open water out of danger);
-- - TASK FORCES (stage 4) only when clearly the stronger (ratio 1.5 against every enemy warship known, 6+ ships; one
--   fleet again under 1.1): an escort of a quarter while a landing is on, of the rest a sixth home, a fifth raiders, a
--   sixth patrol (from 6), the others hunters; ships keep their force, a force over its size (one more kept) lets
--   the least fitting go, one under takes the best fitting free ship (k_tf_fit). Hunters: the enemy warship group
--   (within 12 tiles of one) where they are 1.2+ stronger, near ones first; raiders: transports 4, fishing boats 3,
--   coastal citizens 3, docks 2, buildings 1, soldiers 0.5 over (1 + d / 25) where the enemy ships within 12 are not
--   stronger; patrol: transports within 16, warships there at 1 (else back off), the sea lane's waypoints (35% and
--   65% of the way to the nearest known enemy building, 10 tiles to each side half way); escort: the landing's route
--   cleared or covered; no target: the enemy's fishing grounds, its shore, a sweep. Retreat at half the hit points
--   or outmatched near, 45 s; damaged under half: repairs;
-- - SHIPS TO THE DOCKS (cover.c fall_back fleet_cover, on by default): a warship under half its hit points breaks off
--   to the water by the nearest dock out of danger (one with an own tower within 8 tiles counts half the way) until
--   80%; a retreat goes under the towers (the rally while they cover it), else to the haven;
-- - FISHING BOATS (navy_fleet_boats): an enemy warship seen within 10 tiles of a boat and no fleet within 20 that wins
--   (ratio 1): the boat goes to its nearest dock; released a minute later (the engine gives it new work).
-- The fleet's own warship orders (fl_build) are in ageofai.navy (the account pays them).
local S = require("ageofai.settings")
local M = {}

-- roles (navy.c nv_role): 0 none, F frigates, L battleships, A galleys and catapult ships, S attack submarines,
-- AA cruisers (aircraft only)
M.NONE, M.FRIG, M.LINE, M.ANTI, M.SUB, M.AA = 0, 1, 2, 3, 4, 5
M.ROLE_NAMES = {[0] = "-", "F", "L", "A", "S", "AA"}
local role_cache = {}   -- type name -> role (static data)
function M.role(t)
    if t == nil then
        return 0
    end
    local r = role_cache[t]
    if r ~= nil then
        return r
    end
    r = 0
    if t:find("fishing") or t:find("transport") or t:find("carrier") or t:find("naval_mine") or t:find("flattop") then
        r = 0
    elseif t:find("trident") or t:find("triton") then
        r = 0   -- missile submarines hit no ship
    elseif t:find("battleship") then
        r = M.LINE
    elseif t:find("frigate") or t:find("warrington") then
        r = M.FRIG
    elseif t:find("cruiser") then
        r = M.AA
    elseif t:find("catapult") or t:find("galley") or t:find("galleon") then
        r = M.ANTI
    elseif t:find("submarine") or t:find("u_boat") then
        r = M.SUB
    end
    role_cache[t] = r
    return r
end

-- the hit factor of a weapon role against a target role (docs/navy.md 1, in percent); catapult ships 450 against L
local FACTOR = {
    [M.FRIG] = {[M.FRIG] = 100, [M.LINE] = 100, [M.ANTI] = 100, [M.SUB] = 100, [M.AA] = 100},
    [M.LINE] = {[M.FRIG] = 100, [M.LINE] = 100, [M.ANTI] = 25, [M.SUB] = 0, [M.AA] = 100},
    [M.ANTI] = {[M.FRIG] = 100, [M.LINE] = 380, [M.ANTI] = 100, [M.SUB] = 0, [M.AA] = 100},
    [M.SUB] = {[M.FRIG] = 100, [M.LINE] = 350, [M.ANTI] = 100, [M.SUB] = 100, [M.AA] = 100},
    [M.AA] = {[M.FRIG] = 0, [M.LINE] = 0, [M.ANTI] = 0, [M.SUB] = 0, [M.AA] = 0},
}
function M.factor(atype, ttype)
    local a, t = M.role(atype), M.role(ttype)
    if a == 0 then
        return 0
    end
    if t == 0 then
        -- fishing boats, transports and carriers (and every land or air unit) are hit by every warship but cruisers
        return a == M.AA and 0 or 100
    end
    local f = FACTOR[a][t]
    if a == M.ANTI and t == M.LINE and atype:find("catapult") then
        f = 450
    end
    return f
end
function M.hits(atype, ttype)
    return M.factor(atype, ttype) > 0
end

-- the kinds of a remembered type by its class name (ai.memory keeps no category): warships (a role), fishing boats,
-- sea transports, aircraft, siege
function M.kind(t)
    if t == nil then
        return "other"
    end
    if M.role(t) ~= 0 then
        return "ship"
    end
    if t:find("fishing") then
        return "boat"
    end
    if t:find("^s%d%d_") and t:find("transport") then
        return "transport"
    end
    if t:find("^a_") then
        return "air"
    end
    if t:find("siege") or t:find("artillery") or t:find("cannon") or t:find("howitzer") or t:find("catapult") then
        return "siege"
    end
    return "other"
end
-- the enemy's mobile units remembered within `age` ms, of a kind (M.kind)
function M.remembered(now, age, kind)
    local out = {}
    for _, r in ipairs(ai.memory{since = math.max(now - age, 0)}) do
        if not r.building and M.kind(r.type) == kind then
            out[#out + 1] = r
        end
    end
    return out
end

local cls_cache = {}
function M.cls(t)
    local c = cls_cache[t]
    if c == nil then
        c = ai.class(t) or {}
        cls_cache[t] = c
    end
    return c
end
-- damage a second x 1000 (attack x 1000 / (reload + 2 s)), as upstream's fl_dps x 1000
function M.dps(t)
    local c = M.cls(t)
    local a = c.attack or 0
    local rl = (c.reload or 0) > 0 and c.reload or 2000
    return a > 0 and a * 1000000 // (rl + 2000) or 0
end

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

-- ---------------------------------------------------------------------------------------------------------------
-- The scan (every 2 s): own warships (records), the enemy ships known, the prey, the coast, the danger zones, the
-- own docks, towers, boats and buildings. Kept in a module local: rebuilt from the engine every tick

local scan = {}

local function records(ids)
    local out = {}
    if #ids == 0 then
        return out
    end
    local cols = ai.get(ids, {"x", "y", "hp", "hp_max", "type", "action", "target", "tx", "ty"})
    for k = 1, #ids do
        if cols.x[k] ~= nil then
            out[#out + 1] = {id = ids[k], x = cols.x[k], y = cols.y[k], hp = cols.hp[k] or 0, hp_max = cols.hp_max[k] or 1,
                             type = cols.type[k], action = cols.action[k], target = cols.target[k], tx = cols.tx[k], ty = cols.ty[k]}
        end
    end
    return out
end

-- a land unit or building threatens ships: a defence with an attack (its range + 2), a unit whose weapon hits
-- battleships x3+ (siege) or reaches 7+ tiles
local function danger_r(rec)
    local c = M.cls(rec.type)
    if (c.attack or 0) <= 0 then
        return 0
    end
    local range = (c.range or 0) // 100
    if rec.building then
        return range <= 1 and 0 or range + 1 + 2
    end
    if rec.category == "siege" or range >= 7 then
        return range >= 2 and range + 2 or 0
    end
    return 0
end

function M.scan(view, mem, exclude)
    local now = view.time
    local s = {t = now, ships = {}, es = {}, ev = {}, ec = {}, dz = {}, boats = {}, bld = {}, docks = {}, towers = {},
               eb = nil, ed = {}}
    scan = s
    local ex = {}
    for _, id in ipairs(exclude or {}) do
        ex[id] = true
    end
    local ids = {}
    for _, id in ipairs(ai.units{category = "ship", armed = true}) do
        if not ex[id] then
            ids[#ids + 1] = id
        end
    end
    for _, r in ipairs(records(ids)) do
        if M.role(r.type) ~= 0 then
            s.ships[#s.ships + 1] = r
        end
    end
    -- the enemy's warships: seen now, remembered 5 minutes; its prey 2 minutes
    local mobile = ai.memory{since = math.max(now - 300000, 0)}
    for _, r in ipairs(mobile) do
        local kind = M.kind(r.type)
        if not r.building then
            if kind == "ship" then
                s.es[#s.es + 1] = {id = r.id, x = r.x, y = r.y, hp = r.hp, type = r.type, seen = now - r.seen <= 2000, player = r.player}
            elseif (kind == "boat" or kind == "transport") and now - r.seen <= 120000 then
                s.ev[#s.ev + 1] = {id = r.id, x = r.x, y = r.y, hp = r.hp, type = r.type, seen = now - r.seen <= 2000, category = kind}
            elseif kind == "siege" then
                local dr = danger_r({type = r.type, category = "siege"})
                if dr > 0 and #s.dz < 384 then
                    s.dz[#s.dz + 1] = {x = r.x, y = r.y, r = dr}
                end
            end
        end
    end
    -- own: boats, docks, towers (armed buildings), buildings
    s.boats = ai.units{category = "boat"}
    local bids = ai.units{category = "building", built = true}
    if #bids > 0 then
        local cols = ai.get(bids, {"x", "y", "type", "armed", "range"})
        for k = 1, #bids do
            if cols.x[k] ~= nil then
                s.bld[#s.bld + 1] = {x = cols.x[k], y = cols.y[k]}
                if cols.type[k] == "b_dock" then
                    s.docks[#s.docks + 1] = {id = bids[k], x = cols.x[k], y = cols.y[k]}
                elseif cols.armed[k] and (cols.range[k] or 0) >= 200 then
                    s.towers[#s.towers + 1] = {x = cols.x[k], y = cols.y[k], r = cols.range[k] // 100}
                end
            end
        end
    end
    -- the enemy's buildings known: the nearest to the base, its docks, the danger zones
    local tx, ty = view.town_x, view.town_y
    local bd = nil
    for _, r in ipairs(ai.memory{buildings = true}) do
        local d = d2(r.x, r.y, tx, ty)
        if bd == nil or d < bd then
            bd = d
            s.eb = {x = r.x, y = r.y}
        end
        if r.type == "b_dock" and #s.ed < 16 then
            s.ed[#s.ed + 1] = {x = r.x, y = r.y}
        end
        local dr = danger_r({type = r.type, building = true})
        if dr > 0 and #s.dz < 384 then
            s.dz[#s.dz + 1] = {x = r.x, y = r.y, r = dr}
        end
    end
    -- the enemy on the coast of the fleet's sea, seen now within 40 tiles of the guard point (the hunt's and the
    -- raiders' targets): citizens, soldiers, siege, buildings with the sea within 4 tiles
    if mem.gx ~= nil and mem.ocean ~= nil then
        local seen = ai.seen{enemy = true, near = {mem.gx, mem.gy, 40}, limit = 24}
        if #seen > 0 then
            local cols = ai.get(seen, {"x", "y", "type", "category", "hp"})
            for k = 1, #seen do
                local cat = cols.category[k]
                if cols.x[k] ~= nil and (cat == "citizen" or cat == "soldier" or cat == "siege" or cat == "building") and M.coast(cols.x[k], cols.y[k], mem.ocean) then
                    s.ec[#s.ec + 1] = {id = seen[k], x = cols.x[k], y = cols.y[k], type = cols.type[k], category = cat, hp = cols.hp[k], seen = true}
                end
            end
        end
    end
    return s
end

-- land with the sea `oc` within 4 tiles (8 looks)
function M.coast(x, y, oc)
    local t = ai.tile(x, y)
    if t == nil or (t.sea or 0) ~= 0 then
        return false
    end
    for _, d in ipairs({{2, 0}, {-2, 0}, {0, 2}, {0, -2}, {4, 0}, {-4, 0}, {0, 4}, {0, -4}}) do
        local q = ai.tile(x + d[1], y + d[2])
        if q ~= nil and q.sea == oc then
            return true
        end
    end
    return false
end
local function sea_of(x, y)
    local t = ai.tile(x, y)
    return t ~= nil and t.sea or 0
end
-- in a danger zone (+ m tiles): the zone's index or nil
function M.in_danger(x, y, m)
    m = m or 0
    for k, z in ipairs(scan.dz or {}) do
        local r = z.r + m
        if d2(x, y, z.x, z.y) <= r * r then
            return k
        end
    end
    return nil
end
local function way_safe(ax, ay, bx, by)
    local dx, dy = bx - ax, by - ay
    local len = isqrt(dx * dx + dy * dy)
    local steps = len // 2 + 1
    for i = 1, steps do
        if M.in_danger(ax + dx * i // steps, ay + dy * i // steps) then
            return false
        end
    end
    return true
end
-- water of the sea oc near p (within 8), else nil
local function water_near(x, y, oc)
    if sea_of(x, y) == oc then
        return x, y
    end
    for d = 1, 8 do
        for _, p in ipairs({{d, 0}, {-d, 0}, {0, d}, {0, -d}, {d, d}, {-d, -d}, {d, -d}, {-d, d}}) do
            if sea_of(x + p[1], y + p[2]) == oc then
                return x + p[1], y + p[2]
            end
        end
    end
    return nil
end
-- a safe water point on the way from `from` to `to`, as near to the target as it gets (2-tile steps back); stage 4:
-- 3 tiles clear of a zone
local function safe_toward(fx, fy, tx, ty, oc, margin)
    local dx, dy = fx - tx, fy - ty
    local len = isqrt(dx * dx + dy * dy)
    local steps = len // 2 + 1
    for i = 0, steps do
        local qx, qy = tx + dx * i // steps, ty + dy * i // steps
        if sea_of(qx, qy) == oc and not M.in_danger(qx, qy, margin) then
            local wx, wy = water_near(qx, qy, oc)
            if wx ~= nil and not M.in_danger(wx, wy, margin) then
                return wx, wy
            end
        end
    end
    return nil
end

-- the strength ratio of the ships given (all, or within r of p) against the enemy ships listed; 99 when the enemy
-- has nothing that hurts them (fl_ratio: sqrt(own hp x own dps / (enemy hp x enemy dps)), each side's damage spread
-- over the other's hit points by the hit factors)
local function ratio_of(ships, foes, px, py, r)
    local he = 0
    for _, e in ipairs(foes) do
        he = he + e.hp
    end
    if he <= 0 then
        return 99
    end
    local ho, any = 0, false
    for _, u in ipairs(ships) do
        if r == nil or d2(u.x, u.y, px, py) <= r * r then
            ho = ho + u.hp
            any = true
        end
    end
    if not any or ho <= 0 then
        return 0
    end
    local dofs, de = 0.0, 0.0
    for _, u in ipairs(ships) do
        if r == nil or d2(u.x, u.y, px, py) <= r * r then
            local fh = 0.0
            local dp = M.dps(u.type) / 1000.0
            for _, e in ipairs(foes) do
                fh = fh + e.hp * M.factor(u.type, e.type) / 100.0
                de = de + (M.dps(e.type) / 1000.0) * u.hp * M.factor(e.type, u.type) / 100.0
            end
            dofs = dofs + dp * fh / he
        end
    end
    de = de / ho
    if de <= 0 then
        return dofs > 0 and 99 or 1
    end
    if dofs <= 0 then
        return 0
    end
    return math.sqrt(ho * dofs / (he * de))
end
M.ratio_of = ratio_of
local function sel(list, pred)
    local out = {}
    for k, e in ipairs(list) do
        if pred(e, k) then
            out[#out + 1] = e
        end
    end
    return out
end
local function centre(ships, gx, gy)
    if #ships == 0 then
        return gx, gy
    end
    local x, y = 0, 0
    for _, u in ipairs(ships) do
        x, y = x + u.x, y + u.y
    end
    return x // #ships, y // #ships
end
local function hp_of(ships)
    local h, m = 0, 0
    for _, u in ipairs(ships) do
        h, m = h + u.hp, m + u.hp_max
    end
    return h, m
end

-- ---------------------------------------------------------------------------------------------------------------
-- Points: the guard point, the haven, the rally

-- the guard point: from the fishing boats' middle (the docks' with fewer than 2 boats) 5 tiles toward the nearest
-- known enemy building, on open water of the fleet's sea, out of danger
local function guard_point(mem)
    local s = scan
    local bx, by, n = 0, 0, 0
    if #s.boats > 0 then
        local cols = ai.get(s.boats, {"x", "y"})
        for k = 1, #s.boats do
            if cols.x[k] ~= nil and sea_of(cols.x[k], cols.y[k]) == mem.ocean then
                bx, by, n = bx + cols.x[k], by + cols.y[k], n + 1
            end
        end
    end
    if n < 2 then
        bx, by, n = 0, 0, 0
        for _, dk in ipairs(s.docks) do
            if dk.sea == nil then
                local wx, wy = water_near(dk.x, dk.y, mem.ocean)
                dk.sea = wx ~= nil and mem.ocean or 0
                dk.wx, dk.wy = wx, wy
            end
            if dk.sea == mem.ocean then
                bx, by, n = bx + dk.x, by + dk.y, n + 1
            end
        end
    end
    if n == 0 then
        return
    end
    local mx, my = bx // n, by // n
    local tx, ty = mx, my
    if s.eb ~= nil then
        local dx, dy = s.eb.x - mx, s.eb.y - my
        local len = isqrt(dx * dx + dy * dy)
        if len > 1 then
            tx, ty = mx + 5 * dx // len, my + 5 * dy // len
        end
    end
    local gx, gy = safe_toward(mx, my, tx, ty, mem.ocean, 0)
    if gx ~= nil then
        mem.gx, mem.gy = gx, gy
    end
end
-- the haven: open water by an own tower near the sea farthest from the nearest known enemy building, else by such
-- a dock; out of danger
local function haven(mem)
    local s = scan
    local bd = -1
    for pass = 1, 2 do
        if bd >= 0 then
            break
        end
        local list = pass == 1 and s.towers or s.docks
        for _, b in ipairs(list) do
            local wx, wy = water_near(b.x, b.y, mem.ocean)
            if wx ~= nil and not M.in_danger(wx, wy) then
                local d = s.eb ~= nil and d2(b.x, b.y, s.eb.x, s.eb.y) or 0
                if d > bd then
                    bd = d
                    mem.hx, mem.hy = wx, wy
                end
            end
        end
    end
    if bd < 0 then
        mem.hx, mem.hy = mem.gx, mem.gy
    end
end
-- the rally (fl_rally, every 20 s): the water tile of the fleet's sea inside the reach (range - 1) of the most own
-- towers, then by a dock, far from the enemy, near its tower; out of danger. None: the haven
local function rally(mem, now)
    if mem.rally_t ~= nil and now - mem.rally_t < 20000 and mem.rx ~= nil and not M.in_danger(mem.rx, mem.ry) then
        return
    end
    mem.rally_t = now
    local s = scan
    local best, bc, br = nil, 0, 0
    local looked = 0
    for _, tw in ipairs(s.towers) do
        local R = math.min(tw.r - 1, 10)
        if R >= 2 and looked < 4 then
            looked = looked + 1
            for dx = -R, R, 2 do
                for dy = -R, R, 2 do
                    local qx, qy = tw.x + dx, tw.y + dy
                    if dx * dx + dy * dy <= R * R and sea_of(qx, qy) == mem.ocean and not M.in_danger(qx, qy) then
                        local cov, cr, dock = 0, 0, 0
                        for _, t2 in ipairs(s.towers) do
                            local rr = t2.r - 1
                            if d2(qx, qy, t2.x, t2.y) <= rr * rr then
                                cov = cov + 1
                                if t2.r > cr then
                                    cr = t2.r
                                end
                            end
                        end
                        for _, dk in ipairs(s.docks) do
                            if d2(dk.x, dk.y, qx, qy) <= 64 then
                                dock = 1
                            end
                        end
                        local away = s.eb ~= nil and isqrt(d2(qx, qy, s.eb.x, s.eb.y)) or 0
                        local sc = 1000 * cov + 200 * dock + 2 * away - 10 * isqrt(dx * dx + dy * dy)
                        if best == nil or sc > best then
                            best, bc, br = sc, cov, cr
                            mem.rx, mem.ry = qx, qy
                        end
                    end
                end
            end
        end
    end
    mem.rcover = bc
    mem.rcr = bc > 0 and br + 2 or 8
    if bc == 0 then
        mem.rx, mem.ry = mem.hx, mem.hy
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Orders

local function tower_cover_near(x, y)
    for _, tw in ipairs(scan.towers) do
        if d2(tw.x, tw.y, x, y) <= 64 then
            return true
        end
    end
    return false
end
-- the dock a damaged ship goes to: the nearest out of danger, one with an own tower within 8 tiles counting half
local function repair_dock(x, y)
    local best, bd = nil, nil
    for _, dk in ipairs(scan.docks) do
        if not M.in_danger(dk.x, dk.y) then
            local d = isqrt(d2(dk.x, dk.y, x, y))
            if tower_cover_near(dk.x, dk.y) then
                d = d // 2
            end
            if bd == nil or d < bd then
                best, bd = dk, d
            end
        end
    end
    return best
end
-- the move to a point: round a danger zone on the way through a waypoint 8 or 16 tiles off the line, else not at
-- all; one order per ship, skipped while its goal is already within 2.5 tiles of the point
local function fl_move(f, u, tx, ty, k)
    local oc = sea_of(u.x, u.y)
    if oc == 0 then
        oc = f.ocean
    end
    tx, ty = tx + (k % 3) - 1, ty + (k // 3 % 3) - 1
    local dx, dy = tx - u.x, ty - u.y
    if dx * dx + dy * dy <= 9 then
        return
    end
    if not way_safe(u.x, u.y, tx, ty) then
        local ok = false
        local len = isqrt(dx * dx + dy * dy)
        for _, off in ipairs({8, -8, 16, -16}) do
            local mx, my = (u.x + tx) // 2 - dy * off // len, (u.y + ty) // 2 + dx * off // len
            local wx, wy = water_near(mx, my, oc)
            if wx ~= nil and not M.in_danger(wx, wy) and way_safe(u.x, u.y, wx, wy) then
                tx, ty, ok = wx, wy, true
                break
            end
        end
        if not ok then
            return
        end
    end
    if u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, tx, ty) <= 6 then
        return
    end
    ai.move(u.id, tx, ty, {forced = true})
    f.orders = (f.orders or 0) + 1
end
-- out of a danger zone: away from its centre to its edge + 3 tiles (or 45 / 90 degrees off), else the guard point
local function flee(f, mem, u)
    local z = M.in_danger(u.x, u.y)
    if z == nil then
        return false
    end
    local zone = scan.dz[z]
    local oc = sea_of(u.x, u.y)
    local dx, dy = u.x - zone.x, u.y - zone.y
    local len = isqrt(dx * dx + dy * dy)
    if len < 1 then
        dx, dy = (mem.gx or u.x) - u.x, (mem.gy or u.y) - u.y
        len = isqrt(dx * dx + dy * dy)
    end
    if len < 1 then
        dx, dy, len = 1, 0, 1
    end
    local go = math.max(zone.r - len + 3, 3)
    for _, rot in ipairs({{1000, 0}, {707, 707}, {707, -707}, {0, 1000}, {0, -1000}}) do
        local ex = (dx * rot[1] - dy * rot[2]) // (len * 1000)
        local ey = (dx * rot[2] + dy * rot[1]) // (len * 1000)
        local wx, wy = nil, nil
        if oc ~= 0 then
            wx, wy = water_near(u.x + ex * go, u.y + ey * go, oc)
        end
        if wx ~= nil and not M.in_danger(wx, wy) then
            if not (u.action == "move" and u.tx ~= nil and d2(u.tx, u.ty, wx, wy) <= 6) then
                ai.move(u.id, wx, wy, {forced = true})
                f.orders = (f.orders or 0) + 1
            end
            f.flees = (f.flees or 0) + 1
            return true
        end
    end
    if mem.gx ~= nil then
        ai.move(u.id, mem.gx, mem.gy, {forced = true})
        f.orders = (f.orders or 0) + 1
    end
    f.flees = (f.flees or 0) + 1
    return true
end
-- the nearest enemy ship seen now (of `use`) that u can hit, out of danger (+3), within r of p; frigates take the
-- submarines first (x0.25)
local function pick(f, u, use, px, py, r)
    local best, bd = nil, nil
    local oc = sea_of(u.x, u.y)
    for _, e in ipairs(use or scan.es) do
        if e.seen and not M.in_danger(e.x, e.y, 3) and d2(e.x, e.y, px, py) <= r * r and (oc == 0 or sea_of(e.x, e.y) == oc) and M.hits(u.type, e.type) then
            local d = d2(e.x, e.y, u.x, u.y)
            if M.role(e.type) == M.SUB and M.role(u.type) == M.FRIG then
                d = d // 4
            end
            if bd == nil or d < bd then
                best, bd = e, d
            end
        end
    end
    return best
end
-- each ship: out of danger first; damaged to the dock; a target inside a danger zone dropped; the task's target,
-- else to p. The attacks are batched by target
local function orders(f, mem, use, px, py, r, hunt_prey, retreat, leash)
    local attacks = {}
    local targets = {}
    local now = ai.time()
    mem.docked = mem.docked or {}
    for i, u in ipairs(f.ships) do
        local done = false
        if flee(f, mem, u) then
            done = true
        end
        -- SEA (fleet_cover): a damaged ship off to the dock before anything else, until 80%
        if not done and S.fleet_cover and #scan.docks > 0 then
            local docked = mem.docked[u.id]
            if u.hp * 2 < u.hp_max or docked ~= nil then
                local dk = repair_dock(u.x, u.y)
                if dk ~= nil then
                    if docked == nil then
                        mem.docked[u.id] = u.hp
                    end
                    if d2(dk.x, dk.y, u.x, u.y) > 16 then
                        local wx, wy = water_near(dk.x, dk.y, sea_of(u.x, u.y) ~= 0 and sea_of(u.x, u.y) or f.ocean)
                        if wx ~= nil then
                            fl_move(f, u, wx, wy, 0)
                        end
                    end
                    done = true
                end
            end
        end
        if not done and retreat then
            fl_move(f, u, px, py, i)
            done = true
        end
        if not done then
            -- keeps fighting a target out of danger and within the leash
            local cur = nil
            if u.action == "attack" and u.target ~= nil then
                for _, e in ipairs(scan.es) do
                    if e.id == u.target then
                        cur = e
                    end
                end
                if cur == nil then
                    for _, e in ipairs(scan.ev) do
                        if e.id == u.target then
                            cur = e
                        end
                    end
                end
                if cur == nil then
                    for _, e in ipairs(scan.ec) do
                        if e.id == u.target then
                            cur = e
                        end
                    end
                end
            end
            local leashed = leash ~= nil and cur ~= nil and d2(cur.x, cur.y, leash.x, leash.y) > leash.r * leash.r
            if cur ~= nil and not leashed and not M.in_danger(cur.x, cur.y) then
                done = true
            end
        end
        if not done and u.hp * 100 < 35 * u.hp_max and #scan.docks > 0 then   -- to the dock (repair)
            local best, bd = nil, nil
            for _, dk in ipairs(scan.docks) do
                local d = d2(dk.x, dk.y, u.x, u.y)
                if bd == nil or d < bd then
                    best, bd = dk, d
                end
            end
            if mem.docked[u.id] == nil then
                mem.docked[u.id] = u.hp
            end
            if bd > 16 then
                fl_move(f, u, best.x, best.y, 0)
            end
            done = true
        end
        if not done then
            local t = pick(f, u, use, px, py, r)
            if t == nil and hunt_prey then
                for pass = 1, 2 do
                    if t ~= nil then
                        break
                    end
                    local list = pass == 1 and scan.ev or scan.ec
                    local bd = nil
                    for _, e in ipairs(list) do
                        if e.seen and not M.in_danger(e.x, e.y, 3) and M.hits(u.type, e.type) then
                            local d = d2(e.x, e.y, px, py)
                            if pass == 2 then
                                d = e.category == "citizen" and d // 4 or e.category == "soldier" and d * 2 or d   -- the economy first
                            end
                            if d <= r * r and (bd == nil or d < bd) then
                                bd, t = d, e
                            end
                        end
                    end
                end
            end
            if t ~= nil then
                if u.target ~= t.id then
                    attacks[t.id] = attacks[t.id] or {}
                    local a = attacks[t.id]
                    a[#a + 1] = u.id
                    targets[#targets + 1] = t.id
                end
            else
                local oc = sea_of(u.x, u.y)
                if oc == f.ocean or oc == 0 then
                    fl_move(f, u, px, py, i)
                end
            end
        end
    end
    table.sort(targets)
    local last = nil
    for _, tid in ipairs(targets) do
        if tid ~= last then
            last = tid
            ai.attack(attacks[tid], tid)
            f.orders = (f.orders or 0) + 1
        end
    end
    -- repaired: ships back at 80% leave the docked list
    for _, id in ipairs(ai.keys(mem.docked)) do
        local u = nil
        for _, q in ipairs(f.ships) do
            if q.id == id then
                u = q
            end
        end
        if u == nil or u.hp * 10 >= 8 * u.hp_max then
            if u ~= nil then
                mem.repaired = (mem.repaired or 0) + math.max(u.hp - mem.docked[id], 0)
            end
            mem.docked[id] = nil
        end
    end
end

-- fishing boats near enemy warships the fleet doesn't win against: to the nearest dock; released a minute later
local function boats(mem, fleet, cx, cy)
    mem.pulled = mem.pulled or {}
    local now = ai.time()
    for _, id in ipairs(ai.keys(mem.pulled)) do
        if now - mem.pulled[id] >= 60000 or ai.unit(id) == nil then
            if ai.unit(id) ~= nil then
                ai.release(id)
            end
            mem.pulled[id] = nil
        end
    end
    if not S.navy_fleet_boats or #scan.boats == 0 or #scan.es == 0 then
        return
    end
    local cols = ai.get(scan.boats, {"x", "y"})
    for k, id in ipairs(scan.boats) do
        local bx, by = cols.x[k], cols.y[k]
        if bx ~= nil and mem.pulled[id] == nil then
            local foes = sel(scan.es, function(e) return e.seen and d2(e.x, e.y, bx, by) <= 100 and M.hits(e.type, "s02_fishing_boat_stone") end)
            if #foes > 0 and not (#fleet > 0 and d2(cx, cy, bx, by) <= 400 and ratio_of(fleet, foes) >= 1.0) then
                local best, bd = nil, nil
                for _, dk in ipairs(scan.docks) do
                    local d = d2(dk.x, dk.y, bx, by)
                    if bd == nil or d < bd then
                        best, bd = dk, d
                    end
                end
                if best ~= nil and bd > 16 then
                    ai.take(id)
                    ai.move(id, best.x, best.y, {forced = true})
                    mem.pulled[id] = now
                    mem.pulls = (mem.pulls or 0) + 1
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Decisions

local function set_task(f, task, why, now)
    if f.task ~= task then
        f.task = task
        f.task_t = now
    end
    f.why = why
end
-- the home threats: enemy warships seen within 15 tiles of the guard point, the buildings or the fishing boats
local function home_threats(mem)
    local s = scan
    local bcols = #s.boats > 0 and ai.get(s.boats, {"x", "y"}) or nil
    local home, n, sx, sy = {}, 0, 0, 0
    for _, e in ipairs(s.es) do
        if e.seen then
            local inr = mem.gx ~= nil and d2(e.x, e.y, mem.gx, mem.gy) <= 225
            if not inr and bcols ~= nil then
                for k = 1, #s.boats do
                    if bcols.x[k] ~= nil and d2(bcols.x[k], bcols.y[k], e.x, e.y) <= 225 then
                        inr = true
                        break
                    end
                end
            end
            if not inr then
                for _, b in ipairs(s.bld) do
                    if d2(b.x, b.y, e.x, e.y) <= 225 then
                        inr = true
                        break
                    end
                end
            end
            if inr then
                home[#home + 1] = e
                n = n + 1
                sx, sy = sx + e.x, sy + e.y
            end
        end
    end
    return home, n, sx, sy
end
local function strength(f, cx, cy)
    f.eknown = #scan.es
    f.ratio = math.floor(math.min(ratio_of(f.ships, scan.es), 99) * 100)   -- x 100: the memory holds integers
    local near = sel(scan.es, function(e) return e.seen and d2(e.x, e.y, cx, cy) <= 144 end)
    f.enear = #near
    f.near_ratio = math.floor(math.min(ratio_of(f.ships, near, cx, cy, 12), 99) * 100)
end
local function spread(f, cx, cy)
    for _, u in ipairs(f.ships) do
        if d2(u.x, u.y, cx, cy) > 64 then
            return true
        end
    end
    return false
end
local function cleared(mem, now, x, y)
    for _, c in ipairs(mem.cleared or {}) do
        if now < c.until_t and math.abs(c.x - x) < 6 and math.abs(c.y - y) < 6 then
            return true
        end
    end
    return false
end
local function clear(mem, now, x, y)
    mem.cleared = mem.cleared or {}
    mem.ci = (mem.ci or 0) % 8 + 1
    mem.cleared[mem.ci] = {x = x, y = y, until_t = now + 180000}
end

-- the fleet's decision (stage 3: the whole fleet; stage 4: the home guard). flags: home (no hunt, no landing),
-- mass (the whole fleet at the rally). Returns: the task changed
local function decide(mem, f, now, flags, L)
    local cx, cy = centre(f.ships, mem.gx, mem.gy)
    strength(f, cx, cy)
    local hp, hmax = hp_of(f.ships)
    local task, prev, why = "guard", f.task, "nothing to do"
    local tx, ty, r, prey, use = mem.gx, mem.gy, 10, false, nil
    local home, nh, hsx, hsy = home_threats(mem)
    if flags.mass and #f.ships > 0 then
        local R = (mem.rcr or 0) > 0 and mem.rcr or 8
        local cov = sel(scan.es, function(e) return e.seen and d2(e.x, e.y, mem.rx, mem.ry) <= R * R end)
        task, tx, ty, r = "mass", mem.rx, mem.ry, R
        if #cov > 0 and ratio_of(f.ships, cov) >= ((mem.rcover or 0) > 0 and 0.5 or 0.8) then
            task, why, use = "defend", (mem.rcover or 0) > 0 and "enemy ships at the rally - with the towers" or "enemy ships at the rally", cov
            if f.task ~= "defend" then
                f.defends = (f.defends or 0) + 1
            end
        else
            why = #cov > 0 and "massing - the enemy at the rally too strong" or (mem.rcover or 0) > 0 and "massing under the towers" or "massing by the dock"
        end
        local changed = task ~= prev
        set_task(f, task, why, now)
        f.gox, f.goy = tx, ty
        orders(f, mem, use, tx, ty, r, false, false, {x = tx, y = ty, r = R + 3})
        return changed
    end
    if #f.ships == 0 then
        why = "no warships"
    elseif not flags.home and L ~= nil and L.phase >= 2 and L.ocean == f.ocean then   -- a landing under way: the rest off its beach
        task, why, r = "support", "landing under way", 12
        local sx, sy = safe_toward(cx, cy, L.water_x, L.water_y, f.ocean, 3)
        if sx ~= nil then
            tx, ty = sx, sy
        end
    elseif nh > 0 and ratio_of(f.ships, home) >= 0.8 then
        task, why, use, tx, ty, r = "defend", "enemy ships at home", home, hsx // nh, hsy // nh, 20
        if f.task ~= "defend" then
            f.defends = (f.defends or 0) + 1
        end
    elseif nh > 0 then
        task, why = "guard", "enemy ships at home - too strong"
        if mem.hx ~= nil then
            tx, ty, r, why = mem.hx, mem.hy, 8, "enemy ships at home - too strong: to the haven"
        end
    end
    if task == "guard" and not flags.home and L ~= nil and L.phase == 1 and L.ocean == f.ocean then   -- a landing getting its transports: the route cleared
        local mx, my = (mem.gx + L.water_x) // 2, (mem.gy + L.water_y) // 2
        local route = sel(scan.es, function(e) return d2(e.x, e.y, L.water_x, L.water_y) <= 225 or d2(e.x, e.y, mx, my) <= 225 end)
        if #route > 0 and ratio_of(f.ships, route) >= 1.0 then
            task, why, use, r = "escort", "clearing the landing's sea", route, 16
            local sx, sy = safe_toward(mem.gx, mem.gy, L.water_x, L.water_y, f.ocean, 3)
            if sx ~= nil then
                tx, ty = sx, sy
            end
            if f.task ~= "escort" then
                f.escorts = (f.escorts or 0) + 1
            end
        end
    end
    if task == "guard" and #f.ships > 0 and nh == 0 and not flags.home then
        -- retreat: a hunt that went wrong - 60 s at the guard point at least
        if f.task == "hunt" and (hp * 2 < (f.hp0 or 0) or (f.enear > 0 and f.near_ratio < 80)) then
            f.retreats = (f.retreats or 0) + 1
            f.retreat_t = now
            set_task(f, "retreat", hp * 2 < (f.hp0 or 0) and "half the hit points lost" or "outmatched near", now)
        end
        if f.task == "retreat" and now - (f.retreat_t or 0) < 60000 then
            task, why = "retreat", f.why
        elseif f.ratio >= S.navy_fleet_raid and hp * 10 >= 6 * hmax then
            -- the hunt's target: the enemy's warships (nearest, out of danger, on its sea) when clearly stronger (1.3),
            -- else its fishing boats and transports, its coast, the water off its nearest known dock
            local hx, hy = nil, nil
            local bd = nil
            if f.ratio >= 130 then
                for _, e in ipairs(scan.es) do
                    local d = d2(e.x, e.y, cx, cy)
                    if (bd == nil or d < bd) and sea_of(e.x, e.y) == f.ocean and not M.in_danger(e.x, e.y, 3) then
                        bd, hx, hy, why = d, e.x, e.y, "the enemy fleet"
                    end
                end
            end
            if hx == nil then
                local pd = nil
                for _, e in ipairs(scan.ev) do
                    local d = d2(e.x, e.y, cx, cy)
                    if (pd == nil or d < pd) and sea_of(e.x, e.y) == f.ocean and not M.in_danger(e.x, e.y, 3) then
                        pd, hx, hy, why = d, e.x, e.y, "fishing boats, transports"
                    end
                end
            end
            if hx == nil and S.navy_fleet_coast then
                local cd = nil
                local ce = nil
                for _, e in ipairs(scan.ec) do
                    local d = d2(e.x, e.y, cx, cy)
                    if (cd == nil or d < cd) and not M.in_danger(e.x, e.y, 3) then
                        cd, ce = d, e
                    end
                end
                if ce ~= nil then
                    hx, hy = safe_toward(cx, cy, ce.x, ce.y, f.ocean, 3)
                    if hx ~= nil then
                        why = "the enemy's coast"
                    end
                end
            end
            if hx == nil then
                local dd = nil
                for _, ed in ipairs(scan.ed) do
                    local d = d2(ed.x, ed.y, cx, cy)
                    if (dd == nil or d < dd) and not cleared(mem, now, ed.x, ed.y) then
                        local wx, wy = safe_toward(cx, cy, ed.x, ed.y, f.ocean, 3)
                        if wx ~= nil then
                            dd, hx, hy, why = d, wx, wy, "the enemy's fishing grounds"
                            f.hunt_x, f.hunt_y = ed.x, ed.y
                        end
                    end
                end
            end
            if hx == nil and (f.task == "hunt" or f.task == "gather") and now - (f.task_t or 0) < 30000 then
                hx, hy, why = f.gox, f.goy, f.task == "hunt" and f.why or "the last target"
            end
            if hx ~= nil then
                if f.task ~= "hunt" and spread(f, cx, cy) and not (f.task == "gather" and now - (f.task_t or 0) >= 60000) then
                    task, why, tx, ty = "gather", "before the hunt", cx, cy
                    local sx, sy = safe_toward(mem.gx, mem.gy, cx, cy, f.ocean, 3)
                    if sx ~= nil then
                        tx, ty = sx, sy
                    end
                else
                    task, tx, ty, r, prey = "hunt", hx, hy, 14, true
                    if f.task ~= "hunt" then
                        f.hunts = (f.hunts or 0) + 1
                        f.hp0 = hp
                    end
                    if why == "the enemy's fishing grounds" and math.abs(cx - hx) < 5 and math.abs(cy - hy) < 5 then
                        clear(mem, now, f.hunt_x, f.hunt_y)
                    end
                end
            end
        end
    end
    if task == "retreat" then
        tx, ty = mem.gx, mem.gy
        if S.fleet_cover then
            if (mem.rcover or 0) > 0 and mem.rx ~= nil then
                tx, ty = mem.rx, mem.ry
            elseif mem.hx ~= nil then
                tx, ty = mem.hx, mem.hy
            end
        end
    end
    local changed = task ~= prev
    set_task(f, task, why, now)
    f.gox, f.goy = tx, ty
    orders(f, mem, use, tx, ty, r, prey, task == "retreat")
    return changed
end

-- the enemy ships known within r of (x, y)
local function local_foes(x, y, r)
    return sel(scan.es, function(e) return d2(e.x, e.y, x, y) <= r * r end)
end
-- the hunters' target: the enemy warship group (within 12 tiles of one) where they are 1.2+ stronger, near ones
-- first; -1 all too strong; 0 none known
local function tf_target_fleet(f, cx, cy)
    local done = {}
    local bs, best, any = 0, nil, 0
    for j, e in ipairs(scan.es) do
        if not done[j] and sea_of(e.x, e.y) == f.ocean and not M.in_danger(e.x, e.y, 3) then
            any = any + 1
            local loc = {}
            for k, q in ipairs(scan.es) do
                if d2(q.x, q.y, e.x, e.y) <= 144 then
                    done[k] = true
                    loc[#loc + 1] = q
                end
            end
            local rr = ratio_of(f.ships, loc)
            if rr >= 1.2 then
                local d = math.sqrt(d2(e.x, e.y, cx, cy))
                local sc = math.min(rr, 3.0) / (1.0 + d / 20.0)
                if sc > bs then
                    bs, best = sc, e
                end
            end
        end
    end
    if best ~= nil then
        return 1, best.x, best.y, "enemy warships"
    end
    return any > 0 and -1 or 0
end
-- the raiders' target: the enemy's economy on the water and the coast
local function tf_target_prey(f, cx, cy)
    local bs, bx, by, bwhy = 0, nil, nil, nil
    for pass = 1, 2 do
        local list = pass == 1 and scan.ev or scan.ec
        for _, e in ipairs(list) do
            local w, wy
            if pass == 1 then
                w, wy = e.category == "transport" and 4.0 or 3.0, e.category == "transport" and "transports" or "fishing boats"
            elseif e.category == "citizen" then
                w, wy = 3.0, "citizens on the coast"
            elseif e.type == "b_dock" then
                w, wy = 2.0, "docks"
            elseif e.category == "building" then
                w, wy = 1.0, "buildings on the coast"
            else
                w, wy = 0.5, "the coast"
            end
            local ok = not (pass == 2 and (not e.seen or not S.navy_fleet_coast)) and not (pass == 1 and sea_of(e.x, e.y) ~= f.ocean)
                       and not M.in_danger(e.x, e.y, 3)
            if ok then
                local d = math.sqrt(d2(e.x, e.y, cx, cy))
                local sc = w / (1.0 + d / 25.0)
                if sc > bs then
                    local loc = local_foes(e.x, e.y, 12)
                    if #loc == 0 or ratio_of(f.ships, loc) >= 1.0 then
                        local ax, ay = e.x, e.y
                        if pass == 2 then
                            ax, ay = safe_toward(cx, cy, e.x, e.y, f.ocean, 3)
                        end
                        if ax ~= nil then
                            bs, bx, by, bwhy = sc, ax, ay, wy
                        end
                    end
                end
            end
        end
    end
    if bx ~= nil then
        return 1, bx, by, bwhy
    end
    return 0
end
-- no target: the enemy's fishing grounds, its shore, a sweep round the guard point (45 degrees every 90 s)
local COS = {1000, 707, 0, -707, -1000, -707, 0, 707}
local SIN = {0, 707, 1000, 707, 0, -707, -1000, -707}
local function tf_scout(f, mem, cx, cy, now)
    local bd, bx, by = nil, nil, nil
    for _, ed in ipairs(scan.ed) do
        local d = d2(ed.x, ed.y, cx, cy)
        if (bd == nil or d < bd) and not cleared(mem, now, ed.x, ed.y) then
            local wx, wy = safe_toward(cx, cy, ed.x, ed.y, f.ocean, 3)
            if wx ~= nil then
                if math.abs(cx - wx) < 5 and math.abs(cy - wy) < 5 then
                    clear(mem, now, ed.x, ed.y)
                else
                    bd, bx, by = d, wx, wy
                end
            end
        end
    end
    if bx ~= nil then
        return 1, bx, by, "the enemy's fishing grounds"
    end
    -- no enemy building known (fog): the unexplored sea furthest from the town (upstream's game scouts find the
    -- enemy; here the hunters look for it over the water)
    if scan.eb == nil and mem.town_x ~= nil then
        local ux, uy = ai.unexplored(cx, cy, {sea = true, grid = 8, away = {mem.town_x, mem.town_y}})
        if ux ~= nil then
            local tx, ty = safe_toward(cx, cy, ux, uy, f.ocean, 3)
            if tx ~= nil and (math.abs(cx - tx) >= 5 or math.abs(cy - ty) >= 5) then
                return 1, tx, ty, "looking for the enemy"
            end
        end
    end
    if now - (f.wp_t or 0) >= 90000 then
        f.wp = (f.wp or 0) + 1
        f.wp_t = now
    end
    for _ = 1, 8 do
        local qx, qy, why
        local wp = f.wp or 0
        if scan.eb ~= nil and wp % 2 == 0 then
            qx, qy, why = scan.eb.x, scan.eb.y, "the enemy's shore"
        else
            local a = wp % 8 + 1
            qx, qy, why = mem.gx + 28 * COS[a] // 1000, mem.gy + 28 * SIN[a] // 1000, "scouting"
        end
        local tx, ty = safe_toward(mem.gx, mem.gy, qx, qy, f.ocean, 3)
        if tx ~= nil and (math.abs(cx - tx) >= 5 or math.abs(cy - ty) >= 5) then
            return 1, tx, ty, why
        end
        f.wp = (f.wp or 0) + 1
        f.wp_t = now
    end
    return 0
end
-- the patrol: transports seen within 16 tiles, then warships there when not weaker (1.0; -1 back off), else the
-- sea lane's waypoints
local function tf_target_patrol(f, mem, cx, cy, now)
    local bd, bx, by = nil, nil, nil
    for _, e in ipairs(scan.ev) do
        local d = d2(e.x, e.y, cx, cy)
        if e.seen and d < 257 and (bd == nil or d < bd) and e.category == "transport" and not M.in_danger(e.x, e.y, 3) then
            bd, bx, by = d, e.x, e.y
        end
    end
    if bx ~= nil then
        return 1, bx, by, "enemy transports"
    end
    local loc = sel(scan.es, function(e) return e.seen and d2(e.x, e.y, cx, cy) <= 256 and not M.in_danger(e.x, e.y, 3) end)
    if #loc > 0 then
        if ratio_of(f.ships, loc) < 1.0 then
            return -1
        end
        return 1, cx, cy, "enemy warships on the lane", loc
    end
    if now - (f.wp_t or 0) >= 80000 then
        f.wp = (f.wp or 0) + 1
        f.wp_t = now
    end
    for _ = 1, 4 do
        local k = (f.wp or 0) % 4
        local qx, qy
        if scan.eb ~= nil then
            local dx, dy = scan.eb.x - mem.gx, scan.eb.y - mem.gy
            local len = math.max(isqrt(dx * dx + dy * dy), 1)
            local a = k == 0 and 35 or k == 2 and 65 or 50
            local side = k == 1 and 10 or k == 3 and -10 or 0
            qx, qy = mem.gx + dx * a // 100 - dy * side // len, mem.gy + dy * a // 100 + dx * side // len
        else
            local a = k * 2 + 1
            qx, qy = mem.gx + 15 * COS[a] // 1000, mem.gy + 15 * SIN[a] // 1000
        end
        local tx, ty = safe_toward(mem.gx, mem.gy, qx, qy, f.ocean, 3)
        if tx ~= nil and (math.abs(cx - tx) >= 4 or math.abs(cy - ty) >= 4) then
            return 2, tx, ty, "the sea lanes"
        end
        f.wp = (f.wp or 0) + 1
        f.wp_t = now
    end
    return 0
end
-- a force's decision (stage 4; the home guard: decide). Returns: the task changed
local function tf_decide(mem, kind, f, home_f, now, L, help, home, hsx, hsy, nh)
    local cx, cy = centre(f.ships, mem.gx, mem.gy)
    strength(f, cx, cy)
    local hp, hmax = hp_of(f.ships)
    local task, prev, r, prey, use = "guard", f.task, 12, false, nil
    local tx, ty = mem.gx, mem.gy
    local engaged = prev == "hunt" or prev == "raid" or prev == "patrol" or prev == "scout"
    local why = "nothing to do"
    if #f.ships == 0 then
        set_task(f, "guard", "no ships", now)
        return false
    end
    if kind == "escort" then
        if L ~= nil and L.phase >= 2 and L.ocean == f.ocean then
            task, why = "support", "landing under way"
            local sx, sy = safe_toward(cx, cy, L.water_x, L.water_y, f.ocean, 3)
            if sx ~= nil then
                tx, ty = sx, sy
            end
        elseif L ~= nil and L.phase == 1 and L.ocean == f.ocean then
            local mx, my = (mem.gx + L.water_x) // 2, (mem.gy + L.water_y) // 2
            local route = sel(scan.es, function(e) return d2(e.x, e.y, L.water_x, L.water_y) <= 225 or d2(e.x, e.y, mx, my) <= 225 end)
            task, r = "escort", 16
            local sx, sy
            if #route > 0 and ratio_of(f.ships, route) >= 1.0 then
                why, use = "clearing the landing's sea", route
                sx, sy = safe_toward(mem.gx, mem.gy, L.water_x, L.water_y, f.ocean, 3)
            else
                why = #route > 0 and "covering the landing's route - enemy too strong" or "covering the landing's route"
                sx, sy = safe_toward(mem.gx, mem.gy, mx, my, f.ocean, 3)
            end
            if sx ~= nil then
                tx, ty = sx, sy
            end
            if prev ~= "escort" then
                f.escorts = (f.escorts or 0) + 1
            end
        end
    elseif kind ~= "raid" and help and nh > 0 then
        local both = {}
        for _, u in ipairs(home_f.ships) do
            both[#both + 1] = u
        end
        for _, u in ipairs(f.ships) do
            both[#both + 1] = u
        end
        if ratio_of(both, home) >= 0.8 then
            task, why, use, tx, ty, r = "defend", "helping the home guard", home, hsx // nh, hsy // nh, 20
            if prev ~= "defend" then
                f.defends = (f.defends or 0) + 1
            end
        end
    end
    if task == "guard" and why == "nothing to do" and kind ~= "escort" then
        if engaged and (hp * 2 < (f.hp0 or 0) or (f.enear > 0 and f.near_ratio < 80)) then
            f.retreats = (f.retreats or 0) + 1
            f.retreat_t = now
            task, why = "retreat", hp * 2 < (f.hp0 or 0) and "half the hit points lost" or "outmatched near"
        elseif prev == "retreat" and now - (f.retreat_t or 0) < 45000 then
            task, why = "retreat", f.why
        elseif hp * 2 < hmax then
            task, why = "guard", "damaged - repairs"
            if S.fleet_cover then
                if (mem.rcover or 0) > 0 and mem.rx ~= nil then
                    tx, ty = mem.rx, mem.ry
                elseif mem.hx ~= nil then
                    tx, ty = mem.hx, mem.hy
                end
            end
        else
            local found, fx, fy, fwhy, fuse
            if kind == "hunt" then
                found, fx, fy, fwhy = tf_target_fleet(f, cx, cy)
            elseif kind == "raid" then
                found, fx, fy, fwhy = tf_target_prey(f, cx, cy)
            else
                found, fx, fy, fwhy, fuse = tf_target_patrol(f, mem, cx, cy, now)
            end
            if found < 0 then
                task, why = "guard", kind == "hunt" and "the enemy fleet stronger everywhere" or "outmatched on the lane"
            else
                local eng = kind == "hunt" and "hunt" or kind == "raid" and "raid" or "patrol"
                if found == 0 and kind ~= "patrol" then
                    found, fx, fy, fwhy = tf_scout(f, mem, cx, cy, now)
                    eng = "scout"
                elseif found == 2 then
                    eng = "patrol"
                elseif kind == "patrol" then
                    eng = "hunt"
                end
                if found > 0 then
                    if not engaged and kind ~= "patrol" and spread(f, cx, cy) and not (prev == "gather" and now - (f.task_t or 0) >= 45000) then
                        task, why = "gather", "gathering"
                        local sx, sy = safe_toward(mem.gx, mem.gy, cx, cy, f.ocean, 3)
                        tx, ty = sx or mem.gx, sy or mem.gy
                    else
                        task, why, tx, ty, r, prey, use = eng, fwhy, fx, fy, 14, true, fuse
                        if not engaged then
                            f.hunts = (f.hunts or 0) + 1
                            f.hp0 = hp
                        end
                    end
                end
            end
        end
    end
    if task == "retreat" then
        tx, ty = mem.gx, mem.gy
        if S.fleet_cover then
            if (mem.rcover or 0) > 0 and mem.rx ~= nil then
                tx, ty = mem.rx, mem.ry
            elseif mem.hx ~= nil then
                tx, ty = mem.hx, mem.hy
            end
        end
    end
    local changed = task ~= prev
    set_task(f, task, why, now)
    f.gox, f.goy = tx, ty
    orders(f, mem, use, tx, ty, r, prey, task == "retreat")
    return changed
end

-- the forces' sizes by the fleet's (tf_sizes): 3 or fewer stay one fleet; else an escort of a quarter while a
-- landing is on, of the rest a sixth home (1+), a fifth raiding (1+), a sixth on patrol (from 6), the others hunt
local TF = {"escort", "home", "hunt", "raid", "patrol"}
local function tf_sizes(n, land)
    local want = {escort = 0, home = 0, hunt = 0, raid = 0, patrol = 0}
    if n <= 3 then
        want.home = n
        return want
    end
    local e = land and math.max(n // 4, 1) or 0
    local rest = n - e
    local h, rd = math.max(rest // 6, 1), math.max(rest // 5, 1)
    local p = rest >= 6 and math.max(rest // 6, 1) or 0
    want.escort, want.home, want.raid, want.patrol, want.hunt = e, h, rd, p, rest - h - rd - p
    return want
end
-- how well a role fits a force (k_tf_fit: NONE, F, L, A, S, AA)
local FIT = {
    home = {[0] = 1, 1, 2, 2, 1, 3}, hunt = {[0] = 1, 2, 3, 2, 3, 1}, raid = {[0] = 1, 3, 1, 3, 1, 0},
    patrol = {[0] = 1, 3, 1, 1, 2, 0}, escort = {[0] = 1, 2, 2, 2, 1, 2},
}
local function fit(t, u)
    return FIT[t][M.role(u.type)]
end
-- each ship stays in its force; a force over its size (one more is kept) lets the least fitting go, a force under
-- it takes the best fitting free ship (else one from a force over its size); the rest hunt. mem.tfm: id -> force
local function tf_assign(mem, ships, want)
    mem.tfm = mem.tfm or {}
    local of, cnt = {}, {escort = 0, home = 0, hunt = 0, raid = 0, patrol = 0}
    local alive = {}
    for i, u in ipairs(ships) do
        local t = mem.tfm[u.id]
        of[i] = t
        alive[u.id] = true
        if t ~= nil then
            cnt[t] = cnt[t] + 1
        end
    end
    for _, t in ipairs(TF) do
        while cnt[t] > want[t] + (want[t] > 0 and 1 or 0) do
            local w = nil
            for i, u in ipairs(ships) do
                if of[i] == t and (w == nil or fit(t, u) < fit(t, ships[w])) then
                    w = i
                end
            end
            if w == nil then
                break
            end
            of[w] = nil
            cnt[t] = cnt[t] - 1
        end
    end
    for _, t in ipairs(TF) do
        while cnt[t] < want[t] do
            local b = nil
            for i, u in ipairs(ships) do
                if of[i] == nil and (b == nil or fit(t, u) > fit(t, ships[b])) then
                    b = i
                end
            end
            if b == nil then
                for i, u in ipairs(ships) do
                    if of[i] ~= nil and of[i] ~= t and cnt[of[i]] > want[of[i]] and (b == nil or fit(t, u) > fit(t, ships[b])) then
                        b = i
                    end
                end
            end
            if b == nil then
                break
            end
            if of[b] ~= nil then
                cnt[of[b]] = cnt[of[b]] - 1
            end
            of[b] = t
            cnt[t] = cnt[t] + 1
        end
    end
    local forces = {escort = {}, home = {}, hunt = {}, raid = {}, patrol = {}}
    for i, u in ipairs(ships) do
        if of[i] == nil then
            of[i] = want.hunt > 0 and "hunt" or "home"
        end
        local fl = forces[of[i]]
        fl[#fl + 1] = u
        mem.tfm[u.id] = of[i]
    end
    for _, id in ipairs(ai.keys(mem.tfm)) do
        if not alive[id] then
            mem.tfm[id] = nil
        end
    end
    return forces
end

local function tf_log(mem, k, f, now)
    local ro = {[0] = 0, 0, 0, 0, 0, 0}
    for _, u in ipairs(f.ships) do
        ro[M.role(u.type)] = ro[M.role(u.type)] + 1
    end
    local hp, hm = hp_of(f.ships)
    local cx, cy = centre(f.ships, mem.gx, mem.gy)
    print(string.format("fleet tf=%s task=%s why=%s ships=%d F%d L%d A%d S%d AA%d hp=%d%% at=%d,%d to=%d,%d ratio=%.2f known=%d near=%.2f/%d hunts=%d retreats=%d defends=%d orders=%d flees=%d",
        k, f.task or "-", f.why or "-", #f.ships, ro[1], ro[2], ro[3], ro[4], ro[5], hm > 0 and 100 * hp // hm or 0, cx, cy, f.gox or -1, f.goy or -1,
        (f.ratio or 0) / 100, f.eknown or 0, (f.near_ratio or 0) / 100, f.enear or 0, f.hunts or 0, f.retreats or 0, f.defends or 0,
        f.orders or 0, f.flees or 0))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (fleet_tick, every 2 s). L: the landing's state for the escort ({phase, ocean, water_x, water_y}), or nil.
-- exclude: ids the fleet leaves alone (the landing's escorts, the sea scout). Returns the scan (navy.lua reads the
-- docks and towers from it)

function M.step(view, mem, L, exclude)
    local now = view.time
    if mem.last ~= nil and now - mem.last < 2000 then
        return nil
    end
    mem.last = now
    local s = M.scan(view, mem, exclude)
    mem.town_x, mem.town_y = view.town_x, view.town_y
    mem.fl = mem.fl or {}
    local f = mem.fl
    f.ships = s.ships
    -- its sea: the docks' (the first with a sea next to it), else the first ship's
    if (mem.ocean or 0) == 0 then
        for _, dk in ipairs(s.docks) do
            local sx, sy, sea = ai.shore(dk.x, dk.y, {max = 4})
            if sea ~= nil and sea ~= 0 then
                mem.ocean = sea
                break
            end
        end
        if (mem.ocean or 0) == 0 and #s.ships > 0 then
            mem.ocean = sea_of(s.ships[1].x, s.ships[1].y)
        end
        if (mem.ocean or 0) == 0 then
            return s
        end
    end
    f.ocean = mem.ocean
    guard_point(mem)
    if mem.gx == nil then
        return s
    end
    local cx, cy = centre(f.ships, mem.gx, mem.gy)
    boats(mem, f.ships, cx, cy)
    -- stage 4: the haven, massing or the shelter, the task forces
    haven(mem)
    local rr = ratio_of(f.ships, s.es)
    f.ratio = math.floor(math.min(rr, 99) * 100)
    mem.ring = mem.ring or {}
    local target = mem.target or 0
    if S.navy_fleet_mass then
        rally(mem, now)
        if mem.rx == nil then
            mem.rx, mem.ry = mem.gx, mem.gy
        end
        local crit = math.max(target // 4, 4)
        if not mem.mass and not (#f.ships >= crit * 3 // 4 and rr >= 1.0) then
            mem.mass = true
            mem.masses = (mem.masses or 0) + 1
        elseif mem.mass and #f.ships >= crit and rr >= 1.25 then
            mem.mass = false
            mem.strikes = (mem.strikes or 0) + 1
            print(string.format("fleet strike ships=%d ratio=%.2f known=%d crit=%d", #f.ships, math.min(rr, 99), #s.es, crit))
        end
        mem.shelter = false
    else
        -- SHELTER (fleet in being): a much stronger enemy fleet known (ratio < 0.6, 3+ warships): all at the haven, out at 0.9
        local weak = #s.es >= 3 and rr < 0.6
        if not mem.shelter and weak and mem.hx ~= nil then
            mem.shelter = true
            mem.shelters = (mem.shelters or 0) + 1
        elseif mem.shelter and (rr >= 0.9 or #s.es < 3) then
            mem.shelter = false
        end
    end
    -- SPLIT only when clearly the stronger at sea (1.5 against every enemy warship known, 6+ ships; together under 1.1)
    local land = L ~= nil and L.phase >= 1 and L.ocean == f.ocean
    if not mem.split and #f.ships >= 6 and rr >= 1.5 then
        mem.split = true
    elseif mem.split and (#f.ships < 6 or rr < 1.1) then
        mem.split = false
    end
    if mem.mass then
        mem.split = false
    end
    local want = tf_sizes(#f.ships, land)
    if mem.shelter or mem.mass or not mem.split then
        want = {escort = 0, home = #f.ships, hunt = 0, raid = 0, patrol = 0}
    end
    local forces = tf_assign(mem, f.ships, want)
    mem.tf = mem.tf or {}
    local ch = {}
    for _, t in ipairs(TF) do
        mem.tf[t] = mem.tf[t] or {}
        mem.tf[t].ships = forces[t]
        mem.tf[t].ocean = f.ocean
    end
    local home_f = mem.tf.home
    local flags = {mass = mem.mass, home = mem.split and not mem.shelter, shelter = mem.shelter}
    if mem.shelter then
        flags.home = true
    end
    ch.home = decide(mem, home_f, now, flags, L)
    local home, nh, hsx, hsy = home_threats(mem)
    local help = nh > 0 and ratio_of(home_f.ships, home) < 1.0
    for _, t in ipairs({"hunt", "raid", "patrol", "escort"}) do
        ch[t] = tf_decide(mem, t, mem.tf[t], home_f, now, L, help, home, hsx, hsy, nh)
    end
    for _, t in ipairs(TF) do
        local q = mem.tf[t]
        if #q.ships > 0 and (ch[t] or now - (q.log_t or 0) >= 60000) then
            q.log_t = now
            tf_log(mem, t, q, now)
        end
        q.ships = nil   -- not saved: rebuilt every tick
    end
    if now - (mem.log_t or 0) >= 60000 then
        mem.log_t = now
        local hp, hm = hp_of(f.ships)
        print(string.format("fleet ships=%d hp=%d%% ratio=%.2f known=%d target=%d %s rally=%d,%d towers=%d guard=%d,%d haven=%d,%d danger=%d pulled=%d repaired=%d masses=%d strikes=%d forces=e%d/h%d/hu%d/r%d/p%d",
            #f.ships, hm > 0 and 100 * hp // hm or 0, math.min(rr, 99), #s.es, target, mem.mass and "massing" or mem.split and "split" or "one-fleet",
            mem.rx or -1, mem.ry or -1, mem.rcover or 0, mem.gx, mem.gy, mem.hx or -1, mem.hy or -1, #s.dz, mem.pulls or 0, mem.repaired or 0,
            mem.masses or 0, mem.strikes or 0, #forces.escort, #forces.home, #forces.hunt, #forces.raid, #forces.patrol))
    end
    f.ships = nil
    return s
end

-- the fleet's own warship orders need the scan's docks: the last scan
function M.last_scan()
    return scan
end

return M
