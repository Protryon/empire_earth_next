-- The economy's bench metrics (docs/STRATEGY_RESEARCH.md §4.5): idle town-centre seconds, crowded forage patches, the
-- town centres. require("lib.econ_stats"). Call M.step(view, memory, wanted) every strategy step (wanted: true while
-- the plan wants more citizens); once a game minute it prints
--   econ cit=<citizens> tcs=<finished town centres and capitols> tc_idle=<s> tc_idle_any=<s> crowd=<patches over 6>
--        crowd_max=<most gatherers sent to one patch> food=<food gatherers> boats=<n>
-- tc_idle counts a producer's second with no citizen in production while the plan wants more citizens (the first
-- macro rule: a producer always training); tc_idle_any counts every such second. Both are cumulative.
local V = require("lib.view")

local M = {}

local PATCH_PLACES = 6   -- a forage patch holds 6 gatherers (the base game's mining limit)

function M.step(view, memory, wanted)
    local s = memory.econ_stats
    if s == nil then
        s = {idle = 0, idle_any = 0}
        memory.econ_stats = s
    end
    local citizen = V.cost(view, "citizen")
    local producers = citizen and citizen.producers or 0
    local making = citizen and citizen.in_production or 0
    local free = producers - making
    if free > 0 then
        s.idle_any = s.idle_any + free
        if wanted and view.population < view.population_cap then
            s.idle = s.idle + free
        end
    end
    if view.time % 60000 < 1000 then
        local crowd, most = 0, 0
        for _, site in ipairs(view.sites) do
            if site.patch and site.workers ~= nil then
                if site.workers > PATCH_PLACES then
                    crowd = crowd + 1
                end
                if site.workers > most then
                    most = site.workers
                end
            end
        end
        local tcs = 0
        for _, t in ipairs({"b_town_center", "b_capitol"}) do
            local b = V.buildings(view, t)
            if b ~= nil then
                tcs = tcs + b.finished
            end
        end
        print(string.format("econ cit=%d tcs=%d tc_idle=%d tc_idle_any=%d crowd=%d crowd_max=%d food=%d boats=%d",
            view.citizens, tcs, s.idle, s.idle_any, crowd, most, view.gatherers.food, view.boats))
    end
end

return M
