//! The main game form's saved-group buttons as a kind over the button shell (UI cluster round 4; once
//! `EEUIBSavedGroup`): the group's first unit's portrait under the button, a click to save, add to or select the
//! group, a double click to select it with the camera's jump, a right click to clear it, the two button sounds.
//! The messages, the sounds, the players' groups and the portrait's cell are the host's.

use core::cell::Cell;
use core::ffi::CStr;

use crate::screens::shells::*;
use crate::screens::shells_editor::EE_SCREENS_SH_BITMAP_RELEASE;

/// `EEUIBSavedGroup` (a button; no args).
pub const EE_SCREENS_SK_SAVED_GROUP: u32 = 0x80;

/// `Refresh()` / `SetUnit((EEComplexUnit*)a)`.
pub const EE_SCREENS_SKOP_SG_REFRESH: u32 = 0x80;
pub const EE_SCREENS_SKOP_SG_SET_UNIT: u32 = 0x81;

/// -> a new empty `GEBitmap`.
pub const EE_SCREENS_SH_SG_NEW_PORTRAIT: u32 = 0xb0;
/// The sound `a` (0 `kSoundButton1`, 1 `kSoundButton2`) loaded -> its sample id (0: none) / unloaded (`b` 1).
pub const EE_SCREENS_SH_SG_SOUND: u32 = 0xb1;
/// `gSoundManager->Play(a, 1, 0, 1, false, NULL)`.
pub const EE_SCREENS_SH_SG_PLAY: u32 = 0xb2;
/// -> the local player (0: none).
pub const EE_SCREENS_SH_SG_PLAYER: u32 = 0xb3;
/// -> the local player's selected group's first unit / its saved group `a`'s first unit when it has units (b 1; 0
/// else).
pub const EE_SCREENS_SH_SG_FIRST_UNIT: u32 = 0xb4;
/// The message `a` sent for the group `b & 0xff` (`EE_SCREENS_SG_MSG_*`; `b >> 8` the select's double-click flag).
pub const EE_SCREENS_SH_SG_SEND: u32 = 0xb5;
/// -> `UIControl::ShiftKeyPressed()`.
pub const EE_SCREENS_SH_SG_SHIFT: u32 = 0xb6;
/// -> `GetIndex()`.
pub const EE_SCREENS_SH_SG_INDEX: u32 = 0xb7;
/// The unit `a`'s portrait button cut into the bitmap `b` for the control's rectangle -> its texture, `f[0..2]` its
/// cell.
pub const EE_SCREENS_SH_SG_FACE: u32 = 0xb8;
/// The texture `a` referenced (`b` 1) or released (0).
pub const EE_SCREENS_SH_SG_TEXTURE_REF: u32 = 0xb9;
/// The bitmap `a`'s texture set to `b` on the control's screen.
pub const EE_SCREENS_SH_SG_SET_TEXTURE: u32 = 0xba;
/// The bitmap `a` drawn at the button's bitmap location.
pub const EE_SCREENS_SH_SG_DRAW: u32 = 0xbb;

/// The messages: select (with the double click flag), add the selection, create, clear.
pub const EE_SCREENS_SG_MSG_SELECT: i64 = 0;
pub const EE_SCREENS_SG_MSG_ADD: i64 = 1;
pub const EE_SCREENS_SG_MSG_CREATE: i64 = 2;
pub const EE_SCREENS_SG_MSG_CLEAR: i64 = 3;

/// The first saved group's control index.
const FIRST_INDEX: i64 = 0xd4;

pub(crate) fn make(kind: u32, args: &[i64]) -> Option<Box<dyn Kind>> {
    Some(match kind {
        EE_SCREENS_SK_SAVED_GROUP => Box::new(SavedGroup::default()),
        _ => return crate::screens::shells_icon_bar::make(kind, args),
    })
}

/// `EEUIBSavedGroup`'s members.
#[derive(Default)]
struct SavedGroup {
    /// `mPortrait` (a `GEBitmap*`), `mTexture` (a `GETexture*`, referenced).
    portrait: Cell<i64>,
    texture: Cell<i64>,
    /// `mPortraitX`, `mPortraitY`: the cell as of the last `SetUnit`.
    cell: Cell<(f32, f32)>,
    /// `mHasUnit`, `mDrawPortrait` (`mHasUnit` as of the last `CopyData`).
    has_unit: Cell<bool>,
    draw: Cell<bool>,
    /// The sounds' samples (`sSavedGroupDownSound`, `sSavedGroupUpSound`).
    down: Cell<i64>,
    up: Cell<i64>,
}

impl SavedGroup {
    fn dirty(c: &Ctx) {
        c.base(EE_SCREENS_SV_SET_DIRTY, 1, 0, &mut [0.0; 4]);
    }
    fn group(c: &Ctx) -> i64 {
        c.call(EE_SCREENS_SH_SG_INDEX, 0, 0).wrapping_sub(FIRST_INDEX)
    }
    fn send(c: &Ctx, msg: i64, group: i64, flag: bool) {
        c.call(EE_SCREENS_SH_SG_SEND, msg, (group & 0xff) | i64::from(flag) << 8);
    }
    /// `SetUnit`: the unit's portrait (dirty when it changed; the texture referenced again each time).
    fn set_unit(&self, c: &Ctx, unit: i64) {
        let old = self.texture.replace(0);
        if old != 0 {
            c.call(EE_SCREENS_SH_SG_TEXTURE_REF, old, 0);
        }
        if unit == 0 {
            self.has_unit.set(false);
            return;
        }
        let mut f = [0.0f32; 4];
        let texture = c.host.call(c.ctl, EE_SCREENS_SH_SG_FACE, unit, self.portrait.get(), &mut f);
        let cell = (f[0], f[1]);
        if !self.has_unit.get() {
            self.has_unit.set(true);
            self.cell.set(cell);
            self.texture.set(texture);
            Self::dirty(c);
        } else if self.texture.get() != texture || cell.0 != self.cell.get().0 || cell.1 != self.cell.get().1 {
            // (the held texture was released above: the original compares NULL with the button's texture)
            self.cell.set(cell);
            self.texture.set(texture);
            Self::dirty(c);
        }
        // (NULL when nothing changed: the original references it anyway; the host skips NULL)
        c.call(EE_SCREENS_SH_SG_TEXTURE_REF, self.texture.get(), 1);
    }
    fn refresh(&self, c: &Ctx) {
        if c.call(EE_SCREENS_SH_SG_PLAYER, 0, 0) == 0 {
            return;
        }
        let first = c.call(EE_SCREENS_SH_SG_FIRST_UNIT, Self::group(c), 1);
        if first != 0 {
            self.set_unit(c, first);
        } else {
            let had = self.has_unit.get();
            self.set_unit(c, 0);
            if had {
                Self::dirty(c);
            }
        }
    }
}

impl Kind for SavedGroup {
    fn name(&self) -> &'static CStr {
        c"EEUIBSavedGroup"
    }
    fn overrides(&self) -> u64 {
        (1 << EE_SCREENS_SV_COPY_DATA)
            | (1 << EE_SCREENS_SV_DRAW_POLYGONS)
            | (1 << EE_SCREENS_SV_MOUSE_L_DOUBLE)
            | (1 << EE_SCREENS_SV_MOUSE_L_UP)
            | (1 << EE_SCREENS_SV_MOUSE_R_DOWN)
            | (1 << EE_SCREENS_SV_MOUSE_R_UP)
    }
    fn init(&self, c: &Ctx) {
        self.portrait.set(c.call(EE_SCREENS_SH_SG_NEW_PORTRAIT, 0, 0));
        self.down.set(c.call(EE_SCREENS_SH_SG_SOUND, 0, 0));
        self.up.set(c.call(EE_SCREENS_SH_SG_SOUND, 1, 0));
    }
    fn virt(&self, c: &Ctx, v: u32, a: i64, b: i64, f: &mut [f32; 4]) -> i64 {
        match v {
            EE_SCREENS_SV_COPY_DATA => {
                self.draw.set(self.has_unit.get());
                if self.draw.get() {
                    c.call(EE_SCREENS_SH_SG_SET_TEXTURE, self.portrait.get(), self.texture.get());
                }
                c.base0(EE_SCREENS_SV_COPY_DATA)
            }
            EE_SCREENS_SV_DRAW_POLYGONS => {
                if self.draw.get() {
                    c.call(EE_SCREENS_SH_SG_DRAW, self.portrait.get(), 0);
                }
                c.base0(EE_SCREENS_SV_DRAW_POLYGONS)
            }
            EE_SCREENS_SV_MOUSE_L_DOUBLE => {
                let group = Self::group(c);
                if c.call(EE_SCREENS_SH_SG_PLAYER, 0, 0) != 0 {
                    if self.has_unit.get() {
                        Self::send(c, EE_SCREENS_SG_MSG_SELECT, group, true);
                    }
                    c.call(EE_SCREENS_SH_SG_PLAY, self.down.get(), 0);
                }
                c.call(EE_SCREENS_SH_RELEASE_MOUSE, 0, 0);
                0
            }
            EE_SCREENS_SV_MOUSE_L_UP => {
                let player = c.call(EE_SCREENS_SH_SG_PLAYER, 0, 0);
                let group = Self::group(c);
                if player != 0 {
                    let msg = if c.call(EE_SCREENS_SH_SG_SHIFT, 0, 0) != 0 {
                        if !self.has_unit.get() {
                            self.set_unit(c, c.call(EE_SCREENS_SH_SG_FIRST_UNIT, 0, 0));
                        }
                        EE_SCREENS_SG_MSG_ADD
                    } else if !self.has_unit.get() {
                        self.set_unit(c, c.call(EE_SCREENS_SH_SG_FIRST_UNIT, 0, 0));
                        EE_SCREENS_SG_MSG_CREATE
                    } else {
                        EE_SCREENS_SG_MSG_SELECT
                    };
                    c.call(EE_SCREENS_SH_SG_PLAY, self.down.get(), 0);
                    Self::send(c, msg, group, false);
                }
                c.base0(EE_SCREENS_SV_MOUSE_L_UP)
            }
            // a right press is the base's left press
            EE_SCREENS_SV_MOUSE_R_DOWN => c.base0(EE_SCREENS_SV_MOUSE_L_DOWN),
            EE_SCREENS_SV_MOUSE_R_UP => {
                let group = Self::group(c);
                if self.has_unit.replace(false) {
                    Self::send(c, EE_SCREENS_SG_MSG_CLEAR, group, false);
                }
                c.base0(EE_SCREENS_SV_MOUSE_L_UP);
                c.call(EE_SCREENS_SH_SG_PLAY, self.up.get(), 0);
                0
            }
            _ => c.base(v, a, b, f),
        }
    }
    fn op(&self, c: &Ctx, op: u32, a: i64, _b: i64, _f: &mut [f32; 4]) -> i64 {
        match op {
            EE_SCREENS_SKOP_SG_REFRESH => self.refresh(c),
            EE_SCREENS_SKOP_SG_SET_UNIT => self.set_unit(c, a),
            _ => {}
        }
        0
    }
    fn free(&self, c: &Ctx) {
        c.call(EE_SCREENS_SH_BITMAP_RELEASE, self.portrait.get(), 0);
        let t = self.texture.replace(0);
        if t != 0 {
            c.call(EE_SCREENS_SH_SG_TEXTURE_REF, t, 0);
        }
        c.call(EE_SCREENS_SH_SG_SOUND, 0, 1);
        c.call(EE_SCREENS_SH_SG_SOUND, 1, 1);
    }
}

#[cfg(test)]
mod tests;
