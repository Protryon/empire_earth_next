-- Age of AI port: upstream dll/ai/stations.c (the force by territory: every land soldier's station, the territories'
-- shares of the force, the giving, the orders, the push, the scouts, the patrols, the demolition, a stuck order;
-- UPSTREAM has the map and the licence, GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.stations"). ALTERNATIVE land force to army.lua, behind the setting [ai_base] stations (off as
-- shipped: upstream plays it only while the game's own computer player is off, and the port's one force is measured).
--
-- The player, 2026-11-14: "the single main force isnt the way anymore", then the territories' shares (produce.c
-- SHARES: "100% is the full military force and the terriotories distribute that under each other"), and of the attack:
-- "make it push on its own, no more attack timers, either its a good idea to attack or its not".
-- Every SN_EVERY (5 s) per AI, on the territory view (territories.lua) and the shares:
--   STATIONS  - every land soldier has a station: the land territory it stands in (where it was when it is new).
--               THE MAIN FORCE (soldiers that are no siege): a territory's share of them, in soldiers. A share worth
--               fewer than SN_MIN goes to the neighbour with the largest (single soldiers hold nothing), unless it is
--               under attack. SIEGE (siege weapons, rams): among the territories planning an attack (beside enemy
--               buildings it knows, by their number) that have a share of the main force - never alone. A share holds
--               until the new one is off by SN_BAND of it, SN_BAND_MIN soldiers at least (STEADY).
--   GIVING    - a territory short of its share (by SN_BAND of it, SN_BAND_MIN at least, or by any under attack - those
--               first) is given soldiers of the nearest territories with more, territory by territory over land: their
--               station changes, they march. SN_MOVES a look, SN_GIVE_STEPS (ATTACK) territories away at most, one more
--               of the surplus kept per step; a soldier sent somewhere stays SN_STAY, a pushing territory gives none.
--   ORDERS    - a soldier away from its station's place goes there (attack-move: it fights what it meets): the middle
--               of the enemy soldiers seen in the territory when there are any, else the station's place (a finished
--               Town Centre or Capitol, a Settlement, another building of its there, else the territory's middle).
--               Asked again when it stands idle elsewhere, when the place moved, SN_REORDER otherwise. AS A GROUP: the
--               soldiers of a territory going to the same place get one move of the game's, the fast waiting for the
--               slow, once SN_GROUP of them or SN_GROUP_WAIT passed - a push and enemies in the territory wait for
--               nobody.
--   A PUSH    - no attack timers: a held territory with SN_PUSH_MIN of the force stationed, three quarters of them
--               there, beside a territory with enemy buildings it knows: its soldiers and siege attack-move at the
--               nearest of those buildings (what it knows of the enemy there: their hit points x attack) when that
--               beats the enemy there by SN_PUSH (150%); from there on at the next building nearest them when one is
--               down, back under SN_BACK (80%) or with fewer than SN_SIEGE_ESCORT soldiers left, and SN_PUSH_WAIT
--               before the next push from that territory.
--   SCOUTS    - while it has land territories no unit of its ever stood in, up to SN_SCOUTS of the main force, the
--               fastest (of equally fast ones the one that sees furthest), go to the nearest of them one after the
--               other (one not reached in SN_SCOUT_GIVE_UP is given up); none left: they are the main force's again,
--               stationed where they stand, and none is taken for SN_SCOUT_REST. A LOOK AGAIN: a land territory it
--               doesn't hold that no unit of its has stood in for SN_RESCOUT is due again.
--   PATROLS   - single soldiers of the main force at the patches of its own ground it has long had out of sight
--               (watch.lua: W.patrols_wanted / patrol_target / skip), patch after patch; the main force's again when
--               none is left.
--   DEMOLITION - a territory it has taken (its own, no enemy building it knows but walls in it, no enemy soldier
--               there, no push from it) has the enemy's wall pieces and gates in it brought down, a low task: SN_DEMO
--               of its idle main force and siege stationed there at the piece nearest its place, one after the other.
--   A STUCK ORDER - a soldier that has not come SN_STILL_D tiles from where it stood for SN_STILL since its last
--               order counts as idle: its order is taken for dead and it is ordered again (upstream's sn_idle).
--
-- THE SOLDIERS (the handover): the first look takes every own land soldier and siege weapon into one MANUAL group of
-- lib.groups (ai.take through G.add, taken out of army.lua's force and any other group), and this module orders them
-- one by one, the form army.lua uses. They go back to the engine (G.disband: ai.stop + ai.release) through
-- M.release, so army.lua's force takes them again as its free soldiers when the setting turns off; M.step calls
-- M.release itself while stations is off, so the integrator may also call M.step unconditionally. A soldier another
-- manager claimed (claims.lua) is not taken nor ordered while the claim lasts (upstream's cl_may(m->u, CLP_ROUTINE,
-- CLW_STATION)); every order this module gives is claimed at ROUTINE for CL.DUR.rout.
--
-- DEVIATIONS (the engine and the port): the territories are the port's own (territories.lua's cell model, not the
-- retail game's regions); a share's weight is computed here from the territory's facts (a held land territory 10, an
-- enemy soldier there 10 each, 12 at most, an enemy building beside it 10 each, 3 at most, a third of the guard
-- wanted) where upstream reads produce.c's g_pr_share - produce.lua's own shares are by front station, not by
-- territory; the judgement is hit points x attack of the classes (this engine exposes no damage tables, as
-- territories.lua's TV_LOSING says); "enemy buildings it knows" are the memory's (`ai.memory`, seen or remembered:
-- upstream attack.c seen_by); the station's place is a finished Town Centre/Capitol, a Settlement or any other
-- building of its found in the territory (one pass over the own buildings a look); a push's first building is the
-- one nearest the station's place and the next the one nearest its soldiers' middle (upstream: the middle both
-- times); the patrols are watch.lua's (one watch in this engine: watch.lua runs watch.c's sweep and exposes the three
-- calls stations.c makes). An enemy wall piece is attacked by its memory record's id (a building in the memory may be
-- attacked, docs/AI_LUA_UNITS §2.4); upstream sends a gate by the player's attack message instead because its
-- wall-family goal lookup crashed on a gate. The orders table and the budget of M.step are not touched: the soldiers
-- are ordered through the per-unit interface, as army.lua's are.
--
-- NOT HERE (gaps against upstream stations.c, deliberate): ACROSS THE WATER (THE PUSHES ACROSS: a territory beside a
-- territory with enemy buildings across the water sends its soldiers over on the ferry, one wave at a time), THE
-- BRIDGEHEAD and A BASE OVER THERE (they are in, behind settings.push_across; see below), COAST BATTERIES, ANTI-AIR (its
-- share and posts), THE SIEGE'S ESCORT and SIEGE AT THE GATES, RAMS AT THE TOWERS / RAMS WITH THE SOLDIERS, THE WAY and
-- A BREACH (the route search ar_route over armies.c's cells), THE CALL, THE RESERVE and THE LOADS (Fortress stashes,
-- bunker.c), QUARANTINE (the plague, a Hospital/Temple cure), AN ESCORT (holy.c's prophet), A STANDING FORMATION (the
-- patches' formation move), THE CLEARED GROUND (holding a cleared territory until a building of its claims it; the push
-- ends and the shares say where the soldiers stand), A SCENARIO'S TOGGLES and TARGETS (scen_targets.c, player_ai_toggle:
-- the port has no trigger API), THROUGH THE TERRITORY (explore.c's goals inside a scout's territory), the anti-air pool
-- and THE SQUADRON AHEAD (squadrons.c: not ported; the ferry's waves log it).
--
-- ACROSS THE WATER (ACROSS: upstream's push_across, measured worse than the landings - OFF in the port; it runs only when
-- settings.push_across AND settings.expand_across are true, both false here): a held quiet territory with SN_PUSH_MIN of
-- the force stationed whose neighbours over land have no enemy buildings it knows looks for one across the water - a
-- land territory beside it through a sea, with enemy buildings it knows and a unit of its stood in lately (LOOKED AT
-- FIRST: mem.look within SN_RESCOUT) - takes the one with the most of them. The push is judged PLANNED FOR LOSSES (one
-- transport's worth of soldiers gone, two from four transports on) and against its beach as well as its target, the
-- weaker of the two; its beach is the target land's shore on the sea both lands touch, nearest the aim, out of the
-- enemy defences it knows (ai.danger). THE FERRY (ferry.lua) carries its soldiers over ONE WAVE AT A TIME (GATHERED
-- FIRST: a wave waits FY_GATHER for room for three quarters of the push, the first wave for all of it; BOARD TOGETHER;
-- the meeting point off the beach; THE LAST WORD judged against the beach and the target), takes THE BRIDGEHEAD's
-- citizens along (2 a transport, SN_CITS at most) and hands every soldier back to this module (M.ferried) so the push
-- still knows it. THE BRIDGEHEAD (sn_bridgehead, A BASE OVER THERE): its citizens build by the beach, one going up at a
-- time - a tower, a production building, a Hospital, a tower, a Temple, a production building, a town building, a tower -
-- one laid every SN_BH_EVERY when it can pay it, dropped after SN_BH_PAY; a soldier new within SN_BH_JOIN of a standing
-- production building joins the push. All done or the push over: the citizens the game's again.
--
-- Cost: one look every SN_EVERY (a scan of the own units, one ai.get over the soldiers, two ai.seen of the enemy's,
-- the memory's buildings, a few ai.memory reads round a push's target), the steps in between are none; a look tracks
-- SN_UNITS soldiers at most.
--
-- Settings (their upstream default; add them to settings.lua to tune - stations itself is off):
--   stations = false         -- the switch (upstream: the file runs while [ai_base] game_ai = 0)
--   sn_every = 5000          -- the look, ms (SN_EVERY)
--   sn_min = 3               -- a share worth fewer soldiers goes to a neighbour (SN_MIN)
--   sn_band = 25             -- percent: a share moves (SN_BAND 1/4)
--   sn_band_min = 2          -- ... soldiers at least (SN_BAND_MIN)
--   sn_moves = 16            -- changes of station a look (SN_MOVES)
--   sn_give_steps = 3        -- territories a soldier is sent at most (SN_GIVE_STEPS) ...
--   sn_give_steps_attack = 5 -- ... to one under attack (SN_GIVE_STEPS_ATTACK)
--   sn_stay = 180000         -- a soldier given a station keeps it this long (SN_STAY)
--   sn_near = 10             -- tiles from its station's place a soldier is at it (SN_NEAR)
--   sn_reorder = 30000       -- an idle soldier is ordered again this often (SN_REORDER)
--   sn_still = 180000, sn_still_d = 4   -- A STUCK ORDER (SN_STILL, SN_STILL_D)
--   sn_group = 4, sn_group_wait = 45000 -- AS A GROUP (SN_GROUP, SN_GROUP_WAIT)
--   sn_push_min = 6          -- the force stationed before a push (SN_PUSH_MIN)
--   sn_push = 150, sn_back = 80         -- percent: judged to win / called off (SN_PUSH 1.5, SN_BACK 0.8)
--   sn_push_wait = 120000    -- before the next push from a territory (SN_PUSH_WAIT)
--   sn_scouts = 3, sn_scout_give_up = 180000, sn_scout_rest = 300000, sn_rescout = 600000   -- SCOUTS
--   sn_demo = 3              -- DEMOLITION: soldiers a territory sets at the enemy's walls at most (SN_DEMO)
local S = require("ageofai.settings")
local T = require("ageofai.territories")
local W = require("ageofai.watch")
local CL = require("ageofai.claims")
local G = require("lib.groups")
local V = require("lib.view")

local M = {}

-- stations.c's constants (the settings' defaults, the names of the keys above)
local SN_EVERY = 5000
local SN_UNITS = 1024
local SN_MIN = 3
local SN_BAND = 25
local SN_BAND_MIN = 2
local SN_MOVES = 16
local SN_NEAR = 10
local SN_REORDER = 30000
local SN_STILL = 180000
local SN_STILL_D = 4
local SN_PUSH_MIN = 6
local SN_GIVE_STEPS = 3
local SN_GIVE_STEPS_ATTACK = 5
local SN_GROUP = 4
local SN_GROUP_WAIT = 45000
local SN_DEMO = 3
local SN_STAY = 180000
local SN_GROUP_MAX = 160
local SN_PUSH = 150                 -- percent (upstream's SN_PUSH 1.5)
local SN_BACK = 80                  -- percent (upstream's SN_BACK 0.8)
local SN_PUSH_WAIT = 120000
local SN_PUSHES = 6
local SN_SCOUTS = 3
local SN_SCOUT_GIVE_UP = 180000
local SN_SCOUT_REST = 300000
local SN_RESCOUT = 600000
local SN_SIEGE_ESCORT = 3           -- THE SIEGE'S ESCORT: soldiers left for a push (siege alone is no push)
local SN_EB = 8                     -- enemy buildings remembered a territory (the nearest to its place)
local SN_FOE_R = 20                 -- tiles round a target the enemy there is judged in (army.lua's SG_FOE)
local SN_PUSH_THERE = 15            -- a soldier this near its station's place is "there" for a push

-- ACROSS (PUSH_ACROSS; off in the port): the beach, the crossing's time, the bridgehead, the losses planned for, THE LAST
-- WORD, and the news the ferry sends back
local SN_BEACH_R = 40               -- the beach: the target land's shore nearest the aim, this far searched (SN_BEACH_R)
local SN_CROSS_T = 480000           -- a push with none of it over in this long is given up (SN_CROSS_T)
local SN_CITS = 6                   -- THE BRIDGEHEAD: citizens a push across takes along at most (SN_CITS)
local SN_BH = 8                     -- ... the buildings it lays by the beach (SN_BH)
local SN_BH_JOIN = 12               -- ... a soldier new this near a production building of it joins the push (SN_BH_JOIN)
local SN_BH_EVERY = 10000           -- ... one laid this often at most (SN_BH_EVERY)
local SN_BH_PAY = 180000            -- ... one it can't build or pay this long: dropped (SN_BH_PAY)
local SN_LAST = 100                 -- THE LAST WORD: what is aboard, its losses planned for, judged at least this against
                                    -- the beach and the target (percent: upstream's SN_LAST 1.0)
local SN_LAST_WEAK = 20000          -- ... judged less this long: called off (SN_LAST_WEAK)
local SN_CALLED_OFF = 600000        -- ... then no push from that territory this long (SN_CALLED_OFF)
local SN_JOIN_T = 120000            -- the ferry's news about a passenger is fresh this long (sn_ferried's window)
local SN_TR_CAP = 10                -- the transports' places (looked up each look: the newest it has, 10 with none)
-- THE BRIDGEHEAD's plan, in order: 0 a tower, 1 a Hospital, 2 a Temple, 3 a production building, 4 a town building
local BH_PLAN = {0, 3, 1, 0, 2, 3, 4, 0}

-- the share's weight (produce.c's SHARES, computed from the territory's facts: see the header)
local SN_W_HELD = 10
local SN_W_FOE = 10
local SN_W_BLD = 10
local SN_W_BLD_MAX = 3

-- the pools (SNP_*) and what the stations use a soldier for (upstream's scout: 0 a station, 1 a scout, 3 a patrol)
local SNP_MAIN, SNP_SIEGE = 1, 2
local SC_TER, SC_SCOUT, SC_PATROL = 0, 1, 3

local TOWNS = {b_town_center = 3, b_capitol = 3, b_settlement = 2}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function is_wall(t)
    t = t or ""
    return t:sub(1, 7) == "b_wall_" or t:sub(1, 7) == "b_gate_"
end

-- a setting or the constant upstream ships
local function s(k, d)
    local v = S[k]
    return v ~= nil and v or d
end

-- ACROSS's switch (upstream ON, measured worse than the landings: the port ships it OFF; every key must say on, and the
-- ferry service carries it)
local function across_on()
    return S.push_across == true and S.expand_across == true and S.ferry_service == true
end

local function rnd(v)
    return math.floor((v or 0) + 0.5)
end

local function now_of(t)
    return t > 0 and t or 1
end

-- the band a share moves by, a territory is short by (SN_BAND of it, SN_BAND_MIN at least)
local function band(v)
    local b = v * s("sn_band", SN_BAND) // 100
    local m = s("sn_band_min", SN_BAND_MIN)
    return b > m and b or m
end

-- ---------------------------------------------------------------------------------------------------------------
-- The memory

local function new_state()
    return {
        t = 0, gid = 0, n = 0, u = {}, byid = {},
        -- by territory: the share in soldiers, the stationed, when a unit of its last stood there, no push before then
        tc = {}, ac = {}, seen = {}, look = {}, calm = {},
        push = {},
        -- ACROSS: the ferry's news (a passenger put off takes its station again), its passengers still to cross
        join = {}, transit = {},
        log_t = 0, scout_off = 0,
        moved = 0, orders = 0, groups = 0, grouped = 0, pushes = 0, won = 0, lost = 0, scouted = 0, demos = 0,
        stale = 0, took = 0, gave_back = 0, ferried = 0, lost_across = 0,
    }
end

local function entry_new(id)
    return {id = id, ter = -1, pool = SNP_MAIN, scout = SC_TER, x = 0, y = 0, hp = 0, atk = 0, sp = 0, cat = "",
            idle = 0, order_t = 0, to_x = -1, to_y = -1, moved_t = 0, wait_t = 0,
            still_x = -1, still_y = -1, still_t = 0, since = 0, look_x = -1, look_y = -1}
end

-- the push from territory r that is on (nil: none)
local function push_from(mem, r)
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 and p.from == r then
            return p
        end
    end
    return nil
end

-- A STUCK ORDER (sn_idle): a soldier that has not come SN_STILL_D tiles from where it stood for SN_STILL since its
-- last order counts as idle - its order is taken for dead
local function stuck(e, now, mem)
    if e.idle == 1 then
        return true
    end
    local sd = s("sn_still_d", SN_STILL_D)
    if e.still_t == 0 or e.still_x < 0 or d2(e.x, e.y, e.still_x, e.still_y) >= sd * sd then
        e.still_x, e.still_y, e.still_t = e.x, e.y, now
        return false
    end
    local still = s("sn_still", SN_STILL)
    if now - e.still_t < still or e.order_t == 0 or now < e.order_t or now - e.order_t < still then
        return false
    end
    e.still_t = now
    mem.stale = mem.stale + 1
    return true
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look's facts (territories.lua and the engine)

local function ctx_new(n)
    return {n = n, land = {}, role = {}, state = {}, foe = {}, bld = {}, ebld = {}, cx = {}, cy = {}, px = {}, py = {},
            prank = {}, w = {}, fx = {}, fy = {}, fn = {}, eb = {}, wall_x = {}, wall_y = {}, wall_d = {},
            wall_id = {}, main = 0, siege = 0, scouts = 0, idle = {}}
end

-- a held land territory (upstream: g_ter.t[r].water 0 and t->role != TVR_NONE)
local function held(c, r)
    return r >= 1 and r <= c.n and c.land[r] == 1 and c.role[r] ~= T.TVR_NONE
end

-- the territory's facts (territories.lua's look; the model is the port's own)
local function facts(c)
    for r = 1, c.n do
        local t = T.info(r)
        if t ~= nil then
            c.land[r] = t.land and 1 or 0
            c.role[r] = t.role or 0
            c.state[r] = t.state or 0
            c.foe[r] = t.foe or 0
            c.bld[r] = t.bld or 0
            c.ebld[r] = t.ebld or 0
            c.cx[r], c.cy[r] = t.x or 0, t.y or 0
        else
            c.land[r], c.role[r], c.state[r], c.foe[r], c.bld[r], c.ebld[r] = 0, 0, 0, 0, 0, 0
            c.cx[r], c.cy[r] = 0, 0
        end
        c.px[r], c.py[r] = c.cx[r], c.cy[r]
        c.prank[r] = 0
        c.fx[r], c.fy[r], c.fn[r] = 0, 0, 0
        c.eb[r] = {}
        c.wall_x[r], c.wall_y[r], c.wall_d[r], c.wall_id[r] = -1, -1, -1, nil
    end
end

-- every own unit marks the territory it stands in (A LOOK AGAIN: a unit of its stands there), the buildings give the
-- station's place; the land soldiers and siege weapons become the stations' entries (upstream's loop over
-- player_get_units, SN_UNITS at most)
-- A BASE OVER THERE (sn_bh_recruit): a soldier new to the stations by a standing production building of a push's
-- bridgehead is that push's (the game trains at the building it picks; over there already, no crossing)
local function sn_bh_recruit(mem, e)
    if across_on() ~= true then
        return
    end
    local ql = T.area_of(T.terr_at(e.x, e.y) or 0)
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 and p.across ~= 0 and ql ~= 0 and ql == p.to_land then
            for j = 1, p.nbh or 0 do
                local h = p.bh[j]
                if h.state == 2 and h.kind == 3 and d2(e.x, e.y, h.x, h.y) <= SN_BH_JOIN * SN_BH_JOIN then
                    e.ter = p.from
                    p.recruits = (p.recruits or 0) + 1
                    mem.ferried = (mem.ferried or 0) + 1
                    return
                end
            end
        end
    end
end

local function own_units(c, mem, now)
    local old = mem.byid or {}
    local ids = ai.units({})
    mem.n, mem.u, mem.byid = 0, {}, {}
    if ids == nil or #ids == 0 then
        return
    end
    local a = ai.get(ids, {"x", "y", "category", "type", "built"})
    local sol = {}
    for k = 1, #ids do
        local x, y = a.x[k], a.y[k]
        local cat = a.category[k]
        if x ~= nil and y ~= nil and x >= 0 and cat ~= nil then
            local r = T.terr_at(x, y)
            if r ~= nil and r >= 1 and r <= c.n then
                mem.seen[r] = 1
                mem.look[r] = now
                if cat == "building" then
                    local ty = a.type[k]
                    if a.built[k] and not is_wall(ty) then
                        local rank = TOWNS[ty] or 1
                        if rank > c.prank[r] then
                            c.prank[r] = rank
                            c.px[r], c.py[r] = x, y
                        end
                    end
                elseif cat == "soldier" or cat == "siege" or cat == "hero" then
                    if #sol < SN_UNITS then
                        sol[#sol + 1] = ids[k]
                    end
                end
            end
        end
    end
    if #sol == 0 then
        return
    end
    local b = ai.get(sol, {"x", "y", "hp", "inside", "action", "speed", "sight", "attack", "type", "category"})
    local byid, list = {}, {}
    for k = 1, #sol do
        local id = sol[k]
        local x, y, hp = b.x[k], b.y[k], b.hp[k]
        if x ~= nil and y ~= nil and x >= 0 and hp ~= nil and hp > 0 and b.inside[k] == nil then
            local e = old[id]
            local fresh = false
            if e == nil then
                e = entry_new(id)
                fresh = true
            end
            local ty = b.type[k] or ""
            e.x, e.y = x, y
            e.hp, e.atk = rnd(hp), rnd(b.attack[k])
            e.sp = rnd((b.speed[k] or 0) * 100) + rnd(b.sight[k])
            e.cat = b.category[k] or "soldier"
            e.idle = b.action[k] == "idle" and 1 or 0
            e.pool = (e.cat == "siege" or ty:find("ram", 1, true) ~= nil) and SNP_SIEGE or SNP_MAIN
            local r = T.terr_at(x, y)
            if r == nil or r < 1 or r > c.n then
                r = -1
            end
            if e.scout == SC_TER and (e.ter < 1 or e.ter > c.n or c.land[e.ter] ~= 1) then
                e.ter = r   -- where it stands, when it is new (upstream's sn_rejoin's default)
            end
            if fresh then
                -- ACROSS: the ferry's news - a passenger put off takes its station again (sn_ferried; the look has not
                -- seen it since the ferry put it off). Applied after the tile's own, as upstream's sn_rejoin does
                local j = mem.join ~= nil and mem.join[id] or nil
                if j ~= nil then
                    if j.t <= 0 or now - j.t > SN_JOIN_T then
                        mem.join[id] = nil
                    elseif j.ter >= 1 and j.ter <= c.n then
                        e.ter = j.ter
                        mem.join[id] = nil
                    end
                elseif e.pool ~= SNP_SIEGE then
                    sn_bh_recruit(mem, e)
                end
            end
            byid[id] = e
            list[#list + 1] = e
        end
    end
    mem.byid = byid
    mem.u = list
    mem.n = #list
    for _, e in ipairs(list) do
        if e.scout == SC_TER then
            if e.pool == SNP_SIEGE then
                c.siege = c.siege + 1
            else
                c.main = c.main + 1
            end
        elseif e.scout == SC_SCOUT then
            c.scouts = c.scouts + 1
        end
    end
end

-- the enemy it knows: the soldiers it sees now by territory (their middle), the buildings to push at, the wall pieces
-- and gates to bring down (upstream's grid_it_all with seen_by and fronts.c fr_build/fr_target)
local function enemy_facts(c)
    local seen = {}
    for _, cat in ipairs({"soldier", "siege"}) do
        for _, id in ipairs(ai.seen{enemy = true, category = cat}) do
            seen[#seen + 1] = id
        end
    end
    if #seen > 0 then
        local g = ai.get(seen, {"x", "y"})
        for k = 1, #seen do
            local x, y = g.x[k], g.y[k]
            if x ~= nil and y ~= nil and x >= 0 then
                local r = T.terr_at(x, y)
                if r ~= nil and held(c, r) then
                    c.fx[r] = c.fx[r] + x
                    c.fy[r] = c.fy[r] + y
                    c.fn[r] = c.fn[r] + 1
                end
            end
        end
    end
    for _, m in ipairs(ai.memory{buildings = true}) do
        local x, y = m.x, m.y
        if x ~= nil and y ~= nil and x >= 0 then
            local r = T.terr_at(x, y)
            -- every land territory it knows (a push goes into one it does not hold: the buildings it knows there)
            if r ~= nil and r >= 1 and r <= c.n and c.land[r] == 1 then
                if is_wall(m.type) then
                    if held(c, r) then
                        local d = d2(x, y, c.px[r], c.py[r])
                        if c.wall_x[r] < 0 or d < c.wall_d[r] then
                            c.wall_x[r], c.wall_y[r], c.wall_d[r], c.wall_id[r] = x, y, d, m.id
                        end
                    end
                else
                    local list = c.eb[r]
                    local d = d2(x, y, c.px[r], c.py[r])
                    local at = #list + 1
                    for k = 1, #list, 3 do
                        if d < list[k + 2] then
                            at = k
                            break
                        end
                    end
                    if at <= 3 * SN_EB - 2 then
                        table.insert(list, at, x)
                        table.insert(list, at + 1, y)
                        table.insert(list, at + 2, d)
                        while #list > 3 * SN_EB do
                            table.remove(list)
                            table.remove(list)
                            table.remove(list)
                        end
                    end
                end
            end
        end
    end
end

-- the enemy strength near a tile: hit points x attack of what it knows there (soldiers seen or remembered, defences)
local function enemy_str(x, y, cls)
    local str = 0
    for _, m in ipairs(ai.memory{near = {x, y, SN_FOE_R}, limit = 32}) do
        local a = cls[m.type]
        if a == nil then
            local rec = ai.class(m.type)
            a = rnd(rec ~= nil and rec.attack or 0)
            cls[m.type] = a
        end
        if a > 0 then
            str = str + rnd(m.hp) * a
        end
    end
    return str
end

-- the enemy building of a territory's remembered list nearest a tile; nil, nil: none left there
local function nearest_eb(list, x, y)
    local bx, by, bd = nil, nil, -1
    for k = 1, #list, 3 do
        local d = d2(x, y, list[k], list[k + 1])
        if bd < 0 or d < bd then
            bx, by, bd = list[k], list[k + 1], d
        end
    end
    return bx, by
end

-- ---------------------------------------------------------------------------------------------------------------
-- STATIONS: the shares in soldiers (produce.c SHARES, STEADY, SN_MIN)

local function shares(c, mem)
    local sum = 0
    for r = 1, c.n do
        local w = 0
        if held(c, r) then
            w = SN_W_HELD
            if c.foe[r] > 0 then
                w = w + SN_W_FOE * (c.foe[r] < 12 and c.foe[r] or 12)
            end
            local b = 0
            for _, nb in ipairs(T.neighbours(r)) do
                if nb.o >= 1 and nb.o <= c.n and c.land[nb.o] == 1 and c.ebld[nb.o] > 0 then
                    b = b + 1
                end
            end
            if b > SN_W_BLD_MAX then
                b = SN_W_BLD_MAX
            end
            w = w + SN_W_BLD * b
            w = w + T.guard_want(r) // 3
        end
        c.w[r] = w
        sum = sum + w
    end
    local tc = {}
    for r = 1, c.n do
        local v = 0
        if sum > 0 and c.w[r] > 0 then
            v = (c.main * c.w[r] + sum // 2) // sum
        end
        local was = mem.tc[r] or 0
        -- STEADY: a share stays what it was until the new one is off by the band (not one under attack)
        if v > 0 and was > 0 and math.abs(v - was) < band(was) and not (c.foe[r] > 0 and c.role[r] ~= T.TVR_NONE) then
            v = was
        end
        tc[r] = v
    end
    -- SN_MIN: a share worth fewer soldiers goes to the land neighbour with the largest (single soldiers hold nothing)
    for r = 1, c.n do
        if tc[r] > 0 and tc[r] < s("sn_min", SN_MIN) and not (c.foe[r] > 0 and c.role[r] ~= T.TVR_NONE) then
            local b = -1
            for _, nb in ipairs(T.neighbours(r)) do
                local o = nb.o
                if o >= 1 and o <= c.n and c.land[o] == 1 and tc[o] >= s("sn_min", SN_MIN)
                        and (b < 0 or tc[o] > tc[b]) then
                    b = o
                end
            end
            if b > 0 then
                tc[b] = tc[b] + tc[r]
                tc[r] = 0
            end
        end
    end
    for r = 1, c.n do
        mem.tc[r] = tc[r]
    end
    return tc
end

-- the siege's shares: the territories planning an attack (beside enemy buildings it knows, by their number) that
-- have a share of the main force; none planned: no siege wanted anywhere
local function siege_shares(c, tc)
    local tcs, wsum = {}, 0
    for r = 1, c.n do
        tcs[r] = 0
        if held(c, r) and tc[r] > 0 then
            local a = 0
            for _, nb in ipairs(T.neighbours(r)) do
                local o = nb.o
                if o >= 1 and o <= c.n and c.land[o] == 1 and c.ebld[o] > 0 and 10 + c.ebld[o] > a then
                    a = 10 + c.ebld[o]
                end
            end
            tcs[r] = a
            wsum = wsum + a
        end
    end
    if wsum > 0 then
        for r = 1, c.n do
            tcs[r] = (tcs[r] * c.siege + wsum // 2) // wsum
        end
    end
    return tcs
end

-- ---------------------------------------------------------------------------------------------------------------
-- GIVING: the territories short of their share given those of the nearest with more, over land

-- the stationed of a pool counted by territory (nil: both)
local function count_stationed(c, mem, pool)
    local ac = {}
    for r = 1, c.n do
        ac[r] = 0
    end
    for _, e in ipairs(mem.u) do
        if e.scout == SC_TER and e.ter >= 1 and e.ter <= c.n and (pool == nil or e.pool == pool) then
            ac[e.ter] = ac[e.ter] + 1
        end
    end
    return ac
end

local function give(c, mem, tc, ac, pool, budget, now)
    local any = 0
    local done = {}
    while budget > 0 do
        local r, best = -1, 0
        for k = 1, c.n do
            local df = tc[k] - ac[k]
            local hard = c.foe[k] > 0 and c.role[k] ~= T.TVR_NONE
            if not done[k] and df > 0 and (hard or df >= band(tc[k])) then
                local pr = df + (hard and 100000 or 0)
                if pr > best then
                    best, r = pr, k
                end
            end
        end
        if r < 0 then
            break
        end
        done[r] = true
        local vis, dep, q = {}, {}, {}
        local qh, qt = 1, 2
        q[1] = r
        vis[r] = true
        dep[r] = 0
        local hard_r = c.foe[r] > 0 and c.role[r] ~= T.TVR_NONE
        local far = hard_r and s("sn_give_steps_attack", SN_GIVE_STEPS_ATTACK) or s("sn_give_steps", SN_GIVE_STEPS)
        while qh < qt and tc[r] > ac[r] and budget > 0 do
            local o = q[qh]
            qh = qh + 1
            -- BY STEPS: one more of the surplus kept for each step beyond the first, none beyond SN_GIVE_STEPS
            local take = 0
            if o ~= r then
                take = ac[o] - tc[o] - (hard_r and 0 or (dep[o] - 1))
            end
            if take > 0 and push_from(mem, o) == nil then
                for _, e in ipairs(mem.u) do
                    if take <= 0 or tc[r] <= ac[r] or budget <= 0 then
                        break
                    end
                    if e.pool == pool and e.scout == SC_TER and e.ter == o then
                        local stay = not hard_r and e.moved_t > 0 and now >= e.moved_t
                                and now - e.moved_t < s("sn_stay", SN_STAY)
                        if not stay then
                            e.ter = r
                            e.order_t = 0
                            e.moved_t = now
                            ac[o] = ac[o] - 1
                            ac[r] = ac[r] + 1
                            take = take - 1
                            budget = budget - 1
                            mem.moved = mem.moved + 1
                            any = any + 1
                        end
                    end
                end
            end
            if dep[o] < far then
                for _, nb in ipairs(T.neighbours(o)) do
                    local w = nb.o
                    if w >= 1 and w <= c.n and not vis[w] and c.land[w] == 1 then
                        vis[w] = true
                        dep[w] = dep[o] + 1
                        q[qt] = w
                        qt = qt + 1
                    end
                end
            end
        end
    end
    return any, budget
end

-- ---------------------------------------------------------------------------------------------------------------
-- A PUSH: the ones on, then a new one (the nearest enemy building it knows there, judged, on to the next)

local function judge(c, cls, x, y, own)
    local foe = enemy_str(x, y, cls)
    if foe <= 0 then
        return 1000, 0
    end
    return own * 100 // foe, foe
end

-- the newest transport's places (10 with none of its yet: sn_loss's load)
local function transport_cap()
    local ids = ai.units{category = "transport", limit = 4}
    if #ids > 0 then
        local u = ai.unit(ids[1])
        local k = u ~= nil and ai.class(u.type) or nil
        if k ~= nil and (k.transport or 0) > 0 then
            return k.transport
        end
    end
    return 10
end

-- PLANNED FOR LOSSES (sn_loss): a push over the water is judged without the soldiers of its lost transports - one, two
-- from four transports on, a full load each (the transport type's capacity; 10 with none of its yet)
local function sn_loss(n, cap)
    local ships = (n + cap - 1) // cap
    local lost = (ships > 3 and 2 or 1) * cap
    return lost < n and lost or n
end

-- ACROSS: the judgement with the losses planned for and against its beach as well as its target (the weaker of the two)
local function judge_across(cls, n, own_all, aim_x, aim_y, beach_x, beach_y)
    local skip = sn_loss(n, SN_TR_CAP)
    if skip >= n then
        return 0, 0
    end
    local own = own_all * (n - skip) // n
    local r, foe = judge(nil, cls, aim_x, aim_y, own)
    if d2(aim_x, aim_y, beach_x, beach_y) > SN_FOE_R * SN_FOE_R then
        local rb = judge(nil, cls, beach_x, beach_y, own)
        if rb < r then
            r = rb
        end
    end
    return r, foe
end

local function push_end(mem, p, now, why)
    p.on = 0
    p.end_t = now
    p.why = why
    if p.from >= 1 then
        mem.calm[p.from] = now + (p.wait > 0 and p.wait or s("sn_push_wait", SN_PUSH_WAIT))
    end
    for _, e in ipairs(mem.u) do
        if e.ter == p.from then
            e.order_t = 0
        end
    end
    if p.cits ~= nil and #p.cits > 0 then   -- THE BRIDGEHEAD's citizens, work.c's again
        ai.release(p.cits)
        p.cits, p.ncits = {}, 0
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE BRIDGEHEAD (ACROSS): the citizens a push over the water took along build by its beach

local TOWER_CLASSES = {"b_guard_tower_paleo", "b_guard_tower_copper", "b_guard_tower_bronze", "b_guard_tower_middle",
                       "b_guard_tower_imperial", "b_guard_tower_ww1", "b_guard_tower_digital", "b_guard_tower_space"}

-- the newest tower class the player can make (territories.lua's list)
local function tower_class(view)
    for k = #TOWER_CLASSES, 1, -1 do
        if V.cost(view, TOWER_CLASSES[k]) ~= nil then
            return TOWER_CLASSES[k]
        end
    end
    return nil
end

-- it can build and pay the type now (bd_pays' stand-in: the cost and the stock)
local function pays(view, ty)
    local c = V.cost(view, ty)
    if c == nil then
        return false
    end
    for _, r in ipairs(V.resources) do
        if view.stock[r] < c.cost[r] then
            return false
        end
    end
    return true
end

-- THE BRIDGEHEAD's type of a kind (sn_bh_type): the newest tower, a Hospital, a Temple, a production building (the first
-- it hasn't got there: its Barracks, else the army's own producer), a Town Center or a Settlement
local function bh_type(view, p, kind, memory)
    if kind == 0 then
        local ty = tower_class(view)
        return ty ~= nil and pays(view, ty) and ty or nil
    elseif kind == 1 then
        return pays(view, "b_hospital") and "b_hospital" or nil
    elseif kind == 2 then
        return pays(view, "b_temple") and "b_temple" or nil
    elseif kind == 3 then
        local had = {}
        for j = 1, p.nbh do
            local h = p.bh[j]
            if (h.state == 1 or h.state == 2) and h.type ~= nil and h.type ~= "" then
                had[h.type] = true
            end
        end
        local a = memory.army or {}
        local pr = a.plan ~= nil and a.plan.producer or a.last_producer
        for _, ty in ipairs({"b_barracks", pr}) do
            if ty ~= nil and not had[ty] and pays(view, ty) then
                return ty
            end
        end
        return nil
    end
    if pays(view, "b_town_center") then
        return "b_town_center"
    end
    return pays(view, "b_settlement") and "b_settlement" or nil
end

-- where a bridgehead building goes: the spot nearest the citizens' middle on the push's land, out of the enemy's reach it
-- knows, 3 tiles from the others (sn_bh_spot)
local function bh_spot(p, ty, cx, cy)
    local x, y = ai.find_site(ty, cx, cy, {min = 2, max = 12, safe = true})
    if x == nil then
        x, y = ai.find_site(ty, cx, cy, {min = 2, max = 12})
    end
    if x == nil or T.area_of(T.terr_at(x, y) or 0) ~= p.to_land then
        return nil
    end
    if (ai.danger(x, y) or 0) > 0 then
        return nil
    end
    for j = 1, p.nbh do
        local h = p.bh[j]
        if (h.state == 1 or h.state == 2) and math.abs(h.x - x) < 4 and math.abs(h.y - y) < 4 then
            return nil
        end
    end
    return x, y
end

-- the bridgehead's step (sn_bridgehead), every look while the push is on: its citizens alive and its own again (the ferry
-- put them off), one building going up at a time in BH_PLAN's order, one laid every SN_BH_EVERY, every citizen on it; all
-- done: its citizens the game's again
local function bridgehead(mem, p, view, memory, now)
    p.cits = p.cits or {}
    p.nbh = p.nbh or 0
    p.bh = p.bh or {}
    local cols = ai.get(p.cits, {"x", "y", "held"})
    local keep, sx, sy, n = {}, 0, 0, 0
    for k, id in ipairs(p.cits) do
        if cols.x[k] ~= nil then
            keep[#keep + 1] = id
            if cols.held[k] ~= "lua" then
                ai.take(id)   -- the ferry released it: the bridgehead's again
            end
            sx, sy, n = sx + cols.x[k], sy + cols.y[k], n + 1
        end
    end
    p.cits, p.ncits = keep, #keep
    if n == 0 then
        return
    end
    if p.nbh == 0 then
        p.bh, p.nbh = {}, SN_BH
        for j = 1, SN_BH do
            p.bh[j] = {kind = BH_PLAN[j], type = "", state = 0, id = 0, x = 0, y = 0, t = 0}
        end
    end
    local cx, cy = sx // n, sy // n
    local going = -1
    for j = 1, p.nbh do
        local h = p.bh[j]
        if h.state == 1 then
            local u = ai.unit(h.id)
            if u == nil then
                h.state = 3
            elseif u.built then
                h.state = 2
                mem.bh_built = (mem.bh_built or 0) + 1
            else
                going = j
            end
        end
    end
    if going < 0 and (p.bh_t == 0 or now - p.bh_t >= SN_BH_EVERY) then
        for j = 1, p.nbh do
            local h = p.bh[j]
            if h.state == 0 then
                if h.t == 0 then
                    h.t = now_of(now)
                end
                local ty = bh_type(view, p, h.kind, memory)
                if ty == nil or not pays(view, ty) then
                    if now - h.t >= SN_BH_PAY then
                        h.state = 3
                        mem.bh_drop = (mem.bh_drop or 0) + 1
                    end
                else
                    local ax, ay = bh_spot(p, ty, cx, cy)
                    local id = nil
                    if ax ~= nil and #keep > 0 then
                        local code, reason, nn, bid = ai.build(keep, ty, ax, ay)
                        if code == "ok" and bid ~= nil then
                            id = bid
                        end
                    end
                    p.bh_t = now_of(now)
                    if id == nil then
                        h.state = 3
                        mem.bh_drop = (mem.bh_drop or 0) + 1
                    else
                        h.type, h.id, h.x, h.y, h.state = ty, id, ax, ay, 1
                        mem.bh_laid = (mem.bh_laid or 0) + 1
                        going = j
                    end
                end
                break   -- one at a time
            end
        end
    end
    local left = 0
    for j = 1, p.nbh do
        if p.bh[j].state < 2 then
            left = left + 1
        end
    end
    if left == 0 then
        ai.release(keep)   -- the bridgehead done: its citizens where they stand, the game's again
        p.cits, p.ncits = {}, 0
        mem.bh_done = (mem.bh_done or 0) + 1
        return
    end
    if going < 0 then
        return
    end
    ai.repair(keep, p.bh[going].id)   -- all of them on the one going up
    mem.orders = mem.orders + 1
end

-- the pushes that are on: judged, the next building or the end
local function pushes_on(c, mem, view, memory, now)
    local cls = {}
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 then
            if p.across ~= 0 and (p.ncits or 0) > 0 then
                bridgehead(mem, p, view, memory, now)   -- THE BRIDGEHEAD
            end
            -- ACROSS: those over on the land it pushes into are the push's (their middle, their strength); those still on
            -- the land it left and the ferry's passengers are still to cross - without, a push whose every soldier was
            -- with the ferry was "nobody left"
            local n, mn, sx, sy, msx, msy, own, home_n = 0, 0, 0, 0, 0, 0, 0, 0
            for _, e in ipairs(mem.u) do
                if e.ter == p.from and e.scout == SC_TER and e.pool ~= SNP_AA then
                    if p.across ~= 0 and T.area_of(T.terr_at(e.x, e.y) or 0) ~= p.to_land then
                        home_n = home_n + 1
                    else
                        n = n + 1
                        sx, sy = sx + e.x, sy + e.y
                        if e.pool == SNP_MAIN then
                            mn = mn + 1
                            msx, msy = msx + e.x, msy + e.y
                        end
                        own = own + e.hp * e.atk
                    end
                end
            end
            if p.across ~= 0 then
                p.over, p.left = n, home_n + (mem.transit[k] or 0)
            end
            local mx, my = p.at_x, p.at_y
            if mn > 0 then
                mx, my = msx // mn, msy // mn   -- THE SIEGE'S ESCORT: where its soldiers are
            end
            p.n, p.mn, p.mx, p.my = n, mn, mx, my
            -- ACROSS: the ferry still at it - the beach held meanwhile; none over in SN_CROSS_T: given up
            local crossing, why = false, nil
            if p.across ~= 0 and p.left > 0 and (p.over == 0 or p.left * 4 > p.left + p.over) then
                if now - p.start_t < SN_CROSS_T then
                    crossing = true
                elseif p.over == 0 then
                    why = "not carried over in time"
                    mem.lost_across = (mem.lost_across or 0) + 1
                end
            end
            if crossing then
                -- nothing to judge: they are on their way
            elseif why ~= nil then
                push_end(mem, p, now, why)
            elseif n == 0 and (p.left or 0) == 0 then
                push_end(mem, p, now, "nobody left")
            elseif mn < SN_SIEGE_ESCORT then
                push_end(mem, p, now, "too few soldiers left for its siege")
            else
                local bx, by = nearest_eb(c.eb[p.into] or {}, mx, my)
                if bx == nil then
                    -- no building left there it knows: the push is over, the shares say where next
                    mem.won = mem.won + 1
                    push_end(mem, p, now, "no enemy building left there it knows")
                else
                    local ratio
                    if p.across ~= 0 then
                        ratio = judge_across(cls, n, own, bx, by, p.beach_x, p.beach_y)
                    else
                        ratio = judge(c, cls, bx, by, own)
                    end
                    if ratio < s("sn_back", SN_BACK) then
                        mem.lost = mem.lost + 1
                        push_end(mem, p, now, "losing")
                    else
                        p.at_x, p.at_y, p.ratio = bx, by, ratio
                    end
                end
            end
        end
    end
end

-- ACROSS: the territory across the water a push from r goes into - the land on the other side of a sea beside it, one a
-- unit of its stood in lately (LOOKED AT FIRST), the one with the most enemy buildings it knows
local function across_target(c, mem, r, now)
    local into, best = -1, 0
    local rescout = s("sn_rescout", SN_RESCOUT)
    for _, nb in ipairs(T.neighbours(r)) do
        local s_ = nb.o
        if s_ >= 1 and s_ <= c.n and c.land[s_] ~= 1 then   -- a sea beside it
            for _, q in ipairs(T.neighbours(s_)) do
                local o = q.o
                if o ~= r and o >= 1 and o <= c.n and c.land[o] == 1 and c.ebld[o] > best
                        and (mem.seen[o] or 0) ~= 0 and mem.look[o] ~= nil and now - mem.look[o] < rescout then
                    into, best = o, c.ebld[o]
                end
            end
        end
    end
    return into
end

-- ACROSS: the beach - the target land's shore on the sea both lands touch, nearest the aim, out of the enemy's reach it
-- knows (sn_beach, SN_BEACH_R)
local function across_beach(c, r, into, ax, ay)
    local land = T.area_of(into)
    if land == 0 then
        return nil
    end
    for _, nb in ipairs(T.neighbours(r)) do
        local s_ = nb.o
        if s_ >= 1 and s_ <= c.n and c.land[s_] ~= 1 then
            local sea = T.area_of(s_)
            local touch = false
            for _, q in ipairs(T.neighbours(s_)) do
                if q.o == into then
                    touch = true
                end
            end
            if touch and sea ~= 0 then
                local sx, sy = ai.shore(ax, ay, {sea = sea, land = land, max = SN_BEACH_R})
                if sx ~= nil and (ai.danger(sx, sy) or 0) == 0 then
                    return sx, sy, sea
                end
            end
        end
    end
    return nil
end

-- a new push: a held, quiet territory with SN_PUSH_MIN stationed, three quarters of them there, beside a territory
-- with enemy buildings it knows, judged to beat them by SN_PUSH
local function pushes_new(c, mem, ac, now)
    local cls = {}
    for r = 1, c.n do
        if held(c, r) and c.state[r] == T.TV_QUIET and ac[r] >= s("sn_push_min", SN_PUSH_MIN)
                and push_from(mem, r) == nil and now >= (mem.calm[r] or 0) then
            local into, best = -1, 0
            for _, nb in ipairs(T.neighbours(r)) do
                local o = nb.o
                if o >= 1 and o <= c.n and c.land[o] == 1 and c.ebld[o] > best then
                    into, best = o, c.ebld[o]
                end
            end
            local across = 0
            if into < 0 and across_on() then   -- ACROSS: no enemy beside it over land - one across the water
                into = across_target(c, mem, r, now)
                across = into > 0 and 1 or 0
            end
            if into > 0 then
                local cn, nthere = 0, 0
                for _, e in ipairs(mem.u) do
                    if e.ter == r and e.scout == SC_TER then
                        cn = cn + 1
                        if T.terr_at(e.x, e.y) == r or d2(e.x, e.y, c.px[r], c.py[r]) <= SN_PUSH_THERE * SN_PUSH_THERE then
                            nthere = nthere + 1
                        end
                    end
                end
                if cn > 0 and nthere * 4 >= cn * 3 then
                    local bx, by = nearest_eb(c.eb[into] or {}, c.px[r], c.py[r])
                    if bx ~= nil then
                        local own = 0
                        for _, e in ipairs(mem.u) do
                            if e.ter == r and e.scout == SC_TER then
                                own = own + e.hp * e.atk
                            end
                        end
                        local ratio, beach_x, beach_y, beach_sea = 0, bx, by, 0
                        if across ~= 0 then   -- PLANNED FOR LOSSES, and against the beach as well as the target
                            beach_x, beach_y, beach_sea = across_beach(c, r, into, bx, by)
                            if beach_x ~= nil then
                                ratio = judge_across(cls, cn, own, bx, by, beach_x, beach_y)
                            end
                        else
                            ratio = judge(c, cls, bx, by, own)
                        end
                        if ratio >= s("sn_push", SN_PUSH) then
                            local slot = -1
                            for k = 1, SN_PUSHES do
                                local p = mem.push[k]
                                if p == nil then
                                    p = {on = 0, from = -1, into = -1, at_x = -1, at_y = -1, mx = -1, my = -1,
                                         n = 0, mn = 0, start_t = 0, end_t = 0, wait = 0, ratio = 0, why = "",
                                         across = 0, from_land = 0, to_land = 0, beach_x = -1, beach_y = -1,
                                         left = 0, over = 0, waves = 0, wave_t = 0,
                                         cits = {}, ncits = 0, cits_want = 0, bh = {}, nbh = 0, bh_t = 0,
                                         judge_t = 0, judge_r = 0, weak_t = 0, recruits = 0}
                                    mem.push[k] = p
                                    slot = k
                                    break
                                elseif p.on == 0 and (slot < 0 or p.end_t < mem.push[slot].end_t) then
                                    slot = k
                                end
                            end
                            if slot > 0 then
                                local p = mem.push[slot]
                                p.on, p.from, p.into = 1, r, into
                                p.at_x, p.at_y = bx, by
                                p.mx, p.my, p.n, p.mn = -1, -1, cn, cn
                                p.start_t, p.end_t, p.wait, p.ratio, p.why = now, 0, 0, ratio, ""
                                p.across, p.from_land, p.to_land = across, T.area_of(r), T.area_of(into)
                                p.beach_x, p.beach_y = beach_x, beach_y
                                p.left, p.over, p.waves, p.wave_t = cn, 0, 0, 0
                                p.cits, p.ncits, p.cits_want = {}, 0, 0
                                p.bh, p.nbh, p.bh_t = {}, 0, 0
                                p.judge_t, p.judge_r, p.weak_t, p.recruits = 0, ratio, 0, 0
                                mem.pushes = mem.pushes + 1
                                for _, e in ipairs(mem.u) do
                                    if e.ter == r then
                                        e.order_t = 0
                                    end
                                end
                            end
                        end
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- SCOUTS

-- a territory a scout goes to: 2 never stood in, 1 not held and no unit of its stood in for SN_RESCOUT, 0 not due
local function due(c, mem, r, now)
    if c.land[r] ~= 1 then
        return 0
    end
    if (mem.seen[r] or 0) == 0 then
        return 2
    end
    if not held(c, r) and now - (mem.look[r] or 0) >= s("sn_rescout", SN_RESCOUT) then
        return 1
    end
    return 0
end

-- the fastest of the main force (of equally fast ones the one that sees furthest) - the index in mem.u, -1: none;
-- with want_ter, only of a territory with no enemy soldiers in it and not pushing
local function pick_fast(c, mem, want_ter)
    local b, bf = -1, -1
    for k, e in ipairs(mem.u) do
        if e.pool == SNP_MAIN and e.scout == SC_TER and e.ter >= 1 and e.ter <= c.n then
            local ok = true
            if want_ter then
                ok = c.foe[e.ter] == 0 and push_from(mem, e.ter) == nil
            end
            if ok and e.sp > bf then
                bf, b = e.sp, k
            end
        end
    end
    return b
end

-- the scouts: taken, the next territory (never stood in first, not another's), given up, back to the force
local function scouts(c, mem, view, memory, now)
    local dues, due_n, sc_of = {}, 0, {}
    for r = 1, c.n do
        dues[r] = due(c, mem, r, now)
        if dues[r] ~= 0 then
            due_n = due_n + 1
        end
    end
    for _, e in ipairs(mem.u) do
        if e.scout == SC_SCOUT and e.ter >= 1 then
            sc_of[e.ter] = true
        end
    end
    local want = 0
    -- (upstream also asks the scenario's Explore toggle here: the port has no trigger API - a gap)
    if due_n > 0 and c.main + c.scouts >= 3 and now >= mem.scout_off then
        want = 1 + due_n // 8
        if want > s("sn_scouts", SN_SCOUTS) then
            want = s("sn_scouts", SN_SCOUTS)
        end
    end
    local have = c.scouts
    while have < want do
        local b = pick_fast(c, mem, true)
        if b < 0 then
            break
        end
        local e = mem.u[b]
        e.scout, e.ter, e.order_t, e.since = SC_SCOUT, -1, 0, 0
        e.look_x, e.look_y = -1, -1
        have = have + 1
        c.scouts = c.scouts + 1
        c.main = c.main - 1
    end
    for k, e in ipairs(mem.u) do
        if e.scout == SC_SCOUT then
            if e.ter >= 1 and e.ter <= c.n and due(c, mem, e.ter, now) ~= 0 and e.since > 0
                    and now - e.since > s("sn_scout_give_up", SN_SCOUT_GIVE_UP) then
                mem.look[e.ter] = now   -- not reached in that time: given up, till SN_RESCOUT
                dues[e.ter] = due(c, mem, e.ter, now)
            end
            if e.ter < 1 or e.ter > c.n or due(c, mem, e.ter, now) == 0 then
                -- the next one: the nearest due on its land, none another scout's (never stood in first)
                local land = T.area_of(T.terr_at(e.x, e.y) or 0)
                local b, bd = -1, 0
                for r = 1, c.n do
                    local du = due(c, mem, r, now)
                    if du ~= 0 and (land == 0 or T.area_of(r) == land) and not sc_of[r] then
                        local d = d2(c.cx[r] or 0, c.cy[r] or 0, e.x, e.y) // (du == 2 and 4 or 1)
                        if b < 0 or d < bd then
                            b, bd = r, d
                        end
                    end
                end
                if b < 0 then
                    local r = T.terr_at(e.x, e.y)
                    e.scout, e.ter, e.order_t = SC_TER, (r ~= nil and r >= 1 and r <= c.n) and r or 0, 0
                    mem.scout_off = now + s("sn_scout_rest", SN_SCOUT_REST)   -- none left: no scout for a while
                    mem.scouted = mem.scouted + 1
                    c.scouts = c.scouts - 1
                    c.main = c.main + 1
                else
                    if e.ter >= 1 then
                        sc_of[e.ter] = nil
                    end
                    e.ter, e.order_t, e.since = b, 0, now
                    sc_of[b] = true
                end
            end
            if e.scout == SC_SCOUT and e.ter >= 1 and e.ter <= c.n then
                if e.order_t == 0 or (now - e.order_t > s("sn_reorder", SN_REORDER) and c.idle[k] == 1) then
                    if CL.take(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, CL.DUR.rout, {category = e.cat}) then
                        ai.move({e.id}, c.cx[e.ter] or e.x, c.cy[e.ter] or e.y, {forced = true})
                        e.order_t, e.to_x, e.to_y = now, c.cx[e.ter] or e.x, c.cy[e.ter] or e.y
                        mem.orders = mem.orders + 1
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- PATROLS (watch.lua's patches)

local function patrols(c, mem, view, memory, now)
    local want = S.fog_patrols == false and 0 or W.patrols_wanted(c.main)
    local have = 0
    for _, e in ipairs(mem.u) do
        if e.scout == SC_PATROL then
            have = have + 1
        end
    end
    while have < want do
        local b = pick_fast(c, mem, true)
        if b < 0 then
            break
        end
        local e = mem.u[b]
        e.scout, e.ter, e.order_t, e.since = SC_PATROL, -1, 0, 0
        e.look_x, e.look_y = -1, -1
        have = have + 1
        c.main = c.main - 1
    end
    for k, e in ipairs(mem.u) do
        if e.scout == SC_PATROL then
            if want == 0 then
                e.look_x, e.look_y = -1, -1   -- switched off, the scenario's Explore off, too few soldiers: back
            elseif e.look_x >= 0 and W.in_sight(e.look_x, e.look_y) then
                e.look_x, e.look_y = -1, -1   -- seen: the next one
            elseif e.look_x >= 0 and e.since > 0 and now - e.since > W.WT_GIVE_UP then
                W.skip(e.look_x, e.look_y, now)   -- not seen in that time: given up
                e.look_x, e.look_y = -1, -1
            end
            if e.look_x < 0 and have <= want then
                local others = {}
                for _, o in ipairs(mem.u) do
                    if o.scout == SC_PATROL and o.look_x >= 0 then
                        others[#others + 1] = {x = o.look_x, y = o.look_y}
                    end
                end
                local land = T.area_of(T.terr_at(e.x, e.y) or 0)
                local px, py = W.patrol_target(e.x, e.y, land ~= 0 and land or nil, others)
                if px ~= nil then
                    e.look_x, e.look_y, e.since, e.order_t = px, py, now, 0
                end
            end
            if e.look_x < 0 then   -- nothing left for it (or one too many): the main force's again
                local r = T.terr_at(e.x, e.y)
                e.scout, e.ter, e.order_t = SC_TER, (r ~= nil and r >= 1 and r <= c.n) and r or 0, 0
                have = have - 1
                c.main = c.main + 1
            elseif e.order_t == 0 or (now - e.order_t > s("sn_reorder", SN_REORDER) and c.idle[k] == 1) then
                if CL.take(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, CL.DUR.rout, {category = e.cat}) then
                    ai.move({e.id}, e.look_x, e.look_y, {forced = true})
                    e.order_t, e.to_x, e.to_y = now, e.look_x, e.look_y
                    mem.orders = mem.orders + 1
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- DEMOLITION: the enemy's wall pieces and gates in a territory it has taken, a low task

local function demolition(c, mem, view, memory, now, e, dn)
    local r = e.ter
    if e.scout ~= SC_TER or e.idle ~= 1 or c.wall_x[r] < 0 or c.wall_id[r] == nil or push_from(mem, r) ~= nil then
        return false
    end
    if c.ebld[r] ~= 0 or c.foe[r] ~= 0 or not held(c, r) then
        return false
    end
    if dn[r] >= s("sn_demo", SN_DEMO) then
        return false
    end
    if not CL.take(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, CL.DUR.rout, {category = e.cat}) then
        return false
    end
    ai.attack({e.id}, c.wall_id[r])
    e.order_t, e.to_x, e.to_y, e.wait_t = now, c.wall_x[r], c.wall_y[r], 0
    dn[r] = dn[r] + 1
    mem.orders = mem.orders + 1
    mem.demos = mem.demos + 1
    return true
end

-- ---------------------------------------------------------------------------------------------------------------
-- ORDERS: the station's place, the enemies seen there, the push; as a group

-- one move for the bucket (the game's group move: the fast wait for the slow; AS A GROUP)
local function flush(c, mem, view, memory, b, now)
    local n = b.n
    if n == 0 then
        return
    end
    b.n = 0
    local ids, mi = {}, {}
    for j = 1, n do
        local e = mem.u[b.mi[j]]
        if CL.take(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, CL.DUR.rout, {category = e.cat}) then
            ids[#ids + 1] = e.id
            mi[#mi + 1] = b.mi[j]
        end
    end
    if #ids == 0 then
        return
    end
    ai.move(ids, b.dx, b.dy, {forced = b.fight == 0, formation = #ids > 1})
    for j = 1, #mi do
        local e = mem.u[mi[j]]
        e.order_t, e.to_x, e.to_y, e.wait_t = now, b.dx, b.dy, 0
    end
    mem.orders = mem.orders + 1
    if #ids > 1 then
        mem.groups = mem.groups + 1
        mem.grouped = mem.grouped + #ids
    end
end

local function orders(c, mem, view, memory, now)
    local buckets, nb = {}, 0
    local dn = {}
    for r = 1, c.n do
        dn[r] = 0
    end
    -- at a wall piece already (DEMOLITION): counted, left there
    for _, e in ipairs(mem.u) do
        if e.scout == SC_TER and e.ter >= 1 and e.ter <= c.n and e.order_t ~= 0 and c.wall_x[e.ter] >= 0
                and d2(e.to_x, e.to_y, c.wall_x[e.ter], c.wall_y[e.ter]) <= 2 * 2 then
            dn[e.ter] = dn[e.ter] + 1
        end
    end
    for k, e in ipairs(mem.u) do
        if e.scout == SC_TER and e.ter >= 1 and e.ter <= c.n then
            local r = e.ter
            if not CL.may(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, {category = e.cat}) then
                e.wait_t = 0   -- another's for now (ORDERS BY PRIORITY)
            elseif not demolition(c, mem, view, memory, now, e, dn) then
                local p = push_from(mem, r)
                local foe_here = c.fn[r] > 0 and held(c, r)
                local dx, dy, reach, fight, urgent
                if p ~= nil then
                    if e.pool == SNP_SIEGE then
                        dx, dy, reach, fight = p.mx, p.my, 4, 1   -- THE SIEGE'S ESCORT: never ahead to the building
                    else
                        dx, dy, reach, fight = p.at_x, p.at_y, 5, 1
                    end
                    urgent = 1
                elseif foe_here then
                    if e.pool == SNP_SIEGE and (mem.ac[r] or 0) < SN_SIEGE_ESCORT then
                        dx, dy, reach, fight = c.px[r], c.py[r], s("sn_near", SN_NEAR), 0
                    else
                        dx, dy, reach, fight = c.fx[r] // c.fn[r], c.fy[r] // c.fn[r], 5, 1
                    end
                    urgent = 1
                else
                    dx, dy, reach, fight = c.px[r], c.py[r], s("sn_near", SN_NEAR), e.pool ~= SNP_SIEGE and 1 or 0
                end
                if d2(e.x, e.y, dx, dy) <= reach * reach then
                    e.wait_t = 0
                else
                    local moved = e.order_t ~= 0 and d2(e.to_x, e.to_y, dx, dy) > 6 * 6
                    if e.order_t == 0 or (moved and now - e.order_t >= 10000)
                            or (now - e.order_t > s("sn_reorder", SN_REORDER) and c.idle[k] == 1) then
                        local cur = T.terr_at(e.x, e.y) or 0
                        local b
                        for j = 1, nb do
                            local o = buckets[j]
                            if o.r == r and o.cur == cur and o.fight == fight then
                                b = o
                                break
                            end
                        end
                        if b == nil then
                            nb = nb + 1
                            b = {r = r, cur = cur, fight = fight, dx = dx, dy = dy, ids = {}, mi = {}, n = 0,
                                 oldest = 0}
                            buckets[nb] = b
                        end
                        if b.n >= SN_GROUP_MAX then
                            flush(c, mem, view, memory, b, now)
                        end
                        b.ids[b.n + 1] = e.id
                        b.mi[b.n + 1] = k
                        b.n = b.n + 1
                        if e.wait_t == 0 then
                            e.wait_t = now
                        end
                        if b.oldest == 0 or e.wait_t < b.oldest then
                            b.oldest = e.wait_t
                        end
                        b.urgent = urgent or b.urgent
                    end
                end
            end
        end
    end
    for j = 1, nb do
        local b = buckets[j]
        if b.n > 0 and (b.n >= s("sn_group", SN_GROUP) or b.urgent
                or now - b.oldest >= s("sn_group_wait", SN_GROUP_WAIT)) then
            flush(c, mem, view, memory, b, now)
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The log

local function log(mem, c, now)
    if mem.log_t ~= 0 and now - mem.log_t < 60000 then
        return
    end
    mem.log_t = now
    local parts = {}
    for r = 1, c.n do
        if (mem.tc[r] or 0) > 0 or (mem.ac[r] or 0) > 0 then
            parts[#parts + 1] = string.format(" %d:%d/%d%s", r, mem.ac[r] or 0, mem.tc[r] or 0,
                (c.fn[r] or 0) > 0 and c.role[r] ~= T.TVR_NONE and "!" or "")
        end
    end
    local on = 0
    for k = 1, SN_PUSHES do
        if mem.push[k] ~= nil and mem.push[k].on ~= 0 then
            on = on + 1
        end
    end
    print(string.format("stations soldiers=%d main=%d siege=%d scouts=%d pushes on %d started %d won %d lost %d | "
        .. "stations changed %d, orders %d (%d to groups, %d soldiers), scouts back %d, walls set %d, "
        .. "orders taken for dead %d | territory:stationed/share:%s", mem.n, c.main, c.siege, c.scouts, on,
        mem.pushes, mem.won, mem.lost, mem.moved, mem.orders, mem.groups, mem.grouped, mem.scouted, mem.demos,
        mem.stale, table.concat(parts)))
    local ac_on = 0
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 and p.across ~= 0 then
            ac_on = ac_on + 1
        end
    end
    if ac_on > 0 or (mem.ferried or 0) > 0 or (mem.bh_laid or 0) > 0 then
        print(string.format("stations across: pushes on %d, soldiers ferried %d, bridgehead %d laid, %d standing, "
            .. "%d dropped, %d done, %d given up over the water", ac_on, mem.ferried or 0, mem.bh_laid or 0,
            mem.bh_built or 0, mem.bh_drop or 0, mem.bh_done or 0, mem.lost_across or 0))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The look

-- the soldier is in my group already
local function in_group(g, id)
    for _, u in ipairs(g.units) do
        if u == id then
            return true
        end
    end
    return false
end

local function look(view, memory, mem, now)
    local n = T.count()
    if n < 1 then
        return
    end
    local c = ctx_new(n)
    facts(c)
    mem.join, mem.transit = mem.join or {}, mem.transit or {}
    if across_on() then   -- ACROSS: the transports' places, the losses planned for
        SN_TR_CAP = transport_cap()
    end
    own_units(c, mem, now)
    if mem.n == 0 then
        mem.ac = count_stationed(c, mem, nil)
        return
    end
    enemy_facts(c)
    -- A STUCK ORDER (sn_idle), once a look
    for k, e in ipairs(mem.u) do
        c.idle[k] = stuck(e, now, mem) and 1 or 0
    end
    -- into the group: the soldiers no other manager holds (the handover from army.lua's force)
    local g = mem.gid ~= 0 and G.get(memory, mem.gid) or nil
    if g == nil then
        g = G.create(memory, {task = "defend", role = "stations", manual = true,
                              x = view.town_x, y = view.town_y})
        mem.gid = g.id
        g.x, g.y = view.town_x, view.town_y
        g.state = "stations"   -- a manual group: the lib keeps the list only (no forming move of G.add)
    end
    local news = {}
    for _, e in ipairs(mem.u) do
        if not in_group(g, e.id)
                and CL.may(view, memory, e.id, CL.PRIO.routine, CL.WHO.station, {category = e.cat}) then
            news[#news + 1] = e.id
        end
    end
    if #news > 0 then
        G.add(memory, g, news)
        mem.took = mem.took + #news
    end
    -- STATIONS and GIVING
    local tc = shares(c, mem)
    local main_ac = count_stationed(c, mem, SNP_MAIN)
    local budget = s("sn_moves", SN_MOVES)
    local _, left = give(c, mem, tc, main_ac, SNP_MAIN, budget, now)
    if c.siege > 0 then
        local tcs = siege_shares(c, tc)
        give(c, mem, tcs, count_stationed(c, mem, SNP_SIEGE), SNP_SIEGE, left, now)
    end
    mem.ac = count_stationed(c, mem, nil)
    -- A PUSH (and ACROSS: the ferry's waves, the bridgehead)
    pushes_on(c, mem, view, memory, now)
    pushes_new(c, mem, mem.ac, now)
    -- SCOUTS and PATROLS
    scouts(c, mem, view, memory, now)
    patrols(c, mem, view, memory, now)
    -- ORDERS
    orders(c, mem, view, memory, now)
    log(mem, c, now)
end

-- ---------------------------------------------------------------------------------------------------------------
-- The public API

local function state(memory)
    local mem = memory.stations
    if mem == nil then
        mem = new_state()
        memory.stations = mem
    end
    return mem
end

-- the soldiers back to the engine (army.lua's force takes them again as its free soldiers); called when the setting
-- turns off - M.step calls it itself while stations is off, so it may be called unconditionally
function M.release(view, memory)
    local mem = memory.stations
    if mem == nil or mem.gid == 0 then
        return false
    end
    local g = G.get(memory, mem.gid)
    if g ~= nil then
        G.disband(memory, g)
    end
    mem.gid = 0
    mem.u, mem.byid, mem.n = {}, {}, 0
    mem.gave_back = mem.gave_back + 1
    return true
end

-- every step: the look every SN_EVERY, nothing in between (or the soldiers back while stations is off)
function M.step(view, memory, orders, budget)
    if S.stations ~= true then
        return M.release(view, memory)
    end
    local mem = state(memory)
    local now = view.time
    if now < mem.t then   -- a new game
        memory.stations = new_state()
        mem = memory.stations
    end
    if mem.t ~= 0 and now >= mem.t and now - mem.t < s("sn_every", SN_EVERY) then
        return
    end
    mem.t = now_of(now)
    look(view, memory, mem, now)
    return nil
end

-- a new game: nothing of the last one (the soldiers the engine's again)
function M.clear(memory)
    local mem = memory.stations
    if mem ~= nil and mem.gid ~= 0 then
        local g = G.get(memory, mem.gid)
        if g ~= nil then
            G.disband(memory, g)
        end
        mem.gid = 0
    end
    memory.stations = nil
end

-- the soldiers it holds now
function M.count(memory)
    local mem = memory.stations
    return mem ~= nil and mem.n or 0
end

-- a territory's share (in soldiers) and what stands stationed there, as of the last look
function M.share(memory, r)
    local mem = memory.stations
    if mem == nil or r == nil then
        return 0
    end
    return mem.tc[r] or 0
end

function M.stationed(memory, r)
    local mem = memory.stations
    if mem == nil or r == nil then
        return 0
    end
    return mem.ac[r] or 0
end

-- the pushes on: {{from, into, x, y, n, mn, ratio}, ...}
function M.pushes(memory)
    local out = {}
    local mem = memory.stations
    if mem == nil then
        return out
    end
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 then
            out[#out + 1] = {from = p.from, into = p.into, x = p.at_x, y = p.at_y, n = p.n, mn = p.mn,
                             ratio = p.ratio}
        end
    end
    return out
end

-- is a push of its on into a territory (build.lua's claim may want it; upstream's sn_holding)
function M.holding(memory, r)
    local mem = memory.stations
    if mem == nil then
        return false
    end
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 and p.into == r then
            return true
        end
    end
    return false
end

-- the counters of the minute line (the integrator's log)
function M.counts(memory)
    local mem = memory.stations
    if mem == nil then
        return nil
    end
    return {n = mem.n, moved = mem.moved, orders = mem.orders, pushes = mem.pushes, won = mem.won, lost = mem.lost,
            scouted = mem.scouted, demos = mem.demos, stale = mem.stale, took = mem.took}
end

-- ---------------------------------------------------------------------------------------------------------------
-- ACROSS: what the ferry asks (ferry.lua's waves; nothing here runs while the switch is off)

-- the pushes over the water that are on, for the ferry's waves: {{k, from, from_land, to_land, beach_x, beach_y,
-- left, over, waves, cits, at_x, at_y, x, y}, ...} (ferry.c fy_look)
function M.across(memory)
    local out = {}
    local mem = memory.stations
    if mem == nil or not across_on() then
        return out
    end
    for k = 1, SN_PUSHES do
        local p = mem.push[k]
        if p ~= nil and p.on ~= 0 and p.across ~= 0 then
            local t = T.info(p.from)
            out[#out + 1] = {k = k, from = p.from, from_land = p.from_land, to_land = p.to_land,
                             beach_x = p.beach_x, beach_y = p.beach_y, left = p.left or 0, over = p.over or 0,
                             waves = p.waves or 0, cits = (p.cits_want or 0) - (p.ncits or 0),
                             at_x = p.at_x, at_y = p.at_y, x = t ~= nil and t.x or 0, y = t ~= nil and t.y or 0}
        end
    end
    return out
end

-- push k's soldiers still on the land it left, nearest (x, y) first, at most max (sn_push_pick): the ferry takes them
function M.push_pick(view, memory, k, x, y, max)
    local out = {}
    local mem = memory.stations
    if mem == nil or k == nil or not across_on() then
        return out
    end
    local p = mem.push[k]
    if p == nil or p.on == 0 or p.across == 0 then
        return out
    end
    local cand = {}
    for _, e in ipairs(mem.u) do
        if e.ter == p.from and e.scout == SC_TER and e.pool ~= SNP_AA and e.hp > 0
                and T.area_of(T.terr_at(e.x, e.y) or 0) ~= p.to_land then
            cand[#cand + 1] = {id = e.id, d = d2(e.x, e.y, x, y), cat = e.cat}
        end
    end
    table.sort(cand, function(a, b)
        if a.d ~= b.d then
            return a.d < b.d
        end
        return a.id < b.id
    end)
    local want = max or 16
    for i = 1, #cand do
        if #out >= want then
            break
        end
        -- ORDERS BY PRIORITY: not one another manager took (the ferry's own already aboard are hidden from the look)
        if CL.may(view, memory, cand[i].id, CL.PRIO.carry, CL.WHO.ferry, {category = cand[i].cat}) then
            out[#out + 1] = cand[i].id
        end
    end
    return out
end

-- the ferry put a unit off (ferry.c's sn_ferried): it takes its station (ter) again - push k's soldier, a scout; kept
-- SN_JOIN_T for the look that has not seen it yet
function M.ferried(memory, id, ter, scout)
    local mem = memory.stations
    if mem == nil or id == nil then
        return
    end
    mem.join = mem.join or {}
    mem.join[id] = {ter = ter or -1, scout = scout and 1 or 0, t = ai.time()}
    mem.ferried = (mem.ferried or 0) + 1
end

-- a citizen of push k put off by its beach (sn_bh_land): the bridgehead's from now on
function M.bh_land(memory, k, id)
    local mem = memory.stations
    if mem == nil or id == nil then
        return
    end
    local p = mem.push[k]
    if p == nil or p.on == 0 or (p.ncits or 0) >= SN_CITS then
        return
    end
    p.cits[#p.cits + 1] = id
    p.ncits = (p.ncits or 0) + 1
    ai.take(id)
end

-- THE BRIDGEHEAD: how many citizens push k's first wave still wants, and the ferry's count of them (ferry.c cits_want)
function M.bh_want(memory, k)
    local mem = memory.stations
    local p = mem ~= nil and k ~= nil and mem.push[k] or nil
    if p == nil or p.on == 0 or p.across == 0 or (p.waves or 0) > 0 then
        return 0
    end
    local w = (p.cits_want or 0) - (p.ncits or 0)
    return w > 0 and w or 0
end

function M.bh_want_set(memory, k, n)
    local mem = memory.stations
    local p = mem ~= nil and k ~= nil and mem.push[k] or nil
    if p == nil then
        return
    end
    p.cits_want = n or 0
end

-- a push's wave is under way (ONE WAVE AT A TIME): counted for THE LAST WORD and the bridgehead's first wave
function M.push_wave(memory, k)
    local mem = memory.stations
    local p = mem ~= nil and k ~= nil and mem.push[k] or nil
    if p == nil or p.on == 0 then
        return
    end
    p.waves = (p.waves or 0) + 1
    p.wave_t = 0
end

-- THE LAST WORD (sn_last_word): push k's first wave aboard, the soldiers ids - judged at once against the beach and the
-- target, its losses planned for: 1 go, 0 wait, -1 called off (under SN_LAST for SN_LAST_WEAK)
function M.last_word(view, memory, k, ids, now)
    local mem = memory.stations
    local p = mem ~= nil and k ~= nil and mem.push[k] or nil
    if p == nil or p.on == 0 or p.across == 0 then
        return 1
    end
    if (p.over or 0) ~= 0 or (p.waves or 0) > 1 then
        return 1   -- a later wave goes to those over
    end
    local n = ids ~= nil and #ids or 0
    if n == 0 then
        return 1
    end
    if p.judge_t == 0 or now - p.judge_t >= 2000 then
        p.judge_t = now_of(now)
        local cols = ai.get(ids, {"hp", "attack"})
        local own = 0
        for i = 1, n do
            own = own + rnd(cols.hp[i]) * rnd(cols.attack[i])
        end
        p.judge_r = judge_across({}, n, own, p.at_x, p.at_y, p.beach_x, p.beach_y)
    end
    if p.judge_r >= SN_LAST then
        p.weak_t = 0
        return 1
    end
    if p.weak_t == 0 then
        p.weak_t = now_of(now)
    end
    if now - p.weak_t < SN_LAST_WEAK then
        return 0
    end
    return -1
end

-- the ferry could not carry the push (THE LAST WORD called it off, the sea held): over, SN_CALLED_OFF before the next
function M.push_off(memory, k, now)
    local mem = memory.stations
    local p = mem ~= nil and k ~= nil and mem.push[k] or nil
    if p == nil or p.on == 0 then
        return
    end
    if p.cits ~= nil and #p.cits > 0 then
        ai.release(p.cits)
        p.cits = {}
    end
    p.on, p.end_t, p.wait, p.ncits = 0, now_of(now), SN_CALLED_OFF, 0
    if p.from >= 1 then
        mem.calm[p.from] = p.end_t + SN_CALLED_OFF
    end
    for _, e in ipairs(mem.u) do
        if e.ter == p.from then
            e.order_t = 0
        end
    end
    mem.lost_across = (mem.lost_across or 0) + 1
end

-- the ferry's passengers still to cross, by push (fy_push_transit): counted as still to cross by the next look
function M.transit(memory, counts)
    local mem = memory.stations
    if mem == nil then
        return
    end
    mem.transit = counts or {}
end

return M
