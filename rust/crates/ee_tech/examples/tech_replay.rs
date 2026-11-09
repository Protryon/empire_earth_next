//! TEST ONLY: replays the tech tree and effect records of simulation captures (`tools/sim_capture.py replay`)
//! through this crate (`ee_tech::capture::replay`) and compares every decision with the game's. Exit 1 on any
//! difference.

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
        match ee_tech::capture::replay(&data) {
            Ok(out) => {
                let counts: Vec<String> = out.checked.iter().map(|(t, n)| format!("{t} {n}")).collect();
                println!(
                    "{path}: tech {} checks, {} differ ({})",
                    out.checked.values().sum::<u64>(),
                    out.mismatch_count,
                    counts.join(", ")
                );
                for m in &out.mismatches {
                    eprintln!("  {m}");
                }
                bad += out.mismatch_count;
            }
            Err(e) => {
                eprintln!("{path}: {e}");
                bad += 1;
            }
        }
    }
    std::process::exit(if bad == 0 { 0 } else { 1 });
}
