-- Age of AI port, PRODUCTION BY TERRITORY (upstream dll/ai/produce.c; UPSTREAM has the map and the licence, GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.produce"), called once a step from main.lua (see the
-- report).
--
-- The playtester (2026-11-14): "let it place buildings, wit production buildings it should actually prefer frontier
-- territories (that are somehwat established and secured) so units are close to where they are needed. probably bets
-- if territories request buildings based on demand. they say how much units they want (defence, exploration, attack)
-- and neighbouring territoirs give thes units if they a free. if no units a free and must be produced the territory
-- should build the production buildings itself. scaled with distance to existing production, if there is a production
-- hub in a neigbor its fine to use it, if the next prodcution is many land and even sea territories away: lets build
-- up localy".
--
-- SHARES - the territories deal in shares of the whole force (the player: "100% is the full military force and the
-- terriotories distribute that under each other"), not in soldiers. Of its 1000 thousandths:
--   AN ATTACK comes first: a territory with enemy soldiers in it needs PR_MARGIN times their number - converted: that
--   many of the force as a share of it (more than the whole force in all: each its part of the whole, and the look
--   says by how much the force is too small). The size holds PR_HOLD (they step in and out of sight between two looks).
--   THE REST is divided by weight among the territories it holds: DEFENCE, a front's guard (a third of its citizens,
--   3 to 12, 8 with enemy soldiers seen there in the last 5 minutes), EXPLORATION, 1 with a land neighbour nobody is
--   known to hold, ATTACK, beside a territory with enemy buildings it has seen PR_ATTACK and one for each of them,
--   PR_ATTACK_MAX at most.
-- HAS    - the share of the force standing in the territory. Beyond its own share it is free: a neighbour short of
--          its share counts it as its own (the count only, here: who marches where is the armys').
-- SHORT  - the share still missing is what production has to bring there. Times the way to its nearest production
--          building (a finished one, territory by territory: 1 a land territory, PR_SEA onto water, PR_FAR with none
--          anywhere) beyond the first step it is the territory's claim: from PR_LOCAL it builds its own - a hub next
--          door serves whatever is missing ("if there is a production hub in a neigbor its fine to use it"), two
--          territories off a quarter of the force missing builds, three off an eighth, across a sea a twelfth. Only a
--          territory that is established (PR_BLD of its buildings) and secured (no enemy soldier in it, not being
--          lost) builds, and none before it is fed.
-- WHAT   - the first building of a hub: the first of PR_KINDS the game offers (a Barracks). A kind it has nowhere yet
--          follows, in the hub that wants the most - but past its first production building nothing before it is FED
--          (the player 2026-11-08: one production building until 2 farms). And one more of a kind the army plan found
--          busy (econ.lua production's army.producer/army.busy, upstream army.c BUILD): in the hub that wants the most
--          too.
-- "# produce:" a minute.
--
-- DEVIATIONS (the engine and the port):
--   - The territories are the port's front stations (fronts.lua's memory.fronts.stations, every 10 s) plus the home
--     town: this engine has no territory map (territories.c is not ported), so "in a territory" is "within PR_NEAR of
--     its station", the way to the next production is measured in distance steps (PR_STEP tiles) and crossed water
--     adds PR_SEA. A station is a front by construction, so every station counts as one for the defence weight.
--   - EXPLORATION has no weight: the port's scouting is the army's scout group (army.lua, a soldier of the force), not
--     a territory's; the bit stays 0 (a gap: no territory map).
--   - The enemy buildings beside a territory are those the memory knows within PR_REACH of its station (upstream: the
--     neighbour territories', from the territory view's list); walls and gates do not count.
--   - FED is "two granaries, or the food income at what every citizen producer uses without a pause": the train time
--     is not exposed (ai.class has none), so a citizen every 30 s is assumed (the estimate says so in the code).
--   - THE PRODUCTION ITSELF is the engine's: the verbs' request queue and the unit manager pick which producer trains
--     what (docs/AI_LUA_VERBS.md §2), so "which producer trains what" is not a script decision here (a gap), nor is a
--     refused unit replaced by this file (the engine's queue retries it; army.lua substitutes for the land army). The
--     room by pool: M.room (train_room), M.pools and M.room_by_pool are the service a future stations.lua reads; the
--     pool split main / siege / anti-air / explorers is the count of the land force by role.
--   - The building order is the port's build verb at the station's point (near = "tile"): the placement keeps a tile
--     free round every building; no spot: the station is passed over for 5 minutes.
-- Settings: produce (1, the port's own switch; upstream has no key - the file runs while game_ai = 0), train_room
-- (65535, the mask of players whose own orders keep the room under the population cap: M.room), train_by_need (1) and
-- fleet_by_need (0): the army's and the navy's picks (army.lua/metal.lua/navy.lua), not this file's - read here only
-- for the log.
local V = require("lib.view")
local P = require("ageofai.priority")
local F = require("ageofai.fleet")     -- F.kind: the siege by the class name (as metal.lua's)
local G = require("lib.groups")        -- the scout group: the explorers' pool
local S = require("ageofai.settings")
local M = {}

local PR_EVERY = 10000    -- the look (upstream PR_EVERY)
local PR_SEA = 3          -- a step of the way onto water (upstream)
local PR_FAR = 8          -- no production known: this far (upstream)
local PR_LOCAL = 250      -- thousandths of the force missing x steps beyond the first: a claim (upstream)
local PR_HOLD = 60000     -- an attack's size holds this long (upstream)
local PR_MARGIN = 125     -- against an attack: 5/4 of the enemy soldiers there (upstream PR_MARGIN)
local PR_BLD = 3          -- a territory's buildings before it builds (upstream)
local PR_ATTACK = 10      -- beside enemy buildings: this weight ...
local PR_ATTACK_MAX = 30  -- ... and one each, this at most (upstream)
local PR_NEAR = 24        -- DEVIATION: tiles that make a territory (a station's cell)
local PR_REACH = 32       -- enemy buildings counted within this of a station
local PR_STEP = 24        -- DEVIATION: tiles of one "territory step" of the way
local PR_FOE = 300000     -- enemy soldiers seen there this long still weigh for the guard (upstream TV_FOE_MEMORY)
local PR_STAY_OFF = 300000
local PR_KINDS = {"b_barracks", "b_archery_range", "b_stable", "b_siege_factory", "b_tank_factory",
                  "b_cyber_factory"}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function isqrt(v)
    if v <= 0 then
        return 0
    end
    local r = math.floor(math.sqrt(v))
    while r * r > v do
        r = r - 1
    end
    while (r + 1) * (r + 1) <= v do
        r = r + 1
    end
    return r
end

local function is_wall(t)
    local p = string.sub(t or "", 1, 7)
    return p == "b_wall_" or p == "b_gate_"
end

local function state(memory)
    memory.pr = memory.pr or {t = 0, log_t = 0, type = nil, ter = -1, why = nil, first = false, hubs = 0,
                              buildings = 0, force = 0, attack = 0, over = 0, shortn = 0, claims = 0, share = {},
                              has = {}, foe = {}, foe_t = {}, nosite = {}, pools = {main = 0, siege = 0, aa = 0,
                              scout = 0}, more_t = 0, more_off = {}}
    return memory.pr
end

-- the pools of the land force (upstream stations.c: main, siege, anti-air, explorers) - from the look's counts
local function pool_of(t)
    if F.kind(t) == "siege" then
        return "siege"
    end
    if string.sub(t or "", 1, 4) == "b_aa" or string.sub(t or "", 1, 2) == "aa"
            or string.find(t or "", "_aa_", 1, true) ~= nil
            or string.find(t or "", "missile_trooper", 1, true) ~= nil
            or string.find(t or "", "anti_missile", 1, true) ~= nil then
        return "aa"
    end
    return "main"
end

-- the last look's pool counts and shares (thousandths of the land force) -> {main=, siege=, aa=, scout=, total=}
function M.pools(memory)
    local pr = memory.pr
    return pr ~= nil and pr.pools or nil
end

-- the room under the population cap a player's own training orders keep (econ.c train_room): nil when the mask leaves
-- this player unlimited. The engine queues the request and refuses at the cap; a caller caps its n by this.
function M.room(view)
    local mask = S.train_room
    if mask == nil or mask == 65535 then
        return nil
    end
    local bit = (ai.me() or 0) % 16
    if (mask >> bit) & 1 == 0 then
        return nil
    end
    return math.max(view.population_cap - view.population, 0)
end

-- the room by pool: the room the population has, divided by the last look's pool shares (upstream stations.c's
-- pools: main force, siege, anti-air, explorers). nil when train_room leaves this player unlimited.
function M.room_by_pool(view, memory)
    local room = M.room(view)
    local p = M.pools(memory)
    if room == nil or p == nil then
        return nil
    end
    local total = (p.main or 0) + (p.siege or 0) + (p.aa or 0) + (p.scout or 0)
    local out = {}
    for _, k in ipairs({"main", "siege", "aa", "scout"}) do
        out[k] = total > 0 and room * (p[k] or 0) // total or 0
    end
    return out
end

-- the station points of this AI (fronts.lua) plus the home town: a territory each
local function territories(view, memory)
    local out = {}
    local fr = memory.fronts
    for _, st in ipairs(fr ~= nil and fr.stations or {}) do
        out[#out + 1] = {x = st.x, y = st.y, w = st.weight or 1, home = false}
    end
    if view.town_x >= 0 then
        out[#out + 1] = {x = view.town_x, y = view.town_y, w = 1, home = true}
    end
    return out
end

-- the nearest territory of a point (within PR_NEAR), or nil
local function near_ter(ter, x, y)
    local best, bd = nil, PR_NEAR * PR_NEAR
    for k, t in ipairs(ter) do
        local d = d2(x, y, t.x, t.y)
        if d <= bd then
            best, bd = k, d
        end
    end
    return best
end

-- the first PR_KIND the game offers and no row of the priority list holds back
local function first_kind(view, memory)
    for _, t in ipairs(PR_KINDS) do
        if V.cost(view, t) ~= nil and P.refuses(view, memory, t) == nil then
            return t
        end
    end
    return nil
end

-- the food a minute every finished citizen producer uses without a pause (DEVIATION: 30 s a citizen; cz_nonstop_food)
local function nonstop_food(view)
    local c = V.cost(view, "citizen")
    if c == nil then
        return 0
    end
    return (c.producers or 0) * (c.cost.food or 0) * (60000 // 30000)
end

-- -- one look ------------------------------------------------------------------------------------------------------
function M.step(view, memory, orders)
    local pr = state(memory)
    if (S.produce == false) or view.town_x < 0 then
        return
    end
    -- the result of the look before (the port's convention: view.results of the previous step's orders)
    if pr.pending ~= nil then
        local r = view.results[pr.pending.at] or nil
        if r ~= nil and r.verb == "build" and (r.reason == "no_site" or r.reason == "unsafe") then
            pr.nosite[pr.pending.ter] = view.time + PR_STAY_OFF
            print(string.format("produce nosite ter=%d reason=%s", pr.pending.ter, r.reason))
        end
        pr.pending = nil
    end
    if view.time - (pr.t or 0) < PR_EVERY and (pr.t or 0) > 0 then
        return
    end
    if view.time < (pr.t or 0) then
        pr.t = 0   -- a game before
    end
    pr.t = view.time > 0 and view.time or 1
    pr.type, pr.ter, pr.why, pr.first = nil, -1, nil, false
    local ter = territories(view, memory)
    if #ter == 0 then
        return
    end
    for _, t in ipairs(ter) do
        t.bld, t.cits, t.sold, t.prod, t.foe, t.ebld, t.open = 0, 0, 0, 0, 0, 0, 0
        t.key = t.x * 100000 + t.y
    end
    pr.pools.main, pr.pools.siege, pr.pools.aa, pr.pools.scout = 0, 0, 0, 0
    -- its own units: buildings (any state), citizens, the land soldiers by pool; the finished production buildings
    local prod_at = {}
    local ids = ai.units{}
    local cols = ai.get(ids, {"x", "y", "type", "built", "category"})
    local prod_have = {}
    for k = 1, #ids do
        local x, y, t = cols.x[k], cols.y[k], cols.type[k]
        if x ~= nil and t ~= nil then
            local q = near_ter(ter, x, y)
            local kind = cols.category[k]
            if kind == "building" then
                if q ~= nil and not is_wall(t) then
                    ter[q].bld = ter[q].bld + 1
                end
                local is_prod = false
                for _, pk in ipairs(PR_KINDS) do
                    if pk == t then
                        is_prod = true
                    end
                end
                if is_prod then
                    prod_have[t] = (prod_have[t] or 0) + 1
                    if cols.built[k] then
                        prod_at[#prod_at + 1] = {x = x, y = y}
                        if q ~= nil then
                            ter[q].prod = ter[q].prod + 1
                        end
                    elseif q ~= nil then
                        ter[q].open = ter[q].open + 1
                    end
                end
            elseif kind == "citizen" then
                if q ~= nil then
                    ter[q].cits = ter[q].cits + 1
                end
            elseif kind == "soldier" then
                if q ~= nil then
                    ter[q].sold = ter[q].sold + 1
                end
                local pool = pool_of(t)
                pr.pools[pool] = (pr.pools[pool] or 0) + 1
            end
        end
    end
    -- the force: its land soldiers, wherever they stand
    local force = 0
    for _, t in ipairs(ter) do
        force = force + t.sold
    end
    pr.force = force
    -- the army's scouts are the explorers (upstream's scouts of the main force): counted apart, not in the main pool
    local sg = G.role(memory, "scout")
    pr.pools.scout = sg ~= nil and #sg.units or 0
    if pr.pools.main >= pr.pools.scout then
        pr.pools.main = pr.pools.main - pr.pools.scout
    else
        pr.pools.main = 0
    end
    pr.pools.total = pr.pools.main + pr.pools.siege + pr.pools.aa
    -- the enemy soldiers it sees, by territory (the attack's size, held)
    local seen = ai.seen{enemy = true, category = "soldier"}
    if #seen > 0 then
        local sc = ai.get(seen, {"x", "y"})
        for k = 1, #seen do
            local q = sc.x[k] ~= nil and near_ter(ter, sc.x[k], sc.y[k]) or nil
            if q ~= nil then
                ter[q].foe = ter[q].foe + 1
            end
        end
    end
    -- SHARES: an attack's as a share of the force, the weights of the rest
    local hsum, wsum, hard, weight = 0, 0, {}, {}
    pr.hubs, pr.buildings, pr.claims = 0, 0, 0
    for k, t in ipairs(ter) do
        local key = t.key
        local foe = t.foe
        if foe >= (pr.foe[key] or 0) or view.time < (pr.foe_t[key] or 0)
                or view.time - (pr.foe_t[key] or 0) >= PR_HOLD then
            pr.foe[key] = foe
            pr.foe_t[key] = view.time > 0 and view.time or 1
        else
            foe = pr.foe[key] or 0
        end
        hard[k] = foe > 0 and (force > 0 and (foe * PR_MARGIN // 100 + 1) * 1000 // force or 1000) or 0
        -- the enemy buildings it knows there (the attack's weight, and what a push would meet)
        local eb = ai.memory{buildings = true, near = {t.x, t.y, PR_REACH}}
        local ebld = 0
        for _, m in ipairs(eb) do
            if not is_wall(m.type) then
                ebld = ebld + 1
            end
        end
        t.ebld = ebld
        local d = 0
        if t.cits > 0 then
            d = (t.cits // 3) < 3 and 3 or (t.cits // 3)
            if d > 12 then
                d = 12
            end
        end
        if foe > 0 or ((pr.foe[key] or 0) > 0 and view.time - (pr.foe_t[key] or 0) < PR_FOE) then
            d = math.max(d, 8)
        end
        local a = 0
        if ebld > 0 then
            a = PR_ATTACK + ebld
            if a > PR_ATTACK_MAX then
                a = PR_ATTACK_MAX
            end
        end
        weight[k] = d + a   -- + EXPLORATION (0 here: see the header)
        hsum = hsum + hard[k]
        wsum = wsum + weight[k]
        if t.prod > 0 then
            pr.hubs = pr.hubs + 1
        end
    end
    pr.buildings = #prod_at
    pr.attack = hsum > 1000 and 1000 or hsum
    pr.over = hsum > 1000 and hsum - 1000 or 0
    local want, have, free, short = {}, {}, {}, {}
    pr.shortn = 0
    pr.share, pr.has = {}, {}
    for k, t in ipairs(ter) do
        local h = hsum > 1000 and hard[k] * 1000 // hsum or hard[k]
        want[k] = h + (wsum > 0 and (1000 - pr.attack) * weight[k] // wsum or 0)
        have[k] = force > 0 and t.sold * 1000 // force or 0
        free[k] = math.max(have[k] - want[k], 0)
        short[k] = math.max(want[k] - have[k], 0)
        pr.share[t.key] = want[k]
        pr.has[t.key] = have[k]
    end
    -- HAS: a neighbour's free share counts (the nearest with spare, the way over land)
    for k = 1, #ter do
        if short[k] > 0 then
            local order = {}
            for o = 1, #ter do
                if o ~= k and free[o] > 0 then
                    order[#order + 1] = {o = o, d = d2(ter[k].x, ter[k].y, ter[o].x, ter[o].y)}
                end
            end
            table.sort(order, function(a, b)
                if a.d ~= b.d then
                    return a.d < b.d
                end
                return a.o < b.o
            end)
            for _, e in ipairs(order) do
                if short[k] <= 0 then
                    break
                end
                local give = math.min(free[e.o], short[k])
                free[e.o] = free[e.o] - give
                short[k] = short[k] - give
            end
        end
    end
    for k = 1, #ter do
        pr.shortn = pr.shortn + short[k]
    end
    -- THE WAY: the nearest finished production building, territory by territory
    local dist = {}
    for k, t in ipairs(ter) do
        if t.prod > 0 then
            dist[k] = 0
        else
            local best, bestp = nil, nil
            for _, b in ipairs(prod_at) do
                local d = d2(t.x, t.y, b.x, b.y)
                if best == nil or d < best then
                    best, bestp = d, b
                end
            end
            if bestp == nil then
                dist[k] = PR_FAR
            else
                local steps = 1 + isqrt(best) // PR_STEP
                if steps > PR_FAR then
                    steps = PR_FAR
                end
                -- across the water adds PR_SEA (upstream: a water territory step)
                if ai.reach(t.x, t.y, bestp.x, bestp.y) ~= true then
                    steps = steps + PR_SEA
                end
                dist[k] = steps
            end
        end
    end
    -- FED: 2 granaries, or the food coming in is what its citizen producers use without a pause
    local gran = V.buildings(view, "b_granary")
    local gran2 = V.buildings(view, "b_granary_improved")
    local granaries = (gran ~= nil and gran.finished or 0) + (gran2 ~= nil and gran2.finished or 0)
    local fed = granaries >= 2 or view.income.food >= nonstop_food(view)
    -- SHORT: the claims (a territory short of its share builds its own, from PR_LOCAL)
    local best, bscore, hub, hwant, first, fscore = nil, 0, nil, -1, nil, -1
    local total = pr.buildings
    local prod_open = false
    for _, t in ipairs(PR_KINDS) do
        local b = V.buildings(view, t)
        if b ~= nil and b.unfinished + b.queued > 0 then
            prod_open = true
        end
    end
    for k, t in ipairs(ter) do
        if t.prod > 0 and want[k] > hwant then
            hwant, hub = want[k], k
        end
        if t.prod == 0 and t.open == 0 and t.bld >= PR_BLD and t.foe == 0 then
            local threat = ai.danger(t.x, t.y, "threat") or 0
            local lost = ai.danger(t.x, t.y, "lost") or 0
            if lost == 0 and threat == 0 then
                local fs = (t.home and 0 or 100000) + short[k] * 10 + t.bld
                if fs > fscore then
                    fscore, first = fs, k
                end
                local score = short[k] * (dist[k] - 1)
                if score < PR_LOCAL then
                    score = 0
                end
                if score > 0 then
                    pr.claims = pr.claims + 1
                    if score > bscore then
                        bscore, best = score, k
                    end
                end
            end
        end
    end
    -- WHAT: the first building of a hub, a kind it has nowhere, one more of the busy kind
    local ter_, type_, why_ = nil, nil, nil
    if total == 0 and first ~= nil and not prod_open then
        ter_, why_ = first, "its first production"
    elseif best ~= nil and total > 0 and fed then
        ter_, why_ = best, "a territory builds its own: too far from the next production for what it misses"
    end
    if ter_ ~= nil then
        type_ = first_kind(view, memory)
    end
    if type_ == nil and total > 0 and fed and hub ~= nil then
        for _, pk in ipairs(PR_KINDS) do
            if (prod_have[pk] or 0) == 0 and V.cost(view, pk) ~= nil and P.refuses(view, memory, pk) == nil
                    and (pr.nosite[ter[hub].key] or 0) <= view.time then
                ter_, type_, why_ = hub, pk, "a kind it has nowhere yet"
                break
            end
        end
    end
    local more = memory.army_plan or {}
    if type_ == nil and more.producer ~= nil and more.busy and hub ~= nil and fed
            and V.cost(view, more.producer) ~= nil and (pr.more_off[more.producer] or 0) <= view.time
            and (pr.nosite[ter[hub].key] or 0) <= view.time then
        local first_ask = pr.more_t == 0 or pr.more_type ~= more.producer
        if first_ask then
            pr.more_type, pr.more_t = more.producer, view.time > 0 and view.time or 1
        end
        if view.time - pr.more_t <= 120000 then
            ter_, type_, why_ = hub, more.producer, "one more: the army plan found that kind busy"
        else
            pr.more_off[more.producer] = view.time + 120000   -- asked 2 minutes and not built: the plan asks again
            pr.more_t = 0
        end
    end
    if type_ ~= nil and ter_ ~= nil and (pr.nosite[ter[ter_].key] or 0) <= view.time then
        local t = ter[ter_]
        pr.type, pr.ter, pr.why, pr.first = type_, ter_, why_, total == 0
        orders[#orders + 1] = {verb = "build", type = type_, near = "tile", x = t.x, y = t.y}
        pr.pending = {at = #orders, ter = t.key}
    end
    -- "# produce:" a minute
    if pr.log_t == 0 or view.time < pr.log_t or view.time - pr.log_t >= 60000 then
        pr.log_t = view.time > 0 and view.time or 1
        local parts = {}
        local keys = {}
        for k = 1, #ter do
            local key = ter[k].key
            keys[key] = true
            local want_k, have_k = pr.share[key] or 0, pr.has[key] or 0
            if want_k > 0 or have_k > 0 then
                parts[#parts + 1] = string.format("%d:%d/%d%s", k, (have_k + 5) // 10, (want_k + 5) // 10,
                    (want_k > have_k) and "!" or "")
            end
        end
        for key in pairs(pr.foe) do   -- the stations move: the old keys go
            if not keys[key] then
                pr.foe[key] = nil
                pr.foe_t[key] = nil
                pr.share[key] = nil
                pr.has[key] = nil
            end
        end
        print(string.format("produce force=%d attack=%d%%%s short=%d%% hubs=%d buildings=%d granaries=%d fed=%s " ..
            "claims=%d asks=%s in=%d(%s) pools=%d/%d/%d/%d by_need=%d/%d territory:has%%/share%%=%s",
            pr.force, (pr.attack + 5) // 10, pr.over > 0 and " (the force is too small)" or "", (pr.shortn + 5) // 10,
            pr.hubs, pr.buildings, granaries, tostring(fed), pr.claims, tostring(pr.type), pr.ter,
            tostring(pr.why), pr.pools.main, pr.pools.siege, pr.pools.aa, pr.pools.scout,
            S.train_by_need and 1 or 0, S.fleet_by_need and 1 or 0,
            #parts > 0 and table.concat(parts, " ") or "none"))
    end
end

-- a new game (produce_new_game): nothing looked at
function M.clear(memory)
    memory.pr = nil
end

return M
