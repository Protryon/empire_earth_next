//! The golden scenarios: `golden_sound.txt` holds, for each script, what the real C++ `SESoundManager` did (every
//! call into the fake mixer, the results, state dumps), printed by a throwaway driver compiled from the C++ source
//! before it was replaced, over the same fake mixer as `fake.rs`. Here the same script runs against the Rust manager
//! the way the C++ facade calls it, and must print the same.
//!
//! Script ops (one per space-separated group, see `run`): `new`, `file`, `load`, `play`, `playn`, `rel`, `ref`,
//! `stop`, `stopn`, `vol`/`pan`/`pitch`/`params` (by id) and their `...n` forms (by held node), `get`, `getn`,
//! `total`, `ends`, `rem`, `failset`, `scav`, `upd`, `oneshot`, `stopos`, `unload`, `maxch`, `pct`, `prio`,
//! `dump`, `end`.

use crate::fake::{hex, signed, Fake};
use crate::manager::{Host, Node, SoundManager, NONE};

struct Run {
    h: Fake,
    m: Option<SoundManager>,
    slots: [Option<Node>; 16],
    out: Vec<String>,
}

fn arg<T: std::str::FromStr>(t: &[&str], i: usize) -> T
where
    T::Err: std::fmt::Debug,
{
    t[i].parse().unwrap_or_else(|e| panic!("{:?}: arg {i}: {e:?}", t))
}

fn u(t: &[&str], i: usize) -> u32 {
    arg::<i64>(t, i) as u32
}

impl Run {
    fn node_text(&self, n: Node) -> String {
        let d = self.h.node(n);
        let s = d.sample().map_or("-".to_string(), Fake::label);
        format!(
            "{}.{}.{}.{}.{}.{}.{}.{}.{}.{}.{}",
            signed(d.instance_id),
            signed(d.sample_id),
            signed(d.instance_index),
            signed(d.channel),
            signed(d.category),
            d.max_instances,
            s,
            hex(d.volume),
            hex(d.pan),
            hex(d.pitch),
            self.h.refs(n)
        )
    }

    fn dump(&self) -> String {
        let m = self.m.as_ref().unwrap();
        let ch: Vec<String> = m.channels.iter().map(|c| c.map_or("-".to_string(), |n| self.node_text(n))).collect();
        let pq: Vec<String> = m.paused.iter().map(|&n| self.node_text(n)).collect();
        let pm: Vec<String> = m.paused_instances.keys().map(|k| signed(*k).to_string()).collect();
        let cat: Vec<String> = m
            .categories
            .iter()
            .map(|c| {
                c.map_or("-".to_string(), |c| {
                    format!("{}/{}/{}", c.max_instances, c.number_of_instances, signed(c.priority))
                })
            })
            .collect();
        let snd: Vec<String> = m
            .sounds
            .iter()
            .map(|s| {
                let insts = s.instances.as_ref().map_or("-".to_string(), |i| {
                    format!("[{}]", i.iter().map(|&n| self.node_text(n)).collect::<Vec<_>>().join(","))
                });
                format!("{}:{}:{}", String::from_utf8_lossy(&s.name), s.load_count, insts)
            })
            .collect();
        let os: Vec<String> = m
            .one_shots
            .iter()
            .map(|o| format!("{}/{}/{}", Fake::label(o.sample), signed(o.sample_id), signed(o.instance_id)))
            .collect();
        format!(
            "ch=[{}] pq=[{}] pm=[{}] cat=[{}] snd=[{}] os=[{}] next={} avail={} live={}",
            ch.join(","),
            pq.join(","),
            pm.join(","),
            cat.join(","),
            snd.join(","),
            os.join(","),
            m.next_instance_id,
            m.available_channels,
            self.h.live_nodes()
        )
    }

    fn op(&mut self, t: &[&str]) -> String {
        let h = &mut self.h;
        let r: String = match t[0] {
            "new" => {
                let mut n = u(t, 2);
                if n == 0 {
                    h.log.push("chcount".to_string());
                    n = 8;
                }
                self.m = Some(SoundManager::new(u(t, 1), n));
                String::new()
            }
            "file" => {
                h.add_file(t[1], u(t, 2), u(t, 3));
                String::new()
            }
            "ends" => {
                h.end_channel(u(t, 1));
                String::new()
            }
            "rem" => {
                h.remaining.insert(arg::<usize>(t, 1) - 1, arg(t, 2));
                String::new()
            }
            "failset" => {
                h.fail_channels.push(u(t, 1));
                String::new()
            }
            "rel" => {
                if let Some(n) = self.slots[arg::<usize>(t, 1)].take() {
                    h.node_dereference(n);
                }
                String::new()
            }
            "dump" => return self.dump(),
            "end" => {
                let mut m = self.m.take().unwrap();
                m.destroy(h);
                format!("live={} samples={}", h.live_nodes(), h.live_samples())
            }
            _ => {
                let m = self.m.as_mut().unwrap();
                match t[0] {
                    "load" => signed(m.load(h, t[1].as_bytes(), u(t, 2), u(t, 3))).to_string(),
                    "play" => {
                        let n = m.play(h, u(t, 1), arg(t, 2), arg(t, 3), arg(t, 4), arg::<u8>(t, 5) != 0);
                        let id = n.map_or(NONE, |n| {
                            let id = h.node(n).instance_id;
                            h.node_dereference(n);
                            id
                        });
                        signed(id).to_string()
                    }
                    "playn" => {
                        let n = m.play(h, u(t, 2), arg(t, 3), arg(t, 4), arg(t, 5), arg::<u8>(t, 6) != 0);
                        self.slots[arg::<usize>(t, 1)] = n;
                        signed(n.map_or(NONE, |n| h.node(n).instance_id)).to_string()
                    }
                    "ref" => {
                        let n = m.find_instance(h, u(t, 2)).inspect(|&n| h.node_reference(n));
                        self.slots[arg::<usize>(t, 1)] = n;
                        signed(n.map_or(NONE, |n| h.node(n).instance_id)).to_string()
                    }
                    "stop" => m.stop(h, u(t, 1), arg::<u8>(t, 2) != 0).to_string(),
                    "stopn" => {
                        // Stop(node): the node's id, then Stop(id)
                        let id = self.slots[arg::<usize>(t, 1)].map_or(NONE, |n| h.node(n).instance_id);
                        m.stop(h, id, arg::<u8>(t, 2) != 0).to_string()
                    }
                    "vol" => m.set_volume(h, u(t, 1), arg(t, 2)).to_string(),
                    "pan" => m.set_pan(h, u(t, 1), arg(t, 2)).to_string(),
                    "pitch" => m.set_pitch(h, u(t, 1), arg(t, 2)).to_string(),
                    "params" => m.set_parameters(h, u(t, 1), arg(t, 2), arg(t, 3), arg(t, 4)).to_string(),
                    "voln" | "pann" | "pitchn" | "paramsn" => {
                        // the C++ node setters do nothing for a null node
                        let Some(n) = self.slots[arg::<usize>(t, 1)] else { return format!("{}=0", t[0]) };
                        match t[0] {
                            "voln" => SoundManager::node_set_volume(h, n, arg(t, 2)),
                            "pann" => SoundManager::node_set_pan(h, n, arg(t, 2)),
                            "pitchn" => SoundManager::node_set_pitch(h, n, arg(t, 2)),
                            _ => SoundManager::node_set_parameters(h, n, arg(t, 2), arg(t, 3), arg(t, 4)),
                        }
                        .to_string()
                    }
                    "get" => {
                        let id = u(t, 1);
                        let a = m.is_looping(h, id) as u8;
                        let b = m.is_playing(h, id) as u8;
                        let (pan, pitch, vol) = (m.pan(h, id), m.pitch(h, id), m.volume(h, id));
                        let (sid, rate) = (m.sample_id(h, id), m.sample_rate(h, id));
                        let left = m.time_left_of(h, id);
                        format!("{a},{b},{},{},{},{},{rate},{left}", hex(pan), hex(pitch), hex(vol), signed(sid))
                    }
                    "getn" => match self.slots[arg::<usize>(t, 1)] {
                        Some(n) => format!(
                            "{},{},{},{},{},{}",
                            SoundManager::node_is_looping(h, n) as u8,
                            SoundManager::node_is_playing(h, n) as u8,
                            hex(SoundManager::node_pan(h, n)),
                            hex(SoundManager::node_pitch(h, n)),
                            signed(h.node(n).sample_id),
                            signed(h.node(n).instance_id)
                        ),
                        None => format!("0,0,{},{},-1,-1", hex(0.0), hex(0.0)),
                    },
                    "total" => m.total_play_time_of(h, u(t, 1)).to_string(),
                    "scav" => {
                        let muted = arg::<u8>(t, 1) != 0;
                        m.update_one_shots(h);
                        m.scavenge(h, muted);
                        m.update_one_shots(h);
                        String::new()
                    }
                    "upd" => {
                        m.update_one_shots(h);
                        String::new()
                    }
                    "oneshot" => m.register_one_shot(h, u(t, 1), u(t, 2)).to_string(),
                    "stopos" => m.stop_all_one_shots(h).to_string(),
                    "unload" => m.unload(h, u(t, 1)).to_string(),
                    "maxch" => m.set_max_channels(h, u(t, 1)).to_string(),
                    "pct" => m.set_category_percentage(u(t, 1), arg(t, 2)).to_string(),
                    "prio" => m.set_category_priority(u(t, 1), u(t, 2)).to_string(),
                    other => panic!("unknown op {other}"),
                }
            }
        };
        let calls = std::mem::take(&mut self.h.log);
        let mut s = t[0].to_string();
        if !calls.is_empty() {
            s += &format!("{{{}}}", calls.join(" "));
        }
        if !r.is_empty() {
            s += "=";
            s += &r;
        }
        s
    }
}

/// Runs a script (ops separated by `;`) and returns its output (one entry per op, separated by ` | `).
pub fn run(script: &str) -> String {
    let mut r = Run { h: Fake::default(), m: None, slots: [None; 16], out: Vec::new() };
    for op in script.split(';').map(str::trim).filter(|s| !s.is_empty()) {
        let t: Vec<&str> = op.split(' ').collect();
        let s = r.op(&t);
        r.out.push(s);
    }
    r.out.join(" | ")
}

#[test]
fn scenarios_equal_the_cpp_sound_manager() {
    let golden = include_str!("golden_sound.txt");
    let mut n = 0;
    for l in golden.lines().filter(|l| l.starts_with("sm\t")) {
        let f: Vec<&str> = l.split('\t').collect();
        let got = run(f[2]);
        if got != f[3] {
            let g: Vec<&str> = got.split(" | ").collect();
            let w: Vec<&str> = f[3].split(" | ").collect();
            let at = g.iter().zip(&w).position(|(a, b)| a != b).unwrap_or(g.len().min(w.len()));
            panic!(
                "scenario {}: first difference at op {at}\n rust: {}\n c++:  {}",
                f[1],
                g.get(at).unwrap_or(&"<end>"),
                w.get(at).unwrap_or(&"<end>")
            );
        }
        n += 1;
    }
    assert!(n >= 10, "only {n} scenarios");
}
