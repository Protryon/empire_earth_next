//! `EEUnit::TakeHit` over its C++ statements (the units' lane, round 7; was EEUnit.cpp). Every C++ statement of the
//! original is one operation of [`EuHost::eu`] (the game's: `ee_units_eu_host`, EEUnit.cpp, one statement a case),
//! asked in the original's order; the choices and the hit's bookkeeping are here. The combat formulas
//! (`EECombatHitPoints`, `EECombatDamageEffect`, the morale) stay C++ statements: they record their CB* captures.
//! `unit` is the `EEUnit*` hit, `a` an object, `b`, `c` values.

// ---- the statements (`op`; the answer)
/// `mComplexUnit`
pub const EE_UNITS_EU_COMPLEX: i32 = 1;
/// `mComplexUnit->mPreviousPart`
pub const EE_UNITS_EU_PREVIOUS_PART: i32 = 2;
/// complex unit `a`'s `TakeHit(the four values at b: attacker, damage, player, mode)`
pub const EE_UNITS_EU_PART_TAKE_HIT: i32 = 3;
/// `gWorld.mInCinematic`
pub const EE_UNITS_EU_IN_CINEMATIC: i32 = 4;
/// `mComplexUnit->mIsDisabled && !mComplexUnit->IsProtected()`
pub const EE_UNITS_EU_DISABLED_UNPROTECTED: i32 = 5;
/// `mHitPoints`
pub const EE_UNITS_EU_HIT_POINTS: i32 = 6;
/// `mUnitClass->mImpactSparksEffect != NULL`
pub const EE_UNITS_EU_HAS_SPARKS: i32 = 7;
/// `gEffectManager->CreateUnitEffect(this, mUnitClass->mImpactSparksEffect, NULL, false, 0, 0)`
pub const EE_UNITS_EU_SPARKS: i32 = 8;
/// `mUnitClass->mImpactBloodEffect != NULL`
pub const EE_UNITS_EU_HAS_BLOOD: i32 = 9;
/// `gEffectManager->CreateUnitEffect(this, mUnitClass->mImpactBloodEffect, NULL, false, 0, 0)`
pub const EE_UNITS_EU_BLOOD: i32 = 10;
/// the modifiers' input `= b` (the original's `theInput`)
pub const EE_UNITS_EU_SET_INPUT: i32 = 11;
/// the area damage `= b` (`theAreaDamage`)
pub const EE_UNITS_EU_SET_AREA_DAMAGE: i32 = 12;
/// `mComplexUnit->mAffectingAreaEffects.ApplyModifiers(1, theInput, theAreaDamage, mComplexUnit)`
pub const EE_UNITS_EU_APPLY_MODIFIERS: i32 = 13;
/// the area damage
pub const EE_UNITS_EU_AREA_DAMAGE: i32 = 14;
/// the damage `= b` (`theDamage`)
pub const EE_UNITS_EU_SET_DAMAGE: i32 = 15;
/// `mComplexUnit->ApplyMorale(theAreaDamage, theDamage)`
pub const EE_UNITS_EU_APPLY_MORALE: i32 = 16;
/// the damage
pub const EE_UNITS_EU_DAMAGE: i32 = 17;
/// unit `a`'s `mTileNode != NULL`
pub const EE_UNITS_EU_ON_MAP: i32 = 18;
/// unit `a`'s player is not this unit's and is its enemy (`mPlayer->mEnemies[a->mPlayer->mPlayerID] != 0`)
pub const EE_UNITS_EU_IS_ENEMY: i32 = 19;
/// unit `a`'s `mComplexUnit->mOwner`
pub const EE_UNITS_EU_OWNER: i32 = 20;
/// `ee_units_uu_targetable(this, unit a's GetUnitClass(), 0)`
pub const EE_UNITS_EU_TARGETABLE: i32 = 21;
/// unit `a`'s `mComplexUnit`
pub const EE_UNITS_EU_COMPLEX_OF: i32 = 22;
/// `mComplexUnit->SetLastAttacker(a)`
pub const EE_UNITS_EU_SET_LAST_ATTACKER: i32 = 23;
/// `mComplexUnit->mCommand != NULL && mComplexUnit->mCommand->mType == kActionIdle`
pub const EE_UNITS_EU_IS_IDLE: i32 = 24;
/// `mComplexUnit->mUnitClass->FindAction(kActionUnderAttack) != NULL`
pub const EE_UNITS_EU_HAS_UNDER_ATTACK: i32 = 25;
/// `mComplexUnit->StartAction(kActionUnderAttack)`
pub const EE_UNITS_EU_START_UNDER_ATTACK: i32 = 26;
/// `mComplexUnit->mUnitClass->ActionChanged(mComplexUnit)`
pub const EE_UNITS_EU_ACTION_CHANGED: i32 = 27;
/// unit `a`'s class's shot effect is 3D tracers (`mShotEffect != NULL && mShotEffect->mKind == kEffect3DTracers`)
pub const EE_UNITS_EU_SHOOTS_TRACERS: i32 = 28;
/// the hit formula `EECombatHitPoints({b, c, mStats->mMaxHitPoints})` (CBTH), its answer kept
pub const EE_UNITS_EU_HIT_FORMULA: i32 = 29;
/// the kept answer's `badly_hurt`
pub const EE_UNITS_EU_HIT_BADLY_HURT: i32 = 30;
/// the kept answer's `hit_points`
pub const EE_UNITS_EU_HIT_NEW_POINTS: i32 = 31;
/// `mUnitClass->BadlyHurt(this)`
pub const EE_UNITS_EU_BADLY_HURT: i32 = 32;
/// `dynamic_cast<EEPlayer*>(mPlayer)->DamageTaken(b, c, this)`
pub const EE_UNITS_EU_DAMAGE_TAKEN: i32 = 33;
/// `gWorld.mNumPlayers`
pub const EE_UNITS_EU_NUM_PLAYERS: i32 = 34;
/// player `b` has a computer player with a Lua units state
pub const EE_UNITS_EU_HAS_UNITS_STATE: i32 = 35;
/// `EECPUnitsOnDealt(gWorld.mPlayers[b]->mComputerPlayer, c, this)`
pub const EE_UNITS_EU_ON_DEALT: i32 = 36;
/// unit `a`'s `mTileX` (0: this unit's)
pub const EE_UNITS_EU_TILE_X: i32 = 37;
/// unit `a`'s `mTileY` (0: this unit's)
pub const EE_UNITS_EU_TILE_Y: i32 = 38;
/// complex unit `a`'s `mContainer`
pub const EE_UNITS_EU_CONTAINER: i32 = 39;
/// `thePlayer->mEvents.AddPlayerEvent(EETileLocation(mTileX, mTileY), EETileLocation(b, c), 1, thePlayer, -1, false)`
pub const EE_UNITS_EU_ADD_ATTACK_EVENT: i32 = 40;
/// `mUnitClass->mDamageEffect`
pub const EE_UNITS_EU_DAMAGE_EFFECT: i32 = 41;
/// `mTileNode == NULL || mComplexUnit->mIsBuilt`
pub const EE_UNITS_EU_SHOWS_DAMAGE: i32 = 42;
/// the damage effect formula `EECombatDamageEffect({mHitPoints, b, mStats->mMaxHitPoints, effect a's GetParam2()})`
/// (CBDE)
pub const EE_UNITS_EU_DAMAGE_EFFECT_FORMULA: i32 = 43;
/// `mUnitClass->mCategory`
pub const EE_UNITS_EU_CATEGORY: i32 = 44;
/// `gEffectManager->CreateUnitEffect(this, effect a, NULL, false, 0, 0)`
pub const EE_UNITS_EU_EFFECT_ON_SELF: i32 = 45;
/// the gate's last linked part's damage effect (`GetLastLinked(mComplexUnit)`)
pub const EE_UNITS_EU_GATE_EFFECT_LINKED: i32 = 46;
/// the gate's last linked carrier's damage effect (`GetLastLinkedCarrier(mComplexUnit)`)
pub const EE_UNITS_EU_GATE_EFFECT_CARRIER: i32 = 47;
/// `mHitPoints = b`
pub const EE_UNITS_EU_SET_HIT_POINTS: i32 = 48;
/// `mComplexUnit->mTargetSearchTimer`
pub const EE_UNITS_EU_TARGET_SEARCH_TIMER: i32 = 49;
/// `mComplexUnit->mTargetSearchTimer = 0`
pub const EE_UNITS_EU_CLEAR_TARGET_SEARCH_TIMER: i32 = 50;
/// `mComplexUnit->mUnitClass->mCategory`
pub const EE_UNITS_EU_COMPLEX_CATEGORY: i32 = 51;
/// `mComplexUnit->mAmmunition`
pub const EE_UNITS_EU_AMMUNITION: i32 = 52;
/// `mComplexUnit->mFlightTime`
pub const EE_UNITS_EU_FLIGHT_TIME: i32 = 53;
/// `mComplexUnit->mFlightTime = 0`
pub const EE_UNITS_EU_CLEAR_FLIGHT_TIME: i32 = 54;
/// `mPlayer->mPlayerID`
pub const EE_UNITS_EU_PLAYER_ID: i32 = 55;
/// `gWorld.mPlayers[0]->mPlayerID`
pub const EE_UNITS_EU_FIRST_PLAYER_ID: i32 = 56;
/// `gWorld.mPlayers[b]->mSavedGroups.Reset(c, 1)`
pub const EE_UNITS_EU_STATS_RESET: i32 = 57;
/// `Kill(true, false, false)`
pub const EE_UNITS_EU_KILL: i32 = 58;
/// `mUnitClass->mUnitType`
pub const EE_UNITS_EU_UNIT_TYPE: i32 = 59;
/// `gEffectManager->CreateUnitEffect(this, EEGraphicEffect::Get(kGraphicEffectAircraftExplosion), NULL, false, 0, 0)`
pub const EE_UNITS_EU_ICBM_EXPLOSION: i32 = 60;
/// `mPlayer == gWorld.GetCurrentPlayer()`
pub const EE_UNITS_EU_IS_CURRENT_PLAYER: i32 = 61;
/// `gMiniMap->Ping(GE2DPoint((float)mTileX, (float)mTileY))`
pub const EE_UNITS_EU_MINIMAP_PING: i32 = 62;
/// `mPlayer->mSelectedGroup->Contains(mComplexUnit)`
pub const EE_UNITS_EU_SELECTED: i32 = 63;
/// `gUserInterface->PostMessage424Once()`
pub const EE_UNITS_EU_POST_424: i32 = 64;

/// `kActionUnderAttack` is the C++ statements'; the categories, the ICBM and the off-map tile are here
const CATEGORY_AMBIENT: i64 = 0;
const CATEGORY_RESOURCE: i64 = 1;
const CATEGORY_BUILDING: i64 = 2;
const CATEGORY_AIRPLANE: i64 = 3;
const CATEGORY_BUILDING19: i64 = 0x13;
const CATEGORY_MINING_BUILDING: i64 = 0x18;
const CATEGORY_GATE: i64 = 0x19;
const CATEGORY_PLANET_SPACESHIP: i64 = 0x22;
const UNIT_TYPE_CP_ICBM: i64 = 0x2ee;
/// the tile of a unit off the map
const OFF_MAP: i64 = -10000;

/// The C++ statements of the body.
pub trait EuHost {
    /// one statement (`EE_UNITS_EU_*`) on the unit `unit`
    fn eu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64;
}

struct S<'h, H: EuHost + ?Sized> {
    h: &'h mut H,
    unit: u64,
}

impl<H: EuHost + ?Sized> S<'_, H> {
    fn q(&mut self, op: i32) -> i64 {
        self.h.eu(op, self.unit, 0, 0, 0)
    }
    fn qa(&mut self, op: i32, a: u64) -> i64 {
        self.h.eu(op, self.unit, a, 0, 0)
    }
    fn qb(&mut self, op: i32, b: i64) -> i64 {
        self.h.eu(op, self.unit, 0, b, 0)
    }
    fn qab(&mut self, op: i32, a: u64, b: i64) -> i64 {
        self.h.eu(op, self.unit, a, b, 0)
    }
    fn qbc(&mut self, op: i32, b: i64, c: i64) -> i64 {
        self.h.eu(op, self.unit, 0, b, c)
    }
    fn yes(&mut self, op: i32) -> bool {
        self.q(op) != 0
    }
}

/// The impact effect: sparks, else blood.
fn impact<H: EuHost + ?Sized>(s: &mut S<'_, H>, sparks: bool) {
    if s.yes(EE_UNITS_EU_HAS_SPARKS) && sparks {
        s.q(EE_UNITS_EU_SPARKS);
    } else if s.yes(EE_UNITS_EU_HAS_BLOOD) {
        s.q(EE_UNITS_EU_BLOOD);
    }
}

/// `TakeHit(attacker, damage, player, mode)`: the area effects and the morale adjust the damage and the attacker is
/// remembered; then the unit loses hit points with damage and impact effects, an attack event and a minimap ping,
/// or dies. A linked part's hit goes to its first part.
pub fn take_hit<H: EuHost + ?Sized>(h: &mut H, unit: u64, attacker: u64, damage_in: i64, player: i64, mode: i64) {
    let s = &mut S { h, unit };
    let complex = s.q(EE_UNITS_EU_COMPLEX) as u64;
    if complex != 0 {
        let previous = s.q(EE_UNITS_EU_PREVIOUS_PART) as u64;
        if previous != 0 {
            let args = [attacker as i64, damage_in, player, mode];
            s.qab(EE_UNITS_EU_PART_TAKE_HIT, previous, args.as_ptr() as usize as i64);
            return;
        }
    }
    if s.yes(EE_UNITS_EU_IN_CINEMATIC) || (complex != 0 && s.yes(EE_UNITS_EU_DISABLED_UNPROTECTED)) {
        if s.q(EE_UNITS_EU_HIT_POINTS) > 0 {
            impact(s, true);
        }
        return;
    }
    let mut hit_points = s.q(EE_UNITS_EU_HIT_POINTS);
    if hit_points <= 0 {
        return;
    }
    let mut damage = damage_in;
    let mut suppress = false;
    if complex != 0 {
        if mode != 2 {
            s.qb(EE_UNITS_EU_SET_INPUT, damage_in);
            s.qb(EE_UNITS_EU_SET_AREA_DAMAGE, damage_in);
            s.q(EE_UNITS_EU_APPLY_MODIFIERS);
            s.qb(EE_UNITS_EU_SET_DAMAGE, damage);
            s.q(EE_UNITS_EU_APPLY_MORALE);
            damage = s.q(EE_UNITS_EU_DAMAGE);
        }
        if attacker != 0 && s.qa(EE_UNITS_EU_ON_MAP, attacker) != 0 && s.qa(EE_UNITS_EU_IS_ENEMY, attacker) != 0 {
            let mut target = 0u64;
            let owner = s.qa(EE_UNITS_EU_OWNER, attacker) as u64;
            if owner == 0 {
                if s.qa(EE_UNITS_EU_TARGETABLE, attacker) != 0 {
                    target = s.qa(EE_UNITS_EU_COMPLEX_OF, attacker) as u64;
                }
            } else if s.qa(EE_UNITS_EU_TARGETABLE, owner) != 0 {
                target = owner;
            }
            if target != 0 {
                s.qa(EE_UNITS_EU_SET_LAST_ATTACKER, target);
                if s.yes(EE_UNITS_EU_IS_IDLE) && s.yes(EE_UNITS_EU_HAS_UNDER_ATTACK) {
                    s.q(EE_UNITS_EU_START_UNDER_ATTACK);
                    s.q(EE_UNITS_EU_ACTION_CHANGED);
                }
                if s.qa(EE_UNITS_EU_SHOOTS_TRACERS, target) != 0 {
                    suppress = true;
                }
            }
        }
    }
    s.qbc(EE_UNITS_EU_HIT_FORMULA, hit_points, damage);
    if s.yes(EE_UNITS_EU_HIT_BADLY_HURT) {
        s.q(EE_UNITS_EU_BADLY_HURT);
    }
    hit_points = s.q(EE_UNITS_EU_HIT_NEW_POINTS);
    s.qbc(EE_UNITS_EU_DAMAGE_TAKEN, player, damage);
    // new: the attacking computer player's Lua units interface counts the hit it dealt
    if player > 0 && (player as u32 as u64) < (s.q(EE_UNITS_EU_NUM_PLAYERS) as u64) && s.qb(EE_UNITS_EU_HAS_UNITS_STATE, player) != 0 {
        s.qbc(EE_UNITS_EU_ON_DEALT, player, damage);
    }
    let tile_x = s.qa(EE_UNITS_EU_TILE_X, 0);
    if tile_x != OFF_MAP {
        if attacker != 0 {
            let owner = if s.qa(EE_UNITS_EU_ON_MAP, attacker) != 0 { s.qa(EE_UNITS_EU_OWNER, attacker) as u64 } else { 0 };
            let from = if owner != 0 {
                let container = s.qa(EE_UNITS_EU_CONTAINER, owner) as u64;
                if container == 0 {
                    owner
                } else {
                    container
                }
            } else {
                attacker
            };
            let mut x = s.qa(EE_UNITS_EU_TILE_X, from);
            let mut y = s.qa(EE_UNITS_EU_TILE_Y, from);
            if x < 0 || y < 0 {
                x = s.qa(EE_UNITS_EU_TILE_X, 0);
                y = s.qa(EE_UNITS_EU_TILE_Y, 0);
            }
            s.qbc(EE_UNITS_EU_ADD_ATTACK_EVENT, x, y);
        } else {
            let x = s.qa(EE_UNITS_EU_TILE_X, 0);
            let y = s.qa(EE_UNITS_EU_TILE_Y, 0);
            s.qbc(EE_UNITS_EU_ADD_ATTACK_EVENT, x, y);
        }
    }
    if hit_points > 0 {
        let effect = s.q(EE_UNITS_EU_DAMAGE_EFFECT) as u64;
        if effect != 0 && s.yes(EE_UNITS_EU_SHOWS_DAMAGE) && s.qab(EE_UNITS_EU_DAMAGE_EFFECT_FORMULA, effect, hit_points) != 0 {
            if s.q(EE_UNITS_EU_CATEGORY) != CATEGORY_GATE {
                s.qa(EE_UNITS_EU_EFFECT_ON_SELF, effect);
            } else {
                s.q(EE_UNITS_EU_GATE_EFFECT_LINKED);
                s.q(EE_UNITS_EU_GATE_EFFECT_CARRIER);
            }
        }
        impact(s, !suppress);
        s.qb(EE_UNITS_EU_SET_HIT_POINTS, hit_points);
        if complex != 0 {
            if s.q(EE_UNITS_EU_TARGET_SEARCH_TIMER) != 0 {
                s.q(EE_UNITS_EU_CLEAR_TARGET_SEARCH_TIMER);
            }
            let category = s.q(EE_UNITS_EU_COMPLEX_CATEGORY);
            if (category == CATEGORY_AIRPLANE || category == CATEGORY_PLANET_SPACESHIP)
                && s.q(EE_UNITS_EU_AMMUNITION) > 0
                && hit_points == 1
                && s.q(EE_UNITS_EU_FLIGHT_TIME) != 0
            {
                s.q(EE_UNITS_EU_CLEAR_FLIGHT_TIME);
            }
        }
    } else {
        let category = s.q(EE_UNITS_EU_CATEGORY);
        if category == CATEGORY_RESOURCE || category == CATEGORY_AMBIENT {
            s.qb(EE_UNITS_EU_SET_HIT_POINTS, 0);
        } else {
            if complex != 0
                && s.q(EE_UNITS_EU_PLAYER_ID) != s.q(EE_UNITS_EU_FIRST_PLAYER_ID)
                && s.q(EE_UNITS_EU_PLAYER_ID) != player
            {
                let category = s.q(EE_UNITS_EU_COMPLEX_CATEGORY);
                let series = if [CATEGORY_BUILDING, CATEGORY_BUILDING19, CATEGORY_GATE, CATEGORY_MINING_BUILDING].contains(&category) {
                    2
                } else {
                    3
                };
                s.qbc(EE_UNITS_EU_STATS_RESET, player, series);
            }
            s.q(EE_UNITS_EU_KILL);
            s.qb(EE_UNITS_EU_SET_HIT_POINTS, hit_points);
            if s.q(EE_UNITS_EU_UNIT_TYPE) == UNIT_TYPE_CP_ICBM && hit_points == 0 {
                s.q(EE_UNITS_EU_ICBM_EXPLOSION);
            }
        }
    }
    if s.yes(EE_UNITS_EU_IS_CURRENT_PLAYER) && s.q(EE_UNITS_EU_CATEGORY) != CATEGORY_RESOURCE {
        s.q(EE_UNITS_EU_MINIMAP_PING);
    }
    if complex != 0 && s.yes(EE_UNITS_EU_IS_CURRENT_PLAYER) && s.yes(EE_UNITS_EU_SELECTED) {
        s.q(EE_UNITS_EU_POST_424);
    }
}

#[cfg(not(test))]
pub(crate) mod game {
    use super::*;
    use std::os::raw::c_void;

    extern "C" {
        fn ee_units_eu_host(op: i32, unit: *mut c_void, a: u64, b: i64, c: i64) -> i64;
    }

    /// the game's host (eu_body.rs's extern functions use it too)
    pub(crate) struct Game;

    impl EuHost for Game {
        fn eu(&mut self, op: i32, unit: u64, a: u64, b: i64, c: i64) -> i64 {
            // SAFETY: the body runs inside the C++ member that handed over its live unit
            unsafe { ee_units_eu_host(op, unit as usize as *mut c_void, a, b, c) }
        }
    }

    /// `EEUnit::TakeHit(attacker, damage, player, mode)` on `unit`.
    ///
    /// # Safety
    /// `unit` is a live `EEUnit`, `attacker` a live one or null, on the simulation thread.
    #[no_mangle]
    pub unsafe extern "C" fn ee_units_eu_take_hit(unit: *mut c_void, attacker: *mut c_void, damage: i64, player: u32, mode: i64) {
        take_hit(&mut Game, unit as usize as u64, attacker as usize as u64, damage, i64::from(player), mode);
    }
}

#[cfg(test)]
#[path = "eu_hit_tests.rs"]
mod tests;
