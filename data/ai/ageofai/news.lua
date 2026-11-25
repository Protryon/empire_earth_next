-- Age of AI port: THE NEWS (upstream dll/ai/news.c, the lines the force and the territories give an AI to say;
-- UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.news").
--
-- A light stand-in for news.c: the territories it holds are counted every step (a finished building that shoots or
-- trains holds one, territories.lua's hold), and a gain or a loss says the persona's line through voice.lua (the
-- game's chat, or the AI trace). One line a minute at most, only with settings.news and settings.ai_lines.
local S = require("ageofai.settings")
local TV = require("ageofai.territories")
local VC = require("ageofai.voice")

local M = {}
local EVERY = 60000

function M.step(view, memory)
    if S.news == false or S.ai_lines == false then
        return
    end
    memory.news = memory.news or {held = nil, t = -1000000}
    local n = memory.news
    local held = 0
    for i = 1, TV.count() do
        local info = TV.info(i)
        if info ~= nil and (info.hold or 0) > 0 then
            held = held + 1
        end
    end
    if n.held == nil then
        n.held = held
        return
    end
    if view.time - (n.t or 0) < EVERY then
        return
    end
    if held > n.held then
        VC.say(memory, "capture")
    elseif held < n.held then
        VC.say(memory, "loss")
    end
    n.held, n.t = held, view.time
end

function M.clear(memory)
    if memory ~= nil then
        memory.news = nil
    end
end

return M