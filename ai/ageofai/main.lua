-- Age of AI: a Lua port of Grille's "Empire Earth: Age of AI" (GPL-3.0; UPSTREAM in this folder names the upstream
-- commit, the per-file map and the licence; docs/AI_RESEARCH.md has the bench). require("ageofai.main").
--
-- The economy pays what a human pays and sees what a human sees: a budget with an epoch reserve and a military
-- account, worker targets by shortfall and the measured rates, towers by the town centre first and at the fronts
-- later, drop sites before gathering, the gathering upgrades; the land army is one force ordered as one through the
-- per-unit interface (ageofai.army); the navy, landings and other islands (ageofai.navy, fleet, towns); the
-- diplomacy (ageofai.diplo: war answered, opinions, peace, blocks, vassals, capitulation, surrender).
local E = require("ageofai.econ")
local A = require("ageofai.army")
local T = require("ageofai.tactics")
local TW = require("ageofai.towns")   -- [towns.c, expand.c]
local DP = require("ageofai.diplo")   -- [diplo.c]
local AIR = require("ageofai.air")   -- [airstrike.c]
local W = require("ageofai.walls")   -- [defence.c walls, enclose]
local SF = require("ageofai.safety")   -- [attack.c citizens_safety]
local FR = require("ageofai.fronts")   -- [fronts.c]
local MT = require("ageofai.metal")   -- [metal.c]
local V = require("lib.view")
local S = require("ageofai.settings")

local M = {}

function M.init(view, memory)
    memory.started = view.time
    if S.citizen_militia and ai.reflex ~= nil then
        ai.reflex{militia = false}   -- [defence.c citizen_militia] the script's militia instead of the engine's reflex
    end
    print(string.format("ageofai init epoch=%d cap=%d town=%d,%d", view.epoch, view.population_cap, view.town_x, view.town_y))
    return {}
end

function M.step(view, memory)
    local orders = {}
    -- the army's plan from the last step tells the economy what the training wants (the workers gather for it)
    -- the events: the micro step takes them every 250 ms and keeps the attacks for the step (memory.hits)
    local events = memory.hits or {}
    for _, e in ipairs(ai.events()) do
        events[#events + 1] = e
    end
    memory.hits = {}
    memory.epoch_now = view.epoch
    local budget = E.step(view, memory, orders, memory.army_plan)
    T.garrison(view, memory.eco, function(o) orders[#orders + 1] = o end, budget.citizens_wanted)
    -- [priority.c] a building sees the stock the list leaves a building (budget.build); the fortress asks the list
    T.bunkers(view, memory.army, function(o) orders[#orders + 1] = o end, budget.build, memory)
    -- [fronts.c] the territory, the border and the fronts every 10 s; station 1 is the force's home (army.lua)
    FR.step(view, memory)
    -- [metal.c] the 5-minute line of the metal plan (what it picked, cut and freed)
    MT.tick(view, memory)
    -- [towns.c, expand.c] the towns on the home continent and the outposts on other islands (ageofai.towns)
    TW.step(view, memory, orders, budget)
    -- [defence.c walls, enclose] the front walls and their gates, the ring round the town or the island's coast
    -- (ageofai.walls; econ.lua reads the stone it owes and the ring's tower spots from memory.eco)
    W.step(view, memory)
    memory.army_plan = A.step(view, memory, orders, budget, events) or memory.army_plan
    -- [diplo.c] the diplomacy (ageofai.diplo): every step (the stances and the pending answers), the blocks every
    -- 2 s, the world scan every 10 s, the initiative every 60 s from minute 10; the events for the hits and losses
    DP.step(view, memory, events)
    -- [airstrike.c] the air force: the shooters' spots from the step's attacked events, the bombers' wing every 5 s
    AIR.step(view, memory, orders, budget, events)
    if view.time % 60000 < 1000 then
        local t = budget.target or {}
        print(string.format("ageofai e=%d saving=%s cit=%d/%d army=%d/%d towers=%d want=%d/%d/%d/%d/%d stock=%d/%d/%d/%d/%d inc=%d/%d/%d/%d/%d next=%d/%d/%d/%d/%d sol=%d",
            view.epoch, tostring(budget.saving), view.citizens, budget.citizens_wanted or 0, view.army_count, view.army_strength,
            E.tower_count(view), t.food or 0, t.wood or 0, t.stone or 0, t.gold or 0, t.iron or 0, view.stock.food, view.stock.wood,
            view.stock.stone, view.stock.gold, view.stock.iron, view.income.food, view.income.wood, view.income.stone,
            view.income.gold, view.income.iron, view.next_epoch_cost.food, view.next_epoch_cost.wood,
            view.next_epoch_cost.stone, view.next_epoch_cost.gold, view.next_epoch_cost.iron, view.sol_missing))
    end
    return orders
end

-- every 250 game ms: the counter-fire on the attacks since the last call; the attacked events are kept for the step
function M.micro(info, memory)
    memory.hits = memory.hits or {}
    local events = ai.events()
    for _, e in ipairs(events) do
        if e.kind == "attacked" then
            memory.hits[#memory.hits + 1] = e
        end
    end
    memory.army = memory.army or {}
    -- [attack.c citizens_safety] the hurt spots from the hits, the citizens sent back or fled every 4 s
    memory.safety = memory.safety or {}
    SF.note(memory.safety, events, info.time)
    SF.step(memory.safety, memory, info.time)
    T.counterfire(memory.army, events, info.time)
    T.militia(memory.army, events, info.time, memory.epoch_now)   -- [defence.c citizen_militia]
    T.bait(memory.army, events, info.time, memory.epoch_now)
    T.shelter(memory.army, events, info.time)
    return nil
end

return M
