-- Age of AI port, the army (upstream dll/ai/armies.c the force, stage.c the staging point, defence.c home_guard and
-- muster, econ.c substitute_soldier; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI
-- contributors). require("ageofai.army"). The soldiers are ordered one by one through the per-unit interface
-- (docs/AI_LUA_UNITS.md) with lib.groups' state machine.
--
-- THE FORCE: every land soldier is one force, ordered as one. Its home is the station (a bit behind the own tower
-- nearest the front, else the base): a member idle away from it walks there. At the attack time the force takes
-- the enemy building nearest its home that it knows (seen or remembered) when the whole force beats the enemy there
-- (the fighters seen and remembered within SG_FOE of it, the defences covering it) by ar_go_pct. Under that it waits
-- at home and grows. Its way goes round the danger it knows (ai.route over the threat and lost layers) to a staging
-- point - the last waypoint out of the enemy's reach - and from there as an attack-move (the engine fights what it
-- meets). Out, every ar_judge_every the force is judged as a whole against the enemy at its middle and at its
-- destination, the lesser; under ar_back_pct twice running, or bled to ar_bleed_pct, it goes home as one (a convoy:
-- the fast wait for the slow). The building down (none seen within ar_done of the destination with a member
-- there): the next known enemy building nearest the force's middle, judged the same; under ar_go_pct: home.
-- A raid (ar_raid_band enemy land fighters seen within ar_raid_r of an own building within ar_raid_reach of home,
-- or one attacking) while the force is home: the whole force attack-moves to it and goes home once no enemy fighter
-- is seen there. Soldiers trained while the force is out gather at home and join the next push.
-- HOME GUARD: every 10 s, fewer than 6 + home_guard x epoch soldiers at home (or home_guard_share of the military
-- room) -> up to 5 more, the strongest soldier the account pays for (substitute_soldier).
-- MUSTER: an enemy army in the base the soldiers and towers cannot fight (muster_enemies, muster_ratio): every own
-- soldier there goes to a rally point muster_out tiles beyond the base away from the enemy; the strike once the
-- soldiers there are muster_strike of the enemy in the base; ended when the enemy is gone or beatable again.
local V = require("lib.view")
local U = require("lib.units")
local G = require("lib.groups")
local S = require("ageofai.settings")
local T = require("ageofai.tactics")
local NV = require("ageofai.navy")
local AT = require("ageofai.attack")   -- [attack.c] the weak spots, the breaches, the objective
local MS = require("ageofai.mass")     -- [mass.c] the army as one: the reinforcements' rally, the wait, the tether
local DT = require("ageofai.detach")   -- [armies.c det] the detachments, the raiders, the riders' look, the sites
local SK = require("ageofai.skirmish") -- [skirmish.c] the sharpshooters' sorties, the spotter
local MT = require("ageofai.metal")    -- [metal.c] the soldier that does most per gold + iron
local FR = require("ageofai.fronts")   -- [fronts.c] the front station
local M = {}

local SG_FOE = 20                   -- tiles round the target the enemy there is counted in
local HOME_SCAN = 25                -- tiles: soldiers this near the base are at home (home_guard's 625)
local LOOK_EVERY = 240000           -- a look at the enemy's base every 4 minutes once it is known

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

-- ---------------------------------------------------------------------------------------------------------------
-- The soldier to train (substitute_soldier): the strongest land soldier (hit points x attack) the account pays for
-- and a finished producer makes

local SHIP_PRODUCERS = {b_dock = true, b_navy_yard = true, b_space_dock = true}

function M.pick_soldier(view, avail, want)
    local best, best_score, best_n = nil, 0, 0
    for _, c in ipairs(view.costs) do
        if c.military and not c.building and c.producer ~= nil and c.producers > 0 and not SHIP_PRODUCERS[c.producer] then
            local cls = ai.class(c.type)
            local score = cls ~= nil and (cls.hp or 0) * (cls.attack or 0) or 0
            if score > 0 and V.sum(c.cost) > 0 then
                local n = want
                for _, r in ipairs(V.resources) do
                    if c.cost[r] > 0 then
                        n = math.min(n, avail[r] // c.cost[r])
                    end
                end
                if n > 0 and score > best_score then
                    best, best_score, best_n = c, score, n
                end
            end
        end
    end
    return best, best_n
end

-- the strength of the enemy units of the memory within r of a tile: hit points x attack of their class (the
-- fighters and the defences; citizens and boats count nothing)
local function enemy_at(x, y, r, player)
    local f = {near = {x, y, r}}
    if player ~= nil and player > 0 then
        f.player = player
    end
    local list = ai.memory(f)
    local str, n = 0, 0
    for _, m in ipairs(list) do
        local cls = ai.class(m.type)
        if cls ~= nil and (cls.attack or 0) > 0 then
            str = str + m.hp * cls.attack
            if not m.building then
                n = n + 1
            end
        end
    end
    return str, n
end

-- ---------------------------------------------------------------------------------------------------------------
-- Home: the station

local function station(view, mem, memory)
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return nil
    end
    -- [fronts.c] armies.c ar_home: the first front's station (its tower nearest the heaviest front, FA_BEHIND back
    -- towards the base) when it is out of danger; else as below
    if memory ~= nil then
        local sx, sy = FR.station(memory, 1)
        if sx ~= nil and ai.danger(sx, sy) == 0 then
            return sx, sy
        end
    end
    -- the own tower nearest the front, 3 tiles back towards the town
    local fx, fy = mem.front_x or 0, mem.front_y or 0
    local best, bd = nil, nil
    local towers = ai.units{category = "building", armed = true, near = {tx, ty, 20}}
    for _, id in ipairs(towers) do
        local u = ai.unit(id)
        if u ~= nil and u.built and u.type ~= "b_town_center" and u.type ~= "b_capitol" then
            local along = (u.x - tx) * fx + (u.y - ty) * fy
            if bd == nil or along > bd then
                best, bd = u, along
            end
        end
    end
    if best ~= nil and bd ~= nil and bd > 0 then
        local len = isqrt(fx * fx + fy * fy)
        if len > 0 then
            return best.x - fx * 3 // len, best.y - fy * 3 // len
        end
        return best.x, best.y
    end
    return tx, ty
end

-- ---------------------------------------------------------------------------------------------------------------
-- Training

local function train(view, memory, mem, order, budget)
    if view.time - (mem.train_t or 0) < 10000 or view.town_x < 0 then
        return mem.plan
    end
    mem.train_t = view.time
    local e = view.epoch
    local target = 6 + S.home_guard * e
    -- defence.c home_guard: max(6 + 4 x epoch, home_guard_share x military room); the room from the cap upstream's
    -- unit_limit means (the game's cap that follows the human's population, about 166 in its games): here the
    -- lobby's limit (400) stands in the view, so the cap is home_guard_cap at least and the own population when
    -- larger, never above the lobby's (ENGINE: see settings.lua)
    local cap = math.min(view.population_cap, math.max(S.home_guard_cap, view.population))
    local room = cap * (100 - S.citizen_share) // 100 * S.home_guard_share // 100
    target = math.max(target, room)
    if memory.eco ~= nil and memory.eco.recovering and view.threat_strength == 0 then
        return nil   -- RECOVERY: no soldiers unless enemy soldiers are in the base
    end
    if S.early_army and e == 0 then
        -- the first epoch's soldiers only once enemy soldiers have shown up (the threats seen, or a raid answered):
        -- a quiet map keeps the owner's opening, a raided one gets upstream's alternative
        if view.threat_strength > 0 or (mem.raids or 0) > 0 then
            mem.threat_seen_t = view.time
        end
        target = (mem.threat_seen_t ~= nil and view.time >= 300000) and (S.early_soldiers or 0) or 0
        if target == 0 then
            return nil
        end
    end
    -- soldiers at home: the force's members within HOME_SCAN of the base, and those coming
    local home = 0
    local force = G.role(memory, "force")
    local tx, ty = view.town_x, view.town_y
    if force ~= nil then
        for _, id in ipairs(force.units) do
            local u = ai.unit(id)
            if u ~= nil and d2(u.x, u.y, tx, ty) <= HOME_SCAN * HOME_SCAN then
                home = home + 1
            end
        end
    end
    local coming = 0
    for _, c in ipairs(view.costs) do
        if c.military and not c.building and not SHIP_PRODUCERS[c.producer or ""] then
            coming = coming + c.requested + c.in_production
        end
    end
    home = home + coming
    local want = {}
    if home >= target then
        mem.plan = {producer = mem.last_producer, busy = false, wants = want}
        return mem.plan
    end
    local pick, n = M.pick_soldier(view, budget.avail, math.min(target - home, 5))
    -- [metal.c] best_per_metal: the soldier asked for (the strongest affordable) is trained as the one that does most
    -- against the enemy army known per gold + iron, when that one is 25% better (mt_choose; the budget pays as before)
    do
        local alt, an = MT.choose(view, memory, pick, budget.avail, view.stock, budget.hold or {}, math.min(target - home, 5))
        if alt ~= nil then
            pick, n = alt, an
        end
    end
    if pick == nil then
        -- what the cheapest soldier costs: the workers gather for it
        local cheap = nil
        for _, c in ipairs(view.costs) do
            if c.military and not c.building and c.producer ~= nil and not SHIP_PRODUCERS[c.producer]
                    and (cheap == nil or V.sum(c.cost) < V.sum(cheap.cost)) then
                cheap = c
            end
        end
        if cheap ~= nil then
            for _, r in ipairs(V.resources) do
                want[r] = cheap.cost[r] * math.min(target - home, 5)
            end
        end
        -- [metal.c] MIX counts what the refused order was short of (the cheapest soldier's price against the account)
        MT.note_short(view, memory, cheap ~= nil and cheap.cost or nil, budget.avail)
        mem.plan = {producer = mem.last_producer, busy = false, wants = want}
        return mem.plan
    end
    MT.note_short(view, memory, nil, budget.avail)   -- [metal.c] the minute's window rolls
    -- the producers' queues: train_queue at most each
    local b = V.buildings(view, pick.producer)
    local busy = b ~= nil and b.finished > 0 and pick.requested + pick.in_production >= S.train_queue * b.finished
    if not busy then
        n = math.min(n, S.train_queue * (b and b.finished or 1) - pick.requested - pick.in_production)
        if n > 0 then
            order({verb = "train", type = pick.type, n = n})
            mem.last_producer = pick.producer
            mem.trained = (mem.trained or 0) + n
        end
    end
    for _, r in ipairs(V.resources) do
        want[r] = pick.cost[r] * math.max(target - home - n, 0)
    end
    mem.plan = {producer = pick.producer, busy = busy, wants = want}
    return mem.plan
end

-- ---------------------------------------------------------------------------------------------------------------
-- The force

-- the enemy building known nearest a tile (the memory), the enemy's drop sites and towers like any other
local function nearest_building(x, y, r)
    local list = ai.memory{buildings = true, near = {x, y, r}, limit = 8}
    for _, m in ipairs(list) do
        if not AT.is_wall(m.type) then   -- [attack.c] never a wall piece (a gap is the breach's business)
            return m
        end
    end
    return nil
end

-- the force beats the enemy at d by pct?
local function beats(own, x, y, pct)
    local foe = enemy_at(x, y, SG_FOE)
    return own * 100 >= foe * pct, foe
end

-- the way to a destination: the route round the danger; the staging point is the last waypoint whose cell shows no
-- threat; the attack-move goes from there
local function march(view, mem, force, x, y)
    local cx, cy = force.cx or view.town_x, force.cy or view.town_y
    local path = ai.route(cx, cy, x, y, {avoid = S.ar_route_avoid, avoid_lost = S.ar_route_avoid_lost})
    local stage_x, stage_y = nil, nil
    if path ~= nil then
        for k = 1, #path - 1 do
            if ai.danger(path[k][1], path[k][2]) == 0 then
                stage_x, stage_y = path[k][1], path[k][2]
            end
        end
    end
    mem.stage_x, mem.stage_y = stage_x or -1, stage_y or -1
    if stage_x ~= nil then
        -- the legs to the staging point as forced moves in formation (nothing fights on the way), then the
        -- attack-move; lib.groups moves the force on from there
        local legs = {}
        for k = 1, #path - 1 do
            legs[#legs + 1] = path[k]
            if path[k][1] == stage_x and path[k][2] == stage_y then
                break
            end
        end
        for k, w in ipairs(legs) do
            ai.move(force.units, w[1], w[2], {queue = k > 1, forced = true, formation = true})
        end
        ai.move(force.units, x, y, {queue = true, forced = false})
        force.way_t = view.time   -- lib.groups gives its own leg again only after MOVE_MS
        return #legs
    end
    return 0
end

local function go_home(view, memory, mem, force, why)
    local hx, hy = mem.home_x or view.town_x, mem.home_y or view.town_y
    if mem.raid_x ~= nil and mem.raid_x >= 0 and why ~= "raid_over" then
        mem.raid_cool = view.time + 120000
        mem.raid_cx, mem.raid_cy = mem.raid_x, mem.raid_y
    end
    print(string.format("force home why=%s units=%d str=%d", why, #force.units, force.strength or 0))
    G.retreat(view, memory, force, hx, hy)
    -- the convoy: the move in formation, the fast wait for the slow
    ai.move(force.units, hx, hy, {forced = true, formation = true})
    force.way_t = view.time
    -- [attack.c] the breach is over (breach_note -1), the focus with it; the siege's shelling stops
    AT.breach_forget(mem)
    force.focus_x, force.focus_y = -1, -1
    mem.shell = {}
    mem.out = false
    mem.losing = 0
    mem.out_hp = 0
    mem.raid_x, mem.raid_y = -1, -1
    mem.back_t = view.time   -- [armies.c det] the force came home: a site asked of it waits AR_DET_PAUSE
end

-- the force's hit points now
local function force_hp(force)
    local cols = ai.get(force.units, {"hp"})
    local hp = 0
    for k = 1, #force.units do
        hp = hp + (cols.hp[k] or 0)
    end
    return hp
end

local function force_step(view, memory, mem, order)
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return
    end
    local hx, hy = station(view, mem, memory)
    mem.home_x, mem.home_y = hx, hy
    local force = G.role(memory, "force")
    local free = G.free_soldiers(tx, ty)
    -- every land soldier (but the scouts) is the force's
    if #free > 0 then
        if force == nil then
            force = G.create(memory, {task = "defend", role = "force", x = hx, y = hy, radius = S.ar_home_r,
                                      home_x = hx, home_y = hy})
        end
        if not mem.out then
            G.add(memory, force, free)
        end
    end
    if force == nil then
        return
    end
    mem.home_x, mem.home_y = hx, hy
    if mem.out then
        -- [mass.c] trained while the force is out: they gather at home (a defend group of their own that is folded
        -- in); with army_mass GATHER at the rally by the towers, released together as a group
        MS.gather(view, memory, mem, force, G.role(memory, "wait"), free)
    end
    local wait = G.role(memory, "wait")
    if wait ~= nil and not mem.out then
        G.add(memory, force, wait.units)
        G.disband(memory, wait)
    end
    force.home_x, force.home_y = hx, hy
    local now = view.time
    local own = force.strength or 0
    -- the attack time
    if mem.next_attack == nil then
        mem.next_attack = (S.attack_first_min + ai.random(0, S.attack_first_max - S.attack_first_min)) * 60000
    end
    if mem.out then
        -- out: a push or a raid answer
        local cx, cy = force.cx or hx, force.cy or hy
        if force.task == "defend" then   -- the retreat ended: home
            mem.out = false
            return
        end
        if mem.raid_x ~= nil and mem.raid_x >= 0 then
            -- the raid answered: home once no enemy fighter is seen there
            local foes = ai.seen{enemy = true, armed = true, near = {mem.raid_x, mem.raid_y, S.ar_raid_r}}
            local fighters = 0
            for _, id in ipairs(foes) do
                local u = ai.unit(id)
                if u ~= nil and u.category == "soldier" then
                    fighters = fighters + 1
                end
            end
            if fighters == 0 then
                go_home(view, memory, mem, force, "raid_over")
                return
            end
        end
        if now - (mem.judge_t or 0) < S.ar_judge_every then
            return
        end
        mem.judge_t = now
        -- the judge: the force against the enemy at its middle and at its destination, the lesser
        local foe_mid = enemy_at(cx, cy, 12)
        local foe_dest = enemy_at(force.x, force.y, SG_FOE)
        local foe = math.max(foe_mid, foe_dest)
        local hp = force_hp(force)
        if (mem.out_hp or 0) > 0 and hp * 100 < mem.out_hp * (100 - S.ar_bleed_pct) then
            go_home(view, memory, mem, force, "bled")
            return
        end
        if foe > 0 and own * 100 < foe * S.ar_back_pct then
            mem.losing = (mem.losing or 0) + 1
            if mem.losing >= 2 then
                go_home(view, memory, mem, force, "losing")
                return
            end
        else
            mem.losing = 0
        end
        if mem.raid_x ~= nil and mem.raid_x >= 0 then
            return   -- a raid answered: no next building
        end
        -- the destination reached and nothing of the enemy's there: the next building nearest the middle
        local here = false
        for _, id in ipairs(force.units) do
            local u = ai.unit(id)
            if u ~= nil and d2(u.x, u.y, force.x, force.y) <= S.ar_done * S.ar_done then
                here = true
                break
            end
        end
        if here or force.state == "arrived" then
            local left = ai.seen{enemy = true, near = {force.x, force.y, S.ar_done}}
            local standing = false
            for _, id in ipairs(left) do
                local u = ai.unit(id)
                -- [attack.c] a wall piece standing is no target left (the breach segment down: on to the objective)
                if u ~= nil and u.category == "building" and not AT.is_wall(u.type) then
                    standing = true
                end
            end
            if not standing then
                -- [attack.c] the next building: the target's objective (forces.c), else wave_retarget's choice
                local nextb = AT.next_target(mem, force.target, cx, cy)
                if nextb == nil then
                    go_home(view, memory, mem, force, "nothing_known")
                    return
                end
                local ok = beats(own, nextb.x, nextb.y, S.ar_go_pct)
                if not ok then
                    go_home(view, memory, mem, force, "next_too_strong")
                    return
                end
                -- [attack.c] the entry point: the weak spot, the breach
                local entry = AT.entry(mem, cx, cy, nextb)
                force.x, force.y = entry.x, entry.y
                force.focus_x, force.focus_y = entry.focus_x, entry.focus_y
                force.target = nextb.player
                force.way_t = 0
                mem.pushes_next = (mem.pushes_next or 0) + 1
                print(string.format("force next to=%d,%d player=%d weak=%s breach=%s", entry.x, entry.y, nextb.player,
                    tostring(entry.weak), tostring(entry.breach)))
                march(view, mem, force, entry.x, entry.y)
            end
        end
        return
    end
    -- [skirmish.c] THE BAIT: the party's chasers near home - the force goes at them as at a raid (home again when
    -- they are cleared)
    local bait = SK.bait(mem, now)
    if bait ~= nil then
        force.task = "attack"
        force.target = 0
        force.x, force.y = bait.x, bait.y
        force.way_t = 0
        force.aims = {}
        mem.out = true
        mem.raid_x, mem.raid_y = bait.x, bait.y
        mem.out_hp = force_hp(force)
        mem.losing = 0
        mem.raids = (mem.raids or 0) + 1
        print(string.format("force bait_answer at=%d,%d n=%d own=%d", bait.x, bait.y, bait.n, own))
        ai.move(force.units, bait.x, bait.y, {forced = false, formation = true})
        force.way_t = now
        return
    end
    -- at home: a raid answered (armies.c: every AR_RAID_EVERY with AR_MIN_RAID soldiers, the raider nearest home with
    -- a band of ar_raid_band enemy fighters within ar_raid_r of an own building, or one hitting ours, seen there twice
    -- running (within AR_RAID_SAME of the last look); the force goes when it beats the enemy there by raid_answer)
    if now - (mem.raid_t or 0) >= 4000 and #force.units >= 3 then
        mem.raid_t = now
        local raiders = ai.seen{enemy = true, armed = true, category = "soldier", near = {hx, hy, S.ar_raid_reach}}
        local rx, ry, rn, rplayer, rd = nil, nil, 0, 0, nil
        if #raiders > 0 then
            local cols = ai.get(raiders, {"x", "y", "player"})
            for k = 1, #raiders do
                local x, y = cols.x[k], cols.y[k]
                if x ~= nil then
                    local d = d2(x, y, hx, hy)
                    if rd == nil or d < rd then
                        local own_b = ai.units{category = "building", near = {x, y, S.ar_raid_r}, limit = 1}
                        if #own_b > 0 then
                            local band = ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, S.ar_raid_r}}
                            local hitting = false
                            for _, id in ipairs(band) do
                                hitting = hitting or (mem.hit_by[id] == true)
                            end
                            if #band >= S.ar_raid_band or hitting then
                                rx, ry, rn, rplayer, rd = x, y, #band, cols.player[k], d
                            end
                        end
                    end
                end
            end
        end
        local again = rx ~= nil and (mem.raid_c_t or 0) > 0 and now - mem.raid_c_t <= 8000
            and d2(rx, ry, mem.raid_cx or -1000, mem.raid_cy or -1000) <= 10 * 10
        mem.raid_cx, mem.raid_cy = rx or -1000, ry or -1000
        mem.raid_c_t = rx ~= nil and now or 0
        if again then
            local foe_there = enemy_at(rx, ry, 12)
            local need = math.max(S.ar_back_pct, S.raid_answer)
            if own * 100 >= foe_there * need then
                force.task = "attack"
                force.target = rplayer
                force.x, force.y = rx, ry
                force.way_t = 0
                force.aims = {}
                mem.out = true
                mem.raid_x, mem.raid_y = rx, ry
                mem.out_hp = force_hp(force)
                mem.losing = 0
                mem.raids = (mem.raids or 0) + 1
                print(string.format("force raid_answer at=%d,%d n=%d own=%d foe=%d", rx, ry, rn, own, foe_there))
                ai.move(force.units, rx, ry, {forced = false, formation = true})
                force.way_t = now
                return
            end
        end
    end
    -- the town threatened (enemy soldiers on the claimed land): the force holds the town, the claim range wide
    if view.threat_strength > 0 then
        force.radius = 12
        if force.task == "defend" and d2(force.x, force.y, tx, ty) > 36 then
            force.x, force.y = tx, ty   -- the town itself, not the station
            force.way_t = 0
        end
    else
        force.radius = S.ar_home_r
        if force.task == "defend" and (force.x ~= hx or force.y ~= hy) then
            force.x, force.y = hx, hy
            force.way_t = 0
        end
    end
    -- [armies.c det] THE SITES' MEMORY: a defended mine (a detachment failed at it, or towers cover it) is the
    -- force's without waiting for the attack time (ar_site_asked): AR_DET_MIN at home, asked every 30 s, AR_DET_PAUSE
    -- after the force came home
    local asked = nil
    if S.mine_raids and #force.units >= 30 and now - (mem.ask_t or 0) >= 30000 and now - (mem.back_t or 0) >= 180000 then
        mem.ask_t = now
        asked = DT.site_asked(view, mem, hx, hy)
    end
    -- the push at the attack time: the enemy building known nearest home, when the force beats what is there
    if asked ~= nil or (now >= mem.next_attack and #force.units >= S.ar_min_launch) then
        local target = asked or nearest_building(hx, hy, 200)
        if target == nil then
            mem.next_attack = now + 60000   -- nothing known yet: the scouts first
            return
        end
        -- [mass.c] WAIT: the land army against the strongest enemy army known; the attack time moved on while weaker
        if asked == nil and MS.wait(view, mem, ai.strength(ai.units{category = "soldier"})) then
            mem.next_attack = now + 20000
            return
        end
        local ok, foe = beats(own, target.x, target.y, S.ar_go_pct)
        if not ok then
            mem.waits = (mem.waits or 0) + 1
            mem.next_attack = now + 30000   -- the force grows; asked again in 30 s
            -- [skirmish.c, airstrike.c] the target found too strong: the skirmishers' and the bombers' position
            -- (a->strong, strong_t)
            mem.strong_x, mem.strong_y, mem.strong_t = target.x, target.y, now > 0 and now or 1
            if mem.waits % 10 == 1 then
                print(string.format("force wait own=%d foe=%d at=%d,%d", own, foe, target.x, target.y))
            end
            return
        end
        -- [attack.c] the entry point (hook_asg_route): the weak spot of the target, the breach through a wall on the
        -- way; the breach segment is the force's focus, the leash holds its members out of known defences
        local entry = AT.entry(mem, force.cx or hx, force.cy or hy, target)
        force.task = "attack"
        force.target = target.player
        force.x, force.y = entry.x, entry.y
        force.focus_x, force.focus_y = entry.focus_x, entry.focus_y
        force.leash = S.attack_leash
        force.way_t = 0
        force.aims = {}
        mem.out = true
        mem.raid_x, mem.raid_y = -1, -1
        mem.out_hp = force_hp(force)
        mem.losing = 0
        mem.pushes = (mem.pushes or 0) + 1
        mem.next_attack = now + (S.attack_interval_min + ai.random(0, S.attack_interval_max - S.attack_interval_min)) * 60000
        local legs = march(view, mem, force, entry.x, entry.y)
        print(string.format("force push to=%d,%d player=%d own=%d foe=%d units=%d legs=%d stage=%d,%d weak=%s breach=%s",
            entry.x, entry.y, target.player, own, foe, #force.units, legs, mem.stage_x or -1, mem.stage_y or -1,
            tostring(entry.weak), tostring(entry.breach)))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Muster (defence.c): the soldiers at home leave a base they cannot hold, gather beyond it and strike together

local function muster(view, memory, mem)
    if not S.muster or view.town_x < 0 or view.time - (mem.muster_t or 0) < 3000 then
        return
    end
    mem.muster_t = view.time
    local tx, ty = view.town_x, view.town_y
    local foes = ai.seen{enemy = true, armed = true, category = "soldier", near = {tx, ty, 22}}
    local force = G.role(memory, "force")
    if force == nil then
        return
    end
    local foe_str, fx, fy = 0, 0, 0
    local cols = ai.get(foes, {"x", "y", "hp", "attack"})
    for k = 1, #foes do
        if cols.hp[k] ~= nil then
            foe_str = foe_str + cols.hp[k] * (cols.attack[k] or 0)
            fx, fy = fx + cols.x[k], fy + cols.y[k]
        end
    end
    local own = 0
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and d2(u.x, u.y, tx, ty) <= 35 * 35 then
            own = own + u.hp * (u.attack or 0)
        end
    end
    local towers = ai.units{category = "building", armed = true, built = true, near = {tx, ty, 22}}
    own = own + ai.strength(towers)
    if mem.mustering then
        local rally = G.role(memory, "rally")
        if rally == nil then
            mem.mustering = false
            return
        end
        if #foes < 3 or (foe_str > 0 and own * 100 >= foe_str * S.muster_ratio) then
            -- the enemy gone or beatable: the soldiers go back to the force
            G.add(memory, force, rally.units)
            G.disband(memory, rally)
            mem.mustering = false
            mem.muster_cool = view.time + 30000
            print("muster end")
            return
        end
        if (rally.strength or 0) * 100 >= foe_str * S.muster_strike and #rally.units > 0 then
            G.add(memory, force, rally.units)
            G.disband(memory, rally)
            mem.mustering = false
            mem.muster_cool = view.time + 60000
            force.task = "defend"
            force.x, force.y = tx, ty
            force.radius = 22
            print(string.format("muster strike own=%d foe=%d", rally.strength or 0, foe_str))
        end
        return
    end
    if view.time < (mem.muster_cool or 0) or mem.out then
        return
    end
    if #foes >= S.muster_enemies and foe_str * 100 > own * S.muster_ratio then
        fx, fy = fx // #foes, fy // #foes
        local dx, dy = tx - fx, ty - fy
        local len = isqrt(dx * dx + dy * dy)
        if len < 1 then
            dx, dy, len = 1, 0, 1
        end
        local rx, ry = tx + dx * S.muster_out // len, ty + dy * S.muster_out // len
        local map = ai.map()
        rx = math.max(2, math.min(rx, map.width - 3))
        ry = math.max(2, math.min(ry, map.height - 3))
        local rally = G.create(memory, {task = "defend", role = "rally", x = rx, y = ry, radius = 8})
        local home = {}
        for _, id in ipairs(force.units) do
            local u = ai.unit(id)
            if u ~= nil and d2(u.x, u.y, tx, ty) <= 35 * 35 then
                home[#home + 1] = id
            end
        end
        G.add(memory, rally, home)
        ai.move(rally.units, rx, ry, {forced = true, formation = true})
        rally.way_t = view.time
        mem.mustering = true
        mem.musters = (mem.musters or 0) + 1
        print(string.format("muster start foes=%d foe=%d own=%d rally=%d,%d", #foes, foe_str, own, rx, ry))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Scouting: one soldier explores from scout_at (again when lost, scouts_max in all); a look at the enemy's base
-- every LOOK_EVERY once it is known

local function scouts(view, memory, mem)
    local scout = G.role(memory, "scout")
    if scout ~= nil and scout.state == "done" then
        G.disband(memory, scout)
        scout = nil
    end
    local force = G.role(memory, "force")
    if scout == nil and view.time >= S.scout_at and (mem.scouts or 0) < S.scouts_max and force ~= nil
            and #force.units > 1 and not mem.out then
        local g = G.create(memory, {task = "scout", role = "scout"})
        if G.recruit(memory, g, 1, {from = force, x = view.town_x, y = view.town_y}) == 0 then
            G.disband(memory, g)
        else
            mem.scouts = (mem.scouts or 0) + 1
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- The step

function M.step(view, memory, orders, budget, events)
    memory.army = memory.army or {}
    local mem = memory.army
    -- the own units attacked since the last step, by a seen attacker (a raid: one of them attacking ours)
    mem.hit_by = {}
    for _, e in ipairs(events or {}) do
        if e.kind == "attacked" and e.attacker ~= nil then
            mem.hit_by[e.attacker] = true
        end
    end
    local function order(o)
        orders[#orders + 1] = o
    end
    ai.config{memory_ms = S.fight_memory_ms}
    -- the front (econ's), for the station
    local E = require("ageofai.econ")
    local fx, fy, len = E.front(view, memory.eco or {})
    mem.front_x, mem.front_y = fx, fy
    local plan = train(view, memory, mem, order, budget)
    T.siege(view, mem, order, budget)
    -- the counter-fire's committed units: the groups leave them alone
    G.committed = {}
    for _, id in ipairs(ai.keys(mem.cf or {})) do
        if mem.cf[id] > view.time then
            G.committed[id] = true
        end
    end
    -- [attack.c] the siege shelling from its firing position (siege_tick): committed too
    T.siege_fire(view, mem, G.role(memory, "force"))
    for _, id in ipairs(ai.keys(mem.shell or {})) do
        if mem.shell[id] > view.time then
            G.committed[id] = true
        end
    end
    muster(view, memory, mem)
    if not mem.mustering then
        force_step(view, memory, mem, order)
    end
    -- [armies.c det, skirmish.c, mass.c] after the force: the detachments and the raiders, the skirmishers' party
    -- and spotter (the shooters' spots from this step's attacked events), the tether and the summary
    SK.note_hits(mem, events, view.time)
    if not mem.mustering then
        local force = G.role(memory, "force")
        DT.step(view, memory, mem, force, order, budget)
        SK.step(view, memory, mem, force, order)
        MS.tether(view, mem, force)
        MS.report(view, memory, mem, force)
    end
    scouts(view, memory, mem)
    NV.step(view, memory, orders, budget, G.role(memory, "force"))
    G.step(view, memory)
    if view.time % 60000 < 1000 then
        local force = G.role(memory, "force")
        local at = mem.at or {}
        print(string.format("army force=%d out=%s next_attack=%d pushes=%d raids=%d musters=%d trained=%d mil=%d/%d/%d/%d/%d weak=%d breach=%d/%d retarget=%d leashed=%d shells=%d",
            force and #force.units or 0, tostring(mem.out or false), (mem.next_attack or 0) // 60000, mem.pushes or 0,
            mem.raids or 0, mem.musters or 0, mem.trained or 0, memory.eco.mil.food or 0, memory.eco.mil.wood or 0,
            memory.eco.mil.stone or 0, memory.eco.mil.gold or 0, memory.eco.mil.iron or 0, at.weak_spots or 0,
            at.breaches or 0, at.breach_none or 0, at.retargets or 0, force and (force.leashed or 0) or 0, mem.shells or 0))
    end
    return plan
end

return M
