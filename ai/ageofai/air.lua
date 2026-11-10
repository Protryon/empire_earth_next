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
-- Not ported: g_ar_breach's focus as the position (armies.c AR_BREACH, off upstream and not ported), ar_price_clear
-- (the port keeps no price), the [autotest] variant and the logs' summary (a print a minute here).
-- Deviations (fog-honest): a target that is a unit out of sight is "gone" (upstream's all-knowing unit_by_id kept it
-- as standing until off the map); a long-range unit at the shooters' spot counts only while seen (upstream let a
-- known one count unseen). Interface: a bomber landed in its airport is "inside" (a container): ai.attack refuses
-- it (cannot_do) where upstream's game air order (0x442AD8) launched it - reported as a gap; the refusals are
-- counted (memory.air.refused) and the order comes again every AS_AGAIN.
--
-- THE RECORD SHARED WITH skirmish.c (skirmish.lua reads it): memory.army.shooters = {{x=, y=, t=, hits=}, ...}, up
-- to AF_SHOOTS spots; a spot is alive while t > 0 and now - t < AF_SHOOT_MS (t: its last hit; hits: the hits seen
-- from it; t = 0: dropped, nothing left to hit there). M.shooters_spot(memory.army, now) -> x, y of the live spot hit
-- from most often, or nil. The spots are updated by M.step from the step's attacked events before any epoch check.
-- Its own state: memory.air (the held list: {id, tgt, t, t0} per bomber; the counters).
local S = require("ageofai.settings")
local V = require("lib.view")
local G = require("lib.groups")

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

-- the own bombers (records by id, the ids in order); new ones taken and listed
function M.held(memory)
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
        end
    end
    s.held = keep
    local take = {}
    for _, id in ipairs(ids) do
        if held_at(s, id) == nil and #s.held < AS_BOMBERS then
            if bombers[id].held ~= "lua" then
                take[#take + 1] = id
            end
            s.held[#s.held + 1] = {id = id, tgt = 0, t = 0, t0 = 0}
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
                    n = math.min(n, (budget.civil and budget.civil[r] or 0) // c.cost[r])
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
-- The wing

-- the run at a position: the wing out kept at it, the ready launched when enough. bombers, ids from M.held
function M.run(memory, now, bombers, ids, px, py, shoot, aa, tg, remembered)
    local s = memory.air
    local army = memory.army or {}
    local na, nt = #aa, #tg
    if na == 0 and nt == 0 then
        if shoot ~= nil then
            army.shooters[shoot].t = 0   -- nothing left at the shooters' spot: the next
        end
        return
    end
    s.needed = AF_FLIGHT * (na + (nt > 0 and 1 or 0))
    s.need_t = now > 0 and now or 1
    -- the wing out: a bomber whose target stands keeps at it; idle again, its next run
    local ready, out = {}, 0
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
                if not busy and u.hp * 100 >= (u.hp_max or u.hp) * AF_READY_PCT then
                    ready[#ready + 1] = id
                end
            end
        end
    end
    if out > 0 then
        return   -- one wing at a time
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
    local bombers, ids = M.held(memory)
    if #ids > 0 and ((s.log_t or 0) == 0 or now - s.log_t >= 60000) then
        s.log_t = now > 0 and now or 1
        print(string.format("air force held=%d trained=%d airports=%d waves=%d sent=%d waits=%d gave_up=%d refused=%d/%s", #ids,
            s.trained or 0, s.airports or 0, s.waves or 0, s.sent or 0, s.waits or 0, s.gave_up or 0, s.refused or 0,
            tostring(s.refused_reason)))
    end
    train(view, memory, s, order, budget, #ids, now)
    local px, py, shoot = M.position(memory, now)
    if px == nil then
        return
    end
    local aa, tg, remembered = M.targets(px, py, shoot)
    M.run(memory, now, bombers, ids, px, py, shoot, aa, tg, remembered)
end

return M
