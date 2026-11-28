-- Age of AI port, THE SAVINGS (upstream dll/ai/savings.c, config/ageofai.ini [ai_base] savings; UPSTREAM has the map
-- and the licence, GPL-3.0, Grille and the Age of AI contributors). require("ageofai.savings").
--
-- The playtester (2026-11-20), after missile subs (600 gold + 600 iron) were trained by 1 AI in 8: "a bit more
-- expensive order shouldn't need to be priority to get through, that points at a potential larger problem", then
-- "build it as a general system from the beginning". The spenders gave up on whatever costs more than they hold at
-- once: the navy's mix bought the next role it could pay now out of the same money, the army's SAVING FIRST
-- substituted after 90 s whatever the money did - frigates and battleships spent what a missile sub was being saved
-- for.
--
-- A SAVING: one per pool - ARMY the soldiers' orders (their military account), NAVY the warships' -. A spender that
-- wants a type the budget refuses (and that it can train) asks M.want; while the saving lasts its whole cost - per
-- resource it needs, the others stay free - is held from that pool's other orders, in the stock (a row of the
-- priority list, priority.c "savings"; the port's priority.lua needs the row, see the report) and in the pool's
-- account. A saving holds the stock from every military order, the other pool's too: the stock is one. It ends:
--   - bought (M.bought: an order of it got through)
--   - no longer asked within SV_ASKED (its spender wants another, or nothing)
--   - stalled: what it has of its cost (the spender's measure, 0..1000) has not grown in SV_STALL (a resource with
--     no income) - or not at all in SV_OBSERVE since it began
--   - out of reach: at the rate it has grown since it began, not there within SV_HORIZON (looked at from SV_OBSERVE)
--   - urgent: its spender needs something at once (the army: enemy soldiers in its base)
-- A type given up is not saved for again for SV_AGAIN: its spender's own way meanwhile.
-- Not held from research, buildings or citizens (upgrades before units): a missile sub's saving stalls where
-- research takes the gold and iron - still about one a game (measured).
--
-- The spender's measure `have`: thousandths of the cost it holds, M.fraction(view, avail, type). `avail` is what its
-- account may spend now (depths econ.lua budget.avail/mil, navy.lua's own). Integres, so every peer is the same.
--
-- The API (upstream's names in brackets):
--   M.want(view, memory, g, type, have, urgent) -> true: saving, wait; false: the spender's own way (sv_want)
--   M.bought(view, memory, type) (sv_bought)         M.type(view, memory, g) -> type or nil (sv_type)
--   M.gave(view, memory, g, type) -> bool (sv_gave)  M.live(view, memory, g) -> bool
--   M.held(view, memory, g, type, held) -> adds the saving's cost to a held map, unless type is the saved one (sv_held)
--   M.hold(view, memory, r, type) -> adds every pool's saving to a row's map (sv_hold)
--   M.avail(view, memory, g, type, avail) -> the account with the row applied: the saved type sees the held cost back
--   M.may(view, memory, g, type, cost, avail) -> may an order of type of cost be paid out of avail now?
--   M.fraction(view, avail, type) -> thousandths           M.log(view, memory) (sv_log)
-- Settings: savings (1 upstream).
local V = require("lib.view")
local S = require("ageofai.settings")
local M = {}

local SV_ASKED = 30000     -- its spender asked for it this recently
local SV_STALL = 120000    -- what it has has not grown this long
local SV_HORIZON = 600000  -- out of reach: not there within this at the rate it grew
local SV_OBSERVE = 90000   -- not before this since it began
local SV_AGAIN = 180000    -- a type given up is not saved for again this long

M.ARMY, M.NAVY = 1, 2
M.N = 2
M.keys = {"army", "navy"}
local KEY = {"army", "navy"}

local function state(memory)
    memory.sv = memory.sv or {e = {}, gave = {}, gave_t = {}, started = {}, bought_n = {}, stalled = {}, reach = {},
                              urgent_n = {}, dropped = {}, log_t = 0}
    memory.sv.e = memory.sv.e or {}
    for g = 1, M.N do
        memory.sv.e[g] = memory.sv.e[g] or {type = nil, since = 0, asked_t = 0, best_t = 0, have0 = 0, best = 0}
    end
    return memory.sv
end

-- the savings are on (settings.savings, 1 upstream since 2026-11-20)
function M.on()
    return (S.savings or 0) ~= 0
end

local function reset(e)
    e.type, e.since, e.asked_t, e.best_t, e.have0, e.best = nil, 0, 0, 0, 0, 0
end

local function live(view, e)
    return e.type ~= nil and e.asked_t ~= 0 and view.time >= e.asked_t and view.time - e.asked_t < SV_ASKED
end

function M.live(view, memory, g)
    if g == nil or g < 1 or g > M.N then
        return false
    end
    return live(view, state(memory).e[g])
end

-- the log's end line (sv_end)
local function ending(view, s, g, why)
    local e = s.e[g]
    if e.type == nil then
        return
    end
    print(string.format("savings %s type=%s ends=%s after=%d min have=%d%%", KEY[g], e.type, why,
        math.max(view.time - e.since, 0) // 60000, e.best // 10))
    reset(e)
end

-- the spender of pool g wants type and holds `have` thousandths of its cost; urgent: it needs something now.
-- true: saving for it (the pool's other orders leave its cost alone); false: not, the spender's own way
function M.want(view, memory, g, type, have, urgent)
    if not M.on() or g == nil or g < 1 or g > M.N or type == nil then
        return false
    end
    local s = state(memory)
    local e = s.e[g]
    local now = view.time
    if e.type ~= nil and (now < e.since or now < e.asked_t) then
        reset(e)   -- a game before
    end
    if e.type ~= nil and e.type ~= type then
        if live(view, e) then
            return false   -- another type saved for
        end
        s.dropped[g] = (s.dropped[g] or 0) + 1
        ending(view, s, g, "no longer asked")
    end
    if e.type == nil then
        if ((s.gave[g] == type) and now >= (s.gave_t[g] or 0) and now - (s.gave_t[g] or 0) < SV_AGAIN)
                or urgent then
            return false
        end
        reset(e)
        e.type = type
        e.since = now > 0 and now or 1
        e.best_t = e.since
        e.have0 = have or 0
        e.best = have or 0
        s.started[g] = (s.started[g] or 0) + 1
        print(string.format("savings %s saves for type=%s have=%d%%", KEY[g], type, (have or 0) // 10))
    end
    e.asked_t = now > 0 and now or 1
    if (have or 0) > e.best + 10 then
        e.best = have
        e.best_t = now
    end
    local why
    if urgent then
        why = "needed something at once"
        s.urgent_n[g] = (s.urgent_n[g] or 0) + 1
    elseif now - e.best_t >= SV_STALL
            or (now - e.since >= SV_OBSERVE and e.have0 < 50 and e.best <= e.have0 + 10) then
        why = "stalled"
        s.stalled[g] = (s.stalled[g] or 0) + 1
    elseif now - e.since >= SV_OBSERVE and e.best > e.have0 and e.best < 1000
            and (1000 - e.best) * (now - e.since) > SV_HORIZON * (e.best - e.have0) then
        why = "out of reach"
        s.reach[g] = (s.reach[g] or 0) + 1
    end
    if why ~= nil then
        s.gave[g] = type
        s.gave_t[g] = now > 0 and now or 1
        ending(view, s, g, why)
        return false
    end
    return true
end

-- an order of type got through: its saving is done (sv_bought)
function M.bought(view, memory, type)
    if type == nil then
        return
    end
    local s = state(memory)
    for g = 1, M.N do
        if s.e[g].type == type then
            s.bought_n[g] = (s.bought_n[g] or 0) + 1
            ending(view, s, g, "bought")
        end
    end
end

-- the type pool g saves for now (nil none)
function M.type(view, memory, g)
    if not M.on() or g == nil or g < 1 or g > M.N then
        return nil
    end
    local e = state(memory).e[g]
    return live(view, e) and e.type or nil
end

-- a type of pool g given up lately (the navy's old skip for it)
function M.gave(view, memory, g, type)
    if g == nil or g < 1 or g > M.N or type == nil then
        return false
    end
    local s = state(memory)
    local now = view.time
    return s.gave[g] == type and now >= (s.gave_t[g] or 0) and now - (s.gave_t[g] or 0) < SV_AGAIN
end

-- the live saving's cost map of a pool (nil: none; the type may have left the epoch)
local function cost_of(view, memory, g)
    local type_ = M.type(view, memory, g)
    if type_ == nil then
        return nil, nil
    end
    local c = V.cost(view, type_)
    return type_, c ~= nil and c.cost or nil
end

-- what pool g's saving holds from an order of type (its own: nothing), added to held (sv_held)
function M.held(view, memory, g, type, held)
    if not M.on() or held == nil or g == nil or g < 1 or g > M.N then
        return
    end
    local saved, cost = cost_of(view, memory, g)
    if saved == nil or saved == type then
        return
    end
    for _, k in ipairs(V.resources) do
        held[k] = (held[k] or 0) + (cost[k] or 0)
    end
end

-- the priority list's row "savings": the stock a soldier's (warship's) order leaves to its pool's saving; type is
-- the order's own type (nil: holds from every order). Both pools, the stock is one (sv_hold).
function M.hold(view, memory, r, type)
    if r == nil then
        return
    end
    for g = 1, M.N do
        M.held(view, memory, g, type, r)
    end
end

-- the account a spender of pool g sees, `avail` being what the priority list's "savings" row already left it: another
-- type's order leaves the saving's cost alone, the saved type sees the cost the row held for everyone. A copy.
-- (A caller that did not add the row must hold the saving itself: M.held with its own type.)
function M.avail(view, memory, g, type, avail)
    local out = {}
    for _, k in ipairs(V.resources) do
        out[k] = avail ~= nil and (avail[k] or 0) or 0
    end
    if not M.on() or g == nil or g < 1 or g > M.N then
        return out
    end
    local saved, cost = cost_of(view, memory, g)
    if saved == nil or saved ~= type then
        return out
    end
    for _, k in ipairs(V.resources) do
        out[k] = out[k] + (cost[k] or 0)
    end
    return out
end

-- may an order of type (its cost map) be paid out of avail now, the row applied? true: yes. This is the spender's
-- question: "may I spend this on this kind now, or does a bigger want save for it?"
function M.may(view, memory, g, type, cost, avail)
    if cost == nil then
        return false
    end
    local left = M.avail(view, memory, g, type, avail)
    for _, k in ipairs(V.resources) do
        if (cost[k] or 0) > (left[k] or 0) then
            return false
        end
    end
    return true
end

-- thousandths of type's cost that avail holds (the spender's measure: the scarcest resource counts)
function M.fraction(view, avail, type)
    local c = type ~= nil and V.cost(view, type) or nil
    if c == nil or avail == nil then
        return 0
    end
    local best = 1000
    for _, k in ipairs(V.resources) do
        local need = c.cost[k] or 0
        if need > 0 then
            local have = math.max(avail[k] or 0, 0) * 1000 // need
            if have < best then
                best = have
            end
        end
    end
    return math.min(best, 1000)
end

-- "# savings:" each minute (sv_log)
function M.log(view, memory)
    local s = memory.sv
    if s == nil or not M.on() or (s.log_t ~= nil and view.time >= s.log_t and view.time - s.log_t < 60000) then
        return
    end
    s.log_t = view.time > 0 and view.time or 1
    for g = 1, M.N do
        local e = s.e[g]
        local on = live(view, e)
        print(string.format("savings %s %s%s have=%d%% started=%d bought=%d stalled=%d reach=%d urgent=%d asked=%d",
            KEY[g], on and "type=" or "none ", on and e.type or 0, e.best // 10, s.started[g] or 0,
            s.bought_n[g] or 0, s.stalled[g] or 0, s.reach[g] or 0, s.urgent_n[g] or 0, s.dropped[g] or 0))
    end
end

-- a new game (savings_new_game): nothing saved yet
function M.clear(memory)
    memory.sv = nil
end

return M
