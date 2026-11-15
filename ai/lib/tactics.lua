-- Combat tactics for any script's lib.groups army, adapted from Grille's "Empire Earth: Age of AI"
-- (https://github.com/Grille/Empire-Earth-Age-of-AI; GPL-3.0, Copyright (C) 2025-2026 Grille and the Age of AI
-- contributors; used with the author's permission, see ai/ageofai/LICENSE and ai/ageofai/UPSTREAM). The sources are
-- the port's ai/ageofai/army.lua (armies.c: the force's judgement), tactics.lua (counterfire.c; attack.c siege_tick)
-- and attack.lua (attack.c: the leash, the siege's firing position). require("lib.tactics"). AI_RESEARCH §8.86 has
-- the measurements.
--
-- Every tactic is off until a script turns it on (M.configure{...}); each is bounded per step:
-- JUDGE (armies.c ar_judge): every JUDGE_MS an attack group out is weighed against the enemy it knows (the memory's
--   fighters and defences, hit points x attack) within JUDGE_R tiles of its middle and of its place, the larger; under
--   judge_back_pct of it twice running it goes home (a forced move, then a defence there). At most JUDGE_MAX groups
--   a step. The script decides what a group home does next (classic_lua: a defence task).
-- GO (armies.c ar_go_pct, the push's test): an attack group's new place is weighed once (the known enemy within
--   GO_R tiles of it); under go_pct of it the attack is called off as JUDGE sends a group home.
-- COUNTERFIRE (counterfire.c, the micro step): own units hit by a seen shooter (range 5 or more) from beyond their
--   own reach + 1; shooters within 6 tiles of each other one target; own soldiers within 24 tiles of it, nearest
--   first, not committed, on its land, join until they are 1.5 x the enemy fighters seen within 10 tiles of it (at
--   most 60); short of that nobody goes. Committed CF_MS (the groups give them no order: lib.groups M.committed).
--   At most CF_SHOOTERS shooters a call.
-- LEASH (attack.c attack_leash): lib.groups' g.leash on the attack groups: no enemy inside known defences is a
--   target unless the group's place lies inside them too.
-- SIEGE (attack.c siege_tick): every SIEGE_MS an attack group's siege weapons (catapults, trebuchets, cannons)
--   take the nearest known defence they outrange within SIEGE_NEAR of the group from the firing position (range -
--   0.7, turned in 15-degree steps until no other known defence reaches it): those within 2 tiles of it attack, the
--   rest walk there (forced); held while enemy soldiers are seen within 8 tiles of the position. Committed for three
--   looks.
-- WEAK SPOT (attack.c attack_weak_spots): an attack group's new place at an enemy building (within 4 tiles) that
--   known defences cover (their range + WEAK_MARGIN) becomes the known enemy building within WEAK_R of it that none
--   covers, on the group's land, the nearest the group (its distance + half the distance to the place least) - the
--   economy behind the towers, the open flank. Looked at once a place.
-- SAFE POST (attack.c avoid_towers, defence.c's stations behind the own towers): a defence group's new place that
--   known enemy defences cover (their range + WEAK_MARGIN) is given up for home (the script's home(g)): the
--   classic's defence tasks pick random forests, hills and choke points of the continent, some under the enemy's
--   towers. Looked at once a place.
-- POOL (armies.c: every land soldier is the force's, at home at its station; the script runs it, see
--   classic_lua/army.lua pool_step): the soldiers in no group are the script's, a defence group at home, instead of
--   the engine's (for the classic: the C++ attack commander's continent groups, which walked them to random places
--   and into the enemy's towers). The script's groups still take them as free soldiers.
-- FOCUS (not Age of AI's: its force spreads its fire through lib.groups as the classic does): lib.groups' g.focus:
--   a unit choosing a target prefers one the group already fires at while that fire is short of its hit points, then
--   the most hurt (see groups.lua fight).
-- HURT (cover.c fall_back, Age of AI's for warships, here for every soldier): a soldier of a fighting group under
--   hurt_pct of its hit points walks HURT_BACK tiles back towards home (forced) and stays out of the fight for
--   HURT_MS. At most HURT_MAX units a step.
local M = {}

M.opts = {
    judge = false, judge_back_pct = 100,
    go = false, go_pct = 130,
    counterfire = false,
    leash = false,
    siege = false,
    weak_spot = false,
    safe_post = false,
    pool = false,
    focus = false,
    hurt = false, hurt_pct = 30,
}

local JUDGE_MS, JUDGE_R, JUDGE_MAX, GO_R = 4000, 12, 8, 20
local CF_MS, CF_REACH, CF_JOIN, CF_FOE_R, CF_MAX, CF_SHOOTERS = 45000, 24, 36, 10, 60, 4
local SIEGE_MS, SIEGE_NEAR = 2000, 40
local HURT_MS, HURT_BACK, HURT_MAX = 15000, 6, 12
local WEAK_R, WEAK_MARGIN = 60, 6

local SIEGE = {gun_siege13_paladin_cannon = true, gun_siege10_howitzer_cannon = true,
               gun_siege09_serpentine_cannon = true, gun_siege07_basilisk_cannon = true, gun_siege06_trebuchet = true,
               gun_siege04_catapult = true}
M.SIEGE = SIEGE

function M.configure(opts)
    opts = opts or {}
    for _, k in ipairs(ai.keys(opts)) do
        M.opts[k] = opts[k]
    end
    return M
end

-- whether any tactic is on
function M.any()
    local o = M.opts
    return o.judge or o.go or o.counterfire or o.leash or o.siege or o.focus or o.hurt or o.weak_spot or o.safe_post or o.pool
end

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

local function st(mem)
    mem.tac = mem.tac or {cf = {}, shell = {}, hurt = {}, n = {}}
    return mem.tac
end

local function count(s, k, n)
    s.n[k] = (s.n[k] or 0) + (n or 1)
end

-- the units a tactic holds now (the groups give them no order): into lib.groups' committed set
function M.commit(mem, committed, now)
    local s = st(mem)
    for _, set in ipairs({s.cf, s.shell, s.hurt}) do
        for _, id in ipairs(ai.keys(set)) do
            if set[id] <= now then
                set[id] = nil
            else
                committed[id] = true
            end
        end
    end
end

local weak_spot

local covered_place

-- the group flags of LEASH and FOCUS, and WEAK SPOT's and SAFE POST's look at a new place (call on every group a
-- step: cheap but for a new place); home(g) -> x, y
function M.flags(g, mem, home)
    if M.opts.safe_post and g.task == "defend" and not g.manual and not g.sea and g.x >= 0
            and g.post_key ~= g.x * 10000 + g.y then
        local hx, hy = home(g)
        if hx ~= nil and hx >= 0 and (hx ~= g.x or hy ~= g.y) and covered_place(g.x, g.y) then
            print(string.format("tactics safe_post group=%d from=%d,%d home=%d,%d", g.id, g.x, g.y, hx, hy))
            g.x, g.y, g.way_t = hx, hy, 0
            count(st(mem), "safe_posts")
        end
        g.post_key = g.x * 10000 + g.y
    end
    if g.task == "attack" then
        g.leash = M.opts.leash or nil
        if M.opts.weak_spot and g.x >= 0 and g.cx ~= nil and g.weak_key ~= g.x * 10000 + g.y then
            local w = weak_spot(g.cx, g.cy, g.x, g.y)
            if w ~= nil then
                print(string.format("tactics weak_spot group=%d from=%d,%d to=%d,%d type=%s", g.id, g.x, g.y, w.x, w.y,
                    w.type))
                g.x, g.y, g.way_t = w.x, w.y, 0
                count(st(mem), "weak_spots")
            end
            g.weak_key = g.x * 10000 + g.y
        end
    end
    g.focus = M.opts.focus or nil
end

-- ---------------------------------------------------------------------------------------------------------------
-- JUDGE

-- [armies.c] the strength of the enemy the memory knows within r of a tile: hit points x attack of the fighters and
-- the defences (citizens and boats count nothing)
local function enemy_at(x, y, r)
    local str = 0
    for _, m in ipairs(ai.memory{near = {x, y, r}}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            str = str + m.hp * c.attack
        end
    end
    return str
end

-- groups: the script's lib.groups groups; home(g) -> x, y; back(g, x, y, why): the script sends it home. Returns the
-- groups sent home
function M.judge(mem, groups, now, home, back)
    if not M.opts.judge and not M.opts.go then
        return 0
    end
    local s = st(mem)
    local n, sent = 0, 0
    for _, g in ipairs(groups) do
        if n >= JUDGE_MAX then
            break
        end
        if M.opts.go and g.task == "attack" and not g.manual and #g.units > 0 and g.x >= 0 and g.strength ~= nil
                and g.go_key ~= g.x * 10000 + g.y then
            n = n + 1
            g.go_key = g.x * 10000 + g.y
            local foe = enemy_at(g.x, g.y, GO_R)
            local own = g.strength or 0
            local hx, hy = home(g)
            if foe > 0 and own * 100 < foe * M.opts.go_pct and hx ~= nil and hx >= 0 then
                count(s, "go_off")
                sent = sent + 1
                print(string.format("tactics go off id=%d units=%d own=%d foe=%d at=%d,%d", g.id, #g.units, own, foe,
                    g.x, g.y))
                back(g, hx, hy, "too_strong")
            else
                count(s, "go_on")
            end
        elseif M.opts.judge and g.task == "attack" and not g.manual and #g.units > 0 and g.cx ~= nil and now - (g.judge_t or 0) >= JUDGE_MS then
            n = n + 1
            g.judge_t = now
            local foe = enemy_at(g.cx, g.cy, JUDGE_R)
            if g.x >= 0 then
                foe = math.max(foe, enemy_at(g.x, g.y, JUDGE_R))
            end
            local own = g.strength or 0
            if foe > 0 and own * 100 < foe * M.opts.judge_back_pct then
                g.losing = (g.losing or 0) + 1
                if g.losing >= 2 then
                    local hx, hy = home(g)
                    if hx ~= nil and hx >= 0 then
                        g.losing = 0
                        count(s, "judge_back")
                        sent = sent + 1
                        print(string.format("tactics judge back id=%d units=%d own=%d foe=%d at=%d,%d", g.id, #g.units,
                            own, foe, g.cx, g.cy))
                        back(g, hx, hy, "losing")
                    end
                end
            else
                g.losing = 0
            end
        end
    end
    return sent
end

-- ---------------------------------------------------------------------------------------------------------------
-- COUNTERFIRE (the micro step: the attacked events since the last call)

function M.counterfire(mem, events, now)
    if not M.opts.counterfire then
        return
    end
    local s = st(mem)
    local cf = s.cf
    local shooters, order = {}, {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil and shooters[e.attacker] == nil and #order < 16 then
            local a = ai.unit(e.attacker)
            local v = ai.unit(e.unit)
            if a ~= nil and v ~= nil and (a.range or 0) >= 500 and a.category ~= "building" and v.category ~= "resource" then
                local reach = (v.range or 0) // 100 + 1
                if d2(a.x, a.y, v.x, v.y) > reach * reach then
                    shooters[a.id] = a
                    order[#order + 1] = a.id
                end
            end
        end
    end
    if #order == 0 then
        return
    end
    local done, looked = {}, 0
    for _, id in ipairs(order) do
        if not done[id] and looked < CF_SHOOTERS then
            looked = looked + 1
            local e = shooters[id]
            local cluster = {}
            for _, id2 in ipairs(order) do
                if not done[id2] and d2(shooters[id2].x, shooters[id2].y, e.x, e.y) <= CF_JOIN then
                    done[id2] = true
                    cluster[#cluster + 1] = id2
                end
            end
            local foes = ai.seen{enemy = true, armed = true, near = {e.x, e.y, CF_FOE_R}}
            local foe = #foes > 0 and ai.strength(foes) or 0
            local own = ai.units{category = "soldier", near = {e.x, e.y, CF_REACH}, limit = 80}
            local land = ai.tile(e.x, e.y)
            land = land and land.continent or 0
            local cands, str, on = {}, 0, 0
            local cols = #own > 0 and ai.get(own, {"hp", "attack", "inside", "x", "y", "target"}) or {}
            for k = 1, #own do
                if cols.hp[k] ~= nil and cols.inside[k] == nil then
                    local hits = false
                    for _, c in ipairs(cluster) do
                        if cols.target[k] == c then
                            hits = true
                        end
                    end
                    if hits then
                        str = str + cols.hp[k] * (cols.attack[k] or 0)
                        on = on + 1
                    elseif cf[own[k]] == nil and (cols.attack[k] or 0) > 0 then
                        local t = ai.tile(cols.x[k], cols.y[k])
                        if t ~= nil and (land == 0 or t.continent == land) then
                            cands[#cands + 1] = {id = own[k], s = cols.hp[k] * cols.attack[k]}
                        end
                    end
                end
            end
            local take = 0
            while str * 2 < foe * 3 and take < #cands and on + take < CF_MAX do
                take = take + 1
                str = str + cands[take].s
            end
            if str * 2 >= foe * 3 and take > 0 then
                local by = {}
                for k = 1, take do
                    local c = cluster[(k - 1) % #cluster + 1]
                    by[c] = by[c] or {}
                    by[c][#by[c] + 1] = cands[k].id
                    cf[cands[k].id] = now + CF_MS
                end
                for _, c in ipairs(ai.keys(by)) do
                    ai.attack(by[c], c)
                end
                count(s, "cf_sorties")
                count(s, "cf_sent", take)
                if (s.n.cf_sorties or 0) <= 20 then
                    print(string.format("tactics counterfire shooters=%d at=%d,%d foe=%d sent=%d own=%d", #cluster, e.x,
                        e.y, foe, take, str))
                end
            else
                count(s, "cf_short")
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- SIEGE

local function is_wall(t)
    local p = string.sub(t or "", 1, 7)
    return p == "b_wall_" or p == "b_gate_"
end

-- [attack.c] the known defences: the memory's armed buildings with a ground attack, range over 1 tile
local function defences(x, y, r)
    local out = {}
    for _, m in ipairs(ai.memory{buildings = true, near = {x, y, r}}) do
        if not is_wall(m.type) and string.sub(m.type, 1, 2) ~= "aa" then
            local c = ai.class(m.type)
            local rg = c ~= nil and (c.range or 0) or 0
            if c ~= nil and (c.attack or 0) > 0 and rg > 100 and rg <= 3000 then
                out[#out + 1] = {id = m.id, x = m.x, y = m.y, r = rg, type = m.type}
            end
        end
    end
    return out
end

local function covered(defs, x, y, ex, ey)
    for _, d in ipairs(defs) do
        if d.x ~= ex or d.y ~= ey then
            local r = d.r / 100 + 1.5
            local dx, dy = x - d.x, y - d.y
            if dx * dx + dy * dy <= r * r then
                return true
            end
        end
    end
    return false
end

-- [attack.c] the known defences covering a tile (their range + WEAK_MARGIN)
local function threat_at(defs, x, y)
    for _, d in ipairs(defs) do
        local r = d.r // 100 + WEAK_MARGIN
        if d2(x, y, d.x, d.y) <= r * r then
            return true
        end
    end
    return false
end

-- [attack.c attack_weak_spots] the uncovered known enemy building for a place a defence covers, or nil
weak_spot = function(sx, sy, dx, dy)
    local defs = defences(dx, dy, WEAK_R + 30)
    if not threat_at(defs, dx, dy) then
        return nil
    end
    local list = ai.memory{buildings = true, near = {dx, dy, WEAK_R}}
    local at = false
    for _, m in ipairs(list) do
        if not is_wall(m.type) and d2(m.x, m.y, dx, dy) <= 16 then
            at = true
            break
        end
    end
    if not at then
        return nil
    end
    local best, best_score = nil, nil
    for _, m in ipairs(list) do
        if not is_wall(m.type) and not threat_at(defs, m.x, m.y) and ai.reach(sx, sy, m.x, m.y) then
            local score = math.sqrt(d2(m.x, m.y, sx, sy)) + 0.5 * math.sqrt(d2(m.x, m.y, dx, dy))
            if best_score == nil or score < best_score then
                best, best_score = m, score
            end
        end
    end
    return best
end

covered_place = function(x, y)
    return threat_at(defences(x, y, 30), x, y)
end

local COS15 = {1.0, 0.96592583, 0.86602540, 0.70710678, 0.5, 0.25881905, 0.0}
local SIN15 = {0.0, 0.25881905, 0.5, 0.70710678, 0.86602540, 0.96592583, 1.0}

-- [attack.c siege_tick] the target (the nearest known defence the siege outranges) and the firing position
local function shelling(siege, near_x, near_y)
    local sx, sy, srange = 0, 0, 9900
    for _, u in ipairs(siege) do
        sx, sy = sx + u.x, sy + u.y
        if (u.range or 0) > 0 and u.range < srange then
            srange = u.range
        end
    end
    sx, sy = sx / #siege, sy / #siege
    local defs = defences(near_x, near_y, SIEGE_NEAR + 15)
    local t, best = nil, nil
    for _, d in ipairs(defs) do
        if d.r + 50 < srange and d2(d.x, d.y, near_x, near_y) <= SIEGE_NEAR * SIEGE_NEAR then
            local dd = d2(d.x, d.y, sx, sy)
            if best == nil or dd < best then
                best, t = dd, d
            end
        end
    end
    if t == nil then
        return nil
    end
    local ux, uy = sx - t.x, sy - t.y
    local ul = math.sqrt(ux * ux + uy * uy)
    if ul > 0 then
        ux, uy = ux / ul, uy / ul
    else
        ux, uy = 1, 0
    end
    local r = srange / 100 - 0.7
    for k = 0, 12 do
        local i = (k + 1) // 2 + 1
        local c = COS15[i]
        local sn = (k % 2 == 1) and SIN15[i] or -SIN15[i]
        local px = t.x + 0.5 + r * (ux * c - uy * sn)
        local py = t.y + 0.5 + r * (ux * sn + uy * c)
        if not covered(defs, px, py, t.x, t.y) then
            return t, math.floor(px), math.floor(py)
        end
    end
    return nil
end

function M.siege(mem, groups, now)
    if not M.opts.siege then
        return
    end
    local s = st(mem)
    for _, g in ipairs(groups) do
        if g.task == "attack" and #g.units > 0 and g.cx ~= nil and now - (g.shell_t or 0) >= SIEGE_MS then
            g.shell_t = now
            local siege = {}
            for _, id in ipairs(g.units) do
                local u = ai.unit(id)
                if u ~= nil and SIEGE[u.type] and u.inside == nil then
                    siege[#siege + 1] = u
                end
            end
            if #siege > 0 then
                local t, px, py = shelling(siege, g.cx, g.cy)
                if t ~= nil and #ai.seen{enemy = true, category = "soldier", near = {px, py, 8}, limit = 1} == 0 then
                    local fire, go = {}, {}
                    for _, u in ipairs(siege) do
                        if d2(u.x, u.y, px, py) <= 4 then
                            if u.target ~= t.id then
                                fire[#fire + 1] = u.id
                            end
                        elseif not (u.action == "move" and u.tx ~= nil and math.abs(u.tx - px) < 2
                                    and math.abs(u.ty - py) < 2) then
                            go[#go + 1] = u.id
                        end
                        s.shell[u.id] = now + SIEGE_MS * 3
                    end
                    if #fire > 0 then
                        ai.attack(fire, t.id)
                    end
                    if #go > 0 then
                        ai.move(go, px, py, {forced = true})
                    end
                    if t.id ~= g.shell_target then
                        g.shell_target = t.id
                        count(s, "shells")
                        print(string.format("tactics siege group=%d target=%s at=%d,%d from=%d,%d siege=%d", g.id, t.type,
                            t.x, t.y, px, py, #siege))
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- HURT

function M.hurt(mem, groups, now, home)
    if not M.opts.hurt then
        return
    end
    local s = st(mem)
    local n = 0
    for _, g in ipairs(groups) do
        if g.state == "fighting" and not g.manual and not g.sea and #g.units > 1 then
            local cols = ai.get(g.units, {"hp", "hp_max", "x", "y"})
            local hx, hy = home(g)
            for k, id in ipairs(g.units) do
                if n >= HURT_MAX or hx == nil or hx < 0 then
                    break
                end
                local hp, mx = cols.hp[k], cols.hp_max[k]
                if hp ~= nil and mx ~= nil and mx > 0 and hp * 100 < mx * M.opts.hurt_pct and s.hurt[id] == nil then
                    local x, y = cols.x[k], cols.y[k]
                    local dx, dy = hx - x, hy - y
                    local l = math.max(math.abs(dx), math.abs(dy), 1)
                    local bx, by = x + dx * HURT_BACK // l, y + dy * HURT_BACK // l
                    if l <= HURT_BACK then
                        bx, by = hx, hy
                    end
                    ai.move({id}, bx, by, {forced = true})
                    s.hurt[id] = now + HURT_MS
                    n = n + 1
                    count(s, "hurt_back")
                end
            end
        end
    end
end

-- the counts a minute (the trace)
function M.report(mem, now)
    local s = mem.tac
    if s == nil or now % 60000 >= 1000 then
        return
    end
    local keys = ai.keys(s.n)
    if #keys == 0 then
        return
    end
    local parts = {}
    for _, k in ipairs(keys) do
        parts[#parts + 1] = k .. "=" .. s.n[k]
    end
    print("tactics " .. table.concat(parts, " "))
end

return M
