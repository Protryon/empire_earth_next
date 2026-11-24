-- Age of AI port, the air force (upstream dll/ai/airstrike.c, AS_ON since 2026-11-04). GPL-3.0: see UPSTREAM in this
-- folder for the licence and the per-file map. require("ageofai.air").
--
-- Every AS_EVERY (5 s) from the bombers' epochs (World War I: view.epoch >= 9, 0-based; upstream current_epoch + 1 >= 10):
-- - HELD: every bomber (the bomber family's airplanes, not the atomic ones) is the script's from its first look
--   (ai.take: the engine's air subgroups never get it, as upstream's player_hold_unit).
-- - TRAINED: while a position needs the wing (seen within AF_STRONG_MS), every AF_TRAIN_EVERY the wing wanted -
--   AF_WING (8), or what the last position needed if more, at most AF_WING_MAX - less the bombers it has and those
--   asked for and not out yet - is trained: the newest bomber the player can, at most 4 a request, paid like the
--   siege controller's priority orders (the stock above the epoch reserve, budget.civil), queued at the finished
--   airports (ai.train takes the least busy: the planes take off from all of them). An Airport is ordered while
--   there are fewer than one per AF_PER_AIRPORT of the wing (up to AF_AIRPORTS; AF_AIRPORT_TRIES queued without one
--   more there, none until there is; one a minute): at the force's staging point when that is AF_FORWARD tiles or
--   more from the base (the forward airfield), else where the engine's placement puts it (near = "town").
-- - THE SHOOTERS first: where a land unit hit the AI's units from AF_SHOOT_D tiles or more (the attacked events with
--   a seen attacker; a field gun - siege, artillery, the medieval field weapons, the cannons - from AF_GUN_D), spots
--   within AF_SHOOT_JOIN joined, for AF_SHOOT_MS after their last hit; the one hit from most often is the position,
--   its long-range units there the first targets after the anti-air; a spot with nothing left to hit is dropped.
--   The spots are kept in every age and with the air force off (skirmish.c's sorties go at them too).
-- - THE WING: else the position - the push's destination while the force is out, else for AF_STRONG_MS the target
--   its last attack time found too strong (army.lua's "force wait": memory.army.strong_x/y/t) -; the anti-air near it
--   (AA buildings seen or remembered, land AA seen, within AS_AA_R) and what holds it (defences with a ground attack,
--   then siege or artillery seen within AS_R, then the soldiers - the nearest first). Needed: a flight of AF_FLIGHT
--   per anti-air, one more for the defences. The bombers ready - no goal (landed, or circling idle) with AF_READY_PCT
--   of their hit points - launch together when they are that many: a flight at each anti-air, the rest in flights
--   at the defences (ai.attack; one call per target). A bomber whose target still stands is left alone while it has
--   any goal and for AS_AGAIN after its order (a plane changes its goals by itself on a run); idle again it goes
--   again. A target down: its bombers back to the ready. A target not down AF_RUN_MS after the launch is given up
--   (one wing at a time). A wing's targets all down: the force judges its push again at once (memory.army.next_attack
--   = now, g_ar_push_now).
--
-- THE DOCTRINE ([ai_base] bomber_doctrine, on since 2026-11-21, its parts bomber_survive / bomber_come_home /
-- bomber_value_wing / bomber_claim on under it; [ai_base] bomber_harass off, its lone runs under the key). The
-- player's rules: "undefended doesn't mean fully undefended here, just not enough to bring the bomber down on its
-- own", "a damaged bomber isn't a loss at all as long as it can return", "the wings are meant to overwhelm in numbers
-- so that even a few lost bombers still turn a net positive"; on counter-fire: "a deliberate task, not stolen from
-- other running tasks".
-- - THE PICTURE (dct_scan, every AS_EVERY while bombers are held): what of the enemy can hit its bombers by the
--   game's own numbers - a type ai.can_hurt lets hit the bomber type held most, its shot at AD_HIT (35%) of the
--   class's attack a reload that lands, over its reach (range + AD_PAD, a mover's AD_CHASE_S of its speed): the guns
--   seen, AA buildings remembered within AH_MEM, an enemy's strongest gun assumed by each town of its not in sight,
--   the strongest known where a bomber was lost; and its fighters (an enemy with an Airport or carrier and none seen
--   gets AD_FIGHTERS, 2) as a cover AH_FIGHTER_R (20) round its buildings, each a fight of AD_FIGHT_S (20 s). A
--   way's damage: each gun's fire for the time the way is in its reach at the bomber's speed, the fighters' for the
--   time in their cover, all of it on the one bomber (as guns aim).
-- - THE SMALL RUNS (bomber_survive; in place of the lone runs' no-anti-air rule): 1 to AD_FLIGHT (4) from one
--   Airport, as many as the target's hit points take, the soundest first, at the target worth most for the way
--   (buildings, working citizens, the counter-fire's shooters) whose damage there, over it and home x AD_MARGIN, with
--   AD_RESERVE of a bomber's hit points left, is below the weakest one's hit points now.
-- - THE WAY HOME (bomber_come_home, dct_home, every second): a bomber in the air whose hit points no longer take it
--   through what is left of its sortie - a small run's: to its target, over it and home; the wing's: to its target and
--   half the time over it, the fire shared by the wing's bombers - is called home while they still take it home; one
--   that cannot make it home either way goes on with its bombs.
-- - THE VALUE WING (bomber_value_wing; in place of the count): the positions - the force's (the shooters' spot, the
--   push's destination, the target found too strong), AD_PUSH_W more, and the enemy's buildings by AD_CELL cell, the
--   AD_CANDS worth most - weighed for each number of the bombers ready: the worth they destroy (prices; an AA gun one
--   bomber more; a target they cannot destroy AD_PARTIAL of the damage) less the bombers expected lost (each gun
--   firing as long as any of them is in its reach, so numbers share its fire; x AD_MARGIN, a bomber lost for every
--   hit point total). Launched at once where the gain is highest, no more than AD_LOSS_MAX (40%) of them expected
--   lost, and the rest fly the small runs. Trained for: the fewest that would gain at the force's position.
-- - NO THEFT (bomber_claim): a bomber on any task of the air force's is claimed in claims.lua (CLW_BOMBERS at
--   CLP_TASK) and given up when it has none; the counter-fire takes no bombers as soldiers (its candidates are land
--   soldiers), and the shooters it answers are the small runs' targets at AD_CF_W (2) their worth.
-- - THE LONE RUNS (bomber_harass, off upstream; [autotest] v_bomber_harass): the wing waits with no time limit and
--   its bombers sit in their Airports meanwhile (the player: "the bombers shouldnt sit idle, the wings are an way to
--   overwelm defenses, lone bombers could still harees undefended targets"; measured within the noise, off - "it is
--   for an enemy without anti-air, not for these"). While the wing gathers or flies, or no position needs it, the
--   bombers free go in flights of AH_FLIGHT (2) from one Airport at what the AI knows of an enemy - buildings (not
--   walls) and citizens at work (two or more within 4 tiles) - that is undefended as far as it knows: no fighter or
--   Airport of that enemy, the ground round the target in sight, and, of an enemy known to have anti-air, the way
--   there too; no gun's reach within AH_MARGIN (AH_AA_MIN at least) of the target or the straight way, nor where a
--   bomber on a run was lost within AH_MEM, nor assumed by a town of an enemy not in sight; the most worth (its
--   price, at most 800; 120 a citizen) for the way, one flight a target at a time. A bomber on a run is claimed,
--   called back to its Airport (a forced move: no EEMReturnToBase in the API) when it goes after something else, its
--   target is down, or it is AH_STRAY (6) off the way; free again back home or AH_RUN_MS (150 s) on.
--
-- Not ported: g_ar_breach's focus as the position (armies.c AR_BREACH, off upstream and not ported), ar_price_clear
-- (the port keeps no price), the [autotest] variants and the measurement lines (# bombers: a summary a minute here).
-- Deviations (fog-honest, engine gaps): a target that is a unit out of sight is "gone" (upstream's all-knowing
-- unit_by_id kept it as standing until off the map); a long-range unit at the shooters' spot counts only while seen
-- (upstream let a known one count unseen). ai.class exposes no damage-class or hit tables and no armour (a shot is
-- its attack, AD_HIT the measured share that lands - stats.lua's stand-in) and no ammunition (AD_NOAMMO 4 shots a
-- sortie); no per-tile last-seen time ("looked at" is "in sight now"); the fighters' cover time is the longest of
-- their buildings' disks the way crosses, not a cell grid's union; an enemy's upgrades are the reader's own class
-- data (ai.class has no player argument); a bomber parked in its Airport is launched by its Airport's rally at the
-- target (airguard.lua's stand-in: the engine has no ai.air, and the rally launches the Airport's whole queue), and
-- called home with a forced move (the engine lands it at its own base, no rebase); the counter-fire's request is the
-- shooters' spots from the same attacked events, not a call into the air force; the counters are upstream's, the
-- lines a summary a minute. THE LOOK'S COST (a Lua step, not C): the wing weighs AD_WING_TARGETS (12) targets a
-- position and a look spends AD_WORK (4000) chords, the positions worth most first (upstream: 64 a position, every
-- one); the way in and the way out cross the same guns, so their times are one leg doubled (upstream computes both);
-- the picture reads the enemy in sight to 128 units and 64 remembered buildings a look (upstream: every unit); the
-- measured worst was 1.0M VM instructions in one step (a pathological 30-AA, 120-target scene) and 0.6M in a
-- 12-bomber, 4-Airport one; the other four steps in five cost the way home only. Interface: a bomber landed in its
-- airport is "inside" (a container): ai.attack refuses it (cannot_do) where upstream's game air order (0x442AD8)
-- launched it - reported as a gap; the refusals are counted (memory.air.refused) and the order comes again every
-- AS_AGAIN.
--
-- THE RECORD SHARED WITH skirmish.c (skirmish.lua reads it): memory.army.shooters = {{x=, y=, t=, hits=}, ...}, up
-- to AF_SHOOTS spots; a spot is alive while t > 0 and now - t < AF_SHOOT_MS (t: its last hit; hits: the hits seen
-- from it; t = 0: dropped, nothing left to hit there). M.shooters_spot(memory.army, now) -> x, y of the live spot hit
-- from most often, or nil. The spots are updated by M.step from the step's attacked events before any epoch check.
-- Its own state: memory.air (the held list: {id, tgt, t, t0} per bomber plus the doctrine's books and, with the
-- doctrine on, the picture of the anti-air: memory.air.pic; the counters).
local S = require("ageofai.settings")
local V = require("lib.view")
local G = require("lib.groups")
local C = require("ageofai.claims")   -- [claims.c] the bombers' claim (CLW_BOMBERS at CLP_TASK; WHO.bombers)

local M = {}

local AS_EVERY = 5000
local AS_R = 14
local AS_AA_R = 14
local AS_TARGETS = 32
local AS_BOMBERS = 128
local AS_AGAIN = 20000
local AF_FLIGHT = 4
local AF_WING = 8
local AF_WING_MAX = 24
local AF_PER_AIRPORT = 4
local AF_AIRPORTS = 6
local AF_READY_PCT = 60
local AF_TRAIN_EVERY = 20000
local AF_STRONG_MS = 300000
local AF_AIRPORT_TRIES = 3
local AF_FORWARD = 15
local AF_SHOOT_D = 7
local AF_GUN_D = 5
local AF_SHOOT_JOIN = 5
local AF_SHOOT_MS = 180000
local AF_SHOOTS = 16
local AF_RUN_MS = 180000
local BOMBER_EPOCH = 9   -- World War I, 0-based (upstream: current_epoch + 1 >= 10)

-- THE DOCTRINE's numbers (airstrike.c AD_*/AH_*)
local AD_THREATS = 64        -- (upstream 384: the threats kept from the picture)
local AD_MARGIN = 150        -- the damage expected x 1.5 (what isn't seen: guns out of sight, fighters, turning shots)
local AD_RESERVE = 10        -- percent of a bomber's hit points left when it lands
local AD_OVER_S = 8          -- seconds over the target: the run in, its bombs, the turn away
local AD_PAD = 50            -- centi-tiles beyond a gun's range it still hits (the plane's size)
local AD_HIT = 35            -- percent of a gun's shot a reload that lands on a bomber in its range (measured)
local AD_CHASE_S = 3         -- a gun that moves closes in for this long
local AD_MEM_MOB = 90000     -- a remembered mover's fire fades to none over this long
local AD_FIGHT_S = 20        -- a fighter at a bomber at most this long
local AD_FIGHTERS = 2        -- fighters assumed of an enemy with an Airport and none seen
local AD_FLIGHT = 4          -- a small run: at most this many from one Airport
local AD_PUSH_W = 150        -- the force's position is worth 1.5 x more
local AD_CANDS = 6           -- other positions weighed for the wing: the enemy's buildings by AD_CELL cell
local AD_CELL = 16
local AD_HOME_EVERY = 1000
local AD_MARGIN_HOME = 120   -- the way home's margin: below the launch's (a bomber breaks off when it fares worse)
local AD_VAL_MAX = 1500
local AD_PARTIAL = 50        -- a target damaged, not destroyed: this percent of the damage's worth
local AD_LOSS_MAX = 40       -- a wing expected to lose more than this percent of its bombers doesn't go
local AD_NOAMMO = 4          -- a bomber type without ammunition: its shots a sortie
local AD_CF_W = 200          -- the counter-fire's shooters as targets: this percent of their worth
local AD_WING_TARGETS = 12   -- a position's targets weighed by the wing (upstream's 64 a position: the cost here)
local AD_WORK = 4000         -- the wing look's chords at most a look (the positions and targets most worth first)

local AH_FLIGHT = 2
local AH_MARGIN = 6          -- tiles beyond a gun's range a lone run keeps clear of
local AH_AA_MIN = 10
local AH_FIGHTER_R = 20
local AH_MEM = 300000
local AH_RUN_MS = 150000
local AH_AA = 48             -- (upstream AH_AA 256: the guns kept)
local AH_CIT = 24
local AH_TARGETS = 32        -- (upstream AH_TARGETS 512)
local AH_FLAK = 16
local AH_FLAK_R = 12
local AH_LOOKED = 60000
local AH_LOOK_R = 12
local AH_WAY_STEP = 8
local AH_STRAY = 6
local AH_BLD = 64            -- (upstream AH_BLD 2048: the remembered buildings read a scan)
local AH_CELL = 8

local FAM_AIRCRAFT, FAM_CARRIER = 5, 50   -- dbfamily: aircraft 5, aircraft_carrier 50

-- the bombers to train, the newest first (upstream k_af_types; the epochs 0-based here: upstream's less one)
local AF_TYPES = {
    {type = "a_bomber_13_b_122_wyvern", epoch = 12}, {type = "a_bomber_12_stealth_b_2", epoch = 11},
    {type = "a_bomber_11_b_17", epoch = 10}, {type = "a_bomber_11_heinkel_111", epoch = 10},
    {type = "a_bomber_10_gotha", epoch = 9},
}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function isqrt(v)
    local r = math.floor(math.sqrt(v))
    while r * r > v do
        r = r - 1
    end
    while (r + 1) * (r + 1) <= v do
        r = r + 1
    end
    return r
end

-- the bomber family's airplanes (dbobjects family bomber, category 3; not the atomic bombers). ai.class carries no
-- family: by the class names of the data
function M.is_bomber(t)
    return string.sub(t or "", 1, 9) == "a_bomber_" or t == "x11_me110" or t == "a_catalina"
end

-- the aa_building and land_aa families (the 88 mm gun and the missile towers; the AA tank, the stinger soldier,
-- the missile trooper, the anti-missile battery)
function M.is_aa(t)
    t = t or ""
    return string.sub(t, 1, 4) == "b_aa" or string.sub(t, 1, 2) == "aa" or string.find(t, "_aa_", 1, true) ~= nil
        or string.find(t, "missile_trooper", 1, true) ~= nil or string.find(t, "anti_missile", 1, true) ~= nil
end

-- a field gun (intel.c sh_gun): siege, artillery, the medieval field weapons (the ballista), the cannons
function M.is_gun(t)
    t = t or ""
    return string.sub(t, 1, 9) == "gun_siege" or string.sub(t, 1, 13) == "gun_artillery" or string.sub(t, 1, 9) == "gun_spear"
        or string.sub(t, 1, 10) == "gun_cannon" or string.sub(t, 1, 5) == "siege"
end

-- siege or artillery (upstream is_siege || KIND_ARTILLERY: after the defences in the target order)
local function is_siege_or_artillery(t)
    t = t or ""
    return string.sub(t, 1, 9) == "gun_siege" or string.sub(t, 1, 13) == "gun_artillery" or string.sub(t, 1, 5) == "siege"
end

-- ---------------------------------------------------------------------------------------------------------------
-- The shooters' spots (memory.army.shooters)

local function spot_alive(sp, now)
    return sp.t > 0 and now - sp.t < AF_SHOOT_MS
end

-- the spot hit from most often that is alive: its index, or nil
local function spot_index(army, now)
    local sh = army.shooters or {}
    local shoot = nil
    for j = 1, #sh do
        if spot_alive(sh[j], now) and (shoot == nil or sh[j].hits > sh[shoot].hits) then
            shoot = j
        end
    end
    return shoot
end

function M.shooters_spot(army, now)
    local j = spot_index(army, now)
    if j == nil then
        return nil
    end
    return army.shooters[j].x, army.shooters[j].y
end

-- as_shooters: the attacked events' seen attackers that hit from afar, joined into spots
local function shooters(army, events, now)
    army.shooters = army.shooters or {}
    local sh = army.shooters
    local done = {}
    for _, e in ipairs(events or {}) do
        if e.kind == "attacked" and e.attacker ~= nil and not done[e.attacker] then
            done[e.attacker] = true
            local a = ai.unit(e.attacker)
            local v = ai.unit(e.unit)
            if a ~= nil and v ~= nil and a.category ~= "building" and v.category ~= "resource" and a.x ~= nil and v.x ~= nil then
                local d = isqrt(d2(a.x, a.y, v.x, v.y))
                if d >= (M.is_gun(a.type) and AF_GUN_D or AF_SHOOT_D) then
                    local j, old = nil, 1
                    for k = 1, #sh do
                        if spot_alive(sh[k], now) and d2(sh[k].x, sh[k].y, a.x, a.y) <= AF_SHOOT_JOIN * AF_SHOOT_JOIN then
                            j = k
                            break
                        end
                        if sh[k].t < sh[old].t then
                            old = k
                        end
                    end
                    if j == nil then
                        if #sh < AF_SHOOTS then
                            j = #sh + 1
                        else
                            j = old
                        end
                        sh[j] = {x = a.x, y = a.y, hits = 0, t = 0}
                    end
                    sh[j].hits = sh[j].hits + 1
                    sh[j].t = now > 0 and now or 1
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- HELD: the bombers the script's; the held list without the dead

local function held_at(s, id)
    for k = 1, #s.held do
        if s.held[k].id == id then
            return k
        end
    end
    return nil
end

-- the own bombers (records by id, the ids in order); new ones taken and listed. now (optional): the doctrine's books
-- (a bomber lost: by where it was, and a lone run's last place kept clear of for AH_MEM)
function M.held(memory, now)
    memory.air = memory.air or {held = {}}
    local s = memory.air
    s.held = s.held or {}
    local bombers, ids = {}, {}
    for _, id in ipairs(ai.units{category = "air"}) do
        local u = ai.unit(id)
        if u ~= nil and M.is_bomber(u.type) and #ids < AS_BOMBERS then
            bombers[id] = u
            ids[#ids + 1] = id
        end
    end
    local keep = {}
    for _, e in ipairs(s.held) do
        if bombers[e.id] ~= nil then
            keep[#keep + 1] = e
        else
            -- THE DOCTRINE's books (upstream's, the doctrine on or not): the bombers lost by where they were
            s.lost = (s.lost or 0) + 1
            if (e.har or 0) > 0 then
                s.lost_har = (s.lost_har or 0) + 1
            end
            if (e.landed or 0) == 1 then
                s.lost_ground = (s.lost_ground or 0) + 1
            elseif (e.home2 or 0) == 1 then
                s.lost_home = (s.lost_home or 0) + 1
            elseif (e.har or 0) == 0 then
                if e.tgt ~= 0 then
                    s.lost_wing = (s.lost_wing or 0) + 1
                else
                    s.lost_air = (s.lost_air or 0) + 1
                end
            end
            -- THE LONE RUNS: shot down by something it did not know of - its place and its target's kept clear of
            if (e.har or 0) > 0 then
                s.flak = s.flak or {}
                for j = 0, 1 do
                    local rec = {x = j == 0 and (e.at_x or 0) or (e.tat_x or 0),
                                 y = j == 0 and (e.at_y or 0) or (e.tat_y or 0),
                                 t = (now or 0) > 0 and now or 1}
                    if #s.flak < AH_FLAK then
                        s.flak[#s.flak + 1] = rec
                    else
                        s.flak_i = ((s.flak_i or 1) - 1) % AH_FLAK + 1
                        s.flak[s.flak_i] = rec
                    end
                end
            end
        end
    end
    s.held = keep
    local take = {}
    for _, id in ipairs(ids) do
        if held_at(s, id) == nil and #s.held < AS_BOMBERS then
            if bombers[id].held ~= "lua" then
                take[#take + 1] = id
            end
            -- the doctrine's fields: on a lone run (har), its Airport (home), called back (back), the damage expected
            -- (pred), counter-fire's (cf), its target's type, out on a sortie (out), in an Airport at the last look
            -- (landed), its hit points then and last, where it was last seen, its run's start and target
            s.held[#s.held + 1] = {id = id, tgt = 0, t = 0, t0 = 0, har = 0, home = 0, back = 0, pred = 0, cf = 0,
                                   ttype = "", out = 0, landed = 0, hp_last = bombers[id].hp or 0,
                                   at_x = bombers[id].x or 0, at_y = bombers[id].y or 0,
                                   tat_x = 0, tat_y = 0, from_x = 0, from_y = 0}
            if (s.asked or 0) > 0 then
                s.asked = s.asked - 1   -- one of those asked for is out
            end
        end
    end
    if #take > 0 then
        ai.take(take)
    end
    return bombers, ids
end

-- ---------------------------------------------------------------------------------------------------------------
-- TRAINED: the wing wanted, the airports for it

-- the finished Airports (ids) and the count of those still going up
local function airports()
    local built, building = {}, 0
    for _, id in ipairs(ai.units{type = "b_airport"}) do
        local u = ai.unit(id)
        if u ~= nil then
            if u.built then
                built[#built + 1] = id
            else
                building = building + 1
            end
        end
    end
    return built, building
end

local function train(view, memory, s, order, budget, have, now)
    if (s.train_t or 0) > 0 and now - s.train_t < AF_TRAIN_EVERY then
        return
    end
    s.train_t = now > 0 and now or 1
    if (s.need_t or 0) == 0 or now - s.need_t >= AF_STRONG_MS then
        return   -- no position needs it
    end
    if (s.asked or 0) > 0 and now - (s.asked_t or 0) >= AF_STRONG_MS then
        s.asked = 0   -- asked long ago and none came: the queue is gone
    end
    s.asked = s.asked or 0
    local want = math.max(s.needed or 0, AF_WING)
    if want > AF_WING_MAX then
        want = AF_WING_MAX
    end
    if have + s.asked >= want then
        return
    end
    local built, building = airports()
    local ap = #built
    local ap_want = math.min((want + AF_PER_AIRPORT - 1) // AF_PER_AIRPORT, AF_AIRPORTS)
    if ap + building > (s.ap_seen or 0) then
        s.ap_tries = 0   -- one more came of it
    end
    s.ap_seen = ap + building
    local e = view.epoch
    local queued = V.total(V.buildings(view, "b_airport")) - ap - building
    if ap + building < ap_want and V.cost(view, "b_airport") ~= nil and queued <= 0 and e >= 0 and e < 14
            and (s.ap_tries or 0) < AF_AIRPORT_TRIES and ((s.airport_t or 0) == 0 or now - s.airport_t >= 60000) then
        s.airport_t = now > 0 and now or 1
        s.ap_tries = (s.ap_tries or 0) + 1
        local army = memory.army or {}
        local hx, hy = army.home_x or -1, army.home_y or -1
        local fwd = view.town_x >= 0 and hx >= 0 and d2(hx, hy, view.town_x, view.town_y) >= AF_FORWARD * AF_FORWARD
        local x, y = nil, nil
        if fwd then
            x, y = ai.find_site("b_airport", hx, hy, {min = 0, max = 12})
        end
        if x ~= nil then
            order({verb = "build", type = "b_airport", near = "tile", x = x, y = y})
        else
            order({verb = "build", type = "b_airport", near = "town"})
        end
        s.airports = (s.airports or 0) + 1
        print(string.format("air airport %s=%d,%d have=%d want=%d wing=%d building=%d", x ~= nil and "forward" or "town",
            x or view.town_x, y or view.town_y, ap, ap_want, want, building))
    end
    if ap == 0 then
        return
    end
    for _, k in ipairs(AF_TYPES) do
        local c = V.cost(view, k.type)
        if k.epoch <= e and c ~= nil then
            local n = math.min(want - have - s.asked, 4)
            -- the siege controller's priority order (g_priority_mil): the stock above the epoch reserve
            for _, r in ipairs(V.resources) do
                if c.cost[r] > 0 then
                    -- [priority.c] a priority order's stock: what the list leaves it (budget.order)
                    n = math.min(n, ((budget.order or budget.civil) and (budget.order or budget.civil)[r] or 0) // c.cost[r])
                end
            end
            if n > 0 then
                local code, reason, got = ai.train(built, k.type, n)
                if got > 0 then
                    s.trained = (s.trained or 0) + got
                    s.asked = s.asked + got
                    s.asked_t = now > 0 and now or 1
                    print(string.format("air trained n=%d type=%s have=%d wing=%d airports=%d", got, k.type, have, want, ap))
                    return
                end
                s.train_refused = (s.train_refused or 0) + 1
                s.train_reason = reason
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The position and its targets

-- the position: the shooters' spot, else the push's destination, else the target found too strong lately
function M.position(memory, now)
    local army = memory.army or {}
    local shoot = spot_index(army, now)
    if shoot ~= nil then
        return army.shooters[shoot].x, army.shooters[shoot].y, shoot
    end
    -- (g_ar_breach's focus: AR_BREACH is off upstream and not ported)
    local force = G.role(memory, "force")
    if army.out and force ~= nil and force.task == "attack" and (force.x or -1) >= 0 then
        return force.x, force.y, nil
    end
    if (army.strong_t or 0) > 0 and now - army.strong_t < AF_STRONG_MS and (army.strong_x or -1) >= 0 then
        return army.strong_x, army.strong_y, nil
    end
    return nil
end

local function insert_sorted(list, cap, rec)
    if #list == cap and list[#list].d <= rec.d then
        return
    end
    local k = #list < cap and #list + 1 or cap
    while k > 1 and list[k - 1].d > rec.d do
        list[k] = list[k - 1]
        k = k - 1
    end
    list[k] = rec
end

-- the anti-air near the position and what holds it, each the nearest first: arrays of {id, d, type, x, y}; and
-- the remembered buildings' ids (for the targets' standing)
function M.targets(px, py, shoot)
    local r = math.max(AS_R, AS_AA_R)
    local cands, seen = {}, {}
    for _, id in ipairs(ai.seen{enemy = true, near = {px, py, r}}) do
        local u = ai.unit(id)
        if u ~= nil then
            cands[#cands + 1] = u
            seen[id] = true
        end
    end
    local remembered = {}
    for _, m in ipairs(ai.memory{buildings = true, near = {px, py, r}}) do
        remembered[m.id] = true
        if not seen[m.id] then
            cands[#cands + 1] = {id = m.id, type = m.type, x = m.x, y = m.y, category = "building", hp = m.hp, armed = true}
        end
    end
    local aa, tg = {}, {}
    local classes = {}
    for _, u in ipairs(cands) do
        local t = u.type
        local d = d2(u.x, u.y, px, py)
        if M.is_aa(t) then
            if d <= AS_AA_R * AS_AA_R then
                insert_sorted(aa, AS_TARGETS, {id = u.id, d = d, type = t, x = u.x, y = u.y})
            end
        elseif d <= AS_R * AS_R then
            local c = classes[t]
            if c == nil then
                c = ai.class(t) or {}
                classes[t] = c
            end
            local lr = u.category == "soldier" and (M.is_gun(t) or (c.range or 0) >= AF_SHOOT_D * 100)
            local take = true
            if shoot == nil or not lr then   -- (at the shooters' spot they come first, by their distance)
                if u.category == "building" then
                    if (c.attack or 0) <= 0 then
                        take = false   -- no ground attack (walls, houses)
                    elseif shoot ~= nil then
                        d = d + AS_R * AS_R
                    end
                elseif u.category == "soldier" and is_siege_or_artillery(t) then
                    d = d + AS_R * AS_R   -- after the defences
                elseif u.category == "soldier" and u.armed then
                    d = d + 2 * AS_R * AS_R   -- the soldiers last: what the force's judgement counts
                else
                    take = false
                end
            end
            if take then
                insert_sorted(tg, AS_TARGETS, {id = u.id, d = d, type = t, x = u.x, y = u.y})
            end
        end
    end
    return aa, tg, remembered
end

-- ---------------------------------------------------------------------------------------------------------------
-- THE DOCTRINE (airstrike.c's dct_*/ah_*): the picture of what shoots at the bombers, the small runs, the way home,
-- the wing by worth, the claim and the lone runs. Keys: bomber_doctrine (on), bomber_survive, bomber_come_home,
-- bomber_value_wing, bomber_claim (on under it) and bomber_harass (off).

-- a setting read with its default when absent (a false must not fall back: `S.x or d` cannot tell false from nil)
local function flag(v, d)
    if v == nil then
        return d
    end
    return v and true or false
end

-- a class by name, once a game (ai.class is a charge a call); an unknown name cached as false too
local cls_cache = {}
local function cls(t)
    local c = cls_cache[t]
    if c == nil then
        c = ai.class(t) or false
        cls_cache[t] = c
    end
    return c or nil
end

local function is_wall(t)
    t = t or ""
    return string.sub(t, 1, 7) == "b_wall_" or string.sub(t, 1, 7) == "b_gate_"
end

local function is_town(t)
    return t == "b_town_center" or t == "b_capitol" or t == "b_settlement"
end

-- whether a type's weapon can hit the bomber type at all (the game's own hit table, ai.can_hurt: a human reads it
-- too); cached by the pair
local hurt_cache = {}
local function hits_air(ref, t)
    if ref == nil or t == nil then
        return false
    end
    local k = ref .. "|" .. t
    local v = hurt_cache[k]
    if v == nil then
        v = ai.can_hurt(t, ref) and true or false
        hurt_cache[k] = v
    end
    return v
end

-- a gun's damage a second at the bomber, in hundredths of a hit point: AD_HIT of a shot's attack a reload. DEVIATION:
-- ai.class exposes no damage-class or hit tables and no armour - the attack itself, as stats.lua's stand-in
local function dps100(k)
    local reload = k ~= nil and (k.reload or 0) or 0
    if reload <= 0 then
        reload = 1000
    end
    return ((k ~= nil and (k.attack or 0) or 0) * AD_HIT * 1000) // reload
end

-- a gun's reach over the bomber, in centi-tiles: its range, AD_PAD, a mover's AD_CHASE_S of its speed
local function reach100(k)
    local r = (k ~= nil and (k.range or 0) or 0) + AD_PAD
    if k ~= nil and k.category ~= "air" and (k.speed or 0) > 0 then
        r = r + (k.speed or 0) * AD_CHASE_S
    end
    return r
end

-- the ground in sight now (the engine keeps no per-tile last-seen time: DEVIATION for upstream's AH_LOOKED)
local function in_sight(x, y)
    local t = ai.tile(x, y)
    return t ~= nil and t.visible == true
end

-- the ground the straight way a - b crosses, a point every AH_WAY_STEP tiles (ah_way_looked)
local function way_in_sight(ax, ay, bx, by)
    local d = math.sqrt(d2(ax, ay, bx, by))
    local t = AH_WAY_STEP
    while t < d do
        if not in_sight(math.floor(ax + (bx - ax) * t / d), math.floor(ay + (by - ay) * t / d)) then
            return false
        end
        t = t + AH_WAY_STEP
    end
    return true
end

-- the squared distance of p to the way a - b, in tiles (ah_seg_d2)
local function seg_d2(ax, ay, bx, by, px, py)
    local dx, dy = bx - ax, by - ay
    local l = dx * dx + dy * dy
    local t = 0
    if l > 0 then
        t = ((px - ax) * dx + (py - ay) * dy) / l
        if t < 0 then
            t = 0
        elseif t > 1 then
            t = 1
        end
    end
    local ex, ey = ax + t * dx - px, ay + t * dy - py
    return ex * ex + ey * ey
end

-- a target's worth (dct_worth): its price, AD_VAL_MAX at most, + 100
local function worth(t)
    local k = cls(t)
    if k == nil then
        return 100
    end
    local v = V.sum(k.cost or {})
    return (v < AD_VAL_MAX and v or AD_VAL_MAX) + 100
end

-- a sortie's damage from the guns and its time in each enemy's fighters' cover (dct_acc)
local function new_acc()
    return {tk = {}, tf = {}}
end

-- the guns and the fighters' buildings that can touch the way a - b (the wing weighs many targets off one corridor: a
-- gun further than its reach + AS_R from it cannot, and the rest are left out of the loops). The lists hold the
-- picture's records themselves (no table a threat: a step weighs many ways)
local function threats_near(pic, ax, ay, bx, by)
    local ths, cov, pos = {}, {}, {}
    for i, t in ipairs(pic.threats) do
        local rt = t.r / 100 + AS_R
        if seg_d2(ax, ay, bx, by, t.x, t.y) <= rt * rt then
            ths[#ths + 1] = t
            pos[i] = #ths
        end
    end
    local rc = AH_FIGHTER_R + AS_R
    for _, c in ipairs(pic.cover) do
        if seg_d2(ax, ay, bx, by, c.x, c.y) <= rc * rc then
            cov[#cov + 1] = c
        end
    end
    return ths, cov, pos
end

-- each gun's ms in reach and each enemy's ms in its fighters' cover on the way a - b (dct_leg); a cover's is the
-- longest of one owner's buildings' disks the way crosses (DEVIATION: not a cell grid's union)
local function leg(pic, ths, cov, ax, ay, bx, by, acc)
    local dx, dy = bx - ax, by - ay
    local l = math.sqrt(dx * dx + dy * dy)
    local function ms_in(x, y, rt)
        local px, py = x - ax, y - ay
        local along = l > 0 and (px * dx + py * dy) / l or 0
        local h2 = px * px + py * py - along * along
        if h2 < 0 then
            h2 = 0
        end
        if h2 >= rt * rt then
            return 0
        end
        local half = math.sqrt(rt * rt - h2)
        local lo, hi = along - half, along + half
        if lo < 0 then
            lo = 0
        end
        if hi > l then
            hi = l
        end
        return hi > lo and (hi - lo) * 100000 / pic.speed or 0
    end
    for i, t in ipairs(ths) do
        local ms = ms_in(t.x, t.y, t.r / 100)
        if ms > 0 then
            acc.tk[i] = (acc.tk[i] or 0) + ms
        end
    end
    for _, c in ipairs(cov) do
        local ms = ms_in(c.x, c.y, AH_FIGHTER_R)
        if ms > (acc.tf[c.pid] or 0) then
            acc.tf[c.pid] = ms
        end
    end
end

-- ... and over p for ms (dct_stay)
local function stay(ths, cov, x, y, ms, acc)
    for i, t in ipairs(ths) do
        local rt = t.r / 100
        if d2(t.x, t.y, x, y) <= rt * rt then
            acc.tk[i] = (acc.tk[i] or 0) + ms
        end
    end
    for _, c in ipairs(cov) do
        if d2(c.x, c.y, x, y) <= AH_FIGHTER_R * AH_FIGHTER_R and ms > (acc.tf[c.pid] or 0) then
            acc.tf[c.pid] = ms
        end
    end
end

-- the guns' and the fighters' damage, in hundredths of a hit point (dct_total: each fighter one fight, the most it
-- does, all of the fire on the one bomber)
local function acc_damage(pic, ths, acc)
    local d = 0
    for i, t in ipairs(ths) do
        local ms = acc.tk[i]
        if ms ~= nil and ms > 0 then
            d = d + t.dps * ms / 1000
        end
    end
    for _, pid in ipairs(pic.pids) do
        local f = pic.f[pid]
        local ms = acc.tf[pid]
        if f ~= nil and ms ~= nil and ms > 0 then
            d = d + f.n * math.min(f.dps * math.min(ms, AD_FIGHT_S * 1000) / 1000, f.sortie)
        end
    end
    return d
end

-- a sortie from -> at -> back, over it over_s seconds (dct_sortie)
local function sortie_damage(pic, fx, fy, tx, ty, bx, by, over_s)
    local ths, cov = threats_near(pic, fx, fy, tx, ty)
    local a1 = new_acc()
    leg(pic, ths, cov, fx, fy, tx, ty, a1)
    local d = acc_damage(pic, ths, a1)
    local ths2, cov2 = threats_near(pic, tx, ty, bx, by)
    local a2 = new_acc()
    stay(ths2, cov2, tx, ty, over_s * 1000, a2)
    leg(pic, ths2, cov2, tx, ty, bx, by, a2)
    return d + acc_damage(pic, ths2, a2)
end

-- ... out and back over the same way (the small runs: from == back), over it over_s seconds: the way's guns fire
-- twice, the guns over it once
local function sortie_same(pic, x, y, tx, ty, over_s)
    local ths, cov = threats_near(pic, x, y, tx, ty)
    local a1 = new_acc()
    leg(pic, ths, cov, x, y, tx, ty, a1)
    local way = acc_damage(pic, ths, a1)
    local a2 = new_acc()
    stay(ths, cov, tx, ty, over_s * 1000, a2)
    return 2 * way + acc_damage(pic, ths, a2)
end

-- the own finished Airports with their tiles
local function own_ports()
    local ports = {}
    for _, id in ipairs(ai.units{type = "b_airport", limit = 32}) do
        local u = ai.unit(id)
        if u ~= nil and u.built and u.x ~= nil then
            ports[#ports + 1] = {id = id, x = u.x, y = u.y}
        end
    end
    return ports
end

-- THE PICTURE (dct_scan): what of the enemy can hit the bombers - the guns seen, AA buildings remembered within
-- AH_MEM, an enemy's strongest gun assumed by each town of theirs not in sight, the strongest known where a bomber was
-- lost; its fighters as a cover AH_FIGHTER_R round its buildings (an enemy with an Airport or carrier gets
-- AD_FIGHTERS) - and what the runs can hit (buildings not walls, citizens at work, the counter-fire's shooters).
-- Every AS_EVERY while bombers are held; memory.air.pic (integers only: hundredths, centi-tiles, prices)
local function picture(view, memory, s, bombers, ids, now)
    -- the bomber type: the most held, else the newest it can train (dct_scan's ref)
    local ref, best = nil, 0
    for _, id in ipairs(ids) do
        local u = bombers[id]
        if u ~= nil then
            local n = 0
            for _, j in ipairs(ids) do
                local v = bombers[j]
                if v ~= nil and v.type == u.type then
                    n = n + 1
                end
            end
            if n > best then
                best, ref = n, u.type
            end
        end
    end
    if ref == nil then
        for _, k in ipairs(AF_TYPES) do
            if k.epoch <= view.epoch and V.cost(view, k.type) ~= nil then
                ref = k.type
                break
            end
        end
    end
    if ref == nil then
        s.pic = nil
        return nil
    end
    local bc = cls(ref)
    local speed = bc ~= nil and (bc.speed or 0) or 0
    if speed < 20 then
        speed = 100
    end
    -- the strongest fighter and gun seen over the game, else the game's defaults (dct_scan's ftype/aatype)
    if s.gun_dps == nil then
        local gc = cls("b_aa10_gun_88mm")
        if gc ~= nil and hits_air(ref, "b_aa10_gun_88mm") then
            s.gun_dps, s.gun_r = dps100(gc), reach100(gc)
        else
            s.gun_dps, s.gun_r = 0, AH_FLAK_R * 100
        end
    end
    if s.fig_dps == nil then
        local fc = cls("a_fighter10_sopwith_camel_f_1")
        s.fig_dps = fc ~= nil and hits_air(ref, "a_fighter10_sopwith_camel_f_1") and dps100(fc) or 0
    end
    local pic = {ref = ref, speed = speed, maxhp = (bc ~= nil and bc.hp or 0) * 100,
                 ammo = AD_NOAMMO, cost = bc ~= nil and V.sum(bc.cost or {}) or 0,
                 threats = {}, seen = {}, mem = {}, rem = {}, is_seen = {}, tg = {}, ports = own_ports(),
                 f = {}, pids = {}, has_aa = {}, airport = {}, towns = {}, cit = {}, cover = {}}
    -- what it sees now: every enemy unit in sight (bound: 128)
    for _, id in ipairs(ai.seen{enemy = true, limit = 128}) do
        local u = ai.unit(id)
        if u ~= nil and u.x ~= nil and u.player ~= nil then
            local t = u.type
            local k = cls(t)
            local cat = u.category or (k ~= nil and k.building and "building" or "soldier")
            local e = {id = id, type = t, x = u.x, y = u.y, hp = u.hp or 0, pid = u.player, cat = cat,
                       w = worth(t), armed = k ~= nil and (k.attack or 0) > 0 or false, aa = false,
                       bld = k ~= nil and k.building or false}
            pic.seen[#pic.seen + 1] = e
            if cat == "building" then
                pic.is_seen[id] = true
            end
            local dp = hits_air(ref, t) and dps100(k) or 0
            if dp > 0 then
                if cat == "air" then
                    local f = pic.f[u.player]
                    if f == nil then
                        f = {n = 0, dps = 0, sortie = 0}
                        pic.f[u.player] = f
                        pic.pids[#pic.pids + 1] = u.player
                    end
                    f.n = f.n + 1
                    if dp > f.dps then
                        f.dps = dp
                        f.sortie = dp * AD_FIGHT_S
                    end
                    if dp > (s.fig_dps or 0) then
                        s.fig_dps = dp
                    end
                else
                    pic.has_aa[u.player] = true
                    e.aa = true
                    if #pic.threats < AD_THREATS then
                        e.th = #pic.threats + 1
                        pic.threats[e.th] = {x = u.x, y = u.y, r = reach100(k), dps = dp, kind = 0, pid = u.player}
                    end
                    if dp * reach100(k) > (s.gun_v or 0) then
                        s.gun_v, s.gun_type, s.gun_dps, s.gun_r = dp * reach100(k), t, dp, reach100(k)
                    end
                end
            end
            if is_town(t) then
                pic.towns[#pic.towns + 1] = {x = u.x, y = u.y, pid = u.player}
            end
            if cat == "citizen" and #pic.cit < AH_CIT then
                pic.cit[#pic.cit + 1] = e
            end
        end
    end
    -- an Airport or a carrier: it has or can have fighters, and they rise at what comes near its buildings
    -- (airguard.c INTERCEPT): AD_FIGHTERS assumed when none was seen (s.fig_dps is final now)
    for _, e in ipairs(pic.seen) do
        local k = cls(e.type)
        if e.type == "b_airport" or (k ~= nil and k.family == FAM_CARRIER) then
            pic.airport[e.pid] = true
            if pic.f[e.pid] == nil then
                pic.f[e.pid] = {n = AD_FIGHTERS, dps = s.fig_dps or 0, sortie = (s.fig_dps or 0) * AD_FIGHT_S}
                pic.pids[#pic.pids + 1] = e.pid
            elseif pic.f[e.pid].n < AD_FIGHTERS then
                pic.f[e.pid].n = AD_FIGHTERS
            end
        end
    end
    -- the enemy's buildings remembered (fogged): the anti-air among them still counts a threat within AH_MEM
    local since = now - AH_MEM
    for _, m in ipairs(ai.memory{buildings = true, limit = AH_BLD}) do
        if m.x ~= nil and m.type ~= nil and pic.is_seen[m.id] == nil and (m.player or 0) ~= 0 then
            local k = cls(m.type)
            pic.rem[m.id] = true
            pic.mem[#pic.mem + 1] = {id = m.id, type = m.type, x = m.x, y = m.y, hp = m.hp or 0, pid = m.player,
                                     cat = "building", w = worth(m.type), armed = k ~= nil and (k.attack or 0) > 0 or false,
                                     aa = false, bld = k ~= nil and k.building or false}
            if is_town(m.type) then
                pic.towns[#pic.towns + 1] = {x = m.x, y = m.y, pid = m.player}
            end
            if k ~= nil and k.building and hits_air(ref, m.type) and (m.seen or 0) >= since
                    and #pic.threats < AD_THREATS then
                pic.has_aa[m.player] = true
                local e = pic.mem[#pic.mem]
                e.aa = true
                e.th = #pic.threats + 1
                pic.threats[e.th] = {x = m.x, y = m.y, r = reach100(k), dps = dps100(k), kind = 1, pid = m.player}
            end
        end
    end
    -- an enemy that has anti-air has it by its towns: assumed there unless in sight (dct_scan's kind 2)
    if (s.gun_dps or 0) > 0 then
        for _, t in ipairs(pic.towns) do
            if pic.has_aa[t.pid] and #pic.threats < AD_THREATS and not in_sight(t.x, t.y) then
                pic.threats[#pic.threats + 1] = {x = t.x, y = t.y, r = AH_FLAK_R * 100, dps = s.gun_dps, kind = 2,
                                                 pid = t.pid}
            end
        end
    end
    -- where a bomber on a run was lost: the strongest gun known there (dct_scan's kind 3)
    for _, f in ipairs(s.flak or {}) do
        if (f.t or 0) > 0 and now - f.t < AH_MEM and (s.gun_dps or 0) > 0 and #pic.threats < AD_THREATS then
            pic.threats[#pic.threats + 1] = {x = f.x, y = f.y, r = AH_FLAK_R * 100, dps = s.gun_dps, kind = 3, pid = 0}
        end
    end
    -- the fighters' cover: the buildings of an enemy with fighters
    for _, e in ipairs(pic.seen) do
        if (e.cat == "building" or (cls(e.type) ~= nil and cls(e.type).family == FAM_CARRIER)) and pic.f[e.pid] ~= nil then
            pic.cover[#pic.cover + 1] = {x = e.x, y = e.y, pid = e.pid}
        end
    end
    for _, e in ipairs(pic.mem) do
        if pic.f[e.pid] ~= nil then
            pic.cover[#pic.cover + 1] = {x = e.x, y = e.y, pid = e.pid}
        end
    end
    -- what the runs can hit: buildings (not walls, not ships), citizens at work (two or more within 4 tiles), and
    -- the counter-fire's shooters at a live spot (AD_CF_W their worth)
    local spots = (memory.army or {}).shooters or {}
    local live = {}
    for _, sp in ipairs(spots) do
        if sp.t > 0 and now - sp.t < AF_SHOOT_MS then
            live[#live + 1] = sp
        end
    end
    local in_tg = {}
    for _, e in ipairs(pic.seen) do
        local k = cls(e.type)
        if (e.cat == "building" or e.cat == "ship") and k ~= nil and k.building then
            if not is_wall(e.type) and e.cat ~= "ship" and #pic.tg < AH_TARGETS then
                pic.tg[#pic.tg + 1] = {id = e.id, type = e.type, x = e.x, y = e.y, hp = e.hp, pid = e.pid, w = e.w,
                                       n = 0, cf = 0}
                in_tg[e.id] = true
            end
        elseif e.cat == "citizen" then
            local n = 0
            for _, o in ipairs(pic.cit) do
                if o.pid == e.pid and d2(o.x, o.y, e.x, e.y) <= 16 then
                    n = n + 1
                end
            end
            if n >= 2 and #pic.tg < AH_TARGETS then
                pic.tg[#pic.tg + 1] = {id = e.id, type = e.type, x = e.x, y = e.y, hp = e.hp, pid = e.pid,
                                       w = 120 * n, n = n, cf = 0}
                in_tg[e.id] = true
            end
        elseif e.cat == "soldier" and e.armed and not in_tg[e.id]
                and (M.is_gun(e.type) or (k ~= nil and (k.range or 0) >= AF_SHOOT_D * 100)) then
            for _, sp in ipairs(live) do
                if d2(sp.x, sp.y, e.x, e.y) <= AF_SHOOT_JOIN * AF_SHOOT_JOIN and #pic.tg < AH_TARGETS then
                    pic.tg[#pic.tg + 1] = {id = e.id, type = e.type, x = e.x, y = e.y, hp = e.hp, pid = e.pid,
                                           w = e.w * AD_CF_W // 100, n = 0, cf = 1}
                    in_tg[e.id] = true
                    break
                end
            end
        end
    end
    for _, e in ipairs(pic.mem) do
        if not is_wall(e.type) and e.bld and #pic.tg < AH_TARGETS then
            pic.tg[#pic.tg + 1] = {id = e.id, type = e.type, x = e.x, y = e.y, hp = e.hp, pid = e.pid, w = e.w,
                                   n = 0, cf = 0}
        end
    end
    s.pic = pic
    return pic
end

-- a position's targets from the picture (as_targets with every building but walls: THE VALUE WING weighs them), each
-- the nearest first: the anti-air, then what holds it; each with its hit points, worth and counter-fire's mark
local function pic_targets(pic, px, py, shoot)
    local aa, tg = {}, {}
    local rr = AS_R * AS_R
    local function add_tg(e, d)
        if #tg >= AS_TARGETS and tg[AS_TARGETS].d <= d then
            return
        end
        insert_sorted(tg, AS_TARGETS, {id = e.id, d = d, type = e.type, x = e.x, y = e.y, hp = e.hp, w = e.w,
                                      cf = e.cf or 0, th = e.th, pid = e.pid})
    end
    local function add(e)
        local d = d2(e.x, e.y, px, py)
        if e.aa then
            if d <= AS_AA_R * AS_AA_R then
                insert_sorted(aa, AS_TARGETS, {id = e.id, d = d, type = e.type, x = e.x, y = e.y, hp = e.hp, w = e.w,
                                              cf = e.cf or 0, th = e.th, pid = e.pid})
            end
        elseif d <= rr then
            local k = cls(e.type)
            local lr = e.cat == "soldier" and (M.is_gun(e.type) or (k ~= nil and (k.range or 0) >= AF_SHOOT_D * 100))
            local take = not (e.cat == "building" and is_wall(e.type))
            if shoot == nil or not lr then   -- (at the shooters' spot they come first, by their distance)
                if e.cat == "building" then
                    if not e.armed then
                        d = d + 3 * rr // 2   -- unarmed buildings after the siege (they are worth bombing all the same)
                    elseif shoot ~= nil then
                        d = d + rr
                    end
                elseif e.cat == "soldier" and is_siege_or_artillery(e.type) then
                    d = d + rr      -- after the defences
                elseif e.cat == "soldier" and e.armed then
                    d = d + 2 * rr  -- the soldiers last: what the force's judgement counts
                else
                    take = false
                end
            end
            if take then
                add_tg(e, d)
            end
        end
    end
    for _, e in ipairs(pic.seen) do
        add(e)
    end
    for _, e in ipairs(pic.mem) do
        if e.bld then
            add(e)
        end
    end
    return aa, tg
end

-- THE DOCTRINE's books: a target of its bombers gone while they were on it - counted once (anti-air, buildings,
-- units and their worth)
local function down(s, id, ttype)
    s.down_ids = s.down_ids or {}
    for _, k in ipairs(s.down_ids) do
        if k == id then
            return
        end
    end
    if #s.down_ids >= 64 then
        table.remove(s.down_ids, 1)
    end
    s.down_ids[#s.down_ids + 1] = id
    local k = cls(ttype)
    if k == nil then
        return
    end
    if k.family == 32 or k.family == 46 then   -- land_aa, aa_building
        s.down_aa = (s.down_aa or 0) + 1
    elseif k.building then
        s.down_bld = (s.down_bld or 0) + 1
    else
        s.down_unit = (s.down_unit or 0) + 1
    end
    s.down_val = (s.down_val or 0) + V.sum(k.cost or {})
end

-- what one bomber's load does to a target (dct_bombs): its shot at the target's type x its shots a sortie
local bombs_cache = {}
local function bombs_at(pic, t)
    local key = pic.ref .. "|" .. tostring(t.type)
    local v = bombs_cache[key]
    if v == nil then
        local bc = cls(pic.ref)
        local att = bc ~= nil and (bc.attack or 0) or 0
        if att < 1 then
            att = 1
        end
        v = ai.can_hurt(pic.ref, t.type) and att * pic.ammo or 0
        bombs_cache[key] = v
    end
    return v
end

-- the bombers free for the next task: ready (no goal and AF_READY_PCT of their hit points), idle (any hit points),
-- and those still out on a run (har_out, upstream's ah_free: free once back - not ordered for AH_RUN_MS)
local function free_lists(s, bombers, ids, now)
    local ready, idle, har_out = {}, {}, 0
    for _, id in ipairs(ids) do
        local e = s.held[held_at(s, id) or 0]
        local u = bombers[id]
        if e ~= nil and u ~= nil then
            local busy = u.action ~= "idle" and u.action ~= "inside"
            if (e.har or 0) > 0 then
                if busy and now - (e.t0 or 0) < AH_RUN_MS then
                    har_out = har_out + 1
                else
                    -- back on the ground (or out of time): free again; its target down counted
                    if e.tgt ~= 0 then
                        local t = ai.unit(e.tgt)
                        if t == nil or (t.hp or 0) <= 0 then
                            down(s, e.tgt, e.ttype)
                            s.har_down = (s.har_down or 0) + 1
                            if (e.cf or 0) > 0 then
                                s.cf_down = (s.cf_down or 0) + 1
                            end
                        end
                    end
                    e.har, e.tgt, e.cf = 0, 0, 0
                end
            end
            if e.tgt == 0 and not busy then
                idle[#idle + 1] = e
                if (u.hp or 0) * 100 >= (u.hp_max or u.hp or 0) * AF_READY_PCT then
                    ready[#ready + 1] = e
                end
            end
        end
    end
    return ready, idle, har_out
end

-- the order to a flight of bombers (upstream's as_order, the claim under bomber_claim): those in the air ai.attack;
-- a bomber parked in its Airport launched by its Airport's rally at the target (the engine has no ai.air: the stand-in
-- airguard.lua uses, which launches the Airport's whole queue; a target only remembered - a fogged building - is no
-- id the rally takes, so the rally is at its tile then). One call a bomber, so the records mark only what went
local function send(view, memory, s, flight, tid, now, claim, dur, tx, ty)
    local sent, reason = 0, nil
    for _, e in ipairs(flight) do
        local u = ai.unit(e.id)
        if u ~= nil then
            local n, why = 0, nil
            if u.inside ~= nil then
                local base = u.base ~= nil and u.base or ((e.home or 0) ~= 0 and e.home or nil)
                if base ~= nil then
                    local _, w, got = ai.rally(base, tid)
                    if (got or 0) == 0 and tx ~= nil then
                        _, w, got = ai.rally(base, tx, ty)
                    end
                    n, why = got or 0, w
                else
                    why = "cannot_do"
                end
            else
                local _, w, got = ai.attack({e.id}, tid)
                n, why = got or 0, w
            end
            if n > 0 then
                sent = sent + 1
                e.tgt, e.t, e.t0 = tid, (now > 0 and now or 1), (now > 0 and now or 1)
                if claim then
                    C.take(view, memory, e.id, C.PRIO.task, C.WHO.bombers, dur or AF_RUN_MS, u)
                end
            else
                reason = why or reason
            end
        end
    end
    return sent, reason
end

-- THE DOCTRINE's books at the look: a sortie from the look it is out of its Airport to the look it is back in one
-- (the damage it took and was expected to, back with damage, repaired in its Airport, its place kept), and the claim
-- on every bomber on a task of the air force's (bomber_claim)
local function books(view, memory, s, bombers, ids, now, claim)
    for _, id in ipairs(ids) do
        local e = s.held[held_at(s, id) or 0]
        local u = bombers[id]
        if e ~= nil and u ~= nil then
            local hp = u.hp or 0
            local inside = u.inside ~= nil
            if not inside and (e.out or 0) == 0 then
                e.out = 1
                e.hp0 = (e.hp_last or 0) > 0 and e.hp_last or hp
                s.sorties = (s.sorties or 0) + 1
            elseif inside and (e.out or 0) == 1 then
                e.out = 0
                e.nohome = 0
                if hp < (e.hp0 or 0) then
                    s.dmg_taken = (s.dmg_taken or 0) + ((e.hp0 or 0) - hp)
                    if hp * 20 < (u.hp_max or hp) * 19 then
                        s.dmg_back = (s.dmg_back or 0) + 1
                    end
                end
                s.dmg_pred = (s.dmg_pred or 0) + (e.pred or 0)
                e.pred = 0
            elseif inside and (e.hp_last or 0) > 0 and hp > e.hp_last then
                s.repaired = (s.repaired or 0) + (hp - e.hp_last)
            end
            if inside or u.action == "idle" then
                e.home2 = 0
            end
            e.landed = inside and 1 or 0
            e.hp_last = hp
            if u.x ~= nil then
                e.at_x, e.at_y = u.x, u.y
            end
            if claim then
                if e.tgt ~= 0 or (e.har or 0) > 0 or (e.home2 or 0) == 1 or ((e.out or 0) == 1 and not inside) then
                    C.take(view, memory, e.id, C.PRIO.task, C.WHO.bombers, 3 * AS_EVERY, u)
                else
                    C.end_(memory, e.id, C.WHO.bombers)
                end
            end
        end
    end
end

-- the wing out: a bomber whose target stands keeps at it (the order again after AS_AGAIN); idle again, its next run.
-- A target not down AF_RUN_MS after the launch is given up. Returns how many are out
local function wing_look(view, memory, s, bombers, ids, rem, now, claim)
    local out = 0
    for _, id in ipairs(ids) do
        local e = s.held[held_at(s, id) or 0]
        local u = bombers[id]
        if e ~= nil and u ~= nil and (e.har or 0) == 0 and e.tgt ~= 0 then
            local t = ai.unit(e.tgt)
            local stands = (t ~= nil and (t.hp or 1) > 0 and t.x ~= nil) or rem[e.tgt] == true
            if stands and now - (e.t0 or 0) >= AF_RUN_MS then
                s.gave_up = (s.gave_up or 0) + 1
                stands = false
            end
            if stands then
                out = out + 1
                local busy = u.action ~= "idle" and u.action ~= "inside"
                if not busy and now - (e.t or 0) >= AS_AGAIN then
                    local n, reason = send(view, memory, s, {e}, e.tgt, now, false, AF_RUN_MS)
                    if n == 0 then
                        s.refused = (s.refused or 0) + 1
                        s.refused_reason = reason
                    end
                end
            else
                down(s, e.tgt, e.ttype)
                e.tgt = 0
            end
        end
    end
    return out
end

-- THE WAY HOME (dct_home, every AD_HOME_EVERY): a bomber in the air whose hit points no longer take it through what
-- is left of its sortie - a small run's: to its target, over it and home; the wing's: to its target and half the time
-- over it, the fire shared by the wing's bombers - with AD_MARGIN_HOME and AD_RESERVE is called back to its Airport
-- while they still take it home (the way home's damage as expected, no margin); one that can't make it home either
-- way goes on with its bombs. DEVIATION: no rebase in the API (EEMReturnToBase): a forced move to its Airport, and
-- the engine lands the plane at its own base
local function home_tick(view, memory, s, bombers, now)
    local pic = s.pic
    if pic == nil then
        return
    end
    -- the wing's bombers in the air: they share the guns' fire
    local wing = 0
    for _, e in ipairs(s.held) do
        if e.tgt ~= 0 and (e.har or 0) == 0 then
            wing = wing + 1
        end
    end
    for _, e in ipairs(s.held) do
        local u = bombers[e.id]
        if u == nil or u.x == nil then
            e.hp_sec = 0
        elseif u.inside ~= nil then
            e.hp_sec = u.hp or 0
            e.home2 = 0
        else
            local share = (e.tgt ~= 0 and (e.har or 0) == 0 and wing > 1) and wing or 1
            -- the guns' fire on it here, against the hit points it lost since the last look (the model's check)
            local here = 0
            for _, t in ipairs(pic.threats) do
                local rt = t.r / 100
                if d2(t.x, t.y, u.x, u.y) <= rt * rt then
                    here = here + t.dps
                end
            end
            if (e.hp_sec or 0) > 0 and e.hp_sec >= (u.hp or 0) then
                s.obs_taken = (s.obs_taken or 0) + (e.hp_sec - (u.hp or 0))
                s.obs_pred = (s.obs_pred or 0) + (e.here_prev or 0)
            end
            e.hp_sec = u.hp or 0
            e.here_prev = math.floor(here * AD_HOME_EVERY / (1000 * share))
            if u.action == "idle" then
                e.home2 = 0
            elseif (e.home2 or 0) == 0 and (e.nohome or 0) == 0 then
                -- its Airport: the one it is based at, else the nearest
                local home = nil
                if u.base ~= nil then
                    for _, p in ipairs(pic.ports) do
                        if p.id == u.base then
                            home = p
                        end
                    end
                    if home == nil then
                        local b = ai.unit(u.base)
                        if b ~= nil and b.x ~= nil then
                            home = {id = u.base, x = b.x, y = b.y}
                        end
                    end
                end
                if home == nil and #pic.ports > 0 then
                    local bd = nil
                    for _, p in ipairs(pic.ports) do
                        local d = d2(p.x, p.y, u.x, u.y)
                        if bd == nil or d < bd then
                            home, bd = p, d
                        end
                    end
                end
                if home ~= nil then
                    local ths, cov = threats_near(pic, u.x, u.y, home.x, home.y)
                    local acc = new_acc()
                    leg(pic, ths, cov, u.x, u.y, home.x, home.y, acc)
                    local way = acc_damage(pic, ths, acc)
                    local need = way * AD_MARGIN_HOME / 100 + AD_RESERVE * pic.maxhp / 100
                    local t = e.tgt ~= 0 and ai.unit(e.tgt) or nil
                    if t ~= nil and t.x ~= nil then
                        if (e.har or 0) == 0 then   -- the wing's: its bombs first, home only when they can't get there
                            local ths2, cov2 = threats_near(pic, u.x, u.y, t.x, t.y)
                            local a2 = new_acc()
                            leg(pic, ths2, cov2, u.x, u.y, t.x, t.y, a2)
                            stay(ths2, cov2, t.x, t.y, AD_OVER_S * 500, a2)
                            need = acc_damage(pic, ths2, a2) / share * AD_MARGIN_HOME / 100
                        else
                            need = sortie_damage(pic, u.x, u.y, t.x, t.y, home.x, home.y, AD_OVER_S) / share
                                * AD_MARGIN_HOME / 100 + AD_RESERVE * pic.maxhp / 100
                        end
                    end
                    if (u.hp or 0) * 100 < need then
                        if (u.hp or 0) * 100 < way then
                            e.nohome = 1   -- it can't make it home either way: its bombs first
                            s.doomed_on = (s.doomed_on or 0) + 1
                        else
                            ai.move({e.id}, home.x, home.y, {forced = true})
                            e.home2 = 1
                            e.tgt, e.har = 0, 0
                            s.called_home = (s.called_home or 0) + 1
                            print(string.format("air bomber home at=%d,%d hp=%d need=%d", u.x, u.y, u.hp or 0,
                                math.floor(need + 0.5)))
                        end
                    end
                end
            end
        end
    end
end

-- the free bombers by the Airport each is based at, the soundest first: groups[base] = {records}, order = bases
local function by_port(bombers, free)
    local groups, order = {}, {}
    for _, e in ipairs(free) do
        local u = bombers[e.id]
        local base = (u ~= nil and u.base ~= nil) and u.base or 0
        local g = groups[base]
        if g == nil then
            g = {}
            groups[base] = g
            order[#order + 1] = base
        end
        g[#g + 1] = e
    end
    for _, base in ipairs(order) do
        local g = groups[base]
        table.sort(g, function(a, b)
            local ua, ub = bombers[a.id], bombers[b.id]
            return (ua ~= nil and ua.hp or 0) > (ub ~= nil and ub.hp or 0)
        end)
    end
    return groups, order
end

-- the tiles a group starts from: its Airport's, else its first bomber's
local function port_at(pic, base, bombers, g)
    for _, p in ipairs(pic.ports) do
        if p.id == base then
            return p.x, p.y
        end
    end
    local u = bombers[g[1].id]
    if u ~= nil and u.x ~= nil then
        return u.x, u.y
    end
    if #pic.ports > 0 then
        return pic.ports[1].x, pic.ports[1].y
    end
    return nil, nil
end

-- THE SMALL RUNS (bomber_survive, dct_runs): the bombers free go in flights of up to AD_FLIGHT from one Airport (as
-- many as the target's hit points take, the soundest first) at the target known worth most for the way that they
-- come back from alive - the damage expected there, over it and home, x AD_MARGIN and AD_RESERVE of a bomber's hit
-- points left, below the hit points of the weakest of the flight (all of the fire on the one); a target of an enemy
-- with anti-air not in sight has one of its guns assumed there. One flight a target at a time
local function small_runs(view, memory, s, bombers, pic, free, now, claim)
    if pic == nil or #free == 0 or #pic.tg == 0 then
        return
    end
    local taken = {}
    for _, e in ipairs(s.held) do
        if (e.har or 0) > 0 and e.tgt ~= 0 then
            taken[e.tgt] = true
        end
    end
    local groups, order = by_port(bombers, free)
    local work = 0
    for _, base in ipairs(order) do
        local g = groups[base]
        local from_x, from_y = port_at(pic, base, bombers, g)
        if from_x ~= nil then
            -- the target worth most for the way first; the damage of the ones it tries (the cost guard) decides
            local best, bn, bdmg = nil, 0, 0
            local tried = {}
            while work <= AD_WORK do
                local cand, bv, cn = nil, -1, 0
                for _, t in ipairs(pic.tg) do
                    if not taken[t.id] and not tried[t.id] then
                        local bombs = bombs_at(pic, t)
                        local hp = t.hp or 0
                        if bombs > 0 and hp > 0 then
                            local need = (hp + bombs - 1) // bombs
                            if need < 1 then
                                need = 1
                            elseif need > AD_FLIGHT then
                                need = AD_FLIGHT
                            end
                            local n = math.min(need, #g)
                            local dist = isqrt(d2(from_x, from_y, t.x, t.y))
                            local v = t.w * (n * bombs < hp and n * bombs / hp or 1) / (30 + dist)
                            if v > bv then
                                cand, bv, cn = t, v, n
                            end
                        end
                    end
                end
                if cand == nil then
                    break
                end
                tried[cand.id] = true
                work = work + 2 * (#pic.threats + 1)
                local dmg = sortie_same(pic, from_x, from_y, cand.x, cand.y, AD_OVER_S)
                if pic.has_aa[cand.pid] and not in_sight(cand.x, cand.y) then
                    -- one of its guns assumed there: the way in and out of its reach, over the target
                    local gun_r = s.gun_r or AH_FLAK_R * 100
                    dmg = dmg + (s.gun_dps or 0) * (2 * gun_r / pic.speed + AD_OVER_S)
                end
                local weakest = nil
                for i = 1, cn do
                    local u = bombers[g[i].id]
                    local h = u ~= nil and (u.hp or 0) or 0
                    if weakest == nil or h < weakest then
                        weakest = h
                    end
                end
                if dmg * AD_MARGIN / 100 + AD_RESERVE * pic.maxhp / 100 < (weakest or 0) * 100 then
                    best, bn, bdmg = cand, cn, dmg
                    break
                end
            end
            if best ~= nil then
                local flight = {}
                for i = 1, bn do
                    flight[#flight + 1] = g[i]
                end
                local sent, reason = send(view, memory, s, flight, best.id, now, claim, AH_RUN_MS, best.x, best.y)
                if sent > 0 then
                    for _, e in ipairs(flight) do
                        if e.tgt == best.id then
                            e.har, e.tat_x, e.tat_y, e.from_x, e.from_y = 1, best.x, best.y, from_x, from_y
                            e.home = base
                            e.ttype = best.type
                            e.pred = math.floor(bdmg + 0.5)
                            e.cf = best.cf or 0
                        end
                    end
                    taken[best.id] = true
                    s.har_runs = (s.har_runs or 0) + 1
                    s.har_sent = (s.har_sent or 0) + sent
                    if (best.cf or 0) > 0 then
                        s.cf_runs = (s.cf_runs or 0) + 1
                        s.cf_sent = (s.cf_sent or 0) + sent
                    end
                    print(string.format("air run n=%d%s at=%d,%d type=%s from=%d,%d dmg=%d hp=%d", sent,
                        (best.cf or 0) > 0 and " (counter-fire)" or "", best.x, best.y, tostring(best.type), from_x,
                        from_y, math.floor(bdmg + 0.5), bombers[flight[1].id] ~= nil and
                        (bombers[flight[1].id].hp or 0) or 0))
                else
                    s.refused = (s.refused or 0) + #flight
                    s.refused_reason = reason
                end
            end
        end
    end
end

-- THE LONE RUNS (bomber_harass, off upstream: ah_runs): what an enemy has that is undefended as far as it knows -
-- no fighter or Airport of that enemy, the ground round the target in sight (and, of an enemy with anti-air, the way
-- there too), no gun's reach within AH_MARGIN (AH_AA_MIN at least) of the target or the straight way - the most worth
-- (its price, at most 800; 120 a citizen) for the way. A flight of AH_FLIGHT from one Airport, one a target at a time
local function harass_runs(view, memory, s, bombers, pic, free, now, claim)
    if pic == nil or #free == 0 or #pic.tg == 0 then
        return
    end
    local taken = {}
    for _, e in ipairs(s.held) do
        if (e.har or 0) > 0 and e.tgt ~= 0 then
            taken[e.tgt] = true
        end
    end
    local groups, order = by_port(bombers, free)
    for _, base in ipairs(order) do
        local g = groups[base]
        local from_x, from_y = port_at(pic, base, bombers, g)
        if from_x ~= nil then
            local best, bv = nil, -1
            for _, t in ipairs(pic.tg) do
                if not taken[t.id] and pic.f[t.pid] == nil and pic.airport[t.pid] == nil and (t.cf or 0) == 0 then
                    local k = cls(t.type)
                    local w = t.w   -- a citizen's: 120 x its neighbours
                    if (t.n or 0) == 0 and k ~= nil then
                        local c = V.sum(k.cost or {})
                        w = (c < 800 and c or 800) + 100   -- a building's: its price, 800 at most, + 100
                    end
                    local open = in_sight(t.x, t.y)
                        and (pic.has_aa[t.pid] ~= true or way_in_sight(from_x, from_y, t.x, t.y))
                    -- no gun's reach within AH_MARGIN (AH_AA_MIN at least) of the target or the straight way, nor
                    -- where a bomber on a run was lost (the flak spots are threats too), nor in a fighter's cover
                    if open then
                        for _, th in ipairs(pic.threats) do
                            local r = th.r / 100 + AH_MARGIN
                            if r < AH_AA_MIN then
                                r = AH_AA_MIN
                            end
                            if seg_d2(from_x, from_y, t.x, t.y, th.x, th.y) <= r * r
                                    or d2(th.x, th.y, t.x, t.y) <= r * r then
                                open = false
                                break
                            end
                        end
                    end
                    if open then
                        for _, c in ipairs(pic.cover) do
                            if seg_d2(from_x, from_y, t.x, t.y, c.x, c.y) <= AH_FIGHTER_R * AH_FIGHTER_R then
                                open = false
                                break
                            end
                        end
                    end
                    if open then
                        local dist = isqrt(d2(from_x, from_y, t.x, t.y))
                        local v = w * 100 // (30 + dist)
                        if v > bv then
                            best, bv = t, v
                        end
                    end
                end
            end
            if best ~= nil then
                local flight = {}
                for i = 1, math.min(AH_FLIGHT, #g) do
                    flight[#flight + 1] = g[i]
                end
                local sent, reason = send(view, memory, s, flight, best.id, now, claim, AH_RUN_MS, best.x, best.y)
                if sent > 0 then
                    for _, e in ipairs(flight) do
                        if e.tgt == best.id then
                            e.har, e.tat_x, e.tat_y, e.from_x, e.from_y = 1, best.x, best.y, from_x, from_y
                            e.home = base
                            e.ttype = best.type
                            e.cf = best.cf or 0
                        end
                    end
                    taken[best.id] = true
                    s.har_runs = (s.har_runs or 0) + 1
                    s.har_sent = (s.har_sent or 0) + sent
                    print(string.format("air lone run n=%d at=%d,%d type=%s from=%d,%d worth=%d", sent, best.x, best.y,
                        tostring(best.type), from_x, from_y, bv))
                else
                    s.refused = (s.refused or 0) + #flight
                    s.refused_reason = reason
                    s.har_none = (s.har_none or 0) + 1
                end
            else
                s.har_none = (s.har_none or 0) + 1
            end
        end
    end
end

-- the free bombers' runs: the small runs under bomber_survive (the doctrine), else the lone runs under bomber_harass
-- (upstream's as_runs: AS_RUNS = AS_HARASS || dct_part(DCP_SURVIVE))
local function runs_any(view, memory, s, bombers, pic, free, now, claim, survive)
    if survive then
        small_runs(view, memory, s, bombers, pic, free, now, claim)
    elseif flag(S.bomber_harass, false) then
        harass_runs(view, memory, s, bombers, pic, free, now, claim)
    end
end

-- the wing's targets down counted in wing_look (the target of each bomber still standing kept at it)

-- THE VALUE WING (dct_wing): the positions - the force's (AD_PUSH_W more) and the enemy's buildings by AD_CELL cell,
-- the AD_CANDS worth most - weighed for each number of the bombers ready: the worth they destroy less the bombers
-- expected lost. Launched where the gain is highest; the rest fly the small runs. s.needed: the fewest that would
-- gain at the force's position (the training)
local function value_wing(view, memory, s, bombers, ids, pic, px, py, shoot, now, claim)
    local army = memory.army or {}
    local out = wing_look(view, memory, s, bombers, ids, pic.rem, now, claim)
    local ready, idle = free_lists(s, bombers, ids, now)
    if out > 0 then   -- one wing at a time (the small runs meanwhile)
        runs_any(view, memory, s, bombers, pic, idle, now, claim, flag(S.bomber_survive, true))
        return
    end
    if s.wing_out then
        s.wing_out = false
        if (s.wing_push or 0) == 1 then   -- its targets down: the force judges its push again now
            army.next_attack = now
            print(string.format("air wing_down at=%d,%d (the force judges its push again)", s.wing_pos_x or -1,
                s.wing_pos_y or -1))
        end
    end
    -- the soundest first
    table.sort(ready, function(a, b)
        local ua, ub = bombers[a.id], bombers[b.id]
        return (ua ~= nil and ua.hp or 0) > (ub ~= nil and ub.hp or 0)
    end)
    -- where they fly from: the mean of the ready bombers' Airports
    local from_x, from_y, nf = 0, 0, 0
    for _, e in ipairs(ready) do
        local u = bombers[e.id]
        if u ~= nil and u.x ~= nil then
            local bx, by = u.x, u.y
            if u.base ~= nil then
                for _, p in ipairs(pic.ports) do
                    if p.id == u.base then
                        bx, by = p.x, p.y
                    end
                end
            end
            from_x, from_y, nf = from_x + bx, from_y + by, nf + 1
        end
    end
    if nf > 0 then
        from_x, from_y = from_x // nf, from_y // nf
    elseif #pic.ports > 0 then
        from_x, from_y = pic.ports[1].x, pic.ports[1].y
    else
        return
    end
    -- the candidates: the force's position and the enemy's buildings, the AD_CANDS cells worth most
    local cands = {}
    if px ~= nil then
        cands[1] = {x = px, y = py, w = AD_PUSH_W, push = true, shoot = shoot}
    end
    local cells, order = {}, {}
    for _, t in ipairs(pic.tg) do
        local k = cls(t.type)
        if k ~= nil and k.building and not is_wall(t.type) then
            local key = (t.x // AD_CELL) * 1000 + (t.y // AD_CELL)
            local c = cells[key]
            if c == nil then
                c = {bx = 0, by = 0, v = 0, bv = -1}
                cells[key] = c
                order[#order + 1] = c
            end
            c.v = c.v + (t.w or 0)
            if (t.w or 0) > c.bv then
                c.bv, c.bx, c.by = t.w, t.x, t.y
            end
        end
    end
    for _ = 1, AD_CANDS do
        local best = nil
        for _, c in ipairs(order) do
            if not c.taken and (best == nil or c.v > best.v) then
                best = c
            end
        end
        if best == nil then
            break
        end
        best.taken = true
        if px == nil or d2(best.bx, best.by, px, py) > AS_R * AS_R then
            cands[#cands + 1] = {x = best.bx, y = best.by, w = 100, push = false}
        end
    end
    -- the weigh: what n bombers (hp100 each) do at a candidate - the worth they destroy (the AA first, the rest as
    -- their hit points take, AD_PARTIAL of a target they can't destroy) less the bombers expected lost (each gun
    -- firing as long as any of them is in its reach, so numbers share its fire; x AD_MARGIN, a bomber a hit point
    -- total); nil where more than AD_LOSS_MAX of them would be lost
    local function weigh(c, list, ths, n, hp_avg)
        local left, val = n, 0
        local alloc, gone = {}, {}
        for j = 1, #list do
            alloc[j] = 0
        end
        for j = 1, #list do
            local e = list[j]
            if left > 0 and e.bombs > 0 then
                local need = (e.hp + e.bombs - 1) // e.bombs
                if need < 1 then
                    need = 1
                end
                if need > left then
                    alloc[j] = left
                    val = val + e.w * (left * e.bombs / e.hp) * AD_PARTIAL / 100
                    left = 0
                else
                    alloc[j] = need
                    left = left - need
                    val = val + e.w
                    if e.th ~= nil then
                        gone[e.th] = true   -- an AA gun knocked out fires no more on the way back
                    end
                end
            end
        end
        if val <= 0 then
            return -1e9, 0, 0, alloc
        end
        local tk, tf = {}, {}
        for j = 1, #list do
            if alloc[j] > 0 then
                local e = list[j]
                for i = 1, #ths do
                    local t1 = e.leg[i] or 0
                    local v = (gone[i] and t1 or 2 * t1) + (e.ov[i] or 0)
                    if v > (tk[i] or 0) then
                        tk[i] = v
                    end
                end
                for _, pid in ipairs(pic.pids) do
                    local v = 2 * (e.fleg[pid] or 0) + (e.fov[pid] or 0)
                    if v > (tf[pid] or 0) then
                        tf[pid] = v
                    end
                end
            end
        end
        local D = acc_damage(pic, ths, {tk = tk, tf = tf})
        local L = D * AD_MARGIN / 100 / (hp_avg > 0 and hp_avg or 1)
        if L > n then
            L = n
        end
        if L > AD_LOSS_MAX / 100 * n then
            return -1e9, D, L, alloc
        end
        return val * c.w / 100 * (1 - 0.5 * L / n) - L * pic.cost, D, L, alloc
    end
    local bc, bn, bloss, bdmg, bev, balloc, blist = nil, 0, 0, 0, -1e9, nil, nil
    local best_ev, nmin = -1e9, 0
    local work = 0   -- the chords this look has spent (THE DOCTRINE's cost guard: the positions most worth first)
    for _, c in ipairs(cands) do
        local aa, tg = pic_targets(pic, c.x, c.y, c.shoot)
        local list = {}
        if #aa + #tg == 0 then
            if c.push and c.shoot ~= nil and army.shooters ~= nil and army.shooters[c.shoot] ~= nil then
                army.shooters[c.shoot].t = 0   -- nothing left at the shooters' spot: the next
            end
        elseif work <= AD_WORK then
            -- the guns that can touch the corridor into this position (the targets are within AS_R of it)
            local ths, cov, thpos = threats_near(pic, from_x, from_y, c.x, c.y)
            for j = 1, #aa do
                list[#list + 1] = {t = aa[j], hp = (aa[j].hp or 0) > 0 and aa[j].hp or 1, w = (aa[j].w or 0) + pic.cost,
                                   aa = j, th = aa[j].th ~= nil and thpos[aa[j].th] or nil}
            end
            for j = 1, #tg do
                local e = tg[j]
                if #list < AD_WING_TARGETS then
                    list[#list + 1] = {t = e, hp = (e.hp or 0) > 0 and e.hp or 1,
                                       w = (e.cf or 0) > 0 and e.w * AD_CF_W // 100 or (e.w or 0), aa = 0, th = nil}
                end
            end
            for _, e in ipairs(list) do
                e.bombs = bombs_at(pic, e.t)
                -- the way in and the way out cross the same guns: their times are the same (the way's chord, tin
                -- doubled in the weigh - once less for a gun knocked out), and the time over the target is its own
                e.leg, e.fleg = {}, {}
                leg(pic, ths, cov, from_x, from_y, e.t.x, e.t.y, {tk = e.leg, tf = e.fleg})
                e.ov, e.fov = {}, {}
                stay(ths, cov, e.t.x, e.t.y, AD_OVER_S * 1000, {tk = e.ov, tf = e.fov})
            end
            work = work + (#ths + 1) * (#list + 1)
            c.list = list
            if c.push then   -- the training: the fewest that would gain here
                for n = 1, AF_WING_MAX do
                    if nmin == 0 and weigh(c, list, ths, n, pic.maxhp) > 0 then
                        nmin = n
                    end
                end
            end
            local hsum = 0
            for n = 1, #ready do
                local u = bombers[ready[n].id]
                hsum = hsum + (u ~= nil and (u.hp or 0) or 0) * 100
                if n <= 6 or n == #ready or (n <= 16 and n % 2 == 0) or n % 4 == 0 then
                    local ev, D, L, alloc = weigh(c, list, ths, n, hsum / n)
                    if ev > best_ev then
                        best_ev = ev
                    end
                    if ev > bev then
                        bev, bc, bn, bloss, bdmg, balloc, blist = ev, c, n, L, D, alloc, list
                    end
                end
            end
        end
    end
    s.best_ev = math.floor(best_ev + 0.5)
    if cands[1] ~= nil and cands[1].push then
        s.needed = nmin > 0 and nmin or AF_WING
        s.need_t = now > 0 and now or 1
    end
    if bc == nil or bev <= 0 then
        s.waits = (s.waits or 0) + 1
        if (s.ev_log or 0) == 0 or now - s.ev_log >= 60000 then
            s.ev_log = now > 0 and now or 1
            print(string.format("air no wing: ready=%d best_ev=%d positions=%d push=%s in_all=%d threats=%d", #ready,
                math.floor(best_ev + 0.5), #cands, tostring(cands[1] ~= nil and cands[1].push or false), #ids,
                #pic.threats))
        end
        runs_any(view, memory, s, bombers, pic, idle, now, claim, flag(S.bomber_survive, true))
        return
    end
    -- the launch: each target its share, the rest spread over them, the anti-air first
    local bn2 = math.min(bn, #ready)
    local given, ntgt = 0, 0
    for j = 1, #blist do
        given = given + (balloc[j] or 0)
        if (balloc[j] or 0) > 0 then
            ntgt = ntgt + 1
        end
    end
    local j = 1
    while given < bn2 and ntgt > 0 do
        if (balloc[j] or 0) > 0 then
            balloc[j] = balloc[j] + 1
            given = given + 1
        end
        j = j % #blist + 1
    end
    local sent, naa, i = 0, 0, 1
    for j2 = 1, #blist do
        local e = blist[j2]
        if (balloc[j2] or 0) > 0 and e.aa > 0 then
            naa = naa + 1
        end
        local flight = {}
        for _ = 1, (balloc[j2] or 0) do
            if ready[i] ~= nil then
                flight[#flight + 1] = ready[i]
                i = i + 1
            end
        end
        if #flight > 0 then
            local n, reason = send(view, memory, s, flight, e.t.id, now, claim, AF_RUN_MS, e.t.x, e.t.y)
            if n > 0 then
                sent = sent + n
                for _, r in ipairs(flight) do
                    if r.tgt == e.t.id then
                        r.ttype = e.t.type
                        r.pred = math.floor(bdmg / bn2 + 0.5)
                    end
                end
            end
            if n < #flight then
                s.refused = (s.refused or 0) + #flight - n
                s.refused_reason = reason
            end
        end
    end
    if sent == 0 then
        return
    end
    s.waves = (s.waves or 0) + 1
    s.sent = (s.sent or 0) + sent
    s.wing_out = true
    s.ev_launch = (s.ev_launch or 0) + 1
    s.wing_pos_x, s.wing_pos_y = bc.x, bc.y
    s.wing_push = bc.push and 1 or 0
    print(string.format("air wing n=%d at=%d,%d by worth=%d lost=%d dmg=%d targets=%d aa=%d bombers=%d", sent, bc.x, bc.y,
        math.floor(bev + 0.5), math.floor(bloss + 0.5), math.floor(bdmg + 0.5), ntgt, naa, #ids))
    -- the rest to the small runs
    local left = {}
    for _, e in ipairs(idle) do
        if e.tgt == 0 and (e.har or 0) == 0 then
            left[#left + 1] = e
        end
    end
    runs_any(view, memory, s, bombers, pic, left, now, claim, flag(S.bomber_survive, true))
end

-- the run at a position: the wing out kept at it, the ready launched when enough. bombers, ids from M.held. doc
-- (THE DOCTRINE, optional): {view, pic, claim, survive} - the free bombers fly the runs while the wing waits, and a
-- bomber sent is claimed
function M.run(memory, now, bombers, ids, px, py, shoot, aa, tg, remembered, doc)
    local s = memory.air
    local army = memory.army or {}
    local na, nt = #aa, #tg
    if na == 0 and nt == 0 then
        if shoot ~= nil then
            army.shooters[shoot].t = 0   -- nothing left at the shooters' spot: the next
        end
        if doc ~= nil then
            local _, idle = free_lists(s, bombers, ids, now)
            runs_any(doc.view, memory, s, bombers, doc.pic, idle, now, doc.claim, doc.survive)
        end
        return
    end
    s.needed = AF_FLIGHT * (na + (nt > 0 and 1 or 0))
    s.need_t = now > 0 and now or 1
    -- the wing out: a bomber whose target stands keeps at it; idle again, its next run
    local ready, idle, out = {}, {}, 0
    for _, id in ipairs(ids) do
        local e = s.held[held_at(s, id) or 0]
        local u = bombers[id]
        if e ~= nil and u ~= nil then
            local stands = false
            if e.tgt ~= 0 then
                local t = ai.unit(e.tgt)
                stands = (t ~= nil and (t.hp or 1) > 0) or remembered[e.tgt] == true
                if stands and now - e.t0 >= AF_RUN_MS then
                    print(string.format("air give_up target=%d seen=%s hp=%d bomber_action=%s", e.tgt, tostring(t ~= nil),
                        t ~= nil and t.hp or -1, u.action or "?"))
                    s.gave_up = (s.gave_up or 0) + 1
                    stands = false
                end
            end
            local busy = u.action ~= "idle" and u.action ~= "inside"
            if stands then
                out = out + 1
                if not busy and now - e.t >= AS_AGAIN then
                    local code, reason, n = ai.attack({id}, e.tgt)
                    if n > 0 then
                        e.t = now
                    else
                        s.refused = (s.refused or 0) + 1
                        s.refused_reason = reason
                    end
                end
            else
                e.tgt = 0
                if not busy then
                    idle[#idle + 1] = e
                    if u.hp * 100 >= (u.hp_max or u.hp) * AF_READY_PCT then
                        ready[#ready + 1] = id
                    end
                end
            end
        end
    end
    if out > 0 then
        if doc ~= nil then   -- one wing at a time (the small runs meanwhile)
            runs_any(doc.view, memory, s, bombers, doc.pic, idle, now, doc.claim, doc.survive)
        end
        return
    end
    if s.wing_out then   -- its targets down: the force judges its push again now, not at its next attack time
        s.wing_out = false
        army.next_attack = now
        print(string.format("air wing_down at=%d,%d (the force judges its push again)", px, py))
    end
    local nr = #ready
    if nr < s.needed and nr < AF_WING_MAX then
        s.waits = (s.waits or 0) + 1
        if (s.wait_log or 0) == 0 or now - s.wait_log >= 60000 then
            s.wait_log = now > 0 and now or 1
            print(string.format("air wait at=%d,%d ready=%d needed=%d aa=%d defences=%d bombers=%d", px, py, nr, s.needed, na, nt, #ids))
        end
        if doc ~= nil then
            local har_out = 0
            for _, e in ipairs(s.held) do
                if (e.har or 0) > 0 then
                    har_out = har_out + 1
                end
            end
            if nr + har_out < s.needed and nr + har_out < AF_WING_MAX then   -- else those out come back to make it
                runs_any(doc.view, memory, s, bombers, doc.pic, idle, now, doc.claim, doc.survive)
            end
        end
        return
    end
    -- the launch: a flight at each anti-air, the rest in flights at the defences
    local by, order = {}, {}
    for i = 1, nr do
        local f = (i - 1) // AF_FLIGHT
        local t
        if f < na then
            t = aa[f + 1]
        elseif nt > 0 then
            t = tg[(f - na) % nt + 1]
        else
            t = aa[f % na + 1]
        end
        if by[t.id] == nil then
            by[t.id] = {}
            order[#order + 1] = t.id
        end
        local l = by[t.id]
        l[#l + 1] = ready[i]
    end
    local sent, at_aa = 0, 0
    for _, tid in ipairs(order) do
        local code, reason, n = ai.attack(by[tid], tid)
        if n > 0 then
            for _, id in ipairs(by[tid]) do
                local e = s.held[held_at(s, id)]
                e.tgt = tid
                e.t = now > 0 and now or 1
                e.t0 = e.t
            end
            sent = sent + n
            for f = 1, na do
                if aa[f].id == tid then
                    at_aa = at_aa + n
                end
            end
        end
        if n < #by[tid] then
            s.refused = (s.refused or 0) + #by[tid] - n
            s.refused_reason = reason
        end
        if doc ~= nil and doc.claim then
            for _, id in ipairs(by[tid]) do
                local e = s.held[held_at(s, id)]
                if e ~= nil and e.tgt == tid then
                    C.take(doc.view, memory, id, C.PRIO.task, C.WHO.bombers, AF_RUN_MS, bombers[id])
                end
            end
        end
    end
    if sent == 0 then
        return
    end
    s.waves = (s.waves or 0) + 1
    s.sent = (s.sent or 0) + sent
    s.wing_out = true
    print(string.format("air wing n=%d at=%d,%d%s at_aa=%d aa=%d targets=%d bombers=%d", sent, px, py,
        shoot ~= nil and " shooters" or "", at_aa, na, nt, #ids))
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step (airstrike_tick): every AS_EVERY. opts.any_epoch (TEST ONLY): no epoch gate

function M.step(view, memory, orders, budget, events, opts)
    memory.army = memory.army or {}
    memory.air = memory.air or {held = {}}
    local s = memory.air
    local now = view.time
    -- the shooters' spots: every step (upstream's 3-s scan; the events are the step's), in every age and with the
    -- air force off
    shooters(memory.army, events, now)
    -- THE WAY HOME (bomber_come_home, every AD_HOME_EVERY): upstream keeps it in the early return too - a bomber
    -- whose hit points no longer take it through the rest of its sortie is called back between the looks
    local doctrine = flag(S.bomber_doctrine, true)
    local claim = doctrine and flag(S.bomber_claim, true)
    local survive = doctrine and flag(S.bomber_survive, true)
    if claim then
        C.sweep(view, memory)   -- [claims.c] the expired claims give their stance back
    end
    if doctrine and flag(S.bomber_come_home, true) and s.pic ~= nil and #(s.held or {}) > 0
            and ((s.home_t or 0) == 0 or now - s.home_t >= AD_HOME_EVERY) then
        s.home_t = now > 0 and now or 1
        -- the bombers out at the last look only (their record's out: the picture and their hit points are as then)
        local boms = {}
        for _, e in ipairs(s.held) do
            if (e.out or 0) == 1 then
                local u = ai.unit(e.id)
                if u ~= nil then
                    boms[e.id] = u
                end
            end
        end
        if next(boms) ~= nil then
            home_tick(view, memory, s, boms, now)
        end
    end
    if (s.t or 0) > 0 and now >= s.t and now - s.t < AS_EVERY then
        return
    end
    s.t = now > 0 and now or 1
    if not S.air_strike or (view.epoch < BOMBER_EPOCH and not (opts and opts.any_epoch)) then
        return
    end
    local function order(o)
        orders[#orders + 1] = o
        return #orders
    end
    local bombers, ids = M.held(memory, now)
    if #ids > 0 and ((s.log_t or 0) == 0 or now - s.log_t >= 60000) then
        s.log_t = now > 0 and now or 1
        print(string.format("air force held=%d trained=%d airports=%d waves=%d sent=%d waits=%d gave_up=%d refused=%d/%s",
            #ids, s.trained or 0, s.airports or 0, s.waves or 0, s.sent or 0, s.waits or 0, s.gave_up or 0,
            s.refused or 0, tostring(s.refused_reason)))
        if doctrine then   -- "# bombers:" (upstream's doctrine line, a summary a minute)
            print(string.format("air bombers doctrine=%d survive=%d home=%d wing=%d claim=%d | lost=%d wing=%d runs=%d home=%d air=%d ground=%d"
                .. " | sorties=%d back_damaged=%d taken=%d expected=%d repaired=%d | called_home=%d doomed=%d"
                .. " | down_aa=%d bld=%d unit=%d worth=%d | ev_launch=%d best_ev=%d | cf_runs=%d cf_sent=%d",
                doctrine and 1 or 0, survive and 1 or 0, flag(S.bomber_come_home, true) and 1 or 0,
                flag(S.bomber_value_wing, true) and 1 or 0, claim and 1 or 0, s.lost or 0, s.lost_wing or 0,
                s.lost_har or 0, s.lost_home or 0, s.lost_air or 0, s.lost_ground or 0, s.sorties or 0,
                s.dmg_back or 0, s.dmg_taken or 0, s.dmg_pred or 0, s.repaired or 0, s.called_home or 0,
                s.doomed_on or 0, s.down_aa or 0, s.down_bld or 0, s.down_unit or 0, s.down_val or 0,
                s.ev_launch or 0, s.best_ev or 0, s.cf_runs or 0, s.cf_sent or 0))
        end
    end
    if doctrine then
        books(view, memory, s, bombers, ids, now, claim)
    end
    train(view, memory, s, order, budget, #ids, now)
    local px, py, shoot = M.position(memory, now)
    local harass = flag(S.bomber_harass, false)
    if (doctrine or harass) and #ids > 0 then
        picture(view, memory, s, bombers, ids, now)
    elseif #ids == 0 and s.pic ~= nil then
        s.pic = nil   -- no bombers: the last picture is no guide any more
    end
    if doctrine and flag(S.bomber_value_wing, true) and s.pic ~= nil then
        -- THE VALUE WING in place of the count below, at the force's position or at the enemy's buildings
        value_wing(view, memory, s, bombers, ids, s.pic, px, py, shoot, now, claim)
        return
    end
    if px == nil then
        -- no position: every free bomber to the runs (upstream's ah_idle)
        if s.pic ~= nil and #ids > 0 and (survive or harass) then
            local _, idle = free_lists(s, bombers, ids, now)
            runs_any(view, memory, s, bombers, s.pic, idle, now, claim, survive)
        end
        return
    end
    local aa, tg, remembered = M.targets(px, py, shoot)
    local doc = nil
    if doctrine or harass then
        doc = {view = view, pic = s.pic, claim = claim, survive = survive}
    end
    M.run(memory, now, bombers, ids, px, py, shoot, aa, tg, remembered, doc)
end

return M
