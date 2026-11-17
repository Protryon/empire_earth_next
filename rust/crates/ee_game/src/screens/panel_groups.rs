//! The main game form's panel groups other than the main group: statistics, chat, diplomacy, options and the
//! civilization builder (the original `EEUIFMGGStatistics`, `EEUIFMGGChat`, `EEUIFMGGDiplomacy`, `EEUIFMGGOptions`,
//! `EEUIFMGGCivBuilder`; `User Interface Thread/EEUIFMGGStatistics.cpp` and its four empty siblings). They are one
//! C++ class now, `EEUIFMGGRust` (`ee/EEUIFMGMainGroup.h`), made with its kind; its `Refresh` and `Select` come
//! here. Only the statistics group has texts: the conversions against their limit, the wonders to win and the epoch
//! wonders become available in. The others' slots were empty (`WantsRefresh` false for all five).

use core::ffi::CStr;

/// The statistics group (`mGroups[1]`).
pub const EE_SCREENS_GROUP_STATISTICS: u32 = 1;
/// The chat group (`mGroups[2]`).
pub const EE_SCREENS_GROUP_CHAT: u32 = 2;
/// The diplomacy group (`mGroups[3]`).
pub const EE_SCREENS_GROUP_DIPLOMACY: u32 = 3;
/// The options group (`mGroups[4]`).
pub const EE_SCREENS_GROUP_OPTIONS: u32 = 4;
/// The civilization builder group (`mGroups[5]`).
pub const EE_SCREENS_GROUP_CIV_BUILDER: u32 = 5;

/// `Refresh` (slot +0x04).
pub const EE_SCREENS_GROUP_REFRESH: u32 = 1;
/// `Select` (slot +0x0c).
pub const EE_SCREENS_GROUP_SELECT: u32 = 2;

/// The local player's conversions (`mSavedGroups.GetLastValue(5)`).
pub const EE_SCREENS_GROUP_VALUE_CONVERSIONS: u32 = 1;
/// The world table's conversions limit (`kWorldConversionsStatisticMax`).
pub const EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX: u32 = 2;
/// `gWorld.mNumberOfWondersToWin`.
pub const EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN: u32 = 3;
/// The title string of the epoch the local player's tech tree shows (`EEEpochTitleID(GetShownEpoch())`).
pub const EE_SCREENS_GROUP_VALUE_EPOCH_TITLE: u32 = 4;
/// Whether the world has a current player.
pub const EE_SCREENS_GROUP_VALUE_HAS_PLAYER: u32 = 5;

// the main game form's labels (`EEUIManager.h`)
const MAIN_GAME_CONVERSIONS: u32 = 95;
const MAIN_GAME_WONDERS_TO_WIN: u32 = 96;
const MAIN_GAME_WONDERS_AVAILABLE: u32 = 97;
// the strings (`EEStringIDs.h`)
const IDS_WONDERS_AVAILABLE_IN: i32 = 30408;
const IDS_WONDERS_TO_WIN: i32 = 30409;
const IDS_GAME_CONVERSIONS: i32 = 30410;

/// One text to print into a label: `format` printed with the strings `strings[0]` then `strings[1]` when it is
/// not -1, else with `strings[0]` and the two values (the host's `sprintf` into a `UWideString`).
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct GroupText {
    pub control: u32,
    pub if_changed: bool,
    pub format: &'static CStr,
    pub strings: [i32; 2],
    pub values: [i32; 2],
}

pub trait GroupHost {
    fn value(&mut self, which: u32) -> i32;
    fn print(&mut self, text: &GroupText);
}

/// A group's `Refresh` or `Select`.
pub fn group_call(kind: u32, call: u32, h: &mut impl GroupHost) {
    if kind != EE_SCREENS_GROUP_STATISTICS {
        return;
    }
    match call {
        EE_SCREENS_GROUP_REFRESH => statistics_refresh(h),
        EE_SCREENS_GROUP_SELECT => {
            // 0x667db1: the wonders to win, the epoch wonders become available in, then the conversions
            let wonders = h.value(EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN);
            h.print(&GroupText {
                control: MAIN_GAME_WONDERS_TO_WIN,
                if_changed: false,
                format: c"%-18.18s  %4.4ld",
                strings: [IDS_WONDERS_TO_WIN, -1],
                values: [wonders, 0],
            });
            let epoch = h.value(EE_SCREENS_GROUP_VALUE_EPOCH_TITLE);
            h.print(&GroupText {
                control: MAIN_GAME_WONDERS_AVAILABLE,
                if_changed: false,
                format: c"%-18.18s %-18.18s",
                strings: [IDS_WONDERS_AVAILABLE_IN, epoch],
                values: [0, 0],
            });
            statistics_refresh(h);
        }
        _ => {}
    }
}

/// 0x667ce7: the conversions against their limit, set only when the text changed; nothing without a player.
fn statistics_refresh(h: &mut impl GroupHost) {
    if h.value(EE_SCREENS_GROUP_VALUE_HAS_PLAYER) == 0 {
        return;
    }
    let conversions = h.value(EE_SCREENS_GROUP_VALUE_CONVERSIONS);
    let limit = h.value(EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX);
    h.print(&GroupText {
        control: MAIN_GAME_CONVERSIONS,
        if_changed: true,
        format: c"%-18.18s  %4.4ld/%-4.4ld",
        strings: [IDS_GAME_CONVERSIONS, -1],
        values: [conversions, limit],
    });
}

#[cfg(test)]
mod tests {
    use super::*;

    #[derive(Default)]
    struct Log {
        player: bool,
        calls: Vec<String>,
    }

    impl GroupHost for Log {
        fn value(&mut self, which: u32) -> i32 {
            self.calls.push(format!("value {which}"));
            match which {
                EE_SCREENS_GROUP_VALUE_HAS_PLAYER => self.player as i32,
                EE_SCREENS_GROUP_VALUE_CONVERSIONS => 3,
                EE_SCREENS_GROUP_VALUE_CONVERSIONS_MAX => 25,
                EE_SCREENS_GROUP_VALUE_WONDERS_TO_WIN => 2,
                EE_SCREENS_GROUP_VALUE_EPOCH_TITLE => 1234,
                _ => panic!(),
            }
        }
        fn print(&mut self, t: &GroupText) {
            self.calls
                .push(format!("print {} {} {:?} {:?} {:?}", t.control, t.if_changed, t.format, t.strings, t.values));
        }
    }

    #[test]
    fn statistics_select_then_refresh() {
        let mut h = Log { player: true, ..Log::default() };
        group_call(EE_SCREENS_GROUP_STATISTICS, EE_SCREENS_GROUP_SELECT, &mut h);
        assert_eq!(
            h.calls,
            [
                "value 3",
                "print 96 false \"%-18.18s  %4.4ld\" [30409, -1] [2, 0]",
                "value 4",
                "print 97 false \"%-18.18s %-18.18s\" [30408, 1234] [0, 0]",
                "value 5",
                "value 1",
                "value 2",
                "print 95 true \"%-18.18s  %4.4ld/%-4.4ld\" [30410, -1] [3, 25]",
            ]
        );
    }

    #[test]
    fn statistics_refresh_needs_a_player() {
        let mut h = Log::default();
        group_call(EE_SCREENS_GROUP_STATISTICS, EE_SCREENS_GROUP_REFRESH, &mut h);
        assert_eq!(h.calls, ["value 5"]);
        h.player = true;
        h.calls.clear();
        group_call(EE_SCREENS_GROUP_STATISTICS, EE_SCREENS_GROUP_REFRESH, &mut h);
        assert_eq!(h.calls.len(), 4);
    }

    #[test]
    fn the_other_groups_do_nothing() {
        for kind in
            [EE_SCREENS_GROUP_CHAT, EE_SCREENS_GROUP_DIPLOMACY, EE_SCREENS_GROUP_OPTIONS, EE_SCREENS_GROUP_CIV_BUILDER]
        {
            for call in [EE_SCREENS_GROUP_REFRESH, EE_SCREENS_GROUP_SELECT] {
                let mut h = Log { player: true, ..Log::default() };
                group_call(kind, call, &mut h);
                assert!(h.calls.is_empty());
            }
        }
    }
}
