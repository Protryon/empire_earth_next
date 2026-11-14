-- EECPUnitManager in Lua (the classic port; ORIGIN in this folder): the unit requests in per-epoch FIFOs, the citizens
-- wanted, the production started at the least busy producers (ai.train), and EEComputerPlayer::CanAfford (the epoch
-- reserve and the personality's rules the classic checks before it pays). require("classic_lua.units").
--
-- Memory (memory.um): req[epoch + 1] = {{class = name, n = count}, ...}; waiting[class] = count.
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local C = T.C

local M = {}

local RES = R.RES

function M.state(memory)
    local um = memory.um
    if um == nil then
        um = {req = {}, waiting = {}, enabled = false}
        for e = 1, 15 do
            um.req[e] = {}
        end
        memory.um = um
    end
    return um
end

-- ---------------------------------------------------------------------------------------------------------------
-- the tech tree as the classic reads it (EETechTreeNode): a class -> its tech's epoch, producers and cost; a tech id ->
-- its record. Rebuilt every epoch (ai.techs; the availability changes); not saved

local sTree = {epoch = -1}

function M.tree(view)
    if sTree.epoch ~= view.epoch or sTree.time == nil or view.time - sTree.time >= 30000 then
        local byClass, byId, list = {}, {}, ai.techs{epoch = 14}
        for _, t in ipairs(list) do
            byId[t.id] = t
            if t.makes ~= nil and byClass[t.makes] == nil then
                byClass[t.makes] = t
            end
        end
        sTree = {epoch = view.epoch, time = view.time, byClass = byClass, byId = byId, list = list}
    end
    return sTree
end

function M.class_tech(view, class)
    local tree = M.tree(view)
    local own = R.class_of(class).tech
    if own ~= nil and tree.byId[own] ~= nil then
        return tree.byId[own]
    end
    return tree.byClass[class]
end

-- the scaled cost of a class (as the player pays it now)
function M.cost(class)
    local c = R.class_of(class)
    return ai.class(class) ~= nil and ai.class(class).cost or (c.cost or {})
end

-- the cost the needs ledger counts (EECPResourceManager::AddCosts: the tech's listed cost, ScaleCost'ed; GetCost's
-- epoch and upgrade factors left out - the future research sentinel's listed cost is what pulls the classic's
-- first gatherers to stone and iron)
function M.ledger_cost(class)
    local c = R.class_of(class)
    return c.data_cost or c.cost or {}
end

local function sum_cost(cost)
    local s = 0
    for _, r in ipairs(RES) do
        s = s + (cost[r] or 0)
    end
    return s
end

-- ---------------------------------------------------------------------------------------------------------------
-- counts (GetCount, the own units of a class; GetNumWorkers; CountMilitaryUnits)

function M.count(class)
    return #R.of_type(class)
end

function M.workers(view, memory)
    return #R.of_type(C.citizen) + #R.of_type(C.female_citizen) + #R.of_type(T.fishing_boat_class(view.epoch))
end

function M.is_military_class(class)
    local c = R.class_of(class)
    local k = c.category
    return k ~= nil and k ~= "citizen" and k ~= "boat" and k ~= "building" and (c.attack or 0) > 0
end

-- the military units and the military requests (CountMilitaryUnits(false, inRequests))
function M.military(view, memory, requests)
    local n = #ai.units{category = "soldier"} + #ai.units{category = "siege"} + #ai.units{category = "ship"}
        + #ai.units{category = "air"}
    if requests then
        local um = M.state(memory)
        for e = 1, 15 do
            for _, q in ipairs(um.req[e]) do
                if M.is_military_class(q.class) then
                    n = n + q.n
                end
            end
        end
    end
    return n
end

-- IsRequested: any epoch's list holds the class
function M.is_requested(memory, class)
    local um = M.state(memory)
    for e = 1, 15 do
        for _, q in ipairs(um.req[e]) do
            if q.class == class then
                return true
            end
        end
    end
    return false
end

-- the producers of a class (its tech's producer classes) that are built and own: {records}
function M.producers(view, class, built_only)
    local t = M.class_tech(view, class)
    local out = {}
    if t == nil then
        return out
    end
    for _, p in ipairs(t.producers or {}) do
        for _, id in ipairs(R.of_type(p)) do
            local u = R.unit(id)
            if u ~= nil and (not built_only or u.built) then
                out[#out + 1] = u
            end
        end
    end
    return out
end

-- IsInProduction: a producer researches or has queued the class (the item being made, `producing`, and the queue
-- behind it, `queued`)
function M.in_production(view, class)
    local t = M.class_tech(view, class)
    if t == nil then
        return false
    end
    for _, u in ipairs(M.producers(view, class, true)) do
        if u.producing == t.id then
            return true
        end
        -- (IsResearchQueued: the queue behind the item being made counts too; the record's `queued`)
        for _, q in ipairs(u.queued or {}) do
            if q == t.id then
                return true
            end
        end
    end
    return false
end

-- HasUnfinished: a building of the class not yet finished
function M.has_unfinished(class)
    for _, id in ipairs(ai.units{type = class}) do
        local u = R.unit(id)
        if u ~= nil and not u.built then
            return true
        end
    end
    return false
end

-- ---------------------------------------------------------------------------------------------------------------
-- EEComputerPlayer::CanAfford (the classic engine's override; a Lua seat's engine check is the plain one, so the port
-- makes the classic's checks before it orders: docs/AI_LUA_UNITS.md decision 1)

local RECOVERY = 12   -- kCPRecoveryCitizens

-- EECPArmyThreat: enemy soldiers within 40 tiles of the town centre stronger (hit points x attack) than the own army;
-- for 4 minutes. DEVIATION (fog): the soldiers seen (the original reads every player's units)
function M.army_threat(view, memory)
    local cp = memory.cp
    if cp.threat_checked ~= nil and view.time - cp.threat_checked < 5000 then
        return view.time < (cp.threat_until or 0)
    end
    cp.threat_checked = view.time
    local tx, ty = R.town(view)
    if tx == nil then
        return view.time < (cp.threat_until or 0)
    end
    local ehp, eat = 0, 0
    for _, id in ipairs(ai.seen{enemy = true, category = "soldier", near = {tx, ty, 40}}) do
        local u = R.unit(id)
        if u ~= nil then
            ehp, eat = ehp + (u.hp or 0), eat + (u.attack or 0)
        end
    end
    local ohp, oat = 0, 0
    for _, id in ipairs(ai.units{category = "soldier"}) do
        local u = R.unit(id)
        if u ~= nil then
            ohp, oat = ohp + (u.hp or 0), oat + (u.attack or 0)
        end
    end
    if ehp > 0 and ehp * eat > ohp * oat then
        cp.threat_until = view.time + 240000
    end
    return view.time < (cp.threat_until or 0)
end

-- the soldiers and the soldiers in training (EECPCountSoldiers)
local function count_soldiers(view)
    local n = #ai.units{category = "soldier"}
    local tree = M.tree(view)
    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = R.unit(id)
        if u ~= nil and u.producing ~= nil then
            local t = tree.byId[u.producing]
            if t ~= nil and t.makes ~= nil and M.is_military_class(t.makes) then
                n = n + 1
            end
        end
    end
    return n
end

-- the epoch pressure share of the pending advance's reserve (0..1000), and the pending epoch's cost (nil: none)
function M.reserve_share(view, memory)
    local C2 = require("classic_lua.civ")
    if memory.rs == nil or memory.rs.pending == nil or T.cheat(memory, "epochs") then
        return 0, nil
    end
    local p = memory.t.personality
    local pressure = C2.epoch_pressure_k(view, memory)
    local span = p.epoch_pressure_full_k - p.epoch_pressure_start_k
    if span <= 0 then
        return 0, nil
    end
    local share = (pressure - p.epoch_pressure_start_k) * 1000 // span
    if share > 1000 then
        share = 1000
    end
    local e = ai.epoch(memory.rs.pending)
    return share, e ~= nil and e.cost or nil
end

-- CanAfford(class or tech cost, n): the stock covers it, and the classic's rules hold
function M.can_afford(view, memory, cost, n, class, is_epoch)
    if is_epoch and T.cheat(memory, "epochs") then
        return true
    end
    for _, r in ipairs(RES) do
        if (cost[r] or 0) * n > view.stock[r] then
            return false
        end
    end
    if is_epoch or T.cheat(memory, "epochs") then
        return true
    end
    local citizen = class == C.citizen or class == C.female_citizen
    local few = M.workers(view, memory) < RECOVERY
    if citizen and few then
        return true
    end
    -- military.prehistoric_army: the Prehistoric army cap unless a stronger enemy army is near
    local cap = view.epoch <= 0 and (memory.t.personality.prehistoric_army or 0) or 0
    if cap > 0 and class ~= nil and M.is_military_class(class) and count_soldiers(view) >= cap
        and not M.army_threat(view, memory) then
        return false
    end
    if not citizen and few then
        local cc = M.cost(C.citizen)
        for _, r in ipairs(RES) do
            local c = (cost[r] or 0) * n
            if c > 0 and (cc[r] or 0) > 0 and view.stock[r] - c < cc[r] then
                return false
            end
        end
    end
    local share, reserved = M.reserve_share(view, memory)
    if reserved == nil or share <= 0 then
        return true
    end
    -- economy.claim_needed: an advance that cannot be reached reserves nothing (a resource short with no gatherer)
    if memory.t.personality.claim_needed then
        local rm = R.state(memory)
        for _, r in ipairs(RES) do
            if (reserved[r] or 0) > view.stock[r] and #rm.lists[r] == 0 then
                return true
            end
        end
    end
    for _, r in ipairs(RES) do
        local c = (cost[r] or 0) * n
        local keep = (reserved[r] or 0) * share // 1000
        if c > 0 and keep > 0 and view.stock[r] - c < keep then
            return false
        end
    end
    return true
end

-- ---------------------------------------------------------------------------------------------------------------
-- GetWantedCitizens: the pop cap x the citizen percentage; the personality's floor: the held sites' capacity up to
-- max_citizen_share of the cap

function M.wanted_citizens(view, memory)
    return R.memo("wanted_citizens", function()
        return M.wanted_citizens_now(view, memory)
    end)
end

function M.wanted_citizens_now(view, memory)
    local cap = view.population_cap
    local wanted = cap * T.b(memory, "citizen_percentage_k") // 1000
    local share = memory.t.personality.max_citizen_share_k or 0
    if share > 0 then
        local E = require("classic_lua.econ")
        local capacity = E.gather_capacity(view, memory, nil, true)
        local ceiling = cap * share // 1000
        if capacity > ceiling then
            capacity = ceiling
        end
        if capacity > wanted then
            wanted = capacity
        end
    end
    return wanted
end

-- ---------------------------------------------------------------------------------------------------------------
-- RequestUnits: a request in its class's epoch list (or more of an existing one), its cost into the needs; a new
-- request first needs a producer, else the cheapest available producer class is ordered

function M.request(view, memory, class, n)
    local um = M.state(memory)
    local t = M.class_tech(view, class)
    if t == nil or n <= 0 then
        return
    end
    local list = um.req[t.epoch + 1]
    if list == nil then
        return
    end
    local cost = M.ledger_cost(class)
    for _, q in ipairs(list) do
        if q.class == class then
            R.add_costs(memory, cost, n)
            q.n = q.n + n
            um.waiting[class] = (um.waiting[class] or 0) + n
            return
        end
    end
    local done = false
    local others = {}
    for _, p in ipairs(t.producers or {}) do
        if M.count(p) > 0 then
            done = true
            break
        end
        if p ~= C.citizen and p ~= C.town_center then
            others[#others + 1] = p
        end
    end
    if not done and #others > 0 then
        local B = require("classic_lua.build")
        table.sort(others, function(a, b)
            local ca, cb = sum_cost(M.cost(a)), sum_cost(M.cost(b))
            if ca ~= cb then
                return ca < cb
            end
            return a < b
        end)
        for _, p in ipairs(others) do
            local pt = M.class_tech(view, p)
            if pt ~= nil and pt.available then
                if R.class_of(p).building then
                    B.queue(view, memory, p, false, false)
                else
                    M.request(view, memory, p, 1)
                end
                done = true
            end
        end
    end
    if done then
        R.add_costs(memory, cost, n)
        list[#list + 1] = {class = class, n = n}
        um.waiting[class] = (um.waiting[class] or 0) + n
    end
end

-- CancelRequest: up to n of a class's unstarted requests dropped (the earliest epochs' first); their costs leave the
-- needs. Returns how many
function M.cancel_request(memory, class, n)
    local um = M.state(memory)
    local left = n
    for e = 1, 15 do
        local list = um.req[e]
        local k = 1
        while left > 0 and k <= #list do
            local q = list[k]
            if q.class == class then
                local take = math.min(left, q.n)
                q.n = q.n - take
                left = left - take
                R.remove_costs(memory, M.ledger_cost(class), take)
                um.waiting[class] = math.max(0, (um.waiting[class] or 0) - take)
                if q.n <= 0 then
                    table.remove(list, k)
                    k = k - 1
                end
            end
            k = k + 1
        end
    end
    return n - left
end

-- DropObsoleteRequests: the requests filed before an epoch whose class went obsolete
function M.drop_obsolete(view, memory, epoch)
    local um = M.state(memory)
    for e = 1, epoch do
        local list = um.req[e]
        for k = #list, 1, -1 do
            local t = M.class_tech(view, list[k].class)
            if t ~= nil and t.obsolete_epoch < epoch then
                table.remove(list, k)
            end
        end
    end
end

function M.has_later_requests(view, memory)
    local um = M.state(memory)
    for e = view.epoch + 2, 15 do
        if #um.req[e] > 0 then
            return true
        end
    end
    return false
end

-- StartProduction: the request's units round robin at the least busy built producers (ai.train one at a time);
-- -> how many were started
local function start_production(view, memory, q, count)
    local units = M.producers(view, q.class, true)
    if #units == 0 then
        return 0
    end
    table.sort(units, function(a, b)
        if (a.queue or 0) ~= (b.queue or 0) then
            return (a.queue or 0) < (b.queue or 0)
        end
        return a.id < b.id
    end)
    local made, j = 0, 1
    for _ = 1, count do
        local code = ai.train(units[j].id, q.class, 1)
        if code ~= "ok" then
            break
        end
        made = made + 1
        j = j + 1
        if j > #units then
            j = 1
        end
    end
    local um = M.state(memory)
    um.waiting[q.class] = math.max(0, (um.waiting[q.class] or 0) - made)
    return made
end

-- the units being made at the own buildings (mNumInProduction: the port counts the queues)
local function in_production_count()
    local n = 0
    for _, id in ipairs(ai.units{category = "building", built = true}) do
        local u = R.unit(id)
        if u ~= nil and u.producing ~= nil and u.producing >= 15 then
            n = n + (u.queue or 0)
        end
    end
    return n
end

-- Update: citizens requested while below the target (production enabled, none requested or being made), then the
-- requests of each epoch to the current one while there is room
function M.update(view, memory)
    local um = M.state(memory)
    local inprod = in_production_count()
    local room = view.population_cap - inprod - view.population
    local requested = M.is_requested(memory, C.citizen) or M.is_requested(memory, C.female_citizen)
    local queued = M.in_production(view, C.citizen) or M.in_production(view, C.female_citizen)
    if room > 0 and um.enabled and not requested and not queued then
        local missing = M.wanted_citizens(view, memory) - M.workers(view, memory)
        if missing > 0 then
            local island = memory.t.island
            if not island then
                if missing > 9 then
                    missing = 10
                end
            elseif missing > 4 then
                missing = 5
            end
            M.request(view, memory, C.citizen, missing)
            requested = true
        end
    end
    -- (the transport first: not ported yet, the islands' ferries are the navy stage)
    if room <= 0 then
        return
    end
    local workers = M.workers(view, memory)
    for e = 1, view.epoch + 1 do
        if room < 1 then
            return
        end
        local list = um.req[e]
        local k = 1
        while k <= #list and room > 0 do
            local q = list[k]
            local t = M.class_tech(view, q.class)
            local size = 1   -- (GetPopulationCost: one for every unit the classic requests)
            -- New (AI_RESEARCH 8.78): citizens wait up to 30 s while a settlement is being filled to become a town
            -- centre, so the batch is shared by both centres (queued a moment earlier, all ten waited at one)
            local civ = memory.civ
            local hold = (q.class == C.citizen or q.class == C.female_citizen) and civ ~= nil and civ.garrison ~= nil
                and view.time < (civ.garrison_t or 0) + 30000
            if t == nil or t.obsolete or q.n == 0 then
                table.remove(list, k)
            elseif hold then
                k = k + 1
            else
                local cost = M.cost(q.class)
                local all = room >= q.n * size
                if not all then
                    if size < room and M.can_afford(view, memory, cost, 1, q.class) then
                        local made = start_production(view, memory, q, 1)
                        room = room - size * made
                        q.n = q.n - made
                    end
                    k = k + 1
                elseif not M.can_afford(view, memory, cost, q.n, q.class) then
                    if M.can_afford(view, memory, cost, 1, q.class) then
                        local skip = false
                        if q.class ~= C.citizen and q.class ~= C.female_citizen then
                            if workers < view.population - workers and requested then
                                skip = true
                            end
                        end
                        if not skip then
                            local made = start_production(view, memory, q, 1)
                            room = room - size * made
                            q.n = q.n - made
                        end
                    end
                    k = k + 1
                else
                    local made = start_production(view, memory, q, q.n)
                    if made == 0 then
                        k = k + 1
                    else
                        room = room - size * made
                        q.n = q.n - made
                        if q.n == 0 then
                            table.remove(list, k)
                        else
                            k = k + 1
                        end
                    end
                end
            end
        end
    end
end

return M
