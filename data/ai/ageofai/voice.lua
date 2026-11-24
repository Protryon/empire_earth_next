-- Age of AI port: THE VOICE (upstream dll/voice/llm.c, voice/persona.c, the retail mod's; UPSTREAM has the map
-- and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.voice").
--
-- Optional (settings.llm_enabled). The engine's transport is ai.llm (EECPNextLLM.cpp): a table configures it, a
-- string starts one request on its worker thread, no argument polls the last answer (nil while none). A slow or
-- missing server never stalls the step. The lines are printed to the AI trace until the game's chat has a Lua door
-- (upstream speaks through persona_say; this engine has no chat binding yet).
local S = require("ageofai.settings")
local PS = require("ageofai.personas")
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
    -- the answer, when it comes: into the game's chat (ai.say) when the game has it, else the trace
    local ans = ai.llm()
    if ans ~= nil and ans ~= "" then
        v.line = ans
        v.answered = (v.answered or 0) + 1
        local said = false
        if ai.say ~= nil and S.llm_lines ~= false then
            said = ai.say(ans) ~= false
        end
        if not said then
            print(string.format("voice: %s", ans))
        end
    end
    -- a line now and then (not while a decision waits for the model: one request at a time)
    if v.dec ~= nil and next(v.dec) ~= nil then
        return
    end
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

-- an LLM yes/no, asynchronous: nil while it thinks (the caller waits), true/false when it answers, `default` with
-- the model off, failing, or after 30 s. Only with settings.llm_decide (upstream default off).
function M.decide(memory, key, question, default)
    if not S.llm_decide or not available() or memory == nil then
        return default
    end
    memory.voice = memory.voice or {}
    local v = memory.voice
    v.dec = v.dec or {}
    local d = v.dec[key]
    if d == nil then
        local now = (ai.time ~= nil and ai.time()) or 0
        v.dec[key] = {t = now}
        ai.llm(question, "Answer with exactly ACCEPT or REJECT, nothing else. You are the computer player; the game's rules bind you.")
        return nil
    end
    local ans = ai.llm()
    if ans ~= nil then
        v.dec[key] = nil
        local a = string.upper(ans)
        if string.find(a, "ACCEPT", 1, true) ~= nil then
            return true
        end
        if string.find(a, "REJECT", 1, true) ~= nil then
            return false
        end
        return default
    end
    local now = (ai.time ~= nil and ai.time()) or 0
    if now - (d.t or 0) > 30000 then
        v.dec[key] = nil
        return default
    end
    return nil
end

function M.clear(memory)
    if memory ~= nil then
        memory.voice = nil
    end
end

-- a persona's line on a situation (war, peace, alliance, betrayal, capitulation, push, greeting): the game's chat
-- through ai.say, else the AI trace. Independent of the LLM (settings.ai_lines).
function M.say(memory, situation)
    if memory == nil or S.ai_lines == false or situation == nil then
        return false
    end
    local id = (memory.persona or {}).id or "balanced"
    local list = PS.lines(id, situation)
    if list == nil or #list == 0 then
        return false
    end
    local k = 1
    if ai ~= nil and ai.random ~= nil then
        k = ai.random(1, #list)
    end
    local text = list[k]
    memory.voice = memory.voice or {}
    memory.voice.said = (memory.voice.said or 0) + 1
    local said = false
    if ai ~= nil and ai.say ~= nil then
        said = ai.say(text) ~= false
    end
    if not said then
        print(string.format("voice: %s", text))
    end
    return true
end

return M