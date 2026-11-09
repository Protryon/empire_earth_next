//! TEST ONLY: replays the combat records of simulation captures (`tools/sim_capture.py replay`) through this crate
//! and compares every output, byte for byte, with the one the capture recorded. Exit 1 on any difference.

use ee_combat::capture::{replay_all, Counts, TAGS};

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
        let mut counts = Counts::new();
        if let Err(e) = replay_all(&data, &mut counts, &mut |line| eprintln!("{path}: {line}")) {
            eprintln!("{path}: {e}");
            bad += 1;
        }
        for (tag, [records, equal, errors]) in &counts {
            let name = TAGS.iter().find(|(t, _)| t.as_slice() == tag.as_bytes()).map_or("?", |(_, n)| *n);
            println!("{path}: {tag} ({name}): {equal} of {records} equal, {errors} malformed");
            bad += records - equal;
        }
    }
    if bad != 0 {
        println!("combat replay: FAIL ({bad})");
        std::process::exit(1);
    }
    println!("combat replay: PASS");
}
