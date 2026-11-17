//! `EESMEffect::ExpandTokens`: a dialogue or chat text's `/#OA`, `/#PA` and `/#OC` tags with the number after each
//! replaced by an object's attribute value, a player condition's value or an object's unit count. The scan is the
//! original's over `UWideString` (characters are UTF-8 code points, decision D6: `Find`, `GetIndexedSubstring` and
//! `GetLength` count them as ee_plat_core's `PlatMbs*` do); the world through three reads (`EE_SCENARIO_READ_TOKEN_*`).
//!
//! The original's quirks, kept: the number is the 5 characters after the tag (`GetIndexedSubstring` is inclusive)
//! and the 5th stays in the text after the replacement; a tag is used only when it starts before `GetLength() - 4`.
//! One change: a text under 4 characters makes `Find(...) < GetLength() - 4` true for every tag (the unsigned
//! subtraction wraps), and the original then appended the replacement to the whole text and looked again; with an
//! empty replacement (no object or condition 0, or one that gives nothing) the text did not change and the loop never
//! ended: the game hung. Here that case goes on to the next tag (the reads of the one pass are made, as before).

use ee_base::plat_core::utf8;

use crate::scenario::condition::{choose_player_recorded, player_value};
use crate::scenario::host::Host;

/// `FindObjectByID(a)`: -1 without the object or (when `b` is 1) when its `mHasAttributeCheck` is 0, else its
/// `Evaluate(0)` and then `mUnits.size()`.
pub const EE_SCENARIO_READ_TOKEN_OBJECT: u32 = 66;
/// `FindObjectByID(a)->GetUnitAttribute(GetAttribute(), GetIsAttributePercent(), gWorld.GetUnit(GetUnitAt(0)), 0,
/// 0)`.
pub const EE_SCENARIO_READ_TOKEN_VALUE: u32 = 67;
/// `FindConditionByID(a)`: -1 without the condition or when its `mType` is not Player Attribute, else its `mPlayer`
/// (`b` 0) or `mAttribute` (`b` 1).
pub const EE_SCENARIO_READ_TOKEN_CONDITION: u32 = 68;

/// The tags in the original's order.
const MARKERS: [&[u8]; 3] = [b"/#OA", b"/#PA", b"/#OC"];

/// `UWideString::Find(inString, 0)`: the first character position where `m` occurs, `len` (the text's characters)
/// when it does not.
fn find(s: &[u8], len: usize, m: &[u8]) -> usize {
    let mlen = utf8::char_len(m);
    // (long)(mLength - inString.mLength) > 0 ? mLength - inString.mLength : 0
    let last = len.saturating_sub(mlen);
    if len >= mlen {
        let mut at = 0;
        for i in 0..=last {
            if utf8::compare_chars(&s[at..], m, mlen) == 0 {
                return i;
            }
            at = utf8::next(s, at);
        }
    }
    len
}

/// The bytes from character `first` on (clamped at the end), at most `count` characters.
fn chars_from(s: &[u8], first: usize, count: usize) -> &[u8] {
    let at = utf8::bytes_of_chars(s, first);
    let rest = &s[at..];
    &rest[..utf8::bytes_of_chars(rest, count)]
}

/// The C runtime's `atoi` on a short text: white space, a sign, decimal digits (5 at most here: no overflow).
fn atoi(s: &[u8]) -> i32 {
    let mut i = 0;
    while i < s.len() && matches!(s[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = i < s.len() && s[i] == b'-';
    if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
        i += 1;
    }
    let mut value = 0i32;
    while i < s.len() && s[i].is_ascii_digit() {
        value = value.wrapping_mul(10).wrapping_add((s[i] - b'0') as i32);
        i += 1;
    }
    if negative {
        value.wrapping_neg()
    } else {
        value
    }
}

/// The replacement of tag `marker` with id `index` (the original's switch; `sprintf` "%ld" / "%zu").
fn replacement<H: Host>(host: &mut H, marker: usize, index: i32) -> Vec<u8> {
    match marker {
        0 => {
            if host.read(EE_SCENARIO_READ_TOKEN_OBJECT, index, 1) > 0 {
                return host.read(EE_SCENARIO_READ_TOKEN_VALUE, index, 0).to_string().into_bytes();
            }
        }
        1 => {
            let kind = host.read(EE_SCENARIO_READ_TOKEN_CONDITION, index, 0);
            if kind >= 0 {
                let (_, players) = choose_player_recorded(host, kind as i32, true, true);
                if let Some(&player) = players.first() {
                    let attribute = host.read(EE_SCENARIO_READ_TOKEN_CONDITION, index, 1) as u32;
                    // the unsigned long value into a long (32 bits in the original)
                    return (player_value(host, player, attribute) as i32).to_string().into_bytes();
                }
            }
        }
        _ => {
            let count = host.read(EE_SCENARIO_READ_TOKEN_OBJECT, index, 0);
            if count >= 0 {
                return count.to_string().into_bytes();
            }
        }
    }
    Vec::new()
}

/// `EESMEffect::ExpandTokens(inText, outText)`: the expanded text.
pub fn expand_tokens<H: Host>(host: &mut H, text: &[u8]) -> Vec<u8> {
    let mut out = text.to_vec();
    let mut marker = 0;
    while marker < MARKERS.len() {
        let m = MARKERS[marker];
        let len = utf8::char_len(&out);
        let found = find(&out, len, m);
        if found < len.wrapping_sub(4) {
            let position = found + utf8::char_len(m);
            let index = atoi(chars_from(&out, position, 5));
            let with = replacement(host, marker, index);
            if len < 4 && with.is_empty() {
                // the original looped here forever (see the module's text)
                marker += 1;
                continue;
            }
            let mut next = chars_from(&out, 0, found).to_vec();
            next.extend_from_slice(&with);
            next.extend_from_slice(chars_from(&out, position + 4, usize::MAX));
            out = next;
            continue;
        }
        marker += 1;
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::scenario::host::{
        EE_SCENARIO_READ_CAPTURE_SEED, EE_SCENARIO_READ_NUM_PLAYERS, EE_SCENARIO_READ_PLAYER_EXISTS,
    };

    /// Objects by id: (attribute check, unit count, value); conditions by id: (player choice, attribute).
    #[derive(Default)]
    struct Mock {
        objects: Vec<(i32, bool, i64, i64)>,
        conditions: Vec<(i32, i64, i64)>,
        reads: Vec<(u32, i32, i32)>,
    }

    impl Host for Mock {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            self.reads.push((op, a, b));
            match op {
                EE_SCENARIO_READ_CAPTURE_SEED => -1,
                EE_SCENARIO_READ_NUM_PLAYERS => 4,
                EE_SCENARIO_READ_PLAYER_EXISTS => 1,
                EE_SCENARIO_READ_TOKEN_OBJECT => match self.objects.iter().find(|o| o.0 == a) {
                    Some(o) if b == 0 || o.1 => o.2,
                    _ => -1,
                },
                EE_SCENARIO_READ_TOKEN_VALUE => self.objects.iter().find(|o| o.0 == a).unwrap().3,
                EE_SCENARIO_READ_TOKEN_CONDITION => match self.conditions.iter().find(|c| c.0 == a) {
                    Some(c) => {
                        if b == 0 {
                            c.1
                        } else {
                            c.2
                        }
                    }
                    None => -1,
                },
                // the variables: player * 100 + variable
                crate::scenario::host::EE_SCENARIO_READ_VARIABLE => (a * 100 + b) as i64,
                _ => 0,
            }
        }
    }

    fn run(m: &mut Mock, text: &str) -> String {
        String::from_utf8(expand_tokens(m, text.as_bytes())).unwrap()
    }

    #[test]
    fn text_without_tags_is_kept() {
        let mut m = Mock::default();
        assert_eq!(run(&mut m, "Hold the bridge!"), "Hold the bridge!");
        assert!(m.reads.is_empty());
    }

    #[test]
    fn unit_count_attribute_and_player_value() {
        let mut m = Mock {
            objects: vec![(12, true, 3, -45), (7, false, 9, 0)],
            conditions: vec![(5, 8, 12)],
            ..Default::default()
        };
        // the 5th character after the tag is read into the number and kept after it
        assert_eq!(run(&mut m, "You have /#OC0007 men"), "You have 9 men");
        assert_eq!(run(&mut m, "HP /#OA0012!"), "HP -45!");
        // choice 8 = player 2, attribute 12 = variable 1
        assert_eq!(run(&mut m, "Score /#PA0005 now"), "Score 201 now");
        // five digits: 00071 is object 71 (none here), and its last digit stays
        assert_eq!(run(&mut m, "x/#OC00071"), "x1");
    }

    #[test]
    fn missing_or_unchecked_ids_vanish() {
        let mut m = Mock { objects: vec![(7, false, 9, 0)], ..Default::default() };
        assert_eq!(run(&mut m, "a /#OA0007 b /#PA0003 c"), "a  b  c");
        assert_eq!(run(&mut m, "/#OC0001"), "");
    }

    #[test]
    fn a_tag_in_the_last_four_characters_stays() {
        let mut m = Mock { objects: vec![(0, false, 2, 0)], ..Default::default() };
        assert_eq!(run(&mut m, "abcd/#OC"), "abcd/#OC");
    }

    #[test]
    fn short_texts_append_the_replacement_or_move_on() {
        // object 0 (no attribute check) counts 2 units: the count tag appends "2" until the text has 4 characters
        let mut m = Mock { objects: vec![(0, false, 2, 0)], ..Default::default() };
        assert_eq!(run(&mut m, "ab"), "ab22");
        // no object 0: the original never ended; the port moves on
        let mut m = Mock::default();
        assert_eq!(run(&mut m, "ok"), "ok");
        assert_eq!(m.reads.iter().filter(|r| r.0 == EE_SCENARIO_READ_TOKEN_OBJECT).count(), 2);
    }

    #[test]
    fn characters_are_code_points() {
        let mut m = Mock { objects: vec![(3, false, 4, 0)], ..Default::default() };
        assert_eq!(run(&mut m, "€€ /#OC0003 ü"), "€€ 4 ü");
    }

    #[test]
    fn atoi_matches_the_c_runtime() {
        assert_eq!(atoi(b"0012 "), 12);
        assert_eq!(atoi(b" -3x"), -3);
        assert_eq!(atoi(b"+7"), 7);
        assert_eq!(atoi(b"abc"), 0);
        assert_eq!(atoi(b""), 0);
    }
}
