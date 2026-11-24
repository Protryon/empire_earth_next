-- Age of AI port, RESEARCH BY RANK (upstream dll/ai/research.c, config/ageofai.ini [ai_base] paid_upgrades and
-- unit_improvements; UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.research"), called once a step from econ.lua's M.step or main.lua (see the report).
--
-- The playtester (2026-11-14): "research first: prioretyes: 1. all resarch and upgrades (exept hero/unit upgrades)
-- doable in capitol/towncenter, universtithy, and hospitals. 2. all unit upgrades of units that actualy exist
-- curently 3. epoch (skiped based on mode and conditions) 4. all other available unit upgrades". Every RS_EVERY, over
-- every tech of its tree that is available, not researched, of its epoch or an earlier one, with a price (the data's
-- "not used" techs have none), no object (a unit or building is no research) and a finished building of its that
-- researches it (without one there is nothing to buy or to save for):
--   RANK 1 - what a Town Center, Capitol, University, Hospital or granary researches that is no unit's upgrade: the
--            gathering techs (first: they pay back), the hospital's and the university's, the granary's, and the walls'
--            and towers' line upgrades (a Town Center's too). The cheapest first.
--   RANK 2 - a line upgrade of a line it fields, and the range improvements of the units it fields (THE UNITS'
--            IMPROVEMENTS, below), the most units first. While one of them is still to buy the epoch is neither bought
--            nor saved for (THE LINES BEFORE THE EPOCH) - the port: see the deviations.
--   RANK 3 - the next epoch: econ.lua's (the copper plan, epoch_pacing, min_workforce, the SOL the engine requires,
--            the advance verb). Not this module's: upstream's rank 3 is ported there.
--   RANK 4 - every other tech it can buy and the other improvements, cheapest first (the techs) or in an order of its
--            own (the improvements: a hash of the player, the type and the attribute, the same on every PC), with what
--            no row of the priority list holds back.
-- THE UNITS' IMPROVEMENTS ([ai_base] unit_improvements, on): the attack, hit points, range and speed a player buys
-- with the "+" buttons for a unit type, for each type it fields (RS_FIELDED: one on the map), never a citizen's or a
-- wall's, while the engine offers the attribute (ai.improve_cost's price above 0) and it is not maxed. The range ones
-- in rank 2 (both levels before the next epoch, the most units first), the others in rank 4 and only with what is left
-- after them. Bought as a player buys: paid now, in after its research time (the engine queues it per type; the queued
-- level counts as bought).
-- A rank buys only with what the ranks above it don't still wait for: the price of the next RS_HOLD1 of rank 1 and
-- RS_HOLD2 of rank 2 it could not buy this look is held back - from its own lower ranks here (M.hold, mem.rs.hold),
-- from everybody else by the row "research" of the priority list (priority.lua; the epoch has its own row).
-- Upstream's file is the clean slate's (game_ai = 0): it researches everything itself. This port keeps the engine's
-- research manager behind the verbs (as docs/AI_LUA_VERBS.md §6 has it) and only decides what to ask for.
--
-- ENGINE DEVIATIONS (the patches of upstream, which this engine does not need):
--   - PAID UPGRADES (rank 2's line upgrades, [ai_base] paid_upgrades): EEComputerPlayer::EnterEpoch researches every
--     kind-7 tech of the epoch for free (EEComputerPlayer.cpp:1731), so the AI never sees an available, unresearched
--     line upgrade: the lines come with the epoch, paid by nothing. settings.paid_upgrades is kept for the record. The
--     "the lines before the epoch" rule therefore never holds an epoch back here (and the engine's advance would not
--     let research spend the epoch's reserve anyway, see below).
--   - A LOST UPGRADE (marked queued that no building holds at two looks running is set free): the engine's queues are
--     the engine's own; a tech the AI asked for and nobody took stays available and is asked again.
--   - "A finished maker, the one with the least in its queue, none that holds a research already or more than
--     RS_QUEUE entries": the port's research verb (lib/verbs.lua v_research/try_research) queues the tech and the
--     runner takes it at an idle built producer; the AI does not pick the building. RS_QUEUE has no counterpart.
--   - The engine checks every verb against the reserve verb (econ.lua sets it to the epoch's + the towns' - the
--     navy's), so ranks 1 and 2 wait for the epoch where upstream's spend above it (priority.lua's note).
--   - The improvement points cap (gWorldTable kWorldUnitImprovementPointsMax, a pot per type) is not exposed: the
--     engine's CanBuy refuses; two refusals of a type's improvement mark the type capped (a pot never empties).
--   - ai.improve_cost (and the improve verb's WHAT table) name only attack, hit_points, range and speed. The engine's
--     ai.improve takes any attribute 0..15 (armours 2..7 cost one point, area effect 0, flight 8, leadership 10,
--     transport 15), but the port's verb layer cannot pass them: those improvements are NOT ported (a one-line gap of
--     lib/verbs.lua, noted in the report).
--   - The engine's own switch that allows unit improvements in the game setup (gUnitImprovementsAllowed) is unreadable
--     from Lua: settings.unit_improvements is the only switch here.
--   - "A temple's range techs in rank 1": a tech's effects are not exposed (ai.techs has no effect list), so the
--     temple's techs (producers b_temple/x_mosque/...) all stay in rank 4, as upstream's other temple techs.
-- Paid: the verb pays (the engine's checks), the stock the module sees is read again at the next look.
local V = require("lib.view")
local P = require("ageofai.priority")
local S = require("ageofai.settings")
local M = {}

local RS_EVERY = 5000     -- the look (upstream RS_EVERY)
local RS_SCAN = 30000     -- DEVIATION: the tech tree is read this often (ai.techs is charged per node); upstream
                          -- reads it every look. Bought techs invalidate it at once.
local RS_IMP = 60000      -- DEVIATION: the improvement prices/levels this often (ai.improve_cost is one call each)
local RS_HOLD1 = 2
local RS_HOLD2 = 4
local RS_BUY = 4          -- candidates bought a look at most (upstream's buys < 4)
local RS_FIELDED = 1
local RS_WAIT = 30000
local RS_LONG = 300000

local ECON, LINE, EPOCH, REST = 1, 2, 3, 4

-- the buildings of upstream's `home` (a tech they research is rank 1)
local HOME = {b_town_center = true, b_capitol = true, b_university = true, b_hospital = true,
              b_granary = true, b_granary_improved = true}
-- the gathering techs (econ.lua's list, is_gather_tech): they come before the other rank-1 techs because they pay back
local GATHER = {hafts_tech = true, hand_axe_tech_from_town_center = true, fine_flaked_tools = true,
                hardened_chisel_tech = true, shaft_mining_tech = true, saw_tech = true, irrigation = true,
                wheeled_drawn_plow = true, mining_explosives_tech = true, granary_upgrade = true,
                hydraulic_mining_tech = true, core_mining_tech = true}
-- the other buildings of a temple's techs (a temple, a mosque, an orthodox church)
local TEMPLE = {b_temple = true, x_mosque = true, x_orthodox_church = true}
-- the improvements the port's improve verb can buy (verbs.lua WHAT) and the engine's attribute numbers (dbupgrade's
-- factors: 1 attack, 9 hit points, 11 range, 14 speed)
local WHAT = {"range", "attack", "hit_points", "speed"}
local ATTR = {range = 11, attack = 1, hit_points = 9, speed = 14}

local function zero()
    local r = {}
    for _, k in ipairs(V.resources) do
        r[k] = 0
    end
    return r
end

local function costmap(c)
    local r, sum = {}, 0
    for _, k in ipairs(V.resources) do
        local v = c ~= nil and (c[k] or 0) or 0
        r[k] = v
        sum = sum + v
    end
    return r, sum
end

local function pays(avail, cost)
    if avail == nil or cost == nil then
        return false
    end
    for _, k in ipairs(V.resources) do
        if (cost[k] or 0) > 0 and (avail[k] or 0) < cost[k] then
            return false
        end
    end
    return true
end

local function add_cost(r, cost)
    for _, k in ipairs(V.resources) do
        r[k] = (r[k] or 0) + (cost[k] or 0)
    end
end

local function state(memory)
    memory.rs = memory.rs or {t = 0, scan_t = 0, ep = -1, cand = {}, imps = {}, imp_t = 0, done = {}, wait = {},
                              hold = zero(), imp_cap = {}, pending = {}, log_t = 0, bought = {0, 0, 0, 0, 0},
                              improved = 0, looks = 0}
    local rs = memory.rs
    rs.pending = rs.pending or {}
    rs.hold = rs.hold or zero()
    for _, k in ipairs(V.resources) do
        rs.hold[k] = rs.hold[k] or 0
    end
    return rs
end

-- the player's index for the hash (upstream `idx`): ai.me is the player id, not the slot; the hash only has to be the
-- same on every peer, so the id serves
local function index()
    return (ai.me() or 0) % 16
end

-- upstream rs_cmp's hash: a hash of the player, the type and the attribute (0..0xFFFFFF)
local function type_number(t)
    local h = 0
    for i = 1, #t do
        h = (h * 31 + string.byte(t, i)) & 0xFFFFFFFF
    end
    return h
end

local function hash(idx, t, attr)
    local h = (idx * 7919 + t * 131 + attr * 2654435761) & 0xFFFFFFFF
    return (h >> 8) & 0xFFFFFF
end

-- the finished building of its that researches the tech (rs_has_maker)
local function maker_finished(view, producers)
    for _, p in ipairs(producers or {}) do
        local b = V.buildings(view, p)
        if b ~= nil and b.finished > 0 then
            return p
        end
    end
    return nil
end

local function is_epoch_tech(tn)
    return tn.name ~= nil and string.sub(tn.name, 1, 6) == "epoch_"
end

-- the tech candidates of the tree: what it may still buy this epoch or an earlier one (research_tick's loop)
local function scan_techs(view, rs)
    local cand = {}
    local list = ai.techs{epoch = view.epoch}
    for _, tn in ipairs(list) do
        if tn.available and not tn.researched and not tn.obsolete and tn.makes == nil and tn.kind == 0
                and not is_epoch_tech(tn) and not rs.done[tn.name] then
            local cost, sum = costmap(tn.cost)
            if sum > 0 and maker_finished(view, tn.producers) ~= nil then
                local home, temple = false, false
                for _, p in ipairs(tn.producers or {}) do
                    if HOME[p] then
                        home = true
                    elseif TEMPLE[p] then
                        temple = true
                    end
                end
                local rank, score
                if home then
                    rank = ECON
                    score = (GATHER[tn.name] and 0 or 100000) + sum
                elseif temple then
                    rank = REST   -- the temple's other techs (its range ones are not distinguishable: the effect list
                    score = sum   -- is not exposed - a gap, see the header)
                else
                    rank = REST
                    score = sum
                end
                cand[#cand + 1] = {kind = "tech", t = tn.name, id = tn.id, rank = rank, score = score, cost = cost,
                                   sum = sum}
            end
        end
    end
    return cand
end

-- the improvement candidates of the types it fields (rs_improvements)
local function scan_improvements(view, rs)
    local out = {}
    local ids = ai.units{}
    local cols = ai.get(ids, {"type"})
    local cnt = {}
    for k = 1, #ids do
        local t = cols.type[k]
        if t ~= nil then
            cnt[t] = (cnt[t] or 0) + 1
        end
    end
    local types = {}
    for t in pairs(cnt) do
        types[#types + 1] = t
    end
    table.sort(types)   -- pairs' order is not the same on every peer; the list's order must be
    local idx = index()
    for _, t in ipairs(types) do
        local n = cnt[t]
        local wall = string.sub(t, 1, 7)
        if n >= RS_FIELDED and t ~= "citizen" and t ~= "female_citizen" and wall ~= "b_wall_" and wall ~= "b_gate_"
                and not rs.imp_cap[t] then
            for _, what in ipairs(WHAT) do
                local ic = ai.improve_cost(t, what)
                if ic ~= nil and (ic.level or 0) >= 1 and (ic.level or 0) <= (ic.max or 0) then
                    local cost, sum = costmap({food = ic.food, wood = ic.wood, stone = ic.stone, gold = ic.gold,
                                               iron = ic.iron})
                    if sum > 0 then   -- the price above 0: the "+" button the player is offered
                        local first = what == "range"
                        out[#out + 1] = {kind = "improve", t = t, what = what, cost = cost, sum = sum,
                                         level = ic.level, max = ic.max, first = first,
                                         rank = first and LINE or REST,
                                         score = first and -n or hash(idx, type_number(t), ATTR[what])}
                    end
                end
            end
        end
    end
    return out
end

local function cmp(a, b)
    if a.rank ~= b.rank then
        return a.rank < b.rank
    end
    if a.score ~= b.score then
        return a.score < b.score
    end
    return tostring(a.t or a.what) < tostring(b.t or b.what)
end

-- what a rank 1 or 2 order may spend: the stock above the engine's reserve (the epoch's and the towns'; view.reserve)
function M.avail(view)
    local r = {}
    for _, k in ipairs(V.resources) do
        r[k] = math.max((view.stock[k] or 0) - ((view.reserve or {})[k] or 0), 0)
    end
    return r
end

-- the price of what rank 1 and 2 still wait for (a resource map), for the priority list's "research" row and for the
-- lower ranks' own spending
function M.hold(memory)
    local rs = memory.rs
    return rs ~= nil and rs.hold or zero()
end

-- the result of the orders the look before (the port's convention: view.results of the previous step's orders; an
-- index is the order's place in that step's list, checked against the verb)
local function feedback(view, rs)
    for _, p in ipairs(rs.pending) do
        local r = p.at ~= nil and view.results[p.at] or nil
        if r ~= nil and r.verb == p.verb then
            if p.kind == "tech" then
                if r.code ~= "queued" and r.code ~= "done" then
                    if r.reason == "unknown_class" or r.reason == "bad_argument" then
                        rs.done[p.t] = true
                    elseif r.reason == "not_in_epoch" or r.reason == "requirements" or r.reason == "no_producer" then
                        rs.wait[p.t] = view.time + RS_LONG
                    else   -- busy, cannot_afford: the tree will say when it is in
                        rs.wait[p.t] = view.time + RS_WAIT
                    end
                end
            else
                if r.code ~= "done" then
                    if r.reason == "cannot_afford" or r.reason == "busy" then
                        rs.wait[p.t .. "|" .. p.what] = view.time + RS_WAIT
                    else
                        rs.imp_cap[p.t] = (rs.imp_cap[p.t] or 0) + 1   -- not offered, or the points pot is full
                    end
                end
            end
        end
    end
    rs.pending = {}
end

-- -- one look ------------------------------------------------------------------------------------------------------
function M.step(view, memory, orders)
    local rs = state(memory)
    feedback(view, rs)
    if view.time - (rs.t or 0) < RS_EVERY and (rs.t or 0) > 0 then
        return
    end
    if view.time < (rs.t or 0) then
        rs.t = 0   -- a game before
    end
    rs.t = view.time > 0 and view.time or 1
    rs.looks = (rs.looks or 0) + 1
    rs.hold = zero()
    -- the candidates: the tech tree (cached) and the improvements of the types it fields (cached)
    if rs.scan_t == 0 or view.time < rs.scan_t or view.time - rs.scan_t >= RS_SCAN or rs.ep ~= view.epoch then
        rs.scan_t = view.time > 0 and view.time or 1
        rs.ep = view.epoch
        rs.cand = scan_techs(view, rs)
    end
    if (S.unit_improvements or 1) ~= 0 then
        if rs.imp_t == 0 or view.time < rs.imp_t or view.time - rs.imp_t >= RS_IMP then
            rs.imp_t = view.time > 0 and view.time or 1
            rs.imps = scan_improvements(view, rs)
        end
    else
        rs.imps = {}
    end
    -- the two lists in one order (rs_cmp): rank 1, the range improvements and the other line upgrades (none here),
    -- the other improvements and techs
    local list = {}
    for _, c in ipairs(rs.cand) do
        if not c.used and (rs.wait[c.t] or 0) <= view.time then
            list[#list + 1] = c
        end
    end
    for _, c in ipairs(rs.imps) do
        if not c.used and (rs.wait[c.t .. "|" .. c.what] or 0) <= view.time then
            list[#list + 1] = c
        end
    end
    table.sort(list, cmp)
    -- ranks 1 and 2, with the price of what they wait for held back from the lower ranks (and from everyone through
    -- M.hold; priority.lua's "research" row)
    local avail = M.avail(view)
    local buys = 0
    local waits1, waits2 = 0, 0
    for _, c in ipairs(list) do
        if c.rank > LINE then
            break
        end
        if buys < RS_BUY then
            local ok = pays(avail, c.cost)
            if ok and c.kind == "tech" then
                orders[#orders + 1] = {verb = "research", tech = c.t}
                rs.pending[#rs.pending + 1] = {at = #orders, verb = "research", kind = "tech", t = c.t}
                rs.wait[c.t] = view.time + RS_WAIT
            elseif ok then
                orders[#orders + 1] = {verb = "improve", type = c.t, what = c.what}
                rs.pending[#rs.pending + 1] = {at = #orders, verb = "improve", kind = "improve", t = c.t, what = c.what}
                rs.wait[c.t .. "|" .. c.what] = view.time + RS_WAIT
            end
            if ok then
                buys = buys + 1
                c.used = true
                rs.bought[1] = rs.bought[1] + (c.kind == "tech" and 1 or 0)
                rs.improved = rs.improved + (c.kind == "improve" and 1 or 0)
                for _, k in ipairs(V.resources) do
                    avail[k] = math.max(avail[k] - (c.cost[k] or 0), 0)
                end
            end
        end
        if not c.used then   -- not bought: the next RS_HOLD1 / RS_HOLD2 hold their price
            local n = c.rank == ECON and waits1 or waits2
            if n < (c.rank == ECON and RS_HOLD1 or RS_HOLD2) then
                add_cost(rs.hold, c.cost)
                if c.rank == ECON then
                    waits1 = waits1 + 1
                else
                    waits2 = waits2 + 1
                end
            end
        end
    end
    -- rank 4: with what no row of the priority list holds back (the epoch's among them), less what 1 and 2 wait for
    local left = P.visible(view, memory, P.S_PLAN)
    for _, k in ipairs(V.resources) do
        left[k] = math.max((left[k] or 0) - rs.hold[k], 0)
    end
    for _, c in ipairs(list) do
        if c.rank == REST and buys < RS_BUY then
            if pays(left, c.cost) then
                if c.kind == "tech" then
                    orders[#orders + 1] = {verb = "research", tech = c.t}
                    rs.pending[#rs.pending + 1] = {at = #orders, verb = "research", kind = "tech", t = c.t}
                    rs.wait[c.t] = view.time + RS_WAIT
                    rs.bought[REST] = rs.bought[REST] + 1
                else
                    orders[#orders + 1] = {verb = "improve", type = c.t, what = c.what}
                    rs.pending[#rs.pending + 1] = {at = #orders, verb = "improve", kind = "improve", t = c.t,
                                                   what = c.what}
                    rs.wait[c.t .. "|" .. c.what] = view.time + RS_WAIT
                    rs.improved = rs.improved + 1
                end
                buys = buys + 1
                c.used = true
                for _, k in ipairs(V.resources) do
                    left[k] = math.max(left[k] - (c.cost[k] or 0), 0)
                end
            end
        end
    end
    -- the log (upstream's minute line)
    if rs.log_t == 0 or view.time < rs.log_t or view.time - rs.log_t >= 60000 then
        rs.log_t = view.time > 0 and view.time or 1
        local h = rs.hold
        print(string.format("research epoch=%d candidates=%d imps=%d held=%d/%d/%d/%d/%d bought=%d improvs=%d looks=%d",
            view.epoch, #rs.cand, #rs.imps, h.food, h.wood, h.stone, h.gold, h.iron, rs.bought[1] + rs.bought[REST],
            rs.improved, rs.looks))
    end
end

-- a new game (research_new_game): nothing bought, nothing held; a tech bought before is in the tree
function M.clear(memory)
    memory.rs = nil
end

return M
