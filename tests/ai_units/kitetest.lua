-- TEST ONLY: archers against melee, with and without kiting (docs/AI_LUA_UNITS.md §8, L2/L3). Needs EE_AI_TEST_SPAWN=1
-- (ai.test_spawn, ai.test_town) and the test folder as EE_AI_SCRIPTS (sim_bench --ai-scripts tests/ai_units).
--
-- At 60 s the script puts 8 bowmen of its own 18 tiles from its town towards the idle human's town, and 8 clubmen
-- of the human's 10 tiles further on. The bowmen attack the nearest clubman they see. With kiting, the micro step
-- (every 250 game ms) walks a bowman 4 tiles straight away from any clubman within 3 tiles, and it shoots again
-- when the clubman is further than that. Every 10 s up to 2 minutes a `kite.result` line goes to the trace.
local M = {}

local ARCHER, MELEE = "arch03_simple_bowman", "inf01_clubman"
local ENEMY = 1   -- the idle human

local function dist2(a, b)
    local dx, dy = a.px - b.px, a.py - b.py
    return dx * dx + dy * dy
end

-- the nearest seen clubman to a unit record, and its squared distance (centi-tiles)
local function nearest(u, r)
    local best, bd
    for _, id in ipairs(ai.seen{player = ENEMY, type = MELEE, near = {u.x, u.y, r}}) do
        local e = ai.unit(id)
        if e ~= nil then
            local d = dist2(u, e)
            if bd == nil or d < bd then
                best, bd = e, d
            end
        end
    end
    return best, bd
end

local function setup(view, mem)
    local tx, ty = view.town_x, view.town_y
    local ex, ey = ai.test_town(ENEMY)
    if tx < 0 or ex == nil then
        if not mem.said then
            mem.said = true
            print(string.format("kite.wait town=%d,%d enemy=%s", tx, ty, tostring(ex)))
        end
        return
    end
    local dx, dy = ex - tx, ey - ty
    local len = math.sqrt(dx * dx + dy * dy)
    local home = ai.tile(tx, ty)
    for _, d in ipairs({18, 15, 12, 10}) do
        local bx, by = tx + math.floor(dx * d / len), ty + math.floor(dy * d / len)
        local cx, cy = tx + math.floor(dx * (d + 10) / len), ty + math.floor(dy * (d + 10) / len)
        local b, c = ai.tile(bx, by), ai.tile(cx, cy)
        -- (unexplored tiles show no continent: the test takes them on trust)
        if b ~= nil and c ~= nil and (b.continent == nil or b.continent == home.continent)
            and (c.continent == nil or c.continent == home.continent) then
            for k = 0, 7 do
                ai.test_spawn(ARCHER, bx + k % 4 - 1, by + k // 4)
                ai.test_spawn(MELEE, cx + k % 4 - 1, cy + k // 4, ENEMY)
            end
            mem.setup = view.time
            mem.cx, mem.cy = cx, cy
            print(string.format("kite.setup t=%d kite=%s at=%d,%d enemy=%d,%d", view.time, tostring(mem.kite), bx, by, cx, cy))
            return
        end
    end
    mem.setup = -1
    print("kite.setup failed: no open ground towards the enemy")
end

local function report(view, mem)
    local archers, hp = 0, 0
    for _, id in ipairs(ai.units{type = ARCHER}) do
        archers = archers + 1
        hp = hp + ai.unit(id).hp
    end
    local melee, mhp = 0, 0
    for _, id in ipairs(ai.seen{player = ENEMY, type = MELEE}) do
        melee = melee + 1
        mhp = mhp + ai.unit(id).hp
    end
    print(string.format("kite.result t=%d kite=%s archers=%d hp=%d clubmen_seen=%d clubmen_hp=%d", view.time - mem.setup,
        tostring(mem.kite), archers, hp, melee, mhp))
end

function M.make(kite)
    local P = {}

    function P.step(view, mem)
        mem.kite = kite
        if mem.setup == nil then
            if view.time >= 60000 then
                setup(view, mem)
            end
            return {}
        end
        if mem.setup < 0 or view.time > mem.setup + 125000 then
            return {}
        end
        if view.time >= mem.setup + 2000 then
            ai.reflex{danger = false, naval_flee = false}
            for _, id in ipairs(ai.units{type = ARCHER}) do
                local u = ai.unit(id)
                local fleeing = (mem.flee or {})[id]
                if u.action ~= "attack" and (fleeing == nil or fleeing <= view.time) then
                    local e = nearest(u, 20)
                    if e ~= nil then
                        ai.attack(id, e.id)
                    elseif u.action == "idle" then
                        ai.move(id, mem.cx, mem.cy)   -- towards where the clubmen were put
                    end
                end
            end
        end
        local t = view.time - mem.setup
        if t >= 10000 and (mem.reported or 0) + 10000 <= t then
            mem.reported = t - t % 10000
            report(view, mem)
        end
        return {}
    end

    if kite then
        function P.micro(info, mem)
            if mem.setup == nil or mem.setup < 0 or info.time < mem.setup + 2000 or info.time > mem.setup + 125000 then
                return nil
            end
            mem.flee = mem.flee or {}
            for _, id in ipairs(ai.units{type = ARCHER}) do
                local u = ai.unit(id)
                local e, d = nearest(u, 5)
                if e ~= nil and d < 300 * 300 then
                    local dx, dy = u.px - e.px, u.py - e.py
                    local len = math.sqrt(dx * dx + dy * dy)
                    if len < 1 then
                        dx, dy, len = 100, 0, 100
                    end
                    local x = math.floor((u.px + dx * 400 / len) / 100)
                    local y = math.floor((u.py + dy * 400 / len) / 100)
                    if ai.tile(x, y) ~= nil then
                        ai.move(id, x, y, {forced = true})
                        mem.flee[id] = info.time + 1500
                    end
                elseif (mem.flee[id] or 0) ~= 0 and mem.flee[id] <= info.time then
                    mem.flee[id] = nil
                    local t = nearest(u, 12)
                    if t ~= nil then
                        ai.attack(id, t.id)
                    end
                end
            end
            return nil
        end
    end

    return P
end

return M
