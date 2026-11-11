//! The conductor's golden scenarios: `golden_conductor.txt` is what the real C++ `SEConductor` (SEConductor.cpp and
//! SEScore.cpp as they were before the move) printed over fake dmusic functions and a fake mixer, for fixed and
//! seeded random op scripts. Here every `> op` line of it runs against [`Conductor`] with a fake host that prints
//! the same dmusic calls (the C++ trampolines' sequences), and the whole text must come out the same.

use std::collections::BTreeSet;
use std::ffi::c_void;
use std::fmt::Write;

use crate::conductor::{Conductor, ConductorFields, Embellishment, Host, Score, Timing};

#[derive(Default)]
struct Fake {
    out: String,
    segments: usize,
    fail_loader: bool,
    fail_resolver: bool,
    fail_perf: bool,
    fail_segment: bool,
    null_segment: bool,
    fail_play: bool,
    fail_transition: bool,
    rate: u32,
    mixer: bool,
    secondary: BTreeSet<usize>,
}

fn label(s: Option<Score>) -> usize {
    s.map_or(0, |s| s.0 as usize)
}

impl Fake {
    fn reset(&mut self) {
        self.fail_loader = false;
        self.fail_resolver = false;
        self.fail_perf = false;
        self.fail_segment = false;
        self.null_segment = false;
        self.fail_play = false;
        self.fail_transition = false;
        self.rate = 44100;
    }
    fn fail(b: bool) -> &'static str {
        if b {
            " FAIL"
        } else {
            ""
        }
    }
}

impl Host for Fake {
    fn output_rate(&mut self) -> u32 {
        if self.mixer {
            self.rate
        } else {
            0
        }
    }
    fn start(&mut self, rate: u32) -> bool {
        writeln!(self.out, "dm loader_create 1{}", Fake::fail(self.fail_loader)).unwrap();
        if self.fail_loader {
            return false;
        }
        writeln!(self.out, "dm add_resolver R{}", Fake::fail(self.fail_resolver)).unwrap();
        if !self.fail_resolver {
            writeln!(self.out, "dm perf_create {rate}{}", Fake::fail(self.fail_perf)).unwrap();
            if !self.fail_perf {
                return true;
            }
        }
        writeln!(self.out, "dm loader_release L").unwrap();
        false
    }
    fn release(&mut self) {
        self.out.push_str("dm perf_release P\ndm loader_release L\n");
    }
    fn set_music_source(&mut self, on: bool) {
        if on {
            assert!(self.mixer, "SetMusicSource through a null mixer");
            self.out.push_str("mixer source on\n");
        } else if self.mixer {
            self.out.push_str("mixer source off\n");
        }
    }
    fn load(&mut self, name: &[u8]) -> Option<Score> {
        let name = String::from_utf8_lossy(name);
        if self.fail_segment {
            writeln!(self.out, "dm get_segment {name} FAIL").unwrap();
            return None;
        }
        if self.null_segment {
            writeln!(self.out, "dm get_segment {name} NULL").unwrap();
            return None;
        }
        self.segments += 1;
        writeln!(self.out, "dm get_segment {name} S{}", self.segments).unwrap();
        Some(Score(self.segments as *mut c_void))
    }
    fn delete(&mut self, s: Score) {
        writeln!(self.out, "dm seg_release S{}", label(Some(s))).unwrap();
    }
    fn is_secondary(&self, s: Score) -> bool {
        self.secondary.contains(&label(Some(s)))
    }
    fn play(&mut self, s: Option<Score>, timing: Timing) -> bool {
        let seg = s.map_or("-".to_string(), |s| format!("S{}", label(Some(s))));
        writeln!(self.out, "dm play {seg} {}{}", timing as u32, Fake::fail(self.fail_play)).unwrap();
        !self.fail_play
    }
    fn play_transition(&mut self, s: Score, pattern: Embellishment) -> bool {
        writeln!(
            self.out,
            "dm transition S{} {} {}{}",
            label(Some(s)),
            pattern as u32,
            Timing::Measure as u32,
            Fake::fail(self.fail_transition)
        )
        .unwrap();
        !self.fail_transition
    }
    fn set_performance_volume(&mut self, amplitude: f32) {
        writeln!(self.out, "dm volume {:08x}", amplitude.to_bits()).unwrap();
    }
    fn set_groove_level(&mut self, level: i8) {
        writeln!(self.out, "dm groove {level}").unwrap();
    }
}

struct Run {
    h: Fake,
    c: Option<(Conductor, ConductorFields)>,
}

impl Run {
    fn fields(&mut self) {
        let f = self.c.as_ref().unwrap().1;
        writeln!(
            self.h.out,
            "= vol {:08x} state {} rev {} next {} playing {} int {} cgc {}",
            f.volume.to_bits(),
            f.state,
            f.reverb_state,
            f.next_score_id,
            f.playing_id,
            f.intensity,
            f.channel_group_count
        )
        .unwrap();
    }
    fn dump(&mut self) {
        self.fields();
        let c = &self.c.as_ref().unwrap().0;
        for (i, e) in c.scores.iter().enumerate() {
            writeln!(
                self.h.out,
                "= [{i}] {} S{} id {} n {}",
                String::from_utf8_lossy(&e.name),
                label(e.score),
                e.id,
                e.load_count
            )
            .unwrap();
        }
    }

    fn op(&mut self, line: &str) {
        writeln!(self.h.out, "> {line}").unwrap();
        let t: Vec<&str> = line.split(' ').collect();
        let int = |i: usize| -> i64 { t[i].parse().unwrap() };
        let h = &mut self.h;
        match t[0] {
            "new" => {
                self.c = Some((Conductor::new(), ConductorFields::new()));
                self.dump();
            }
            "end" => {
                let (mut c, mut f) = self.c.take().unwrap();
                if f.state == 1 {
                    c.uninitialize(h, &mut f);
                }
            }
            "rate" => h.rate = int(1) as u32,
            "mixer" => h.mixer = int(1) != 0,
            "fail" => {
                let v = int(2) != 0;
                match t[1] {
                    "loader" => h.fail_loader = v,
                    "resolver" => h.fail_resolver = v,
                    "perf" => h.fail_perf = v,
                    "segment" => h.fail_segment = v,
                    "null" => h.null_segment = v,
                    "play" => h.fail_play = v,
                    "transition" => h.fail_transition = v,
                    k => panic!("fail {k}"),
                }
            }
            _ => {
                let (c, f) = self.c.as_mut().unwrap();
                let id = || int(1) as i32;
                let (r, dump): (String, u8) = match t[0] {
                    "load" => {
                        let (r, out) = c.load_score(h, f, t[1].as_bytes(), false);
                        (format!("{r} id {}", out.unwrap_or(-99)), 2)
                    }
                    "unload" => (c.unload_score(h, f, id()).to_string(), 2),
                    "play" => (c.play(h, f, id()).to_string(), 1),
                    "queue" => (c.queue(h, f, id()).to_string(), 1),
                    "stop" => (Conductor::stop(h, f).to_string(), 1),
                    "trans" => (c.transition_to(h, f, id(), int(2) as u32).to_string(), 1),
                    "state" => (c.set_state(h, f, int(1) as u32).to_string(), 2),
                    "vol" => {
                        let v = f32::from_bits(u32::from_str_radix(t[1], 16).unwrap());
                        (Conductor::set_volume(h, f, v).to_string(), 1)
                    }
                    "int" => (Conductor::set_intensity(h, f, int(1) as u32, int(2) != 0).to_string(), 1),
                    "cgc" => (Conductor::set_channel_group_count(f, int(1) as u32).to_string(), 1),
                    "get" => (format!("S{}", label(c.score(id()))), 0),
                    "getid" => (c.score_id(c.score(id())).to_string(), 0),
                    "cur" => (Conductor::currently_playing(f).to_string(), 0),
                    "secondary" => {
                        let s = c.score(id());
                        if s.is_some() {
                            h.secondary.insert(label(s));
                        }
                        (format!("S{}", label(s)), 0)
                    }
                    k => panic!("op {k}"),
                };
                writeln!(h.out, "< {r}").unwrap();
                match dump {
                    1 => self.fields(),
                    2 => self.dump(),
                    _ => {}
                }
            }
        }
    }
}

#[test]
fn conductor_equals_the_cpp() {
    let golden = include_str!("golden_conductor.txt");
    let mut run = Run { h: Fake { mixer: true, ..Fake::default() }, c: None };
    run.h.reset();
    for line in golden.lines() {
        if let Some(op) = line.strip_prefix("> ") {
            if op == "new" {
                run.h.reset();
                run.h.mixer = true;
            }
            run.op(op);
        }
    }
    let ops = golden.lines().filter(|l| l.starts_with("> ")).count();
    assert!(ops > 2500, "{ops} ops");
    let want: Vec<&str> = golden.lines().collect();
    let got: Vec<&str> = run.h.out.lines().collect();
    for (i, (w, g)) in want.iter().zip(&got).enumerate() {
        assert_eq!(w, g, "line {}", i + 1);
    }
    assert_eq!(want.len(), got.len());
}
