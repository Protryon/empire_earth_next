-- Age of AI port, the tactics (upstream dll/ai/attack.c the siege controller, garrison.c the settlement upgrades,
-- counterfire.c; UPSTREAM has the map and the licence: GPL-3.0, Grille and the Age of AI contributors).
-- require("ageofai.tactics").
--
-- SIEGE (attack_siege): the AI keeps 2 + a quarter of the known enemy defences siege weapons, at most 4, from the
-- Bronze Age (a Siege Factory first); 3 lost within 10 minutes: no new ones for 10 minutes. They gather at home and
-- go with the force (lib.groups: a member like any other, but with the force's attack-move the engine keeps them at
-- their range).
-- SETTLEMENT UPGRADES (garrison_upgrades): every 20 s the holder with the most metal mines within reach (their
-- gatherers counting, gold and iron double) that has room takes up to 10 spare citizens of its land, the nearest
-- first: idle ones, then wood cutters while the wood stock is over 5000, then farmers with 10000 food in the bank;
-- a holder ordered waits a minute (or until its citizens are in). Each citizen inside raises the stone, gold and iron
-- dropped there. THE MINES' HOLDERS: only a holder with a stone, gold or iron mine in its bonus's reach, and the one
-- where the next citizen raises the income most (the mines' gatherers x 50/25/15% by what is inside), the mine's own
-- nearest holder; and only where its citizens work (a territory it holds, not being lost). A SECOND PRODUCER: food
-- income over 1.2 x what its producers turn into citizens without a pause: a Settlement gets the citizens it lacks
-- to 5 (the Town Center open to it). A DRAIN, NOT A COST: a Town Center or Capitol only with 10000 food in the bank
-- and citizens beyond the work's need. A GRANARY BEFORE THE NEXT: build.lua's ask (mem.gu_gran, memory.eco) fills the
-- fullest granary with room instead.
-- COUNTER-FIRE (counter_fire): every 2 s (the micro step) the enemy shooters that hit own units from beyond their
-- reach (the attacked events: the attacker further off than the victim's range + 1, its own range 5 tiles at
-- least); shooters within 6 tiles of each other are one target; the own soldiers within 24 tiles, nearest first,
-- join until they are 1.5 x the enemy fighters seen within 10 tiles of the shooter (at most 60); short of that nobody
-- goes. They are committed for 45 s (the force's orders leave them alone).
local V = require("lib.view")
local S = require("ageofai.settings")
local AT = require("ageofai.attack")   -- [attack.c] the siege's target and firing position
local LA = require("lib.army")        -- [forces.c] goal_near
local G = require("lib.groups")       -- [bunker.c] the reserve takes the force's soldiers at home
local FR = require("ageofai.fronts")  -- [bunker.c TOWN GUARDS] the front stations, [fronts.c]
local TV = require("ageofai.territories")   -- [work.c wk_workable / wk_beside] where its citizens may work
local M = {}

local SIEGE = {"gun_siege13_paladin_cannon", "gun_siege10_howitzer_cannon", "gun_siege09_serpentine_cannon",
               "gun_siege07_basilisk_cannon", "gun_siege06_trebuchet", "gun_siege04_catapult"}
local SIEGE_SET = {}
for _, t in ipairs(SIEGE) do
    SIEGE_SET[t] = true
end
M.SIEGE_SET = SIEGE_SET
local TOWNS = {b_settlement = 1, b_town_center = 2, b_capitol = 3}

local function d2(x1, y1, x2, y2)
    local dx, dy = x1 - x2, y1 - y2
    return dx * dx + dy * dy
end

-- ---------------------------------------------------------------------------------------------------------------
-- Siege

function M.siege(view, mem, order, budget)
    if not S.attack_siege or view.epoch < 3 or view.time - (mem.siege_t or 0) < 30000 then
        return
    end
    mem.siege_t = view.time
    -- the known enemy defences (armed buildings in the memory)
    local def = 0
    for _, m in ipairs(ai.memory{buildings = true}) do
        local c = ai.class(m.type)
        if c ~= nil and (c.attack or 0) > 0 then
            def = def + 1
        end
    end
    local want = def > 0 and math.min(2 + def // 4, 4) or 0
    local have = 0
    for _, t in ipairs(SIEGE) do
        local ids = ai.units{type = t}
        have = have + #ids
    end
    -- losses: 3 within 10 minutes pause the training for 10
    if (mem.siege_had or 0) > have then
        mem.siege_losses = mem.siege_losses or {}
        local l = mem.siege_losses
        l[#l + 1] = view.time
        while #l > 3 do
            table.remove(l, 1)
        end
        if #l == 3 and view.time - l[1] < 600000 then
            mem.siege_pause = view.time + 600000
            print(string.format("siege pause lost=3 within=%d", (view.time - l[1]) // 1000))
        end
    end
    mem.siege_had = have
    if have >= want or view.time < (mem.siege_pause or 0) then
        mem.special = nil   -- [priority.c] nothing wanted: the special row empty
        return
    end
    local factory = V.buildings(view, "b_siege_factory")
    if V.total(factory) == 0 then
        if V.cost(view, "b_siege_factory") ~= nil and view.time - (mem.siege_factory_t or 0) >= 300000 then
            order({verb = "build", type = "b_siege_factory", near = "town"})
            mem.siege_factory_t = view.time
        end
        return
    end
    if factory.finished == 0 then
        return
    end
    for _, t in ipairs(SIEGE) do
        local c = V.cost(view, t)
        if c ~= nil and c.producers > 0 then
            local n = want - have
            for _, r in ipairs(V.resources) do
                if c.cost[r] > 0 then
                    -- [attack.c] a priority order (g_priority_mil): the stock above the epoch reserve, not the
                    -- military account alone. [priority.c] what the list leaves a priority order (budget.order)
                    n = math.min(n, (budget.order or budget.civil)[r] // c.cost[r])
                end
            end
            if n > 0 and c.requested + c.in_production == 0 then
                order({verb = "train", type = t, n = n})
                mem.siege_trained = (mem.siege_trained or 0) + n
                mem.special = nil
                return c
            end
            -- [priority.c] the special row (econ.c special_want SP_SIEGE): what it saves for is held from the
            -- ordinary soldiers (and warships) for SP_KEEP
            if n <= 0 then
                mem.special = {type = t, n = want - have, t = view.time > 0 and view.time or 1}
            end
            return nil   -- the newest it can make: short of its price
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- [attack.c] The siege's shelling (siege_tick): every siege_fire_every while the force is out, its siege weapons
-- (the force's members of the siege types) take the target attack.lua names - the force's breach wall, else the
-- nearest known defence they outrange, within siege_fire_near of the force's middle (the force is the escort;
-- upstream's waves went from home with idle soldiers along) - from the firing position (range - 0.7 from it, out of
-- every other known defence): those within 2 tiles of it attack the target, the rest move there (a forced move);
-- held while enemy soldiers are seen within 8 tiles of the position ("enemy soldiers on the way"). They are
-- committed for three looks (the force's orders leave them alone); the force home ends it (army.lua).

function M.siege_fire(view, mem, force)
    mem.shell = mem.shell or {}
    local now = view.time
    for _, id in ipairs(ai.keys(mem.shell)) do
        if mem.shell[id] <= now then
            mem.shell[id] = nil
        end
    end
    if not S.attack_siege or force == nil or not mem.out or now - (mem.shell_t or 0) < S.siege_fire_every then
        return
    end
    mem.shell_t = now
    local siege = {}
    for _, id in ipairs(force.units) do
        local u = ai.unit(id)
        if u ~= nil and SIEGE_SET[u.type] and u.inside == nil then
            siege[#siege + 1] = u
        end
    end
    if #siege == 0 then
        return
    end
    local t, px, py, srange = AT.shelling(mem, siege)
    if t == nil then
        return
    end
    local cx, cy = force.cx or force.x, force.cy or force.y
    if d2(t.x, t.y, cx, cy) > S.siege_fire_near * S.siege_fire_near then
        return
    end
    local foes = ai.seen{enemy = true, category = "soldier", near = {px, py, 8}, limit = 1}
    if #foes > 0 then
        mem.shell_held = (mem.shell_held or 0) + 1
        return
    end
    local fire, go = {}, {}
    for _, u in ipairs(siege) do
        if d2(u.x, u.y, px, py) <= 4 then
            if u.target ~= t.id then
                fire[#fire + 1] = u.id
            end
        elseif not (u.action == "move" and LA.goal_near(u, px, py, 2)) then   -- [forces.c] goal_near
            go[#go + 1] = u.id
        end
        mem.shell[u.id] = now + S.siege_fire_every * 3
    end
    if #fire > 0 then
        ai.attack(fire, t.id)
    end
    if #go > 0 then
        ai.move(go, px, py, {forced = true})
    end
    if t.id ~= (mem.shell_target or 0) then
        mem.shell_target = t.id
        mem.shells = (mem.shells or 0) + 1
        print(string.format("siege shell target=%s at=%d,%d from=%d,%d range=%d siege=%d", t.type, t.x, t.y, px, py,
            srange // 100, #siege))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Settlement upgrades (garrison.c)

local GU_REACH2 = 20       -- GU_REACH2: a mine within this squared distance of the building (4.5 tiles)
local GU_MINES_R = 5       -- GU_MINES_R: the box the mines are looked for in
local GU_GRAN_MS = 60000   -- GU_GRAN_MS: how long build.lua's granary ask counts
local GU_SAFE_R = 12       -- GU_SAFE_R: no enemy soldiers within this of the holder

-- a class's garrison capacity (the most citizens inside; ai.class is a charge a call, the types are few)
local GU_CAP = {}
local function garrison_cap(t)
    local c = GU_CAP[t]
    if c == nil then
        local k = ai.class(t)
        c = (k and k.garrison or 0)
        GU_CAP[t] = c
    end
    return c
end

-- the metal mines within the bonus's reach of a holder, each once and once more per gatherer (gu_mines)
local function mines_near(x, y)
    local score = 0
    local res = ai.resources{near = {x + 1, y + 1, GU_MINES_R}}
    for _, id in ipairs(res) do
        local r = ai.unit(id)
        if r ~= nil and (r.kind == "stone" or r.kind == "gold" or r.kind == "iron") and (r.amount or 0) > 0
                and d2(r.x, r.y, x + 1, y + 1) <= GU_REACH2 then
            score = score + 1 + (r.workers or 0)
        end
    end
    return score
end

-- THE MINES' HOLDERS (garrison.c gu_gain): the mines whose drops the holder would raise - the mines' gatherers there
-- (gold and iron double), times what a citizen inside adds at its level (5% up to 5, 2.5% up to 15, 1.5% beyond) -
-- and only the mine's own holder: the nearest of its towns to it (the player 2026-11-15: "by near enough it picked
-- the settlment behind the woodline here")
local function mine_gain(t, towns)
    local w = 0
    for _, id in ipairs(ai.resources{near = {t.x + 1, t.y + 1, GU_MINES_R}}) do
        local r = ai.unit(id)
        if r ~= nil and (r.kind == "stone" or r.kind == "gold" or r.kind == "iron") and (r.amount or 0) > 0 then
            local d = d2(r.x, r.y, t.x + 1, t.y + 1)
            if d <= GU_REACH2 then
                local nearer = false
                for _, o in ipairs(towns) do
                    if o.id ~= t.id and d2(o.x, o.y, r.x, r.y) < d then
                        nearer = true
                        break
                    end
                end
                if not nearer then
                    w = w + 1 + (r.workers or 0) * (r.kind == "stone" and 2 or 4)
                end
            end
        end
    end
    local inside = t.garrisoned
    return w * (inside < 5 and 50 or inside < 15 and 25 or 15)
end

-- its citizens' ground (work.c wk_workable): a land territory it holds and is not losing. The territories model may
-- not be known yet (its first cells): then as before
local function workable(x, y)
    if TV.count() == 0 then
        return true
    end
    return TV.holds(x, y) and not TV.unsafe(x, y)
end

-- the same territory, or two side by side (work.c wk_beside): nobody marches across the map to go inside
local function beside(ax, ay, bx, by)
    local ra, rb = TV.terr_at(ax, ay), TV.terr_at(bx, by)
    if ra ~= nil and ra == rb then
        return true
    end
    if ra == nil or rb == nil then
        return false
    end
    for _, n in ipairs(TV.neighbours(ra)) do
        if n.o == rb and n.land then
            return true
        end
    end
    return false
end

-- the food a minute that keeps every finished citizen producer training without a pause (cz_nonstop_food; 30 s a
-- citizen, produce.lua's deviation)
local function nonstop_food(view)
    local c = V.cost(view, "citizen")
    if c == nil then
        return 0
    end
    return (c.producers or 0) * (c.cost.food or 0) * 2
end

function M.garrison(view, mem, order, wanted)
    if not S.garrison_upgrades or view.time - (mem.gu_t or 0) < S.gu_every then
        return
    end
    local now = view.time
    mem.gu_t = now
    local st = view.stock
    local off = S.gu_mine_holders == nil or S.gu_mine_holders
    -- ... only where its citizens work (work.c wk_workable / wk_beside, on the port's territories model: a switch of
    -- the port's own, the model being a deviation from the retail split)
    local where = off and S.gu_workable ~= false
    -- the citizens the economy still wants (cz_wanted) and the food its producers use without a pause
    local nons = nonstop_food(view)
    local income_food = view.income.food or 0
    local idle_spare = view.citizens >= (wanted or 0) or income_food >= nons
    -- A SECOND PRODUCER: the food income over gu_kick of what its producers turn into citizens without a pause, and
    -- the Town Center open to it
    local kick = off and (S.gu_kick or 120) > 0 and income_food * 100 >= (S.gu_kick or 120) * nons
        and V.cost(view, "b_town_center") ~= nil
    -- A DRAIN, NOT A COST: a Town Center or Capitol gets citizens only while both are in excess - gu_food in the bank
    -- and citizens beyond the work's need (the pool: those the economy does not want; a port stand-in for work.c
    -- wk_pool, whose shares the port does not expose)
    local pool = math.max(view.citizens - (wanted or 0), 0)
    local rich = not off or (S.gu_food or 10000) <= 0 or (st.food >= (S.gu_food or 10000) and pool > 0)
    local food_spare = off and ((S.gu_food or 10000) <= 0 or st.food >= (S.gu_food or 10000))
    local wood_spare = st.wood > S.gu_wood or (off and pool > 0)
    -- A GRANARY BEFORE THE NEXT: build.lua's ask (mem.gu_gran: when it was about to lay another granary while one of
    -- its has room inside) - the fullest granary with room where its citizens may work takes the citizens instead
    local ask = mem.gu_gran or nil
    local gran_asked = off and S.gu_granary ~= false and ask ~= nil and now >= ask and now - ask < GU_GRAN_MS
    -- spare citizens only (the player: "as long as it only pulls from excess"); never under 30 citizens (on an island
    -- the citizens waiting for a drop site are idle too: a first draft put the whole workforce into the settlements)
    if view.citizens < 30 then
        return
    end
    if off then
        if not (idle_spare or wood_spare or kick or gran_asked or food_spare) then
            return
        end
    elseif not ((view.idle >= 4 and view.citizens >= (wanted or 0) - 2) or st.wood > S.gu_wood) then
        return
    end
    mem.gu_sent = mem.gu_sent or {}
    mem.gu_order = mem.gu_order or {}
    local function key(x, y)
        return x * 4096 + y
    end
    -- A DONE ORDER (garrison.c): the order stays on the holder while its citizens walk in (gu_wait at most); one whose
    -- citizens are all in is done - unless that filled it - and the holder may be asked again. Nobody for it (below)
    -- notes the holder too: the next one's turn.
    local function busy(t)
        local k = key(t.x, t.y)
        local at = mem.gu_sent[k]
        if at == nil or now < at or now - at >= S.gu_wait then
            return false
        end
        local o = mem.gu_order[k]
        return o == nil or t.garrisoned < o.at + o.n
    end
    -- its towns: the Settlements, Town Centers and Capitols
    local towns = {}
    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = ai.unit(id)
        local kind = u ~= nil and TOWNS[u.type] or nil
        if kind ~= nil then
            local inside = u.garrisoned or 0
            towns[#towns + 1] = {id = id, x = u.x, y = u.y, type = u.type, kind = kind, garrisoned = inside,
                                 room = garrison_cap(u.type) - inside}
        end
    end
    local best, best_score, best_kind = nil, 0, 0
    for _, t in ipairs(towns) do
        local kind = t.kind
        -- A DRAIN, NOT A COST: a Settlement still gets its 5 (it becomes a Town Center); the rest need the excess
        if t.room > 0 and not busy(t) and (not off or rich or kind == 1) then
            local raiders = ai.seen{enemy = true, armed = true, category = "soldier", near = {t.x, t.y, GU_SAFE_R}, limit = 1}
            if #raiders == 0 and (not where or workable(t.x, t.y)) then
                local score
                if off then
                    score = mine_gain(t, towns)   -- THE MINES' HOLDERS
                    if kick and kind == 1 then
                        score = 1000 + t.garrisoned * 10 + score
                    end
                else
                    score = mines_near(t.x, t.y)
                end
                if score > 0 and (best == nil
                        or (off and score > best_score)
                        or (not off and ((kind == 1 and 1 or 0) > (best_kind == 1 and 1 or 0)
                            or ((kind == 1) == (best_kind == 1) and score > best_score)))) then
                    best, best_score, best_kind = t, score, kind
                end
            end
        end
    end
    local gran = false
    if gran_asked then
        -- A GRANARY BEFORE THE NEXT: the fullest granary with room where its citizens may work
        local gb = nil
        for _, id in ipairs(ai.units{type = {"b_granary", "b_granary_improved"}, built = true}) do
            local u = ai.unit(id)
            if u ~= nil then
                local inside = u.garrisoned or 0
                local t = {id = id, x = u.x, y = u.y, type = u.type, kind = 4, garrisoned = inside,
                           room = garrison_cap(u.type) - inside}
                if t.room > 0 and not busy(t)
                        and #ai.seen{enemy = true, armed = true, category = "soldier", near = {t.x, t.y, GU_SAFE_R}, limit = 1} == 0
                        and (not where or workable(t.x, t.y))
                        and (gb == nil or t.garrisoned > gb.garrisoned) then
                    gb = t
                end
            end
        end
        if gb ~= nil then
            best, best_kind, best_score, gran = gb, 4, 0, true
        end
    end
    if best == nil then
        return
    end
    local want = math.min(best.room, S.gu_batch)
    kick = kick and best_kind == 1
    if off and pool > 0 and not gran and want > pool then
        want = pool   -- THE POOL: as many as there are beyond the work's need
    end
    if kick then
        want = math.max(5 - best.garrisoned, 1)
    end
    -- the spare citizens: idle ones first, then wood cutters while the wood is piled up (or the pool wants them), the
    -- farmers with the food secure, and for the kick the miners last
    local land = ai.tile(best.x, best.y)
    land = land and land.continent or 0
    local picks, picked = {}, {}
    local function take(id, x, y)
        if #picks >= want or picked[id] then
            return
        end
        local t = ai.tile(x, y)
        if t == nil or (land ~= 0 and t.continent ~= land) then
            return
        end
        -- THE MINES' HOLDERS: its own territory or one beside it - THE POOL: from anywhere
        if where and pool <= 0 and not beside(x, y, best.x, best.y) then
            return
        end
        picks[#picks + 1] = id
        picked[id] = true
    end
    if idle_spare or not off then
        for _, id in ipairs(ai.units{category = "citizen", idle = true, near = {best.x, best.y, 60}}) do
            if #picks >= want then
                break
            end
            local u = ai.unit(id)
            if u ~= nil and u.held == "engine" and u.inside == nil then
                take(id, u.x, u.y)
            end
        end
    end
    if #picks < want and (wood_spare or kick or food_spare) then
        for _, id in ipairs(ai.units{category = "citizen", action = "gather", near = {best.x, best.y, 60}}) do
            if #picks >= want then
                break
            end
            local u = ai.unit(id)
            if u ~= nil and u.held == "engine" and u.inside == nil and not picked[id] then
                local kind = nil
                local r = u.target ~= nil and ai.unit(u.target) or nil
                if r ~= nil then
                    kind = r.kind
                    if kind == nil and (r.type == "b_farm" or r.type == "b_robotic_farm") then
                        kind = "food"
                    end
                end
                local ok
                if kind == "wood" then
                    ok = wood_spare or kick or gran
                elseif kind == "food" then
                    ok = food_spare and not gran
                else
                    ok = kick and (kind == "stone" or kind == "gold" or kind == "iron")   -- the miners, the kick's last
                end
                if ok then
                    take(id, u.x, u.y)
                end
            end
        end
    end
    local k = key(best.x, best.y)
    if #picks == 0 then
        mem.gu_sent[k] = now > 0 and now or 1   -- nobody for it: the next holder's turn
        mem.gu_order[k] = nil
        return
    end
    local code, reason, n = ai.garrison(picks, best.id)
    if code == "ok" and n > 0 then
        mem.gu_sent[k] = now > 0 and now or 1
        mem.gu_order[k] = {at = best.garrisoned, n = n}
        mem.gu_citizens = (mem.gu_citizens or 0) + n
        print(string.format("garrison n=%d into=%s at=%d,%d inside=%d mines=%d pool=%d kick=%s rich=%s gran=%s", n,
            best.type, best.x, best.y, best.garrisoned, best_score, pool, tostring(kick), tostring(rich), tostring(gran)))
        -- the citizens are the engine's again once inside (they are used up); until then they walk on their own
        ai.release(picks)
    end
    -- the order notes of holders long gone (a town is a new unit on the same tile after an upgrade: the tile's note ages)
    if now - (mem.gu_prune or 0) >= 600000 then
        mem.gu_prune = now
        for _, k2 in ipairs(ai.keys(mem.gu_sent)) do
            if now >= mem.gu_sent[k2] and now - mem.gu_sent[k2] >= 600000 then
                mem.gu_sent[k2] = nil
                mem.gu_order[k2] = nil
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Counter-fire (the micro step)

-- events: the attacked events since the last call. Own units hit by a seen shooter from beyond their reach
function M.counterfire(mem, events, now)
    if not S.counter_fire then
        return
    end
    mem.cf = mem.cf or {}
    local cf = mem.cf
    -- the commitments still running
    for _, id in ipairs(ai.keys(cf)) do
        if cf[id] <= now then
            cf[id] = nil
        end
    end
    local shooters = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            local a = ai.unit(e.attacker)
            local v = ai.unit(e.unit)
            if a ~= nil and v ~= nil and (a.range or 0) >= 500 and v.category ~= "resource" then
                local reach = ((v.range or 0) // 100 + 1)
                if d2(a.x, a.y, v.x, v.y) > reach * reach then
                    shooters[a.id] = a
                end
            end
        end
    end
    local ids = ai.keys(shooters)
    if #ids == 0 then
        return
    end
    local done = {}
    for _, id in ipairs(ids) do
        if not done[id] then
            local e = shooters[id]
            local cluster = {}
            for _, id2 in ipairs(ids) do
                if not done[id2] and d2(shooters[id2].x, shooters[id2].y, e.x, e.y) <= 36 then
                    done[id2] = true
                    cluster[#cluster + 1] = id2
                end
            end
            -- the enemy fighters seen within 10 of the shooter
            local foes = ai.seen{enemy = true, armed = true, near = {e.x, e.y, 10}}
            local foe = ai.strength(foes)
            -- own soldiers within 24, nearest first, not committed, not inside, on its land
            local own = ai.units{category = "soldier", near = {e.x, e.y, 24}}
            local land = ai.tile(e.x, e.y)
            land = land and land.continent or 0
            local cands, str, on = {}, 0, 0
            local cols = ai.get(own, {"hp", "attack", "inside", "x", "y", "target"})
            for k = 1, #own do
                -- [skirmish.c] the spotter is left alone (an attack order would give it away)
                if cols.hp[k] ~= nil and cols.inside[k] == nil and own[k] ~= mem.sk_spotter then
                    local hits = false
                    for _, c in ipairs(cluster) do
                        if cols.target[k] == c then
                            hits = true
                        end
                    end
                    if hits then
                        str = str + cols.hp[k] * (cols.attack[k] or 0)
                        on = on + 1
                    elseif cf[own[k]] == nil then
                        local t = ai.tile(cols.x[k], cols.y[k])
                        if (cols.attack[k] or 0) > 0 and t ~= nil and (land == 0 or t.continent == land) then
                            cands[#cands + 1] = {id = own[k], s = cols.hp[k] * cols.attack[k]}
                        end
                    end
                end
            end
            local take = 0
            while str * 2 < foe * 3 and take < #cands and on + take < 60 do
                take = take + 1
                str = str + cands[take].s
            end
            if str * 2 >= foe * 3 and take > 0 then
                local by = {}
                for k = 1, take do
                    local c = cluster[(k - 1) % #cluster + 1]
                    by[c] = by[c] or {}
                    by[c][#by[c] + 1] = cands[k].id
                    cf[cands[k].id] = now + 45000
                end
                for _, c in ipairs(ai.keys(by)) do
                    ai.attack(by[c], c)
                end
                mem.cf_sorties = (mem.cf_sorties or 0) + 1
                mem.cf_sent = (mem.cf_sent or 0) + take
                print(string.format("counterfire shooters=%d at=%d,%d foe=%d sent=%d own=%d", #cluster, e.x, e.y, foe, take, str))
            else
                mem.cf_short = (mem.cf_short or 0) + 1
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Citizen militia (defence.c CITIZEN MILITIA, citizen_militia): every 2 s - enemy citizens building a defence within 12
-- tiles of own buildings or citizens (any epoch), and up to the Bronze Age the enemy units attacking a unit of ours
-- within 10 tiles of own citizens (the step's attacked events) - are attacked by the gathering or idle citizens
-- within 10, at most 3 per enemy and against at most 6, only when those citizens' strength (hit points x attack) is
-- three times that of the enemies within 6 tiles of the target. The engine's own militia reflex (1.5 x, the towers
-- counted) is off while this runs (ai.reflex{militia = false}); the citizens go back to work after 10 s.

function M.militia(mem, events, now, epoch)
    if not S.citizen_militia then
        return
    end
    if mem.militia_t ~= nil and now - mem.militia_t < 2000 then
        return
    end
    mem.militia_t = now
    mem.militia = mem.militia or {}
    local release = {}
    for _, id in ipairs(ai.keys(mem.militia)) do
        if mem.militia[id] <= now or ai.unit(id) == nil then
            if ai.unit(id) ~= nil then
                release[#release + 1] = id
            end
            mem.militia[id] = nil
        end
    end
    if #release > 0 then
        ai.release(release)
    end
    if #ai.units{category = "citizen", held = "engine", limit = 1} == 0 then
        return
    end
    -- the targets: enemy builders of a defence near ours; the attackers of ours (up to the Bronze Age)
    local targets = {}
    for _, id in ipairs(ai.seen{enemy = true, category = "citizen", action = "build", limit = 8}) do
        local u = ai.unit(id)
        if u ~= nil and (#ai.units{category = "building", near = {u.x, u.y, 12}, limit = 1} > 0
                or #ai.units{category = "citizen", near = {u.x, u.y, 12}, limit = 1} > 0) then
            targets[#targets + 1] = u
        end
    end
    if (epoch or 0) <= 3 then
        local seen = {}
        for _, e in ipairs(events) do
            if e.kind == "attacked" and e.attacker ~= nil and not seen[e.attacker] then
                seen[e.attacker] = true
                local a = ai.unit(e.attacker)
                if a ~= nil and a.category ~= "building" and a.category ~= "animal"
                        and #ai.units{category = "citizen", near = {a.x, a.y, 10}, limit = 1} > 0 then
                    targets[#targets + 1] = a
                end
            end
        end
    end
    local used = {}
    local n = 0
    for _, t in ipairs(targets) do
        if n >= 6 then
            break
        end
        local near = {}
        local cand = ai.units{category = "citizen", held = "engine", near = {t.x, t.y, 10}, limit = 12}
        local cc = #cand > 0 and ai.get(cand, {"action"}) or nil
        for k, id in ipairs(cand) do
            if not used[id] and #near < 3 then
                local act = cc.action[k]
                if act == "gather" or act == "idle" or act == "none" then
                    near[#near + 1] = id
                end
            end
        end
        if #near > 0 then
            local foes = ai.seen{enemy = true, armed = true, near = {t.x, t.y, 6}}
            local foe = #foes > 0 and ai.strength(foes) or 0
            if ai.strength(near) >= 3 * foe then
                local code = ai.attack(near, t.id)
                if code == "ok" then
                    for _, id in ipairs(near) do
                        used[id] = true
                        mem.militia[id] = now + 10000
                    end
                    n = n + 1
                    mem.militias = (mem.militias or 0) + #near
                    if (mem.militias or 0) <= 30 or now % 60000 < 250 then
                        print(string.format("militia n=%d target=%d own=%d foe=%d militias=%d", #near, t.id, ai.strength(near), foe, mem.militias))
                    end
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Tower bait (early.c tower_bait): up to the Bronze Age, every 2 s - a citizen with an enemy soldier within 5 tiles
-- (or one attacking it) and outside its own towers' reach walks to the nearest finished tower (or other own building
-- that shoots) within 25 tiles, to the spot 1.5 tiles behind it from the enemy: the raiders that follow come under
-- the tower. Once it is there the order ends and the assignment sends it back to work (the safety's released claims
-- keep it off raided sites meanwhile). Citizens the militia sent at the enemy are left alone.

function M.bait(mem, events, now, epoch, town_x, town_y)
    mem.town_x, mem.town_y = town_x, town_y
    if not S.tower_bait or (epoch or 0) > S.tower_bait_epoch then
        return
    end
    if mem.bait_t ~= nil and now - mem.bait_t < 2000 then
        return
    end
    mem.bait_t = now
    mem.bait = mem.bait or {}
    -- those sent before: released once there (within 2 tiles of the spot) or after 20 s
    local release = {}
    for _, id in ipairs(ai.keys(mem.bait)) do
        local b = mem.bait[id]
        local u = ai.unit(id)
        if u == nil then
            mem.bait[id] = nil
        elseif b.t <= now or (math.abs(u.x - b.x) <= 2 and math.abs(u.y - b.y) <= 2) then
            release[#release + 1] = id
            mem.bait[id] = nil
        end
    end
    if #release > 0 then
        ai.release(release)
    end
    local bx, by = mem.town_x or -1, mem.town_y or -1
    if bx < 0 then
        return
    end
    -- own towers and shooting buildings: where, their reach
    local cov = ai.units{category = "building", armed = true, built = true, near = {bx, by, 36}, limit = 24}
    if #cov == 0 then
        return
    end
    local cc = ai.get(cov, {"x", "y", "range"})
    -- the enemy soldiers seen: where, whom they attack (the step's events)
    local en = ai.seen{enemy = true, armed = true, category = "soldier", near = {bx, by, 36}, limit = 64}
    if #en == 0 then
        return
    end
    local ec = ai.get(en, {"x", "y"})
    local victim_of = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.attacker ~= nil then
            victim_of[e.unit] = e.attacker
        end
    end
    local cits = ai.units{category = "citizen", held = "engine", near = {bx, by, 32}}
    if #cits == 0 then
        return
    end
    local cx = ai.get(cits, {"x", "y"})
    local sent = 0
    for k, id in ipairs(cits) do
        local ux, uy = cx.x[k], cx.y[k]
        if ux ~= nil and (mem.militia == nil or mem.militia[id] == nil) and mem.bait[id] == nil then
            local hit = nil
            for j = 1, #en do
                local ex, ey = ec.x[j], ec.y[j]
                if ex ~= nil and (victim_of[id] == en[j] or (math.abs(ex - ux) <= 5 and math.abs(ey - uy) <= 5)) then
                    hit = j
                    break
                end
            end
            if hit ~= nil then
                local best, bd, covered = nil, 25 * 25, false
                for c = 1, #cov do
                    local tx, ty = cc.x[c], cc.y[c]
                    if tx ~= nil then
                        local d = (tx - ux) * (tx - ux) + (ty - uy) * (ty - uy)
                        local r = (cc.range[c] or 0) / 100 - 0.5
                        if d <= r * r then
                            covered = true   -- covered already: the tower fights
                            break
                        end
                        if d < bd then
                            bd, best = d, c
                        end
                    end
                end
                if not covered and best ~= nil then
                    -- the spot 1.5 tiles behind the tower from the enemy
                    local tx, ty = cc.x[best], cc.y[best]
                    local ex, ey = ec.x[hit], ec.y[hit]
                    local dx, dy = tx - ex, ty - ey
                    local len = math.max(math.abs(dx), math.abs(dy), 1)
                    local sx, sy = tx + dx * 3 // (2 * len), ty + dy * 3 // (2 * len)
                    if sx == tx and sy == ty then
                        sx, sy = tx + (dx >= 0 and 1 or -1), ty
                    end
                    ai.move({id}, sx, sy, {forced = true})
                    mem.bait[id] = {x = sx, y = sy, t = now + 20000}
                    sent = sent + 1
                    mem.baits = (mem.baits or 0) + 1
                end
            end
        end
    end
    if sent > 0 and ((mem.baits or 0) <= 20 or now % 60000 < 250) then
        print(string.format("bait n=%d baits=%d enemies=%d", sent, mem.baits, #en))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- Bunkers (bunker.c): the Fortress holds units like a transport (inside they are off the map and out of the
-- population). THE SHELTER (bunkers): from the Copper Age a Fortress by the drop site nearest the base once citizens
-- were hurt three times within two minutes or enemy aircraft are possible (both sides in World War I; two
-- fortresses from 40 citizens); its wood is held back from unit training while it waits (bunker_reserve, econ.lua's
-- reserve); a citizen hit by a seen soldier, and the citizens within 6 tiles of the attacker, go into the nearest
-- finished fortress within 15 tiles with room (ai.load; the micro step); a fortress with nothing hurt within 10
-- tiles for 45 s and no enemy soldiers seen within 10 lets them out after two quiet looks 8 s apart, 3 tiles to a
-- side, the next side when nobody came out (the game takes an unload it cannot do and does nothing), and they are
-- released. THE RESERVE (bunker_reserve): every 10 s with the map full (population at the cap - 1), up to 10 of the
-- force's soldiers at home (within 25 tiles of the base, the force home; 4 stay out) go into the fortress with the
-- most room and training fills the map again; once the population is 4 below the cap with nothing loaded for a
-- minute (losses), or enemy soldiers come within 20 tiles of it, the fullest fortress lets everyone out (not one
-- where citizens shelter) and they are the force's again. THE SHELF (shelve_citizens): every 10 s from the Copper
-- Age, while the population stands at the cap (5 under at most, units queued) and food, wood, gold and iron each
-- lie at shelve_stock or more, up to 20 citizens go into a fortress of the shelf - the nearest gatherers of the
-- resource with the most in stock first, then of any resource at twice shelve_stock, never under max(cap / 8, 40)
-- left out - and no new citizens are trained (econ.lua's citizens wanted); no room in the shelf: one more fortress
-- (up to 8). Out again, a fortress at a time, once one of the four falls under half of shelve_stock or the citizens
-- out are under 3/4 of what is kept out; enemies near keep them in. TOWN GUARDS (town_guards, off as upstream ships
-- it; with the reserve): a fortress also at each front station (fronts.lua's FR.station 1 and 2) without one within
-- TG_NEAR tiles, and the reserve fills each fortress from the free soldiers round it, not only those at the base - the
-- one with the most - and it lets its guard out when enemies come near it. Deviations: the port's "free soldiers" are
-- the force's at home (upstream's are the game's in no group); the room is the view's population (with the units
-- in production, which upstream's room left out); the fortress's spot is ai.find_site by the drop site or the station.

-- the step's part: the fortress wanted and the quiet fortresses' unload
local SIDES = {{0, 3}, {3, 0}, {0, -3}, {-3, 0}, {3, 3}, {-3, 3}, {3, -3}, {-3, -3}}
local TG_NEAR = 12   -- TOWN GUARDS: a front station without a fortress within these tiles wants one

local function raiders_near(x, y, r)
    return #ai.seen{enemy = true, armed = true, category = "soldier", near = {x, y, r}, limit = 1} > 0
end

-- bk_unload: the holder lets everyone out 3 tiles to a side on land; holding as many at this unload as at the last,
-- it tries the next side
local function unload_fort(mem, f)
    mem.bk_out = mem.bk_out or {}
    local o = mem.bk_out[f.id] or {side = 1, held = -1}
    if o.held == (f.cargo or 0) then
        o.side = o.side % 8 + 1
    end
    o.held = f.cargo or 0
    for _ = 1, 8 do
        local d = SIDES[o.side]
        local t = ai.tile(f.x + d[1], f.y + d[2])
        if t ~= nil and (t.continent or 0) ~= 0 then
            break
        end
        o.side = o.side % 8 + 1
    end
    mem.bk_out[f.id] = o
    local d = SIDES[o.side]
    local code = ai.unload(f.id, f.x + d[1], f.y + d[2])
    return code == "ok"
end

-- the citizens hit within r tiles of (x, y) in the last ms (the micro step's hurt spots)
local function hurt_near(mem, now, x, y, r, ms)
    local n = 0
    for _, h in ipairs(mem.hurt_at or {}) do
        if now - h.t <= ms and math.abs(h.x - x) <= r and math.abs(h.y - y) <= r then
            n = n + 1
        end
    end
    return n
end

-- the units of a hold record (shelter, reserve, shelf) that are not inside go back to the engine; returns how many
local function release_out(rec)
    local keep, out = {}, {}
    for _, id in ipairs(rec.units or {}) do
        local u = ai.unit(id)
        if u ~= nil and u.inside ~= nil then
            keep[#keep + 1] = id
        elseif u ~= nil then
            out[#out + 1] = id
        end
    end
    if #out > 0 then
        ai.release(out)
    end
    rec.units = keep
    return #out
end

local function take_from_group(g, ids)
    local set = {}
    for _, id in ipairs(ids) do
        set[id] = true
    end
    local keep = {}
    for _, id in ipairs(g.units) do
        if not set[id] then
            keep[#keep + 1] = id
        else
            g.aims[id] = nil
        end
    end
    g.units = keep
end

-- a hold record's soldiers or citizens out again (after an unload, or not in within a minute of the load) are the
-- engine's; an empty record is dropped
local function hold_records(mem, recs, now)
    for _, fid in ipairs(ai.keys(recs)) do
        local rec = recs[fid]
        local f = ai.unit(fid)
        if f == nil then
            if #(rec.units or {}) > 0 then
                ai.release(rec.units)
            end
            recs[fid] = nil
        else
            if rec.unloading or now - (rec.in_t or 0) > 60000 then
                release_out(rec)
            end
            if rec.unloading and (f.cargo or 0) == 0 then
                rec.unloading = nil
            end
            if #rec.units == 0 and (f.cargo or 0) == 0 then
                recs[fid] = nil
            end
        end
    end
end

-- the step's part: the fortress wanted, the quiet fortresses' unload, the reserve and the shelf
function M.bunkers(view, mem, order, above, memory)
    if not S.bunkers or view.epoch < 2 or view.town_x < 0 then
        return
    end
    mem.bunker_wood = 0   -- [priority.c] the fortress row (bunker_reserve): the wood it cannot pay yet
    local now = view.time
    mem.hurt = mem.hurt or {}
    local keep = {}
    for _, t in ipairs(mem.hurt) do
        if now - t <= 120000 then
            keep[#keep + 1] = t
        end
    end
    mem.hurt = keep
    mem.shelter = mem.shelter or {}
    mem.reserve = mem.reserve or {}
    mem.shelf = mem.shelf or {forts = {}}
    local forts, nall = {}, 0
    for _, fid in ipairs(ai.units{type = "b_fortress"}) do
        local f = ai.unit(fid)
        if f ~= nil then
            nall = nall + 1
            if f.built then
                forts[#forts + 1] = f
            end
        end
    end
    -- THE FORTRESS WANTED (bk_build, once a minute): citizens hurt or aircraft possible, the reserve near the cap,
    -- the shelf full
    if now - (mem.fort_t or 0) >= 60000 then
        mem.fort_t = now
        local air = false
        if view.epoch >= 9 then
            for _, en in ipairs(view.enemies or {}) do
                if (en.epoch or 0) >= 9 then
                    air = true
                end
            end
        end
        local want = (air or #mem.hurt >= 3) and (view.citizens >= 40 and 2 or 1) or 0
        local room = view.population_cap - view.population
        if S.bunker_reserve and room <= 5 and want < 1 then
            want = 1
        end
        if mem.shelf.want and want < nall + 1 then
            want = math.min(nall + 1, 8)
        end
        -- TOWN GUARDS ([ai_base] town_guards, off upstream; with the reserve): a fortress at each front station
        -- (fronts.c: its tower nearest the front, a bit back - FR.station) without one within TG_NEAR tiles. The
        -- reserve fills it from the free soldiers round it (below) and it lets its guard out when enemies come near it.
        local guard_at = nil
        if S.town_guards and S.bunker_reserve and memory ~= nil and nall < 8 then
            for k = 1, 2 do
                local sx, sy = FR.station(memory, k)
                if sx ~= nil and guard_at == nil then
                    local has = false
                    for _, f in ipairs(forts) do
                        if d2(f.x, f.y, sx, sy) <= TG_NEAR * TG_NEAR then
                            has = true
                        end
                    end
                    if not has then
                        guard_at = {x = sx, y = sy}
                    end
                end
            end
            if guard_at ~= nil then
                want = math.max(want, nall + 1)
            end
        end
        local c = V.cost(view, "b_fortress")
        memory.eco.bunker_wood = 0
        if nall < want and c ~= nil then
            if view.stock.wood < c.cost.wood then
                memory.eco.bunker_wood = c.cost.wood   -- can't pay yet: saved for (bunker_reserve)
                mem.bunker_wood = c.cost.wood          -- [priority.c] the fortress row reads it here
            elseif require("ageofai.priority").refuses(view, memory, "b_fortress") == nil then   -- [priority.c]
                -- the drop site nearest the base without a fortress within 10 tiles; the town guard's station when one
                -- of the front stations lacks a fortress
                local best, bd = nil, nil
                if guard_at == nil then
                    for _, id in ipairs(ai.units{category = "building", built = true}) do
                        local u = ai.unit(id)
                        if u ~= nil and (u.type == "b_town_center" or u.type == "b_capitol" or u.type == "b_settlement") then
                            local near = false
                            for _, f in ipairs(forts) do
                                if math.abs(f.x - u.x) <= 10 and math.abs(f.y - u.y) <= 10 then
                                    near = true
                                end
                            end
                            local d = d2(u.x, u.y, view.town_x, view.town_y)
                            if not near and (bd == nil or d < bd) then
                                best, bd = u, d
                            end
                        end
                    end
                end
                local bx, by = nil, nil
                if guard_at ~= nil then
                    bx, by = guard_at.x, guard_at.y
                elseif best ~= nil then
                    bx, by = best.x, best.y
                end
                if bx ~= nil then
                    local x, y = ai.find_site("b_fortress", bx, by, {min = 4, max = 9, safe = true})
                    if x == nil then
                        x, y = ai.find_site("b_fortress", bx, by, {min = 4, max = 9})
                    end
                    if x ~= nil then
                        order({verb = "build", type = "b_fortress", near = "tile", x = x, y = y})
                        mem.forts_ordered = (mem.forts_ordered or 0) + 1
                        print(string.format("bunker fortress at=%d,%d why=%s hurt=%d forts=%d citizens=%d", x, y,
                            guard_at ~= nil and "town guard" or mem.shelf.want and "shelf" or air and "aircraft"
                                or #mem.hurt >= 3 and "hurt" or "reserve",
                            #mem.hurt, nall, view.citizens))
                    end
                end
            end
        end
    end
    -- THE SHELTER's quiet release (bk_flush)
    for _, f in ipairs(forts) do
        local sh = mem.shelter[f.id]
        if sh ~= nil then
            if (f.cargo or 0) <= 0 then
                if now - sh.t > 30000 then   -- empty, and nobody still walking in
                    sh.unloading = true
                end
            elseif hurt_near(mem, now, f.x, f.y, 10, 45000) > 0 or raiders_near(f.x, f.y, 10) then
                sh.quiet = nil
            elseif sh.quiet == nil then
                sh.quiet = now
            elseif now - sh.quiet >= 8000 then   -- two quiet looks
                if unload_fort(mem, f) then
                    sh.quiet = nil
                    sh.unloading = true
                    sh.outs = (sh.outs or 0) + 1
                    mem.sheltered_out = (mem.sheltered_out or 0) + (f.cargo or 0)
                    print(string.format("bunker quiet at=%d,%d out=%d outs=%d", f.x, f.y, f.cargo or 0, sh.outs))
                end
            end
        end
    end
    hold_records(mem, mem.shelter, now)
    -- THE RESERVE (bunker_reserve): every 10 s with a finished fortress
    if S.bunker_reserve and #forts > 0 and now - (mem.res_t or 0) >= 10000 then
        mem.res_t = now
        local room = view.population_cap - view.population
        local threat = nil
        for _, f in ipairs(forts) do
            if threat == nil and mem.reserve[f.id] ~= nil and (f.cargo or 0) > 0 and mem.shelter[f.id] == nil
                    and mem.shelf.forts[f.id] == nil and raiders_near(f.x, f.y, 20) then
                threat = f
            end
        end
        if room <= 1 and threat == nil then
            -- the free soldiers at home: the force's members on the map while the force is home
            local force = G.role(memory, "force")
            local free = {}
            if force ~= nil and not mem.out then
                local cols = ai.get(force.units, {"x", "y", "inside", "speed"})
                for k, id in ipairs(force.units) do
                    if cols.x[k] ~= nil and cols.inside[k] == nil and (cols.speed[k] or 0) > 0 and id ~= mem.sk_spotter
                            and not M.committed(mem, id, now) then
                        free[#free + 1] = {id = id, x = cols.x[k], y = cols.y[k]}
                    end
                end
            end
            local best, space, bx, by = nil, 0, view.town_x, view.town_y
            if S.town_guards and #free > 0 then
                -- TOWN GUARDS: the fortress with the most free soldiers round it (and room) takes them, not only the
                -- base's (bunker.c bk_reserve_tick)
                local most = 0
                for _, f in ipairs(forts) do
                    local c = ai.class(f.type)
                    local rec = mem.reserve[f.id]
                    local walking = rec ~= nil and math.max(#rec.units - (f.cargo or 0), 0) or 0
                    local s = (c and c.transport or 0) - (f.cargo or 0) - walking
                    if s > 0 and mem.shelf.forts[f.id] == nil and mem.shelter[f.id] == nil then
                        local n = 0
                        for _, w in ipairs(free) do
                            if d2(w.x, w.y, f.x, f.y) <= 625 then
                                n = n + 1
                            end
                        end
                        if n > most then
                            most, best, space, bx, by = n, f, s, f.x, f.y
                        end
                    end
                end
            else
                -- the fortress with the most room (not the shelf's)
                for _, f in ipairs(forts) do
                    local c = ai.class(f.type)
                    local rec = mem.reserve[f.id]
                    local walking = rec ~= nil and math.max(#rec.units - (f.cargo or 0), 0) or 0
                    local s = (c and c.transport or 0) - (f.cargo or 0) - walking
                    if s > space and mem.shelf.forts[f.id] == nil then
                        best, space = f, s
                    end
                end
            end
            -- the free soldiers within 25 tiles of it
            local cands = {}
            for _, w in ipairs(free) do
                if d2(w.x, w.y, bx, by) <= 625 then
                    cands[#cands + 1] = w
                end
            end
            local n = math.min(#cands - 4, 10, space)
            if best ~= nil and n > 0 then
                local fx, fy = best.x, best.y
                table.sort(cands, function(a, b)
                    local da, db = d2(a.x, a.y, fx, fy), d2(b.x, b.y, fx, fy)
                    if da ~= db then
                        return da < db
                    end
                    return a.id < b.id
                end)
                local ids = {}
                for k = 1, n do
                    ids[k] = cands[k].id
                end
                local code = ai.load(ids, best.id)
                if code == "ok" then
                    take_from_group(force, ids)
                    local rec = mem.reserve[best.id] or {units = {}}
                    for _, id in ipairs(ids) do
                        rec.units[#rec.units + 1] = id
                    end
                    rec.in_t = now
                    mem.reserve[best.id] = rec
                    mem.res_load_t = now
                    mem.res_in = (mem.res_in or 0) + n
                    print(string.format("reserve in room=%d n=%d at=%d,%d holds=%d in=%d out=%d", room, n, fx, fy, best.cargo or 0,
                        mem.res_in, mem.res_out or 0))
                end
            end
        elseif threat ~= nil or (room >= 4 and (mem.res_load_t == nil or now - mem.res_load_t >= 60000)) then
            local best, held = threat, threat ~= nil and (threat.cargo or 0) or 0
            if threat == nil then
                for _, f in ipairs(forts) do
                    if mem.reserve[f.id] ~= nil and (f.cargo or 0) > held and mem.shelter[f.id] == nil and mem.shelf.forts[f.id] == nil then
                        best, held = f, f.cargo
                    end
                end
            end
            if best ~= nil and unload_fort(mem, best) then
                mem.reserve[best.id].unloading = true
                mem.res_out = (mem.res_out or 0) + held
                print(string.format("reserve out room=%d%s at=%d,%d n=%d in=%d out=%d", room, threat ~= nil and " enemies_near" or "",
                    best.x, best.y, held, mem.res_in or 0, mem.res_out))
            end
        end
    end
    hold_records(mem, mem.reserve, now)
    -- THE SHELF (shelve_citizens): every 10 s from the Copper Age
    local shelf = mem.shelf
    if S.shelve_citizens and now - (mem.shelf_t or 0) >= 10000 then
        mem.shelf_t = now
        local st = view.stock
        local function rich(k)
            return st.food >= k and st.wood >= k and st.gold >= k and st.iron >= k
        end
        local keep_out = math.max(view.population_cap // 8, 40)
        local inside = 0   -- citizens in the fortresses (the shelf's and the shelter's)
        for _, f in ipairs(forts) do
            if shelf.forts[f.id] ~= nil or mem.shelter[f.id] ~= nil then
                inside = inside + (f.cargo or 0)
            end
        end
        local out = view.citizens - inside
        local room = view.population_cap - view.population
        local queued = 0
        for _, c in ipairs(view.costs) do
            if not c.building then
                queued = queued + c.in_production
            end
        end
        shelf.want = nil
        if not rich(S.shelve_stock // 2) or out < keep_out * 3 // 4 then
            -- out again: the fullest of the shelf's, if quiet
            local best, held = nil, 0
            for _, f in ipairs(forts) do
                local rec = shelf.forts[f.id]
                if rec ~= nil then
                    if (f.cargo or 0) <= 0 and #rec.units == 0 then
                        shelf.forts[f.id] = nil   -- empty: an ordinary fortress again
                    elseif (f.cargo or 0) > held and not raiders_near(f.x, f.y, 15) then
                        best, held = f, f.cargo
                    end
                end
            end
            if best ~= nil and unload_fort(mem, best) then
                shelf.forts[best.id].unloading = true
                shelf.n_out = (shelf.n_out or 0) + held
                print(string.format("shelf out n=%d at=%d,%d out=%d keep=%d stock=%d/%d/%d/%d in=%d out_total=%d", held, best.x, best.y,
                    out, keep_out, st.food, st.wood, st.gold, st.iron, shelf.n_in or 0, shelf.n_out))
            end
        elseif room <= 5 and queued >= 1 and rich(S.shelve_stock) and out > keep_out then
            shelf.on_t = now
            -- a fortress of the shelf with room, else an empty one (not the shelter's, not the reserve's)
            local best, space = nil, 0
            for _, f in ipairs(forts) do
                local c = ai.class(f.type)
                local rec = shelf.forts[f.id]
                local walking = rec ~= nil and math.max(#rec.units - (f.cargo or 0), 0) or 0
                local s = (c and c.transport or 0) - (f.cargo or 0) - walking
                local skip = mem.shelter[f.id] ~= nil or (rec == nil and ((f.cargo or 0) > 0 or mem.reserve[f.id] ~= nil))
                if not skip and s > space + (rec ~= nil and -1 or 0) then
                    best, space = f, s
                end
            end
            if best == nil or space < 5 then
                shelf.want = true
            else
                local n = math.min(out - keep_out, 20, space)
                local richest = "food"   -- the resource with the most in stock: its gatherers first
                for _, k in ipairs({"wood", "stone", "gold", "iron"}) do
                    if st[k] > st[richest] then
                        richest = k
                    end
                end
                local cands = ai.units{category = "citizen", action = "gather", near = {best.x, best.y, 60}, limit = 80}
                local cols = ai.get(cands, {"x", "held", "inside", "target"})
                local pick, picked, kinds = {}, {}, {}
                for pass = 0, 1 do
                    for k, id in ipairs(cands) do   -- nearest first
                        if #pick >= n then
                            break
                        end
                        if cols.x[k] ~= nil and cols.held[k] == "engine" and cols.inside[k] == nil and cols.target[k] ~= nil and not picked[id] then
                            local kind = kinds[k]
                            if kind == nil then
                                local r = ai.unit(cols.target[k])
                                kind = r ~= nil and r.kind or false
                                kinds[k] = kind
                            end
                            -- never the scarce resource's gatherers (under twice shelve_stock)
                            if kind and (pass == 0) == (kind == richest) and (st[kind] or 0) >= 2 * S.shelve_stock then
                                pick[#pick + 1] = id
                                picked[id] = true
                            end
                        end
                    end
                end
                if #pick > 0 then
                    local code = ai.load(pick, best.id)
                    if code == "ok" then
                        local rec = shelf.forts[best.id] or {units = {}}
                        for _, id in ipairs(pick) do
                            rec.units[#rec.units + 1] = id
                        end
                        rec.in_t = now
                        shelf.forts[best.id] = rec
                        shelf.n_in = (shelf.n_in or 0) + #pick
                        print(string.format("shelf in room=%d n=%d at=%d,%d holds=%d out=%d keep=%d stock=%d/%d/%d/%d in=%d out_total=%d",
                            room, #pick, best.x, best.y, best.cargo or 0, out, keep_out, st.food, st.wood, st.gold, st.iron, shelf.n_in, shelf.n_out or 0))
                    end
                end
            end
        end
    end
    hold_records(mem, shelf.forts, now)
    -- shelf_wanted (econ.lua): while the shelf works or holds citizens, no more citizens than there are
    local held = 0
    for _, f in ipairs(forts) do
        if shelf.forts[f.id] ~= nil then
            held = held + (f.cargo or 0)
        end
    end
    memory.shelf_hold = (shelf.on_t ~= nil and now - shelf.on_t < 30000) or held > 0
end

-- the micro step's part: the citizens hit go into a fortress near
function M.shelter(mem, events, now)
    if not S.bunkers then
        return
    end
    mem.hurt = mem.hurt or {}
    mem.hurt_at = mem.hurt_at or {}
    mem.shelter = mem.shelter or {}
    local attackers = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" then
            local v = ai.unit(e.unit)
            if v ~= nil and v.category == "citizen" then
                mem.hurt[#mem.hurt + 1] = now
                mem.hurt_at[#mem.hurt_at + 1] = {x = v.x, y = v.y, t = now}
                if #mem.hurt_at > 64 then
                    table.remove(mem.hurt_at, 1)
                end
                if e.attacker ~= nil then
                    local a = ai.unit(e.attacker)
                    if a ~= nil and a.category == "soldier" and a.player ~= v.player then
                        attackers[a.id] = a
                    end
                end
            end
        end
    end
    for _, aid in ipairs(ai.keys(attackers)) do
        local a = attackers[aid]
        local best, bd = nil, nil
        for _, fid in ipairs(ai.units{type = "b_fortress", built = true, near = {a.x, a.y, 15}}) do
            local f = ai.unit(fid)
            local c = f ~= nil and ai.class(f.type) or nil
            -- not the shelf's fortresses (the shelter and the reserve keep off them)
            if f ~= nil and c ~= nil and (f.cargo or 0) < (c.transport or 0) and (mem.shelf == nil or mem.shelf.forts[fid] == nil) then
                local d = d2(f.x, f.y, a.x, a.y)
                if bd == nil or d < bd then
                    best, bd = f, d
                end
            end
        end
        if best ~= nil then
            local pick = {}
            for _, id in ipairs(ai.units{category = "citizen", near = {a.x, a.y, 6}}) do
                local u = ai.unit(id)
                if u ~= nil and u.inside == nil and (mem.bait == nil or mem.bait[id] == nil) then
                    pick[#pick + 1] = id
                end
            end
            if #pick > 0 then
                local code, reason, n = ai.load(pick, best.id)
                if code == "ok" then
                    local sh = mem.shelter[best.id] or {units = {}, t = now}
                    sh.t = now
                    sh.in_t = now
                    sh.quiet = nil
                    sh.unloading = nil
                    for _, id in ipairs(pick) do
                        sh.units[#sh.units + 1] = id
                    end
                    mem.shelter[best.id] = sh
                    mem.sheltered = (mem.sheltered or 0) + n
                end
            end
        end
    end
end

-- whether a unit is committed to a counter-fire sortie (the force's orders leave it alone)
function M.committed(mem, id, now)
    return mem.cf ~= nil and mem.cf[id] ~= nil and mem.cf[id] > now
end

return M

