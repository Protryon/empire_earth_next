//! The factories against the real C++: a scratch driver (`build/sc13/fac_driver.cpp`) called the unmodified
//! `EEUIManager.cpp`'s fourteen control-event factories in the ctor's registration order (the toolkit's loop: the
//! first answer that is not NULL) and its form-event factory, for every id below 1024 and each value of the screen
//! and editor flags, printing the event made (`golden_factories.txt`: the lines of the events made; every other id
//! made none). The driver relinked over the new unit (`EECreateGameEvent` over this module, then WON's factory, and
//! `EECreateFormEvent`) printed the same 5,121 lines. The replay checks each id's code: a screen event's code, the
//! `EE_SCREENS_CPP_*` code of a C++ class, 0 for WON's server select Cancel (WON's own factory, after this table).

use super::*;

const GOLDEN: &str = include_str!("../golden_factories.txt");

fn expected(what: &str) -> u32 {
    if let Some(code) = what.strip_prefix("screen ") {
        return u32::from_str_radix(code, 16).unwrap();
    }
    match what {
        "cancel editor" | "cancel maingame" => EE_SCREENS_CPP_CANCEL_LOAD,
        "class EEUIELoadUnit" => EE_SCREENS_CPP_LOAD_UNIT,
        "class EEUIEWNSOpen" => EE_SCREENS_CPP_WNS_OPEN,
        "class EEUIEWNSCancel" => 0,
        other => panic!("{other}"),
    }
}

#[test]
fn replays_the_golden() {
    let mut control = std::collections::HashMap::new();
    let mut form = std::collections::HashMap::new();
    for line in GOLDEN.lines() {
        let words: Vec<&str> = line.splitn(4, ' ').collect();
        let id: u32 = words[1].parse().unwrap();
        if words[0] == "control" {
            control.insert((id, words[2].parse::<u32>().unwrap()), expected(words[3]));
        } else {
            form.insert(id, expected(line.splitn(3, ' ').nth(2).unwrap()));
        }
    }
    let mut made = 0;
    for id in 0..1024u32 {
        for context in 0..4u32 {
            let want = control.get(&(id, context)).copied().unwrap_or(0);
            let got = control_event_for(id as c_ulong, context & 1 != 0, context & 2 != 0);
            assert_eq!(got, want, "control {id} {context}");
            made += (got != 0) as u32;
        }
        assert_eq!(form_event_for(id as c_ulong), form.get(&id).copied().unwrap_or(0), "form {id}");
    }
    assert!(made > 800, "{made}");
}

#[test]
fn the_first_table_that_knows_an_id_answers() {
    // the save and load forms' Cancel is theirs whatever the flags; their other ids depend on them
    for context in 0..4 {
        assert_eq!(control_event_for(15, context & 1 != 0, context & 2 != 0), EE_SCREENS_CPP_CANCEL_LOAD);
    }
    assert_eq!(control_event_for(0xffff_ffff, true, true), 0);
    assert_eq!(form_event_for(2), 0);
}
