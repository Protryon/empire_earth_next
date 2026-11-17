//! The civilization builder's list box whole (the original `EECBUIListBox.cpp`): its lines are '/'-separated
//! tokens, an upgrade "category/name/bonus/id/cost/points/value text" or a category's header
//! "category/cost/Category"; the lines kept sorted (the special powers first, then case-insensitively), a header per
//! category with upgrades, the selection kept on upgrade lines, the points recomputed as the edited civilization's
//! costs or refunds, and the lines drawn. The C++ class keeps the toolkit's base (`UILBString`: the lines, the
//! selection, the drawing) and its members; every override is a forwarder into these functions, which reach the
//! base and the game through [`CbHost`] ops on the list (`p` the `EECBUIListBox*`).

use core::cmp::Ordering;
use core::ffi::c_long;

use crate::civ_whole::*;

/// The tokens of a line.
pub const TOKEN_CATEGORY: usize = 0;
pub const TOKEN_NAME: usize = 1;
pub const TOKEN_BONUS: usize = 2;
pub const TOKEN_UPGRADE: usize = 3;
pub const TOKEN_COST: usize = 4;
pub const TOKEN_POINTS: usize = 5;
pub const TOKEN_VALUE: usize = 6;

const SEPARATOR: u8 = b'/';
const CATEGORY: &[u8] = b"Category";
const INDENT: &[u8] = b"       ";
const SPACER: &[u8] = b"  ";

/// `GetToken`: the token at `index` of a '/'-separated line (empty tokens skipped, as `UWideString::Tokenize`).
pub fn token(line: &[u8], index: usize) -> Option<Vec<u8>> {
    line.split(|&b| b == SEPARATOR).filter(|t| !t.is_empty()).nth(index).map(<[u8]>::to_vec)
}

/// `sscanf(token, "%lu")`: white space, a sign, at least one digit (an unsigned long wraps).
pub fn scan_unsigned(text: &[u8]) -> Option<u32> {
    let mut i = 0;
    while i < text.len() && matches!(text[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let negative = i < text.len() && text[i] == b'-';
    if i < text.len() && (text[i] == b'-' || text[i] == b'+') {
        i += 1;
    }
    let start = i;
    let mut value: u32 = 0;
    while i < text.len() && text[i].is_ascii_digit() {
        value = value.wrapping_mul(10).wrapping_add((text[i] - b'0') as u32);
        i += 1;
    }
    if i == start {
        return None;
    }
    Some(if negative { value.wrapping_neg() } else { value })
}

/// `GetNumber`: the token at `index` as an unsigned number.
pub fn number(line: &[u8], index: usize) -> Option<u32> {
    token(line, index).and_then(|t| scan_unsigned(&t))
}

/// `IsUpgradeLine`: the line has an upgrade token.
pub fn is_upgrade_line(line: &[u8]) -> bool {
    token(line, TOKEN_UPGRADE).is_some()
}

/// A category's header line.
pub fn header(category: &[u8], cost: &[u8]) -> Vec<u8> {
    let mut out = category.to_vec();
    out.push(SEPARATOR);
    out.extend_from_slice(cost);
    out.push(SEPARATOR);
    out.extend_from_slice(CATEGORY);
    out
}

fn lower(text: &[u8]) -> Vec<u8> {
    let mut b = text.to_vec();
    b.push(0);
    // SAFETY: a NUL-terminated buffer, lowered in place
    unsafe { ee_base::plat_core::string::PlatMbsLwr(b.as_mut_ptr()) };
    b.pop();
    b
}

/// The first `count` characters (`GetIndexedSubstring(0, count - 1)`; none for 0).
fn prefix(text: &[u8], count: usize) -> &[u8] {
    let mut b = text.to_vec();
    b.push(0);
    // SAFETY: a NUL-terminated buffer
    let end = unsafe { ee_base::plat_core::string::PlatMbsNInc(b.as_ptr(), count).offset_from(b.as_ptr()) } as usize;
    &text[..end.min(text.len())]
}

fn mbs_icmp(left: &[u8], right: &[u8]) -> i32 {
    let mut l = left.to_vec();
    l.push(0);
    let mut r = right.to_vec();
    r.push(0);
    // SAFETY: NUL-terminated buffers
    unsafe { ee_base::plat_core::string::PlatMbsICmp(l.as_ptr(), r.as_ptr()) }
}

/// `EECBLineSorter`: whether `left` goes before `right`: the lines that start with the special powers' name
/// (both lowered, the name's character count compared) first, then case-insensitively. `special` is the name as the
/// language gives it.
pub fn line_less(special: &[u8], left: &[u8], right: &[u8]) -> bool {
    let unassigned = lower(special);
    let count = char_count(special);
    let l_special = prefix(&lower(left), count) == unassigned.as_slice();
    let r_special = prefix(&lower(right), count) == unassigned.as_slice();
    if l_special && !r_special {
        true
    } else if !l_special && r_special {
        false
    } else {
        mbs_icmp(left, right) < 0
    }
}

/// [`line_less`] as an order.
pub fn line_order(special: &[u8], left: &[u8], right: &[u8]) -> Ordering {
    if line_less(special, left, right) {
        Ordering::Less
    } else if line_less(special, right, left) {
        Ordering::Greater
    } else {
        Ordering::Equal
    }
}

/// The characters of a UTF-8 text (`UWideString::GetLength`).
fn char_count(text: &[u8]) -> usize {
    let mut b = text.to_vec();
    b.push(0);
    // SAFETY: a NUL-terminated buffer
    unsafe { ee_base::plat_core::string::PlatMbsLen(b.as_ptr()) }
}

/// The lines sorted by [`line_order`] (no two lines of a list compare equal: their upgrades or categories differ,
/// so this stable sort orders them as the original's `std::sort`).
pub fn sorted(special: &[u8], lines: &[Vec<u8>]) -> Vec<Vec<u8>> {
    let mut out = lines.to_vec();
    out.sort_by(|a, b| line_order(special, a, b));
    out
}

// ---------------------------------------------------------------------------------------------- the base's

fn count<H: CbHost + ?Sized>(h: &mut H, l: usize) -> u32 {
    h.v(EE_SCREENS_CBW_L_COUNT, l, 0, 0, 0) as u32
}

/// The line at `index` (`GetData`).
pub fn line_at<H: CbHost + ?Sized>(h: &mut H, l: usize, index: u32) -> Vec<u8> {
    h.t(EE_SCREENS_CBW_L_LINE, l, index as i64, 0, 0).1
}

/// `GetLine`: the line, `None` past the end.
fn get_line<H: CbHost + ?Sized>(h: &mut H, l: usize, index: u32) -> Option<Vec<u8>> {
    (index < count(h, l)).then(|| line_at(h, l, index))
}

/// Every line (`GetDataArray`).
pub fn lines<H: CbHost + ?Sized>(h: &mut H, l: usize) -> Vec<Vec<u8>> {
    (0..count(h, l)).map(|i| line_at(h, l, i)).collect()
}

fn append<H: CbHost + ?Sized>(h: &mut H, l: usize, line: &[u8]) {
    h.vt(EE_SCREENS_CBW_L_APPEND, l, 0, 0, line);
}

fn base_select<H: CbHost + ?Sized>(h: &mut H, l: usize, index: i64, fire: bool, scroll: bool) {
    h.v(EE_SCREENS_CBW_L_SELECT, l, index, fire as i64, scroll as i64);
}

/// `GetCurrentSelection`: the selected line, `None` without a selection.
pub fn current_selection<H: CbHost + ?Sized>(h: &mut H, l: usize) -> Option<u32> {
    let (r, values) = h.vs(EE_SCREENS_CBW_L_CURRENT, l, 0, 0);
    (r == 0).then(|| values.first().copied().unwrap_or(0) as u32)
}

fn flag<H: CbHost + ?Sized>(h: &mut H, l: usize, which: i64) -> bool {
    h.v(EE_SCREENS_CBW_L_FLAG, l, which, 0, 0) != 0
}

fn special_name<H: CbHost + ?Sized>(h: &mut H) -> Vec<u8> {
    h.string(IDS_SPECIAL_POWERS).1
}

// ---------------------------------------------------------------------------------------------- the class's

/// `AddCategory`: the header of an upgrade line's category, once (then sorted).
pub fn add_category<H: CbHost + ?Sized>(h: &mut H, l: usize, line: &[u8]) -> bool {
    let Some(category) = token(line, TOKEN_CATEGORY) else { return false };
    if find_category(h, l, &category).is_some() {
        return false;
    }
    let Some(cost) = token(line, TOKEN_COST) else { return false };
    append(h, l, &header(&category, &cost));
    sort_lines(h, l);
    true
}

/// `AddLine`: a line (and its category's header when new), sorted.
pub fn add_line<H: CbHost + ?Sized>(h: &mut H, l: usize, line: &[u8]) {
    append(h, l, line);
    if !add_category(h, l, line) {
        sort_lines(h, l);
    }
}

/// `AppendStringArray`: each line added.
pub fn append_lines<H: CbHost + ?Sized>(h: &mut H, l: usize, new_lines: &[Vec<u8>]) {
    for line in new_lines {
        add_line(h, l, line);
    }
}

/// `ChangeSelectionIndex`: in upgrade-only mode a header passes the selection to the nearest upgrade line in the
/// direction of travel (none with auto select); with that mode off the selection is reset.
pub fn change_selection<H: CbHost + ?Sized>(h: &mut H, l: usize, index: u32, fire: bool, scroll: bool) {
    if !flag(h, l, EE_SCREENS_CBW_FLAG_UPGRADES_ONLY) {
        reset_selection(h, l);
        return;
    }
    if is_upgrade_at(h, l, index).is_some() {
        base_select(h, l, index as i64, fire, scroll);
        return;
    }
    if h.v(EE_SCREENS_CBW_L_AUTO, l, 0, 0, 0) != 0 {
        return;
    }
    let current = current_selection(h, l).unwrap_or(0);
    // the direction of travel first (the original's long loops from index + 1 and index - 1)
    let step = if current < index { 1 } else { -1 };
    let index = index as i32 as i64;
    if !search(h, l, index + step, step, fire, scroll) {
        search(h, l, index - step, -step, fire, scroll);
    }
}

/// The first upgrade line from `from` in the direction `step` selected (the base's); false when there is none.
fn search<H: CbHost + ?Sized>(h: &mut H, l: usize, from: i64, step: i64, fire: bool, scroll: bool) -> bool {
    let mut i = from;
    // (the backward loop is bounded by 0 only: a line past the end is no upgrade line)
    while i >= 0 && (step < 0 || i < count(h, l) as i32 as i64) {
        if is_upgrade_at(h, l, i as u32).is_some() {
            base_select(h, l, i, fire, scroll);
            return true;
        }
        i += step;
    }
    false
}

/// `SetUpgrades`: the list rebuilt from upgrades (a formatted line each, a header per category), sorted, the
/// points recomputed (`buy`: costs, else refunds).
pub fn set_upgrades<H: CbHost + ?Sized>(h: &mut H, l: usize, upgrades: &[u32], buy: bool) {
    let sort = flag(h, l, EE_SCREENS_CBW_FLAG_SORT);
    h.v(EE_SCREENS_CBW_L_DELETE_ALL, l, 0, 0, 0);
    h.v(EE_SCREENS_CBW_L_SET_FLAG, l, EE_SCREENS_CBW_FLAG_SORT, 0, 0);
    let mut categories: Vec<Vec<u8>> = Vec::new();
    for &u in upgrades {
        let Some(line) = format_upgrade(h, u, buy, 0) else { continue };
        append(h, l, &line);
        let Some(category) = token(&line, TOKEN_CATEGORY) else { continue };
        if categories.contains(&category) {
            continue;
        }
        let Some(cost) = token(&line, TOKEN_COST) else { continue };
        append(h, l, &header(&category, &cost));
        categories.push(category);
    }
    h.v(EE_SCREENS_CBW_L_SET_FLAG, l, EE_SCREENS_CBW_FLAG_SORT, sort as i64, 0);
    sort_lines(h, l);
    reformat(h, l, buy);
}

/// `DeleteLine`: a line deleted, its category's header with it when no upgrade of that category is left.
pub fn delete_line<H: CbHost + ?Sized>(h: &mut H, l: usize, index: u32) {
    let Some(line) = get_line(h, l, index) else { return };
    if token(&line, TOKEN_CATEGORY).is_none() {
        return;
    }
    h.v(EE_SCREENS_CBW_L_DELETE, l, index as i64, 0, 0);
    remove_category_if_empty(h, l, &line);
}

/// `ReformatUpgrades`: every upgrade line formatted again with its cost (`buy`) or refund given its place in its
/// category.
pub fn reformat<H: CbHost + ?Sized>(h: &mut H, l: usize, buy: bool) {
    let mut index = 0i64;
    for i in 0..count(h, l) {
        let line = line_at(h, l, i);
        if !is_upgrade_line(&line) {
            index = 0;
            continue;
        }
        if let Some(u) = number(&line, TOKEN_UPGRADE) {
            if let Some(new) = format_upgrade(h, u, buy, index) {
                h.vt(EE_SCREENS_CBW_L_SET_LINE, l, i as i64, 0, &new);
            }
            index += 1;
        }
    }
}

/// `FormatUpgrade`: "category/name/bonus/id/cost/points/value", the points its cost (`buy`) or its refund less the
/// category's cost per place before it; `None` for an upgrade without a level or names.
pub fn format_upgrade<H: CbHost + ?Sized>(h: &mut H, upgrade: u32, buy: bool, index: i64) -> Option<Vec<u8>> {
    let (category, level) = (upgrade >> 16, upgrade & 0xffff);
    let (found, levels) = h.vs(EE_SCREENS_CBW_LOOKUP_LEVEL, 0, upgrade as i64, 0);
    let value = f32::from_bits(levels.first().copied().unwrap_or(0) as u32);
    let bonus = f32::from_bits(levels.get(1).copied().unwrap_or(0) as u32);
    let cost = h.v(EE_SCREENS_CBW_EXTRA_COST, 0, upgrade as i64, 0, 0);
    if found == 0 || value == 0.0 {
        return None;
    }
    let (has_name, ids) = h.vs(EE_SCREENS_CBW_CATEGORY_NAME_ID, 0, category as i64, 0);
    if has_name == 0 {
        return None;
    }
    let (error, category_name) = h.string(ids.first().copied().unwrap_or(0));
    let mut points = 0i64;
    if let Some(e) = editable() {
        points = if buy {
            e.cost(h, upgrade) as i32 as i64
        } else {
            (e.refund(h, upgrade) as i32).wrapping_sub((cost as i32).wrapping_mul(index as i32)) as i64
        };
    }
    if error != 0 {
        return None;
    }
    let name_id = h.v(EE_SCREENS_CBW_LEVEL_NAME_ID, 0, level as i64, 0, 0);
    let (error, upgrade_name) = h.string(name_id);
    let (_, value_text) = h.t(EE_SCREENS_CBW_PERCENT, 0, (value * 100.0).to_bits() as i64, 0, 0);
    if error != 0 {
        return None;
    }
    let mut out = category_name;
    for part in [
        upgrade_name,
        ((bonus * 100.0) as i32).to_string().into_bytes(),
        (upgrade as i32).to_string().into_bytes(),
        (cost as i32).to_string().into_bytes(),
        (points as i32).to_string().into_bytes(),
        value_text,
    ] {
        out.push(SEPARATOR);
        out.extend_from_slice(&part);
    }
    Some(out)
}

/// `FindCategory`: the header line of a category.
pub fn find_category<H: CbHost + ?Sized>(h: &mut H, l: usize, category: &[u8]) -> Option<u32> {
    (0..count(h, l)).find(|&i| {
        let line = line_at(h, l, i);
        token(&line, TOKEN_BONUS).as_deref() == Some(CATEGORY)
            && token(&line, TOKEN_CATEGORY).as_deref() == Some(category)
    })
}

/// `IsUpgradeAt`: the line at `index` when it is an upgrade line.
pub fn is_upgrade_at<H: CbHost + ?Sized>(h: &mut H, l: usize, index: u32) -> Option<Vec<u8>> {
    get_line(h, l, index).filter(|line| is_upgrade_line(line))
}

/// `FindUpgrade`: the line of an upgrade.
pub fn find_upgrade<H: CbHost + ?Sized>(h: &mut H, l: usize, upgrade: u32) -> Option<u32> {
    (0..count(h, l)).find(|&i| number(&line_at(h, l, i), TOKEN_UPGRADE) == Some(upgrade))
}

/// `GetSelectedUpgrade`: the selected line's upgrade, bonus and cost; `Err` with `GetCurrentSelection`'s error, or
/// 3 on the first line. A line that does not read whole answers Ok with `init` for what it lacks (the original's
/// out parameters were left as they were).
pub fn selected_upgrade<H: CbHost + ?Sized>(
    h: &mut H,
    l: usize,
    init: (u32, u32, u32),
) -> Result<(u32, u32, u32), c_long> {
    let (r, values) = h.vs(EE_SCREENS_CBW_L_CURRENT, l, 0, 0);
    if r != 0 {
        return Err(r as c_long);
    }
    let selection = values.first().copied().unwrap_or(0) as u32;
    if selection == 0 {
        return Err(ERROR_FAILED);
    }
    // GetUpgrade sets its value on success only; GetNumber sets 0 for a missing token, leaves it for one that does
    // not scan
    let scan = |line: &[u8], index: usize, value: &mut u32| -> bool {
        match token(line, index) {
            None => {
                *value = 0;
                false
            }
            Some(t) => match scan_unsigned(&t) {
                Some(v) => {
                    *value = v;
                    true
                }
                None => false,
            },
        }
    };
    let mut out = init;
    if let Some(line) = get_line(h, l, selection) {
        if let Some(u) = number(&line, TOKEN_UPGRADE) {
            out.0 = u;
            if scan(&line, TOKEN_BONUS, &mut out.1) {
                scan(&line, TOKEN_COST, &mut out.2);
            }
        }
    }
    Ok(out)
}

/// `SetUpgradesExcept`: every upgrade but the excluded ones, at their costs.
pub fn set_upgrades_except<H: CbHost + ?Sized>(h: &mut H, l: usize, excluded: &[u32]) {
    if l == 0 {
        return;
    }
    let (_, all) = h.vs(EE_SCREENS_CBW_ALL_UPGRADES, 0, 0, 0);
    let upgrades: Vec<u32> = all.into_iter().map(|v| v as u32).filter(|u| !excluded.contains(u)).collect();
    set_upgrades(h, l, &upgrades, true);
}

/// `SetCivilizationUpgrades`: the civilization's upgrades, at their refunds.
pub fn set_civilization_upgrades<H: CbHost + ?Sized>(h: &mut H, l: usize, upgrades: &[u32]) {
    if l == 0 {
        return;
    }
    set_upgrades(h, l, upgrades, false);
}

/// `RemoveCategory`: the first line of a category (its header) deleted.
pub fn remove_category<H: CbHost + ?Sized>(h: &mut H, l: usize, category: &[u8]) -> bool {
    for i in 0..count(h, l) {
        if token(&line_at(h, l, i), TOKEN_CATEGORY).as_deref() == Some(category) {
            h.v(EE_SCREENS_CBW_L_DELETE, l, i as i64, 0, 0);
            return true;
        }
    }
    false
}

/// `RemoveCategoryIfEmpty`: a deleted line's category's header removed when no upgrade of it is left.
pub fn remove_category_if_empty<H: CbHost + ?Sized>(h: &mut H, l: usize, line: &[u8]) -> bool {
    let Some(category) = token(line, TOKEN_CATEGORY) else { return false };
    for i in 0..count(h, l) {
        if let Some(other) = is_upgrade_at(h, l, i) {
            if token(&other, TOKEN_CATEGORY).as_deref() == Some(category.as_slice()) {
                return false;
            }
        }
    }
    remove_category(h, l, &category)
}

/// `ResetSelection`: line 0 selected without an event.
pub fn reset_selection<H: CbHost + ?Sized>(h: &mut H, l: usize) {
    base_select(h, l, 0, false, false);
}

/// `SortLines`: the lines sorted when sorting is on.
pub fn sort_lines<H: CbHost + ?Sized>(h: &mut H, l: usize) {
    if !flag(h, l, EE_SCREENS_CBW_FLAG_SORT) {
        return;
    }
    let special = special_name(h);
    let old = lines(h, l);
    let new = sorted(&special, &old);
    for (i, (a, b)) in old.iter().zip(new.iter()).enumerate() {
        if a != b {
            h.vt(EE_SCREENS_CBW_L_SET_LINE, l, i as i64, 0, b);
        }
    }
}

/// `TabbedTo`: in upgrade-only mode without auto select or a selection, a selection off a header moved; redrawn.
pub fn tabbed_to<H: CbHost + ?Sized>(h: &mut H, l: usize) {
    if h.v(EE_SCREENS_CBW_L_AUTO, l, 0, 0, 0) == 0
        && flag(h, l, EE_SCREENS_CBW_FLAG_UPGRADES_ONLY)
        && h.v(EE_SCREENS_CBW_L_HAS, l, 0, 0, 0) == 0
    {
        let index = h.v(EE_SCREENS_CBW_L_INDEX, l, 0, 0, 0) as u32;
        if is_upgrade_at(h, l, index).is_none() {
            change_selection(h, l, 0, false, true);
        }
    }
    h.v(EE_SCREENS_CBW_L_DIRTY, l, 0, 0, 0);
}

fn draw<H: CbHost + ?Sized>(h: &mut H, l: usize, line: i64, flags: i64, colour: i64, text: &[u8]) -> i64 {
    h.op(EE_SCREENS_CBW_L_DRAW, l, line, flags, colour, text, &mut EeScreensCbOut::default())
}

/// `DrawTextA`: every visible line drawn as an upgrade line (its value and name left, its points right in dark
/// blue when shown; a power's value left out) or a header (the special powers' in bold), then the scroll bar's
/// text.
pub fn draw_text<H: CbHost + ?Sized>(h: &mut H, l: usize) -> i64 {
    let drawn = h.v(EE_SCREENS_CBW_L_DRAW_BEGIN, l, 0, 0, 0);
    let mut result = 0i64;
    let mut i = 0i64;
    while i < drawn && result == 0 {
        let line = h.t(EE_SCREENS_CBW_L_COPY_LINE, l, i, 0, 0).1;
        result = if is_upgrade_line(&line) { draw_upgrade(h, l, i, &line) } else { draw_header(h, l, i, &line) };
        i += 1;
    }
    h.v(EE_SCREENS_CBW_L_DRAW_END, l, 0, 0, 0)
}

fn draw_header<H: CbHost + ?Sized>(h: &mut H, l: usize, i: i64, line: &[u8]) -> i64 {
    let (Some(category), Some(_)) = (token(line, TOKEN_CATEGORY), token(line, TOKEN_NAME)) else {
        return ERROR_FAILED as i64;
    };
    let bold = category == special_name(h);
    draw(h, l, i, if bold { EE_SCREENS_CBW_DRAW_BOLD } else { 0 }, EE_SCREENS_CBW_COLOUR_HEADER, &category)
}

fn draw_upgrade<H: CbHost + ?Sized>(h: &mut H, l: usize, i: i64, line: &[u8]) -> i64 {
    let (Some(name), Some(mut value), Some(mut points)) =
        (token(line, TOKEN_NAME), token(line, TOKEN_VALUE), token(line, TOKEN_POINTS))
    else {
        return ERROR_FAILED as i64;
    };
    value.push(b' ');
    if let Some(u) = number(line, TOKEN_UPGRADE) {
        if h.v(EE_SCREENS_CBW_IS_POWER_LEVEL, 0, (u & 0xffff) as i64, 0, 0) != 0 {
            value.clear();
        }
    }
    let mut text = INDENT.to_vec();
    text.extend_from_slice(&value);
    text.extend_from_slice(&name);
    let result = draw(h, l, i, 0, EE_SCREENS_CBW_COLOUR_BLACK, &text);
    if result != 0 || !flag(h, l, EE_SCREENS_CBW_FLAG_COPIED_POINTS) {
        return result;
    }
    points.extend_from_slice(SPACER);
    draw(h, l, i, EE_SCREENS_CBW_DRAW_RIGHT, EE_SCREENS_CBW_COLOUR_DARK_BLUE, &points)
}

#[cfg(test)]
mod tests;
