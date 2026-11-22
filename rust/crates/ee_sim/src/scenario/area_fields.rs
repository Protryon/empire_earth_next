//! `EESMArea`'s numbers as a record the C++ class derives from (`class EESMArea : public EeScenarioAreaFields`,
//! ee/EESMArea.h): the anchor, the rectangle, the cached centre tile, the select mode, the tile cache's time, the id
//! and the two flags keep their C++ names and layouts (`U2DPoint<long>`, `GERectangle`'s bottom, left, right, top),
//! so the C++ readers are unchanged. The defaults, the copy, the anchor and rectangle setters, `IsUnset`, `IsOffMap`,
//! the save's copies and the tile cache's test are these functions; the queries' bodies are area.rs over the record
//! [`EeScenarioAreaFields::record`] makes. The name, the text and the tile list stay members of the C++ class.
//!
//! [`region_input`] is an area's input to the continent manager's `BuildRegions` (the AI's scenario rectangles).

#![allow(non_snake_case)]
// `c_ulong` and `c_long` are 32 bits on Windows and 64 elsewhere: the casts to the 32-bit records are needed there
#![allow(clippy::unnecessary_cast)]

use std::os::raw::{c_long, c_ulong};

use crate::astar::manager::EeScenarioArea;
use crate::scenario::area::EeScenarioAreaRecord;
use crate::scenario::fields::EeScenarioAreaSave;

/// The "no area" coordinate of the anchor and the centre tile.
pub const EE_SCENARIO_AREA_NONE: c_long = -10000;
/// The ticks a cached tile list lives unread (`GetTiles` rebuilds it after them).
pub const EE_SCENARIO_AREA_TILES_TICKS: c_ulong = 3000;

/// `U2DPoint<long>`.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioAreaPoint {
    pub mX: c_long,
    pub mY: c_long,
}

/// `GERectangle` (its members' order).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioAreaRect {
    pub mBottom: f32,
    pub mLeft: f32,
    pub mRight: f32,
    pub mTop: f32,
}

/// `EESMArea`'s numbers.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct EeScenarioAreaFields {
    pub mAnchor: EeScenarioAreaPoint,
    pub mRect: EeScenarioAreaRect,
    /// The cached centre tile (-10000 while unset).
    pub mCenterTile: EeScenarioAreaPoint,
    pub mSelectMode: c_ulong,
    pub mTilesTime: c_ulong,
    pub mID: c_ulong,
    pub mHasRectangle: bool,
    pub mHasAnchor: bool,
}

impl EeScenarioAreaFields {
    /// The constructor's: no rectangle or anchor, rectangle mode, id 0, no cached centre.
    pub fn defaults() -> Self {
        EeScenarioAreaFields {
            mCenterTile: EeScenarioAreaPoint { mX: EE_SCENARIO_AREA_NONE, mY: EE_SCENARIO_AREA_NONE },
            ..Default::default()
        }
    }

    /// `Copy`'s numbers: the rectangle, the anchor, the flags, the select mode and the id (not the tile cache or the
    /// cached centre).
    pub fn copy_from(&mut self, o: &EeScenarioAreaFields) {
        self.mRect = o.mRect;
        self.mHasRectangle = o.mHasRectangle;
        self.mAnchor = o.mAnchor;
        self.mHasAnchor = o.mHasAnchor;
        self.mSelectMode = o.mSelectMode;
        self.mID = o.mID;
    }

    /// `SetAnchor`'s numbers (the C++ clears the tile list): the anchor set, the centre forgotten.
    pub fn set_anchor(&mut self, x: c_long, y: c_long) {
        self.mHasAnchor = true;
        self.mAnchor = EeScenarioAreaPoint { mX: x, mY: y };
        self.mCenterTile.mX = EE_SCENARIO_AREA_NONE;
    }

    /// `SetRectangle`'s numbers (the C++ clears the tile list): the rectangle set, the centre forgotten.
    pub fn set_rectangle(&mut self, r: EeScenarioAreaRect) {
        self.mHasRectangle = true;
        self.mRect = r;
        self.mCenterTile.mX = EE_SCENARIO_AREA_NONE;
    }

    /// `IsUnset`: an anchor coordinate is the no-area value.
    pub fn is_unset(&self) -> bool {
        !(self.mAnchor.mX != EE_SCENARIO_AREA_NONE && self.mAnchor.mY != EE_SCENARIO_AREA_NONE)
    }

    /// `IsOffMap`: a negative anchor coordinate (compared as floats, as the original).
    pub fn is_off_map(&self) -> bool {
        (self.mAnchor.mX as f32) < 0.0 || (self.mAnchor.mY as f32) < 0.0
    }

    /// The queries' record (`EESMAreaRecord`: the 32-bit casts of the C++).
    pub fn record(&self) -> EeScenarioAreaRecord {
        EeScenarioAreaRecord {
            select_mode: self.mSelectMode as u32,
            anchor_x: self.mAnchor.mX as i32,
            anchor_y: self.mAnchor.mY as i32,
            left: self.mRect.mLeft,
            top: self.mRect.mTop,
            right: self.mRect.mRight,
            bottom: self.mRect.mBottom,
            center_x: self.mCenterTile.mX as i32,
            center_y: self.mCenterTile.mY as i32,
        }
    }

    /// The save's copy (`SaveIO`).
    pub fn save_record(&self) -> EeScenarioAreaSave {
        EeScenarioAreaSave {
            anchor_x: self.mAnchor.mX,
            anchor_y: self.mAnchor.mY,
            left: self.mRect.mLeft,
            top: self.mRect.mTop,
            right: self.mRect.mRight,
            bottom: self.mRect.mBottom,
            has_rectangle: self.mHasRectangle,
            has_anchor: self.mHasAnchor,
            select_mode: self.mSelectMode,
            tiles_time: self.mTilesTime,
            id: self.mID,
        }
    }

    /// The save's copy stored back (`SaveIO` after the read or write).
    pub fn store_save_record(&mut self, s: &EeScenarioAreaSave) {
        self.mAnchor = EeScenarioAreaPoint { mX: s.anchor_x, mY: s.anchor_y };
        self.mRect = EeScenarioAreaRect { mBottom: s.bottom, mLeft: s.left, mRight: s.right, mTop: s.top };
        self.mHasRectangle = s.has_rectangle;
        self.mHasAnchor = s.has_anchor;
        self.mSelectMode = s.select_mode;
        self.mTilesTime = s.tiles_time;
        self.mID = s.id;
    }

    /// `GetTiles`' test: the cached list (of `tile_count` tiles) is rebuilt when empty or unread for 3000 ticks
    /// (`(unsigned long)(tick - mTilesTime) >= 3000`).
    pub fn tiles_stale(&self, tile_count: usize, tick: c_ulong) -> bool {
        tile_count == 0 || tick.wrapping_sub(self.mTilesTime) >= EE_SCENARIO_AREA_TILES_TICKS
    }
}

/// An area's input to the continent manager's `BuildRegions` (`EEAIContinentManager::BuildRegions`' loop over the
/// scenario manager's areas): set when the area exists and is not unset, then its rectangle with its height
/// (bottom - top) and width (right - left); and its id (0 for no area).
pub fn region_input(area: Option<&EeScenarioAreaFields>) -> (EeScenarioArea, c_long) {
    let Some(a) = area else {
        return (EeScenarioArea::default(), 0);
    };
    let mut input = EeScenarioArea { set: !a.is_unset(), ..Default::default() };
    if input.set {
        let r = &a.mRect;
        input.top = r.mTop;
        input.bottom = r.mBottom;
        input.left = r.mLeft;
        input.right = r.mRight;
        input.height = r.mBottom - r.mTop;
        input.width = r.mRight - r.mLeft;
    }
    (input, a.mID as c_long)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn area(x: c_long, y: c_long) -> EeScenarioAreaFields {
        let mut a = EeScenarioAreaFields::defaults();
        a.set_anchor(x, y);
        a
    }

    #[test]
    fn defaults_are_the_constructors() {
        let a = EeScenarioAreaFields::defaults();
        assert_eq!(a.mCenterTile, EeScenarioAreaPoint { mX: -10000, mY: -10000 });
        assert_eq!(a.mAnchor, EeScenarioAreaPoint::default());
        assert!(!a.mHasAnchor && !a.mHasRectangle);
        assert_eq!((a.mSelectMode, a.mID, a.mTilesTime), (0, 0, 0));
        // a fresh area's anchor is (0, 0): not unset, not off the map
        assert!(!a.is_unset() && !a.is_off_map());
    }

    #[test]
    fn unset_and_off_map() {
        assert!(area(-10000, 5).is_unset());
        assert!(area(5, -10000).is_unset());
        assert!(!area(5, 5).is_unset());
        assert!(area(-1, 5).is_off_map());
        assert!(area(5, -1).is_off_map());
        assert!(!area(0, 0).is_off_map());
    }

    #[test]
    fn setters_forget_the_centre() {
        let mut a = EeScenarioAreaFields::defaults();
        a.mCenterTile = EeScenarioAreaPoint { mX: 3, mY: 4 };
        a.set_rectangle(EeScenarioAreaRect { mBottom: 9.0, mLeft: 1.0, mRight: 8.0, mTop: 2.0 });
        assert!(a.mHasRectangle);
        assert_eq!(a.mCenterTile, EeScenarioAreaPoint { mX: -10000, mY: 4 });
        a.mCenterTile.mX = 3;
        a.set_anchor(7, 6);
        assert!(a.mHasAnchor);
        assert_eq!((a.mAnchor.mX, a.mAnchor.mY, a.mCenterTile.mX), (7, 6, -10000));
    }

    #[test]
    fn copy_keeps_the_cache() {
        let mut a = area(1, 2);
        a.mCenterTile = EeScenarioAreaPoint { mX: 10, mY: 11 };
        a.mTilesTime = 77;
        let mut b = area(5, 6);
        b.mSelectMode = 2;
        b.mID = 9;
        b.set_rectangle(EeScenarioAreaRect { mBottom: 4.0, mLeft: 1.0, mRight: 3.0, mTop: 2.0 });
        b.mTilesTime = 5;
        a.copy_from(&b);
        assert_eq!((a.mAnchor, a.mSelectMode, a.mID, a.mRect, a.mHasRectangle), (b.mAnchor, 2, 9, b.mRect, true));
        assert_eq!((a.mCenterTile.mX, a.mTilesTime), (10, 77));
    }

    #[test]
    fn save_record_round_trip() {
        let mut a = area(3, 4);
        a.set_rectangle(EeScenarioAreaRect { mBottom: 4.5, mLeft: 1.5, mRight: 3.5, mTop: 2.5 });
        a.mSelectMode = 1;
        a.mTilesTime = 99;
        a.mID = 12;
        let s = a.save_record();
        assert_eq!((s.left, s.top, s.right, s.bottom), (1.5, 2.5, 3.5, 4.5));
        let mut b = EeScenarioAreaFields::defaults();
        b.store_save_record(&s);
        assert_eq!(b.mRect, a.mRect);
        assert_eq!((b.mAnchor, b.mSelectMode, b.mTilesTime, b.mID), (a.mAnchor, 1, 99, 12));
        assert!(b.mHasAnchor && b.mHasRectangle);
        // the cache is not saved
        assert_eq!(b.mCenterTile.mX, -10000);
    }

    #[test]
    fn record_casts() {
        let mut a = area(-5, 7);
        a.mRect = EeScenarioAreaRect { mBottom: 4.0, mLeft: 1.0, mRight: 3.0, mTop: 2.0 };
        a.mSelectMode = 2;
        let r = a.record();
        assert_eq!((r.anchor_x, r.anchor_y, r.select_mode), (-5, 7, 2));
        assert_eq!((r.left, r.top, r.right, r.bottom), (1.0, 2.0, 3.0, 4.0));
        assert_eq!((r.center_x, r.center_y), (-10000, -10000));
    }

    #[test]
    fn tiles_cache_test() {
        let mut a = EeScenarioAreaFields::defaults();
        a.mTilesTime = 1000;
        assert!(a.tiles_stale(0, 1000));
        assert!(!a.tiles_stale(5, 3999));
        assert!(a.tiles_stale(5, 4000));
        // the unsigned difference wraps: a tick before the time reads as a long time
        assert!(a.tiles_stale(5, 999));
    }

    #[test]
    fn region_inputs() {
        assert_eq!(region_input(None), (EeScenarioArea::default(), 0));
        let mut a = area(2, 3);
        a.mID = 41;
        a.mRect = EeScenarioAreaRect { mBottom: 20.0, mLeft: 4.0, mRight: 10.0, mTop: 5.0 };
        let (input, id) = region_input(Some(&a));
        assert_eq!(id, 41);
        assert!(input.set);
        assert_eq!((input.top, input.bottom, input.left, input.right), (5.0, 20.0, 4.0, 10.0));
        assert_eq!((input.height, input.width), (15.0, 6.0));
        // an unset area: its id, nothing else
        let mut u = area(-10000, 3);
        u.mID = 8;
        u.mRect = a.mRect;
        let (input, id) = region_input(Some(&u));
        assert_eq!((input, id), (EeScenarioArea::default(), 8));
    }
}
