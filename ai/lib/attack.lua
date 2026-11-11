-- Attacks from strength, over the per-unit interface and lib.groups (docs/AI_LUA_UNITS.md §3; docs/AI_RESEARCH.md
-- §8.41). require("lib.attack"). NOT WIRED YET: no script calls it.
-- (The next round wires it into boom and benches it.) Generic: any script may use it; the numbers are options.
--
--   A.step(view, mem, opts)   every strategy step. opts: guard (the group the force comes from), keep (soldiers the
--                             guard keeps at home), enemy (the target's player; default the nearest known town)
--
-- The rules and where they come from:
-- - [owner, research §4.4] no attack before `from_epoch` (the Copper Age).
-- - [research §4.4] the windows: within `window_ms` of the own epoch advance (new units), or of a failed enemy
--   attack (a raid of `repel_n`+ soldiers in the town's zone gone while the guard kept most of its strength).
-- - [research §4.4, ageofai attack.lua's 130%] go at `go_odds` (1.3 x) of what is known at the target (lib.scout's
--   intel: the soldiers last seen and the towers by its town), retreat below `retreat_odds` (0.8 x) of what the force
--   sees round it; between the two it keeps on (hysteresis).
-- - [research §4.4] the target with more than `siege_towers` (6) towers and no siege in the force is raided (its
--   gatherers away from the towers: lib.groups' raid), not attacked.
-- - [lib.groups] the force fights as an attack or raid group: targets by priority, melee never at ships, the raid's
--   flight at 1.5 x.
local G = require("lib.groups")
local SC = require("lib.scout")

local M = {}

M.opts = {
    from_epoch = 2,        -- the Copper Age
    window_ms = 300000,    -- a window lasts this long
    repel_n = 5,           -- a raid this large gone from the zone ...
    repel_keep = 70,       -- ... while the guard kept this share of its strength: a failed attack
    go_odds = 130,         -- percent of the known enemy at the target
    retreat_odds = 80,     -- percent of the enemy seen round the force
    siege_towers = 6,
    min_force = 8,         -- soldiers a force takes at least
    tower_hp = 4,          -- a known tower counts its hit points / this ...
    tower_atk = 2,         -- ... and its attack / this in the estimate
    cooldown_ms = 180000,  -- after a retreat, no new force for this long
}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

-- the Lanchester strength (summed hit points x summed attack) of own ids
local function strength(ids)
    if #ids == 0 then
        return 0, 0, 0
    end
    local c = ai.get(ids, {"hp", "attack"})
    local hp, atk = 0, 0
    for k = 1, #ids do
        hp, atk = hp + (c.hp[k] or 0), atk + (c.attack[k] or 0)
    end
    return hp * atk, hp, atk
end

-- the nearest enemy with a known town
local function target(view, player)
    local best, bd = nil, nil
    for _, e in ipairs(view.enemies or {}) do
        if e.town_x ~= nil and e.town_x >= 0 and (player == nil or e.player == player) then
            local d = d2(e.town_x, e.town_y, view.town_x, view.town_y)
            if bd == nil or d < bd then
                best, bd = e, d
            end
        end
    end
    return best
end

-- the windows: the own epoch advance; a failed enemy attack (lib.defence's raids in the zone)
local function windows(view, mem, guard)
    local o = M.opts
    local now = view.time
    if mem.att_epoch ~= view.epoch then
        mem.att_epoch = view.epoch
        if view.epoch >= o.from_epoch then
            mem.att_window = now + o.window_ms
            print(string.format("attack window epoch=%d until=%d", view.epoch, mem.att_window // 1000))
        end
    end
    -- a raid of repel_n+ in the zone: its size and the guard's strength then; gone with the guard mostly whole: repelled
    local D = require("lib.defence")
    local big = 0
    for _, cl in ipairs(D.clusters()) do
        if d2(cl.x, cl.y, view.town_x, view.town_y) <= (view.claim_range + 5) * (view.claim_range + 5) then
            big = math.max(big, cl.n)
        end
    end
    local gs = guard ~= nil and strength(guard.units) or 0
    if big >= o.repel_n then
        if mem.att_raid == nil then
            mem.att_raid = {t = now, gs = gs}
        end
        mem.att_raid.last = now
    elseif mem.att_raid ~= nil and now - (mem.att_raid.last or now) >= 30000 then
        if gs * 100 >= mem.att_raid.gs * o.repel_keep and view.epoch >= o.from_epoch then
            mem.att_window = now + o.window_ms
            print(string.format("attack window repelled guard=%d/%d", gs, mem.att_raid.gs))
        end
        mem.att_raid = nil
    end
    return now <= (mem.att_window or -1)
end

function M.step(view, mem, opts)
    opts = opts or {}
    local o = M.opts
    local now = view.time
    if view.town_x < 0 then
        return
    end
    local guard = opts.guard
    local open = windows(view, mem, guard)
    local force = G.role(mem, "force")
    if force ~= nil then
        -- judge the force: what it sees round it (soldiers and towers) against itself; retreat below retreat_odds
        if force.task ~= "retreat" and force.cx ~= nil then
            local foes = ai.seen{enemy = true, armed = true, near = {force.cx, force.cy, 12}}
            local ehp, eatk = 0, 0
            if #foes > 0 then
                local c = ai.get(foes, {"hp", "attack", "category"})
                for k = 1, #foes do
                    if c.category[k] == "building" then
                        ehp, eatk = ehp + (c.hp[k] or 0) // o.tower_hp, eatk + (c.attack[k] or 0) // o.tower_atk
                    elseif c.category[k] == "soldier" then
                        ehp, eatk = ehp + (c.hp[k] or 0), eatk + (c.attack[k] or 0)
                    end
                end
            end
            local own = strength(force.units)
            if ehp * eatk > 0 and own * 100 < ehp * eatk * o.retreat_odds then
                print(string.format("attack retreat units=%d own=%d enemy=%d", #force.units, own, ehp * eatk))
                G.retreat(view, mem, force)
                mem.att_cool = now + o.cooldown_ms
                mem.att_window = nil
            end
        elseif force.task == "retreat" then
            -- home again: back into the guard
        end
        if force.task == "defend" then   -- (lib.groups: a retreat that arrived defends) back to the guard
            if guard ~= nil then
                G.add(mem, guard, force.units)
            end
            G.disband(mem, force)
        end
        return
    end
    if not open or now < (mem.att_cool or 0) or guard == nil then
        return
    end
    local e = target(view, opts.enemy)
    if e == nil then
        return
    end
    local keep = opts.keep or 8
    local n = #guard.units - keep
    if n < o.min_force then
        return
    end
    -- the estimate at the target: the soldiers known (lib.scout) and its towers
    local i = SC.intel(view, mem, e.player)
    local towers = i.towers
    local ehp = i.hp + towers * 1000 // o.tower_hp
    local eatk = i.atk + towers * 50 // o.tower_atk
    local ids = {}
    for k = 1, n do
        ids[k] = guard.units[#guard.units - n + k]
    end
    local own, ohp, oatk = strength(ids)
    if own * 100 < ehp * eatk * o.go_odds then
        return
    end
    local raid = towers > o.siege_towers
    local g = G.create(mem, {task = raid and "raid" or "attack", role = "force", target = e.player,
                             x = e.town_x, y = e.town_y, home_x = view.town_x, home_y = view.town_y})
    G.add(mem, g, ids)
    g.leash = true
    mem.att_sent = (mem.att_sent or 0) + 1
    print(string.format("attack go task=%s units=%d own=%d enemy=%d soldiers=%d towers=%d", g.task, #ids, own,
        ehp * eatk, i.soldiers, towers))
end

return M
