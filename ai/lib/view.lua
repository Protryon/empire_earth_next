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

-- the wall and tower upgrades (dbtechtree ids) in epoch order, from the Copper Age: each turns the towers and walls
-- of the epoch before into its own (dbeffects kind 19) and makes the next one available. A computer player is given
-- each with its epoch (EEComputerPlayer::EnterEpoch researches the epoch's kind-7 techs), so `research` comes back
-- done; it is ordered for the rules where it is not free (the town centre researches it)
M.tower_upgrades = {"copper_tower_upgrade", "bronze_wall_and_tower_upgrade", "middle_wall_and_tower_upgrade",
                    "imperial_wall_and_tower_upgrade", "ww1_wall_and_tower_upgrade", "info_wall_and_tower_upgrade",
                    "space_wall_and_tower_upgrade"}

-- the next tower upgrade's research order, or nil. It reads the result of the order it gave the step before
-- (mem.tower_up_at: that order's index, which the caller sets): done goes on to the next upgrade, a refusal for the
-- epoch waits for the next epoch, others (queued, busy, cannot_afford, no_producer) try again in 30 s
function M.tower_upgrade(view, mem)
    local k = mem.tower_up or 1
    local r = mem.tower_up_at ~= nil and view.results[mem.tower_up_at] or nil
    mem.tower_up_at = nil
    if r ~= nil and r.verb == "research" then
        if r.code == "done" then
            k = k + 1
        elseif r.reason == "not_in_epoch" then
            mem.tower_up_epoch = view.epoch
        elseif r.reason == "bad_argument" or r.reason == "unknown_class" then
            k = #M.tower_upgrades + 1   -- not in this data (a binary tree has no ids)
        else
            mem.tower_up_wait = view.time + 30000
        end
    end
    mem.tower_up = k
    if k > #M.tower_upgrades or mem.tower_up_epoch == view.epoch or (mem.tower_up_wait or 0) > view.time then
        return nil
    end
    return {verb = "research", tech = M.tower_upgrades[k]}
end

return M
