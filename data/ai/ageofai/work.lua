-- Age of AI port: upstream dll/ai/work.c (THE CITIZENS' WORK: the worker plan's need model, the army's appetite, the
-- citizen plan's goal and caps, the mine crowds, the food plan's pull and feed, the wood drop site, the herds) and the
-- economy parts of the diff 0ac6495..HEAD in econ.c, claims.c, food.c, citizens.c and mines.c. UPSTREAM has the map and
-- the licence: GPL-3.0, Grille and the Age of AI contributors. require("ageofai.work"), hooked from econ.lua's step.
--
-- Upstream's own economy (work.c) is played while the game's computer player is off, its citizens sent one by one
-- (resource_mgr_assign). This port steers the coarse verbs: the gather order sets what each resource wants and the
-- runner (lib.verbs) picks the citizen and the site. So what is ported here is what a verb can express - the worker
-- plan's numbers by what the economy spends (wk_needs), the citizen plan's caps (citizens.c), the mine crowds and
-- mine_choice's free mine (sites.c), the food plan's PULL and FEED (food.c, citizens.c), the wood drop site
-- (mines.c), the rebalance (econ.lua's gather orders' exact) - plus one order the verbs cannot: mine_crowds' citizen
-- moves, ai.gather with the site's own id.
--
-- Deviations (the engine's limits, each cited where it is):
--   the territories of work.c (WHERE, ITS TERRITORY, wk_cost, the retreat, claim_from_towers) do not exist for a
--   script: the view's claim range (45 tiles, below LOCAL_MINE_FAR 60) already keeps the work by the town, and the
--   sites' `blocked` is the engine's own threat test (an armed building remembered, the way from its drop site), which
--   is wk_danger's job in work.c (ENGINE, sites.c). HERDS KEPT needs the animals' herds and their growth, which no
--   record carries (an animal's hp is not its scale): an engine gap.
--   food_plan count_trained is econ.lua's own rule (the food of the citizens the Town Center trains in the plan);
--   back_to_food (PULL), forage_near, hunt_near, no_fields_wild and wild_settlement are diet/hunt rules of the
--   engine's food runner (the finders) except PULL, which is here. citizen_plan's mine_slots is econ.lua's
--   citizens_wanted (already), food_pays is FEED here, mine_settlement and before_reserve are here. workforce_plan's
--   army_room is army.c's (army.lua, metal.lua) and replace_refused is metal.lua's MIX; what_is_used and wood_spent are
--   here. army_plan's top_up, room_soldiers and more_buildings are the army's account (econ.lua's M.accounts, army.lua's
--   training): not this file.
local V = require("lib.view")
local S = require("ageofai.settings")
-- [claims.c] the units' order claims, when the tree has the module (work.c's hand-outs ask first: CLP_ROUTINE / CLW_WORK)
local okC, CL = pcall(require, "ageofai.claims")
if not okC then
    CL = nil
end
local M = {}

local FARM_EPOCH = 2                 -- the Copper Age: farms, the mines' slots, the citizen plan
local MINERALS = {"stone", "gold", "iron"}
local WIN = 10                       -- WK_WIN: the last ten minutes of buckets (wk_needs)
local ARMY_MS = 30000                -- WK_ARMY_MS: a soldier's training, about (the army's appetite)
local ARMY_BANK = 400                -- WK_ARMY_BANK: this much of a resource above the reserve needs no soldiers
local PER_CIT = {food = 20, wood = 20, stone = 15, gold = 15, iron = 15}   -- WK_PER_CIT, until 3 citizen-minutes
local CIT_MS = 28000                 -- a citizen takes ~28 s of a producer (docs/economy.md "Citizen production")
local NEED_FLOOR = 4                 -- wk_needs: never under this percent of all that is needed
local WOOD_RATE_MIN = 5              -- wf_tick: the wood rate per worker at least 5 a minute
local CROWD_MAX = 20                 -- mine_crowds: the citizens moved a look at most
local CROWD_EVERY = 10000            -- ... every 10 s
local PULL_MAX = 4                   -- food_plan PULL: gatherers back to food every 10 s at most
local LOCAL_MINE_FAR = 60            -- sites.c LOCAL_MINE_FAR: a mine's drop site this far from the citizen is another's
local WOOD_CUTTERS = 8               -- mines.c wood_drops: woodcutters at trees 15+ tiles from a drop site ...
local WOOD_WALK = 15                 -- ...

-- the [ai_base] keys that are a list of parts apart by commas or '|' ("mine_slots, food_pays"; empty = none). The
-- bits are upstream's order; a number is read as its bits, a plain true as every part (the port's settings may hold
-- either a list or the ini's bits).
local PARTS = {
    food_plan = {"count_trained", "back_to_food", "forage_near", "hunt_near", "no_fields_wild", "wild_settlement"},
    citizen_plan = {"mine_slots", "food_pays", "mine_settlement", "before_reserve"},
    mine_fill = {"fill_slots", "mine_settlement", "mines_first"},
    workforce_plan = {"what_is_used", "wood_spent", "army_room", "replace_refused"},
}
local DEFAULTS = {
    food_plan = "count_trained, back_to_food",   -- upstream's default (food_plan=count_trained, back_to_food)
    citizen_plan = "mine_slots, food_pays",      -- (citizen_plan=mine_slots, food_pays)
    mine_fill = "",                              -- (mine_fill=, off; its mines_first became mine_places_first)
    workforce_plan = "",                         -- (workforce_plan=, off)
}
local sParsed = {}

local function clamp(v, lo, hi)
    if hi ~= nil and v > hi then
        v = hi
    end
    if lo ~= nil and v < lo then
        v = lo
    end
    return v
end

-- the part of a list setting in force; nil key: the key is not in settings.lua yet
function M.has(key, part)
    local src = S[key]
    if src == nil then
        src = DEFAULTS[key]
    end
    local c = sParsed[key]
    if c ~= nil and c.src == src then
        return c.set[part] == true
    end
    local set = {}
    if type(src) == "string" then
        for name in string.gmatch(src, "[^,|%s]+") do
            set[name] = true
        end
    elseif type(src) == "table" then
        for k, v in pairs(src) do
            if type(k) == "number" then
                set[tostring(v)] = true
            elseif v then
                set[k] = true
            end
        end
    elseif type(src) == "number" then
        for k, name in ipairs(PARTS[key] or {}) do
            if (src & (1 << (k - 1))) ~= 0 then
                set[name] = true
            end
        end
    elseif src == true then
        for _, name in ipairs(PARTS[key] or {}) do
            set[name] = true
        end
    end
    sParsed[key] = {src = src, set = set}
    return set[part] == true
end

-- the facts of a game: the gatherers a mine takes (the game's table 0x16, 6; ai.tuning's mining_limit), the citizen's
-- price and the finished buildings that train citizens (citizens.c cz_producers: a Town Center, a Capitol)
local function facts(view, memory)
    memory.work = memory.work or {}
    local w = memory.work
    if w.mine_cap == nil then
        local n = (ai.tuning() or {}).mining_limit or 0
        w.mine_cap = (n >= 1 and n <= 32) and n or 6
    end
    local c = V.cost(view, "citizen")
    w.cit_food = c ~= nil and c.cost.food or 50
    local producers = 0
    for _, t in ipairs({"b_town_center", "b_capitol"}) do
        local b = V.buildings(view, t)
        if b ~= nil then
            producers = producers + b.finished
        end
    end
    w.producers = producers
    return w
end

-- ---------------------------------------------------------------------------------------------------------------
-- The food producers' need (cz_nonstop_food) and the measured rates

-- the food a minute that keeps every finished citizen producer training without a pause (cz_nonstop_food)
local function nonstop_food(w)
    return w.producers * w.cit_food * 60 / (CIT_MS / 1000)
end

-- the measured food a gatherer brings a minute (at least 5: cz_feed's floor for the rate per worker)
local function food_rate(view)
    local g = view.gatherers.food or 0
    if g <= 0 then
        return 5
    end
    return math.max((view.income.food or 0) // g, 5)
end

-- the wood spent a minute over the last five buckets (wf_tick: the income less the stock's growth, over 5 minutes)
local function wood_spent(nd)
    if nd == nil or nd.n <= 0 then
        return 0
    end
    local n = math.min(nd.n, 5)
    local s = 0
    for k = 1, n do
        s = s + (nd.spend[k].wood or 0)
    end
    return s / n
end

-- the measured wood a gatherer brings a minute (at least 5: wf_tick's floor)
local function wood_rate(view)
    local g = view.gatherers.wood or 0
    if g <= 0 then
        return WOOD_RATE_MIN
    end
    return math.max((view.income.wood or 0) // g, WOOD_RATE_MIN)
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE NEEDS (work.c wk_needs)

-- THE ARMY'S APPETITE (wk_needs): what its land production buildings would spend training, without a pause, the
-- soldier the army asks for (the most requested), one each ARMY_MS; gold and iron at least half the type's metal each
-- (the next type may cost the other). Nothing under min_workforce: the citizens first (upstream's measurement).
local function appetite(view)
    local a = {food = 0, wood = 0, stone = 0, gold = 0, iron = 0}
    if view.citizens < (S.min_workforce or 40) then
        return a
    end
    local MT = require("ageofai.metal")   -- [metal.c] econ.c land_soldier
    local pick, most = nil, 0
    for _, c in ipairs(view.costs) do
        local n = c.requested + c.in_production
        if c.military and not c.building and c.producer ~= nil and n > most and MT.land_soldier(c) then
            pick, most = c, n
        end
    end
    if pick == nil then
        return a
    end
    local b = V.buildings(view, pick.producer)
    local nb = b ~= nil and b.finished or 0
    if nb == 0 then
        return a
    end
    local per = nb * 60000 / ARMY_MS
    local metal = (pick.cost.gold + pick.cost.iron) * per * 0.5
    for _, r in ipairs(V.resources) do
        a[r] = pick.cost[r] * per
    end
    for _, r in ipairs({"gold", "iron"}) do
        if a[r] < metal then
            a[r] = metal
        end
    end
    return a
end

-- the minute's bucket of THE NEEDS: what came in of each resource (the income the budget counts), what went out (that
-- less the bank's change) and how many citizens stood on it (the mean of the looks)
local function bucket(view, nd)
    if view.time - nd.t < 60000 then
        return
    end
    local k = nd.cur
    for _, r in ipairs(V.resources) do
        local inc = (view.income_total[r] or 0) - (nd.inc0[r] or 0)
        local out = inc - (view.stock[r] - nd.stock0[r])
        nd.inc[k][r] = inc > 0 and inc or 0
        nd.spend[k][r] = out > 0 and out or 0
        nd.cm[k][r] = nd.samples > 0 and nd.acc[r] // nd.samples or 0
        nd.inc0[r], nd.stock0[r], nd.acc[r] = view.income_total[r] or 0, view.stock[r], 0
    end
    nd.samples = 0
    nd.cur = k % WIN + 1
    nd.t = view.time
    if nd.n < WIN then
        nd.n = nd.n + 1
    end
end

-- THE NEEDS: what each resource needs a minute - what was spent (the last WIN minutes' mean), the reserve's shortfall
-- over five minutes, the army's appetite, half as much again while the bank has under 150 (spending follows income),
-- a half or a fifth while it piles up - and the citizens that brings, from what a citizen brings (its measured rate,
-- WK_PER_CIT until three citizen-minutes). Nil until three minutes are sampled. ctx.reserve: the reserve in force
-- (econ.lua passes the civilians').
function M.needs(view, memory, ctx)
    memory.work = memory.work or {}
    local w = memory.work
    local nd = w.nd
    if nd == nil or nd.n < 3 then
        return nil
    end
    if nd.look_t == view.time and nd.cits ~= nil then
        return nd.cits   -- (the look's numbers: W.step computed them before the plan asked again)
    end
    local reserve = (ctx ~= nil and ctx.reserve) or view.reserve or {}
    local arm = appetite(view)
    local need, cits, total = {}, {}, 0
    for _, r in ipairs(V.resources) do
        local sum_in, sum_out, cm = 0, 0, 0
        for k = 1, nd.n do
            sum_in = sum_in + (nd.inc[k][r] or 0)
            sum_out = sum_out + (nd.spend[k][r] or 0)
            cm = cm + (nd.cm[k][r] or 0)
        end
        nd.sum_in[r], nd.sum_out[r] = sum_in, sum_out
        local pc = (cm >= 3 and sum_in > 0) and sum_in // cm or PER_CIT[r]
        nd.per_cit[r] = math.floor(clamp(pc, 3, 80))
        local spend = sum_out // nd.n
        local n = spend
        if (reserve[r] or 0) > view.stock[r] then
            n = n + (reserve[r] - view.stock[r]) // 5
        end
        if r == "food" then
            -- the producers' food: what keeps every producer training without a pause, and the army's
            if view.stock.food - (reserve.food or 0) > ARMY_BANK then
                arm.food = 0
            end
            local floor_ = math.floor(nonstop_food(w) + (arm.food or 0))
            if n < floor_ then
                n = floor_
            end
        end
        -- enough of it lies there: another resource is what its soldiers wait for
        if view.stock[r] - (reserve[r] or 0) > ARMY_BANK then
            arm[r] = 0
        end
        if r ~= "food" and n < (arm[r] or 0) then
            n = arm[r]
        end
        if view.stock[r] < 150 then
            n = n * 1.5
        end
        if view.stock[r] > 4000 + 15 * spend then
            n = n * 0.2
        elseif view.stock[r] > 1500 + 5 * spend then
            n = n * 0.5
        end
        need[r] = math.floor(n)
        total = total + need[r]
    end
    if total <= 0 then
        return nil
    end
    for _, r in ipairs(V.resources) do
        -- what it never had it never spent: never under 4% of all that is needed
        local floor_ = total * NEED_FLOOR // 100
        if need[r] < floor_ then
            need[r] = floor_
        end
        cits[r] = math.floor(need[r] / nd.per_cit[r])
    end
    nd.need, nd.cits, nd.total, nd.reserve = need, cits, total, reserve
    nd.look_t = view.time
    return cits
end

-- the split's weights of the worker plan: the citizens THE NEEDS wants (econ.lua's plan uses them when it has three
-- minutes of them; S.work_needs)
function M.weights(view, memory, ctx)
    if not S.work_needs then
        return nil
    end
    return M.needs(view, memory, ctx)
end

-- FEED (citizen_plan food_pays; cz_feed): the food gatherers that pay the producers the citizens still missing need -
-- one producer per ten missing, at most the producers there are - at the measured food per gatherer
function M.feed(view, memory, missing)
    if not M.has("citizen_plan", "food_pays") or view.epoch < FARM_EPOCH or missing <= 0 then
        return 0
    end
    local w = facts(view, memory)
    local np = math.min(w.producers, (missing + 9) // 10)
    if np <= 0 then
        return 0
    end
    return math.ceil(np * w.cit_food * 60 / (CIT_MS / 1000) / food_rate(view))
end

-- FIRST (citizen_plan before_reserve; cz_visible_food): the citizens come before the epoch's reserve while the food
-- income pays both it and the producers (twice the food that keeps them training)
function M.citizen_food(view, memory, stock, visible, goal, citizens)
    if not M.has("citizen_plan", "before_reserve") or view.epoch < 1 then
        return visible
    end
    if (goal or 0) ~= 0 and (citizens or 0) >= goal then
        return visible
    end
    local w = facts(view, memory)
    if (view.income.food or 0) >= 2 * nonstop_food(w) then
        return math.max(stock, visible)
    end
    return visible
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE CITIZEN PLAN's caps (citizens.c citizen_mine_cap, workforce_plan WHAT IT IS USED)

-- the citizens wanted after the caps: WORKFORCE PLAN CAP (what the economy uses: the mines' slots or their gatherers,
-- the food gatherers at least the fields + 2, the woodcutters the wood actually spent needs, 4 builders, a margin of
-- cap / 40; after five minutes of samples, never under 30) and MINE CAP (at most half the cap and the mineral mines
-- known with something left x the gatherers a mine takes x 1.5). want is econ.lua's citizens_wanted. ctx: places (the
-- claimed mines' places by resource near a drop site), fields, target
function M.cap(view, memory, want, ctx)
    memory.work = memory.work or {}
    ctx = ctx or {}
    local w = facts(view, memory)
    local nd = w.nd
    local before = want
    -- WORKFORCE CAP
    if M.has("workforce_plan", "what_is_used") and view.epoch >= FARM_EPOCH and nd ~= nil and nd.n >= 5 then
        local spent = wood_spent(nd)
        local need_wood = math.ceil(spent * 1.25 / wood_rate(view) + 0.99) + 3
        local metal = 0
        for _, r in ipairs(MINERALS) do
            metal = metal + math.max((ctx.places or {})[r] or 0, view.gatherers[r] or 0)
        end
        local food = math.max(view.gatherers.food or 0, (ctx.fields or 0) + 2)
        local margin = math.max(view.population_cap // 40, 4)
        local used = metal + food + need_wood + 4 + margin
        if used < 30 then
            used = 30
        end
        if want > used then
            want = used
            w.wf_held = (w.wf_held or 0) + (before - used)
        end
    end
    -- MINE CAP. The port knows only the sites within the claim range (45 tiles; upstream's zones hold the whole map's
    -- mines), so the ports' cap comes out far under upstream's on a small map (6 known mines x 9 = 54 citizens): off
    -- unless settings.lua says otherwise (see the report)
    if S.citizen_mine_cap == true and view.epoch >= FARM_EPOCH then
        if w.mine_t == nil or view.time - w.mine_t >= 30000 then
            w.mine_t = view.time
            -- the mineral mines with something left that it knows: the sites' resource units (view.sites is the claim
            -- range's centres, upstream's zones hold the whole map's mines: a deviation, the count is smaller here)
            local mines = 0
            for _, s in ipairs(view.sites) do
                if (s.resource == "stone" or s.resource == "gold" or s.resource == "iron") then
                    mines = mines + (s.units or 0)
                end
            end
            w.mine_n = mines
            local lim = view.population_cap // 2
            local by_mines = mines * w.mine_cap * 3 // 2
            if by_mines < lim then
                lim = by_mines
            end
            w.mine_lim = mines > 0 and lim or nil
        end
        if w.mine_lim ~= nil and want > w.mine_lim then
            want = w.mine_lim
        end
    end
    if want ~= (w.cap_log or -1) and view.time - (w.cap_t or -100000) >= 60000 then
        w.cap_t = view.time
        w.cap_log = want
        print(string.format("work citizens want=%d econ=%d cap=%d mines=%d lim=%d held=%d",
            want, before, view.population_cap, w.mine_n or 0, w.mine_lim or -1, w.wf_held or 0))
    end
    return want
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE MINE CROWDS and mine_choice (sites.c mine_crowds, select_mine_near_drop)

-- a free mine of a resource for a citizen at (cx, cy): something left, room under the cap (the gatherers sent count,
-- walking and waiting), a finished drop site at hand, no enemy or known tower, on the citizen's own land. mine_choice:
-- nearest a drop site (here: a site with a drop at hand, then the town's measure; local_mines/town_work: nearest the
-- citizen)
local function free_mine(view, sites, res, cx, cy, g, cap)
    local local_ = S.local_mines or S.town_work
    local best, bkey = nil, nil
    for _, s in ipairs(sites) do
        if s.resource == res and (s.units or 0) > 0 and (g[s.id] or 0) < cap and s.has_drop
                and s.blocked ~= "tower" and s.blocked ~= "enemy_near" and (s.danger or 0) == 0
                and ai.reach(cx, cy, s.x, s.y) then
            local d = local_ and ((s.x - cx) * (s.x - cx) + (s.y - cy) * (s.y - cy)) or (s.distance or 0)
            if not local_ or d <= LOCAL_MINE_FAR * LOCAL_MINE_FAR then
                local key = d * 100000 + (s.index or 0)
                if bkey == nil or key < bkey then
                    best, bkey = s, key
                end
            end
        end
    end
    return best
end

-- MINE CROWDS: a mine takes mine_cap gatherers, those walking and waiting included (ai.gatherers counts every citizen
-- whose gather goal is that site; the view's workers only those at work). Every 10 s the citizens beyond a mine's
-- places go to the nearest free mine of the kind, the farthest from the mine first (the walkers before the waiters), at
-- most 20 a look; none free: they wait. The runner (lib.verbs' shed) sheds the crowds it lists; this also takes the
-- ones the game sent, and sends them to a mine of their kind rather than to any work.
function M.mine_crowds(view, memory)
    if S.mine_crowds == false or S.mine_choice == false or view.time - ((memory.work or {}).crowd_t or 0) < CROWD_EVERY then
        return
    end
    memory.work = memory.work or {}
    local w = memory.work
    w.crowd_t = view.time
    local cap = facts(view, memory).mine_cap
    local sites, over = {}, {}
    for _, s in ipairs(view.sites) do
        if (s.units or 0) > 0 and (s.resource == "stone" or s.resource == "gold" or s.resource == "iron") then
            sites[#sites + 1] = s
        end
    end
    if #sites == 0 then
        return
    end
    local g = ai.gatherers()
    for _, s in ipairs(sites) do
        local n = math.max(g[s.id] or 0, s.workers or 0)
        if n > cap then
            over[#over + 1] = {site = s, n = n}
        end
    end
    if #over == 0 then
        return
    end
    -- the citizens at each over-full mine, farthest from it first
    local at = {}
    local ids = ai.units{category = "citizen"}
    if #ids > 0 then
        local cols = ai.get(ids, {"target", "x", "y"})
        for k, id in ipairs(ids) do
            local t, x, y = cols.target[k], cols.x[k], cols.y[k]
            if t ~= nil and x ~= nil then
                local l = at[t]
                if l == nil then
                    l = {}
                    at[t] = l
                end
                l[#l + 1] = {id = id, x = x, y = y, d = 0}
            end
        end
    end
    local moved = 0
    for _, o in ipairs(over) do
        local left = o.n - cap
        local list = at[o.site.id]
        if list ~= nil then
            for _, c in ipairs(list) do
                c.d = (c.x - o.site.x) * (c.x - o.site.x) + (c.y - o.site.y) * (c.y - o.site.y)
            end
            table.sort(list, function(a, b)
                if a.d ~= b.d then
                    return a.d > b.d
                end
                return a.id < b.id
            end)
            for _, c in ipairs(list) do
                if left <= 0 or moved >= CROWD_MAX then
                    break
                end
                -- [claims.c] another's claim at its own or a higher priority stands (the citizens' safety's, the
                -- militia's): work asks only, as upstream's cl_may does
                if CL == nil or CL.may(view, memory, c.id, CL.PRIO.routine, CL.WHO.work) then
                    local to = free_mine(view, sites, o.site.resource, c.x, c.y, g, cap)
                    if to == nil then
                        break
                    end
                    local code = ai.gather({c.id}, to.id)
                    if code == "ok" then
                        moved = moved + 1
                        left = left - 1
                        g[to.id] = (g[to.id] or 0) + 1
                        g[o.site.id] = math.max((g[o.site.id] or 1) - 1, 0)
                    end
                end
            end
        end
    end
    if moved > 0 then
        w.crowd_moved = (w.crowd_moved or 0) + moved
        if view.time - (w.crowd_log_t or -100000) >= 60000 then
            w.crowd_log_t = view.time
            print(string.format("work mine_crowds moved=%d total=%d cap=%d sites=%d", moved, w.crowd_moved, cap, #over))
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The worker plan's adjustments: THE MINES FIRST, WOOD (workforce_plan), PULL (food_plan back_to_food)

-- the mineral resources in the order the free places fill: mine_places_first by each mineral's free places (the
-- largest first: the need), else econ.lua's own order (stone, gold, iron)
function M.mine_order(target, places)
    local order = {}
    for _, r in ipairs(MINERALS) do
        order[#order + 1] = r
    end
    if S.mine_places_first == false then
        return order
    end
    table.sort(order, function(a, b)
        local pa = math.max(((places or {})[a] or 0) - (target[a] or 0), 0)
        local pb = math.max(((places or {})[b] or 0) - (target[b] or 0), 0)
        if pa ~= pb then
            return pa > pb
        end
        return a < b
    end)
    return order
end

-- WOOD (workforce_plan wood_spent; wf_targets): while the wood stock holds three minutes of its spending + 1000, wood
-- wants at most what that spending needs (the last five minutes' spending at the measured rate, + 25% and 3): the rest
-- goes to the metals up to their places, then to food up to its fields + 2; what has nowhere to go stays on wood
function M.wood_target(view, memory, target, ctx)
    if not M.has("workforce_plan", "wood_spent") or view.epoch < FARM_EPOCH then
        return
    end
    ctx = ctx or {}
    local nd = (memory.work or {}).nd
    if nd == nil or nd.n < 5 then
        return
    end
    local spent = wood_spent(nd)
    if view.stock.wood < 3 * spent + 1000 or target.wood <= 0 then
        return
    end
    local need = math.ceil(spent * 1.25 / wood_rate(view) + 0.99) + 3
    if target.wood <= need then
        return
    end
    local extra = target.wood - need
    local moved = 0
    for _, r in ipairs(MINERALS) do
        local places = (ctx.places or {})[r] or 0
        while extra > 0 and target[r] < places do
            target[r] = target[r] + 1
            target.wood = target.wood - 1
            extra = extra - 1
            moved = moved + 1
        end
    end
    local fields = (ctx.fields or 0) + 2
    while extra > 0 and target.food < fields do
        target.food = target.food + 1
        target.wood = target.wood - 1
        extra = extra - 1
        moved = moved + 1
    end
    if moved > 0 then
        memory.work = memory.work or {}
        if view.time - (memory.work.wood_log_t or -100000) >= 60000 then
            memory.work.wood_log_t = view.time
            print(string.format("work wood need=%d target=%d spent=%d moved=%d", need, target.wood, spent, moved))
        end
    end
end

-- PULL (food_plan back_to_food; food_plan_needs PULL): while food wants two or more more than it has, up to four come
-- back to it from the resource farthest above its target (the game never takes a gatherer back; the runner rebalances
-- on the targets). A resource's target never goes under zero here (upstream moves the citizens themselves).
function M.pull(view, memory, target)
    if not M.has("food_plan", "back_to_food") then
        return 0
    end
    local have = view.gatherers
    local pulled = 0
    while pulled < PULL_MAX and target.food - (have.food or 0) >= 2 do
        local from, over = nil, 1
        for _, r in ipairs({"wood", "stone", "gold", "iron"}) do
            local o = (have[r] or 0) - (target[r] or 0)
            if target[r] > 0 and o > over then
                from, over = r, o
            end
        end
        if from == nil then
            break
        end
        target[from] = target[from] - 1
        target.food = target.food + 1
        pulled = pulled + 1
    end
    if pulled > 0 then
        memory.work = memory.work or {}
        memory.work.pulled = (memory.work.pulled or 0) + pulled
    end
    return pulled
end

-- ---------------------------------------------------------------------------------------------------------------
-- The drop sites: the wood drop (mines.c wood_drops), an unworked mine's Settlement (mine_fill / citizen_plan)

-- WOOD DROP (mines.c wood_drops): a Settlement by the forest its woodcutters work when 8 or more of them cut 15+
-- tiles from any drop site (the walk with each load decides the wood income). DEVIATION: the port's wood gatherers work
-- only trees by a finished drop site (the engine's finder), so no cutter can cut 15+ tiles from one - the condition
-- becomes the edge being 15+ tiles out and either 8 woodcutters or no tree by a drop site at all (the port then must
-- put the Settlement there or wood stops). S.wood_drop off: the port's own rule (a Settlement at the edge as soon as
-- wood is wanted).
function M.wood_drop_ok(view)
    if not S.wood_drop then
        return true
    end
    local wd = view.wood
    if wd == nil or (wd.distance or 0) < WOOD_WALK then
        return false
    end
    return (view.gatherers.wood or 0) >= WOOD_CUTTERS or not wd.ready
end

-- MINE SETTLEMENT: a Settlement at the home continent's unworked mine - a mineral site with something left, no drop
-- site and nobody working it, nearest the town, reached over land. The site returned (econ.lua claims it and settles)
function M.mine_settlement(view)
    if not (M.has("mine_fill", "mine_settlement") or M.has("citizen_plan", "mine_settlement")) then
        return nil
    end
    if view.epoch < FARM_EPOCH or view.town_x < 0 then
        return nil
    end
    local best, bd = nil, nil
    for _, s in ipairs(view.sites) do
        if (s.units or 0) > 0 and s.resource ~= "food" and s.resource ~= "wood" and not s.has_drop
                and (s.workers or 0) == 0 and s.blocked ~= "tower" and s.blocked ~= "enemy_near" and (s.danger or 0) == 0
                and ai.reach(view.town_x, view.town_y, s.x, s.y) then
            local d = s.distance or 0
            if bd == nil or d < bd or (d == bd and s.index < best.index) then
                best, bd = s, d
            end
        end
    end
    return best
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look and its line

-- the look (from econ.lua's pass, every 10 s): the minute's bucket of THE NEEDS, the citizens the needs want, the line.
-- budget: econ.lua's budget table (its reserve and target are read) with places (the claimed mines' places by
-- resource) and fields added; the reserve of the step is the civilians' (econ.lua passes its reserve_civ)
function M.step(view, memory, budget)
    memory.work = memory.work or {}
    local w = memory.work
    w.t = view.time
    facts(view, memory)
    local nd = w.nd
    if nd == nil or nd.t == nil or view.time < nd.t or view.time - nd.t > 120000 then
        nd = {t = view.time, cur = 1, n = 0, samples = 0, acc = {}, inc0 = {}, stock0 = {},
              inc = {}, spend = {}, cm = {}, per_cit = {}, need = {}, cits = {}, sum_in = {}, sum_out = {}}
        for k = 1, WIN do
            nd.inc[k], nd.spend[k], nd.cm[k] = {}, {}, {}
        end
        for _, r in ipairs(V.resources) do
            nd.acc[r] = 0
            nd.inc0[r] = view.income_total[r] or 0
            nd.stock0[r] = view.stock[r]
        end
        w.nd = nd
    end
    for _, r in ipairs(V.resources) do
        nd.acc[r] = nd.acc[r] + (view.gatherers[r] or 0)
    end
    nd.samples = nd.samples + 1
    bucket(view, nd)
    local cits = M.needs(view, memory, budget)
    if view.time - (w.log_t or -100000) >= 300000 then
        w.log_t = view.time
        if nd.n > 0 then
            local c = cits or {}
            print(string.format("work needs n=%d food c=%d wood c=%d stone c=%d gold c=%d iron c=%d | mine cap=%d (mines %d, lim %d) | pulled=%d crowds=%d",
                nd.n, math.floor((c.food or 0) + 0.5), math.floor((c.wood or 0) + 0.5), math.floor((c.stone or 0) + 0.5),
                math.floor((c.gold or 0) + 0.5), math.floor((c.iron or 0) + 0.5), facts(view, memory).mine_cap,
                w.mine_n or 0, w.mine_lim or -1, w.pulled or 0, w.crowd_moved or 0))
        end
    end
    return cits
end

return M
