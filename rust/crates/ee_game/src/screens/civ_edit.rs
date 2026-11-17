//! The civilization being edited in the builder whole: the original `EECBEditableCivilization` (a player's
//! civilization: points left, the upgrades picked here, a name, sent as `EEMCivUpgradesRequested` when applied) and
//! its subclass `EECBCivilizationCreator` (a civilization made in the editor: applied by nothing, always changeable;
//! unlimited with the free camera, its points then counting up as spent). The `EECivilization` it edits stays C++:
//! its costs, counts, points and name are host ops ([`crate::civ_whole::CbHost`]).

use core::cell::{Cell, RefCell};

use crate::screens::civ_whole::*;

/// The edited civilization. Used through `&self` (its state in cells): the list box's formatting reads its costs
/// while the builder's helpers hold it.
#[derive(Debug)]
pub struct Editable {
    /// The `EECivilization*` edited.
    pub civ: usize,
    /// The player (the low byte of the player id; the creator's 0) the request names.
    pub player: u8,
    /// `Some(unlimited)` for the creator.
    pub creator: Option<bool>,
    /// The upgrades picked here (`mUpgrades`).
    pub upgrades: RefCell<Vec<u32>>,
    /// `mPointsLeft` (a long: the points left; the unlimited creator's points spent).
    pub points: Cell<i32>,
    /// `mName` (UTF-8).
    pub name: RefCell<Vec<u8>>,
}

/// An upgrade's category (`EECivilizationManager::SplitAttribute`).
pub fn category(upgrade: u32) -> u32 {
    upgrade >> 16
}

impl Editable {
    /// The ctor (`EECBEditableCivilization(civ, player)`: back to the start, then the civilization's name); the
    /// creator's (`EECBCivilizationCreator(civ, unlimited)`: player 0, no points when unlimited).
    pub fn new<H: CbHost + ?Sized>(h: &mut H, civ: usize, player: u8, creator: Option<bool>) -> Self {
        let e = Self {
            civ,
            player,
            creator,
            upgrades: RefCell::new(Vec::new()),
            points: Cell::new(0),
            name: RefCell::new(Vec::new()),
        };
        e.reset_to_start(h);
        let (_, name) = h.t(EE_SCREENS_CBW_C_NAME, civ, 0, 0, 0);
        *e.name.borrow_mut() = name;
        if creator == Some(true) {
            e.points.set(0);
        }
        e
    }

    fn unlimited(&self) -> bool {
        self.creator == Some(true)
    }

    /// `Apply`: the picked upgrades and the name requested (to the server in multiplayer, executed otherwise) when
    /// anything was chosen; the creator applies nothing (true).
    pub fn apply<H: CbHost + ?Sized>(&self, h: &mut H) -> bool {
        if self.creator.is_some() {
            return true;
        }
        if !self.has_choices(h) {
            return false;
        }
        let name = self.name();
        let mut text = name.clone();
        for u in self.upgrades.borrow().iter() {
            text.extend_from_slice(&u.to_le_bytes());
        }
        h.op(
            EE_SCREENS_CBW_REQUEST,
            0,
            self.player as i64,
            name.len() as i64,
            0,
            &text,
            &mut EeScreensCbOut::default(),
        );
        true
    }

    fn costs<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> (i64, i64) {
        let base = h.v(EE_SCREENS_CBW_C_BASE_COST, self.civ, upgrade as i64, 0, 0);
        let extra = h.v(EE_SCREENS_CBW_C_EXTRA_COST, self.civ, upgrade as i64, 0, 0);
        (base, extra)
    }

    /// `GetCost`: the cost given how many of its category are picked.
    pub fn cost<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> i64 {
        let (base, extra) = self.costs(h, upgrade);
        let count = self.picked_in_category(h, upgrade);
        h.v(EE_SCREENS_CBW_C_COMPUTE, self.civ, base, extra, count as i64)
    }

    /// `GetRefund`: the cost with one fewer of its category picked.
    pub fn refund<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> i64 {
        let (base, extra) = self.costs(h, upgrade);
        let count = self.picked_in_category(h, upgrade).saturating_sub(1);
        h.v(EE_SCREENS_CBW_C_COMPUTE, self.civ, base, extra, count as i64)
    }

    /// `ResetToStart`: the default name, the civilization's points, nothing picked.
    pub fn reset_to_start<H: CbHost + ?Sized>(&self, h: &mut H) {
        let (_, name) = h.string(IDS_DEFAULT_CIVILIZATION);
        *self.name.borrow_mut() = name;
        self.points.set(h.v(EE_SCREENS_CBW_C_AVAILABLE, self.civ, 0, 0, 0) as i32);
        self.upgrades.borrow_mut().clear();
    }

    /// `PickedInCategory`: the upgrades of its category picked here and owned by the civilization.
    pub fn picked_in_category<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> u32 {
        let here = self.upgrades.borrow().iter().filter(|&&u| category(u) == category(upgrade)).count() as u32;
        let owned = h.v(EE_SCREENS_CBW_C_COUNT, self.civ, upgrade as i64, 0, 0) as u32;
        owned.wrapping_add(here)
    }

    /// `CanChange`: the creator always; in multiplayer an unchanged civilization; else while the current player
    /// plays.
    pub fn can_change<H: CbHost + ?Sized>(&self, h: &mut H) -> bool {
        if self.creator.is_some() {
            return true;
        }
        if h.v(EE_SCREENS_CBW_MULTIPLAYER, 0, 0, 0, 0) != 0 {
            h.v(EE_SCREENS_CBW_C_UNCHANGED, self.civ, 0, 0, 0) != 0
        } else {
            h.v(EE_SCREENS_CBW_GAME_RESULT, 0, 0, 0, 0) == 0
        }
    }

    /// `GetPointsLeft` (an unsigned long): the unlimited creator's 0.
    pub fn points_left(&self) -> u32 {
        if self.unlimited() {
            0
        } else {
            self.points.get() as u32
        }
    }

    /// `GetMaxPoints`: the civilization's; the unlimited creator's points spent.
    pub fn max_points<H: CbHost + ?Sized>(&self, h: &mut H) -> i64 {
        if self.unlimited() {
            self.points.get() as i64
        } else {
            h.v(EE_SCREENS_CBW_C_MAX, self.civ, 0, 0, 0)
        }
    }

    /// `AddUpgrade`: picked and paid when affordable (the unlimited creator: always, its cost counted as spent).
    pub fn add_upgrade<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> bool {
        if upgrade == 0 {
            return false;
        }
        // an unsigned long cost against the unsigned points left
        let cost = self.cost(h, upgrade) as i32;
        if self.unlimited() {
            self.upgrades.borrow_mut().push(upgrade);
            self.points.set(self.points.get().wrapping_add(cost));
            return true;
        }
        if cost as u32 <= self.points_left() {
            self.upgrades.borrow_mut().push(upgrade);
            self.points.set(self.points.get().wrapping_sub(cost));
            return true;
        }
        false
    }

    /// `RemoveUpgrade`: a picked upgrade dropped and refunded (the unlimited creator: taken off the points spent).
    pub fn remove_upgrade<H: CbHost + ?Sized>(&self, h: &mut H, upgrade: u32) -> bool {
        let Some(at) = self.upgrades.borrow().iter().position(|&u| u == upgrade) else { return false };
        let delta = self.refund(h, upgrade) as i32;
        if self.unlimited() {
            self.points.set(self.points.get().wrapping_sub(delta));
        } else {
            self.points.set(self.points.get().wrapping_add(delta));
        }
        self.upgrades.borrow_mut().remove(at);
        true
    }

    /// `CopyUpgradeIDs`: the picked upgrades, then the civilization's purchased ones.
    pub fn copy_upgrade_ids<H: CbHost + ?Sized>(&self, h: &mut H) -> Vec<u32> {
        let mut ids = self.upgrades.borrow().clone();
        ids.extend(self.civilization_upgrades(h));
        ids
    }

    /// `HasChoices`: an upgrade picked or the name changed.
    pub fn has_choices<H: CbHost + ?Sized>(&self, h: &mut H) -> bool {
        if !self.upgrades.borrow().is_empty() {
            return true;
        }
        let (_, civ_name) = h.t(EE_SCREENS_CBW_C_NAME, self.civ, 0, 0, 0);
        *self.name.borrow() != civ_name
    }

    /// `GetIsUnchanged`: the civilization's flag.
    pub fn is_unchanged<H: CbHost + ?Sized>(&self, h: &mut H) -> bool {
        h.v(EE_SCREENS_CBW_C_UNCHANGED, self.civ, 0, 0, 0) != 0
    }

    /// `GetName`.
    pub fn name(&self) -> Vec<u8> {
        self.name.borrow().clone()
    }

    /// `SetName`.
    pub fn set_name(&self, name: &[u8]) {
        *self.name.borrow_mut() = name.to_vec();
    }

    /// `GetCivilizationUpgrades`: the civilization's purchased upgrades.
    pub fn civilization_upgrades<H: CbHost + ?Sized>(&self, h: &mut H) -> Vec<u32> {
        h.vs(EE_SCREENS_CBW_C_PURCHASED, self.civ, 0, 0).1.into_iter().map(|v| v as u32).collect()
    }
}
