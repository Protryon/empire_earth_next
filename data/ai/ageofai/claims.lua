-- Age of AI port, ORDERS BY PRIORITY (upstream dll/ai/claims.c; UPSTREAM has the map and the licence, GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.claims").
--
-- The playtester (2026-11-19): "generaly the ai units seem undecisive, ships send to a dock for repair drive off
-- again just to be called back again in a loop / i think we need locking order with priorethy, a unit with a task
-- cant get another order unless its of a heigher priorety (get away from the storm would override stay to heal for
-- example)", and "some orders should maybe work with the unit stances, repairing units could get stand your ground".
--
-- A unit's CLAIM: who holds it, at which priority, until when. A manager asks before it orders:
--   M.take (cl_take)  claims the unit (free, its own already, or held at a lower priority by another: taken over);
--                     refuses when another holds it at its priority or a higher one.
--   M.may (cl_may)    asks only, changes nothing.
--   M.stance          the claimed unit's stance while the claim lasts; the stance it had before comes back when the
--                     claim ends (M.end_) or expires.
--   M.end_ (cl_end)   the holder gives the unit up; the stance from before returns.
--   M.owner (cl_owner->the entry's who), M.on (cl_on minding the bit mask), M.live (cl_live), M.bit (cl_bit),
--   M.sweep, M.log (the minute line), M.clear.
-- Priorities, most important first (cl_prio): SURVIVE the unit itself lives, REPAIR it heals, CARRY it carries
-- soldiers/cargo, HOLD holding the ground, TASK a task, ROUTINE the everyday. A claim may be taken over only by a
-- strictly more important one.
-- Durations (upstream's): a repair's 120 s, renewed while it goes; survival 15 s; a citizen sent off by its safety
-- (or into a fight as militia, or to build HOLDING THE GROUND) 30 s. A task passes its own time.
-- The bits (cl_on, upstream's [autotest] v_claims default 31): 1 ships and aircraft, 2 citizens, 4 all others,
-- 8 a ferry's passengers, 16 the muster's soldiers. settings.claims (default 31) is the mask.
--
-- Deviations and gaps:
--   - The engine's ids are stable and never reused (docs/AI_LUA_UNITS.md §6), so an id keys the claim; a unit that
--     died or was swapped away is found by M.live's ai.unit test (the engine forwards swapped ids itself).
--   - Upstream's "the player's own orders come first" (pc_held) does not exist here: a human cannot order a computer
--     player's units, so the engine's own ownership test is all there is.
--   - Upstream hashes 4096 slots; here the table is keyed by id, so no collision and no table-full case.
--   - The log's per-pair refusal matrix is kept (who asked by who held it), the counts per second of game time are
--     as upstream's.
-- Wired from the modules that order units (fleet.c/squadrons.c/cover.c/stations.c/counterfire.c/defence.c's muster/
-- build.c's builders/work.c, citizens' safety; the port's army.lua/tactics.lua/safety.lua/detach.lua/navy.lua could
-- ask it): each calls M.take before its own order and M.end_ when its task is over.
local S = require("ageofai.settings")
local M = {}

-- priorities (upstream's CLP_*)
local SURVIVE, REPAIR, CARRY, HOLD, TASK, ROUTINE = 1, 2, 3, 4, 5, 6
M.PRIO = {survive = SURVIVE, repair = REPAIR, carry = CARRY, hold = HOLD, task = TASK, routine = ROUTINE}

-- who (upstream's CLW_*; the order is the numbers, the names the log's)
local WHO = {"-", "storm", "danger zone", "repair", "squadron", "fleet", "citizens' safety", "militia",
             "holding the ground", "work", "builders", "stations", "healing", "cover", "counter-fire",
             "counter-siege", "ferry", "exploring", "muster", "lone bomber runs", "fighter sweeps", "the player"}
M.names = WHO
M.WHO = {none = 1, storm = 2, flee = 3, repair = 4, squadron = 5, fleet = 6, safety = 7, militia = 8, hold = 9,
         work = 10, build = 11, station = 12, heal = 13, cover = 14, counter = 15, csiege = 16, ferry = 17,
         explore = 18, muster = 19, bombers = 20, fighters = 21, player = 22}

-- the upstream durations, for the callers that have no time of their own
M.DUR = {survive = 15000, repair = 120000, citizen = 30000, task = 60000, rout = 120000}

local CL_SWEEP = 5000      -- the expired claims are swept this often
local CL_DEFAULT = 60000   -- a claim given no time of its own

local function state(memory)
    memory.claims = memory.claims or {by = {}, taken = 0, over = 0, refused = {}, log_t = 0}
    return memory.claims
end

-- the mask (settings.claims, 31 upstream: ships 1, citizens 2, the rest 4, a ferry's 8, the muster's 16)
local function mask()
    return S.claims or 31
end

local function has(bits, bit)
    return (bits & bit) ~= 0
end

-- the unit's own category -> its bit (cl_bit): a ship, boat or aircraft 1; a citizen 2; anything else 4
function M.bit(c)
    if c == nil then
        return 4
    end
    local k = c.category
    if k == "citizen" then
        return 2
    elseif k == "ship" or k == "boat" or k == "air" or k == "transport" then
        return 1
    end
    return 4
end

-- claims apply to this unit at all (cl_on)
function M.on(memory, c)
    if c == nil then
        return false
    end
    return has(mask(), M.bit(c))
end

function M.live(view, entry)
    if entry == nil or entry.id == nil or entry.until_t == nil or view.time >= entry.until_t then
        return false
    end
    return ai.unit(entry.id) ~= nil
end

-- the entry holding the unit now, or nil
local function find(st, id)
    return st.by[id]
end

-- the stance from before goes back and the claim is dropped (cl_free)
function M.free(view, memory, entry)
    if entry == nil then
        return
    end
    if entry.s0 ~= nil and entry.s0 >= 0 then
        local c = ai.unit(entry.id)
        if c ~= nil and c.stance ~= nil and c.stance ~= entry.s0 then
            ai.stance({entry.id}, entry.s0)
        end
    end
    entry.id, entry.prio, entry.who, entry.until_t, entry.s0 = nil, nil, nil, nil, nil
end

-- 1: who may order the unit at prio now (and holds it until now + dur); 0: another holds it at that priority or a
-- higher one (cl_take). c: the unit's record (ai.unit) or nil to let the call read it.
function M.take(view, memory, id, prio, who, dur, c)
    if id == nil or prio == nil or who == nil then
        return false
    end
    c = c or ai.unit(id)
    if c == nil then
        return false
    end
    local st = state(memory)
    if not M.on(memory, c) then
        return true
    end
    local now = view.time
    local e = find(st, id)
    if e ~= nil and not M.live(view, e) then
        M.free(view, memory, e)
        e = nil
    end
    if e ~= nil then
        if e.who ~= who and prio >= e.prio then
            local by = st.refused[who]
            by = by or {}
            st.refused[who] = by
            by[e.who] = (by[e.who] or 0) + 1
            return false
        elseif e.who ~= who then
            st.over = st.over + 1
        end
    else
        e = {id = id, s0 = -1}
        st.by[id] = e
        st.taken = st.taken + 1
    end
    e.prio = prio
    e.who = who
    e.until_t = now + (dur or CL_DEFAULT)
    return true
end

-- asks only: 1 when who may order the unit at prio now (cl_may)
function M.may(view, memory, id, prio, who, c)
    if id == nil or prio == nil or who == nil then
        return true
    end
    c = c or ai.unit(id)
    if c == nil then
        return false
    end
    local st = state(memory)
    if not M.on(memory, c) then
        return true
    end
    local e = find(st, id)
    if e == nil or not M.live(view, e) or e.who == who or prio < e.prio then
        return true
    end
    local by = st.refused[who]
    by = by or {}
    st.refused[who] = by
    by[e.who] = (by[e.who] or 0) + 1
    return false
end

-- the claimed unit's stance while the claim lasts; the first one from before is the one that comes back (cl_stance)
function M.stance(memory, id, stance, c)
    local st = state(memory)
    local e = find(st, id)
    if e == nil or stance == nil then
        return false
    end
    c = c or ai.unit(id)
    if c == nil or c.stance == stance then
        return false
    end
    if e.s0 < 0 then
        e.s0 = c.stance or 0
    end
    return ai.stance({id}, stance) == "ok"
end

-- the holder gives the unit up (cl_end); another holder's claim stays
function M.end_(memory, id, who)
    local st = state(memory)
    local e = find(st, id)
    if e ~= nil and (who == nil or e.who == who) then
        M.free(nil, memory, e)
        st.by[id] = nil
    end
end

-- who holds the unit now (the holder's number), or nil (cl_owner)
function M.owner(memory, id)
    local st = memory.claims
    local e = st ~= nil and st.by[id] or nil
    return e ~= nil and e.who or nil
end

-- the expired claims: their stance back, the place free (cl_sweep)
function M.sweep(view, memory)
    local st = state(memory)
    if st.sweep_t ~= nil and view.time >= st.sweep_t and view.time - st.sweep_t < CL_SWEEP then
        return
    end
    if view.time < (st.sweep_t or 0) then
        st.sweep_t = 0   -- a game before
    end
    st.sweep_t = view.time > 0 and view.time or 1
    for id, e in pairs(st.by) do
        if not M.live(view, e) then
            M.free(view, memory, e)
            st.by[id] = nil
        end
    end
end

local function who_name(k)
    return WHO[k] or tostring(k)
end

-- "# claims:" each minute: taken, taken over, and the orders refused by who asked and who held the unit
function M.log(view, memory)
    local st = memory.claims
    if st == nil or (st.log_t ~= nil and view.time >= st.log_t and view.time - st.log_t < 60000) then
        return
    end
    st.log_t = view.time > 0 and view.time or 1
    local parts = {}
    for a, by in pairs(st.refused) do
        for h, n in pairs(by) do
            parts[#parts + 1] = string.format(" %s by %s %d", who_name(a), who_name(h), n)
        end
    end
    print(string.format("claims taken=%d over=%d refused:%s", st.taken, st.over,
        #parts > 0 and table.concat(parts, ",") or " none"))
end

-- a new game (claims_new_game): nothing to give back
function M.clear(memory)
    memory.claims = nil
end

return M
