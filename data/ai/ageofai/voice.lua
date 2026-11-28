-- Age of AI port: THE VOICE (upstream dll/voice/llm.c, voice/persona.c, the retail mod's; UPSTREAM has the map
-- and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.voice").
--
-- Optional (settings.llm_enabled). The engine's transport is ai.llm (EECPNextLLM.cpp): a table configures it, a
-- string starts one request on its worker thread and returns its sequence, a number polls this AI's answer to that
-- request (nil while none; the transport keeps an answer per player, so AIs never read each other's lines). A slow
-- or missing server never stalls the step.
local S = require("ageofai.settings")
local PS = require("ageofai.personas")
local M = {}

local MAX_REQ = 40          -- requests before it gives up for the game
local FAIL_LIMIT = 3        -- consecutive LLM failures before it stops asking for a while
local DOWN = 300000         -- how long it stays quiet after FAIL_LIMIT failures (5 minutes), then it tries again

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
    local who = PS.get(p.id or "balanced")
    return string.format(
        "You speak as %s, %s, a computer player in Empire Earth: The Art of Conquest. One short line, at most 20 words, " ..
        "no quotes and no stage directions. Traits: aggression %d, loyalty %d, pride %d, greed %d, forgiveness %d, " ..
        "caution %d.", tostring(p.name or "an AI"), (who ~= nil and who.character) or "an AI", t.aggression or 50,
        t.loyalty or 50, t.pride or 50, t.greed or 50, t.forgiveness or 50, t.caution or 50)
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
    -- the answer, when it comes: into the game's chat (ai.say) when the game has it, else the trace. v.seq is the
    -- request this AI waits for: the transport keeps one answer per player and a poll asks only for answers after
    -- that sequence, so a line is said once (the old poll returned the same answer at every step, a chat loop).
    if v.seq ~= nil then
        local ans, seq = ai.llm(v.seq)
        if ans ~= nil and ans ~= "" then
            v.seq = seq or v.seq
            if S.llm_log then
                print(string.format("voice: answer %s", tostring(ans)))
            end
            if type(ans) == "string" and string.sub(ans, 1, 9) == "llm error" then
                -- a dead or refusing server: count it, say nothing, and stay quiet for a while after a few
                v.fails = (v.fails or 0) + 1
                if v.fails >= FAIL_LIMIT then
                    v.fails = 0
                    v.down_until = view.time + DOWN
                    print(string.format("voice: LLM unavailable (%s) - no requests for %d minutes", ans, DOWN // 60000))
                end
            elseif ans ~= v.line then
                -- a small model says the same thing again and again: keep the chat free of copies
                v.line = ans
                v.answered = (v.answered or 0) + 1
                v.fails = 0
                v.down_until = nil
                local said = false
                if ai.say ~= nil and S.llm_lines ~= false then
                    said = ai.say(ans) ~= false
                end
                if not said then
                    print(string.format("voice: %s", ans))
                end
            end
        end
    end
    -- a model that keeps failing: no requests for a while (a long game should not spend itself on a dead server),
    -- and the persona's own line now and then so the chat is not silent; the next period tries again
    if v.down_until ~= nil then
        if view.time < v.down_until then
            if view.time - (v.persona_t or 0) >= 300000 then
                v.persona_t = view.time
                M.say(memory, "greeting")
            end
            return
        end
        v.down_until = nil
    end
    -- the human's chat (llm_ai_talk): answer the next line from the human (player 1, single player)
    if ai.chat ~= nil and S.llm_ai_talk then
        local text, player = ai.chat()
        if text ~= nil and (player or 0) == 1 and text ~= "" then
            v.talk = (v.talk or 0) + 1
            v.seq = ai.llm("The human said: \"" .. tostring(text) .. "\". Answer in one short line.",
                           system_prompt(memory)) or v.seq
            return
        end
    end
    -- a line now and then (not while a decision waits for the model: one request at a time)
    if v.dec ~= nil and next(v.dec) ~= nil then
        return
    end
    local every = tonumber(S.llm_line_seconds or 60) * 1000
    if every <= 0 or view.time - (v.t or 0) < every or (v.req or 0) >= MAX_REQ then
        return
    end
    v.t = view.time
    local prompt = "The game is at minute " .. tostring(view.time // 60000) ..
        ". Say one line about how it is going for you."
    local seq = ai.llm(prompt, system_prompt(memory))
    if seq ~= nil then
        v.req = (v.req or 0) + 1
        v.seq = seq
        if S.llm_log then
            print(string.format("voice: ask %s", prompt))
        end
    else
        -- the transport takes one request at a time and another AI has it: ask again shortly (not a whole period)
        v.t = view.time - math.max(every - 2000, 0)
        if S.llm_log and not v.busy_note then
            v.busy_note = true
            print("voice: the transport is busy with another AI's request - trying again")
        end
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
        local seq = ai.llm(question, "Answer with exactly ACCEPT or REJECT, nothing else. You are the computer player; the game's rules bind you.")
        v.dec[key] = {t = now, seq = seq or 0}
        return nil
    end
    if (d.seq or 0) == 0 then
        v.dec[key] = nil
        return default   -- the transport was busy: the rules decide
    end
    local ans = ai.llm(d.seq)
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