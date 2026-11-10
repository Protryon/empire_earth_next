-- Age of AI: a Lua port of Grille's "Empire Earth: Age of AI" (GPL-3.0; UPSTREAM in this folder names the upstream
-- commit, the per-file map and the licence; docs/AI_RESEARCH.md has the bench). require("ageofai.main").
--
-- The economy pays what a human pays and sees what a human sees: a budget with an epoch reserve and a military
-- account, worker targets by shortfall and the measured rates, towers by the town centre first and at the fronts
-- later, drop sites before gathering, the gathering upgrades; the land army is one force ordered as one through the
-- per-unit interface (ageofai.army). The navy, landings, other islands and diplomacy are not ported yet.
local E = require("ageofai.econ")
local A = require("ageofai.army")
local T = require("ageofai.tactics")
local V = require("lib.view")

local M = {}

function M.init(view, memory)
    memory.started = view.time
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
    memory.army_plan = A.step(view, memory, orders, budget, events) or memory.army_plan
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
    T.counterfire(memory.army, events, info.time)
    T.bait(memory.army, events, info.time, memory.epoch_now)
    return nil
end

return M
