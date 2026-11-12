-- TEST ONLY: the air force's bombing run (ai/ageofai/air.lua, upstream airstrike.c). Needs EE_AI_TEST_SPAWN=1 and
-- the test folder as EE_AI_SCRIPTS (sim_bench --ai-scripts tests/ai_units --env EE_AI_TEST_SPAWN=1
-- --env EE_AI_NEXT_SCRIPT=airwing --engine "next,classic" --game-seconds 300). The engine's require reads
-- "ageofai.air" and "lib.*" from EE_AI_SCRIPTS first, then from the data tree's Data/ai: when the tree's copy of
-- ai/ is older than the checkout, run it from a folder that holds this script beside the checkout's ai/ modules
-- (tools: copy ai/* and this file into one scratch folder and pass that as --ai-scripts).
--
-- At 30 s the script puts an Airport and 4 Gothas of its own 6 tiles from its town, an enemy WWI tower and an
-- 88 mm AA gun 30 tiles away (towards the idle human's town; theirs), and a spotter (an observation balloon) 10
-- tiles short of them so they are seen. Then air.lua's step runs every second with the enemy position as "the
-- target found too strong" (memory.army.strong_*) and no epoch gate: with 4 bombers the wing waits while the AA
-- gun is seen (1 AA + 1 defence need 8: `air wait`); at 15 s 4 more Gothas appear and the wing launches when
-- enough circle idle at the field (`air wing`: a flight at each anti-air seen, the rest at the tower). `airwing.*`
-- lines report every 15 s: the bombers by action, the targets' hit points, the module's counters (waves, sent,
-- waits, gave_up, refused: ai.attack on a bomber landed in its airport is the interface gap the module's header
-- names), and the fight estimate of stats.lua for the bombers against the two. Measured 2026-11-09 (seed 2): a wing
-- of 4 at the tower at 31 s (the AA gun was not seen then: needed 4), the tower 4875 -> 75 hit points, the bombers
-- landed and every later order refused (cannot_do, 164 in all), the target given up after AF_RUN_MS.
local AIR = require("ageofai.air")
local ST = require("ageofai.stats")

local BOMBER, AIRPORT, SPOTTER = "a_bomber_10_gotha", "b_airport", "a_balloon10_observation_balloon"
local TOWER, AA = "b_guard_tower_ww1", "b_aa10_gun_88mm"
local ENEMY = 1

local P = {}

local function towards(x0, y0, x1, y1, n)
    local dx, dy = x1 - x0, y1 - y0
    local len = math.sqrt(dx * dx + dy * dy)
    if len < 1 then
        return x0 + n, y0
    end
    return x0 + math.floor(dx * n / len + 0.5), y0 + math.floor(dy * n / len + 0.5)
end

local function report(view, mem, why)
    local by = {}
    local ids = ai.units{category = "air"}
    local n = 0
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and u.type == BOMBER then
            n = n + 1
            local a = u.action or "?"
            by[a] = (by[a] or 0) + 1
        end
    end
    local parts = {}
    for _, k in ipairs(ai.keys(by)) do
        parts[#parts + 1] = k .. "=" .. by[k]
    end
    local tower, aa = ai.unit(mem.tower or 0), ai.unit(mem.aa or 0)
    local s = mem.air or {}
    local foes = {}
    if tower ~= nil then
        foes[#foes + 1] = tower.id
    end
    if aa ~= nil then
        foes[#foes + 1] = aa.id
    end
    local own = {}
    for _, id in ipairs(ids) do
        local u = ai.unit(id)
        if u ~= nil and u.type == BOMBER then
            own[#own + 1] = id
        end
    end
    local ratio = (#foes > 0 and #own > 0) and ST.ratio(own, foes) or -1
    print(string.format("airwing.%s t=%d bombers=%d [%s] tower=%s aa=%s waves=%d sent=%d waits=%d gave_up=%d refused=%d/%s ratio=%.2f",
        why, view.time - (mem.setup or 0), n, table.concat(parts, " "), tower ~= nil and tostring(tower.hp) or "unseen",
        aa ~= nil and tostring(aa.hp) or "unseen", s.waves or 0, s.sent or 0, s.waits or 0, s.gave_up or 0, s.refused or 0,
        tostring(s.refused_reason), ratio))
end

function P.step(view, mem)
    if mem.setup == nil then
        if view.time >= 30000 then
            local ex, ey = ai.test_town(ENEMY)
            if ex == nil or view.town_x < 0 then
                mem.setup = -1
                return {}
            end
            local tx, ty = view.town_x, view.town_y
            local ax, ay = towards(tx, ty, ex, ey, 6)
            local px, py = towards(tx, ty, ex, ey, 30)
            local sx, sy = towards(tx, ty, ex, ey, 20)
            local ok_ap = ai.test_spawn(AIRPORT, ax, ay)
            local ok_b = 0
            for k = 0, 3 do
                if ai.test_spawn(BOMBER, ax + k % 2 + 4, ay + k // 2 + 4) then
                    ok_b = ok_b + 1
                end
            end
            local ok_t = ai.test_spawn(TOWER, px, py, ENEMY)
            local ok_aa = ai.test_spawn(AA, px + 4, py, ENEMY)
            local ok_s = ai.test_spawn(SPOTTER, sx, sy)
            mem.setup, mem.px, mem.py = view.time, px, py
            mem.ax, mem.ay = ax, ay
            print(string.format("airwing.setup t=%d airport=%s@%d,%d bombers=%d tower=%s aa=%s@%d,%d spotter=%s@%d,%d", view.time,
                tostring(ok_ap), ax, ay, ok_b, tostring(ok_t), tostring(ok_aa), px, py, tostring(ok_s), sx, sy))
        end
        return {}
    end
    if mem.setup < 0 or mem.state == "done" then
        return {}
    end
    local t = view.time - mem.setup
    -- the targets' ids once seen (the spotter's sight)
    if mem.tower == nil or mem.aa == nil then
        for _, id in ipairs(ai.seen{player = ENEMY, near = {mem.px, mem.py, 8}}) do
            local u = ai.unit(id)
            if u ~= nil and u.type == TOWER then
                mem.tower = id
            elseif u ~= nil and u.type == AA then
                mem.aa = id
            end
        end
    end
    -- the spawned units are the engine's AI's until taken: the spotter is taken and stopped (it would explore), the
    -- bombers not yet sent at a target are stopped once (the engine's air subgroup sent them off on a patrol before
    -- the module's first look; stopped, they circle idle at the field: ready)
    if t >= 1000 and not mem.spotter_held then
        mem.spotter_held = true
        local sp = ai.units{type = SPOTTER}
        ai.take(sp)
        ai.stop(sp)
    end
    if t >= 15000 and not mem.more then
        mem.more = true
        local n = 0
        for k = 0, 3 do
            if ai.test_spawn(BOMBER, mem.ax + k % 2 + 4, mem.ay + k // 2 + 7) then
                n = n + 1
            end
        end
        print(string.format("airwing.more t=%d bombers=%d", t, n))
    end
    if t < 60000 and mem.air ~= nil then
        mem.stopped = mem.stopped or {}
        local stop = {}
        for _, e in ipairs(mem.air.held or {}) do
            local u = ai.unit(e.id)
            if u ~= nil and e.tgt == 0 and u.action == "move" and not mem.stopped[e.id] then
                mem.stopped[e.id] = true
                stop[#stop + 1] = e.id
            end
        end
        if #stop > 0 then
            ai.stop(stop)
            print(string.format("airwing.stop t=%d n=%d", t, #stop))
        end
    end
    -- the position: the target found too strong, now
    mem.army = mem.army or {}
    mem.army.strong_x, mem.army.strong_y, mem.army.strong_t = mem.px, mem.py, view.time
    local orders = {}
    local budget = {civil = {}}
    for _, r in ipairs({"food", "wood", "stone", "gold", "iron"}) do
        budget.civil[r] = view.stock[r]
    end
    AIR.step(view, mem, orders, budget, ai.events(), {any_epoch = true})
    if t >= 2000 and (mem.report_t == nil or view.time - mem.report_t >= 15000) then
        mem.report_t = view.time
        report(view, mem, "state")
    end
    if t >= 260000 then
        report(view, mem, "result")
        mem.state = "done"
    end
    return orders
end

return P
