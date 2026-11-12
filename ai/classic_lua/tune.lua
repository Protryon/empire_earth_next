-- The classic port's data: the seat's tuning (ai.tuning(): the difficulty, the cheats, the personality of
-- aipersonalities.yaml and the dbcpbehavior values EEComputerPlayer::Init copied), the class names the classic code
-- names by number (EEUnitClassIDs.h), and the classic's accessors that pick a value by difficulty
-- (EEComputerPlayer.cpp GetAttackDelayModifier ... GetPopLimitAdjustment). require("classic_lua.tune").
--
-- The tuning is read once a game (init) into memory.t: integers, booleans and strings only; the floats of the C++
-- are thousandths (keys ending in _k).
local M = {}

-- the classic's unit class constants by name (EEUnitClassIDs.h; ai.* takes and returns the names)
M.C = {
    citizen = "citizen",                    -- 0x20
    female_citizen = "female_citizen",      -- 0x1e0
    barracks = "b_barracks",                -- 0x22
    town_center = "b_town_center",          -- 0x23
    dock = "b_dock",                        -- 0x24
    house = "b_house",                      -- 0x25
    temple = "b_temple",                    -- 0x27
    capitol = "b_capitol",                  -- 0x29
    farm = "b_farm",                        -- 0x2a
    settlement = "b_settlement",            -- 0x2b
    tower_paleo = "b_guard_tower_paleo",    -- 0x2f
    granary = "b_granary",                  -- 0x34
    archery_range = "b_archery_range",      -- 0x36
    forage_bush = "res_forage_bush",        -- 0x37
    university = "b_university",            -- 0x3c
    hospital = "b_hospital",                -- 0x3e
    siege_factory = "b_siege_factory",      -- 0x3f
    stable = "b_stable",                    -- 0x40
    navy_yard = "b_navy_yard",              -- 0x41
    airport = "b_airport",                  -- 0x42
    tank_factory = "b_tank_factory",        -- 0x44
    cyber_factory = "b_cyber_factory",      -- 0x45
    cyber_lab = "b_cyber_laboratory",
    space_dock = "b_space_dock",
    fish = "res_fish",                      -- 0x13b
    robotic_farm = "b_robotic_farm",        -- 0x33a
    priest_tower = "b_priest_tower",
}

local C = M.C

-- EECPCivilizationManager::GetTowerClass by epoch (0 Prehistoric .. 14 Space)
local TOWERS = {[0] = "b_guard_tower_paleo", "b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze",
    "b_guard_tower_bronze", "b_guard_tower_middle", "b_guard_tower_middle", "b_guard_tower_imperial",
    "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_ww1", "b_guard_tower_ww1", "b_guard_tower_digital",
    "b_guard_tower_digital", "b_guard_tower_space"}

function M.tower_class(epoch)
    return TOWERS[epoch]
end

-- EECPCivilizationManager::GetFishingBoatClass
function M.fishing_boat_class(epoch)
    if epoch <= 2 then
        return "s02_fishing_boat_stone"
    elseif epoch <= 8 then
        return "s04_fishing_boat_bronze"
    end
    return "s11_fishing_boat_modern"
end

-- EECPCivilizationManager::GetAntiAirClass (0 before WW1)
function M.anti_air_class(epoch)
    if epoch < 9 then
        return nil
    elseif epoch <= 11 then
        return "b_aa10_gun_88mm"
    elseif epoch <= 13 then
        return "b_aa13_missile_tower"
    end
    return "b_aa15_missile_tower"
end

-- the buildings the classic's building counts track (EECPCivilizationManager's mUnitCounts keys, in the std::map's
-- order of the class numbers): key = the class counted, per = the behaviour's units-per field, max = its maximum,
-- military = the min_military_buildings rule; the tower, anti-air and farm keys stand for the epoch's class
M.COUNTED = {
    {key = "b_barracks", num = 0x22, per = "units_per_barracks", max = "barracks_max", military = true},
    {key = "b_dock", num = 0x24, per = "units_per_dock", max = "dock_max", military = true},
    {key = "b_temple", num = 0x27, per = "units_per_temple", max = "temple_max"},
    {key = "b_farm", num = 0x2a, per = "units_per_granary", max = "granary_max", farm = true},
    {key = "tower", num = 0x2f, per = "units_per_tower", max = "tower_max", tower = true},
    {key = "b_archery_range", num = 0x36, per = "units_per_archery_range", max = "archery_range_max", military = true},
    {key = "b_university", num = 0x3c, per = "units_per_university", max = "university_max"},
    {key = "b_hospital", num = 0x3e, per = "units_per_hospital", max = "hospital_max"},
    {key = "b_siege_factory", num = 0x3f, per = "units_per_siege_factory", max = "siege_factory_max", military = true},
    {key = "b_stable", num = 0x40, per = "units_per_stable", max = "stable_max", military = true},
    {key = "b_navy_yard", num = 0x41, per = "units_per_naval_yard", max = "naval_yard_max", military = true},
    {key = "b_airport", num = 0x42, per = "units_per_airport", max = "airport_max", military = true},
    {key = "b_tank_factory", num = 0x44, per = "units_per_tank_factory", max = "tank_factory_max", military = true},
    {key = "b_cyber_factory", num = 0x45, per = "units_per_cyber_factory", max = "cyber_factory_max", military = true},
    {key = "anti_air", num = 0x91, per = "units_per_anti_air", max = "anti_air_max", anti_air = true},
    {key = "b_cyber_laboratory", num = 0x24a, per = "units_per_cyber_lab", max = "cyber_lab_max", military = true},
    {key = "b_space_dock", num = 0x2d8, per = "units_per_space_dock", max = "space_dock_max", military = true},
    {key = "b_priest_tower", num = 0x2f0, per = nil, per_value = 12, max_value = 16},
    {key = "space_turret", num = 0x302, per = "units_per_tower", max = "tower_max", space_turret = true},
}

-- reads the seat's tuning into memory once
function M.load(memory)
    if memory.t ~= nil then
        return memory.t
    end
    local t = ai.tuning()
    memory.t = t
    return t
end

-- the behaviour value (an integer; a float field is in thousandths)
function M.b(memory, name)
    return memory.t.behavior[name] or 0
end

-- the personality's value
function M.p(memory, name)
    return memory.t.personality[name]
end

function M.cheat(memory, name)
    return memory.t.cheats[name] == true
end

-- the difficulty picks (0 easy, 1 medium, 2 hard): a value of three, by name stem (stem_easy / _medium / _hard)
function M.by_difficulty(memory, easy, medium, hard)
    local d = memory.t.difficulty
    if d == 0 then
        return easy
    elseif d == 2 then
        return hard
    end
    return medium
end

-- EEComputerPlayer::GetEpochFrequencyModifier in thousandths: the deathmatch catch-up value, else easy/hard
-- percent, else 1
function M.epoch_frequency_modifier_k(memory)
    local b = memory.t.behavior
    if memory.t.deathmatch then
        return b.epoch_catch_up_deathmatch_k
    elseif memory.t.difficulty == 0 then
        return b.epoch_frequency_modifier_easy * 10
    elseif memory.t.difficulty == 2 then
        return b.epoch_frequency_modifier_hard * 10
    end
    return 1000
end

-- EEComputerPlayer::GetResearchFrequencyModifier in thousandths
function M.research_frequency_modifier_k(memory)
    local b = memory.t.behavior
    if memory.t.difficulty == 0 then
        return b.research_frequency_modifier_easy * 10
    elseif memory.t.difficulty == 2 then
        return b.research_frequency_modifier_hard * 10
    end
    return 1000
end

-- EEComputerPlayer::GetAttackDelayModifier / GetAttackFrequencyModifier in thousandths
function M.attack_delay_modifier_k(memory)
    local b = memory.t.behavior
    if memory.t.difficulty == 0 then
        return b.attack_delay_modifier_easy * 10
    elseif memory.t.difficulty == 2 then
        return b.attack_delay_modifier_hard * 10
    end
    return 1000
end

function M.attack_frequency_modifier_k(memory)
    local b = memory.t.behavior
    if memory.t.difficulty == 0 then
        return b.attack_frequency_modifier_easy * 10
    elseif memory.t.difficulty == 2 then
        return b.attack_frequency_modifier_hard * 10
    end
    return 1000
end

-- EEComputerPlayer::GetExpansionTimer by difficulty (minutes)
function M.expansion_timer(memory)
    local b = memory.t.behavior
    return M.by_difficulty(memory, b.expansion_timer_easy, b.expansion_timer_medium, b.expansion_timer_hard)
end

-- EEComputerPlayer::GetIslandExpansionTimer (ms; 60 s when unset)
function M.island_expansion_timer(memory)
    local b = memory.t.behavior
    local v = M.by_difficulty(memory, b.island_expansion_timer_easy, b.island_expansion_timer_medium,
        b.island_expansion_timer_hard)
    return v
end

-- EECPEpochTimerSpan: the mean of the epoch frequency's bounds x its modifier, at least a minute (ms)
function M.epoch_timer_span(memory)
    local b = memory.t.behavior
    local span = (b.epoch_frequency_min + b.epoch_frequency_max) * M.epoch_frequency_modifier_k(memory) * 30
    return span > 60000 and span or 60000
end

-- an integer in [lo, hi] from the script's own stream (gPhysics->GetRandom's draws: DEVIATION, the script cannot draw
-- the engine's synced stream; the classic's draw of a float in [lo, hi) becomes an integer draw of the same span)
function M.random(lo, hi)
    if hi <= lo then
        return lo
    end
    return ai.random(lo, hi)
end

-- a float in [0, 1) as thousandths
function M.random_k()
    return ai.random(0, 999)
end

return M
