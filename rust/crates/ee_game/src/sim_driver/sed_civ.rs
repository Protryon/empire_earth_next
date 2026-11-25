//! TEST ONLY: the civilization builder's steps of the scripted scenario editor gate (the "cb" command; was
//! EESimCivBuilder.cpp; tools/sim_editor.py --civbuilder over tools/sim_editor/civbuilder.txt).
//!
//! The builder is opened as a person opens it (the editor's civilization builder button: the creator over the
//! editor's civilization; or the game's paths: the edit of the current player's, the civilization timer), then driven
//! through its forms' controls (7 the builder, 15 the name dialog, 16 save, 17 load, and the message boxes), and
//! digested through what a person sees and what the game keeps: every control of a form (texts, lists, selections,
//! flags), the forms on the stack, the editor's and the players' civilizations, the edited civilization, and the
//! civilization files' bytes.
//!   cb open                         the editor's civilization builder button released (EESEEOpenCivBuilder)
//!   cb edit | cb timer              CivBuilderEditCivilization | EEProcessTimerExpired (the game's paths)
//!   cb blink 0|1                    EESetCivBuilderButtonBlinking
//!   cb fire FORM ID                 a form's button released (ButtonUp, as by the mouse); ID 0x.. or decimal
//!   cb event FORM ID TRIGGER        a control's trigger event (0x3e8.., e.g. 0x3f0 a list's release) processed
//!   cb pick FORM ID LINE            a list's line selected with its event (its ChangeSelectionIndex)
//!   cb pickname FORM ID NAME        a list's line NAME selected with its event
//!   cb reset FORM ID                a list's ResetSelection; cb tab FORM ID: its TabbedTo
//!   cb copy FORM ID                 a list's CopyData and its snapshot
//!   cb text FORM ID TEXT            an edit box's text ('_' a space, '-' none)
//!   cb answer ID                    the message box on top: its button ID (1 OK/Yes, 2 No) released
//!   cb form FORM                    the form's controls; cb top: the form on top's
//!   cb state                        the stack (and a message box on top: its controls), the editor's and the
//!                                   current player's civilizations, the edited one
//!   cb file NAME | cb rm NAME       Users\default\Civilizations\NAME.civ: its bytes as hex | deleted
//!   cb put NAME HEX                 such a file written from hex bytes
//!   cb save NAME 0|1                CivBuilderSaveCivilization (the campaign's path with 1)
//!   cb load NAME                    EELoadCivilization from the user's directory: its result, name and upgrades
//!   cb apply PLAYER NAME            EEApplyCivilizationFile to a player, then its civilization
//!   cb flag freecam|custom|scenario 0|1   gFreeCamera, gAllowCustomCivs, gIsScenarioGame set
//!   cb close                        the form on top popped
#![allow(clippy::unnecessary_cast)] // c_long and c_ulong are 32 bits on Windows, 64 on LP64

use crate::gamecpp as gc;
use std::ffi::{c_long, c_ulong};

use ee_render::gx::cformat::strtoul_0;

use super::sed_digest::{colour, strip_class};
use super::sed_host::{self as x, h, hs, ht, sel, EeSimDriverSed as O};

/// The bytes as lower-case hex.
pub fn hexed(data: &[u8]) -> String {
    data.iter().map(|b| format!("{b:02x}")).collect()
}

/// "[a,b,...]" of upgrade ids.
pub fn upgrades(list: &[c_ulong]) -> String {
    format!("[{}]", list.iter().map(|u| u.to_string()).collect::<Vec<_>>().join(","))
}

/// A word's text: '-' none, '_' a space.
pub fn word(w: &str) -> String {
    if w == "-" {
        String::new()
    } else {
        w.replace('_', " ")
    }
}

/// "PUT" hex: two digits a byte (strtoul base 16 of each pair; an odd last digit dropped).
pub fn unhex(text: &str) -> Vec<u8> {
    let b = text.as_bytes();
    let mut out = Vec::new();
    let mut i = 0;
    while i + 1 < b.len() {
        let pair = std::str::from_utf8(&b[i..i + 2]).unwrap_or("");
        let digits: String = pair.trim_start().chars().take_while(|c| c.is_ascii_hexdigit()).collect();
        out.push(u32::from_str_radix(&digits, 16).unwrap_or(0) as u8);
        i += 2;
    }
    out
}

fn number(text: &str) -> c_ulong {
    strtoul_0(text) as c_ulong
}

fn form(id: &str) -> usize {
    h(O::UiFormById, 0, &[number(id) as i64]) as usize
}

fn control(f: &str, id: &str) -> usize {
    let form = form(f);
    if form == 0 {
        0
    } else {
        h(O::UiFormControlById, form, &[number(id) as i64]) as usize
    }
}

fn quoted(text: &str) -> String {
    format!("'{text}'")
}

/// A control's or form's class: a shell's is the class its kind replaces.
fn class_of(shell: usize, name: O, object: usize) -> String {
    if shell != 0 {
        ht(O::ShellKindName, shell, 0)
    } else {
        strip_class(ht(name, object, 0))
    }
}

/// A form's controls: index, class, id, display mode, then a label's lines, an edit box's text, a list's flags,
/// selection and every line, a button's trigger events.
fn form_digest(form: usize, out: &mut Vec<String>) {
    if form == 0 {
        out.push("  no form".into());
        return;
    }
    let count = h(O::UiFormControlCount, form, &[]) as c_ulong;
    out.push(format!(
        "  form {} {} controls {count}",
        h(O::UiFormId, form, &[]) as c_ulong,
        class_of(h(O::UiFormShellOf, form, &[]) as usize, O::UiFormTypeName, form)
    ));
    for i in 0..count {
        let c = h(O::UiFormControl, form, &[i as i64]) as usize;
        if c == 0 {
            continue;
        }
        let mut line = format!(
            "  c{i} {} id={} m={}",
            class_of(h(O::ShellOf, c, &[]) as usize, O::ControlTypeName, c),
            h(O::ControlId, c, &[]) as c_ulong,
            x::display_mode(c)
        );
        let b01 = |op: O| if h(op, c, &[]) != 0 { "1" } else { "0" };
        if h(O::IsCivList, c, &[]) != 0 {
            let selection = sel(O::ListSelection, c);
            let n = x::line_count(c);
            line += &format!(
                " upgradesonly={} points={} sort={} tab={} auto={} has={} ix={} hc={} n={n} sel={}",
                b01(O::CivListUpgradesOnly),
                b01(O::CivListShowPoints),
                b01(O::CivListSortLines),
                b01(O::ControlTabable),
                b01(O::ListAutoSelect),
                b01(O::ListHasSelection),
                h(O::ListSelectionIndex, c, &[]) as c_ulong,
                colour(x::color(h(O::CivListHeaderColor, c, &[]) as usize)),
                selection.map_or_else(|| "none".to_string(), |v| v.to_string())
            );
            out.push(line);
            for k in 0..n {
                out.push(format!("    {k} {}", quoted(&ht(O::ListData, c, k as i64))));
            }
            continue;
        }
        if h(O::IsList, c, &[]) != 0 {
            let n = x::line_count(c);
            line += &format!(
                " n={n} sel={}",
                sel(O::ListSelection, c).map_or_else(|| "none".to_string(), |v| v.to_string())
            );
            for k in 0..n.min(8) {
                line += &format!(" {}", quoted(&ht(O::ListData, c, k as i64)));
            }
        } else if h(O::IsEdit, c, &[]) != 0 {
            let n = h(O::EditLineCount, c, &[]) as c_ulong;
            line += &format!(" lines={n} {}", if n > 0 { quoted(&ht(O::EditText, c, 0)) } else { "''".into() });
        } else if h(O::IsTextLabel, c, &[]) != 0 {
            let n = h(O::LabelLineCount, c, &[]) as c_ulong;
            line += &format!(" lines={n}");
            for k in 0..n.min(4) {
                line += &format!(" {}", quoted(&ht(O::LabelText, c, k as i64)));
            }
        }
        // kControlTriggerRangeChanged .. kControlTriggerRightButtonUp
        for t in x::TRIGGER_RANGE_CHANGED..=x::TRIGGER_RANGE_CHANGED + 14 {
            let e = x::trigger_event(c, t);
            if e != 0 {
                line += &format!(" e{}={}", t - x::TRIGGER_RANGE_CHANGED, strip_class(ht(O::EventTypeName, e, 0)));
            }
        }
        out.push(line);
    }
}

fn upgrade_list(count: O, at: O, p: usize) -> Vec<c_ulong> {
    (0..h(count, p, &[])).map(|i| h(at, p, &[i]) as c_ulong).collect()
}

/// A civilization's name, points and upgrades (`label none` without one).
fn civ_digest(label: &str, civ: usize) -> String {
    if civ == 0 {
        return format!("  {label} none");
    }
    format!(
        "  {label} name={} points={} max={} unchanged={} nopoints={} bought={} deferred={}",
        quoted(&ht(O::CivName, civ, 0)),
        h(O::CivAvailablePoints, civ, &[]) as c_ulong,
        h(O::CivMaxPoints, civ, &[]) as c_ulong,
        h(O::CivUnchanged, civ, &[]),
        h(O::CivNoPointsAdded, civ, &[]),
        upgrades(&upgrade_list(O::CivPurchasedCount, O::CivPurchasedAt, civ)),
        upgrades(&upgrade_list(O::CivDeferredCount, O::CivDeferredAt, civ))
    )
}

// the edited civilization as the builder keeps it (ee_screens civ_whole.rs: its TEST ONLY queries)
fn query(what: u32, k: u32) -> i64 {
    // SAFETY: the builder's Rust state (a query only)
    unsafe { x::ee_screens_cb_civilization_query(what, k) }
}

fn query_list(count: u32, item: u32) -> String {
    let items: Vec<String> = (0..query(count, 0)).map(|i| query(item, i as u32).to_string()).collect();
    format!("[{}]", items.join(","))
}

fn rust_text(get: unsafe extern "C" fn(*mut u8, usize) -> usize) -> String {
    // SAFETY: the Rust getter's size query, then the copy into a buffer of that size
    let n = unsafe { get(std::ptr::null_mut(), 0) };
    let mut buffer = vec![0u8; n];
    unsafe { get(buffer.as_mut_ptr(), n) };
    format!("'{}'", String::from_utf8_lossy(&buffer))
}

fn edit_digest() -> String {
    let kind = query(0, 0);
    if kind == 0 {
        return "  edit none".into();
    }
    let kinds = ["", "edit", "create", "create-unlimited"];
    let q = |what: u32| query(what, 0).to_string();
    format!(
        "  edit {} name={} left={} max={} raw={} player={} choices={} canchange={} upgrades={} ids={}",
        kinds[kind as usize],
        rust_text(x::ee_screens_cb_civilization_name),
        q(1),
        q(2),
        q(3),
        q(4),
        q(5),
        q(6),
        query_list(7, 8),
        query_list(9, 10)
    )
}

/// Users\default\Civilizations\`name`.civ
fn user_spec(name: &str) -> usize {
    let spec = x::spec_in("Users\\default\\Civilizations\\");
    hs(O::SpecSetFile, spec, &format!("{name}.civ"));
    spec
}

/// One "cb" step (on the UI thread, in the step's action): false for an unknown sub-command.
pub fn step(w: &[String], out: &mut Vec<String>) -> bool {
    let c = w.get(1).map_or("", |s| s.as_str());
    let arg = |i: usize| w.get(i).map_or("", |s| s.as_str());
    match c {
        "open" => {
            let button = x::control_by_id(17);
            let e = x::trigger_event(button, x::TRIGGER_BUTTON_UP);
            if e != 0 {
                x::process_event(e, button);
            }
        }
        "edit" => out.push(format!("  edit -> {}", h(O::CivEdit, 0, &[]) as c_long)),
        "timer" => out.push(format!("  timer -> {}", h(O::CivTimer, 0, &[]) as c_long)),
        "blink" => {
            out.push(format!("  blink -> {}", h(O::CivBlink, 0, &[(number(arg(2)) != 0) as i64]) as c_long));
            let main = h(O::UiFormById, 0, &[11]) as usize; // kFormMainGame
                                                            // kMainGameCivSelectorBuilder, kMainGameLargeCivBuilderAlert
            for k in [5i64, 234] {
                let button = if main == 0 { 0 } else { h(O::UiFormControl, main, &[k]) as usize };
                out.push(format!(
                    "  main c{k} {}",
                    if button == 0 { "none".to_string() } else { format!("m={}", x::display_mode(button)) }
                ));
            }
        }
        "fire" => {
            let b = control(arg(2), arg(3));
            if b == 0 || h(O::IsButton, b, &[]) == 0 {
                out.push("  fire: no button".into());
            } else {
                h(O::ButtonUp, b, &[]);
            }
        }
        "event" => {
            // cb event FORM ID TRIGGER: a control's trigger event (0x3e8..) processed
            let ctl = control(arg(2), arg(3));
            let e = if ctl == 0 { 0 } else { x::trigger_event(ctl, number(arg(4))) };
            if e == 0 {
                out.push("  event: none".into());
            } else {
                out.push(format!(
                    "  event -> {}",
                    h(O::ProcessEventOn, e, &[form(arg(2)) as i64, ctl as i64]) as c_long
                ));
            }
        }
        "pick" => {
            let l = control(arg(2), arg(3));
            if l == 0 || h(O::IsList, l, &[]) == 0 {
                out.push("  pick: no list".into());
            } else {
                h(O::ListChangeSelection, l, &[number(arg(4)) as i64, 1, 0]);
            }
        }
        "pickname" => {
            // cb pickname FORM ID NAME: a file list's line NAME selected with its event
            let l = control(arg(2), arg(3));
            let l = if l != 0 && h(O::IsList, l, &[]) != 0 { l } else { 0 };
            let name = word(arg(4));
            let n = if l != 0 { x::line_count(l) } else { 0 };
            let mut k = 0;
            while l != 0 && k < n && x::hsa(O::ListDataEquals, l, &[k as i64], &name) == 0 {
                k += 1;
            }
            if l == 0 || k == n {
                out.push("  pickname: none".into());
            } else {
                out.push(format!("  pickname line {k}"));
                h(O::ListChangeSelection, l, &[k as i64, 1, 0]);
            }
        }
        "reset" | "tab" => {
            let l = control(arg(2), arg(3));
            if l == 0 || h(O::IsCivList, l, &[]) == 0 {
                out.push(format!("  {c}: no list"));
            } else if c == "reset" {
                h(O::CivListReset, l, &[]);
            } else {
                h(O::CivListTabbedTo, l, &[]);
            }
        }
        "copy" => {
            // cb copy FORM ID: a list's CopyData, then its snapshot's flags
            let l = control(arg(2), arg(3));
            if l == 0 || h(O::IsCivList, l, &[]) == 0 {
                out.push("  copy: no list".into());
            } else {
                let result = h(O::CivListCopyData, l, &[]) as c_long;
                out.push(format!(
                    "  copy -> {result} points={} hc={} has={} drawn={}",
                    h(O::CivListCopiedShowPoints, l, &[]),
                    colour(x::color(h(O::CivListCopiedColor, l, &[]) as usize)),
                    h(O::ListCopiedHasSelection, l, &[]),
                    h(O::ListDrawCount, l, &[]) as c_ulong
                ));
            }
        }
        "text" => {
            let e = control(arg(2), arg(3));
            if e == 0 || h(O::IsEdit, e, &[]) == 0 {
                out.push("  text: no edit box".into());
            } else {
                hs(O::EditSetText, e, &word(arg(4)));
            }
        }
        "answer" => {
            let top = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
            let b = if top == 0 { 0 } else { h(O::UiFormControlById, top, &[number(arg(2)) as i64]) as usize };
            if b == 0 || h(O::IsButton, b, &[]) == 0 {
                out.push("  answer: no button".into());
            } else {
                h(O::ButtonUp, b, &[]);
            }
        }
        "form" => form_digest(form(arg(2)), out),
        "top" => form_digest(gc::h(gc::O::SsCurrentForm, 0, &[]) as usize, out),
        "state" => {
            let mut stack = "  stack".to_string();
            for i in 0..h(O::UiFormCount, 0, &[]) {
                let f = h(O::UiFormStacked, 0, &[i]) as usize;
                stack += &if f == 0 { " null".to_string() } else { format!(" {}", h(O::UiFormId, f, &[]) as c_ulong) };
            }
            out.push(stack);
            // a message box left on top (none in a passing run's states): its text names why (an error box of
            // the builder's file writes, a question not yet closed)
            let top = gc::h(gc::O::SsCurrentForm, 0, &[]) as usize;
            if top != 0 && class_of(h(O::UiFormShellOf, top, &[]) as usize, O::UiFormTypeName, top) == "EEUIFError" {
                out.push("  a message box on top:".into());
                form_digest(top, out);
            }
            let editor = x::editor();
            out.push(civ_digest(
                "editor",
                if editor != 0 { h(O::EditorCivilization, editor, &[]) as usize } else { 0 },
            ));
            let player = gc::h(gc::O::C11NmCurrent, 0, &[]) as usize;
            out.push(civ_digest(
                "player",
                if player != 0 { h(O::PlayerCivilization, player, &[]) as usize } else { 0 },
            ));
            if player != 0 {
                out.push(format!(
                    "  chosen={} locked={} file={}",
                    h(O::PlayerCivilizationChosen, player, &[]),
                    h(O::PlayerCivilizationLocked, player, &[]),
                    quoted(&ht(O::PlayerCivilizationFile, player, 0))
                ));
            }
            out.push(edit_digest());
            out.push(format!("  last={}", rust_text(x::ee_screens_cb_last_file)));
        }
        "file" => {
            let spec = user_spec(arg(2));
            let mut line = format!("  file {}", arg(2));
            let mut size: c_ulong = 0;
            if h(O::SpecOpenRead, spec, &[]) != 0 || h(O::SpecGetLength, spec, &[&mut size as *mut c_ulong as i64]) != 0
            {
                line += " missing";
            } else {
                let mut bytes = vec![0u8; size as usize + 1];
                let mut read: c_ulong = 0;
                h(O::SpecRead, spec, &[bytes.as_mut_ptr() as i64, size as i64, &mut read as *mut c_ulong as i64]);
                line += &format!(" {read} {}", hexed(&bytes[..read as usize]));
            }
            h(O::SpecClose, spec, &[]);
            h(O::SpecFree, spec, &[]);
            out.push(line);
        }
        "rm" => {
            let spec = user_spec(arg(2));
            h(O::SpecDelete, spec, &[]);
            h(O::SpecFree, spec, &[]);
        }
        "put" => {
            let spec = user_spec(arg(2));
            let bytes = unhex(arg(3));
            h(O::SpecDelete, spec, &[]);
            h(O::SpecCreate, spec, &[]);
            let mut written: c_ulong = 0;
            if h(O::SpecOpenWrite, spec, &[]) == 0 {
                h(
                    O::SpecWrite,
                    spec,
                    &[bytes.as_ptr() as i64, bytes.len() as i64, &mut written as *mut c_ulong as i64],
                );
            }
            h(O::SpecClose, spec, &[]);
            h(O::SpecFree, spec, &[]);
        }
        "save" => {
            let result = x::hsa(O::CivSave, 0, &[(number(arg(3)) != 0) as i64], &word(arg(2)));
            out.push(format!("  save -> {}", result as c_long));
        }
        "load" => {
            let name = h(O::WideNew, 0, &[]) as usize;
            let list = h(O::UpgradesNew, 0, &[]) as usize;
            let result = x::hsa(O::CivLoad, 0, &[name as i64, list as i64], &word(arg(2)));
            out.push(format!(
                "  load -> {} {} {}",
                result as c_long,
                quoted(&ht(O::WideText, name, 0)),
                upgrades(&upgrade_list(O::UpgradesCount, O::UpgradesAt, list))
            ));
            h(O::UpgradesFree, list, &[]);
            gc::h(gc::O::SsDeleteString, name, &[]);
        }
        "apply" => {
            let player = gc::h(gc::O::SsWorldPlayer, 0, &[number(arg(2)) as i64]) as usize;
            let name = hs(O::NarrowNew, 0, &word(arg(3))) as usize;
            out.push(format!("  apply -> {}", h(O::CivApply, player, &[name as i64]) as c_long));
            out.push(civ_digest("applied", h(O::PlayerCivilization, player, &[]) as usize));
            gc::h(gc::O::UStrFree, name, &[]);
        }
        "flag" => {
            // cb flag freecam|custom|scenario 0|1: the game's flags the builder reads
            let on = (number(arg(3)) != 0) as i64;
            match arg(2) {
                "freecam" => h(O::SetFreeCamera, 0, &[on]),
                "custom" => h(O::SetAllowCustomCivs, 0, &[on]),
                _ => h(O::SetScenarioGame, 0, &[on]),
            };
        }
        "close" => {
            h(O::UiPop, 0, &[]);
        }
        _ => return false,
    }
    true
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn texts() {
        assert_eq!(hexed(&[0, 0xab, 0x10]), "00ab10");
        assert_eq!(unhex("00ab1"), vec![0, 0xab]);
        assert_eq!(unhex("zz7f"), vec![0, 0x7f]);
        assert_eq!(upgrades(&[3, 4]), "[3,4]");
        assert_eq!(upgrades(&[]), "[]");
        assert_eq!(word("-"), "");
        assert_eq!(word("a_b"), "a b");
    }
}
