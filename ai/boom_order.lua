-- "Boom (owner order)" (ai/ais.json boom_order): boom with the owner's build order (lib.build_order, AI_RESEARCH §8.51)
-- and the fishing grounds' guard (boom-defence's B4, §8.50: the towers' attack once a warship is seen, 2 guard
-- warships, the boats' flight to a dock). The raid stays off; the 20-soldier reserve trains after the Copper advance
-- (PLAN[3]); during the Bronze save half the guard's iron infantry share goes to ranged soldiers (iron_lean, §8.50).
-- The shore scout walks the shore by home before the Stone advance (the dock's site). The default "boom"
-- entry is unchanged.
return require("boom").configure{
    econ = {order = true},
    order = {shore_scout = true, boat_wood = true, lua_farms = true, soldiers_after_boats = true, blocked_skip = true},
    plan = {tower_up_ships = true, boat_guard = 2, iron_lean = 50},
    defence = {boats = true},
}
