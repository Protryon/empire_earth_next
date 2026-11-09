//! The tests' [`Host`]: a node pool with reference counts, samples, and a small mixer whose channels hold a sample
//! and play or not. The C++ golden driver (see `golden.rs`) has the same fake mixer, call for call, so that the
//! log of mixer calls and the state dumps can be compared with the real C++ manager's.

use std::collections::BTreeMap;
use std::ffi::c_void;

use crate::manager::{FileSpec, Host, Node, NodeFields, Sample, CHANNEL_IDLE, CHANNEL_PLAYING, NONE, OK};

/// A pooled node: its fields, reference count and whether it is back in the pool.
#[derive(Clone, Copy, Debug)]
pub struct FakeNode {
    pub fields: NodeFields,
    pub refs: u32,
    pub free: bool,
}

#[derive(Clone, Copy, Debug)]
pub struct FakeSample {
    pub looping: bool,
    pub volume: f32,
    pub pan: f32,
    pub pitch: f32,
    pub mixer_channel: u32,
    pub length: u32,
    pub rate: u32,
    pub offset: Option<u32>,
    pub deleted: bool,
}

#[derive(Clone, Copy, Debug, Default)]
pub struct FakeChannel {
    pub sample: Option<usize>,
    pub playing: bool,
}

/// Handles are 1-based indices (nodes, samples, files) cast to pointers.
#[derive(Debug)]
pub struct Fake {
    pub nodes: Vec<FakeNode>,
    pub samples: Vec<FakeSample>,
    pub files: Vec<(String, u32, u32)>,
    pub channels: Vec<FakeChannel>,
    pub fail_channels: Vec<u32>,
    pub remaining: BTreeMap<usize, i32>,
    pub state_override: BTreeMap<u32, i32>,
    pub log: Vec<String>,
}

impl Default for Fake {
    fn default() -> Self {
        Fake {
            nodes: Vec::new(),
            samples: Vec::new(),
            files: Vec::new(),
            channels: vec![FakeChannel::default(); 16],
            fail_channels: Vec::new(),
            remaining: BTreeMap::new(),
            state_override: BTreeMap::new(),
            log: Vec::new(),
        }
    }
}

fn idx(p: *mut c_void) -> usize {
    p as usize - 1
}

fn handle(i: usize) -> *mut c_void {
    (i + 1) as *mut c_void
}

/// A u32 as the C++ dump prints it (`(long)`).
pub fn signed(v: u32) -> i32 {
    v as i32
}

pub fn hex(v: f32) -> String {
    format!("{:08x}", v.to_bits())
}

impl Fake {
    pub fn add_file(&mut self, name: &str, length: u32, rate: u32) {
        self.files.push((name.to_string(), length, rate));
    }

    /// A label for the log: `S<n>`, n from 1 in creation order.
    pub fn label(s: Sample) -> String {
        format!("S{}", idx(s.0) + 1)
    }

    pub fn live_nodes(&self) -> usize {
        self.nodes.iter().filter(|n| !n.free).count()
    }

    pub fn live_samples(&self) -> usize {
        self.samples.iter().filter(|s| !s.deleted).count()
    }

    pub fn set_state(&mut self, channel: u32, state: i32) {
        self.state_override.insert(channel, state);
    }

    /// The sample on the mixer channel ends.
    pub fn end_channel(&mut self, channel: u32) {
        if let Some(c) = self.channels.get_mut(channel as usize) {
            c.playing = false;
        }
    }

    pub fn offset_of(&self, s: Sample) -> Option<u32> {
        self.samples[idx(s.0)].offset
    }

    pub fn refs(&self, n: Node) -> u32 {
        self.nodes[idx(n.0)].refs
    }

    fn new_sample(&mut self, s: FakeSample) -> Sample {
        self.samples.push(s);
        Sample(handle(self.samples.len() - 1))
    }

    fn s(&self, s: Sample) -> &FakeSample {
        let f = &self.samples[idx(s.0)];
        assert!(!f.deleted, "use of deleted sample {}", Self::label(s));
        f
    }

    fn sm(&mut self, s: Sample) -> &mut FakeSample {
        let f = &mut self.samples[idx(s.0)];
        assert!(!f.deleted, "use of deleted sample {}", Self::label(s));
        f
    }

    fn detach(&mut self, channel: u32) {
        if let Some(c) = self.channels.get_mut(channel as usize) {
            if let Some(s) = c.sample.take() {
                self.samples[s].mixer_channel = NONE;
            }
            c.playing = false;
        }
    }
}

impl Host for Fake {
    fn node_new(&mut self) -> Node {
        let fields = NodeFields {
            category: NONE,
            sample_id: NONE,
            max_instances: 0,
            instance_id: NONE,
            instance_index: NONE,
            channel: NONE,
            sample: std::ptr::null_mut(),
            volume: 0.0,
            pan: 0.0,
            pitch: 0.0,
        };
        self.nodes.push(FakeNode { fields, refs: 1, free: false });
        Node(handle(self.nodes.len() - 1))
    }

    fn node_reference(&mut self, n: Node) {
        self.nodes[idx(n.0)].refs += 1;
    }

    fn node_dereference(&mut self, n: Node) {
        let node = &mut self.nodes[idx(n.0)];
        assert!(!node.free, "dereference of a pooled node");
        if node.refs > 1 {
            node.refs -= 1;
        } else {
            node.refs = 0;
            node.free = true;
        }
    }

    fn node(&self, n: Node) -> NodeFields {
        let node = &self.nodes[idx(n.0)];
        assert!(!node.free, "read of a pooled node");
        node.fields
    }

    fn set_node(&mut self, n: Node, f: &NodeFields) {
        let node = &mut self.nodes[idx(n.0)];
        assert!(!node.free, "write of a pooled node");
        node.fields = *f;
    }

    fn sample_is_looping(&self, s: Sample) -> bool {
        self.s(s).looping
    }

    fn sample_set_looping(&mut self, s: Sample, looping: bool) {
        self.log.push(format!("loop:{}:{}", Self::label(s), looping as u8));
        self.sm(s).looping = looping;
    }

    fn sample_volume(&self, s: Sample) -> f32 {
        self.s(s).volume
    }

    fn sample_pan(&self, s: Sample) -> f32 {
        self.s(s).pan
    }

    fn sample_pitch(&self, s: Sample) -> f32 {
        self.s(s).pitch
    }

    fn sample_set_volume(&mut self, s: Sample, v: f32) -> i32 {
        self.log.push(format!("vol:{}:{}", Self::label(s), hex(v)));
        self.sm(s).volume = v;
        OK
    }

    fn sample_set_pan(&mut self, s: Sample, v: f32) -> i32 {
        self.log.push(format!("pan:{}:{}", Self::label(s), hex(v)));
        self.sm(s).pan = v;
        OK
    }

    fn sample_set_pitch(&mut self, s: Sample, v: f32) -> i32 {
        self.log.push(format!("pitch:{}:{}", Self::label(s), hex(v)));
        self.sm(s).pitch = v;
        OK
    }

    fn sample_mixer_channel(&self, s: Sample) -> u32 {
        self.s(s).mixer_channel
    }

    fn sample_set_mixer_channel(&mut self, s: Sample, channel: u32) {
        self.sm(s).mixer_channel = channel;
    }

    fn sample_length(&self, s: Sample) -> u32 {
        self.s(s).length
    }

    fn sample_rate(&self, s: Sample) -> u32 {
        self.s(s).rate
    }

    fn sample_load(&mut self, file: FileSpec) -> Sample {
        let (_, length, rate) = self.files[idx(file.0)].clone();
        let s = self.new_sample(FakeSample {
            looping: false,
            volume: 1.0,
            pan: 0.0,
            pitch: 1.0,
            mixer_channel: NONE,
            length,
            rate,
            offset: None,
            deleted: false,
        });
        self.log.push(format!("loadwave:{}", Self::label(s)));
        s
    }

    fn sample_duplicate(&mut self, s: Sample) -> Option<Sample> {
        let mut copy = *self.s(s);
        copy.mixer_channel = NONE;
        copy.offset = None;
        let d = self.new_sample(copy);
        self.log.push(format!("dup:{}>{}", Self::label(s), Self::label(d)));
        Some(d)
    }

    fn sample_delete(&mut self, s: Option<Sample>) {
        let Some(s) = s else { return };
        self.log.push(format!("del:{}", Self::label(s)));
        // as the real mixer (ee_mixer_delete_wave_data): a sample on a channel leaves it
        let channel = self.s(s).mixer_channel;
        if channel != NONE {
            self.detach(channel);
        }
        self.sm(s).deleted = true;
    }

    fn find_file(&mut self, name: &[u8]) -> Option<FileSpec> {
        let name = String::from_utf8_lossy(name).into_owned();
        match self.files.iter().position(|f| f.0 == name) {
            Some(i) => {
                self.log.push(format!("find:{name}"));
                Some(FileSpec(handle(i)))
            }
            None => {
                self.log.push(format!("find:{name}:no"));
                None
            }
        }
    }

    fn file_delete(&mut self, _f: FileSpec) {
        self.log.push("fdel".to_string());
    }

    fn stop_channel(&mut self, channel: u32) {
        self.log.push(format!("stop:{}", signed(channel)));
        self.detach(channel);
    }

    fn set_sample_offset(&mut self, s: Sample, offset: u32) {
        self.log.push(format!("off:{}:{}", Self::label(s), offset));
        self.sm(s).offset = Some(offset);
    }

    fn channel_state(&mut self, channel: u32) -> i32 {
        let state = match self.state_override.get(&channel) {
            Some(&v) => v,
            None => match self.channels.get(channel as usize) {
                Some(c) if c.sample.is_some() && c.playing => CHANNEL_PLAYING,
                _ => CHANNEL_IDLE,
            },
        };
        self.log.push(format!("busy:{}={}", signed(channel), state));
        state
    }

    fn playback_time(&mut self, s: Sample) -> (i32, i32) {
        self.log.push(format!("pbt:{}", Self::label(s)));
        let f = self.s(s);
        let total = if f.rate == 0 { 0 } else { (u64::from(f.length) * 1000 / u64::from(f.rate)) as i32 };
        (total, *self.remaining.get(&idx(s.0)).unwrap_or(&total))
    }

    fn set_channel_sample(&mut self, channel: u32, s: Sample) -> i32 {
        let result = if self.fail_channels.contains(&channel) || channel as usize >= self.channels.len() {
            9
        } else if self.channels[channel as usize].playing && self.channels[channel as usize].sample.is_some() {
            5
        } else if self.s(s).mixer_channel == channel {
            OK
        } else if self.s(s).mixer_channel != NONE {
            11
        } else {
            self.detach(channel);
            self.channels[channel as usize].sample = Some(idx(s.0));
            self.sm(s).mixer_channel = channel;
            OK
        };
        self.log.push(format!("setch:{}:{}={}", signed(channel), Self::label(s), result));
        result
    }

    fn play_channel(&mut self, channel: u32) {
        self.log.push(format!("play:{}", signed(channel)));
        if let Some(c) = self.channels.get_mut(channel as usize) {
            if c.sample.is_some() {
                c.playing = true;
            }
        }
    }

    fn stop_all_channels(&mut self) {
        self.log.push("stopall".to_string());
        for c in 0..self.channels.len() as u32 {
            self.detach(c);
        }
    }

    fn destroy_channels(&mut self) {
        self.log.push("destroych".to_string());
        for c in 0..self.channels.len() as u32 {
            self.detach(c);
        }
        self.channels.clear();
    }

    fn create_channels(&mut self, count: u32) -> i32 {
        self.log.push(format!("createch:{count}"));
        self.channels = vec![FakeChannel::default(); count as usize];
        OK
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_fake_mixer_binds_and_detaches_samples() {
        let mut f = Fake::default();
        f.add_file("a", 10, 1000);
        let file = f.find_file(b"a").unwrap();
        let s = f.sample_load(file);
        assert_eq!(f.set_channel_sample(2, s), OK);
        assert_eq!(f.sample_mixer_channel(s), 2);
        assert_eq!(f.channel_state(2), CHANNEL_IDLE);
        f.play_channel(2);
        assert_eq!(f.channel_state(2), CHANNEL_PLAYING);
        assert_eq!(f.set_channel_sample(2, s), 5);
        f.stop_channel(2);
        assert_eq!(f.sample_mixer_channel(s), NONE);
        assert_eq!(f.playback_time(s), (10, 10));
        let n = f.node_new();
        f.node_reference(n);
        f.node_dereference(n);
        f.node_dereference(n);
        assert_eq!(f.live_nodes(), 0);
    }
}
