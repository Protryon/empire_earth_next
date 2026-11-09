//! The Rust side of the data gate (tools/eedata/cppgate.py): the same command line as the C++ `eedata_gate` tool.
//!   ee_data_gate <text dir> <expected .dat dir>
//!   ee_data_gate --batch <dir> <text dir>
//!   ee_data_gate --dump-text <text dir> <out.json>
//!   ee_data_gate --dump-dat <db dir> <out.json>
//!   ee_data_gate --rules-batch <dir> <text dir>
//!   ee_data_gate --floats <in.txt> <out.txt>
//! Exit code 0 on success. Logic lives in `ee_data::gate`, which the unit tests call.

use std::process::ExitCode;

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    match ee_data::gate::run(&args) {
        0 => ExitCode::SUCCESS,
        n => ExitCode::from(n as u8),
    }
}
