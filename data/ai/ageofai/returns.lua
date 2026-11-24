-- Age of AI port: upstream dll/ai/returns.c (the returns: what each kind of force kills and loses for what it
-- fields, the recent fighting weighed most; UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.returns").

local V = require("lib.view")
local S = require("ageofai.settings")

local M = {}

-- constants (returns.c)
local RT_EVERY = 10000
local RT_HALF = 600000
local RT_STEP = 0.98851402   -- 0.5 ^ (RT_EVERY / RT_HALF)
local RT_PRIOR = 1000
local RT_WORTH_MIN = 0.2
local RT_WORTH_MAX = 2.0
local RK_N = 8
local RK = { LAND = 0, LAND_AA = 1, NAVY = 2, FIGHTER = 3, BOMBER = 4, HELI = 5, DEF = 6, OTHER = 7 }

local function enabled()
    return S.strategy_plans == true
end

-- helpers over ai.class
local cls_cache = {}
local function cls(t)
    if t == nil then
        return {}
    end
    local c = cls_cache[t]
    if c == nil then
        c = (ai.class and ai.class(t)) or {}
        cls_cache[t] = c
    end
    return c
end

local function is_aa_name(t)
    t = t or ""
    return string.sub(t, 1, 4) == "b_aa" or string.sub(t, 1, 2) == "aa"
        or string.find(t, "_aa_", 1, true) ~= nil
        or string.find(t, "missile_trooper", 1, true) ~= nil
        or string.find(t, "anti_missile", 1, true) ~= nil
end

local function is_tower_name(t)
    t = t or ""
    return string.find(t, "tower", 1, true) ~= nil
end

-- rt_kind (returns.c): land soldiers, anti-air, navy, fighters, bombers, helicopters, defences, other
local function rt_kind(t)
    local c = cls(t)
    local cat = c.category
    if cat == "air" then
        local fam = c.family
        if fam == 0x10 then   -- helicopter family
            return (c.attack or 0) > 0 and RK.HELI or RK.OTHER
        end
        if string.find(t or "", "heli", 1, true) then
            return (c.attack or 0) > 0 and RK.HELI or RK.OTHER
        end
        if string.sub(t or "", 1, 9) == "a_bomber_" or t == "x11_me110" or t == "a_catalina" then
            return RK.BOMBER
        end
        if string.sub(t or "", 1, 12) == "a_at_fighter"
            or string.sub(t or "", 1, 16) == "a_strafe_fighter"
            or string.sub(t or "", 1, 9) == "a_fighter" then
            return RK.FIGHTER
        end
        return RK.OTHER
    end
    if cat == "building" then
        local fam = c.family
        if fam == 0x2e then   -- AA building family
            return RK.LAND_AA
        end
        if (c.attack or 0) > 0 or is_tower_name(t) or fam == 0x25 then
            return RK.DEF
        end
        return RK.OTHER
    end
    if cat == "ship" then
        if (c.attack or 0) > 0 then
            return RK.NAVY
        end
        return RK.OTHER
    end
    if cat == "soldier" or cat == "siege" or cat == "hero" then
        local fam = c.family
        if fam == 0x20 then
            return RK.LAND_AA
        end
        if is_aa_name(t) then
            return RK.LAND_AA
        end
        return RK.LAND
    end
    return RK.OTHER
end

local function price_of(view, t)
    local c = V.cost(view, t)
    if c and c.cost then
        local s = V.sum(c.cost)
        if s and s > 0 then
            return math.floor(s + 0.5)
        end
    end
    local cl = cls(t)
    if cl.cost then
        local s = V.sum(cl.cost)
        if s and s > 0 then
            return math.floor(s + 0.5)
        end
    end
    if string.find(t or "", "citizen", 1, true) then
        return 1
    end
    return 1
end

local function state(memory)
    memory.returns = memory.returns or {}
    local s = memory.returns
    if type(s.killed) ~= "table" or #s.killed < RK_N then
        s.killed = { 0, 0, 0, 0, 0, 0, 0, 0 }
    end
    if type(s.lost) ~= "table" or #s.lost < RK_N then
        s.lost = { 0, 0, 0, 0, 0, 0, 0, 0 }
    end
    if type(s.field) ~= "table" or #s.field < RK_N then
        s.field = { 0, 0, 0, 0, 0, 0, 0, 0 }
    end
    if type(s.all_killed) ~= "table" or #s.all_killed < RK_N then
        s.all_killed = { 0, 0, 0, 0, 0, 0, 0, 0 }
    end
    if type(s.all_lost) ~= "table" or #s.all_lost < RK_N then
        s.all_lost = { 0, 0, 0, 0, 0, 0, 0, 0 }
    end
    if type(s.lost_to) ~= "table" or #s.lost_to < RK_N then
        s.lost_to = {}
        for i = 1, RK_N do
            s.lost_to[i] = { 0, 0, 0, 0, 0, 0, 0, 0 }
        end
    else
        for i = 1, RK_N do
            if type(s.lost_to[i]) ~= "table" or #s.lost_to[i] < RK_N then
                s.lost_to[i] = { 0, 0, 0, 0, 0, 0, 0, 0 }
            end
        end
    end
    s.field_t = s.field_t or 0
    s.log_t = s.log_t or 0
    s.decay_t = s.decay_t or 0
    return s
end

local function kind_index(kind)
    if type(kind) == "number" then
        local k = math.floor(kind)
        if k >= 0 and k < RK_N then
            return k
        end
        return nil
    end
    if type(kind) == "string" then
        local low = string.lower(kind)
        if low == "land" or low == "soldier" or low == "soldiers" then return 0 end
        if low == "anti-air" or low == "anti_air" or low == "land_aa" or low == "aa" then return 1 end
        if low == "navy" or low == "ship" or low == "ships" or low == "warship" or low == "warships" then return 2 end
        if low == "fighter" or low == "fighters" then return 3 end
        if low == "bomber" or low == "bombers" then return 4 end
        if low == "heli" or low == "helicopter" or low == "helicopters" then return 5 end
        if low == "defence" or low == "defences" or low == "def" or low == "tower" or low == "towers" then return 6 end
        if low == "other" then return 7 end
    end
    return nil
end

function M.step(view, memory, events)
    memory = memory or {}
    local s = state(memory)
    -- new game reset
    if s.decay_t ~= 0 and view.time < s.decay_t then
        s.killed = { 0, 0, 0, 0, 0, 0, 0, 0 }
        s.lost = { 0, 0, 0, 0, 0, 0, 0, 0 }
        s.field = { 0, 0, 0, 0, 0, 0, 0, 0 }
        s.all_killed = { 0, 0, 0, 0, 0, 0, 0, 0 }
        s.all_lost = { 0, 0, 0, 0, 0, 0, 0, 0 }
        for i = 1, RK_N do
            s.lost_to[i] = { 0, 0, 0, 0, 0, 0, 0, 0 }
        end
        s.decay_t = 0
        s.field_t = 0
    end
    if s.decay_t == 0 then
        s.decay_t = (view.time and view.time > 0) and view.time or 1
    end
    -- decay
    while (view.time or 0) - s.decay_t >= RT_EVERY do
        for k = 1, RK_N do
            s.killed[k] = math.floor(s.killed[k] * RT_STEP + 0.5)
            s.lost[k] = math.floor(s.lost[k] * RT_STEP + 0.5)
            for j = 1, RK_N do
                s.lost_to[k][j] = math.floor(s.lost_to[k][j] * RT_STEP + 0.5)
            end
        end
        s.decay_t = s.decay_t + RT_EVERY
    end

    events = events or (ai.events and ai.events()) or {}
    local me = nil
    if ai.me then pcall(function() me = ai.me() end) end
    if me == nil then
        pcall(function()
            for _, p in ipairs(ai.players()) do
                if p.me then me = p.id end
            end
        end)
    end
    if me == nil then me = view.player or 0 end

    for _, e in ipairs(events) do
        if type(e) == "table" and e.kind == "died" then
            local dead_type = e.type
            if dead_type ~= nil then
                local dead_player = e.player or e.victim_player or e.owner
                if dead_player == nil and e.unit ~= nil and ai.unit then
                    local u = ai.unit(e.unit)
                    dead_player = u and u.player
                end
                local killer_id = e.killer
                local killer_player = e.killer_player
                if killer_player == nil and killer_id ~= nil and ai.unit then
                    local ku = ai.unit(killer_id)
                    killer_player = ku and ku.player
                end
                if dead_player ~= nil and killer_player ~= nil and dead_player ~= killer_player then
                    local killer_type = e.killer_type
                    if killer_type == nil and killer_id ~= nil and ai.unit then
                        local ku = ai.unit(killer_id)
                        killer_type = ku and ku.type
                    end
                    local kk = killer_type and rt_kind(killer_type) or RK.OTHER
                    local vk = rt_kind(dead_type)
                    local price = price_of(view, dead_type)
                    if price > 0 then
                        if killer_player == me then
                            s.killed[kk + 1] = (s.killed[kk + 1] or 0) + price
                            s.all_killed[kk + 1] = (s.all_killed[kk + 1] or 0) + price
                        end
                        if dead_player == me then
                            s.lost[vk + 1] = (s.lost[vk + 1] or 0) + price
                            s.lost_to[vk + 1][kk + 1] = (s.lost_to[vk + 1][kk + 1] or 0) + price
                            s.all_lost[vk + 1] = (s.all_lost[vk + 1] or 0) + price
                        end
                    end
                end
            end
        end
    end

    -- field refresh
    if s.field_t == 0 or (view.time or 0) - s.field_t >= RT_EVERY then
        s.field_t = (view.time and view.time > 0) and view.time or 1
        for k = 1, RK_N do
            s.field[k] = 0
        end
        local counts = {}
        if ai.units then
            local ids = ai.units({})
            for _, id in ipairs(ids or {}) do
                local u = (ai.unit and ai.unit(id)) or nil
                if u and u.type ~= nil then
                    local hp = u.hp
                    if hp == nil or hp > 0 then
                        if u.finished ~= false then
                            counts[u.type] = (counts[u.type] or 0) + 1
                        end
                    end
                end
            end
        end
        for t, n in pairs(counts) do
            local k = rt_kind(t)
            s.field[k + 1] = (s.field[k + 1] or 0) + n * price_of(view, t)
        end
    end
end

function M.worth_of(memory, mask)
    if not enabled() then
        return 1.0
    end
    memory = memory or {}
    local s = state(memory)
    if type(s) ~= "table" then
        return 1.0
    end
    local kd = RT_PRIOR
    local ls = RT_PRIOR
    local m = mask or 0
    for k = 0, RK_N - 1 do
        if (m >> k) & 1 == 1 then
            kd = kd + (s.killed and s.killed[k + 1] or 0)
            ls = ls + (s.lost and s.lost[k + 1] or 0)
        end
    end
    local w = kd / ls
    if w < RT_WORTH_MIN then
        w = RT_WORTH_MIN
    elseif w > RT_WORTH_MAX then
        w = RT_WORTH_MAX
    end
    return w
end

function M.worth(memory, kind)
    if not enabled() then
        return 1.0
    end
    memory = memory or {}
    local k = kind_index(kind)
    if k == nil then
        return 1.0
    end
    return M.worth_of(memory, 1 << k)
end

return M
