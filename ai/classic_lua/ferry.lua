-- The classic's ferries in Lua (EECPResourceManager::RequestFerry / FerryGatherers / TransportIdle,
-- EEComputerPlayer::IsIslandExpansionDue / SetNextIslandExpansionTime, EECPCivilizationManager::ChooseTransportClass;
-- the classic port, ORIGIN in this folder). On an island or planets map, when a mineral is wanted (or the island
-- expansion is due) and its target lies on land the town's citizens cannot walk to, the idle citizens there are
-- ferried over by a transport and set to gather it. The crossing is lib.navy's landing (board, sail, unload).
-- require("classic_lua.ferry").
--
-- Memory: memory.ferry = {next_island = ms, trip = {L = landing, far = id, kind = r, ids = {citizens}} or nil}.
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local N = require("lib.navy")
local C = T.C

local M = {}

local function U()
    return require("classic_lua.units")
end

local function state(memory)
    local f = memory.ferry
    if f == nil then
        -- SetNextIslandExpansionTime at the start (EEComputerPlayer::Update's first tick)
        f = {next_island = T.island_expansion_timer(memory) * 60000}
        memory.ferry = f
    end
    return f
end

-- the map kinds the classic ferries on (IsIslandExpansionDue: island or planets maps)
function M.water_map(memory)
    return memory.t.island or memory.t.planets
end

-- IsIslandExpansionDue
function M.due(view, memory)
    return M.water_map(memory) and view.time >= state(memory).next_island
end

-- ChooseTransportClass for the sea (a dock stands): the epoch's transport. DEVIATION: the airport's and space dock's
-- transports (Modern and later) are not chosen yet (the air and space stage)
local SEA = {[0] = "s02_stone_transport", "s02_stone_transport", "s03_copper_transport", "s04_bronze_transport",
             "s04_bronze_transport", "s04_bronze_transport", "s04_bronze_transport", "s08_imperial_transport",
             "s08_imperial_transport", "s10_atomic_age_transport", "s10_atomic_age_transport", "s10_atomic_age_transport",
             "s10_atomic_age_transport", "s13_gargantua_transport", "s13_gargantua_transport"}

function M.transport_class(view)
    if #ai.units{type = C.dock} == 0 then
        return nil
    end
    return SEA[view.epoch]
end

-- the citizens on a ferry trip (the gatherer assignment leaves them alone)
function M.busy(memory, id)
    local f = memory.ferry
    return f ~= nil and f.trip ~= nil and f.trip.busy[id] == true
end

local function free_transports(memory)
    local out = {}
    for _, t in ipairs(N.transports()) do
        if t.cargo == 0 and not (memory.cross_ships or {})[t.id] then
            out[#out + 1] = t
        end
    end
    return out
end

-- RequestFerry(stranded, far): a free transport takes as many of the stranded citizens as it holds to the far
-- resource (FerryGatherers; the island expansion timer starts again); none free: one more transport ordered when
-- fewer than 4 exist and none is requested or being made
function M.request(view, memory, stranded, far)
    local f = state(memory)
    if f.trip ~= nil or #stranded == 0 then
        return false
    end
    local class = M.transport_class(view)
    if class == nil then
        return false
    end
    local free = free_transports(memory)
    if #free > 0 then
        local t = free[1]
        local ids = {}
        for k = 1, math.min(#stranded, math.max(1, t.cap)) do
            ids[#ids + 1] = stranded[k].id
        end
        local L, why = N.plan({x = far.x, y = far.y, player = 0}, ids, {t})
        if L ~= nil then
            local busy = {}
            for _, id in ipairs(ids) do
                busy[id] = true
            end
            f.trip = {L = L, far = far.id, kind = far.kind, busy = busy}
            M.busy_now = busy
            f.next_island = view.time + T.island_expansion_timer(memory) * 60000
            print(string.format("classic_lua ferry.start n=%d to=%d,%d ship=%d", #ids, far.x, far.y, t.id))
            return true
        end
        print(string.format("classic_lua ferry.noplan why=%s", tostring(why)))
        return false
    end
    local all = ai.units{type = class}
    if #all < 4 and not U().is_requested(memory, class) and not U().in_production(view, class) then
        U().request(view, memory, class, 1)
    end
    return false
end

-- the citizens on a trip this step, for the builders' list (build.lua available_citizens)
M.busy_now = {}

-- every step: the trip's landing; ashore, the citizens gather the far resource
function M.step(view, memory)
    local f = memory.ferry
    M.busy_now = f ~= nil and f.trip ~= nil and f.trip.busy or {}
    if f == nil or f.trip == nil then
        return
    end
    local trip = f.trip
    local res, out = N.landing_step(trip.L)
    if res == "landed" then
        local far = R.unit(trip.far)
        local rm = R.state(memory)
        for _, id in ipairs(out) do
            if far ~= nil and (far.amount or 0) > 0 then
                R.send_to_gather(memory, id, far)
            else
                R.add_idle(rm, id)
            end
        end
        print(string.format("classic_lua ferry.landed n=%d", #out))
        f.trip = nil
    elseif res == "aborted" then
        print(string.format("classic_lua ferry.aborted why=%s", tostring(out)))
        f.trip = nil
    end
end

return M
