-- Helpers over the next engine's view (docs/AI_LUA_PLAN.md §3.2 "S2 gate as built"). require("lib.view").
local M = {}

-- the resources in their order (the view's per-resource maps are keyed by these names)
M.resources = {"food", "wood", "stone", "gold", "iron"}

-- the entry of view.costs for a class (nil: the player cannot make it now)
function M.cost(view, type)
    for _, c in ipairs(view.costs) do
        if c.type == type then
            return c
        end
    end
    return nil
end

-- the entry of view.buildings for a class (nil: none built, going up or queued)
function M.buildings(view, type)
    for _, b in ipairs(view.buildings) do
        if b.type == type then
            return b
        end
    end
    return nil
end

-- finished + going up + queued of a buildings entry (0 for nil)
function M.total(entry)
    if entry == nil then
        return 0
    end
    return entry.finished + entry.unfinished + entry.queued
end

-- the sum of a per-resource map (a cost)
function M.sum(amounts)
    local s = 0
    for _, r in ipairs(M.resources) do
        s = s + amounts[r]
    end
    return s
end

return M
