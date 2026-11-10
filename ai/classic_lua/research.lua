-- EECPResearchManager in Lua (the classic port; ORIGIN in this folder): the research requests in per-epoch lists (an
-- epoch advance first), started at an idle producer (ai.research), the random research every research_frequency
-- seconds (a citizen upgrade first, else a random wanted tech whose producer stands), ResearchCheapest (the cheapest
-- buildings that count toward the epoch's SOL). require("classic_lua.research").
--
-- Memory (memory.rs): ids[epoch + 1] = {tech ids}, candidates, the next random research time, the epoch seen,
-- pending (the epoch queued and not started: EECPPendingEpoch).
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local C = T.C

local M = {}

local function U()
    return require("classic_lua.units")
end

local function B()
    return require("classic_lua.build")
end

function M.state(memory)
    local rs = memory.rs
    if rs == nil then
        rs = {ids = {}, candidates = {}, next_time = 0, epoch = -1, started = false, enabled = false}
        for e = 1, 15 do
            rs.ids[e] = {}
        end
        memory.rs = rs
    end
    return rs
end

local function is_epoch(id)
    return id >= 0 and id < 15
end

-- the pending epoch: the first epoch id in the lists (EECPPendingEpoch)
local function update_pending(rs)
    rs.pending = nil
    for e = 1, 15 do
        for _, id in ipairs(rs.ids[e]) do
            if is_epoch(id) then
                rs.pending = id
                return
            end
        end
    end
end

-- IsWanted: no epoch or unit tech, not 0x7eb, researched at a producer that is no citizen and not made by an epoch
local function is_wanted(t)
    if t == nil or t.id <= 0 or t.makes ~= nil or t.id == 0x7eb then
        return false
    end
    return t.plain_producer == true
end
M.is_wanted = is_wanted

local function tech(view, id)
    return U().tree(view).byId[id]
end

-- Research(id): a wanted tech into its epoch's list (an epoch first), its producer ordered when none stands, its cost
-- into the needs (an epoch's too unless the epochs cheat makes it free)
function M.research(view, memory, id)
    local rs = M.state(memory)
    local t = tech(view, id)
    if is_epoch(id) then
        local e = ai.epoch(id)
        if e == nil then
            return
        end
        t = {id = id, epoch = id - 1, producers = {C.town_center, C.capitol}, cost = e.cost}
    elseif not is_wanted(t) then
        return
    end
    local done = false
    for _, p in ipairs(t.producers or {}) do
        if U().count(p) > 0 then
            done = true
            break
        end
    end
    if not done then
        for _, p in ipairs(t.producers or {}) do
            if p ~= C.town_center and p ~= C.capitol then
                if R.class_of(p).building then
                    B().queue(view, memory, p, false, false)
                else
                    U().request(view, memory, p, 1)
                end
                done = true
                break
            end
        end
        if not done then
            return
        end
    end
    local e = t.epoch + 1
    if is_epoch(id) then
        table.insert(rs.ids[e], 1, id)
    else
        rs.ids[e][#rs.ids[e] + 1] = id
    end
    if not is_epoch(id) or not T.cheat(memory, "epochs") then
        R.add_costs(memory, t.cost or {}, 1)
    end
    update_pending(rs)
end

-- TryResearch(id): at an idle built producer (with several, the first idle one) when the player can afford it; with
-- no producer: an epoch advance is rescheduled, a building or unit ordered
-- the research at a producer; an epoch goes to the front of the town centre's queue (QueueResearch(id, true)).
-- DEVIATION: a human's order queues at the back, so the port cancels the citizens queued behind the current one
-- (refunded), orders the epoch and queues them again: the epoch starts after the current citizen, as the classic's
local function order(view, id, uid, epoch)
    if epoch then
        local u = R.unit(uid)
        local behind = u ~= nil and (u.queue or 0) - (u.producing ~= nil and 1 or 0) or 0
        if behind > 0 then
            for _ = 1, behind do
                ai.cancel(uid, C.citizen)
            end
        end
        local code = ai.research(uid, id)
        local left = behind
        while left > 0 do
            local n = left > 5 and 5 or left
            ai.train(uid, C.citizen, n)
            left = left - n
        end
        R.touch(uid)
        return code == "ok"
    end
    local code = ai.research(uid, id)
    R.touch(uid)
    return code == "ok"
end

local function try_research(view, memory, id)
    local t
    local epoch = is_epoch(id)
    if epoch then
        local e = ai.epoch(id)
        if e == nil or not e.available then
            return false
        end
        t = {id = id, producers = {C.town_center, C.capitol}, cost = e.cost, available = true}
    else
        t = tech(view, id)
        if t == nil or not t.available then
            return false
        end
    end
    local U_ = U()
    for _, p in ipairs(t.producers or {}) do
        if p ~= C.citizen and p ~= C.female_citizen then
            local units = R.of_type(p)
            if #units == 0 then
                -- (a class never had: skipped, as GetUnits' NULL; the epoch's lost producer: civ.lua)
            elseif #units > 1 then
                for _, uid in ipairs(units) do
                    local u = R.unit(uid)
                    if u ~= nil and u.built and u.action == "idle" then
                        if not U_.can_afford(view, memory, t.cost or {}, 1, nil, epoch) then
                            return false
                        end
                        return order(view, id, uid, epoch)
                    end
                end
            else
                local u = R.unit(units[1])
                if u ~= nil and u.built then
                    if not U_.can_afford(view, memory, t.cost or {}, 1, nil, epoch) then
                        return false
                    end
                    return order(view, id, units[1], epoch)
                end
            end
        elseif t.makes ~= nil then
            B().queue(view, memory, t.makes, false, false)
            return true
        end
    end
    return false
end

-- BuildCandidates: the wanted techs of the current and earlier epochs not yet researched
local function build_candidates(view, memory)
    local rs = M.state(memory)
    rs.candidates = {}
    for _, t in ipairs(ai.techs{epoch = view.epoch}) do
        if is_wanted(t) and not t.researched then
            rs.candidates[#rs.candidates + 1] = t.id
        end
    end
end

-- ChooseResearch: a producible citizen upgrade, else a random wanted tech whose producer stands; started when its
-- cost is below the stock
local function choose_research(view, memory)
    local rs = M.state(memory)
    if not rs.enabled then
        return
    end
    local found
    for _, id in ipairs(rs.candidates) do
        local t = tech(view, id)
        if t ~= nil and t.producible and t.citizen_upgrade
            and U().can_afford(view, memory, t.cost, 1, nil) then
            found = t
            break
        end
    end
    if found == nil then
        -- EEAIRandomShuffle (DEVIATION: the script's own stream)
        local c = rs.candidates
        for k = #c, 2, -1 do
            local j = ai.random(1, k)
            c[k], c[j] = c[j], c[k]
        end
        for _, id in ipairs(c) do
            local t = tech(view, id)
            if t ~= nil and t.producible and U().can_afford(view, memory, t.cost, 1, nil) then
                for _, p in ipairs(t.producers or {}) do
                    if U().count(p) > 0 then
                        found = t
                        break
                    end
                end
            end
            if found ~= nil then
                break
            end
        end
        if found == nil then
            return
        end
    end
    for _, r in ipairs(R.RES) do
        if (found.cost[r] or 0) >= view.stock[r] then
            return
        end
    end
    M.research(view, memory, found.id)
    for k, id in ipairs(rs.candidates) do
        if id == found.id then
            table.remove(rs.candidates, k)
            break
        end
    end
end

-- ResearchCheapest(n): the cheapest affordable buildings that count toward the epoch and have a site (no town
-- centre, capitol, farm, settlement or granary; no dock without water), repeating the list when short; with none
-- affordable, the same without the cost. (With none at all the classic adds to the next epoch's SOL count itself:
-- not possible for a script, logged)
function M.research_cheapest(view, memory, n)
    local skip = {[C.town_center] = true, [C.capitol] = true, [C.farm] = true, [C.settlement] = true,
                  [C.granary] = true, [C.robotic_farm] = true}
    if memory.t.waterless then
        skip[C.dock] = true
        skip[C.navy_yard] = true
    end
    local list = ai.techs{epoch = view.epoch}
    local entries = {}
    for pass = 1, 2 do
        for _, t in ipairs(list) do
            if t.makes ~= nil and not skip[t.makes] and t.counts_toward_epoch and t.kind ~= 4 and t.kind ~= 6
                and t.producible then   -- (ResearchManager::CanAfford adds the cost to the stock first)
                local x = B().find_site(view, memory, t.makes)
                if x ~= nil then
                    local total = 0
                    if pass == 1 then
                        for _, r in ipairs(R.RES) do
                            total = total + (t.cost[r] or 0)
                        end
                    end
                    entries[#entries + 1] = {total = total, t = t}
                end
            end
        end
        if #entries > 0 then
            break
        end
    end
    if #entries == 0 then
        print(string.format("classic_lua sol none epoch=%d n=%d", view.epoch, n))
        return
    end
    table.sort(entries, function(a, b)
        if a.total ~= b.total then
            return a.total < b.total
        end
        return a.t.id < b.t.id
    end)
    local chosen, again = {}, false
    for _, e in ipairs(entries) do
        if #chosen >= n then
            break
        end
        if not e.t.researched then
            chosen[#chosen + 1] = e.t
        end
    end
    local m = 1
    while #chosen < n do
        chosen[#chosen + 1] = entries[m].t
        again = true
        m = m + 1
        if m > #entries then
            m = 1
        end
    end
    for _, t in ipairs(chosen) do
        if R.class_of(t.makes).building then
            B().queue(view, memory, t.makes, again, false)
        else
            U().request(view, memory, t.makes, 1)
        end
    end
end

function M.has_later_ids(view, memory)
    local rs = M.state(memory)
    for e = rs.epoch + 2, 15 do
        if #rs.ids[e] > 0 then
            return true
        end
    end
    return false
end

-- Update: the first queued tech that starts (the candidates rebuilt on a new epoch); the random research when due
function M.update(view, memory)
    local rs = M.state(memory)
    if not rs.started then
        local f = T.b(memory, "research_frequency") * T.research_frequency_modifier_k(memory) // 1000
        rs.next_time = view.time + f * 1000
        rs.started = true
    end
    if rs.epoch ~= view.epoch then
        rs.epoch = view.epoch
        build_candidates(view, memory)
    end
    local done = false
    for e = 1, rs.epoch + 1 do
        if done then
            break
        end
        local list = rs.ids[e]
        for k = 1, #list do
            if try_research(view, memory, list[k]) then
                table.remove(list, k)
                done = true
                break
            end
        end
    end
    update_pending(rs)
    if view.time > rs.next_time then
        choose_research(view, memory)
        local f = T.b(memory, "research_frequency") * T.research_frequency_modifier_k(memory) // 1000
        rs.next_time = view.time + f * 1000
    end
end

-- EpochReached: the next advance is scheduled
function M.epoch_reached(view, memory)
    memory.civ.advancing = false
    memory.civ.schedule_advance = true
end

return M
