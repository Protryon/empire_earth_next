-- TEST ONLY: the wall, gate, lock, tribute, trade and diplomacy commands (docs/AI_LUA_UNITS.md §3.3). Needs the test
-- folder as EE_AI_SCRIPTS (sim_bench --ai-scripts tests/ai_units --env EE_AI_NEXT_SCRIPT=commands).
--
-- At 30 s three citizens lay a palisade of 7 tiles 6 tiles east of the town; from 90 s on, every 10 s, a gate is tried
-- on its middle tile until one stands, then the gate's pieces are locked. The tribute, the trade and the stance go to
-- the other computer player. `commands.*` lines go to the trace with every result.
local WALL, GATE = "b_wall_palisades", "b_gate_palisades"

local P = {}

local function say(what, code, reason, n, id)
    print(string.format("commands %s result=%s reason=%s n=%d id=%s", what, tostring(code), tostring(reason), n or 0, tostring(id)))
end

function P.step(view, mem)
    if view.town_x < 0 then
        return {}
    end
    local tx, ty = view.town_x, view.town_y
    if mem.wall == nil and view.time >= 30000 then
        mem.wall = 1
        local cits = ai.units{category = "citizen"}
        local ids = {}
        for k = 1, math.min(3, #cits) do
            ids[k] = cits[k]
        end
        local code, reason, n = ai.wall(ids, WALL, tx + 6, ty - 3, tx + 6, ty + 3)
        say("wall", code, reason, n)
        mem.gx, mem.gy = tx + 6, ty
    end
    if mem.wall == 1 and view.time >= 90000 and view.time % 10000 < 1000 then
        local code, reason, n = ai.gate(GATE, mem.gx, mem.gy)
        say("gate", code, reason, n)
        if code == "ok" then
            mem.wall = 2
            local ids = {}
            for _, id in ipairs(ai.units{near = {mem.gx, mem.gy, 3}}) do
                local u = ai.unit(id)
                if u ~= nil and u.type == GATE then
                    ids[#ids + 1] = id
                end
            end
            local c2, r2, n2 = ai.lock(ids, true)
            say("lock", c2, r2, n2)
        elseif view.time >= 240000 then
            mem.wall = 3
        end
    end
    if mem.diplo == nil and view.time >= 40000 then
        mem.diplo = true
        local other = nil
        for _, p in ipairs(ai.players()) do
            if not p.me and not p.human then
                other = p.id
            end
        end
        if other ~= nil then
            say("tribute", ai.tribute(other, {food = 50, wood = 20}))
            say("tribute_too_much", ai.tribute(other, {gold = 100000}))
            say("diplomacy_ally", ai.diplomacy(other, "ally"))
            local allied = false
            for _, p in ipairs(ai.players()) do
                if p.id == other then
                    allied = p.ally
                end
            end
            print(string.format("commands ally_now=%s", tostring(allied)))
            say("diplomacy_enemy", ai.diplomacy(other, "enemy"))
            say("diplomacy_bad", ai.diplomacy(other, "friend"))
        end
        -- the players' stances both ways and the teams; a class's cost and category
        for _, p in ipairs(ai.players()) do
            print(string.format("commands player id=%d me=%s ally=%s enemy=%s hostile=%s allies=%d human=%s", p.id, tostring(p.me),
                tostring(p.ally), tostring(p.enemy), tostring(p.hostile), #(p.allies or {}), tostring(p.human)))
        end
        print(string.format("commands units_type tc=%d tc_built=%d buildings=%d tc_near=%d", #ai.units{type = "b_town_center"},
            #ai.units{type = "b_town_center", built = true}, #ai.units{category = "building"}, #ai.units{type = "b_town_center", near = {tx, ty, 5}}))
        print(string.format("commands tc_record type=%s built=%s", tostring(ai.unit(ai.units{category = "building"}[1]).type), tostring(ai.unit(ai.units{category = "building"}[1]).built)))
        local c = ai.class("inf01_clubman")
        print(string.format("commands class type=%s category=%s family=%d cost=%d/%d/%d/%d/%d", c.type, tostring(c.category),
            c.family or -1, c.cost.food, c.cost.wood, c.cost.stone, c.cost.gold, c.cost.iron))
        print(string.format("commands memory_cat buildings=%d soldiers=%d", #ai.memory{category = "building"}, #ai.memory{category = "soldier"}))
        -- a patrol of the citizens between two points by the town
        local cits = ai.units{category = "citizen"}
        local ids = {cits[#cits]}
        say("patrol", ai.patrol(ids, {{tx + 4, ty}, {tx, ty + 4}, {tx - 4, ty}}))
        say("patrol_bad", ai.patrol(ids, {{tx + 4, ty}}))
        say("trade_sell", ai.trade("wood", "sell", 1))
        say("trade_buy", ai.trade("stone", "buy", 1))
        say("trade_bad", ai.trade("gold", "buy", 1))
    end
    return {}
end

return P
