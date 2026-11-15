-- The gatherer split's stock correction and the epoch saving ramp, after the improved classic AI (balanced in
-- Data/db/aipersonalities.yaml; docs/AI_RESEARCH.md §8.10, 8.11, 8.21, 8.27). require("lib.demand").
local M = {}

-- balanced's economy settings (aipersonalities.yaml `balanced`; EECPResourceManager's stock correction)
M.SURPLUS = 1000            -- a resource with more in stock (beyond what is reserved) ...
M.SURPLUS_CUT = 50          -- ... gets this percent of its gatherers
M.LOW_STOCK = 150           -- a resource with less in stock ...
M.FLOOR = {food = 10, wood = 25, stone = 5, gold = 5, iron = 5}   -- ... at least this percent of the gatherers

local OTHERS = {"wood", "stone", "gold", "iron"}

-- the stock correction of a split (want: gatherers by resource, food the rest; land: the citizens to split; stock and
-- reserve: the view's; opts.floor: percent by resource (default M.FLOOR); opts.lacks: by resource, true where the
-- plan still wants the resource (a low stock of one the plan has no use for is left alone); opts.give: the order
-- the gatherers for a low resource are taken from (default gold, stone, iron, food); opts.keep: the gatherers each
-- resource keeps at least when it gives).
-- 1. surplus (balanced surplus / surplus_cut): a resource with more than SURPLUS beyond its reserve keeps half its
--    gatherers; the rest go to food (the caller's places cap passes them on).
-- 2. low stock (balanced low_stock / floor): a resource under LOW_STOCK the plan wants gets at least its floor share
--    of the citizens, taken from the others in opts.give's order (never below opts.keep).
-- Returns a table of the changes for the trace ({wood = +3, gold = -3, ...}).
function M.correct(want, land, stock, reserve, opts)
    opts = opts or {}
    local floor = opts.floor or M.FLOOR
    local keep = opts.keep or {}
    local give = opts.give or {"gold", "stone", "iron", "food"}
    local delta = {}
    local function move(from, to, n)
        want[from] = want[from] - n
        want[to] = want[to] + n
        delta[from] = (delta[from] or 0) - n
        delta[to] = (delta[to] or 0) + n
    end
    for _, r in ipairs(OTHERS) do
        if stock[r] - (reserve[r] or 0) > M.SURPLUS and want[r] > 0 then
            move(r, "food", want[r] - want[r] * M.SURPLUS_CUT // 100)
        end
    end
    for _, r in ipairs(OTHERS) do
        if stock[r] < M.LOW_STOCK and (opts.lacks == nil or opts.lacks[r]) then
            local least = (land * (floor[r] or 0) + 99) // 100
            for _, g in ipairs(give) do
                if want[r] >= least then
                    break
                end
                if g ~= r then
                    local spare = want[g] - math.max(keep[g] or 0, g ~= "food" and (land * (floor[g] or 0) + 99) // 100 or 0)
                    if g ~= "food" and stock[g] < M.LOW_STOCK then
                        spare = 0   -- (it is low itself)
                    end
                    local n = math.min(math.max(spare, 0), least - want[r])
                    if n > 0 then
                        move(g, r, n)
                    end
                end
            end
        end
    end
    return delta
end

-- the epoch saving ramp (balanced epoch_pressure start / full: the reserve grows from nothing to the advance's whole
-- cost): the pressure in percent, the larger of the time's ramp (from_min .. full_min minutes into the epoch) and
-- the citizens' ramp (from_cit .. full_cit). ramp: {from_min, full_min, from_cit, full_cit}; ms: game ms into the
-- epoch.
function M.pressure(ramp, ms, citizens)
    local function part(x, a, b)
        if a == nil or b == nil then
            return 0
        end
        if x >= b then
            return 100
        end
        if x <= a then
            return 0
        end
        return (x - a) * 100 // (b - a)
    end
    local function ms_of(m)
        return m ~= nil and m * 60000 or nil
    end
    return math.max(part(ms, ms_of(ramp.from_min), ms_of(ramp.full_min)), part(citizens, ramp.from_cit, ramp.full_cit))
end

return M
