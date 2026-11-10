-- Age of AI port: THE CITIZENS' SAFETY (attack.c citizens_safety, DAMAGE MEMORY, SEEN HURTS; GPL-3.0, see UPSTREAM).
-- Every 4 s (the micro step): a citizen hit (not by an animal) marks its spot for 2 minutes - 20 s when an enemy
-- soldier was in sight within 6 tiles (hurt_seen_short) - and a site within 6 tiles of such spots is dangerous:
-- food and wood from one spot, stone, gold and iron from three. A citizen gathering at a raided site (enemy
-- soldiers seen within 6) or a hurt one is sent back to the nearest drop site unless the town holds there
-- (raid_held: its soldiers and towers within 8 beat the raiders by raid_held 150%); the claimed site is released
-- while the danger lasts, so the engine's finders send nobody there (upstream's searches refuse such sites). While
-- the base is overrun (a muster on, army.lua): every citizen with raiders within 8 of it flees to the rally point,
-- or 12 tiles in the clear direction nearest it when the way is raided (flee_point).
-- This engine's own danger zones (a hit citizen's spot for 30 s, EECPRMResourceInDanger) stay on underneath.
local S = require("ageofai.settings")
local G = require("lib.groups")

local M = {}

local HURT_N = 64
local HURT_MS = 120000
local HURT_SEEN_MS = 20000
local EVERY = 4000

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

-- the raiders this step: the enemy soldiers seen within 70 of the town, read once (the micro budget)
local RAIDERS = {n = 0, x = {}, y = {}, id = {}}

local function raiders_scan(hx, hy)
    local ids = ai.seen{enemy = true, armed = true, category = "soldier", near = {hx, hy, 70}}
    RAIDERS.n, RAIDERS.id = #ids, ids
    if #ids > 0 then
        local cols = ai.get(ids, {"x", "y"})
        RAIDERS.x, RAIDERS.y = cols.x, cols.y
    else
        RAIDERS.x, RAIDERS.y = {}, {}
    end
end

-- the raiders within r of x,y (their ids), from the step's list
local function raiders_near(x, y, r)
    local out = {}
    for k = 1, RAIDERS.n do
        local rx, ry = RAIDERS.x[k], RAIDERS.y[k]
        if rx ~= nil and (rx - x) * (rx - x) + (ry - y) * (ry - y) <= r * r then
            out[#out + 1] = RAIDERS.id[k]
        end
    end
    return out
end

-- a spot still counting
local function live(h, now)
    return h.t > 0 and now >= h.t and now - h.t <= (h.seen and S.hurt_seen_short and HURT_SEEN_MS or HURT_MS)
end

-- citizens hurt within 6 tiles in the last 2 minutes
local function hurt_near(mem, x, y, now)
    local n = 0
    for _, h in ipairs(mem.hurt or {}) do
        if live(h, now) and math.abs(h.x - x) <= 6 and math.abs(h.y - y) <= 6 then
            n = n + 1
        end
    end
    return n
end

-- the hits of the step's events mark their spots (hurt_note: a loss of hit points; hunters' animals excepted)
function M.note(mem, events, now)
    if not S.citizen_threats then
        return
    end
    mem.hurt = mem.hurt or {}
    mem.hurt_at = mem.hurt_at or 0
    local looked = 0
    for _, e in ipairs(events) do
        if e.kind == "attacked" and looked < 8 then
            looked = looked + 1
            local v = ai.unit(e.unit)
            if v ~= nil and v.category == "citizen" then
                local a = e.attacker ~= nil and ai.unit(e.attacker) or nil
                if a == nil or a.category ~= "animal" then
                    local seen = #ai.seen{enemy = true, armed = true, category = "soldier", near = {v.x, v.y, 6}, limit = 1} > 0
                    local k = mem.hurt_at % HURT_N + 1
                    mem.hurt[k] = {x = v.x, y = v.y, t = now > 0 and now or 1, seen = seen}
                    mem.hurt_at = mem.hurt_at + 1
                    mem.hurt_count = (mem.hurt_count or 0) + 1
                end
            end
        end
    end
end

-- the town holds there: own soldiers and towers within 8 beat the raiders within 8 by raid_held
local function raid_held(x, y)
    local foes = raiders_near(x, y, 8)
    if #foes == 0 then
        return true
    end
    local own = ai.units{armed = true, near = {x, y, 8}, limit = 40}
    local mine = #own > 0 and ai.strength(own) or 0
    return mine * 100 >= ai.strength(foes) * S.raid_held
end

-- the way from (x0,y0) to (x1,y1) raided: enemy soldiers seen within 4 of a point every 4 tiles, or within 6 of the end
local function path_raided(x0, y0, x1, y1)
    local dx, dy = x1 - x0, y1 - y0
    local l = math.max(math.abs(dx), math.abs(dy))
    local t = 5
    while t < l do
        if #raiders_near(x0 + dx * t // l, y0 + dy * t // l, 4) > 0 then
            return true
        end
        t = t + 4
    end
    return #raiders_near(x1, y1, 6) > 0
end

local DIRS = {{12, 0}, {8, 8}, {0, 12}, {-8, 8}, {-12, 0}, {-8, -8}, {0, -12}, {8, -8}}

-- where a citizen at x,y flees while the base is overrun: the rally point if the way is clear, else 12 tiles in the
-- clear direction nearest the rally point; none clear: the rally point after all
local function flee_point(x, y, rx, ry, map)
    if not path_raided(x, y, rx, ry) then
        return rx, ry
    end
    local best, fx, fy = nil, rx, ry
    for _, d in ipairs(DIRS) do
        local px, py = x + d[1], y + d[2]
        px = math.max(3, math.min(px, map.width - 4))
        py = math.max(3, math.min(py, map.height - 4))
        local dd = d2(px, py, rx, ry)
        if (best == nil or dd < best) and not path_raided(x, y, px, py) then
            best, fx, fy = dd, px, py
        end
    end
    return fx, fy
end

-- the micro step: every 250 ms a sixteenth of the citizens (each looked at every 4 s, as upstream's tick; the
-- whole town at once blew the micro step's instruction budget at 100+ citizens). memory: the script's memory (the
-- muster's rally group, memory.army.mustering; the claimed sites released through memory.eco.site_danger)
local SPOTS = {}

function M.step(mem, memory, now)
    local army, eco = memory.army, memory.eco
    if not S.avoid_towers_citizens then
        return
    end
    mem.sent_back = mem.sent_back or {}
    if now - (mem.scan_t or 0) >= 1000 or (mem.scan_t or 0) == 0 then
        mem.scan_t = now
        local town = ai.units{type = "b_town_center", built = true, limit = 1}
        local hx, hy = 0, 0
        if #town > 0 then
            local t = ai.unit(town[1])
            if t ~= nil then
                hx, hy = t.x, t.y
            end
        end
        raiders_scan(hx, hy)
        SPOTS = {}
        for _, h in ipairs(mem.hurt or {}) do
            if live(h, now) then
                SPOTS[#SPOTS + 1] = h
            end
        end
        -- those sent back or fled earlier are released once their time is up
        local release = {}
        for _, id in ipairs(ai.keys(mem.sent_back)) do
            if mem.sent_back[id] <= now or ai.unit(id) == nil then
                if ai.unit(id) ~= nil then
                    release[#release + 1] = id
                end
                mem.sent_back[id] = nil
            end
        end
        if #release > 0 then
            ai.release(release)
        end
    end
    if RAIDERS.n == 0 and #SPOTS == 0 then
        return
    end
    local spots = SPOTS
    local sent, fled, held = 0, 0, 0
    local rally = army ~= nil and army.mustering and G.role(memory, "rally") or nil
    local all = ai.units{category = "citizen", held = "engine"}
    if #all == 0 then
        return
    end
    -- this tick's slice
    local per = math.max(1, (#all + 15) // 16)
    local cursor = (mem.cursor or 0) % #all
    local citizens = {}
    for k = 1, per do
        citizens[k] = all[(cursor + k - 1) % #all + 1]
    end
    mem.cursor = (cursor + per) % #all
    local cols = ai.get(citizens, {"x", "y", "action", "tx", "ty", "inside"})
    local map = rally ~= nil and ai.map() or nil
    for k = 1, #citizens do
        local id, x, y = citizens[k], cols.x[k], cols.y[k]
        if x ~= nil and not cols.inside[k] and mem.sent_back[id] == nil then
            if rally ~= nil then
                -- the base is overrun: citizens near the enemy (or working near it) flee to the rally point
                if d2(x, y, rally.x, rally.y) > 36 then
                    local danger = #raiders_near(x, y, 8) > 0
                    if not danger and cols.action[k] == "gather" and cols.tx[k] ~= nil then
                        danger = #raiders_near(cols.tx[k], cols.ty[k], 8) > 0
                    end
                    if danger then
                        local fx, fy = flee_point(x, y, rally.x, rally.y, map)
                        ai.move({id}, fx, fy, {forced = true})
                        mem.sent_back[id] = now + 30000
                        fled = fled + 1
                    end
                end
            elseif cols.action[k] == "gather" then
                -- the site: where it gathers (its goal's tile; the citizen's own spot when none)
                local sx, sy = cols.tx[k] or x, cols.ty[k] or y
                local raided = #raiders_near(sx, sy, 6) > 0
                local nh = 0
                for _, h in ipairs(spots) do
                    if math.abs(h.x - sx) <= 6 and math.abs(h.y - sy) <= 6 then
                        nh = nh + 1
                    end
                end
                local mineral = false
                if not raided and nh >= 1 then
                    -- the site's resource only when a hurt spot is near (the budget): minerals take three spots
                    local site = ai.unit(ai.units{category = "resource", near = {sx, sy, 2}, limit = 1}[1] or -1)
                    if site ~= nil and site.kind ~= nil then
                        mineral = site.kind == "stone" or site.kind == "gold" or site.kind == "iron"
                    end
                end
                local hurt = not raided and nh >= (mineral and 3 or 1)
                if (raided or hurt) then
                    if raid_held(sx, sy) then
                        held = held + 1
                    else
                        local did, dd, dx, dy = ai.drop_site(sx, sy, mineral and "stone" or "food")
                        if did ~= nil and dx ~= nil and not (math.abs(dx - sx) <= 3 and math.abs(dy - sy) <= 3) then
                            ai.move({id}, dx, dy, {forced = true})
                            mem.sent_back[id] = now + 8000
                            sent = sent + 1
                        end
                        -- the claimed site there is released while the danger lasts (the finders send nobody)
                        if eco ~= nil then
                            eco.site_danger = eco.site_danger or {}
                            eco.site_danger[tostring(sx) .. "," .. tostring(sy)] = now + (raided and 30000 or HURT_MS)
                        end
                    end
                end
            end
        end
    end
    mem.sent = (mem.sent or 0) + sent + fled
    mem.held = (mem.held or 0) + held
    if (sent + fled > 0) and now - (mem.log_t or 0) >= 60000 then
        mem.log_t = now
        print(string.format("safety sent=%d fled=%d held=%d hurt=%d muster=%s", sent, fled, held, mem.hurt_count or 0, tostring(rally ~= nil)))
    end
end

-- whether a site at x,y is dangerous by the safety's book (econ.lua's claims: released, not claimed again)
function M.site_dangerous(eco, x, y, now)
    local d = eco ~= nil and eco.site_danger or nil
    if d == nil then
        return false
    end
    for _, key in ipairs(ai.keys(d)) do
        if d[key] <= now then
            d[key] = nil
        else
            local sx, sy = key:match("^(-?%d+),(-?%d+)$")
            if sx ~= nil and math.abs(tonumber(sx) - x) <= 6 and math.abs(tonumber(sy) - y) <= 6 then
                return true
            end
        end
    end
    return false
end

return M
