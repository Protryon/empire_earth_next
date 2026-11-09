//! The game's startup load of a text db, timed and with its allocations counted (the R2 performance check):
//!   ee_data_startup <Data/db dir> [--repeat N] [--tables]
//! The same entry points as the game: one Content over the folder, the rules documents first (EELoadRules), then
//! every table with a text form compiled (EEDbFindFile), with the ids of dbobjects and dbtechtree (EEDbRowIds).
//! Prints the time and the allocation count of each phase (`--tables`: of each table); exit code 1 when anything fails to load.

use ee_data::content::{text_extension, Content};
use ee_data::generated::specs_gen::TABLES;
use ee_data::rules::Rules;
use std::alloc::{GlobalAlloc, Layout, System};
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicU64, Ordering::Relaxed};
use std::time::Instant;

/// The system allocator, counting.
struct Counting;

static ALLOCS: AtomicU64 = AtomicU64::new(0);
static REALLOCS: AtomicU64 = AtomicU64::new(0);
static BYTES: AtomicU64 = AtomicU64::new(0);
static LIVE: AtomicU64 = AtomicU64::new(0);
static PEAK: AtomicU64 = AtomicU64::new(0);
static FREES: AtomicU64 = AtomicU64::new(0);
static HEAP_NS: AtomicU64 = AtomicU64::new(0); // time inside the system allocator

// the heap's time by block size (classes up to 256 B, 4 KB, 64 KB, 1 MB, more) and operation (allocation, free)
const CLASSES: [u64; 4] = [256, 4096, 65536, 1 << 20];
static CLASS_NS: [[AtomicU64; 5]; 2] = [const { [const { AtomicU64::new(0) }; 5] }; 2];
static CLASS_N: [[AtomicU64; 5]; 2] = [const { [const { AtomicU64::new(0) }; 5] }; 2];

fn timed<T>(op: usize, size: usize, f: impl FnOnce() -> T) -> T {
    let t = Instant::now();
    let r = f();
    let ns = t.elapsed().as_nanos() as u64;
    HEAP_NS.fetch_add(ns, Relaxed);
    let c = CLASSES.iter().position(|&c| size as u64 <= c).unwrap_or(4);
    CLASS_NS[op][c].fetch_add(ns, Relaxed);
    CLASS_N[op][c].fetch_add(1, Relaxed);
    r
}

fn report_classes() {
    const NAMES: [&str; 5] = ["<=256 B", "<=4 KB", "<=64 KB", "<=1 MB", ">1 MB"];
    for (op, what) in ["alloc", "free"].iter().enumerate() {
        for c in 0..5 {
            let n = CLASS_N[op][c].load(Relaxed);
            if n > 0 {
                let ms = CLASS_NS[op][c].load(Relaxed) as f64 / 1e6;
                println!("  {what:<5} {:>8} {n:>8} blocks {ms:9.1} ms", NAMES[c]);
            }
        }
    }
}

fn grew(n: u64) {
    let live = LIVE.fetch_add(n, Relaxed) + n;
    PEAK.fetch_max(live, Relaxed);
}

unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, l: Layout) -> *mut u8 {
        ALLOCS.fetch_add(1, Relaxed);
        BYTES.fetch_add(l.size() as u64, Relaxed);
        grew(l.size() as u64);
        timed(0, l.size(), || System.alloc(l))
    }
    unsafe fn dealloc(&self, p: *mut u8, l: Layout) {
        LIVE.fetch_sub(l.size() as u64, Relaxed);
        FREES.fetch_add(1, Relaxed);
        timed(1, l.size(), || System.dealloc(p, l))
    }
    unsafe fn realloc(&self, p: *mut u8, l: Layout, n: usize) -> *mut u8 {
        REALLOCS.fetch_add(1, Relaxed);
        if n > l.size() {
            BYTES.fetch_add((n - l.size()) as u64, Relaxed);
            grew((n - l.size()) as u64);
        } else {
            LIVE.fetch_sub((l.size() - n) as u64, Relaxed);
        }
        timed(0, n, || System.realloc(p, l, n))
    }
}

#[global_allocator]
static GLOBAL: Counting = Counting;

struct Mark {
    at: Instant,
    allocs: u64,
    reallocs: u64,
    bytes: u64,
    frees: u64,
    heap_ns: u64,
}

fn mark() -> Mark {
    Mark {
        at: Instant::now(),
        allocs: ALLOCS.load(Relaxed),
        reallocs: REALLOCS.load(Relaxed),
        bytes: BYTES.load(Relaxed),
        frees: FREES.load(Relaxed),
        heap_ns: HEAP_NS.load(Relaxed),
    }
}

fn report(what: &str, m: &Mark) {
    let e = mark();
    println!(
        "{what:<14} {:9.1} ms {:>10} allocs {:>9} reallocs {:>10} frees {:>8.1} MB allocated {:9.1} ms in the heap",
        e.at.duration_since(m.at).as_secs_f64() * 1000.0,
        e.allocs - m.allocs,
        e.reallocs - m.reallocs,
        e.frees - m.frees,
        (e.bytes - m.bytes) as f64 / 1048576.0,
        (e.heap_ns - m.heap_ns) as f64 / 1e6
    );
}

fn load(dir: &Path, per_table: bool) -> bool {
    let d = dir.to_path_buf();
    let mut c = Content::new(Box::new(move |name: &str| {
        let ext = text_extension(name.as_bytes())?;
        std::fs::read(d.join(format!("{name}{ext}"))).ok()
    }));
    let all = mark();
    let m = mark();
    let mut rules = Rules::default();
    if let Err(e) = rules.load(&mut c) {
        println!("rules: {}", String::from_utf8_lossy(&e.text));
        return false;
    }
    report("rules", &m);
    let m = mark();
    let mut total = 0usize;
    for t in TABLES.iter().filter(|t| !t.text_only) {
        let ext = text_extension(t.name.as_bytes()).expect("a table has an extension");
        if !dir.join(format!("{}{ext}", t.name)).exists() {
            continue;
        }
        let one = mark();
        match c.compile(t) {
            Ok(bytes) => total += bytes.len(),
            Err(e) => {
                println!("{}: {}", t.name, String::from_utf8_lossy(&e.text));
                return false;
            }
        }
        if t.name == "dbobjects" || t.name == "dbtechtree" {
            if let Err(e) = c.ids(t.name).map(|i| i.ids.len()) {
                println!("{} ids: {}", t.name, String::from_utf8_lossy(&e.text));
                return false;
            }
        }
        if per_table {
            report(&format!("  {}", t.name), &one);
        }
    }
    report("tables", &m);
    report("startup", &all);
    if per_table {
        report_classes();
    }
    println!("{total} bytes of db files; peak {:.1} MB live", PEAK.load(Relaxed) as f64 / 1048576.0);
    true
}

fn main() -> std::process::ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let Some(dir) = args.first() else {
        eprintln!("usage: ee_data_startup <Data/db dir> [--repeat N] [--tables]");
        return std::process::ExitCode::from(2);
    };
    let repeat: usize = match args.iter().position(|a| a == "--repeat") {
        Some(i) => args.get(i + 1).and_then(|n| n.parse().ok()).unwrap_or(1),
        None => 1,
    };
    let per_table = args.iter().any(|a| a == "--tables");
    let dir = PathBuf::from(dir);
    for _ in 0..repeat {
        if !load(&dir, per_table) {
            return std::process::ExitCode::from(1);
        }
    }
    std::process::ExitCode::SUCCESS
}
