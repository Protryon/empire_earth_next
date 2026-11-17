//! The scenario editor tab hosts' plain world reads (object storage lane S), answered over the world's views
//! (`ee_sim::world`) instead of their C++ cases in `Scenario Editor/EESERBUnitTypes.cpp` (`ee_screenscpp_sepl`) and
//! `EESEUnit.cpp` (`ee_screenscpp_seun`): the player count, a player's presence, computer player, enemy table and
//! id, and the unit class table's count and placement flags. The other ops (writes, the toolkit, the tech tree, the
//! computer player's flags, the editor's selection) stay the C++ host's.
//!
//! Threads (docs/progress/obj-lane-s.md "Threads"): these reads run on the UI thread under the world's write lock
//! (an action, the tab switch), under its read lock (`UpdateUI`), or on the editor's construction thread with none;
//! the token takes no lock, so it is right in all three.

use ee_sim::world::{ClassView, PlayerView, WorldRead, WorldView};

use crate::screens::editor_players::*;
use crate::screens::editor_unit::*;

/// `kAIKindHero`, `kUnitCategoryLandMilitaryHero` (ee/EEUnitClassIDs.h).
const AI_KIND_HERO: u64 = 0x1b;
const CATEGORY_LAND_MILITARY_HERO: u64 = 0x17;
/// `kMovementAir`, `kMovementLand`, `kMovementSea`, `kMovementSpace`.
const MOVEMENT_AIR: u8 = 1;
const MOVEMENT_LAND: u8 = 2;
const MOVEMENT_SEA: u8 = 4;
const MOVEMENT_SPACE: u8 = 8;

/// The world's reads an editor op needs.
pub(crate) trait EditorWorld {
    /// `gWorld.GetNumPlayers()`.
    fn num_players(&self) -> u64;
    /// `gWorld.mPlayers[i]`: (its `mPlayerID`, its computer player is set, `mEnemies`), None for an empty slot.
    fn player(&self, i: usize) -> Option<(u32, bool, [u64; 17])>;
    /// `gUnitClassTable.size()`.
    fn class_count(&self) -> usize;
    /// `gUnitClassTable[i]`'s placement facts, None for an empty slot.
    fn class(&self, i: usize) -> Option<ClassFacts>;
}

/// A class's facts as `EE_SCREENS_SEUN_OP_CLASS` reports them.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub(crate) struct ClassFacts {
    pub unit_type: u64,
    pub scenario_selectable: bool,
    pub gaia_placeable: bool,
    pub player_placeable: bool,
    pub ai_kind: u64,
    pub category: u64,
    pub domains: u8,
    pub land_building: bool,
}

/// An op's answer from the world (`None`: the C++ host's op). `i` and `f` as the host's (in and out).
pub(crate) fn answer(w: &impl EditorWorld, op: u32, i: &mut [i32; 4], f: &mut [f32; 4]) -> Option<i32> {
    let _ = f;
    let slot = |i: i32| usize::try_from(i).ok();
    Some(match op {
        EE_SCREENS_SEPL_OP_NUM_PLAYERS => w.num_players() as i32,
        EE_SCREENS_SEPL_OP_PLAYER_EXISTS => slot(i[0]).and_then(|p| w.player(p)).is_some() as i32,
        EE_SCREENS_SEPL_OP_IS_COMPUTER => slot(i[0]).and_then(|p| w.player(p)).is_some_and(|p| p.1) as i32,
        EE_SCREENS_SEPL_OP_ENEMY => match (slot(i[0]).and_then(|p| w.player(p)), slot(i[1])) {
            (Some(p), Some(j)) if j < 17 => p.2[j] as i32,
            _ => 0,
        },
        EE_SCREENS_SEPL_OP_PLAYER_GET if i[1] == EE_SCREENS_SEPL_PV_ID => {
            slot(i[0]).and_then(|p| w.player(p)).map_or(0, |p| p.0 as i32)
        }
        EE_SCREENS_SEUN_OP_CLASS_COUNT => w.class_count() as i32,
        EE_SCREENS_SEUN_OP_CLASS => {
            let Some(c) = slot(i[0]).and_then(|k| w.class(k)) else { return Some(0) };
            i[1] = c.unit_type as i32;
            let mut flags = 0;
            let mut set = |on: bool, flag: i32| {
                if on {
                    flags |= flag;
                }
            };
            set(c.scenario_selectable, EE_SCREENS_SEUN_CLASS_SELECTABLE);
            set(c.gaia_placeable, EE_SCREENS_SEUN_CLASS_GAIA);
            set(c.player_placeable, EE_SCREENS_SEUN_CLASS_PLAYER);
            set(c.ai_kind == AI_KIND_HERO || c.category == CATEGORY_LAND_MILITARY_HERO, EE_SCREENS_SEUN_CLASS_HERO);
            set(c.domains & MOVEMENT_AIR != 0, EE_SCREENS_SEUN_CLASS_AIR);
            set(c.land_building, EE_SCREENS_SEUN_CLASS_BUILDING);
            set(c.domains & MOVEMENT_LAND != 0, EE_SCREENS_SEUN_CLASS_LAND);
            set(c.domains & MOVEMENT_SEA != 0, EE_SCREENS_SEUN_CLASS_SEA);
            set(c.domains & MOVEMENT_SPACE != 0, EE_SCREENS_SEUN_CLASS_SPACE);
            flags
        }
        _ => return None,
    })
}

/// The game's world through its views (one token per op).
#[cfg_attr(test, allow(dead_code))]
pub(crate) struct Views<'t>(pub(crate) &'t WorldRead);

#[cfg_attr(test, allow(dead_code))]
impl Views<'_> {
    fn world(&self) -> Option<WorldView<'_>> {
        WorldView::global(self.0)
    }
}

#[cfg_attr(test, allow(dead_code))]
fn player_facts(p: PlayerView<'_>) -> (u32, bool, [u64; 17]) {
    (p.id(), p.has_computer_player(), std::array::from_fn(|j| p.enemy(j)))
}

impl EditorWorld for Views<'_> {
    fn num_players(&self) -> u64 {
        self.world().map_or(0, |w| w.num_players())
    }
    fn player(&self, i: usize) -> Option<(u32, bool, [u64; 17])> {
        self.world()?.player(i).map(player_facts)
    }
    fn class_count(&self) -> usize {
        ClassView::count(self.0)
    }
    fn class(&self, i: usize) -> Option<ClassFacts> {
        let c = ClassView::by_index(self.0, i)?;
        let r = c.fields();
        Some(ClassFacts {
            unit_type: c.unit_type(),
            scenario_selectable: ee_sim::world_field!(r, mIsScenarioSelectable),
            gaia_placeable: ee_sim::world_field!(r, mIsGaiaPlaceable),
            player_placeable: ee_sim::world_field!(r, mIsPlayerPlaceable),
            ai_kind: c.ai_kind(),
            category: c.category(),
            domains: ee_sim::world_field!(r, mDomains),
            land_building: c.land_building(),
        })
    }
}

/// An editor tab host op: traced (SGVW), answered here when it is a plain world read, else by `call` (C++).
#[cfg(not(test))]
pub(crate) fn host(
    op: u32,
    i: &mut [i32; 4],
    f: &mut [f32; 4],
    call: impl FnOnce(&mut [i32; 4], &mut [f32; 4]) -> i32,
) -> i32 {
    crate::screens::sgvw::editor(op, i, f, |i, f| {
        // SAFETY: the editor's reads hold the world (the caller's lock, or no game running; see the module's note)
        let t = unsafe { WorldRead::begin() };
        let r = answer(&Views(&t), op, i, f);
        drop(t);
        r.unwrap_or_else(|| call(i, f))
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Fake {
        players: Vec<Option<(u32, bool, [u64; 17])>>,
        classes: Vec<Option<ClassFacts>>,
    }

    impl EditorWorld for Fake {
        fn num_players(&self) -> u64 {
            self.players.len() as u64
        }
        fn player(&self, i: usize) -> Option<(u32, bool, [u64; 17])> {
            self.players.get(i).copied().flatten()
        }
        fn class_count(&self) -> usize {
            self.classes.len()
        }
        fn class(&self, i: usize) -> Option<ClassFacts> {
            self.classes.get(i).copied().flatten()
        }
    }

    fn fake() -> Fake {
        let mut enemies = [0u64; 17];
        enemies[2] = 1;
        let hero = ClassFacts { unit_type: 77, ai_kind: AI_KIND_HERO, domains: MOVEMENT_LAND, ..Default::default() };
        let ship = ClassFacts {
            unit_type: 5,
            scenario_selectable: true,
            gaia_placeable: true,
            player_placeable: true,
            category: CATEGORY_LAND_MILITARY_HERO,
            domains: MOVEMENT_AIR | MOVEMENT_SEA | MOVEMENT_SPACE,
            land_building: true,
            ..Default::default()
        };
        Fake {
            players: vec![Some((0, false, [0; 17])), None, Some((2, true, enemies))],
            classes: vec![Some(hero), None, Some(ship)],
        }
    }

    fn ask(op: u32, i: [i32; 4]) -> (Option<i32>, [i32; 4]) {
        let mut i = i;
        let r = answer(&fake(), op, &mut i, &mut [0.0; 4]);
        (r, i)
    }

    #[test]
    fn players() {
        assert_eq!(ask(EE_SCREENS_SEPL_OP_NUM_PLAYERS, [0; 4]).0, Some(3));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_PLAYER_EXISTS, [2, 0, 0, 0]).0, Some(1));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_PLAYER_EXISTS, [1, 0, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_IS_COMPUTER, [2, 0, 0, 0]).0, Some(1));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_IS_COMPUTER, [0, 0, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_IS_COMPUTER, [1, 0, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_ENEMY, [2, 2, 0, 0]).0, Some(1));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_ENEMY, [2, 3, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_ENEMY, [1, 2, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEPL_OP_PLAYER_GET, [2, EE_SCREENS_SEPL_PV_ID, 0, 0]).0, Some(2));
        // the player's other values and every set are the C++ host's
        assert_eq!(ask(EE_SCREENS_SEPL_OP_PLAYER_GET, [2, EE_SCREENS_SEPL_PV_CAP, 0, 0]).0, None);
        assert_eq!(ask(EE_SCREENS_SEPL_OP_PLAYER_SET, [2, EE_SCREENS_SEPL_PV_ID, 0, 0]).0, None);
        assert_eq!(ask(EE_SCREENS_SEPL_OP_WORLD_GET, [0; 4]).0, None);
    }

    #[test]
    fn classes() {
        assert_eq!(ask(EE_SCREENS_SEUN_OP_CLASS_COUNT, [0; 4]).0, Some(3));
        assert_eq!(
            ask(EE_SCREENS_SEUN_OP_CLASS, [0, 0, 0, 0]),
            (Some(EE_SCREENS_SEUN_CLASS_HERO | EE_SCREENS_SEUN_CLASS_LAND), [0, 77, 0, 0])
        );
        // an empty slot: 0, i[1] untouched
        assert_eq!(ask(EE_SCREENS_SEUN_OP_CLASS, [1, 9, 0, 0]), (Some(0), [1, 9, 0, 0]));
        let all = EE_SCREENS_SEUN_CLASS_SELECTABLE
            | EE_SCREENS_SEUN_CLASS_GAIA
            | EE_SCREENS_SEUN_CLASS_PLAYER
            | EE_SCREENS_SEUN_CLASS_HERO
            | EE_SCREENS_SEUN_CLASS_AIR
            | EE_SCREENS_SEUN_CLASS_BUILDING
            | EE_SCREENS_SEUN_CLASS_SEA
            | EE_SCREENS_SEUN_CLASS_SPACE;
        assert_eq!(ask(EE_SCREENS_SEUN_OP_CLASS, [2, 0, 0, 0]), (Some(all), [2, 5, 0, 0]));
        assert_eq!(ask(EE_SCREENS_SEUN_OP_CLASS, [3, 0, 0, 0]).0, Some(0));
        assert_eq!(ask(EE_SCREENS_SEUN_OP_LIST_ADD, [0; 4]).0, None);
    }
}
