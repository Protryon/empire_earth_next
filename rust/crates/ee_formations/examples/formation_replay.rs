//! TEST ONLY: replays the formation records of simulation captures (`tools/sim_capture.py replay`) through this
//! crate and compares every slot, floats by bits, with the slots the capture recorded. Exit 1 on any difference.

use ee_formations::capture::{constructed_as_preset, decode, records, replay, slots_eq};
use std::collections::BTreeMap;

fn main() {
    let mut bad = 0u64;
    for path in std::env::args().skip(1) {
        let data = match std::fs::read(&path) {
            Ok(d) => d,
            Err(e) => {
                eprintln!("{path}: {e}");
                bad += 1;
                continue;
            }
        };
        let records = match records(&data) {
            Ok(r) => r,
            Err(e) => {
                eprintln!("{path}: {e}");
                bad += 1;
                continue;
            }
        };
        // per formation type: calls, equal calls, slots, calls whose members are not the preset's
        let mut counts: BTreeMap<i32, [u64; 4]> = BTreeMap::new();
        for record in &records {
            let Some(call) = decode(record) else { continue };
            let call = match call {
                Ok(c) => c,
                Err(e) => {
                    eprintln!("{path}: {e}");
                    bad += 1;
                    continue;
                }
            };
            let entry = counts.entry(call.kind).or_default();
            entry[0] += 1;
            entry[2] += call.slots.len() as u64;
            if !constructed_as_preset(&call) {
                entry[3] += 1;
                if entry[3] <= 3 {
                    eprintln!("{path}: type {} members differ from the preset: {:?}", call.kind, call.shape);
                }
            }
            let got = replay(&call);
            if slots_eq(&got, &call.slots) {
                entry[1] += 1;
            } else if entry[0] - entry[1] <= 3 {
                eprintln!(
                    "{path}: type {} differs\n  input {:?}\n  C++  {:?}\n  Rust {:?}",
                    call.kind, call, call.slots, got
                );
            }
        }
        for (kind, [calls, equal, slots, not_preset]) in &counts {
            println!(
                "{path}: type {kind}: {equal} of {calls} calls equal ({slots} slots), {not_preset} not the preset"
            );
            bad += (calls - equal) + not_preset;
        }
        if counts.is_empty() {
            println!("{path}: no formation records");
        }
    }
    if bad != 0 {
        println!("formation replay: FAIL ({bad})");
        std::process::exit(1);
    }
    println!("formation replay: PASS");
}
