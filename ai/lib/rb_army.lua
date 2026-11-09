-- The army of the rush and balanced scripts (docs/AI_LUA_PLAN.md §5 "S4 as built"): training, a scout, the home
-- guard, attacks, raids and retreats over the group verbs. The numbers and the rules are the caller's (cfg).
-- require("lib.rb_army").
--
-- Roles (memory.groups, lib.army): "scout" explores; "home" defends the town (radius: the claim range) and holds the
-- soldiers not out; "main" attacks the target's town; "raid" hunts its gatherers. A group that comes back from an
-- attack or a raid defends the town, and is folded into the home guard.
--
-- cfg:
--   producers[age]          the buildings that train soldiers (ids)
--   army(view, memory)      the soldiers wanted now (the training stops there)
--   queue                   requests per producer kept waiting at most (default 2)
--   scout_at, rescout_ms    the first scout's time; a new scout when the target was not seen for this long
--   wave(view, memory, e)   the soldiers an attack needs now against enemy e (nil: no attack now)
--   home_keep               soldiers a wave leaves at home (default 0)
--   raid_size, raid_every   a raid's soldiers, and the least time between raids (nil: no raids)
--   raid_ok(view, memory, e)  whether a raid may go now (default: always)
--   retreat_ratio           a group out retreats when the seen enemy near it is this many times stronger (x100)
--   recall_ratio            the groups out come home when the threat at the town is this many times the home guard
--   punish_epoch            a wave when the target reaches a new epoch (its resources went into the advance)
-- eco (lib.rb_economy's step): {saving =, short =}: a town short of citizens trains soldiers only from a surplus
local V = require("lib.view")
local A = require("lib.army")
local E = require("lib.rb_economy")
local M = {}

-- train soldiers at every producer up to the army wanted
local function train(view, memory, cfg, order, eco)
    local age = E.age(view)
    local wanted = cfg.army(view, memory)
    local have = view.army_count + A.coming(view)
    if have >= wanted or view.population >= view.population_cap then
        return
    end
    -- citizens first: a town short of them keeps a citizen's food (two with the epoch saved for) over the cost
    local keep = {}
    local citizen = V.cost(view, "citizen")
    if eco ~= nil and eco.short and citizen ~= nil then
        for _, r in ipairs(V.resources) do
            keep[r] = citizen.cost[r] * 2
        end
    end
    memory.flip = (memory.flip or 0) + 1
    for _, producer in ipairs(cfg.producers[age] or {}) do
        local b = V.buildings(view, producer)
        if b ~= nil and b.finished > 0 then
            local classes = A.classes_of(view, producer)
            -- the two dearest classes in turn (a mix of the lines)
            local pick = classes[1]
            if #classes >= 2 and memory.flip % 3 == 0 then
                pick = classes[2]
            end
            local spare = true
            if pick ~= nil then
                for _, r in ipairs(V.resources) do
                    spare = spare and view.stock[r] - view.reserve[r] >= pick.cost[r] + (keep[r] or 0)
                end
            end
            if pick ~= nil and pick.requested + pick.in_production < (cfg.queue or 2) * b.finished
                    and (eco == nil or not eco.short or spare or have < wanted // 2) then
                order({verb = "train", type = pick.type, n = 1})
                have = have + 1
                if have >= wanted then
                    return
                end
            end
        end
    end
end

local function far(view, g)
    local dx, dy = g.x - view.town_x, g.y - view.town_y
    return dx * dx + dy * dy > view.claim_range * view.claim_range
end

-- the army's orders for this step; returns {target =} (the enemy it fights, nil: none known)
function M.step(view, memory, cfg, orders, eco)
    local function order(o)
        orders[#orders + 1] = o
    end
    A.collect(view, memory)
    memory.army = memory.army or {}
    local mem = memory.army
    local now = view.time
    local target = A.pick_enemy(view, memory)
    local home = A.role(view, memory, "home")
    local scout = A.role(view, memory, "scout")

    train(view, memory, cfg, order, eco)

    -- groups that came home (a retreat ends as a defence of the town) join the home guard
    for _, role in ipairs({"main", "raid"}) do
        local g = A.role(view, memory, role)
        if g ~= nil and g.task == "defend" then
            if role == "main" and g.lost * 2 > g.started then
                mem.failed = (mem.failed or 0) + 1
            end
            memory.groups[role] = nil
            if home == nil then
                memory.groups.home = g.id
                home = g
            else
                order({verb = "disband", group = g.id})   -- its soldiers are free again: the home guard takes them
            end
        end
    end
    local main = A.role(view, memory, "main")
    local raid = A.role(view, memory, "raid")
    if main ~= nil then
        mem.main_id = main.id
    end
    -- a wave that died out counts as failed
    if mem.main_id ~= nil and main == nil and (home == nil or home.id ~= mem.main_id) then
        if A.group(view, mem.main_id) == nil then
            mem.failed = (mem.failed or 0) + 1
        end
        mem.main_id = nil
    end

    -- the scout: one soldier, early, and again when the target has not been seen for a while
    if scout ~= nil and scout.state == "done" then
        order({verb = "disband", group = scout.id})
    end
    local stale = target == nil or now - target.last_seen > (cfg.rescout_ms or 600000)
    if scout == nil and now >= (cfg.scout_at or 120000) and stale and now - (mem.scout_time or -1000000) >= 120000 then
        if view.free_soldiers > 0 then
            order({verb = "scout", n = 1})
            A.track(memory, orders, "scout")
            mem.scout_time = now
        elseif home ~= nil and home.units > 1 then
            order({verb = "scout", n = 1, from = home.id})
            A.track(memory, orders, "scout")
            mem.scout_time = now
        end
    end

    -- the threat at the town: the home guard fights it; the groups out come home when it is too strong
    local threat = view.threat_strength
    local home_strength = (home and home.strength or 0) + view.free_strength
    if threat > 0 and threat * 100 > home_strength * (cfg.recall_ratio or 150) then
        for _, g in ipairs({main, raid}) do
            if g ~= nil and g.task ~= "retreat" and g.task ~= "defend" then
                order({verb = "retreat", group = g.id})
            end
        end
    end

    -- a group out that meets a stronger enemy comes home
    for _, g in ipairs({main, raid}) do
        if g ~= nil and (g.task == "attack" or g.task == "raid") and far(view, g)
                and g.enemy_strength * 100 > g.strength * (cfg.retreat_ratio or 160) then
            order({verb = "retreat", group = g.id})
        end
    end

    -- the target's epoch: a wave as it advances
    if target ~= nil then
        mem.epochs = mem.epochs or {}
        local key = tostring(target.player)
        local before = mem.epochs[key]
        mem.epochs[key] = target.epoch
        if cfg.punish_epoch and before ~= nil and target.epoch > before then
            mem.punish = now
        end
    end

    -- the free soldiers join the home guard first (the waves and raids below take from it)
    if view.free_soldiers > 0 then
        if home ~= nil then
            order({verb = "defend", group = home.id, n = view.free_soldiers})
        else
            order({verb = "defend", n = view.free_soldiers})
            A.track(memory, orders, "home")
            return {target = target}   -- (its id comes with the next step's results)
        end
    end
    if home == nil then
        return {target = target}
    end
    local ready = home.units + view.free_soldiers

    -- a wave at the target's town: not while the town itself is threatened
    if target ~= nil and target.town_x >= 0 and threat == 0 then
        if main == nil then
            local need = cfg.wave(view, memory, target)
            local punish = mem.punish ~= nil and now - mem.punish < 120000
            local keep = cfg.home_keep or 0
            if ((need ~= nil and ready >= need) or (punish and ready >= 4 + keep)) and ready > keep then
                order({verb = "attack", n = ready - keep, from = home.id, player = target.player})
                A.track(memory, orders, "main")
                mem.waves = (mem.waves or 0) + 1
                mem.punish = nil
                mem.wave_time = now
                return {target = target}
            end
        elseif main.task == "attack" and ready - (cfg.home_keep or 0) >= 3 then
            -- reinforcements
            order({verb = "attack", group = main.id, n = ready - (cfg.home_keep or 0), from = home.id, player = target.player})
            return {target = target}
        end
    end

    -- raids on the target's gatherers
    if cfg.raid_size ~= nil and target ~= nil and raid == nil and target.citizen_x >= 0 and threat == 0
            and now - (mem.raid_time or -10000000) >= (cfg.raid_every or 300000)
            and (cfg.raid_ok == nil or cfg.raid_ok(view, memory, target)) and ready >= cfg.raid_size then
        order({verb = "raid", n = cfg.raid_size, from = home.id, player = target.player})
        A.track(memory, orders, "raid")
        mem.raid_time = now
    end
    return {target = target}
end

return M
