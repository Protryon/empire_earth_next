-- Age of AI port: PERSONAS (upstream dll/base/persona.c, dll/ai/plans.c; UPSTREAM has the map and the licence,
-- GPL-3.0, Grille and the Age of AI contributors). require("ageofai.personas").
--
-- The retail engine's persona loader (words.c/persona.c, the .eeai files in config/personalities) is not in this
-- engine, so the port carries its own table of named personas: traits (0..100, 50 without a personality) and the
-- six leanings (0..100, 50 neutral). A persona is picked per AI at the start (settings.persona: a name or "random")
-- and its leanings only change play when settings.persona_style is true (upstream [ai_base] ai_style, default off:
-- "personalities only talk and negotiate" - here they change the plan's lever, which is the port's equivalent).
-- The traits are handed to diplo.lua (its TRAIT table) regardless of the style switch, so the diplomacy model
-- always varies by persona.
local S = require("ageofai.settings")
local M = {}

M.DEFAULT_TRAITS = {aggression = 50, loyalty = 50, pride = 50, greed = 50, forgiveness = 50, caution = 50}
M.DEFAULT_LEANINGS = {air = 50, navy = 50, anti_air = 50, pressure = 50, defences = 50, spread = 50}

-- six leanings in the order plans.lua stores them (LN_AIR, LN_NAVY, LN_ANTI_AIR, LN_PRESSURE, LN_DEF, LN_SPREAD)
M.LEAN_KEYS = {"air", "navy", "anti_air", "pressure", "defences", "spread"}

M.PERSONAS = {
    balanced = {name = "Balanced",
        traits = {aggression = 50, loyalty = 50, pride = 50, greed = 50, forgiveness = 50, caution = 50},
        leanings = {air = 50, navy = 50, anti_air = 50, pressure = 50, defences = 50, spread = 50}},
    aggressive = {name = "Aggressive",
        traits = {aggression = 75, loyalty = 40, pride = 65, greed = 55, forgiveness = 35, caution = 30},
        leanings = {air = 55, navy = 45, anti_air = 40, pressure = 75, defences = 35, spread = 65}},
    defensive = {name = "Defensive",
        traits = {aggression = 25, loyalty = 60, pride = 40, greed = 45, forgiveness = 60, caution = 75},
        leanings = {air = 45, navy = 55, anti_air = 70, pressure = 30, defences = 75, spread = 35}},
    tall = {name = "Tall",
        traits = {aggression = 40, loyalty = 65, pride = 45, greed = 30, forgiveness = 55, caution = 60},
        leanings = {air = 40, navy = 40, anti_air = 55, pressure = 30, defences = 45, spread = 35}},
    wide = {name = "Wide",
        traits = {aggression = 55, loyalty = 40, pride = 55, greed = 60, forgiveness = 40, caution = 45},
        leanings = {air = 45, navy = 45, anti_air = 45, pressure = 45, defences = 60, spread = 75}},
    naval = {name = "Naval",
        traits = {aggression = 50, loyalty = 50, pride = 50, greed = 45, forgiveness = 50, caution = 50},
        leanings = {air = 60, navy = 80, anti_air = 50, pressure = 40, defences = 45, spread = 40}},
}

M.ORDER = {"balanced", "aggressive", "defensive", "tall", "wide", "naval"}

-- the persona picked for this AI (stored in memory.persona); "random" picks by the view's/ai's rng
function M.pick(memory, view)
    memory.persona = memory.persona or {}
    local p = memory.persona
    if p.id ~= nil then
        return p.id
    end
    local id = S.persona or "balanced"
    if id == "random" then
        local r = 0
        if view ~= nil and view.time ~= nil then
            r = (view.time + (memory.me or 0) * 7919) % #M.ORDER
        end
        id = M.ORDER[r + 1]
    end
    if M.PERSONAS[id] == nil then
        id = "balanced"
    end
    p.id = id
    return id
end

function M.get(id)
    return M.PERSONAS[id] or M.PERSONAS.balanced
end

function M.traits(memory, view)
    local id = M.pick(memory, view)
    local p = M.get(id)
    local t = {}
    for k, v in pairs(M.DEFAULT_TRAITS) do
        t[k] = p.traits[k] or v
    end
    return t
end

function M.leanings(memory, view)
    local id = M.pick(memory, view)
    local p = M.get(id)
    local l = {}
    for i, k in ipairs(M.LEAN_KEYS) do
        l[i] = (p.leanings[k] or 50) / 100.0
    end
    return l
end

-- apply the persona to the AI's memory: traits for diplo, leanings for the plans (only when persona_style is on)
function M.step(memory, view)
    memory.persona = memory.persona or {}
    local id = M.pick(memory, view)
    memory.persona = memory.persona or {}
    memory.persona.trait = M.traits(memory, view)
    if S.persona_style then
        memory.plan = memory.plan or {}
        memory.plan.leanings = M.leanings(memory, view)
    end
    memory.persona.id = id
    memory.persona.name = M.get(id).name
end

function M.clear(memory)
    if memory ~= nil then
        memory.persona = nil
    end
end

return M