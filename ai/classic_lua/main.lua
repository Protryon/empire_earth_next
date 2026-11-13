-- The classic computer player in Lua: a port of the original's managers (src/Empire Earth/AI Thread/EEComputerPlayer
-- and the EECP* managers) on the per-unit interface (docs/AI_LUA_UNITS.md). ORIGIN in this folder maps every C++ file
-- to its module here (PORTED / PARTLY / ENGINE / NOT STARTED) and lists the deviations. require("classic_lua.main").
--
-- The personality and the difficulty come from the rules data, as for the C++ classic: ai.tuning() (aipersonalities
-- .yaml with EE_AI_PERSONALITY, gameoptions.yaml's ai_cheats with EE_AI_CHEATS, the dbcpbehavior values). The cheats
-- that change the engine (cost, reveal, extra, popband, epochs' payment, retire) act on this seat in C++ as on a
-- classic seat; the decisions they change (the epoch timer's catch-up, the free epoch's affordability) are here.
--
-- EEComputerPlayer::Update's order: the start (the managers enabled, the attack and expansion times, the resource
-- centres grouped), then every game second: the combat state, EnableManagers on its timer, FlagGroup on the
-- expansion timer, the civilization manager (units, resources, builds, research, the epoch, the building counts,
-- repairs, the garrisons, the houses), the standing army every 30 s.
local T = require("classic_lua.tune")
local R = require("classic_lua.resource")
local U = require("classic_lua.units")
local CIV = require("classic_lua.civ")
local RS = require("classic_lua.research")
local MIL = require("classic_lua.militia")
local ARMY = require("classic_lua.army")
local EXPLORE = require("classic_lua.explore")
local C = T.C

local M = {}

-- ---------------------------------------------------------------------------------------------------------------
-- EnableManagers: the managers that run, by the combat state's priority list, each only with its stock threshold
-- (the resource's stock for the manager kind: production 2 = food ... CanEnableManager), the "other" managers
-- shuffled and appended only behind a priority one; at most max_enabled_managers + 1

local RES_INDEX = {[2] = "food", [3] = "wood", [4] = "stone", [5] = "gold", [6] = "iron"}

local function can_enable(view, memory, kind, r)
    local b = memory.t.behavior
    local v = view.stock[RES_INDEX[r]]
    local th = ({civilization = b.enable_stock_civilization, resource = b.enable_stock_resource,
                 research = b.enable_stock_research, production = b.enable_stock_production,
                 build = b.enable_stock_build, five = b.enable_stock_5, defense = b.enable_stock_defense,
                 exploration = b.enable_stock_exploration, combat = b.enable_stock_combat,
                 morale = b.enable_stock_build})[kind] or 0
    return v >= th
end

local function enable_managers(view, memory)
    local cp = memory.cp
    local b = memory.t.behavior
    local now = view.time
    local wait = cp.next_attack > now and cp.next_attack - now or 0
    local civ = CIV.state(memory)
    local adv_wait = civ.next_advance > now and civ.next_advance - now or 0
    local adv_limit = b.manager_epoch_advance_wait * 1000
    local wait_limit = b.manager_attack_wait * 1000
    local limit = b.max_enabled_managers + 1
    civ.enabled_advance = false
    R.state(memory).settle_enabled = false
    RS.state(memory).enabled = false
    U.state(memory).enabled = false
    civ.counts_enabled = false
    civ.morale_enabled = false
    cp.defense_enabled = false
    cp.explore_enabled = false
    cp.improve_enabled = false
    local prio, other = {}, {}
    local function add(list, v)
        list[#list + 1] = v
    end
    local defense = (can_enable(view, memory, "defense", 2) and can_enable(view, memory, "defense", 5)
        and can_enable(view, memory, "defense", 6)) or cp.building_damaged or cp.citizen_damaged
    local explore = can_enable(view, memory, "exploration", 2) and can_enable(view, memory, "exploration", 5)
        and can_enable(view, memory, "exploration", 6)
    local build = can_enable(view, memory, "build", 3) and can_enable(view, memory, "build", 4)
    if memory.t.all_managers then
        prio = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9}
        limit = #prio
    else
        local s = cp.combat_state
        if s == 0 then
            if wait > wait_limit and can_enable(view, memory, "production", 2) then add(prio, 3) end
            if can_enable(view, memory, "resource", 3) then add(prio, 1) end
            if adv_wait < adv_limit and wait > wait_limit then add(prio, 0) end
            if defense then add(prio, 6) end
            if build then add(prio, 4) end
            if explore then add(prio, 7) end
            if can_enable(view, memory, "morale", 3) then add(other, 9) end
            if can_enable(view, memory, "research", 5) then add(other, 2) end
            if can_enable(view, memory, "combat", 5) then add(other, 8) end
        elseif s == 1 then
            if adv_wait < adv_limit and wait > wait_limit then add(prio, 0) end
            if can_enable(view, memory, "research", 5) then add(prio, 2) end
            if can_enable(view, memory, "resource", 3) then add(prio, 1) end
            if wait > wait_limit and can_enable(view, memory, "production", 2) then add(prio, 3) end
            if build then add(prio, 4) end
            if can_enable(view, memory, "morale", 3) then add(prio, 9) end
            if defense then add(other, 6) end
            if explore then add(other, 7) end
            if can_enable(view, memory, "combat", 5) then add(other, 8) end
        else
            if can_enable(view, memory, "combat", 5) then add(prio, 8) end
            if defense then add(prio, 6) end
            if build then add(prio, 4) end
            if wait > wait_limit and can_enable(view, memory, "production", 2) then add(prio, 3) end
            if can_enable(view, memory, "resource", 3) then add(prio, 1) end
            if can_enable(view, memory, "morale", 3) then add(prio, 9) end
            if can_enable(view, memory, "research", 5) then add(other, 2) end
            if adv_wait < adv_limit and wait > wait_limit then add(other, 0) end
            if explore then add(other, 7) end
        end
        if #prio > 0 and #other > 0 then
            -- EEAIRandomShuffle (DEVIATION: the script's own stream)
            for k = #other, 2, -1 do
                local j = ai.random(1, k)
                other[k], other[j] = other[j], other[k]
            end
            for _, v in ipairs(other) do
                add(prio, v)
            end
        end
    end
    for n, v in ipairs(prio) do
        if n > limit then
            break
        end
        if v == 0 then civ.enabled_advance = true
        elseif v == 1 then R.state(memory).settle_enabled = true
        elseif v == 2 then RS.state(memory).enabled = true
        elseif v == 3 then U.state(memory).enabled = true
        elseif v == 4 then civ.counts_enabled = true
        elseif v == 6 then cp.defense_enabled = true
        elseif v == 7 then cp.explore_enabled = true
        elseif v == 8 then cp.improve_enabled = true
        elseif v == 9 then civ.morale_enabled = true
        end
    end
    cp.next_enable = now + b.enable_interval_k   -- SetEnableTime: mEnableInterval seconds
end

-- SetNextAttackTime: the first attack after the initial delay (minutes, + per epoch of the game's span) x the
-- difficulty's delay modifier, clamped; later ones after the frequency
local function set_next_attack(view, memory)
    local cp = memory.cp
    local t = memory.t
    local b = t.behavior
    local epochs = t.epoch_end - t.epoch_start + 1
    local minutes
    if not cp.started then
        minutes = T.random(b.initial_attack_delay_min, b.initial_attack_delay_max) + b.initial_attack_delay_per_epoch * epochs
        local k = t.deathmatch and T.by_difficulty(memory, b.dm_initial_attack_modifier_easy_k,
            b.dm_initial_attack_modifier_medium_k, b.dm_initial_attack_modifier_hard_k) or T.attack_delay_modifier_k(memory)
        minutes = (minutes * k + 500) // 1000
        minutes = math.max(b.attack_delay_floor, math.min(b.attack_delay_ceiling, minutes))
    else
        minutes = T.random(b.attack_frequency_min, b.attack_frequency_max) + (b.attack_frequency_per_epoch_k * epochs + 500) // 1000
        local k = t.deathmatch and T.by_difficulty(memory, b.dm_attack_frequency_modifier_easy_k,
            b.dm_attack_frequency_modifier_medium_k, b.dm_attack_frequency_modifier_hard_k) or T.attack_frequency_modifier_k(memory)
        minutes = (minutes * k + 500) // 1000
        minutes = math.max(b.attack_frequency_floor, math.min(b.attack_frequency_ceiling, minutes))
        cp.attack_started = true
    end
    cp.next_attack = view.time + minutes * 60000
end
M.set_next_attack = set_next_attack

-- ChooseFavoriteLandType and ChooseFavoriteSeaType (EnterEpoch): a random buildable soldier class of the epoch. DEVIATION: the classic's
-- favourite screener and AI lines are not exposed; the port draws among the soldier classes the player can train now
local function choose_favorite_land(view, memory)
    local list = {}
    local land = {b_barracks = true, b_archery_range = true, b_stable = true, b_siege_factory = true,
                  b_tank_factory = true, b_cyber_factory = true}
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building then
            local cls = R.class_of(c.type)
            local t = U.class_tech(view, c.type)
            local trained = false
            for _, p in ipairs(t ~= nil and t.producers or {}) do
                if land[p] then
                    trained = true
                end
            end
            if cls.category == "soldier" and trained then
                list[#list + 1] = c.type
            end
        end
    end
    table.sort(list)
    if #list > 0 then
        memory.cp.favorite_land = list[ai.random(1, #list)]
    end
    -- ChooseFavoriteSeaType: a random warship class the player can train now that passes IsFavoriteSea (DEVIATION as
    -- the land's: the screener's AI lines are not exposed)
    local sea = {}
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building and R.class_of(c.type).domain == "sea" and ARMY.favorite_sea_ok(c.type) then
            sea[#sea + 1] = c.type
        end
    end
    table.sort(sea)
    if #sea > 0 then
        memory.cp.favorite_sea = sea[ai.random(1, #sea)]
    end
    -- ChooseFavoriteSpaceType: a random space class the player can train now that passes IsFavoriteSpace (the same
    -- DEVIATION); none before the space units, so no random draw then
    local space = {}
    for _, c in ipairs(view.costs or {}) do
        if c.military and not c.building and R.class_of(c.type).domain == "space" and ARMY.favorite_space_ok(c.type) then
            space[#space + 1] = c.type
        end
    end
    table.sort(space)
    if #space > 0 then
        memory.cp.favorite_space = space[ai.random(1, #space)]
    end
end

-- KeepStandingArmy: the personality's standing_army per worker, up to the military room, 5 at a time, of the favourite
local function keep_standing_army(view, memory)
    local cp = memory.cp
    local fav = cp.favorite_land
    local k = memory.t.personality.standing_army_k or 0
    if fav == nil or k <= 0 then
        return
    end
    local target = k * U.workers(view, memory) // 1000
    local army = U.military(view, memory, true)
    -- GetUnitsAvailable: (1 - citizen share - military / cap) x cap
    local cap = view.population_cap
    local room = (cap * (1000 - memory.t.behavior.citizen_percentage_k) + 999) // 1000 - army
    local wanted = math.min(target - army, room, 5)
    if wanted > 0 then
        U.request(view, memory, fav, wanted)
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the hooks

function M.init(view, memory)
    T.load(memory)
    memory.cp = {started = false, combat_state = 0, next_enable = 0, next_attack = 0, next_expansion = 0,
                 attack_started = false, epoch = view.epoch}
    -- the engine's reflexes for the units the script holds are off (the classic's own militia and flight are the
    -- script's: ORIGIN); the naval flight stays the engine's
    if ai.reflex ~= nil then
        ai.reflex{militia = false, danger = false}
    end
    print(string.format("classic_lua init personality=%s difficulty=%d cheats=%s%s%s%s%s%s%s",
        memory.t.personality.id, memory.t.difficulty,
        T.cheat(memory, "cost") and "cost," or "", T.cheat(memory, "reveal") and "reveal," or "",
        T.cheat(memory, "epochs") and "epochs," or "", T.cheat(memory, "extra") and "extra," or "",
        T.cheat(memory, "popband") and "popband," or "", T.cheat(memory, "catchup") and "catchup," or "",
        T.cheat(memory, "retire") and "retire" or ""))
    local b = memory.t.behavior
    print(string.format("classic_lua tuning drop=%d mining=%d citizen_k=%d epoch_freq=%d-%d mod_k=%d research=%d max_mgr=%d enable_k=%d expansion=%d attack=%d-%d",
        b.drop_off_distance, b.mining_limit, b.citizen_percentage_k, b.epoch_frequency_min, b.epoch_frequency_max,
        T.epoch_frequency_modifier_k(memory), b.research_frequency, b.max_enabled_managers, b.enable_interval_k,
        T.expansion_timer(memory), b.initial_attack_delay_min, b.initial_attack_delay_max))
    return {}
end

local function events(view, memory)
    local cp = memory.cp
    local list = memory.hits or {}
    memory.hits = {}
    local fresh = ai.events()
    for _, e in ipairs(fresh) do
        list[#list + 1] = e
    end
    MIL.on_events(memory, fresh, view.time)   -- (the micro step answered the others)
    for _, e in ipairs(list) do
        if e.kind == "attacked" then
            cp.last_damaged = view.time
            local u = ai.unit(e.unit)
            if u ~= nil then
                if u.category == "building" then
                    cp.building_damaged = true
                elseif u.category == "citizen" or u.category == "boat" then
                    cp.citizen_damaged = true
                end
            end
            if cp.combat_state ~= 2 then
                cp.combat_state = 2
                cp.next_enable = 0
            end
        end
    end
    ARMY.on_events(view, memory, list)
end

function M.step(view, memory)
    R.begin_step(view)
    local cp = memory.cp
    local t = T.load(memory)
    R.sees_all = t.sees_all == true and ai.enemy_near ~= nil
    if not cp.started then
        enable_managers(view, memory)
        set_next_attack(view, memory)
        cp.next_expansion = view.time + T.expansion_timer(memory) * 1000
        R.update_groups(view, memory)
        choose_favorite_land(view, memory)
        cp.started = true
    end
    -- who is who (the militia's tests in the micro step read them)
    memory.me = ai.me()
    memory.town_x, memory.town_y = view.town_x, view.town_y
    if memory.humans == nil or view.time % 10000 < 1000 then
        memory.humans, memory.allies = {}, {}
        for _, p in ipairs(ai.players()) do
            if p.human then
                memory.humans[p.id] = true
            end
            if p.ally then
                memory.allies[p.id] = true
            end
        end
    end
    events(view, memory)
    MIL.flee(view, memory)
    -- the combat state (EEPlayer: 1 once another player's unit was seen, 2 under attack; 2 decays to 1 a minute after
    -- the last damage with no attack group out)
    if cp.combat_state == 0 and #ai.seen{limit = 1} > 0 then
        cp.combat_state = 1
        cp.next_enable = 0
    end
    if cp.combat_state == 2 and view.time - (cp.last_damaged or 0) > 60000 then
        cp.combat_state = 1
        cp.building_damaged = false
        cp.citizen_damaged = false
        cp.next_enable = 0
    end
    if view.time >= cp.next_enable then
        enable_managers(view, memory)
    end
    if view.time >= cp.next_expansion then
        R.flag_group(view, memory)
        cp.next_expansion = view.time + T.expansion_timer(memory) * 1000
    end
    -- the citizens and boats are the script's (the classic runners never take them)
    local free = ai.units{category = "citizen", held = "engine"}
    for _, id in ipairs(ai.units{category = "boat", held = "engine"}) do
        free[#free + 1] = id
    end
    if #free > 0 then
        ai.take(free)
    end
    if cp.epoch ~= view.epoch then
        cp.epoch = view.epoch
        CIV.epoch_reached(view, memory)
        choose_favorite_land(view, memory)
    end
    CIV.update(view, memory)
    EXPLORE.step(view, memory)   -- (ExplorationManager: before the combat's defence takes the free soldiers)
    ARMY.step(view, memory)   -- (EECPCombatManager::Tick: army.lua)
    if view.time % 30000 < 1000 then
        keep_standing_army(view, memory)
    end
    if view.time % 60000 < 1000 then
        local rm = R.state(memory)
        local w = rm.wanted
        print(string.format("classic_lua t=%d e=%d cit=%d/%d pop=%d/%d gat=%d/%d/%d/%d/%d/%d want=%d/%d/%d/%d/%d stock=%d/%d/%d/%d/%d centres=%d need=%d/%d/%d/%d/%d adv=%s",
            view.time // 1000, view.epoch, view.citizens, U.wanted_citizens(view, memory), view.population,
            view.population_cap, #rm.lists.idle, #rm.lists.food, #rm.lists.wood, #rm.lists.stone, #rm.lists.gold,
            #rm.lists.iron, w.food, w.wood, w.stone, w.gold, w.iron, view.stock.food, view.stock.wood, view.stock.stone,
            view.stock.gold, view.stock.iron, #rm.centres, rm.needs.food, rm.needs.wood, rm.needs.stone, rm.needs.gold,
            rm.needs.iron, tostring(CIV.state(memory).advancing)))
    end
    return {}
end

-- every 250 game ms: the hits on the citizens answered at once (the classic answers in DamageTaken); the events are
-- kept for the step
function M.micro(info, memory)
    if memory.t == nil then
        return nil
    end
    memory.hits = memory.hits or {}
    local list = ai.events()
    local now = ai.time()
    for _, e in ipairs(list) do
        memory.hits[#memory.hits + 1] = e
    end
    MIL.on_events(memory, list, now)
    return nil
end

return M
