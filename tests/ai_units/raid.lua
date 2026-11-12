-- TEST ONLY: a raid that retreats (docs/AI_LUA_UNITS.md §8, L2). Needs EE_AI_TEST_SPAWN=1 and the test folder as
-- EE_AI_SCRIPTS (sim_bench --ai-scripts tests/ai_units).
--
-- At 60 s the script puts 6 clubmen of its own 14 tiles from the idle human's town (on its own side) and sends them
-- at the human's citizens. 20 s later 10 spearmen of the human's appear at its town. When the soldiers seen near the
-- raiders outweigh them (hit points x attack), the raiders run home (a forced move): the step checks every second,
-- the micro step every 250 ms. `raid.*` lines go to the trace: the retreat, then the result when the raiders are home
-- or at 2.5 minutes.
local RAIDER, DEFENDER = "inf01_clubman", "inf02_spearman"
local ENEMY = 1

local P = {}

local function alive(mem)
    local ids = {}
    for _, id in ipairs(mem.raiders or {}) do
        if ai.unit(id) ~= nil then
            ids[#ids + 1] = id
        end
    end
    return ids
end

-- the raiders' centre tile
local function centre(ids)
    local sx, sy = 0, 0
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        sx, sy = sx + u.x, sy + u.y
    end
    return sx // #ids, sy // #ids
end

local function check_retreat(time, mem)
    if mem.state ~= "raid" then
        return
    end
    local ids = alive(mem)
    if #ids == 0 then
        return
    end
    local cx, cy = centre(ids)
    local foes = ai.seen{player = ENEMY, armed = true, category = "soldier", near = {cx, cy, 10}}
    if #foes > 0 and ai.strength(foes) * 10 > ai.strength(ids) * 8 then
        ai.move(ids, mem.hx, mem.hy, {forced = true})
        mem.state = "retreat"
        mem.retreat_at = time
        mem.at_retreat = #ids
        print(string.format("raid.retreat t=%d alive=%d foes=%d", time - mem.setup, #ids, #foes))
    end
end

function P.step(view, mem)
    if mem.setup == nil then
        if view.time >= 60000 then
            local ex, ey = ai.test_town(ENEMY)
            if ex == nil or view.town_x < 0 then
                mem.setup = -1
                return {}
            end
            local dx, dy = view.town_x - ex, view.town_y - ey
            local len = math.sqrt(dx * dx + dy * dy)
            local rx, ry = ex + math.floor(dx * 14 / len), ey + math.floor(dy * 14 / len)
            for k = 0, 5 do
                ai.test_spawn(RAIDER, rx + k % 3 - 1, ry + k // 3)
            end
            mem.setup, mem.ex, mem.ey, mem.hx, mem.hy = view.time, ex, ey, view.town_x, view.town_y
            mem.state = "spawned"
            print(string.format("raid.setup t=%d at=%d,%d enemy_town=%d,%d", view.time, rx, ry, ex, ey))
        end
        return {}
    end
    if mem.setup < 0 or mem.state == "done" then
        return {}
    end
    local t = view.time - mem.setup
    if mem.state == "spawned" and t >= 2000 then
        mem.raiders = ai.units{type = RAIDER}
        ai.reflex{danger = false}
        ai.take(mem.raiders)
        mem.state = "raid"
        print(string.format("raid.start raiders=%d", #mem.raiders))
    end
    if mem.state == "raid" and not mem.defenders and t >= 20000 then
        for k = 0, 9 do
            ai.test_spawn(DEFENDER, mem.ex + k % 5 - 2, mem.ey + 3 + k // 5, ENEMY)
        end
        mem.defenders = true
    end
    check_retreat(view.time, mem)
    local ids = alive(mem)
    if mem.state == "raid" then
        for _, id in ipairs(ids) do
            local u = ai.unit(id)
            if u.action ~= "attack" then
                local c = ai.seen{player = ENEMY, category = "citizen", near = {u.x, u.y, 25}, limit = 1}
                if #c > 0 then
                    ai.attack(id, c[1])
                elseif u.action == "idle" then
                    ai.move(id, mem.ex, mem.ey)
                end
            end
        end
    end
    local home = false
    if mem.state == "retreat" and #ids > 0 then
        local cx, cy = centre(ids)
        home = (cx - mem.hx) * (cx - mem.hx) + (cy - mem.hy) * (cy - mem.hy) <= 15 * 15
    end
    if home or t >= 150000 or (#ids == 0 and mem.state ~= "spawned") then
        print(string.format("raid.result t=%d state=%s started=%d at_retreat=%d home=%d", t, mem.state,
            #(mem.raiders or {}), mem.at_retreat or 0, #ids))
        mem.state = "done"
    end
    return {}
end

function P.micro(info, mem)
    if mem.setup ~= nil and mem.setup >= 0 then
        check_retreat(info.time, mem)
    end
    return nil
end

return P
