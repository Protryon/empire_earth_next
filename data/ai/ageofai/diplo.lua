-- Age of AI port: diplo.c (AI diplomacy). require("ageofai.diplo"); D.step(view, memory, events) every step from
-- main.lua (the module gates itself: the stance pass every step, the blocks every 2 s, the world scan every 10 s,
-- the initiative every 60 s from minute 10 - upstream's timings).
--
-- Upstream (dll/ai/diplo.c) decides for every AI of the mod inside one process and reads the whole stance matrix.
-- Here each AI runs alone and sees the game as a human does. What the engine shows: its OWN stance towards each
-- player (ai.players()[k].enemy / ally: EEPlayer::IsAlly = mEnemies[other] == 0, one-way as in the original) and
-- who is alive or human. What it does not show yet (ENGINE GAP, the accessors below take the fields when they come):
--   - a player's stance towards me (`hostile`: mEnemies[me], what the game's diplomacy screen shows a human). Until
--     then the PROXY: a player I stand allied with whose units hit mine (two `attacked` events within a minute) has
--     turned hostile towards me; the first look takes the stances as symmetric (the lobby's teams are), and a
--     stance turned hostile never turns allied again by itself (a peace offer of the other side is invisible);
--   - the alliances between two other players (`allies`: the world's teams, EEWorld::AddAlliance, which the game's
--     stance line announces). Until then a block is me and my direct allies (the chains are unknown);
--   - the other AIs' opinions (their own memory): the block veto knows only my own grudges;
--   - a tribute received (no event): no opinion for it, no thanks;
--   - a resign command: the surrender decides and says so, the player stays in (no ai.resign).
-- Between two AIs upstream answered an offer at once with the other's opinion; here an offer to an AI stands as
-- one to a human does (OFFER_AI_MS, 2 minutes, then "no answer"): the AI fights on until the other turns allied.
-- Not applicable here: the human's diplomacy screen and chat lines (persona_say, sysf, block_line, stance_lines),
-- the language model (llm_*), the kill-stat thunk (the `died` events' killer stands in), the history for the
-- texts (g_hist, g_hlog, g_hum), the situations' lines (diplo_situations: lines only), TEAMS_LOCKED / scenarios
-- (random-map games), the victory over vassals (the game's own end), the save hooks (the memory is saved).
-- Deviations are marked DEVIATION; the numbers are upstream's.
local S = require("ageofai.settings")
local V = require("lib.view")

local D = {}

-- the personality's traits, 0-100 (persona.c trait(): 50 without a personality; the port has no personalities)
local TRAIT = {aggression = 50, loyalty = 50, pride = 50, greed = 50, forgiveness = 50, caution = 50}
local OFFER_AI_MS = 120000      -- an offer to an AI: off unanswered after 2 minutes
local KILLS_RECENT_MIN = 20     -- the victor: whoever destroyed the most of it in the last 20 minutes
local OFFER_KIND_ALLY, OFFER_KIND_CAPIT, OFFER_KIND_BLOCK = 0, 1, 2
local BACKOFF = {1, 2, 4}        -- the next alliance offer to a player who let 0 / 1 / 2+ run out: 10 / 20 / 40 min

local function say(fmt, ...)
    print(string.format("diplo " .. fmt, ...))
end

local function clamp(v, lo, hi)
    if v < lo then return lo elseif v > hi then return hi end
    return v
end

local function g(t, j)
    return t[j] or 0
end

-- log2 from arithmetic only (upstream: every PC must decide the same): the exponent by halving, the mantissa by
-- the atanh series ln m = 2 (t + t^3/3 + t^5/5 + t^7/7), t = (m - 1) / (m + 1)
local function log2(x)
    if x <= 0 then return -100 end
    local e = 0
    while x >= 2 do x = x / 2; e = e + 1 end
    while x < 1 do x = x * 2; e = e - 1 end
    local t = (x - 1) / (x + 1)
    local t2 = t * t
    local ln = 2 * t * (1 + t2 / 3 + t2 * t2 / 5 + t2 * t2 * t2 / 7)
    return e + ln / 0.6931471805599453
end

-- the dice (upstream rnd_game32: the game's RNG): an LCG in the memory, seeded from the game time and the player
local function rnd(M)
    M.rnd = (M.rnd * 1103515245 + 12345) % 2147483648
    return (M.rnd // 65536) % 100000
end

local function tribute_worth()   -- resources one point of opinion is worth: 20 for the greediest, 100 without greed
    return 20 + 0.8 * (100 - TRAIT.greed)
end

local function capit_share()     -- the share of each resource offered in a capitulation
    return 0.2 + TRAIT.caution / 400 + (100 - TRAIT.greed) / 400
end

local function capit_need()      -- the attacker's power over its own that makes it capitulate: pride holds out
    local need = 1.5 + TRAIT.pride / 20 - TRAIT.caution / 50
    return need < 1.5 and 1.5 or need
end

local function init(M, view, me)
    M.rnd = (view.time + me * 7919 + 1) % 2147483648
    M.prev, M.theirs, M.own_prev = {}, {}, {}
    M.hit_t, M.hit_n = {}, {}
    M.due, M.war_since, M.ally_since, M.peace_due, M.peace_refused = {}, {}, {}, {}, {}
    M.offer_t, M.offer_last, M.offer_ignored, M.offer_kind, M.offer_vs = {}, {}, {}, {}, {}
    M.opinion, M.war_betrayal, M.pressure, M.justified = {}, {}, {}, {}
    M.promise = {}
    M.capit_last, M.lord, M.lord_since, M.lord_paid, M.answers = 0, 0, 0, 0, 0
    M.ws = {t = 0, bld_t = 0, power = {}, soldiers = {}, buildings = {}, close = {}, peak = 0,
            my_power = 0, my_soldiers = 0, my_buildings = 0, citizens = 0, centers = 0}
    M.kills = {t = {}, n = {}}
    M.blk = {seen = false, of = {}, mp = {}, last = 0}
    M.last_min, M.last_sit, M.no_cit_since, M.last_status = 0, 0, 0, 0
    M.resigned, M.seen = false, false
end

-- ---------------------------------------------------------------------------------------------------------------
-- the players as this AI knows them

-- j's stance towards me: 0 allied, 1 hostile (the engine's field when it has one, else the proxy)
local function theirs(M, p)
    if p.hostile ~= nil then return p.hostile and 1 or 0 end
    return M.theirs[p.id] or p.mine
end

-- a and b allied both ways (one of them me: both stances known; two others: the engine's `allies` or unknown)
local function allied_pair(M, by, me, a, b)
    if a == b then return true end
    if b == me then a, b = b, a end
    if a == me then
        local p = by[b]
        return p ~= nil and p.mine == 0 and theirs(M, p) == 0
    end
    local p = by[a]
    if p ~= nil and p.allies ~= nil then
        for _, k in ipairs(p.allies) do
            if k == b then return true end
        end
        return false
    end
    return false   -- ENGINE GAP: unknown; at the random map's start everyone is at war with everyone
end

local function at_war(M, by, me, a, b)   -- a's stance towards b hostile (a war lasts until both are allied again)
    if a == me then return by[b] ~= nil and by[b].mine == 1 end
    if b == me then return by[a] ~= nil and theirs(M, by[a]) == 1 end
    return not allied_pair(M, by, me, a, b)
end

local function power_of(M, me, j)
    local p = j == me and M.ws.my_power or g(M.ws.power, j)
    return p > 1 and p or 1
end

local function strongest_enemy(M, P)   -- the strongest player the AI is at war with (or 0)
    local best, bp = 0, -1
    for _, p in ipairs(P) do
        if p.alive and p.mine == 1 then
            local pw = g(M.ws.power, p.id)
            if best == 0 or pw > bp then best, bp = p.id, pw end
        end
    end
    return best
end

local function opinion_add(M, j, d)   -- opinion -100..100, kept in hundredths (the memory holds integers)
    M.opinion[j] = clamp(g(M.opinion, j) + math.floor(d * 100 + 0.5), -10000, 10000)
end

local function opinion(M, j)
    return g(M.opinion, j) / 100
end

local function is_vassal(M, by)   -- its lord in the game and the AI allied with him
    local l = M.lord
    return l ~= 0 and by[l] ~= nil and by[l].alive and by[l].mine == 0
end

local function blocks_on()
    return S.alliance_blocks and S.ai_diplomacy
end

-- the classes seen as soldiers (ai.memory's records carry no category and ai.class none either: the types of the
-- soldiers in sight, own and others', fill this cache - a cache per Lua state, not a memory)
local SOLDIER_TYPE = {}
local function note_soldiers(ids)   -- the hit points of these soldiers (their types remembered)
    local cols = ai.get(ids, {"hp", "type"})
    local hp = 0
    for k = 1, #ids do
        if cols and cols.type and cols.type[k] ~= nil then SOLDIER_TYPE[cols.type[k]] = true end
        hp = hp + ((cols and cols.hp and cols.hp[k]) or 0)
    end
    return hp
end

-- ---------------------------------------------------------------------------------------------------------------
-- the world scan (every 10 s): power = soldiers' hit points + 30 a building; the soldiers of each enemy within 20
-- tiles of the base (the pressure: x0.8 a scan + those there now). Fog-honest: an enemy's soldiers are those the
-- memory holds (seen in the last 3 minutes), its buildings those known (counted every 60 s: the records cost)
local function world_scan(M, view, P, now)
    local ws = M.ws
    if ws.t ~= 0 and now - ws.t < 10000 then return end
    ws.t = now
    local ids = ai.units{category = "soldier"}
    local pw = note_soldiers(ids)
    local bld = 0
    for _, b in ipairs(view.buildings) do
        bld = bld + b.finished
    end
    ws.my_soldiers, ws.my_buildings = #ids, bld
    ws.my_power = pw + 30 * bld
    if bld > ws.peak then ws.peak = bld end
    ws.citizens = view.citizens
    local tc, cap = V.buildings(view, "b_town_center"), V.buildings(view, "b_capitol")
    ws.centers = (tc and tc.finished or 0) + (cap and cap.finished or 0)
    local bx, by = view.town_x, view.town_y
    local count_bld = now - ws.bld_t >= 60000
    if count_bld then ws.bld_t = now end
    for _, p in ipairs(P) do
        local j = p.id
        local soldiers, power, close = 0, 0, 0
        if p.alive then
            if p.mine == 0 then   -- an ally's units are in sight
                local sids = ai.seen{player = j, category = "soldier"}
                power = note_soldiers(sids)
                soldiers = #sids
                if count_bld then ws.buildings[j] = #ai.seen{player = j, category = "building"} end
            else   -- the soldiers in sight name their types; the memory's mobile records of those types count
                note_soldiers(ai.seen{player = j, category = "soldier"})
                for _, r in ipairs(ai.memory{player = j, buildings = false}) do
                    if SOLDIER_TYPE[r.type] then
                        power = power + (r.hp or 0)
                        soldiers = soldiers + 1
                    end
                end
                if count_bld then ws.buildings[j] = #ai.memory{player = j, buildings = true} end
                close = #ai.seen{player = j, category = "soldier", near = {bx, by, 20}}
            end
        end
        ws.soldiers[j], ws.close[j] = soldiers, close
        ws.power[j] = power + 30 * g(ws.buildings, j)
        M.pressure[j] = (g(M.pressure, j) * 8) // 10 + close * 10   -- tenths
    end
end

local function pressure(M, j)
    return g(M.pressure, j) / 10
end

-- what j destroyed of mine, per game minute in a ring of 20 buckets (upstream g_kills_min; the `died` events)
local function kills_add(M, j, now)
    local m = now // 60000 + 1
    local b = m % KILLS_RECENT_MIN + 1
    if M.kills.t[b] ~= m then
        M.kills.t[b] = m
        M.kills.n[b] = {}
    end
    M.kills.n[b][j] = g(M.kills.n[b], j) + 1
end

local function kills_recent(M, j, now)
    local m = now // 60000 + 1
    local r = 0
    for b = 1, KILLS_RECENT_MIN do
        local t = M.kills.t[b]
        if t ~= nil and t + KILLS_RECENT_MIN > m then
            r = r + g(M.kills.n[b] or {}, j)
        end
    end
    return r
end

-- ---------------------------------------------------------------------------------------------------------------
-- stances

local function stance_note(me, j, hostile, why)   -- STANCE LINES: the trace stands in for the game's line
    say("stance me=%d to=%d now=%s why=%s", me, j, hostile and "enemy" or "ally", why)
end

local function make_peace(M, p, now, me, why)
    local j = p.id
    if p.mine == 1 then
        ai.diplomacy(j, "ally")
        p.mine = 0
        stance_note(me, j, false, why)
    end
    M.ally_since[j] = now > 0 and now or 1
    M.war_since[j] = 0
    M.peace_due[j] = 0
    M.peace_refused[j] = false
    opinion_add(M, j, 10)
end

local function declare_war(M, p, now, me, why)
    local j = p.id
    if p.mine == 1 then return end
    if M.lord == j then   -- a vassal rebels: free
        M.lord = 0
        say("vassal me=%d rebels lord=%d t=%d", me, j, now // 60000)
    end
    ai.diplomacy(j, "enemy")
    p.mine = 1
    stance_note(me, j, true, why)
    M.ally_since[j] = 0
    M.offer_t[j] = 0
end

-- ---------------------------------------------------------------------------------------------------------------
-- ALLIANCE BLOCKS ([ai_base] alliance_blocks): the players linked by alliances (allied both ways, through any chain)

local function member_ids(P, me)   -- the members: everyone still in (the smallest id names the block)
    local ids = {me}
    for _, p in ipairs(P) do
        if p.alive then ids[#ids + 1] = p.id end
    end
    table.sort(ids)
    return ids
end

local function blocks_compute(M, P, by, me)
    local ids = member_ids(P, me)
    local blk = {}
    for _, i in ipairs(ids) do blk[i] = i end
    local changed = true
    while changed do
        changed = false
        for x = 1, #ids do
            for y = x + 1, #ids do
                local i, j = ids[x], ids[y]
                if blk[i] ~= blk[j] and allied_pair(M, by, me, i, j) then
                    local lo, hi = math.min(blk[i], blk[j]), math.max(blk[i], blk[j])
                    for _, k in ipairs(ids) do
                        if blk[k] == hi then blk[k] = lo end
                    end
                    changed = true
                end
            end
        end
    end
    return blk, ids
end

local function pair_key(a, b)
    if a > b then a, b = b, a end
    return a * 64 + b
end

-- the grudge the AI holds against q: a war declared on it within pride/20 minutes, else its opinion
local function grudge(M, q, now)
    if g(M.war_since, q) ~= 0 and now - M.war_since[q] < TRAIT.pride * 3000 then return -1000 end
    return opinion(M, q)
end

local function lord_in_deal(M, me, j, blk)   -- a vassal whose lord is part of the deal has no say
    local l = M.lord
    return l ~= 0 and (l == j or (blk[l] ~= nil and (blk[l] == blk[me] or blk[l] == blk[j])))
end

-- BLOCK VETO: peace with j joins the blocks, and inside a block there is peace - every pair of the two blocks at
-- war would have to make peace too. A pair with a real grudge (a war declared within pride/20 minutes, an opinion
-- below -30) won't: the player who stands in the way, else 0. Only my own grudges are known (ENGINE GAP: the other
-- AIs' opinions are their own); humans decide their own stances
local function block_veto(M, P, by, me, j, now)
    if not blocks_on() then return 0 end
    local blk, ids = blocks_compute(M, P, by, me)
    local worst, who = -30, 0
    for _, p in ipairs(ids) do
        if p == me or (blk[me] ~= nil and blk[p] == blk[me]) then
            for _, q in ipairs(ids) do
                if (q == j or (blk[j] ~= nil and blk[q] == blk[j])) and q ~= p and not (p == me and q == j)
                        and not allied_pair(M, by, me, p, q) and blk[p] ~= blk[q] then
                    local gr = 1e9
                    if p == me and not lord_in_deal(M, me, j, blk) then gr = grudge(M, q, now) end
                    if gr < worst then
                        worst = gr
                        who = q == j and p or q
                    end
                end
            end
        end
    end
    if who ~= 0 then
        say("blocks veto me=%d with=%d by=%d grudge=%d", me, j, who, math.floor(worst))
    end
    return who
end

-- ---------------------------------------------------------------------------------------------------------------
-- SURRENDER ([ai_base] surrender): no citizens and no way to train one. The decision is upstream's; the engine
-- has no resign command (ENGINE GAP): the AI says so once and plays on

local function citizen_trainable(M, view)   -- a Town Center/Capitol and enough to pay a citizen
    if M.ws.centers <= 0 then return false end
    local c = V.cost(view, "citizen")
    if c == nil then return false end
    for _, r in ipairs(V.resources) do
        if (c.cost[r] or 0) > 0 and view.stock[r] < c.cost[r] then return false end
    end
    return true
end

local function beyond_saving(M, view)
    return M.ws.citizens <= 0 and not citizen_trainable(M, view)
end

local function victor_of(M, P, now)   -- whoever destroyed the most of it lately, else its strongest enemy
    local best, most = 0, 0
    for _, p in ipairs(P) do
        if p.alive then
            local k = kills_recent(M, p.id, now)
            if k > most then best, most = p.id, k end
        end
    end
    if best ~= 0 then return best end
    return strongest_enemy(M, P)
end

local function surrender_now(M, P, me, now, why)
    local v = victor_of(M, P, now)
    M.resigned = true
    say("surrender me=%d to=%d t=%d why=%s kills=%d (no ai.resign: the player stays in)", me, v, now // 60000, why,
        v ~= 0 and kills_recent(M, v, now) or 0)
end

local function surrender_instead(M, P, view, me, now)   -- a block split: beyond saving, it resigns rather than side
    if not S.surrender then return false end
    if M.resigned then return true end
    if M.no_cit_since == 0 or not beyond_saving(M, view) then return false end
    surrender_now(M, P, me, now, "block split")
    return true
end

-- agg turned hostile towards vic, both of block `old`: this AI (a member of it, neither of the two) takes a side -
-- the one it capitulated to; else its direct ally if allied with only one; else the one it thinks better of (the
-- attacked +10) - and declares war on the other
local function block_split(M, P, by, view, me, agg, vic, blk, now)
    if agg == me or vic == me then return end
    local old = blk[vic]
    if old == nil or blk[me] ~= old then return end
    local side
    if M.lord == agg or M.lord == vic then
        side = M.lord
    elseif allied_pair(M, by, me, me, vic) ~= allied_pair(M, by, me, me, agg) then
        side = allied_pair(M, by, me, me, vic) and vic or agg
    else
        side = (opinion(M, vic) + 10 >= opinion(M, agg)) and vic or agg
    end
    local other = side == vic and agg or vic
    local po = by[other]
    if po == nil or po.mine == 1 then return end
    if surrender_instead(M, P, view, me, now) then
        say("blocks me=%d: %d turned on %d - beyond saving, resigns instead of siding with %d", me, agg, vic, side)
        return
    end
    M.justified[other] = true
    declare_war(M, po, now, me, "block split")
    M.blk.mp[pair_key(me, other)] = nil
    say("blocks me=%d: %d turned on %d - sides with %d, war on %d", me, agg, vic, side, other)
end

local function blocks_pass(M, P, by, view, me, now)
    if not blocks_on() then
        M.blk.seen = false
        return
    end
    if M.blk.seen and now - M.blk.last < 2000 then return end
    M.blk.last = now
    if M.blk.seen then   -- wars inside a block since the last pass
        local ids = member_ids(P, me)
        for x = 1, #ids do
            for y = x + 1, #ids do
                local i, j = ids[x], ids[y]
                if M.blk.mp[pair_key(i, j)] and not allied_pair(M, by, me, i, j) then
                    local agg
                    if i == me or j == me then
                        local o = i == me and j or i
                        local oh, mh = theirs(M, by[o]), by[o].mine
                        if mh == 1 and oh == 0 then agg = me
                        elseif mh == 0 and oh == 1 then agg = o
                        else agg = o end   -- both at once: the human (I am an AI)
                    else
                        -- the stances between two others are unknown (ENGINE GAP): upstream's fallback - the human,
                        -- else the first
                        agg = (not by[i].human) and j or i
                    end
                    local vic = agg == i and j or i
                    say("blocks me=%d: %d turned hostile towards its block's %d", me, agg, vic)
                    block_split(M, P, by, view, me, agg, vic, M.blk.of, now)
                end
            end
        end
    end
    local blk = blocks_compute(M, P, by, me)
    -- peace inside a block: my own pairs (the other AIs make theirs on their side)
    for _, p in ipairs(P) do
        local j = p.id
        if p.alive and blk[me] ~= nil and blk[me] == blk[j] and not allied_pair(M, by, me, me, j) then
            if not p.human then
                make_peace(M, p, now, me, "one block")
                say("blocks me=%d and %d make peace (one block)", me, j)
            elseif g(M.offer_t, j) == 0 then
                if p.mine == 1 then   -- at war with him: it offers peace (turns allied when he does)
                    M.offer_t[j] = now > 0 and now or 1
                    M.offer_kind[j] = OFFER_KIND_BLOCK
                    M.offer_vs[j] = 0
                    say("blocks me=%d offers %d (human, same block) peace", me, j)
                else   -- the AI is allied, he isn't: his war on it
                    block_split(M, P, by, view, me, j, me, blk, now)
                end
            end
        end
    end
    blk = blocks_compute(M, P, by, me)   -- after the peace
    local of = {}
    local ids = member_ids(P, me)
    for _, i in ipairs(ids) do of[i] = blk[i] end
    M.blk.of = of
    local mp = {}
    for x = 1, #ids do
        for y = x + 1, #ids do
            if allied_pair(M, by, me, ids[x], ids[y]) then mp[pair_key(ids[x], ids[y])] = true end
        end
    end
    M.blk.mp = mp
    M.blk.seen = true
end

-- ---------------------------------------------------------------------------------------------------------------
-- PEACE: score = opinion + 25 x log2(his power / own power) (at most +-40) + 0.4 x (50 - aggression)
-- - 0.3 x (pride - 50) + 15 if he fights the AI's strongest enemy; accept if > 0. No peace within pride/20
-- minutes of his declaration (mod/computer.eeai peace_score; the pride minutes are the rule's)
local function peace_score(M, P, by, me, j, now)
    if g(M.war_since, j) ~= 0 and now - M.war_since[j] < TRAIT.pride * 3000 then return -1000 end
    local lead = strongest_enemy(M, P)
    local s = opinion(M, j) + clamp(25 * log2(power_of(M, me, j) / power_of(M, me, me)), -40, 40)
        + 0.4 * (50 - TRAIT.aggression) - 0.3 * (TRAIT.pride - 50)
    if lead ~= 0 and lead ~= j and at_war(M, by, me, j, lead) then s = s + 15 end
    return s
end

-- a pending peace offer of j (allied towards the AI while the AI is at war with him)
local function peace_offer(M, P, by, view, me, p, now)
    local j = p.id
    if is_vassal(M, by) then   -- not its decision: "talk to my lord"
        if not M.peace_refused[j] then
            say("peace me=%d offer_from=%d: a vassal, refers to its lord %d", me, j, M.lord)
        end
        M.peace_refused[j] = true
        M.peace_due[j] = now + 120000
        return
    end
    local s = peace_score(M, P, by, me, j, now)
    say("peace me=%d offer_from=%d score=%d opinion=%d power=%d/%d", me, j, math.floor(s), math.floor(opinion(M, j)),
        math.floor(power_of(M, me, j)), math.floor(power_of(M, me, me)))
    local veto = 0
    if s > 0 then veto = block_veto(M, P, by, me, j, now) end
    if s > 0 and veto == 0 then
        make_peace(M, p, now, me, "peace accepted")
        return
    end
    if not M.peace_refused[j] then
        say("peace me=%d refuses %d%s", me, j, veto ~= 0 and (" (block veto by " .. veto .. ")") or "")
    end
    M.peace_refused[j] = true
    M.peace_due[j] = now + 120000   -- decided again every 2 minutes while the offer stands
end

-- ---------------------------------------------------------------------------------------------------------------
-- STANDING OFFERS: an alliance, a capitulation or a block's peace stands until its reason is gone; the AI fights
-- on until the other turns allied. Why it is off now, nil while it stands; quiet: without a line
local function offer_off(M, P, by, me, p, now)
    local j, vs = p.id, g(M.offer_vs, p.id)
    if not p.alive then return "out of the game", true end
    if p.mine == 0 then return "allied already", true end
    if not p.human then   -- an AI (the classic AI cannot be told from a port: it never answers either)
        if now - M.offer_t[j] > OFFER_AI_MS then return "no answer", false end
        return nil
    end
    local kind = g(M.offer_kind, j)
    if kind == OFFER_KIND_BLOCK then   -- block peace: while both are in one block
        if not blocks_on() then return "no blocks", true end
        local blk = blocks_compute(M, P, by, me)
        if blk[me] ~= nil and blk[me] == blk[j] then return nil end
        return "no longer in one block", true
    end
    if is_vassal(M, by) then return "a vassal now", false end
    if kind == OFFER_KIND_CAPIT then   -- while it is still losing (3/4 of the ratio that made it give in)
        if power_of(M, me, j) < 0.75 * capit_need() * power_of(M, me, me) then return "holds its own again", false end
        return nil
    end
    if vs == 0 then return nil end
    if by[vs] == nil or not by[vs].alive then return "the common enemy is out", false end
    if by[vs].mine == 0 then return "at peace with the common enemy", false end
    if allied_pair(M, by, me, j, vs) then return "allied with the common enemy", false end
    if block_veto(M, P, by, me, j, now) ~= 0 then return "an ally stands in the way", false end
    return nil
end

-- AI me offers `best` an alliance against `lead`. Upstream answered for an AI at once with its opinion (one
-- process); here every offer stands like one to a human (OFFER_AI_MS for an AI)
local function offer_alliance(M, me, best, lead, now)
    M.offer_last[best] = now
    M.offer_t[best] = now > 0 and now or 1
    M.offer_kind[best] = OFFER_KIND_ALLY
    M.offer_vs[best] = lead
    say("offer me=%d alliance to=%d against=%d opinion=%d ignored=%d", me, best, lead, math.floor(opinion(M, best)),
        g(M.offer_ignored, best))
end

-- the betrayal of ally j: war on j - and with blocks on j's whole block (no one left to take sides)
local function betray_now(M, P, by, me, j, now)
    local blk = blocks_on() and blocks_compute(M, P, by, me) or {}
    declare_war(M, by[j], now, me, "betrayal")
    if blocks_on() then
        for _, p in ipairs(P) do
            local k = p.id
            if k ~= j and blk[k] ~= nil and blk[k] == blk[j] then
                declare_war(M, p, now, me, "betrayal (the victim's block)")
                M.blk.mp[pair_key(me, k)] = nil
            end
        end
    end
end

-- A VASSAL'S REBELLION (every 60 s): little loyalty (under 50) and much pride, served 10 minutes, clearly stronger
-- than its lord's side (1 + (loyalty + 100 - pride) / 50 times), not while it thinks well of its lord (30+); then
-- each minute with a chance of (100 - loyalty) x pride / 50000
local function vassal_rebellion(M, P, by, me, now)
    local l = M.lord
    if not is_vassal(M, by) then return "no vassal" end
    if TRAIT.loyalty >= 50 then return "loyal" end
    if now - M.lord_since < 600000 then return "served under 10 minutes" end
    if opinion(M, l) >= 30 then return "thinks well of its lord" end
    local side, need = power_of(M, me, l), 1 + (TRAIT.loyalty + 100 - TRAIT.pride) / 50
    if blocks_on() then
        local blk = blocks_compute(M, P, by, me)
        if blk[l] ~= nil then
            side = 0
            for _, p in ipairs(P) do
                if blk[p.id] == blk[l] then side = side + power_of(M, me, p.id) end
            end
        end
    end
    if power_of(M, me, me) < need * side then return "too weak" end
    if rnd(M) >= (100 - TRAIT.loyalty) * TRAIT.pride * 2 then return "not this minute" end
    say("vassal me=%d rebels against lord=%d power=%d side=%d need=%d", me, l, math.floor(power_of(M, me, me)),
        math.floor(side), math.floor(need * 10))
    local blk = blocks_on() and blocks_compute(M, P, by, me) or {}
    declare_war(M, by[l], now, me, "rebellion")   -- free
    if blocks_on() and blk[l] ~= nil then   -- the lord's whole block
        for _, p in ipairs(P) do
            if p.id ~= l and blk[p.id] == blk[l] then
                declare_war(M, p, now, me, "rebellion (the lord's block)")
                M.blk.mp[pair_key(me, p.id)] = nil
            end
        end
    end
    return nil
end

-- AI INITIATIVE ([ai_base] ai_diplomacy; every 60 s from minute 10): an alliance against a leader (power > 1.5 x
-- own) offered to another enemy of the leader - the one it likes best, above -20, not one whose army is in its base
-- (threat 10+), not one with an offer pending or one made in the last 10 (20, 40) minutes, not one its side can't
-- stand; else the betrayal of a weak ally (allied 10+ minutes, under half its power, opinion < 30, chance
-- (100 - loyalty) x aggression / 100000 a minute), never the player it capitulated to
local function diplo_initiative(M, P, by, me, now)
    if not S.ai_diplomacy or now < 600000 then return end
    if is_vassal(M, by) then
        vassal_rebellion(M, P, by, me, now)
        return
    end
    local lead = strongest_enemy(M, P)
    if lead ~= 0 and power_of(M, me, lead) > 1.5 * power_of(M, me, me) then   -- seek_ally
        local best, bs = 0, -20
        for _, p in ipairs(P) do
            local j = p.id
            if p.alive and j ~= lead and p.mine == 1 and at_war(M, by, me, j, lead) and pressure(M, j) < 10
                    and g(M.offer_t, j) == 0
                    and (g(M.offer_last, j) == 0
                         or now - M.offer_last[j] >= 600000 * BACKOFF[math.min(g(M.offer_ignored, j), 2) + 1]) then
                local sc = opinion(M, j)   -- ally_score
                if sc > bs and block_veto(M, P, by, me, j, now) == 0 then bs, best = sc, j end
            end
        end
        if best ~= 0 then
            offer_alliance(M, me, best, lead, now)
            return
        end
    end
    for _, p in ipairs(P) do   -- betrayal of a weak ally
        local j = p.id
        if p.alive and p.mine == 0 and g(M.ally_since, j) ~= 0 and now - M.ally_since[j] >= 600000 and M.lord ~= j then
            local bpow, lord_in = power_of(M, me, j), false
            if blocks_on() then   -- the victim's whole block: its power, its lord
                local blk = blocks_compute(M, P, by, me)
                bpow = 0
                for _, q in ipairs(P) do
                    if blk[q.id] ~= nil and blk[q.id] == blk[j] then
                        bpow = bpow + power_of(M, me, q.id)
                        if M.lord == q.id then lord_in = true end
                    end
                end
            end
            if not (lord_in or bpow * 2 > power_of(M, me, me) or opinion(M, j) >= 30) then
                local chance = (100 - TRAIT.loyalty) * TRAIT.aggression   -- per 100000
                if rnd(M) < chance then
                    say("betray me=%d ally=%d opinion=%d power=%d/%d", me, j, math.floor(opinion(M, j)),
                        math.floor(power_of(M, me, me)), math.floor(bpow))
                    betray_now(M, P, by, me, j, now)
                    return
                end
            end
        end
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- CAPITULATION (every 10 s, from minute 10, at most every 10 minutes): the attacker (highest threat) has 5+
-- soldiers in its base and threat 15+, it has lost over 10% of its peak buildings, and the attacker's power is at
-- least 1.5 + pride/20 - caution/50 times its own: it offers him peace and a share of each resource (20% +
-- caution/4 % + (100 - greed)/4 %). The tribute stays in the stock while the offer stands (it fights on and spends
-- what it needs) and is paid when he turns allied, as far as the stock still has it
local function capitulate(M, view, me, a, now)
    local share, total = capit_share(), 0
    for _, r in ipairs(V.resources) do
        local n = view.stock[r] > 0 and math.floor(view.stock[r] * share) or 0
        M.promise[r] = n
        total = total + n
    end
    M.capit_last = now > 0 and now or 1
    M.offer_t[a] = now > 0 and now or 1
    M.offer_kind[a] = OFFER_KIND_CAPIT
    M.offer_vs[a] = 0
    say("capitulate me=%d to=%d tribute=%d share=%d power=%d/%d close=%d buildings=%d/%d", me, a, total,
        math.floor(share * 100), math.floor(power_of(M, me, me)), math.floor(power_of(M, me, a)), g(M.ws.close, a),
        M.ws.my_buildings, M.ws.peak)
end

local function pay_promise(M, view, me, j)   -- what was offered, as far as the stock still has it
    local amounts, total = {}, 0
    for _, r in ipairs(V.resources) do
        local n = math.min(g(M.promise, r), view.stock[r])
        if n > 0 then
            amounts[r] = n
            total = total + n
        end
        M.promise[r] = 0
    end
    if total > 0 then
        local code, reason = ai.tribute(j, amounts)
        say("tribute me=%d to=%d total=%d why=capitulation result=%s", me, j, total, tostring(reason))
    end
    return total
end

local function check_capitulation(M, P, by, view, me, now)
    if not S.ai_diplomacy or now < 600000 or is_vassal(M, by) then return end
    if S.surrender and beyond_saving(M, view) then return end   -- nothing left to keep: it resigns instead
    if M.capit_last ~= 0 and now - M.capit_last < 600000 then return end
    local a = 0   -- the attacker: whose soldiers have been in its base the most lately
    for _, p in ipairs(P) do
        local j = p.id
        if p.alive and p.mine == 1 and theirs(M, p) == 1
                and not (g(M.offer_t, j) ~= 0 and (g(M.offer_kind, j) ~= OFFER_KIND_ALLY or not p.human)) then
            if a == 0 or pressure(M, j) > pressure(M, a) then a = j end
        end
    end
    if a == 0 or g(M.ws.close, a) < 5 or pressure(M, a) < 15 then return end
    if M.ws.peak < 5 or M.ws.my_buildings * 10 >= M.ws.peak * 9 then return end
    if power_of(M, me, a) < capit_need() * power_of(M, me, me) then return end
    if g(M.offer_t, a) ~= 0 then   -- its alliance offer to the player who is beating it now: a capitulation instead
        M.offer_t[a] = 0
        say("offer me=%d withdraws its alliance offer to %d to capitulate", me, a)
    end
    capitulate(M, view, me, a, now)
end

local function check_surrender(M, P, view, me, now)
    if not S.surrender or M.resigned then return end
    if M.ws.citizens > 0 then   -- (citizens_useful of expand.c is not ported: every citizen counts)
        M.no_cit_since = 0
        return
    end
    if M.no_cit_since == 0 then
        M.no_cit_since = now > 0 and now or 1
        return
    end
    local can_train = citizen_trainable(M, view)
    if now - M.no_cit_since < (can_train and 180000 or 60000) then return end
    local lead = strongest_enemy(M, P)
    if lead ~= 0 then
        if power_of(M, me, me) * 2 >= power_of(M, me, lead) then return end   -- its army can still win
    elseif can_train then
        return   -- at peace: rebuilds
    end
    surrender_now(M, P, me, now, can_train and "no citizens 3 min" or "no citizens 1 min, none trainable")
end

-- A VASSAL'S TRIBUTE: every vassal_tribute_every minutes vassal_tribute_share percent of each resource to its lord.
-- DEVIATION: upstream's rules pay only the capitulation's promise; the 3 minutes and the quarter are the bounds
-- of the language model's tribute tool ([llm] llm_constrained, llm.c tool_tribute_ok / tribute_cap), made a rule
local function vassal_tribute(M, by, view, me, now)
    if not is_vassal(M, by) or S.vassal_tribute_share <= 0 then return end
    if M.lord_paid ~= 0 and now - M.lord_paid < S.vassal_tribute_every * 60000 then return end
    M.lord_paid = now > 0 and now or 1
    local amounts, total = {}, 0
    for _, r in ipairs(V.resources) do
        local n = view.stock[r] * S.vassal_tribute_share // 100
        if n > 0 then
            amounts[r] = n
            total = total + n
        end
    end
    if total > 0 then
        local code, reason = ai.tribute(M.lord, amounts)
        say("tribute me=%d to=%d total=%d why=vassal result=%s", me, M.lord, total, tostring(reason))
    end
end

-- ---------------------------------------------------------------------------------------------------------------
-- the tick (upstream diplo_tick), every step

local function diplo_tick(M, P, by, view, me, now)
    for _, p in ipairs(P) do
        local j = p.id
        local s, mine = theirs(M, p), p.mine
        if not M.seen then   -- the first look: the stances as they are
            M.opinion[j] = mine == 1 and -1000 or 2000
            if mine == 0 and s == 0 then M.ally_since[j] = now > 0 and now or 1 end
        end
        -- (an attack on a player made peace with: the engine refuses attacks on an ally and the force retargets)
        M.own_prev[j] = mine
        local turned = M.seen and s == 1 and g(M.prev, j) == 0
        if turned and M.lord == j then   -- its lord turned on it: free (and -40 on top: reputation_pass)
            M.lord = 0
            opinion_add(M, j, -40)
            say("vassal me=%d: its lord %d turned on it - no longer a vassal", me, j)
        end
        if turned then   -- j turned hostile
            local betrayal = mine == 0 and g(M.ally_since, j) ~= 0 and now - M.ally_since[j] > 120000
            opinion_add(M, j, (betrayal and -60 or -40) * (0.5 + TRAIT.pride / 100))
            M.war_since[j] = now > 0 and now or 1
            M.ally_since[j] = 0
            M.peace_due[j] = 0
            M.offer_t[j] = 0
            if S.answer_war and mine == 0 then
                M.due[j] = now + 3000 + ((me * 7 + j * 3) % 4) * 1000
                M.war_betrayal[j] = betrayal
            end
            say("war me=%d declared_by=%d t=%d betrayal=%s", me, j, now // 60000, tostring(betrayal))
        end
        if M.seen and s == 0 and g(M.prev, j) == 1 and mine == 1 then   -- j offers peace
            M.peace_due[j] = now + 4000 + ((me * 5 + j) % 5) * 1000
            M.peace_refused[j] = false
        end
        if s == 0 and mine == 0 and g(M.ally_since, j) == 0 then M.ally_since[j] = now > 0 and now or 1 end
        M.prev[j] = s
        if g(M.due, j) ~= 0 and now >= M.due[j] then   -- ANSWER WAR
            M.due[j] = 0
            if s == 1 and p.mine == 0 and p.alive then
                ai.diplomacy(j, "enemy")
                p.mine = 1
                stance_note(me, j, true, M.war_betrayal[j] and "betrayed" or "war answered")
                M.answers = M.answers + 1
                -- (the revenge attack on his most attractive object: the force's next push takes him as an enemy)
            end
        end
        if g(M.peace_due, j) ~= 0 and now >= M.peace_due[j] then
            M.peace_due[j] = 0
            if s == 0 and p.mine == 1 and p.alive then peace_offer(M, P, by, view, me, p, now) end
        end
        if g(M.offer_t, j) ~= 0 then   -- STANDING OFFERS
            local kind = g(M.offer_kind, j)
            if s == 0 then   -- he turned allied: the answer
                M.offer_t[j] = 0
                M.ally_since[j] = now > 0 and now or 1
                if kind == OFFER_KIND_CAPIT then
                    make_peace(M, p, now, me, "capitulation accepted")
                    local paid = pay_promise(M, view, me, j)
                    M.lord = j
                    M.lord_since = now > 0 and now or 1
                    M.war_since[j] = 0
                    opinion_add(M, j, 20)
                    say("capitulate me=%d accepted_by=%d paid=%d", me, j, paid)
                else
                    make_peace(M, p, now, me, kind == OFFER_KIND_BLOCK and "block peace accepted" or "alliance accepted")
                end
                M.offer_ignored[j] = 0
            else
                local off, quiet = offer_off(M, P, by, me, p, now)
                if off ~= nil then
                    M.offer_t[j] = 0
                    if kind == OFFER_KIND_CAPIT then M.promise = {} end
                    say("offer me=%d to=%d kind=%s off=%s", me, j,
                        kind == OFFER_KIND_CAPIT and "capitulation" or kind == OFFER_KIND_BLOCK and "block peace" or "alliance", off)
                    if not quiet and kind == OFFER_KIND_ALLY then M.offer_ignored[j] = g(M.offer_ignored, j) + 1 end
                end
            end
        end
    end
    M.seen = true
end

-- once a minute: the opinions drift (+loyalty/100 allied, -pride/200 at war, +0.5 a common enemy, a grudge fades
-- with forgiveness); the AI's own moves
local function diplo_minute(M, P, by, view, me, now)
    if M.last_min ~= 0 and now - M.last_min < 60000 then return end
    local first = M.last_min == 0
    M.last_min = now > 0 and now or 1
    world_scan(M, view, P, now)
    if first then return end
    local lead = strongest_enemy(M, P)
    for _, p in ipairs(P) do
        local j = p.id
        if p.alive then
            local d = 0
            if p.mine == 0 and theirs(M, p) == 0 then d = d + TRAIT.loyalty / 100 end
            if p.mine == 1 then d = d - TRAIT.pride / 200 end
            if lead ~= 0 and lead ~= j and at_war(M, by, me, j, lead) and by[lead].mine == 1 then d = d + 0.5 end
            if g(M.opinion, j) < 0 then d = d - opinion(M, j) * TRAIT.forgiveness / 2000 end
            opinion_add(M, j, d)
        end
    end
    diplo_initiative(M, P, by, me, now)
    vassal_tribute(M, by, view, me, now)
end

function D.step(view, memory, events)
    if not S.diplomacy then return end
    local me = ai.me()
    local M = memory.diplo
    if M == nil then
        M = {}
        memory.diplo = M
        init(M, view, me)
    end
    local now = view.time
    -- the players as the engine shows them: my stance, alive, human (the fields `hostile` and `allies` when they come)
    local P, by = {}, {}
    for _, p in ipairs(ai.players()) do
        if not p.me and p.id ~= me then
            local rec = {id = p.id, alive = p.alive, human = p.human, mine = p.enemy and 1 or 0,
                         hostile = p.hostile, allies = p.allies}
            P[#P + 1] = rec
            by[p.id] = rec
        end
    end
    -- the PROXY for a stance towards me (no engine field): two hits by a player I stand allied with, within a
    -- minute; the kills by pair from the `died` events (their killer's player, or the last hit on the unit)
    local last_hit = {}
    for _, e in ipairs(events) do
        if e.kind == "attacked" and e.player ~= nil and e.player > 0 and e.player ~= me then
            last_hit[e.unit] = e.player
            local p = by[e.player]
            if p ~= nil and p.hostile == nil and M.seen and (M.theirs[e.player] or p.mine) == 0 then
                if g(M.hit_t, e.player) ~= 0 and now - M.hit_t[e.player] <= 60000 and g(M.hit_n, e.player) >= 1 then
                    M.theirs[e.player] = 1
                    M.hit_n[e.player] = 0
                else
                    M.hit_t[e.player] = now > 0 and now or 1
                    M.hit_n[e.player] = g(M.hit_n, e.player) + 1
                end
            end
        end
    end
    for _, e in ipairs(events) do
        if e.kind == "died" then
            local k = last_hit[e.unit]
            if k == nil and e.killer ~= nil then
                local u = ai.unit(e.killer)
                k = u and u.player
            end
            if k ~= nil and k > 0 and k ~= me then kills_add(M, k, now) end
        end
    end
    if not M.seen then   -- the first look: the stances as symmetric (the proxy's start)
        for _, p in ipairs(P) do
            if M.theirs[p.id] == nil then M.theirs[p.id] = p.mine end
        end
    end
    diplo_tick(M, P, by, view, me, now)
    blocks_pass(M, P, by, view, me, now)
    diplo_minute(M, P, by, view, me, now)
    if M.last_sit == 0 or now - M.last_sit >= 10000 then
        M.last_sit = now > 0 and now or 1
        world_scan(M, view, P, now)
        check_capitulation(M, P, by, view, me, now)
        check_surrender(M, P, view, me, now)
    end
    if now - M.last_status >= 300000 then   -- the state every 5 minutes (the trace)
        M.last_status = now
        local parts = {}
        for _, p in ipairs(P) do
            parts[#parts + 1] = string.format("p%d:%s%s/%s op=%d pow=%d thr=%d%s", p.id, p.human and "h" or "a",
                p.mine == 1 and "E" or "A", theirs(M, p) == 1 and "E" or "A", math.floor(opinion(M, p.id)),
                math.floor(g(M.ws.power, p.id)), math.floor(pressure(M, p.id)), p.alive and "" or " out")
        end
        say("status me=%d t=%d power=%d lord=%d answers=%d %s", me, now // 60000, math.floor(M.ws.my_power), M.lord,
            M.answers, table.concat(parts, " "))
    end
end

return D
