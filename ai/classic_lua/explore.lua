-- EECPExplorationManager in Lua (the classic port; ORIGIN in this folder): every 30 s with exploration enabled, the
-- land explorers kept at num_land_explorers (the fastest land class the player can train: a free unit of it drafted,
-- else one requested), the idle ones sent exploring (ai.explore: the engine's explore goal) with the difficulty's
-- stance (Easy scout, Hard harass); the sea explorers likewise (num_sea_explorers of the fastest warship class, once
-- the dock is researched, with water on the map). The space and air explorers: NOT STARTED.
-- require("classic_lua.explore").
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local U = require("classic_lua.units")

local M = {}

local LAND_TRAINERS = {b_barracks = true, b_archery_range = true, b_stable = true, b_siege_factory = true,
                       b_tank_factory = true, b_cyber_factory = true}

-- the fastest land class trainable now (RequestExplorer's screen. DEVIATION: the explorer screener's classes are not
-- exposed; the land classes the military buildings train)
local function best_class(view)
    local best, bs
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building then
            local t = U.class_tech(view, c.type)
            local land = false
            for _, p in ipairs(t ~= nil and t.producers or {}) do
                if LAND_TRAINERS[p] then
                    land = true
                end
            end
            local cls = ai.class(c.type)
            if land and cls ~= nil and (cls.speed or 0) > 0 and (bs == nil or cls.speed > bs or (cls.speed == bs and c.type < best)) then
                best, bs = c.type, cls.speed
            end
        end
    end
    return best
end

-- the fastest warship class trainable now (RequestExplorer(kExplorerSea): a sea class, no fishing boat)
local function best_sea_class(view)
    local best, bs
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building then
            local cls = ai.class(c.type)
            if cls ~= nil and cls.domain == "sea" and cls.category ~= "boat" and not cls.is_transport
                    and (cls.speed or 0) > 0 and (bs == nil or cls.speed > bs or (cls.speed == bs and c.type < best)) then
                best, bs = c.type, cls.speed
            end
        end
    end
    return best
end

-- one explorer of a class kept: a free unit of it drafted, else one requested
local function draft(view, memory, ex, class)
    local free = ai.units{type = class, held = "engine", idle = true}
    if #free > 0 then
        ex.ids[#ex.ids + 1] = free[1]
        ai.take({free[1]})
    elseif not U.is_requested(memory, class) and not U.in_production(view, class) then
        U.request(view, memory, class, 1)
    end
end

function M.step(view, memory)
    local cp = memory.cp
    local ex = memory.ex
    if ex == nil then
        ex = {ids = {}, next = 0}
        memory.ex = ex
    end
    if view.time < ex.next or not cp.explore_enabled then
        return
    end
    ex.next = view.time + 30000
    local keep = {}
    for _, id in ipairs(ex.ids) do
        if R.unit(id) ~= nil then
            keep[#keep + 1] = id
        end
    end
    ex.ids = keep
    local land, sea = 0, 0
    for _, id in ipairs(ex.ids) do
        local u = R.unit(id)
        if u ~= nil and ai.class(u.type).domain == "sea" then
            sea = sea + 1
        else
            land = land + 1
        end
    end
    if land < T.b(memory, "num_land_explorers") then
        local class = best_class(view)
        if class ~= nil then
            -- the personnel's draft: a free unit of the class, else one requested (picked up when it is trained)
            draft(view, memory, ex, class)
        end
    end
    -- the sea explorers: the dock researched, water on the map (gDCGameSpeed: the water tiles)
    local dock = U.class_tech(view, "b_dock")
    if sea < T.b(memory, "num_sea_explorers") and dock ~= nil and dock.researched and not dock.obsolete
            and (memory.t.water_tiles or 0) > 0 then
        local class = best_sea_class(view)
        if class ~= nil then
            draft(view, memory, ex, class)
        end
    end
    local stance = memory.t.difficulty == 0 and 0 or (memory.t.difficulty == 2 and 3 or nil)
    for _, id in ipairs(ex.ids) do
        local u = R.unit(id)
        if u ~= nil and u.action == "idle" and u.inside == nil then
            if stance ~= nil then
                ai.stance({id}, stance)
            end
            ai.explore({id})
        end
    end
end

return M
