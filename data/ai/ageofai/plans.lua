-- Age of AI port: upstream dll/ai/plans.c (the strategy plans: a plan picked by the personality's leanings and
-- changed on what the fighting shows; UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.plans").

local S = require("ageofai.settings")
local V = require("lib.view")
local R = require("ageofai.returns")

local M = {}

-- constants (plans.c)
local PL_EVERY = 60000
local PL_FIRST = 900000
local PL_DWELL = 600000
local PL_FAIL = 0.6
local PL_BETTER = 1.5
local PL_SEA_MIN = 0.25
local LN_N = 6
local PN_N = 8

local PN = { BALANCED = 0, AIR = 1, SEA = 2, GUARDED = 3, PRESSURE = 4, ECONOMY = 5, FORTRESS = 6, EXPANSION = 7 }

-- k_plans: name, weight, f[6], on mask
local K_PLANS = {
    { name = "balanced",      weight = 0.5, f = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 }, on = 0 },
    { name = "air power",     weight = 1.0, f = { 1.8, 0.8, 1.0, 1.0, 1.0, 1.0 }, on = 1 << 0 },
    { name = "sea power",     weight = 1.0, f = { 0.7, 1.6, 1.0, 1.0, 1.0, 1.0 }, on = 1 << 1 },
    { name = "guarded",       weight = 1.0, f = { 0.9, 1.0, 1.8, 0.9, 1.3, 1.0 }, on = 1 << 2 },
    { name = "pressure",      weight = 1.0, f = { 1.0, 1.0, 0.6, 1.25, 0.8, 1.1 }, on = 1 << 3 },
    { name = "economy first", weight = 1.0, f = { 1.0, 0.9, 1.2, 0.85, 1.0, 1.2 }, on = 0 },
    { name = "fortress",      weight = 1.0, f = { 0.9, 1.0, 1.3, 1.0, 2.0, 0.9 }, on = 0 },
    { name = "expansion",     weight = 1.0, f = { 1.0, 1.0, 1.0, 1.0, 0.8, 1.8 }, on = 0 },
}

local K_LEAN_SPAN = {
    { 0.4, 1.8 }, { 0.6, 1.5 }, { 0.2, 2.0 }, { 0.85, 1.15 }, { 0.5, 1.6 }, { 0.6, 1.5 },
}
local K_LEVER_BOUND = {
    { 0.2, 2.5 }, { 0.5, 2.0 }, { 0.0, 2.0 }, { 0.75, 1.3 }, { 0.4, 2.5 }, { 0.5, 2.2 },
}

local LEAN_NAMES = { "air_force", "navy", "early_anti_air", "pressure", "defences", "spread" }
local SETTINGS_LEAN_KEYS = { "plan_lean_air", "plan_lean_navy", "plan_lean_anti_air", "plan_lean_pressure", "plan_lean_defences", "plan_lean_spread" }

local function enabled()
    return S.strategy_plans == true
end

local function state(memory)
    memory.plans = memory.plans or {}
    local s = memory.plans
    if type(s.f) ~= "table" or #s.f < LN_N then
        s.f = { 0, 0, 0, 0, 0, 0 }
    end
    if type(s.leanings) ~= "table" or #s.leanings < LN_N then
        s.leanings = nil
    end
    s.plan = s.plan or 0
    s.pick_t = s.pick_t or 0
    s.check_t = s.check_t or 0
    s.picks = s.picks or 0
    return s
end

local function load_leanings(memory)
    local s = state(memory)
    if s.leanings then
        return
    end
    local lean_src = nil
    if type(memory.plan) == "table" and type(memory.plan.leanings) == "table" then
        lean_src = memory.plan.leanings
    elseif type(memory.plan_leanings) == "table" then
        lean_src = memory.plan_leanings
    elseif type(memory.leanings) == "table" then
        lean_src = memory.leanings
    end
    local arr = {}
    for i = 1, LN_N do
        local v = nil
        if lean_src then
            v = lean_src[i]
            if v == nil then
                v = lean_src[LEAN_NAMES[i]]
            end
            if v == nil then
                v = lean_src[SETTINGS_LEAN_KEYS[i]]
            end
        end
        if v == nil then
            v = S[SETTINGS_LEAN_KEYS[i]]
        end
        if type(v) ~= "number" then
            v = 50
        else
            if v <= 1 then
                v = math.floor(v * 100 + 0.5)
            else
                v = math.floor(v + 0.5)
            end
        end
        if v < 0 then v = 0 end
        if v > 100 then v = 100 end
        arr[i] = v
    end
    s.leanings = arr
end

local function lean_of(memory, t)
    local s = state(memory)
    load_leanings(memory)
    return (s.leanings[t + 1] or 50) / 100.0
end

local function lean_factor(l, t)
    local lo = K_LEAN_SPAN[t + 1][1]
    local hi = K_LEAN_SPAN[t + 1][2]
    if l <= 0.5 then
        return lo + (1.0 - lo) * 2.0 * l
    else
        return 1.0 + (hi - 1.0) * 2.0 * (l - 0.5)
    end
end

local function lean_pull(l, t)
    if l <= 0.5 then
        return 0.25 + 1.5 * l
    else
        return 1.0 + 3.0 * (l - 0.5)
    end
end

local function plan_evidence(memory, lever)
    if lever == 0 then   -- air
        return R.worth_of(memory, (1 << 3) | (1 << 4) | (1 << 5))
    elseif lever == 1 then   -- navy
        return R.worth_of(memory, 1 << 2)
    elseif lever == 2 then   -- anti-air
        return R.worth_of(memory, 1 << 1)
    elseif lever == 3 then   -- pressure
        return R.worth_of(memory, 1 << 0)
    else
        return 1.0
    end
end

local function plan_fits(memory, p)
    -- sea power only where the water matters to it
    if K_PLANS[p + 1].name ~= "sea power" then
        return true
    end
    local nv = (type(memory) == "table") and memory.navy or nil
    if type(nv) ~= "table" or nv.strength == nil then
        return false
    end
    return (tonumber(nv.strength) or 0) >= 25
end

local function plan_fitness(memory, p)
    local plan = K_PLANS[p + 1]
    local f = 1.0
    for l = 0, LN_N - 1 do
        if (plan.on >> l) & 1 == 1 then
            f = f * plan_evidence(memory, l)
        end
    end
    return f
end

local function plan_weight(memory, p)
    local plan = K_PLANS[p + 1]
    local w = plan.weight
    load_leanings(memory)
    local s = state(memory)
    for l = 0, LN_N - 1 do
        local pull = lean_pull((s.leanings[l + 1] or 50) / 100.0, l)
        if plan.f[l + 1] > 1.0 then
            w = w * pull
        elseif plan.f[l + 1] < 1.0 then
            w = w / pull
        end
    end
    return w
end

local function clamp(v, lo, hi)
    if v < lo then return lo end
    if v > hi then return hi end
    return v
end

local function plan_pick(memory, view, not_this, this_one)
    local s = state(memory)
    load_leanings(memory)
    local w = {}
    local fit = {}
    local sum = 0.0
    for p = 0, PN_N - 1 do
        fit[p + 1] = plan_fitness(memory, p)
        if p == not_this or not plan_fits(memory, p) then
            w[p + 1] = 0.0
        else
            w[p + 1] = plan_weight(memory, p) * fit[p + 1]
        end
        sum = sum + w[p + 1]
    end
    local was = (s.pick_t and s.pick_t > 0) and s.plan or -1
    local pick = PN.BALANCED
    if this_one and this_one >= 0 then
        pick = this_one
    elseif sum > 0 then
        local seed = (memory.plan_seed or 0)
        local r = ((seed * 2654435761 + (view.time or 0)) % 1000) / 1000.0 * sum
        pick = PN_N - 1
        for p = 0, PN_N - 2 do
            if w[p + 1] <= 0 or r >= w[p + 1] then
                if w[p + 1] > 0 then
                    r = r - w[p + 1]
                end
            else
                pick = p
                break
            end
            if p == PN_N - 2 then
                -- fall through
            end
        end
        if w[pick + 1] <= 0 then
            pick = PN.BALANCED
        end
    end
    s.plan = pick
    s.pick_t = (view.time and view.time > 0) and view.time or 1
    s.picks = (s.picks or 0) + 1
    for l = 0, LN_N - 1 do
        local lfactor = lean_of(memory, l)
        local f = K_PLANS[pick + 1].f[l + 1] * lean_factor(lfactor, l)
        f = clamp(f, K_LEVER_BOUND[l + 1][1], K_LEVER_BOUND[l + 1][2])
        s.f[l + 1] = math.floor(f * 1000 + 0.5)
    end
end

function M.step(view, memory)
    if not enabled() then
        return
    end
    memory = memory or {}
    local s = state(memory)
    load_leanings(memory)
    if s.pick_t == 0 then
        -- pick at the start (upstream waits for navy.c's waters; the port has no navy readiness here)
        plan_pick(memory, view, -1, -1)
        s.check_t = (view.time and view.time > 0) and view.time or 1
        return
    end
    if (view.time or 0) - s.check_t < PL_EVERY then
        return
    end
    s.check_t = (view.time and view.time > 0) and view.time or 1
    if (view.time or 0) < PL_FIRST then
        return
    end
    if (view.time or 0) - s.pick_t < PL_DWELL then
        return
    end
    local cur = plan_fitness(memory, s.plan)
    local best = 0.0
    local bp = -1
    for p = 0, PN_N - 1 do
        if p ~= s.plan and plan_fits(memory, p) then
            local f = plan_fitness(memory, p)
            if f > best then
                best = f
                bp = p
            end
        end
    end
    local to = -1
    local why_change = false
    if not plan_fits(memory, s.plan) then
        why_change = true
    elseif cur < PL_FAIL then
        why_change = true
    elseif bp >= 0 and best > cur * PL_BETTER then
        why_change = true
        to = bp
    else
        return
    end
    plan_pick(memory, view, s.plan, to)
end

local LEVER_MAP = {
    air = 0, navy = 1, anti_air = 2, pressure = 3, defences = 4, spread = 5,
    air_force = 0, early_anti_air = 2,
}

function M.plan_f(memory, lever)
    if not enabled() then
        return 1.0
    end
    if type(memory) ~= "table" then
        return 1.0
    end
    memory = memory
    local s = memory.plans
    if type(s) ~= "table" or not s.pick_t or s.pick_t == 0 then
        return 1.0
    end
    local idx = lever
    if type(lever) == "string" then
        idx = LEVER_MAP[lever]
        if idx == nil then
            -- try number
            idx = tonumber(lever)
        end
    end
    if type(idx) ~= "number" then
        return 1.0
    end
    idx = math.floor(idx)
    if idx < 0 or idx >= LN_N then
        return 1.0
    end
    local v = s.f and s.f[idx + 1]
    if type(v) ~= "number" then
        return 1.0
    end
    return v / 1000.0
end

return M
