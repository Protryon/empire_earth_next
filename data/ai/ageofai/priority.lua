-- Age of AI port, THE PRIORITY LIST (upstream dll/ai/priority.c; UPSTREAM has the map and the licence: GPL-3.0,
-- Grille and the Age of AI contributors). require("ageofai.priority").
--
-- The player (2026-11-09): "we should really polish the priority system to handle such things"; (2026-11-08): "a lower
-- tier only gets produced if the resources can pay for the higher tiers". One row for each thing the AI saves for,
-- the most important first. Something new to save for is a row here and nothing else: its function says what it
-- still needs, skip / only say who has to leave that money alone. Every spender asks this list: a unit order through
-- the budget (econ.lua M.accounts: the army's avail, the citizens' civil), a building through M.refuses (econ.lua's
-- buildings, tactics.lua's fortress, towns.lua's projects), a priority order (the siege, the bombers, a landing's
-- transports: budget.order), a warship (budget.ship: the stock but the rows marked apart). The rows' order and
-- conditions are upstream's; what each row holds comes from the module that saves (written into the memory the
-- step before, as the port's towns_reserve and navy_hold already did):
--   epoch     econ.lua M.reserve (memory.eco.epoch_reserve: the next epoch's cost, spend_reserve's horizon)
--   farms     econ.lua farms (memory.eco.farm_hold: the first two silos' 525 wood while it is their turn, farm_first;
--             memory.eco.fields_open: the next fields' 50 wood each up to 2 for units)
--   wants     A PERSONALITY'S LIST: not applicable (the port has no personas, voice/persona.c is not ported) - the
--             row stays, empty
--   landing   navy.lua ships_phase (memory.navy.landing_hold: the missing transports' cost in phase 1)
--   outpost   towns.lua (memory.eco.towns_reserve: towns_reserve; memory.eco.expand_hold: the outpost's Settlement
--             and tower; a building waits for the outpost only with spend_tiers)
--   fortress  tactics.lua bunkers (memory.army.bunker_wood: the Fortress's wood it cannot pay yet)
--   forest    econ.lua settlements (memory.eco.wd_want_t: a Settlement at the forest edge wanted within 2 minutes)
--   upgrades  econ.c paid_upgrades' hold: not applicable (the engine gives a computer player the epoch's line upgrades
--             with the epoch; nothing is paid) - the row stays, empty
--   navy      navy.lua hold (memory.eco.navy_hold: the account, or the warship the mix waits for)
--   special   tactics.lua siege (memory.army.special: the siege type and count wanted within SP_KEEP; the AA and the
--             shore guns of defence.c are not ported)
-- A refused building: upstream's build manager puts the entry at the back of its queue and ends the round; here the
-- module that wanted it simply does not order it this pass (the next pass asks again).
-- DEVIATION (the engine): the coarse verbs (train, build) are checked by the engine against the `reserve` verb's
-- amounts (econ.lua sets it to the epoch's + the outpost's - the navy's, so a warship can spend the account): a
-- building therefore waits for the epoch too, where upstream's buildings (the epoch row skips S_BUILD) spent it.
local V = require("lib.view")
local S = require("ageofai.settings")
local M = {}

-- the spenders (priority.c S_*)
M.S_MIL = 1          -- a military unit
M.S_SHIP = 2         -- a ship
M.S_REQ = 4          -- a unit the next epoch requires (not used by the port: the SOL is buildings)
M.S_ORDER = 8        -- a priority order: siege, a landing's fleet and transports, an outpost's transport, the bombers
M.S_TR_ECON = 16     -- transports_economy: the game's transport for its crossings (off in the port)
M.S_PLAN = 32        -- army_plan's look at what lies unused (army.c FLOAT; not ported)
M.S_BUILD = 64       -- a building
M.S_BUILD_ECON = 128 -- ... that is the economy's own: a town, food, a house, a dock
M.S_WANTED = 256     -- ... that is the wanted entry itself (no personas: never set)
-- the port's own: upstream's globals g_landing_order / g_expand_order (set around that module's own transport
-- order: its reserve is not held from it; the outpost's transport before every reserve)
M.S_LANDING_ORDER = 512
M.S_EXPAND_ORDER = 1024

local SP_KEEP = 120000   -- econ.c SPECIAL ORDERS: a want counts this long after it was last asked
local ECON_BUILDINGS = {b_town_center = true, b_capitol = true, b_settlement = true, b_granary = true,
                        b_granary_improved = true, b_farm = true, b_house = true, b_dock = true}

local function has(s, mask)
    return (s & mask) ~= 0
end

local function add_cost(view, type, n, r)
    local c = V.cost(view, type)
    if c == nil or n <= 0 then
        return
    end
    for _, k in ipairs(V.resources) do
        r[k] = r[k] + c.cost[k] * n
    end
end

local function add_map(m, r)
    if m == nil then
        return
    end
    for _, k in ipairs(V.resources) do
        r[k] = r[k] + (m[k] or 0)
    end
end

-- the rows: {name, hold(view, memory, spender, r), skip, only, apart}
local ROWS = {
    {name = "epoch", skip = M.S_BUILD, only = 0, apart = false,
     hold = function(view, memory, s, r)
         add_map((memory.eco or {}).epoch_reserve, r)
     end},
    -- units: the next fields' wood, or the silo's whole sum while it has its turn; a building only yields to the silo
    {name = "farms", skip = M.S_TR_ECON | M.S_BUILD_ECON, only = 0, apart = false,
     hold = function(view, memory, s, r)
         local eco = memory.eco or {}
         local fh = eco.farm_hold or 0
         if has(s, M.S_BUILD) then
             r.wood = r.wood + fh
         else
             local open = math.min(eco.fields_open or 0, 2)
             r.wood = r.wood + math.max(fh, 50 * open)   -- field_wood
         end
     end},
    {name = "wants", skip = M.S_TR_ECON | M.S_BUILD_ECON | M.S_WANTED, only = 0, apart = false,
     hold = function(view, memory, s, r) end},   -- no personality's list in the port
    {name = "landing", skip = M.S_TR_ECON | M.S_BUILD, only = 0, apart = false,
     hold = function(view, memory, s, r)
         -- landing_reserve: nothing during the landing's own order or a priority order (its phase 5 part: the
         -- bombardment's fleet is not ported)
         if has(s, M.S_LANDING_ORDER) then
             return
         end
         add_map((memory.navy or {}).landing_hold, r)
     end},
    {name = "outpost", skip = M.S_TR_ECON | M.S_PLAN, only = 0, apart = false,
     hold = function(view, memory, s, r)
         local eco = memory.eco or {}
         if not has(s, M.S_BUILD) then
             add_map(eco.towns_reserve, r)   -- expand_reserve: towns_reserve ...
             if not has(s, M.S_LANDING_ORDER) then
                 add_map(eco.expand_hold, r)   -- ... and expand_hold (its own transport and a landing's spend it)
             end
         elseif S.spend_tiers then
             add_map(eco.expand_hold, r)
         end
     end},
    {name = "fortress", skip = M.S_TR_ECON | M.S_BUILD, only = 0, apart = false,
     hold = function(view, memory, s, r)
         r.wood = r.wood + ((memory.army or {}).bunker_wood or 0)
     end},
    {name = "forest", skip = M.S_TR_ECON | M.S_PLAN | M.S_BUILD, only = 0, apart = false,
     hold = function(view, memory, s, r)
         local t = (memory.eco or {}).wd_want_t or 0
         if t > 0 and view.time >= t and view.time - t <= 120000 then
             add_cost(view, "b_settlement", 1, r)   -- mines.c wood_reserve
         end
     end},
    {name = "upgrades", skip = M.S_BUILD, only = M.S_MIL | M.S_PLAN, apart = false,
     hold = function(view, memory, s, r) end},   -- paid_upgrades: the engine's upgrades come with the epoch
    {name = "navy", skip = M.S_SHIP | M.S_REQ | M.S_ORDER | M.S_TR_ECON | M.S_BUILD, only = 0, apart = false,
     hold = function(view, memory, s, r)
         add_map((memory.eco or {}).navy_hold, r)
     end},
    {name = "special", skip = M.S_REQ | M.S_ORDER | M.S_BUILD, only = M.S_MIL, apart = true,
     hold = function(view, memory, s, r)
         local sp = (memory.army or {}).special
         if sp ~= nil and (sp.n or 0) > 0 and sp.t ~= nil and view.time >= sp.t and view.time - sp.t <= SP_KEEP then
             add_cost(view, sp.type, sp.n, r)
         end
     end},
}
M.rows = ROWS

local function applies(row, s)
    return not has(s, row.skip) and (row.only == 0 or has(s, row.only))
end

local function zero()
    local r = {}
    for _, k in ipairs(V.resources) do
        r[k] = 0
    end
    return r
end

-- what the rows ask this spender to leave alone, added up (prio_hold) -> reserve, apart (the rows marked apart:
-- a warship with a target sees the stock but these; the caller adds them to the reserve of every other spender)
function M.hold(view, memory, spender)
    local reserve, apart = zero(), zero()
    for _, row in ipairs(ROWS) do
        if applies(row, spender) then
            row.hold(view, memory, spender, row.apart and apart or reserve)
        end
    end
    return reserve, apart
end

-- the stock this spender may see: the stock less what the rows hold from it (train_budget's visible before the
-- accounts), never below 0
function M.visible(view, memory, spender)
    local reserve, apart = M.hold(view, memory, spender)
    local v = {}
    for _, k in ipairs(V.resources) do
        v[k] = math.max(math.min(view.stock[k] - reserve[k] - apart[k], view.stock[k]), 0)
    end
    return v
end

-- BUILDINGS (prio_refuses): a building that would dip into what a row above it needs waits; per resource (a tower of
-- stone goes up while wood is held). -> the row's name that holds it, or nil. Traced once a minute at most.
function M.refuses(view, memory, type, extra)
    local eco = memory.eco or {}
    if not S.spend_tiers and (eco.farm_hold or 0) == 0 then
        return nil   -- (no wants: the personality's list is not ported)
    end
    local c = V.cost(view, type)
    if c == nil then
        return nil
    end
    local s = M.S_BUILD | (extra or 0)
    if ECON_BUILDINGS[type] then
        s = s | M.S_BUILD_ECON
    end
    local hold = zero()
    local held, by = nil, nil
    for _, row in ipairs(ROWS) do
        if applies(row, s) then
            row.hold(view, memory, s, hold)
            for _, k in ipairs(V.resources) do
                if c.cost[k] > 0 and hold[k] > 0 and view.stock[k] - c.cost[k] < hold[k] then
                    held, by = k, row.name
                    break
                end
            end
        end
        if held ~= nil then
            break
        end
    end
    if held == nil then
        return nil
    end
    memory.prio = memory.prio or {}
    local p = memory.prio
    p.refused = (p.refused or 0) + 1
    if view.time - (p.log_t or -100000) >= 60000 then
        p.log_t = view.time
        print(string.format("priority refused type=%s row=%s res=%s stock=%d held=%d cost=%d n=%d", type, by, held,
            view.stock[held], hold[held], c.cost[held], p.refused))
    end
    return by
end

return M
