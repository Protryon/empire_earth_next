-- The lost mines (boom-econ, docs/AI_RESEARCH.md §8.51; the owner, 2026-11-14): iron is the most important metal, and a
-- town with fewer than 2 working iron sites or fewer than 2 working gold sites loses. In his playtest he took Boom's
-- third iron mine in the Stone Age (spearmen on the miners, then a tower, a settlement and a barracks by it) and Boom
-- never tried to take it back; its Copper save starved on iron for 40 minutes.
--
--   M.step(view, mem, sites)   once a step: the lost sites, the working counts, the operation under way
--   M.urgent(mem)              true while a resource is under 2 working sites and a lost one can be retaken: the
--                              army spends the stockpile (boom: the soldiers wanted at the most, the reserve gives way)
--   M.replace(mem, r)          true while resource r needs a replacement site (claimed further out, at once)
--
-- The rules:
-- - [owner] a site of gold or iron the town held (its drop site touched it) that an enemy now blocks (enemy soldiers
--   near it, or an enemy tower) is lost; a lost site is retaken, then the claims and lib.place settle it again;
-- - [owner] under 2 working sites of a resource: everything goes, the standing army included, with the stockpile
--   spent on soldiers at once (M.urgent); above that, a proportionate force: the free soldiers, when they are at
--   least NEED_BASE + NEED_UNIT a enemy soldier + NEED_TOWER an enemy tower;
-- - a site that cannot be retaken (no force, or the operation failed) gets a replacement claimed at once;
-- - every trigger, operation and outcome is printed ("recover ...") for the benches.
local G = require("lib.groups")

local M = {}

M.NEED_BASE, M.NEED_UNIT, M.NEED_TOWER = 4, 2, 6
M.CLEAR_R = 9          -- tiles round the site the enemy is looked for
M.CLEAR_MS = 20000     -- no enemy near it for this long: cleared
M.OP_MS = 360000       -- an operation gives up after this long ...
M.OP_MIN = 3           -- ... or with fewer soldiers than this left
M.ORDER_MS = 5000      -- the attack is given again this often
M.URGENT_MIN = 2       -- working sites of a resource under this: existential
M.parity = false       -- (BO.OPT.metal_parity, the owner: match or exceed the enemy's gathering) behind the enemy's known
                       -- metal sites, a loss is existential, and without one the army takes an enemy or contested site
M.PARITY_MS = 10000    -- the enemy's metal sites are counted this often
M.PARITY_REACH = 45    -- tiles from home a site to take may lie
local DROPS = {b_settlement = true, b_town_center = true, b_capitol = true}

local MINES = {"gold", "iron"}

local function d2(ax, ay, bx, by)
    return (ax - bx) * (ax - bx) + (ay - by) * (ay - by)
end

local function state(mem)
    mem.recover = mem.recover or {held = {}, lost = {}, replace = {}, working = {gold = 0, iron = 0}}
    return mem.recover
end

-- (M.parity) the metal sites an enemy holds, as far as the town has seen: a remembered enemy drop site or armed
-- building within 4 tiles of the site
local function enemy_held(s)
    for _, m in ipairs(ai.memory{enemy = true, buildings = true, near = {s.x, s.y, 4}, limit = 4}) do
        local c = ai.class(m.type) or {}
        if DROPS[m.type] or (c.attack or 0) > 0 then
            return true
        end
    end
    return false
end

-- the enemy at a spot: the soldiers seen now, the buildings (towers) remembered
local function enemy_at(x, y)
    local units, towers = {}, {}
    for _, id in ipairs(ai.seen{enemy = true, armed = true, near = {x, y, M.CLEAR_R}, limit = 24}) do
        local u = ai.unit(id)
        if u ~= nil and (u.speed or 0) > 0 then
            units[#units + 1] = id
        end
    end
    for _, m in ipairs(ai.memory{enemy = true, buildings = true, near = {x, y, M.CLEAR_R}, limit = 8}) do
        local c = ai.class(m.type) or {}
        if (c.attack or 0) > 0 then
            towers[#towers + 1] = m.id
        end
    end
    return units, towers
end

local function all_soldiers(mem)
    local ids = G.free_soldiers()
    for _, g in ipairs(G.all(mem)) do
        if g.task ~= "recover" and g.task ~= "navy" then
            for _, id in ipairs(g.units) do
                ids[#ids + 1] = id
            end
        end
    end
    return ids
end

function M.urgent(mem)
    return state(mem).urgent == true
end

function M.replace(mem, r)
    return state(mem).replace[r] == true
end

function M.step(view, mem, sites)
    local S = state(mem)
    local now = view.time
    -- the working sites, and the ones lost since the last step
    local working = {gold = 0, iron = 0}
    for _, s in ipairs(sites) do
        for _, r in ipairs(MINES) do
            if (s.holds[r] or 0) > 0 then
                local blocked = s.blocked == "enemy_near" or s.blocked == "tower"
                if s.claimed and (s.drop[r] or 0) > 0 and not blocked then
                    working[r] = working[r] + 1
                    S.held[s.index] = r
                    if S.lost[s.index] ~= nil then   -- (settled again)
                        print(string.format("recover settled r=%s site=%d", r, s.index))
                        S.lost[s.index] = nil
                    end
                elseif S.lost[s.index] ~= nil and S.op == nil and not blocked and not S.lost[s.index].take then
                    S.lost[s.index] = nil   -- (the enemy left it: the claims take it again)
                elseif S.held[s.index] == r and blocked and S.lost[s.index] == nil then
                    S.lost[s.index] = {r = r, x = s.x, y = s.y, t = now}
                    S.held[s.index] = nil
                    print(string.format("recover lost r=%s site=%d at=%d,%d why=%s working=%d", r, s.index, s.x, s.y,
                        s.blocked, working[r]))
                end
            end
        end
    end
    S.working = working
    -- (M.parity) the enemy's metal sites, and whether the town is behind on them
    if M.parity and now - (S.parity_t or -M.PARITY_MS) >= M.PARITY_MS then
        S.parity_t = now
        local theirs, targets = {gold = 0, iron = 0}, {}
        for _, s in ipairs(sites) do
            for _, r in ipairs(MINES) do
                if (s.holds[r] or 0) > 0 and not (s.claimed and (s.drop[r] or 0) > 0) and s.distance ~= nil
                        and s.distance <= M.PARITY_REACH then
                    local held = enemy_held(s)
                    if held then
                        theirs[r] = theirs[r] + 1
                    end
                    if held or s.blocked == "enemy_near" or s.blocked == "tower" then
                        targets[#targets + 1] = {idx = s.index, r = r, x = s.x, y = s.y, d = s.distance}
                    end
                end
            end
        end
        S.theirs, S.targets = theirs, targets
        local behind = {}
        for _, r in ipairs(MINES) do   -- (once the town has had its 2 sites: a fresh start has none settled yet)
            behind[r] = working[r] < theirs[r] and (S.peak or {})[r] ~= nil and S.peak[r] >= M.URGENT_MIN
        end
        S.behind = behind
    end
    -- (the counts matter once the town has had 2 working sites of a resource: a fresh Copper start has none yet)
    S.peak = S.peak or {gold = 0, iron = 0}
    for _, r in ipairs(MINES) do
        S.peak[r] = math.max(S.peak[r], working[r])
    end
    -- a replacement is wanted while a lost site of the resource waits unretaken (given up, or no force), or while the
    -- town is under 2 working sites with no lost one to retake
    local short = {}
    for _, idx in ipairs(ai.keys(S.lost)) do
        local l = S.lost[idx]
        if l.given_up or l.no_force then
            short[l.r] = true
        end
    end
    for _, r in ipairs(MINES) do
        local want = short[r] == true or (M.parity and S.behind ~= nil and S.behind[r] == true)
        if not want and working[r] < M.URGENT_MIN and S.peak[r] >= M.URGENT_MIN then
            local any = false
            for _, idx in ipairs(ai.keys(S.lost)) do
                any = any or S.lost[idx].r == r
            end
            want = not any
        end
        if want and not S.replace[r] then
            print(string.format("recover replace r=%s working=%d", r, working[r]))
        end
        S.replace[r] = want or nil
    end
    -- the operation under way
    local op = S.op
    if op ~= nil then
        local g = G.get(mem, op.group)
        local units, towers = enemy_at(op.x, op.y)
        if #units > 0 or #towers > 0 then
            op.last_enemy = now
        end
        local n = g ~= nil and #g.units or 0
        local result = nil
        if now - (op.last_enemy or op.t) >= M.CLEAR_MS then
            result = "cleared"
        elseif now - op.t >= M.OP_MS or n < M.OP_MIN then
            result = "failed"
        end
        if result ~= nil then
            print(string.format("recover done r=%s site=%d result=%s left=%d of %d ms=%d", op.r, op.site, result, n,
                op.n, now - op.t))
            if g ~= nil then
                G.disband(mem, g)
            end
            if result == "cleared" then
                S.lost[op.site] = nil   -- (the claims and lib.place take it again)
            elseif S.lost[op.site] ~= nil then   -- (nil: settled again while the operation ran)
                S.lost[op.site].given_up = true
                S.tried = S.tried or {}
                S.tried[op.site] = true
            end
            S.op, S.urgent = nil, nil
        elseif g ~= nil and now - (op.ordered or 0) >= M.ORDER_MS then
            op.ordered = now
            local target = units[1] or towers[1]
            if target ~= nil then
                ai.attack(g.units, target)
            else
                ai.move(g.units, op.x, op.y)
            end
            for _, id in ipairs(g.units) do
                G.committed[id] = true
            end
        end
        return
    end
    -- a lost site to retake: the most needed resource first (iron before gold, fewer working first)
    local best = nil
    for _, idx in ipairs(ai.keys(S.lost)) do
        local l = S.lost[idx]
        if not l.given_up and (best == nil or working[l.r] < working[best.r]
                or (working[l.r] == working[best.r] and l.r == "iron" and best.r ~= "iron")) then
            best = {idx = idx, r = l.r, x = l.x, y = l.y}
        end
    end
    S.urgent = nil
    if best == nil and M.parity and S.behind ~= nil and S.targets ~= nil then
        -- (M.parity, the owner: behind on metal sites, the army takes one: an enemy-held or contested site, the
        -- resource the town is behind on, the nearest home first)
        for _, t in ipairs(S.targets) do
            if S.behind[t.r] and (best == nil or (t.r == "iron" and best.r ~= "iron") or (t.r == best.r and t.d < best.d))
                    and not (S.tried or {})[t.idx] then
                best = {idx = t.idx, r = t.r, x = t.x, y = t.y, d = t.d, take = true}
            end
        end
        if best ~= nil and S.lost[best.idx] == nil then
            S.lost[best.idx] = {r = best.r, x = best.x, y = best.y, t = now, take = true}
            print(string.format("recover behind r=%s ours=%d theirs=%d target=%d at=%d,%d", best.r, working[best.r],
                S.theirs[best.r], best.idx, best.x, best.y))
        end
    end
    if best == nil then
        return
    end
    local units, towers = enemy_at(best.x, best.y)
    local need = M.NEED_BASE + M.NEED_UNIT * #units + M.NEED_TOWER * #towers
    local existential = (working[best.r] < M.URGENT_MIN and S.peak[best.r] >= M.URGENT_MIN)
        or (M.parity and S.theirs ~= nil and working[best.r] <= S.theirs[best.r])
    local ids = existential and all_soldiers(mem) or G.free_soldiers(best.x, best.y)
    if existential then
        S.urgent = true   -- (the army spends the stockpile while it gathers its force)
    end
    if #ids >= need then
        local g = G.create(mem, {task = "recover", manual = true, units = ids, x = best.x, y = best.y})
        S.op = {group = g.id, site = best.idx, r = best.r, x = best.x, y = best.y, t = now, n = #ids,
                last_enemy = now}
        print(string.format("recover go r=%s site=%d n=%d need=%d enemy=%d towers=%d existential=%s working=%d",
            best.r, best.idx, #ids, need, #units, #towers, tostring(existential), working[best.r]))
    elseif not S.lost[best.idx].no_force and now - S.lost[best.idx].t >= (existential and 180000 or 60000) then
        -- (no force for it: a replacement at once, the lost site kept in mind)
        S.lost[best.idx].no_force = true
        print(string.format("recover noforce r=%s site=%d n=%d need=%d working=%d", best.r, best.idx, #ids, need,
            working[best.r]))
    end
end

return M
