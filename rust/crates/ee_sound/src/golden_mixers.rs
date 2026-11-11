//! The mixers' golden scenarios: `golden_mixers.txt` is what the real C++ `SEMixer.cpp` and `Sound Engine.cpp`
//! (as they were before the move) printed over a fake plug-in table of up to four sound modules (each with its own
//! validity and SPIM results) and a fake conductor: the start, `UseMixer`, `GetMixer`, deleting a mixer and the
//! shutdown. Here the same ops run against [`mixers`] with a fake host printing what the fake plug-ins and the
//! C++ constructor and destructor print, and the whole text must come out the same.

use std::ffi::c_void;
use std::fmt::Write;

use crate::mixers::{self, EngineHost, Host, Mixer, Registry};

#[derive(Default)]
struct Fake {
    out: String,
    reg: Registry,
    active: Option<Mixer>,
    modules: usize,
    next: usize,
    current: usize,
    valid: [bool; 4],
    act: [i32; 4],
    deact: [i32; 4],
    cs: [i32; 4],
}

/// A mixer handle is its module's index plus one (mixers of one module never coexist here).
fn label(m: Mixer) -> usize {
    m.0 as usize - 1
}

impl Host for Fake {
    fn active(&self) -> Option<Mixer> {
        self.active
    }
    fn set_active(&mut self, m: Option<Mixer>) {
        self.active = m;
    }
    fn stop_music(&mut self, m: Mixer) -> i32 {
        let k = label(m);
        writeln!(self.out, "conductor_state {k} 0 -> {}", self.cs[k]).unwrap();
        self.cs[k]
    }
    fn activate(&mut self, m: Mixer) -> i32 {
        let k = label(m);
        writeln!(self.out, "activate {k} -> {}", self.act[k]).unwrap();
        self.act[k]
    }
    fn deactivate(&mut self, m: Mixer) -> i32 {
        let k = label(m);
        writeln!(self.out, "deactivate {k} -> {}", self.deact[k]).unwrap();
        self.deact[k]
    }
}

impl Fake {
    fn module(&mut self, first: bool) -> i32 {
        if first {
            self.next = 0;
        }
        if self.next >= self.modules {
            self.out.push_str("module -> none\n");
            return mixers::NO_MORE_FILES;
        }
        self.current = self.next;
        self.next += 1;
        writeln!(self.out, "module -> {}", self.current).unwrap();
        mixers::OK
    }
}

impl EngineHost for Fake {
    fn first_module(&mut self) -> i32 {
        self.module(true)
    }
    fn next_module(&mut self) -> i32 {
        self.module(false)
    }
    fn create_mixer(&mut self) -> Mixer {
        // the constructor: registered, then the plug-in initialised
        let m = Mixer((self.current + 1) as *mut c_void);
        self.reg.register(m);
        writeln!(self.out, "initialize {}", self.current).unwrap();
        m
    }
    fn is_valid(&mut self, m: Mixer) -> bool {
        let k = label(m);
        writeln!(self.out, "is_valid {k} -> {}", self.valid[k] as i32).unwrap();
        self.valid[k]
    }
    fn delete_mixer(&mut self, m: Mixer) {
        // the destructor: the decisions, out of the registry, the module unloaded
        mixers::on_destroy(self, m);
        self.reg.unregister(m);
        writeln!(self.out, "unload {}", label(m)).unwrap();
    }
    fn count(&mut self) -> u32 {
        self.reg.count()
    }
    fn first(&mut self) -> Option<Mixer> {
        self.reg.get(0).ok()
    }
}

fn dump(f: &mut Fake) {
    let list: String = f.reg.mixers.iter().map(|&m| format!(" {}", label(m))).collect();
    let active = f.active.map_or(-1, |m| label(m) as i64);
    writeln!(f.out, "= count {} [{list} ] active {active}", f.reg.count()).unwrap();
}

fn ints<const N: usize>(t: &[&str]) -> [i32; N] {
    let mut a = [0; N];
    for (i, w) in t.iter().take(N).enumerate() {
        a[i] = w.parse().unwrap();
    }
    a
}

#[test]
fn mixers_equal_the_cpp() {
    let text = include_str!("golden_mixers.txt");
    let golden: Vec<&str> = text.lines().collect();
    let mut f = Fake::default();
    let mut scenarios = 0;
    for (i, line) in golden.iter().enumerate() {
        let Some(op) = line.strip_prefix("> ") else {
            continue;
        };
        writeln!(f.out, "{line}").unwrap();
        let t: Vec<&str> = op.split(' ').collect();
        match t[0] {
            "scenario" => {
                scenarios += 1;
                f.modules = t[3].parse().unwrap();
                for k in 0..4 {
                    f.valid[k] = t[5 + k] == "1";
                }
                f.out.push_str("register_shutdown\n");
                let r = mixers::initialize_mixers(&mut f);
                writeln!(f.out, "< init {r}").unwrap();
                dump(&mut f);
            }
            "results" => {
                f.act = ints(&t[2..6]);
                f.deact = ints(&t[7..11]);
                f.cs = ints(&t[12..16]);
                // a delete of a missing index printed nothing but the state
                if golden[i + 1].starts_with("= count") {
                    dump(&mut f);
                }
            }
            "use" => {
                let m = if t[1] == "-" || t[1] == "-1" { None } else { f.reg.get(t[1].parse().unwrap()).ok() };
                let r = mixers::use_mixer(&mut f, m);
                writeln!(f.out, "< {r}").unwrap();
                dump(&mut f);
            }
            "get" => {
                let r = f.reg.get(t[1].parse().unwrap());
                match r {
                    Ok(m) => writeln!(f.out, "< 0 {}", label(m)).unwrap(),
                    Err(e) => writeln!(f.out, "< {e} -1").unwrap(),
                }
                dump(&mut f);
            }
            "delete" => {
                let m = f.reg.get(t[1].parse().unwrap()).unwrap();
                f.delete_mixer(m);
                dump(&mut f);
            }
            "shutdown" => {
                mixers::delete_mixers(&mut f);
                dump(&mut f);
                f.active = None;
            }
            k => panic!("op {k}"),
        }
    }
    assert_eq!(scenarios, 60);
    let got: Vec<&str> = f.out.lines().collect();
    for (i, (w, g)) in golden.iter().zip(&got).enumerate() {
        assert_eq!(w, g, "line {}", i + 1);
    }
    assert_eq!(golden.len(), got.len());
}
