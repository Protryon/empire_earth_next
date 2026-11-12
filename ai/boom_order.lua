-- "Boom (owner order)" (ai/ais.json boom_order): boom with the owner's build order (lib.build_order, AI_RESEARCH §8.51)
-- and the fishing grounds' guard (boom-defence's B4, §8.50: the towers' attack once a warship is seen, 2 guard
-- warships, the boats' flight to a dock). The raid stays off; the 20-soldier reserve trains after the Copper advance
-- (PLAN[3]). The epoch's iron is kept from the army while it is short (ECON.iron_guard), and the guard then trains
-- soldiers that cost no iron (plan.iron_free): Bronze in 16 of 32 games against 11 (§8.50). The default "boom" entry is
-- unchanged.
return require("boom").configure{
    econ = {order = true, iron_guard = true},
    plan = {tower_up_ships = true, boat_guard = 2, iron_free = true},
    defence = {boats = true},
}
