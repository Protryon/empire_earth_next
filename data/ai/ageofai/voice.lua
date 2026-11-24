-- Age of AI port: THE VOICE (upstream dll/voice/llm.c, voice/persona.c, the retail mod's; UPSTREAM has the map
-- and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.voice").
--
-- Optional (settings.llm_enabled). The engine's transport is ai.llm (EECPNextLLM.cpp): a table configures it, a
-- string starts one request on its worker thread, no argument polls the last answer (nil while none). A slow or
-- missing server never stalls the step. The lines are printed to the AI trace until the game's chat has a Lua door
-- (upstream speaks through persona_say; this engine has no chat binding yet).
local S = require("ageofai.settings")
local M = {}

local EVERY = 8000          -- a line every this long at most
local MAX_REQ = 6           -- requests before it gives up for the game

local function available()
    return ai ~= nil and ai.llm ~= nil
end

local function configure()
    if not available() then
        return false
    end
    ai.llm{enabled = S.llm_enabled and true or false, url = S.llm_url or "", api_key = S.llm_api_key or "",
            model = S.llm_model or "", timeout = S.llm_timeout or 20, think = S.llm_think and true or false}
    return true
end

-- the persona's name and traits, as the prompt's frame
local function system_prompt(memory)
    local p = memory.persona or {}
    local t = p.trait or {}
    return string.format(
        "You speak as %s, a computer player in Empire Earth: The Art of Conquest. One short line, at most 20 words, " ..
        "no quotes and no stage directions. Traits: aggression %d, loyalty %d, pride %d, greed %d, forgiveness %d, " ..
        "caution %d.", tostring(p.name or "an AI"), t.aggression or 50, t.loyalty or 50, t.pride or 50,
        t.greed or 50, t.forgiveness or 50, t.caution or 50)
end

function M.step(view, memory)
    if not S.llm_enabled or not available() then
        return
    end
    memory.voice = memory.voice or {t = 0, req = 0, started = false}
    local v = memory.voice
    if not v.started then
        v.started = configure()
        if not v.started then
            return
        end
    end
    -- the answer, when it comes
    local ans = ai.llm()
    if ans ~= nil and ans ~= "" then
        v.line = ans
        v.answered = (v.answered or 0) + 1
        print(string.format("voice: %s", ans))
    end
    -- a line now and then
    if view.time - (v.t or 0) < EVERY or (v.req or 0) >= MAX_REQ then
        return
    end
    v.t = view.time
    local prompt = "The game is at minute " .. tostring(view.time // 60000) ..
        ". Say one line about how it is going for you."
    local seq = ai.llm(prompt, system_prompt(memory))
    if seq ~= nil then
        v.req = (v.req or 0) + 1
        v.last_seq = seq
    end
end

function M.clear(memory)
    if memory ~= nil then
        memory.voice = nil
    end
end

return M