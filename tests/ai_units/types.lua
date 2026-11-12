-- TEST ONLY: the `type` filter of ai.units / ai.seen / ai.memory as a list of class names (docs/AI_LUA_UNITS.md §2.2).
-- Every 60 s until 300 s: the ids of a list equal the union of the ids of its names one by one; an unknown name in a
-- list is left out; a list of unknown names and an unknown single name match nothing. `types.*` lines go to the trace.
local P = {}

local function set_of(ids)
    local s, n = {}, 0
    for _, id in ipairs(ids) do
        if not s[id] then
            s[id], n = true, n + 1
        end
    end
    return s, n
end

local function check(name, query, list)
    local whole = query{type = list}
    local s, n = set_of(whole)
    local union, m = {}, 0
    for _, t in ipairs(list) do
        for _, id in ipairs(query{type = t}) do
            if not union[id] then
                union[id], m = true, m + 1
            end
        end
    end
    local same = n == m and #whole == n
    for id in pairs(union) do
        same = same and s[id] == true
    end
    return same, n
end

function P.step(view, mem)
    if view.time >= (mem.next or 30000) and view.time <= 300000 then
        mem.next = view.time + 60000
        local list = {"b_town_center", "citizen", "b_settlement", "no_such_class"}
        local own_ok, own_n = check("units", ai.units, list)
        local seen_ok, seen_n = check("seen", ai.seen, {"b_town_center", "citizen", "no_such_class"})
        local mem_ok, mem_n = check("memory", ai.memory, {"b_town_center", "citizen"})
        local none = #ai.units{type = {"no_such_class", "nor_this"}} + #ai.units{type = "no_such_class"}
        print(string.format("types.check units=%s/%d seen=%s/%d memory=%s/%d none=%d ok=%s", tostring(own_ok), own_n,
            tostring(seen_ok), seen_n, tostring(mem_ok), mem_n, none,
            tostring(own_ok and seen_ok and mem_ok and none == 0 and own_n > 0)))
    end
    return {}
end

return P
