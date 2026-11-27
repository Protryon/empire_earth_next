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
--
-- The mod's own personalities (config/personalities/*.eeai, vendored unchanged in personalities/, turned into
-- ageofai.personas_data by tools/personas.py) are the persona source when that generated file is there: their
-- traits, leanings, character and lines. The table below is then the fallback - for an id the file does not have,
-- and for every field or situation it leaves out. settings.persona names an id here or there; "random" picks among
-- all of them (the built-in ids first, then the mod's, in the generated file's order).
local S = require("ageofai.settings")
local M = {}

M.DEFAULT_TRAITS = {aggression = 50, loyalty = 50, pride = 50, greed = 50, forgiveness = 50, caution = 50}
M.DEFAULT_LEANINGS = {air = 50, navy = 50, anti_air = 50, pressure = 50, defences = 50, spread = 50}

-- the six traits in the .eeai files'/persona.c's order (aggression, loyalty, pride, greed, forgiveness, caution)
M.TRAIT_KEYS = {"aggression", "loyalty", "pride", "greed", "forgiveness", "caution"}

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

-- THE LINES (upstream voice/persona.c persona_say: the texts a personality says on a situation). The port says
-- them through ai.say (the game's chat) or the AI trace when the game has no chat. ASCII only.
M.LINES = {
    balanced = {
        war = {"You leave me no choice. We fight.", "A pity. The sword is drawn."},
        peace = {"Enough blood. Let us talk.", "Peace is the better bargain."},
        alliance = {"An alliance, then. To both our gain.", "Together, we would be stronger."},
        betrayal = {"Your promises were worth nothing.", "I should have seen this coming."},
        capitulation = {"We have lost. I lay down my crown.", "You have won. Spare my people."},
        push = {"The army is ready. We march.", "Time to take what is ours."},
        capture = {"The ground is ours. Hold it.", "Another territory taken."},
        loss = {"We lost ground today.", "They took a territory from us."},
        greeting = {"Well met. Let us see who builds better.", "A new world. Let us begin."},
    },
    aggressive = {
        war = {"Good. I was getting bored.", "Your lands are already mine."},
        peace = {"A pause, not an end.", "Fine. I will sharpen my blades."},
        alliance = {"Keep up, and I will not eat you.", "An ally. Do not disappoint me."},
        betrayal = {"Traitor! You will pay for this.", "I knew you could not be trusted."},
        capitulation = {"This is not over! ... Fine. It is over.", "I yield, but I do not forgive."},
        push = {"Burn it all. Forward!", "The weak have held long enough."},
        greeting = {"I smell weakness. This should be quick.", "Let us make it bloody."},
    },
    defensive = {
        war = {"You attack a patient people. Come, then.", "Our walls will hold."},
        peace = {"We have lost enough. Let us stop.", "A wall is better than a grave."},
        alliance = {"A friend at my side is a wall the stronger.", "Agreed. We watch each other's backs."},
        betrayal = {"You were to be trusted. Never again.", "The walls were for you, it seems."},
        capitulation = {"We cannot hold. We will not die for pride.", "Take the town. Let the people live."},
        push = {"We move when the ground is ready.", "Carefully. Then all at once."},
        greeting = {"We will build high and hold what we have.", "Let them come to us."},
    },
    tall = {
        war = {"You interrupt progress. That is costly.", "War is a poor investment, but I will pay."},
        peace = {"Peace is the cheaper path. Let us take it.", "Good. Back to building."},
        alliance = {"A shared telescope is better than a shared war.", "Agreed. Let us both grow."},
        betrayal = {"You have cost me more than a war would.", "So be it. I will remember."},
        capitulation = {"My towers are dust. I yield.", "The books will say I lost. Let them."},
        push = {"The epoch is ours. Take the ground.", "Advance. The scholars will follow."},
        greeting = {"Knowledge, then steel. In that order.", "Let us see who understands the game."},
    },
    wide = {
        war = {"There is room for one of us.", "You stand where I want to build."},
        peace = {"The map is big. Let us share it.", "A border, then, and no more."},
        alliance = {"An ally who settles: useful.", "Together we can fill the map."},
        betrayal = {"You took the land I gave you.", "This is what I get for sharing."},
        capitulation = {"Too much land to hold. It is yours.", "I cannot be everywhere. You win."},
        push = {"Spread out. Take the corners.", "New lands, new towns, forward."},
        greeting = {"Every hill a town, every shore a dock.", "The world has room. I will take it."},
    },
    naval = {
        war = {"The sea is mine. Your shore is not.", "You will hear my cannons before you see them."},
        peace = {"Let the fleets rest.", "The tide turns. Let us stop."},
        alliance = {"A port in your harbour: agreed.", "Together the waters are ours."},
        betrayal = {"You sank my ships. Now it is personal.", "I will find your harbour."},
        capitulation = {"The fleet is gone. I am beaten.", "Take the docks. Leave the sailors."},
        push = {"Sail. Land. Take it.", "The crossing is clear."},
        greeting = {"I will rule the waves and the coast.", "Let us see who can swim."},
    },
}

-- THE MOD'S PERSONALITIES (upstream config/personalities, vendored in personalities/): the six the retail mod
-- ships - Hedda the builder, Temur the conqueror, Livia the schemer, Benedict the merchant, Brann the brawler,
-- Gunther the knight. tools/personas.py turns their .eeai files into ageofai.personas_data (the numbers, the
-- character text and the lines of each situation; the .eeai conditions are not evaluated there). Loaded once, if
-- the generated file is there; an id, a field or a situation it leaves out falls back to the table above.
local function persona_add(id, p)
    local e = M.PERSONAS[id]
    if e == nil then
        e = {}
        M.PERSONAS[id] = e
        M.ORDER[#M.ORDER + 1] = id      -- "random" picks among the mod's personalities too
    end
    e.traits = e.traits or {}
    e.leanings = e.leanings or {}
    local lines = M.LINES[id] or {}
    M.LINES[id] = lines
    e.lines = lines
    e.name = e.name or id
    if type(p.name) == "string" and p.name ~= "" then
        e.name = p.name
    end
    if type(p.character) == "string" and p.character ~= "" then
        e.character = p.character      -- for voice.lua (upstream voice/prompt: who this is, in a few words)
    end
    if type(p.traits) == "table" then
        for i = 1, #M.TRAIT_KEYS do
            local v = p.traits[M.TRAIT_KEYS[i]]
            if type(v) == "number" then
                e.traits[M.TRAIT_KEYS[i]] = v
            end
        end
    end
    if type(p.leanings) == "table" then
        for i = 1, #M.LEAN_KEYS do
            local v = p.leanings[M.LEAN_KEYS[i]]
            if type(v) == "number" then
                e.leanings[M.LEAN_KEYS[i]] = v
            end
        end
    end
    if type(p.lines) == "table" then
        for situation, list in pairs(p.lines) do
            if type(situation) == "string" and type(list) == "table" and #list > 0 then
                lines[situation] = list
            end
        end
    end
end

-- the generated table in its own order (the tool sorts it): data.list first, then anything a hand edit added
local function personas_load()
    local ok, data = pcall(require, "ageofai.personas_data")
    if not ok or type(data) ~= "table" or type(data.personas) ~= "table" then
        return
    end
    local ids, seen = {}, {}
    local listed = type(data.list) == "table" and data.list or {}
    for i = 1, #listed do
        local id = listed[i]
        if type(id) == "string" and type(data.personas[id]) == "table" and not seen[id] then
            seen[id] = true
            ids[#ids + 1] = id
        end
    end
    local rest = {}
    for id, p in pairs(data.personas) do
        if type(id) == "string" and type(p) == "table" and not seen[id] then
            rest[#rest + 1] = id
        end
    end
    table.sort(rest)
    for i = 1, #rest do
        ids[#ids + 1] = rest[i]
    end
    for i = 1, #ids do
        persona_add(ids[i], data.personas[ids[i]])
    end
end

personas_load()

function M.lines(id, situation)
    local p = M.LINES[id] or M.LINES.balanced
    return p[situation] or (M.LINES.balanced[situation] or {})
end


-- the persona picked for this AI (stored in memory.persona); "random" picks by the view's/ai's rng
function M.pick(memory, view)
    memory.persona = memory.persona or {}
    local p = memory.persona
    if p.id ~= nil then
        return p.id
    end
    local id = S.persona or "balanced"
    if id == "random" then
        -- per AI and per map, without repeats: the player id deals the order (5 is coprime with the list's length),
        -- the map's size shifts the deal so a new map deals anew
        local me = 0
        if ai ~= nil and ai.me ~= nil then
            me = ai.me() or 0
        end
        local off = 0
        if ai ~= nil and ai.map ~= nil then
            local m = ai.map()
            if m ~= nil then
                off = ((m.width or 0) * 7 + (m.height or 0) * 3) % #M.ORDER
            end
        end
        local r = (me * 5 + off) % #M.ORDER
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
    if not memory.persona.logged then
        memory.persona.logged = true
        local me = (ai ~= nil and ai.me ~= nil) and (ai.me() or 0) or 0
        print(string.format("persona p%d %s (%s)", me, id, M.get(id).name or id))
    end
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