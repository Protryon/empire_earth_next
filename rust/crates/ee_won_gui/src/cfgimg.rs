//! The image configs' stateful handlers for the table reader (R6 WON round 41): StretchImageConfig's MULTIEXPORT
//! and MULTICROPEXPORT, ImageFilterConfig's FILTER, EXPORT and MULTIHORZCROP and its HandleFilter, written out as
//! the original's statements ran their reads (a count of exports, an offset, the filter by name). The C++ host's
//! setters keep the objects: the new child configs with the copied stretch attributes or filtered images, the crop
//! filter at each export's place, the filters by name (cfgtab.rs's frame: the raw image read or filtered, the raw
//! strings read).

use crate::cfgtab::{CfgHost, EeWonCfgRead as Rd};
use crate::guiconf::EE_WON_CFG_NAMES_IMAGEFILTERCONFIG_HANDLEFILTER_NAME;

/// The frame's ints in turn: each read's place, for `h.int`.
struct Ints(i32);

impl Ints {
    fn read(&mut self, h: &mut dyn CfgHost, kind: Rd) -> Option<i32> {
        if !h.read(kind) {
            return None;
        }
        self.0 += 1;
        Some(h.int(self.0 - 1))
    }
}

/// `StretchImageConfig` MULTIEXPORT: a count, the image and config name prefixes, an optional first number; one
/// child config per number (setter 4: the image `prefix<n + offset>`, the config `prefix<n>`).
pub fn stretch_multi_export(table: i32, h: &mut dyn CfgHost) -> bool {
    let mut n = Ints(0);
    h.clear();
    let Some(count) = n.read(h, Rd::Int) else { return false };
    if !h.comma() || !h.read(Rd::RawStr) || !h.comma() || !h.read(Rd::RawStr) {
        return false;
    }
    let mut offset = 0i32;
    if !h.end() {
        if !h.comma() {
            return false;
        }
        let Some(first) = n.read(h, Rd::Int) else { return false };
        offset = first.wrapping_sub(1);
    }
    for i in 0..count {
        h.push(i.wrapping_add(1).wrapping_add(offset));
        h.push(i.wrapping_add(1));
        if !h.apply(table << 8 | 4) {
            return false;
        }
    }
    true
}

/// `StretchImageConfig` MULTICROPEXPORT: a count, the image, the config name prefix; the image cut into that many
/// columns (setter 5: their width), one child config per column (setter 6).
pub fn stretch_multi_crop(table: i32, h: &mut dyn CfgHost) -> bool {
    let mut n = Ints(0);
    h.clear();
    let Some(count) = n.read(h, Rd::Int) else { return false };
    if !h.comma() || !h.read(Rd::RawImage) || !h.comma() || !h.read(Rd::RawStr) || !h.apply(table << 8 | 5) {
        return false;
    }
    for i in 0..count {
        h.push(i);
        if !h.apply(table << 8 | 6) {
            return false;
        }
    }
    true
}

/// `ImageFilterConfig::HandleFilter`, its name the frame's string `slot`: the filter's arguments, then setter 10
/// to 14 (SCALEPERCENT, SCALEABS, TINT, CROP, PIXELTRANS) filters the config's raw image into the frame's, or 15
/// empties it (an unknown name).
fn handle_filter(table: i32, h: &mut dyn CfgHost, slot: i32) -> bool {
    let mut n = Ints(0);
    if !h.read(Rd::Str) || !h.comma() {
        return false;
    }
    let kind = h.name(EE_WON_CFG_NAMES_IMAGEFILTERCONFIG_HANDLEFILTER_NAME, slot);
    let ok = match kind {
        0 => {
            let Some(w) = n.read(h, Rd::Int) else { return false };
            if h.end() {
                h.push(w);
                true
            } else {
                h.comma() && n.read(h, Rd::Int).is_some()
            }
        }
        1 => n.read(h, Rd::Int).is_some() && h.comma() && n.read(h, Rd::Int).is_some(),
        2 => (0..3).all(|k| (k == 0 || h.comma()) && n.read(h, Rd::Int).is_some()),
        3 => (0..4).all(|k| (k == 0 || h.comma()) && n.read(h, Rd::Int).is_some()),
        4 => n.read(h, Rd::Color).is_some(),
        _ => true,
    };
    ok && h.apply(table << 8 | if (0..5).contains(&kind) { 10 + kind } else { 15 })
}

/// `ImageFilterConfig` FILTER: the filtered image becomes the config's (setter 0).
pub fn filter_key(table: i32, h: &mut dyn CfgHost) -> bool {
    h.clear();
    handle_filter(table, h, 0) && h.apply(table << 8)
}

/// `ImageFilterConfig` EXPORT: a name, then a filter; a child config of the filtered image under the name
/// (setter 5).
pub fn filter_export(table: i32, h: &mut dyn CfgHost) -> bool {
    h.clear();
    h.read(Rd::Str) && h.comma() && handle_filter(table, h, 1) && h.apply(table << 8 | 5)
}

/// `ImageFilterConfig` MULTIHORZCROP: a count and a name prefix; the config's image cut into that many columns
/// (setter 3: their width), one child config per column (setter 4).
pub fn filter_multi_crop(table: i32, h: &mut dyn CfgHost) -> bool {
    let mut n = Ints(0);
    h.clear();
    let Some(count) = n.read(h, Rd::Int) else { return false };
    if !h.comma() || !h.read(Rd::Str) || !h.apply(table << 8 | 3) {
        return false;
    }
    for i in 0..count {
        h.push(i);
        if !h.apply(table << 8 | 4) {
            return false;
        }
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    /// A host that logs every request; reads succeed, ints answer from `ints`, end-of-string from `ends`.
    #[derive(Default)]
    struct Log {
        log: Vec<String>,
        ints: Vec<i32>,
        ends: Vec<bool>,
        name: i32,
    }

    impl CfgHost for Log {
        fn read(&mut self, kind: Rd) -> bool {
            self.log.push(format!("r{kind:?}"));
            true
        }
        fn comma(&mut self) -> bool {
            self.log.push("c".into());
            true
        }
        fn end(&mut self) -> bool {
            self.log.push("e".into());
            if self.ends.is_empty() {
                true
            } else {
                self.ends.remove(0)
            }
        }
        fn name(&mut self, _t: i32, slot: i32) -> i32 {
            self.log.push(format!("n{slot}"));
            self.name
        }
        fn empty(&mut self, _slot: i32) -> bool {
            false
        }
        fn push(&mut self, v: i32) {
            self.log.push(format!("p{v}"));
        }
        fn apply(&mut self, setter: i32) -> bool {
            self.log.push(format!("a{}", setter & 0xff));
            true
        }
        fn fail(&mut self, _msg: &[u8], _slot: i32) {}
        fn clear(&mut self) {
            self.log.push("x".into());
        }
        fn int(&mut self, slot: i32) -> i32 {
            self.ints[slot as usize]
        }
        fn keyword(&mut self, _slot: i32, _names: &[&[u8]]) -> i32 {
            -1
        }
    }

    #[test]
    fn multi_export_numbers_each_image_from_the_first() {
        // 2 exports from number 5: images 5 and 6, configs 1 and 2
        let mut h = Log { ints: vec![2, 5], ends: vec![false], ..Log::default() };
        assert!(stretch_multi_export(70, &mut h));
        assert_eq!(
            h.log,
            ["x", "rInt", "c", "rRawStr", "c", "rRawStr", "e", "c", "rInt", "p5", "p1", "a4", "p6", "p2", "a4"]
        );
        // no first number: from 1; a count of 0 makes none
        let mut h = Log { ints: vec![0], ..Log::default() };
        assert!(stretch_multi_export(70, &mut h));
        assert_eq!(h.log.last().unwrap(), "e");
    }

    #[test]
    fn crops_take_their_width_then_one_child_per_column() {
        let mut h = Log { ints: vec![3], ..Log::default() };
        assert!(stretch_multi_crop(70, &mut h));
        assert_eq!(h.log, ["x", "rInt", "c", "rRawImage", "c", "rRawStr", "a5", "p0", "a6", "p1", "a6", "p2", "a6"]);
        let mut h = Log { ints: vec![2], ..Log::default() };
        assert!(filter_multi_crop(33, &mut h));
        assert_eq!(h.log, ["x", "rInt", "c", "rStr", "a3", "p0", "a4", "p1", "a4"]);
    }

    #[test]
    fn filters_by_name() {
        // SCALEPERCENT with one size: the height is the width
        let mut h = Log { ints: vec![40], ends: vec![true], name: 0, ..Log::default() };
        assert!(filter_key(33, &mut h));
        assert_eq!(h.log, ["x", "rStr", "c", "n0", "rInt", "e", "p40", "a10", "a0"]);
        // TINT's three, CROP's four, PIXELTRANS's colour
        let mut h = Log { ints: vec![1, 2, 3], name: 2, ..Log::default() };
        assert!(filter_key(33, &mut h));
        assert_eq!(h.log, ["x", "rStr", "c", "n0", "rInt", "c", "rInt", "c", "rInt", "a12", "a0"]);
        let mut h = Log { ints: vec![0; 4], name: 3, ..Log::default() };
        assert!(filter_key(33, &mut h));
        assert_eq!(h.log.iter().filter(|l| *l == "rInt").count(), 4);
        let mut h = Log { ints: vec![7], name: 4, ..Log::default() };
        assert!(filter_export(33, &mut h));
        assert_eq!(h.log, ["x", "rStr", "c", "rStr", "c", "n1", "rColor", "a14", "a5"]);
        // an unknown name reads nothing more and empties the image
        let mut h = Log { name: -1, ..Log::default() };
        assert!(filter_key(33, &mut h));
        assert_eq!(h.log, ["x", "rStr", "c", "n0", "a15", "a0"]);
    }
}
