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

local function station(view, mem)
    local tx, ty = view.town_x, view.town_y
    if tx < 0 then
        return nil
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
    local room = view.population_cap * (100 - S.citizen_share) // 100 * S.home_guard_share // 100
    if e >= 1 then
        target = math.max(target, room)
    end
    -- the strongest enemy army seen in the last five minutes asks for more (the scout's news; the threat at the town)
    local seen = 0
    for _, en in ipairs(view.enemies or {}) do
        if en.last_seen > 0 and view.time - en.last_seen <= 300000 and en.seen_army > seen then
            seen = en.seen_army
        end
    end
    target = math.max(target, seen * 2 // 3 + 2)
    if S.early_army and e == 0 then
        return nil
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
        mem.plan = {producer = mem.last_producer, busy = false, wants = want}
        return mem.plan
    end
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
    local list = ai.memory{buildings = true, near = {x, y, r}, limit = 1}
    if #list > 0 then
        return list[1]
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
    mem.out = false
    mem.losing = 0
    mem.out_hp = 0
    mem.raid_x, mem.raid_y = -1, -1
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
    local hx, hy = station(view, mem)
    mem.home_x, mem.home_y = hx, hy
    local force = G.role(memory, "force")
    local free = G.free_soldiers(tx, ty)
    -- every land soldier (but the scouts) is the force's
    if #free > 0 then
        if force == nil then
            force = G.create(memory, {task = "defend", role = "force", x = hx, y = hy, radius = S.ar_home_r,
                                      home_x = hx, home_y = hy})
        end
        if mem.out then
            -- trained while the force is out: they gather at home (a defend group of their own that is folded in)
            local wait = G.role(memory, "wait")
            if wait == nil then
                wait = G.create(memory, {task = "defend", role = "wait", x = hx, y = hy, radius = S.ar_home_r})
            end
            G.add(memory, wait, free)
        else
            G.add(memory, force, free)
        end
    end
    if force == nil then
        return
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
                if u ~= nil and u.category == "building" then
                    standing = true
                end
            end
            if not standing then
                local nextb = nearest_building(cx, cy, 120)
                if nextb == nil then
                    go_home(view, memory, mem, force, "nothing_known")
                    return
                end
                local ok = beats(own, nextb.x, nextb.y, S.ar_go_pct)
                if not ok then
                    go_home(view, memory, mem, force, "next_too_strong")
                    return
                end
                force.x, force.y = nextb.x, nextb.y
                force.target = nextb.player
                force.way_t = 0
                mem.pushes_next = (mem.pushes_next or 0) + 1
                print(string.format("force next to=%d,%d player=%d", nextb.x, nextb.y, nextb.player))
                march(view, mem, force, nextb.x, nextb.y)
            end
        end
        return
    end
    -- at home: a raid answered (an enemy band at an own building within reach)
    if now - (mem.raid_t or 0) >= 4000 then
        mem.raid_t = now
        local raiders = ai.seen{enemy = true, armed = true, category = "soldier", near = {hx, hy, S.ar_raid_reach}}
        if #raiders > 0 then
            local cols = ai.get(raiders, {"x", "y", "player"})
            local sx, sy, n, player, hitting = 0, 0, 0, 0, false
            for k = 1, #raiders do
                local x, y = cols.x[k], cols.y[k]
                if x ~= nil then
                    local own_b = ai.units{category = "building", near = {x, y, S.ar_raid_r}, limit = 1}
                    if #own_b > 0 then
                        sx, sy, n = sx + x, sy + y, n + 1
                        player = cols.player[k]
                        hitting = hitting or (mem.hit_by[raiders[k]] == true)
                    end
                end
            end
            if n >= S.ar_raid_band or (n > 0 and hitting) then
                local rx, ry = sx // n, sy // n
                local foe_there = enemy_at(rx, ry, 12)
                local cooled = mem.raid_cool == nil or now >= mem.raid_cool or d2(rx, ry, mem.raid_cx or -99, mem.raid_cy or -99) > 15 * 15
                if cooled and own * 100 >= foe_there * 100
                        and (d2(rx, ry, hx, hy) > (S.ar_home_r + 2) * (S.ar_home_r + 2) or n >= S.ar_raid_band) then
                    force.task = "attack"
                    force.target = player
                    force.x, force.y = rx, ry
                    force.way_t = 0
                    force.aims = {}
                    mem.out = true
                    mem.raid_x, mem.raid_y = rx, ry
                    mem.out_hp = force_hp(force)
                    mem.losing = 0
                    mem.raids = (mem.raids or 0) + 1
                    print(string.format("force raid_answer at=%d,%d n=%d own=%d foe=%d", rx, ry, n, own, foe_there))
                    ai.move(force.units, rx, ry, {forced = false, formation = true})
                    force.way_t = now
                    return
                end
            end
        end
    end
    -- the town threatened (enemy soldiers on the claimed land): the force holds the town, the claim range wide
    if view.threat_strength > 0 then
        force.radius = math.max(view.claim_range, S.ar_home_r)
    else
        force.radius = S.ar_home_r
    end
    -- the push at the attack time: the enemy building known nearest home, when the force beats what is there
    if now >= mem.next_attack and #force.units >= S.ar_min_launch then
        local target = nearest_building(hx, hy, 200)
        if target == nil then
            mem.next_attack = now + 60000   -- nothing known yet: the scouts first
            return
        end
        local ok, foe = beats(own, target.x, target.y, S.ar_go_pct)
        if not ok then
            mem.waits = (mem.waits or 0) + 1
            mem.next_attack = now + 30000   -- the force grows; asked again in 30 s
            if mem.waits % 10 == 1 then
                print(string.format("force wait own=%d foe=%d at=%d,%d", own, foe, target.x, target.y))
            end
            return
        end
        force.task = "attack"
        force.target = target.player
        force.x, force.y = target.x, target.y
        force.way_t = 0
        force.aims = {}
        mem.out = true
        mem.raid_x, mem.raid_y = -1, -1
        mem.out_hp = force_hp(force)
        mem.losing = 0
        mem.pushes = (mem.pushes or 0) + 1
        mem.next_attack = now + (S.attack_interval_min + ai.random(0, S.attack_interval_max - S.attack_interval_min)) * 60000
        local legs = march(view, mem, force, target.x, target.y)
        print(string.format("force push to=%d,%d player=%d own=%d foe=%d units=%d legs=%d stage=%d,%d", target.x, target.y,
            target.player, own, foe, #force.units, legs, mem.stage_x or -1, mem.stage_y or -1))
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
    if #foes >= 4 and foe_str * 100 > own * S.muster_ratio then
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
    muster(view, memory, mem)
    if not mem.mustering then
        force_step(view, memory, mem, order)
    end
    scouts(view, memory, mem)
    G.step(view, memory)
    if view.time % 60000 < 1000 then
        local force = G.role(memory, "force")
        print(string.format("army force=%d out=%s next_attack=%d pushes=%d raids=%d musters=%d trained=%d mil=%d/%d/%d/%d/%d",
            force and #force.units or 0, tostring(mem.out or false), (mem.next_attack or 0) // 60000, mem.pushes or 0,
            mem.raids or 0, mem.musters or 0, mem.trained or 0, memory.eco.mil.food or 0, memory.eco.mil.wood or 0,
            memory.eco.mil.stone or 0, memory.eco.mil.gold or 0, memory.eco.mil.iron or 0))
    end
    return plan
end

return M
