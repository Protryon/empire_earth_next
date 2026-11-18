-- TEST ONLY: the bindings no other game calls (tools/ai_binding_golden.py's "cover" game; docs/progress/ai-lua-host.md):
-- can_hurt, attack_ground, load, unload, heal, rally, cancel, formation, pack, dropoff, cast, lock, forest_point,
-- each with good and bad arguments, every 20 s from 20 s to 200 s. Needs the test folder as EE_AI_SCRIPTS and
-- EE_AI_TEST_SPAWN=1 (sim.py --ai-scripts tests/ai_units, EE_AI_NEXT_SCRIPT=cover). `cover.*` lines go to the trace.
local P = {}

local function say(what, ...)
    local out = {}
    for i = 1, select("#", ...) do
        out[#out + 1] = tostring((select(i, ...)))
    end
    print(string.format("cover %s %s", what, table.concat(out, " ")))
end

function P.step(view, mem)
    if view.town_x < 0 or view.time < 20000 or view.time > 200000 or view.time % 20000 >= 1000 then
        return {}
    end
    local tx, ty = view.town_x, view.town_y
    local cits = ai.units{category = "citizen"}
    local tcs = ai.units{category = "building", built = true}
    local soldiers = ai.units{category = "soldier"}
    if mem.spawned == nil then
        mem.spawned = true
        for k = 1, 4 do
            ai.test_spawn("inf01_clubman", tx + 4 + k, ty + 4, ai.me())
        end
    end
    local some = {cits[1], cits[2]}
    say("can_hurt", ai.can_hurt("inf01_clubman", "inf02_spearman"), ai.can_hurt("inf01_clubman", "nope"), ai.can_hurt(3, 4))
    say("attack_ground", ai.attack_ground(soldiers, tx + 6, ty + 6))
    say("attack_ground_bad", ai.attack_ground(soldiers, -5, 3))
    say("load", ai.load(some, tcs[1]))
    say("garrison", ai.garrison({cits[3]}, tcs[1]))
    say("unload", ai.unload(tcs, tx + 2, ty + 2))
    say("heal", ai.heal(soldiers, cits[1]))
    say("rally_xy", ai.rally(tcs, tx + 3, ty + 3))
    say("rally_unit", ai.rally(tcs, cits[1]))
    say("rally_bad", ai.rally(tcs, "x"))
    local u = cits[1] and ai.unit(cits[1])
    if u ~= nil then
        say("cancel", ai.cancel(tcs, u.type))
    end
    say("cancel_bad", ai.cancel(tcs, {}))
    say("formation", ai.formation(soldiers, 2))
    say("formation_bad", ai.formation(soldiers, 99))
    say("pack", ai.pack(soldiers, true))
    say("dropoff", ai.dropoff({cits[4]}, tcs[1]))
    say("cast", ai.cast(soldiers, "t_nope", 1))
    say("cast_xy", ai.cast(soldiers, 5, tx, ty))
    say("lock", ai.lock(some, true))
    say("forest_point", ai.forest_point(tx, ty, 30, view.time // 20000, 7))
    say("stance", ai.stance(soldiers, 1))
    say("explore", ai.explore({soldiers[1]}))
    say("delete_bad", ai.delete({}))
    return {}
end

return P
