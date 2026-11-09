//! `SESoundManager`'s bookkeeping and decisions: the loaded sounds and their instance nodes, the categories (share of
//! the channels, priority, instance count), the channel table, the paused queue, the one-shot sounds and the random
//! start offsets of looping sounds.
//!
//! The objects stay C++'s: an instance node (`SESMInstanceNode`, pooled and reference counted, held by units, unit
//! classes and effects) and a sample (`SESample`, shared with the mixer) are opaque handles here, read and written
//! through [`Host`], and their reference counts change only through it, in the order the C++ changed them. The C++
//! facade holds the manager's lock around every call, as the C++ held it; nothing in [`Host`] reaches the manager.

use std::collections::BTreeMap;
use std::ffi::c_void;

use ee_random::{float_to_long, URandom};

use crate::msvc_sort;

/// The "none" value of an id, index or channel (`UIsNone32`).
pub(crate) const NONE: u32 = u32::MAX;
/// `mChannel` of a node in the paused queue.
pub(crate) const PAUSED: u32 = u32::MAX - 1;
/// `SPIMIsChannelBusyPlaying`: the channel plays.
pub(crate) const CHANNEL_PLAYING: i32 = 5;
/// `SPIMIsChannelBusyPlaying`: the channel is idle (its sample ended).
pub(crate) const CHANNEL_IDLE: i32 = 6;
/// `kErrorOK` / `kErrorFailed`.
pub(crate) const OK: i32 = 0;
pub(crate) const FAILED: i32 = 9;

/// An `SESMInstanceNode*` (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Node(pub *mut c_void);
/// An `SESample*` (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Sample(pub *mut c_void);
/// An `FSFileSpec*` from the sound path (never null).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct FileSpec(pub *mut c_void);

/// The data fields of an `SESMInstanceNode` (its reference count and free flag stay with the C++ object).
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct NodeFields {
    pub category: u32,
    pub sample_id: u32,
    pub max_instances: u32,
    pub instance_id: u32,
    pub instance_index: u32,
    pub channel: u32,
    /// `SESample*`, null for a paused instance without a sample.
    pub sample: *mut c_void,
    pub volume: f32,
    pub pan: f32,
    pub pitch: f32,
}

impl NodeFields {
    pub fn sample(&self) -> Option<Sample> {
        (!self.sample.is_null()).then_some(Sample(self.sample))
    }
}

/// What the manager asks of the C++ side: the nodes (pool, reference counts, fields), the samples, the mixer
/// plug-in and the sound path. No method calls back into the manager.
pub trait Host {
    /// `GetNewSoundInstance`: a cleared node from the pool with one reference.
    fn node_new(&mut self) -> Node;
    fn node_reference(&mut self, n: Node);
    /// `SESMInstanceNode::Dereference`: the last one returns the node to the pool.
    fn node_dereference(&mut self, n: Node);
    fn node(&self, n: Node) -> NodeFields;
    fn set_node(&mut self, n: Node, f: &NodeFields);

    fn sample_is_looping(&self, s: Sample) -> bool;
    /// Through the mixer (`SPIMSetSampleLooping`).
    fn sample_set_looping(&mut self, s: Sample, looping: bool);
    fn sample_volume(&self, s: Sample) -> f32;
    fn sample_pan(&self, s: Sample) -> f32;
    fn sample_pitch(&self, s: Sample) -> f32;
    /// Through the mixer; the mixer's result.
    fn sample_set_volume(&mut self, s: Sample, v: f32) -> i32;
    fn sample_set_pan(&mut self, s: Sample, v: f32) -> i32;
    fn sample_set_pitch(&mut self, s: Sample, v: f32) -> i32;
    fn sample_mixer_channel(&self, s: Sample) -> u32;
    /// The plain store `SESample::SetMixerChannel`.
    fn sample_set_mixer_channel(&mut self, s: Sample, channel: u32);
    fn sample_length(&self, s: Sample) -> u32;
    fn sample_rate(&self, s: Sample) -> u32;
    /// `new SESample` + `LoadWaveData(file)`.
    fn sample_load(&mut self, file: FileSpec) -> Sample;
    /// `SPIMDuplicateSample`; None when the mixer gave none.
    fn sample_duplicate(&mut self, s: Sample) -> Option<Sample>;
    /// `delete sample` (null: nothing).
    fn sample_delete(&mut self, s: Option<Sample>);

    /// `mPath->FindFile(name, spec) == kErrorOK`.
    fn find_file(&mut self, name: &[u8]) -> Option<FileSpec>;
    fn file_delete(&mut self, f: FileSpec);

    fn stop_channel(&mut self, channel: u32);
    fn set_sample_offset(&mut self, s: Sample, offset: u32);
    /// `SPIMIsChannelBusyPlaying`.
    fn channel_state(&mut self, channel: u32) -> i32;
    /// `SPIMGetSamplePlayBackTime`: (total, remaining) milliseconds.
    fn playback_time(&mut self, s: Sample) -> (i32, i32);
    fn set_channel_sample(&mut self, channel: u32, s: Sample) -> i32;
    fn play_channel(&mut self, channel: u32);
    fn stop_all_channels(&mut self);
    fn destroy_channels(&mut self);
    fn create_channels(&mut self, count: u32) -> i32;
}

/// `SESMCategory`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Category {
    pub percentage: f32,
    pub max_instances: u32,
    pub number_of_instances: u32,
    /// A larger value is a lower priority.
    pub priority: u32,
}

/// `SESMSound`: a loaded sample and its instance nodes (one per channel it may use).
#[derive(Clone, Debug, PartialEq)]
pub struct Sound {
    pub name: Vec<u8>,
    /// None: unloaded (the slot stays).
    pub instances: Option<Vec<Node>>,
    pub load_count: u32,
}

/// `SESMOneShotSound`.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct OneShot {
    pub sample: Sample,
    pub sample_id: u32,
    pub instance_id: u32,
}

/// The manager's state (`SESoundManager`'s members, without the C++ objects: path, mixer, thread, events, lock).
pub struct SoundManager {
    pub random: URandom,
    pub one_shots: Vec<OneShot>,
    pub channels: Vec<Option<Node>>,
    pub paused: Vec<Node>,
    pub sounds: Vec<Sound>,
    pub categories: Vec<Option<Category>>,
    pub paused_instances: BTreeMap<u32, Node>,
    pub category_count: u32,
    pub available_channels: u32,
    pub sound_count: u32,
    pub next_instance_id: u32,
}

fn sample_of(h: &impl Host, n: Node) -> Sample {
    h.node(n).sample().expect("ee_sound: an instance on a channel or in a sound has a sample")
}

fn update(h: &mut impl Host, n: Node, f: impl FnOnce(&mut NodeFields)) {
    let mut d = h.node(n);
    f(&mut d);
    h.set_node(n, &d);
}

impl SoundManager {
    /// The constructor's state: `category_count` categories, each allowed every one of `available_channels`
    /// channels (the C++ computes the count: the argument, else the mixer's).
    pub fn new(category_count: u32, available_channels: u32) -> Self {
        let mut m = SoundManager {
            random: URandom::new(0),
            one_shots: Vec::new(),
            channels: vec![None; available_channels as usize],
            paused: Vec::new(),
            sounds: Vec::new(),
            categories: vec![None; category_count as usize],
            paused_instances: BTreeMap::new(),
            category_count,
            available_channels,
            sound_count: 0,
            next_instance_id: 0,
        };
        for i in 0..category_count {
            m.set_category_percentage(i, 1.0);
        }
        m
    }

    fn cat_mut(&mut self, category: u32) -> Option<&mut Category> {
        self.categories.get_mut(category as usize).and_then(|c| c.as_mut())
    }

    fn add_category_instances(&mut self, category: u32, delta: i32) {
        if let Some(c) = self.cat_mut(category) {
            c.number_of_instances = c.number_of_instances.wrapping_add(delta as u32);
        }
    }

    fn instances(&self, sample_id: u32) -> Option<&Vec<Node>> {
        self.sounds.get(sample_id as usize).and_then(|s| s.instances.as_ref())
    }

    /// The destructor's part after the thread and lock are gone: drops the paused queue's and map's references,
    /// deletes every loaded sample and drops its nodes, stops and destroys the mixer's channels.
    pub fn destroy(&mut self, h: &mut impl Host) {
        for n in std::mem::take(&mut self.paused) {
            h.node_dereference(n);
        }
        for (_, n) in std::mem::take(&mut self.paused_instances) {
            h.node_dereference(n);
        }
        for i in 0..self.sound_count as usize {
            let Some(insts) = self.sounds[i].instances.clone() else { continue };
            for n in insts {
                h.sample_delete(h.node(n).sample());
                update(h, n, |d| d.sample = std::ptr::null_mut());
                h.node_dereference(n);
            }
        }
        h.stop_all_channels();
        h.destroy_channels();
    }

    /// `SetCategoryPercentage`: the category's share of the channels (at least one); creates the category.
    pub fn set_category_percentage(&mut self, category: u32, percent: f32) -> u32 {
        let count = self.available_channels as f32 * percent;
        let mut max = float_to_long(count) as u32;
        if max == 0 {
            max = 1;
        }
        let Some(slot) = self.categories.get_mut(category as usize) else { return max };
        let c =
            slot.get_or_insert(Category { percentage: 0.0, max_instances: 0, number_of_instances: 0, priority: NONE });
        c.max_instances = max;
        c.percentage = percent;
        max
    }

    /// `SetCategoryPriority`: -1 for a category that does not exist.
    pub fn set_category_priority(&mut self, category: u32, priority: u32) -> i32 {
        match self.cat_mut(category) {
            Some(c) => {
                c.priority = priority;
                OK
            }
            None => -1,
        }
    }

    /// `GetCategoryPriority` (none: -1).
    pub fn category_priority(&self, category: u32) -> u32 {
        self.categories.get(category as usize).and_then(|c| c.as_ref()).map_or(NONE, |c| c.priority)
    }

    fn category(&self, h: &impl Host, sample_id: u32) -> u32 {
        self.instances(sample_id).map_or(NONE, |i| h.node(i[0]).category)
    }

    fn max_instances(&self, sample_id: u32) -> u32 {
        self.instances(sample_id).map_or(0, |i| i.len() as u32)
    }

    fn max_category_instances(&self, h: &impl Host, sample_id: u32) -> u32 {
        if self.instances(sample_id).is_none() {
            return 0;
        }
        let c = self.category(h, sample_id);
        if c == NONE {
            return 0;
        }
        self.categories.get(c as usize).and_then(|c| c.as_ref()).map_or(0, |c| c.max_instances)
    }

    fn number_of_category_instances(&self, h: &impl Host, sample_id: u32) -> u32 {
        if self.instances(sample_id).is_none() {
            return 0;
        }
        let c = self.category(h, sample_id);
        if c == NONE {
            return 0;
        }
        self.categories.get(c as usize).and_then(|c| c.as_ref()).map_or(0, |c| c.number_of_instances)
    }

    /// Instances of the sample on a channel or paused (any `mChannel` but none), plus its sample-less paused ones.
    fn number_of_instances(&self, h: &impl Host, sample_id: u32) -> u32 {
        let Some(insts) = self.instances(sample_id) else { return 0 };
        let mut count = insts.iter().filter(|&&n| h.node(n).channel != NONE).count() as u32;
        for &n in &self.paused {
            let d = h.node(n);
            if d.sample_id == sample_id && d.sample.is_null() {
                count += 1;
            }
        }
        count
    }

    fn free_channel(&self) -> u32 {
        (0..self.available_channels).find(|&i| self.channels[i as usize].is_none()).unwrap_or(NONE)
    }

    fn playing_instance(&self, h: &impl Host, instance_id: u32) -> Option<(Node, u32)> {
        if instance_id == NONE {
            return None;
        }
        (0..self.available_channels).find_map(|i| match self.channels[i as usize] {
            Some(n) if h.node(n).instance_id == instance_id => Some((n, i)),
            _ => None,
        })
    }

    fn paused_by_id(&self, instance_id: u32) -> Option<Node> {
        if instance_id == NONE {
            return None;
        }
        self.paused_instances.get(&instance_id).copied()
    }

    fn paused_index(&self, h: &impl Host, instance_id: u32) -> Option<(Node, usize)> {
        if instance_id == NONE {
            return None;
        }
        self.paused.iter().enumerate().find_map(|(i, &n)| (h.node(n).instance_id == instance_id).then_some((n, i)))
    }

    /// The playing instance with the id, else the paused one (through the instance map).
    pub fn find_instance(&self, h: &impl Host, instance_id: u32) -> Option<Node> {
        self.playing_instance(h, instance_id).map(|p| p.0).or_else(|| self.paused_by_id(instance_id))
    }

    fn random_offset(&mut self, h: &impl Host, s: Sample) -> u32 {
        let r = self.random.generate() * h.sample_length(s) as f32;
        float_to_long(r) as u32
    }

    fn time_left(h: &mut impl Host, n: Node) -> i32 {
        let s = sample_of(h, n);
        if h.sample_is_looping(s) {
            -1
        } else {
            h.playback_time(s).1
        }
    }

    fn total_play_time(h: &mut impl Host, n: Node) -> i32 {
        let s = sample_of(h, n);
        if h.sample_is_looping(s) {
            -1
        } else {
            h.playback_time(s).0
        }
    }

    /// `GetVolume2(node)`: the sample's volume, else the node's.
    pub fn node_volume(h: &impl Host, n: Node) -> f32 {
        let d = h.node(n);
        d.sample().map_or(d.volume, |s| h.sample_volume(s))
    }

    /// `SetVolume2(node)`: on the sample, else on the node.
    pub fn node_set_volume(h: &mut impl Host, n: Node, v: f32) -> i32 {
        match h.node(n).sample() {
            Some(s) => h.sample_set_volume(s, v),
            None => {
                update(h, n, |d| d.volume = v);
                OK
            }
        }
    }

    pub fn node_set_pan(h: &mut impl Host, n: Node, v: f32) -> i32 {
        match h.node(n).sample() {
            Some(s) => h.sample_set_pan(s, v),
            None => {
                update(h, n, |d| d.pan = v);
                OK
            }
        }
    }

    pub fn node_set_pitch(h: &mut impl Host, n: Node, v: f32) -> i32 {
        match h.node(n).sample() {
            Some(s) => h.sample_set_pitch(s, v),
            None => {
                update(h, n, |d| d.pitch = v);
                OK
            }
        }
    }

    fn release_channel(&mut self, h: &mut impl Host, channel: u32) {
        if let Some(old) = self.channels.get_mut(channel as usize).and_then(|c| c.take()) {
            h.node_dereference(old);
        }
    }

    fn occupy_channel(&mut self, h: &mut impl Host, channel: u32, n: Node) {
        self.release_channel(h, channel);
        self.channels[channel as usize] = Some(n);
        h.node_reference(n);
    }

    /// `DumpChannel`: stops the instance on the channel, rewinds its sample, frees the channel.
    fn dump_channel(&mut self, h: &mut impl Host, channel: u32) {
        let Some(&Some(n)) = self.channels.get(channel as usize) else { return };
        let s = sample_of(h, n);
        h.stop_channel(h.sample_mixer_channel(s));
        h.set_sample_offset(s, 0);
        self.add_category_instances(h.node(n).category, -1);
        update(h, n, |d| d.channel = NONE);
        self.release_channel(h, channel);
    }

    /// `AddInstanceToPausedQueue`: off its channel (stopped) and onto the paused queue.
    fn add_instance_to_paused_queue(&mut self, h: &mut impl Host, n: Node) {
        let s = sample_of(h, n);
        h.stop_channel(h.sample_mixer_channel(s));
        let ch = h.node(n).channel;
        if ch != NONE {
            self.release_channel(h, ch);
        }
        update(h, n, |d| d.channel = PAUSED);
        self.add_prioritized_node_to_paused_queue(h, n);
    }

    /// Before the first node of no larger priority value; the queue and the map each take a reference.
    fn add_prioritized_node_to_paused_queue(&mut self, h: &mut impl Host, n: Node) {
        let priority = self.category_priority(h.node(n).category);
        h.node_reference(n);
        let at = self.paused.iter().position(|&p| self.category_priority(h.node(p).category) <= priority);
        match at {
            Some(i) => self.paused.insert(i, n),
            None => self.paused.push(n),
        }
        h.node_reference(n);
        self.paused_instances.insert(h.node(n).instance_id, n);
    }

    /// `AddNewSampleToPausedQueue`: pauses a free instance of the sample as looping or, without one, queues a new
    /// sample-less instance; its id and the referenced node.
    fn add_new_sample_to_paused_queue(
        &mut self,
        h: &mut impl Host,
        sample_id: u32,
        volume: f32,
        pan: f32,
        pitch: f32,
    ) -> (u32, Option<Node>) {
        let mut instance_id = NONE;
        let mut out = None;
        let max = self.max_instances(sample_id);
        let Some(insts) = self.instances(sample_id).cloned() else { return (instance_id, out) };
        let mut node = insts[0];
        for i in 0..max {
            node = insts[i as usize];
            if h.node(node).channel != NONE {
                continue;
            }
            h.sample_set_looping(sample_of(h, node), true);
            let id = self.next_instance_id;
            self.next_instance_id = id.wrapping_add(1);
            update(h, node, |d| {
                d.instance_id = id;
                d.instance_index = i;
            });
            instance_id = id;
            self.add_instance_to_paused_queue(h, node);
            self.add_category_instances(h.node(node).category, 1);
            out = Some(node);
            h.node_reference(node);
            Self::node_set_volume(h, node, volume);
            Self::node_set_pan(h, node, pan);
            Self::node_set_pitch(h, node, pitch);
            break;
        }
        if instance_id == NONE {
            let model = h.node(node);
            let fresh = h.node_new();
            let id = self.next_instance_id;
            self.next_instance_id = id.wrapping_add(1);
            update(h, fresh, |d| {
                d.category = model.category;
                d.instance_id = id;
                d.sample = std::ptr::null_mut();
                d.pan = pan;
                d.pitch = pitch;
                d.sample_id = sample_id;
                d.channel = PAUSED;
                d.max_instances = model.max_instances;
                d.volume = volume;
            });
            self.add_category_instances(model.category, 1);
            self.add_prioritized_node_to_paused_queue(h, fresh);
            instance_id = id;
            out = Some(fresh);
            h.node_reference(fresh);
            h.node_dereference(fresh);
        }
        (instance_id, out)
    }

    /// `DumpLowerPrioritySample`: frees a channel of lower priority (the one closest to its end; a looping one is
    /// paused), else an equal-priority one (a looping one is paused; a one-shot only if it ends before the new
    /// sample would). The channel, or none.
    fn dump_lower_priority_sample(&mut self, h: &mut impl Host, sample_id: u32, looping: bool) -> u32 {
        let mut channel = NONE;
        let cat = self.category(h, sample_id);
        let priority = self.categories[cat as usize].as_ref().map_or(NONE, |c| c.priority);
        // (channel, looping, seconds left)
        let mut lower = (NONE, false, f32::MAX);
        let mut equal = (NONE, false, f32::MAX);
        for i in 0..self.available_channels {
            let Some(n) = self.channels[i as usize] else { continue };
            let p = self.category_priority(h.node(n).category);
            let slot = if p > priority {
                &mut lower
            } else if p == priority {
                &mut equal
            } else {
                continue;
            };
            if h.sample_is_looping(sample_of(h, n)) {
                if slot.0 == NONE {
                    *slot = (i, true, f32::MAX);
                }
            } else {
                let left = Self::time_left(h, n) as f32 * 0.001f32;
                if left < slot.2 {
                    *slot = (i, false, left);
                }
            }
        }
        if lower.0 != NONE {
            if lower.1 {
                let n = self.channels[lower.0 as usize].expect("ee_sound: the chosen channel is occupied");
                self.add_instance_to_paused_queue(h, n);
            } else {
                self.dump_channel(h, lower.0);
            }
            channel = lower.0;
        } else if equal.0 != NONE {
            if equal.1 {
                let n = self.channels[equal.0 as usize].expect("ee_sound: the chosen channel is occupied");
                self.add_instance_to_paused_queue(h, n);
                channel = equal.0;
            } else if !looping {
                if let Some(&front) = self.instances(sample_id).and_then(|i| i.first()) {
                    if Self::total_play_time(h, front) as f32 * 0.001f32 > equal.2 {
                        self.dump_channel(h, equal.0);
                        channel = equal.0;
                    }
                }
            }
        }
        channel
    }

    /// `DumpNonPlayingChannels`: frees the channels whose one-shot sample ended; the last one freed.
    fn dump_non_playing_channels(&mut self, h: &mut impl Host) -> u32 {
        let mut channel = NONE;
        for i in 0..self.available_channels {
            let Some(n) = self.channels[i as usize] else { continue };
            let s = sample_of(h, n);
            if h.channel_state(h.sample_mixer_channel(s)) == CHANNEL_IDLE && !h.sample_is_looping(s) {
                self.dump_channel(h, i);
                channel = i;
            }
        }
        channel
    }

    /// `Load`: a sample from the sound path in a category with up to `max_instances` instances (at most one per
    /// channel; the extra ones duplicate the first). A name already loaded gains instances and a load. The sample
    /// id, or none.
    pub fn load(&mut self, h: &mut impl Host, name: &[u8], category: u32, max_instances: u32) -> u32 {
        if category >= self.category_count || max_instances == 0 {
            return NONE;
        }
        let Some(file) = h.find_file(name) else { return NONE };
        let mut sample_id = 0;
        let mut insts = None;
        while sample_id < self.sound_count {
            if self.sounds[sample_id as usize].name == name {
                insts = self.sounds[sample_id as usize].instances.take();
                break;
            }
            sample_id += 1;
        }
        let mut insts = insts.unwrap_or_default();
        let max = self.available_channels.min(max_instances);
        for i in insts.len() as u32..max {
            let n = h.node_new();
            let sample = if i != 0 { h.sample_duplicate(sample_of(h, insts[0])) } else { Some(h.sample_load(file)) };
            update(h, n, |d| {
                d.category = category;
                d.channel = NONE;
                d.instance_id = NONE;
                d.sample_id = sample_id;
                d.max_instances = max;
                d.sample = sample.map_or(std::ptr::null_mut(), |s| s.0);
            });
            insts.push(n);
        }
        if sample_id == self.sound_count {
            self.sounds.push(Sound { name: name.to_vec(), instances: Some(insts), load_count: 1 });
        } else {
            let s = &mut self.sounds[sample_id as usize];
            s.instances = Some(insts);
            s.load_count = s.load_count.wrapping_add(1);
        }
        self.sound_count = self.sounds.len() as u32;
        self.categories[category as usize].get_or_insert(Category {
            percentage: 0.0,
            max_instances: 0,
            number_of_instances: 0,
            priority: NONE,
        });
        h.file_delete(file);
        sample_id
    }

    /// `PlayAndReturnReferencedNode`: a free channel, else one whose sample ended, else one of lower priority; at
    /// the instance limits an instance of the sample (or of its category) is replaced; a looping sound with no
    /// channel waits paused. The node with a reference for the caller, or None.
    pub fn play(
        &mut self,
        h: &mut impl Host,
        sample_id: u32,
        volume: f32,
        pan: f32,
        pitch: f32,
        looping: bool,
    ) -> Option<Node> {
        if self.sounds.is_empty()
            || sample_id as usize > self.sounds.len() - 1
            || self.sounds[sample_id as usize].instances.is_none()
            || (volume == 0.0 && !looping)
        {
            return None;
        }
        let mut node = None;
        if self.number_of_instances(h, sample_id) >= self.max_instances(sample_id)
            || self.number_of_category_instances(h, sample_id) >= self.max_category_instances(h, sample_id)
        {
            self.dump_non_playing_channels(h);
        }
        if self.number_of_instances(h, sample_id) < self.max_instances(sample_id)
            && self.number_of_category_instances(h, sample_id) < self.max_category_instances(h, sample_id)
        {
            let mut channel = self.free_channel();
            if channel == NONE {
                channel = self.dump_non_playing_channels(h);
            }
            if channel == NONE {
                channel = self.dump_lower_priority_sample(h, sample_id, looping);
            }
            if channel == NONE {
                if looping {
                    node = self.add_new_sample_to_paused_queue(h, sample_id, volume, pan, pitch).1;
                }
            } else {
                node = self.really_play_it(h, channel, sample_id, volume, pan, pitch, looping).1;
            }
        } else if looping {
            node = self.add_new_sample_to_paused_queue(h, sample_id, volume, pan, pitch).1;
        } else {
            let channel = if self.number_of_instances(h, sample_id) >= self.max_instances(sample_id) {
                self.replace_non_looping_instance(h, sample_id)
            } else {
                self.replace_category_instance(h, sample_id)
            };
            if channel != NONE {
                node = self.really_play_it(h, channel, sample_id, volume, pan, pitch, looping).1;
            }
        }
        node
    }

    /// `ReallyPlayIt`: the sample's first instance not on a channel goes on `channel` and starts (a looping one
    /// at a random offset).
    #[allow(clippy::too_many_arguments)]
    fn really_play_it(
        &mut self,
        h: &mut impl Host,
        channel: u32,
        sample_id: u32,
        volume: f32,
        pan: f32,
        pitch: f32,
        looping: bool,
    ) -> (u32, Option<Node>) {
        let mut instance_id = NONE;
        let mut out = None;
        let max = self.max_instances(sample_id);
        let Some(insts) = self.instances(sample_id).cloned() else { return (instance_id, out) };
        for i in 0..max {
            let n = insts[i as usize];
            if h.node(n).channel != NONE {
                continue;
            }
            let s = sample_of(h, n);
            h.stop_channel(channel);
            h.stop_channel(h.sample_mixer_channel(s));
            h.sample_set_mixer_channel(s, NONE);
            if h.set_channel_sample(channel, s) != OK {
                continue;
            }
            let id = self.next_instance_id;
            self.next_instance_id = id.wrapping_add(1);
            update(h, n, |d| d.instance_id = id);
            instance_id = id;
            h.sample_set_looping(s, looping);
            update(h, n, |d| d.instance_index = i);
            out = Some(n);
            h.node_reference(n);
            if looping {
                let offset = self.random_offset(h, s);
                h.set_sample_offset(s, offset);
            }
            let cat = self.category(h, sample_id);
            self.add_category_instances(cat, 1);
            update(h, n, |d| d.channel = channel);
            self.occupy_channel(h, channel, n);
            Self::node_set_volume(h, n, volume);
            Self::node_set_pan(h, n, pan);
            Self::node_set_pitch(h, n, pitch);
            h.play_channel(channel);
            break;
        }
        (instance_id, out)
    }

    /// `RegisterOneShotSound`: remembers a playing or paused instance (with a sample) to unload once it stops.
    pub fn register_one_shot(&mut self, h: &impl Host, sample_id: u32, instance_id: u32) -> i32 {
        if let Some(n) = self.find_instance(h, instance_id) {
            if let Some(s) = h.node(n).sample() {
                self.one_shots.push(OneShot { sample: s, sample_id, instance_id });
            }
        }
        OK
    }

    /// `ReplaceCategoryInstance`: frees the channel of the instance (of the sample, else of its category) closest
    /// to its end; that channel, or an empty channel seen on the way.
    fn replace_category_instance(&mut self, h: &mut impl Host, sample_id: u32) -> u32 {
        let mut free = NONE;
        let category = self.category(h, sample_id);
        let mut candidate: Option<Node> = None;
        let mut found_sample = false;
        for i in 0..self.available_channels {
            match self.channels[i as usize] {
                Some(n) => {
                    let d = h.node(n);
                    if d.sample_id == sample_id {
                        found_sample = true;
                    } else if found_sample || d.category != category {
                        continue;
                    }
                    match candidate {
                        None => candidate = Some(n),
                        Some(c) => {
                            let left = Self::time_left(h, n);
                            if left < Self::time_left(h, c) {
                                candidate = Some(n);
                            }
                        }
                    }
                }
                None => free = i,
            }
        }
        let Some(c) = candidate else { return free };
        let d = h.node(c);
        let mut channel = d.channel;
        if channel != NONE && channel != PAUSED {
            let looping = h.sample_is_looping(sample_of(h, c));
            if d.sample_id == sample_id {
                if looping {
                    return free;
                }
                self.dump_channel(h, channel);
            } else if looping {
                self.add_instance_to_paused_queue(h, c);
            } else {
                self.dump_channel(h, channel);
            }
        } else {
            channel = NONE;
        }
        if channel != NONE {
            channel
        } else {
            free
        }
    }

    /// `ReplaceNonLoopingInstance`: frees the channel of the quietest one-shot instance of the sample quieter
    /// than 0.01.
    fn replace_non_looping_instance(&mut self, h: &mut impl Host, sample_id: u32) -> u32 {
        let mut channel = NONE;
        let mut volume = 0.01f32;
        for i in 0..self.available_channels {
            let Some(n) = self.channels[i as usize] else { continue };
            if h.node(n).sample_id == sample_id
                && !h.sample_is_looping(sample_of(h, n))
                && Self::node_volume(h, n) < volume
            {
                channel = i;
                volume = Self::node_volume(h, n);
            }
        }
        if channel != NONE {
            self.dump_channel(h, channel);
        }
        channel
    }

    /// One pass of the scavenger thread (the C++ thread waits a second between passes, holding the lock only
    /// here): retires ended or silent channels, detaches the samples of silent paused instances, and restarts the
    /// loudest paused instances (by category priority, then the node's volume) on the free channels.
    // `!(v < 0.01)` and `!(v > 0.01)` as the C++ wrote them: a NaN volume takes the same branch
    #[allow(clippy::neg_cmp_op_on_partial_ord)]
    pub fn scavenge(&mut self, h: &mut impl Host, muted: bool) {
        let count = self.available_channels;
        let mut free: Vec<u32> = Vec::with_capacity(count as usize);
        for i in 0..count {
            let Some(n) = self.channels[i as usize] else {
                free.push(i);
                continue;
            };
            let s = sample_of(h, n);
            if (Self::node_volume(h, n) < 0.01 || h.channel_state(h.sample_mixer_channel(s)) == CHANNEL_IDLE)
                && !h.sample_is_looping(s)
            {
                self.dump_channel(h, i);
                free.push(i);
            } else if h.sample_is_looping(s) && h.node(n).channel == i && (muted || Self::node_volume(h, n) < 0.01) {
                self.add_instance_to_paused_queue(h, n);
                free.push(i);
            }
        }
        let paused_count = if muted { 0 } else { self.paused.len() };
        for j in 0..paused_count {
            let n = self.paused[j];
            let d = h.node(n);
            let Some(s) = d.sample() else { continue };
            if !(h.sample_volume(s) < 0.01) {
                continue;
            }
            update(h, n, |d| d.channel = PAUSED);
            h.sample_set_mixer_channel(s, NONE);
            h.sample_set_volume(s, 0.0);
            let index = d.instance_index;
            if index != NONE && self.instances(d.sample_id).is_some() {
                let fresh = h.node_new();
                self.sounds[d.sample_id as usize].instances.as_mut().unwrap()[index as usize] = fresh;
                update(h, fresh, |f| {
                    f.sample = s.0;
                    f.sample_id = d.sample_id;
                    f.category = d.category;
                    f.max_instances = d.max_instances;
                });
                h.node_dereference(n);
            }
            update(h, n, |d| {
                d.instance_index = NONE;
                d.sample = std::ptr::null_mut();
            });
        }
        let mut free_count = free.len();
        if muted || self.paused.is_empty() || free_count == 0 {
            return;
        }
        let paused_size = self.paused.len();
        let mut keyed: Vec<(Node, u32, f32)> = self
            .paused
            .iter()
            .map(|&n| {
                let d = h.node(n);
                (n, self.category_priority(d.category), d.volume)
            })
            .collect();
        msvc_sort::sort_by(&mut keyed, |a, b| a.1 > b.1 || (a.1 == b.1 && a.2 < b.2));
        self.paused = keyed.into_iter().map(|k| k.0).collect();
        let mut free_index = free_count.wrapping_sub(1);
        for j in (0..paused_size).rev() {
            if free_count == 0 {
                break;
            }
            let n = self.paused[j];
            let d = h.node(n);
            let volume = d.sample().map_or(d.volume, |s| h.sample_volume(s));
            if !(volume > 0.01) {
                continue;
            }
            let channel = free[free_index];
            if let Some(s) = d.sample() {
                let offset = self.random_offset(h, s);
                h.set_sample_offset(s, offset);
                h.set_channel_sample(channel, s);
                h.play_channel(channel);
                update(h, n, |d| d.channel = channel);
                self.occupy_channel(h, channel, n);
                self.paused.remove(j);
                free_count -= 1;
                free_index = free_index.wrapping_sub(1);
                self.paused_instances.remove(&h.node(n).instance_id);
                h.node_dereference(n);
                h.node_dereference(n);
            } else {
                let max = self.max_instances(d.sample_id);
                if self.instances(d.sample_id).is_none() {
                    continue;
                }
                for k in 0..max as usize {
                    let other = self.sounds[d.sample_id as usize].instances.as_ref().unwrap()[k];
                    if h.node(other).channel != NONE {
                        continue;
                    }
                    let s = Sample(h.node(other).sample);
                    update(h, n, |d| d.sample = s.0);
                    update(h, other, |d| d.sample = std::ptr::null_mut());
                    h.sample_set_looping(s, true);
                    update(h, n, |d| {
                        d.instance_index = k as u32;
                        d.channel = channel;
                    });
                    self.occupy_channel(h, channel, n);
                    let f = h.node(n);
                    Self::node_set_volume(h, n, f.volume);
                    let f = h.node(n);
                    Self::node_set_pan(h, n, f.pan);
                    let f = h.node(n);
                    Self::node_set_pitch(h, n, f.pitch);
                    let offset = self.random_offset(h, s);
                    h.set_sample_offset(s, offset);
                    h.set_channel_sample(channel, s);
                    h.play_channel(channel);
                    h.node_dereference(other);
                    self.sounds[d.sample_id as usize].instances.as_mut().unwrap()[k] = n;
                    h.node_reference(n);
                    h.node_dereference(n);
                    self.paused.remove(j);
                    free_count -= 1;
                    free_index = free_index.wrapping_sub(1);
                    self.paused_instances.remove(&h.node(n).instance_id);
                    h.node_dereference(n);
                    break;
                }
            }
        }
    }

    /// `SetMaxChannels`: pauses the looping instances and stops the others, recreates the mixer's channels and
    /// recomputes each category's instance limit.
    pub fn set_max_channels(&mut self, h: &mut impl Host, count: u32) -> i32 {
        if count == 0 {
            return FAILED;
        }
        if count == self.available_channels {
            return OK;
        }
        for i in 0..self.channels.len() {
            let Some(n) = self.channels[i] else { continue };
            if h.sample_is_looping(sample_of(h, n)) {
                self.add_instance_to_paused_queue(h, n);
            } else {
                self.dump_channel(h, i as u32);
            }
        }
        h.destroy_channels();
        let result = h.create_channels(count);
        self.available_channels = count;
        for n in std::mem::take(&mut self.channels).into_iter().flatten() {
            h.node_dereference(n);
        }
        self.channels = vec![None; count as usize];
        for i in 0..self.categories.len() {
            let percentage = self.categories[i].as_ref().map_or(1.0, |c| c.percentage);
            self.set_category_percentage(i as u32, percentage);
        }
        result
    }

    /// `Stop(instance)`: stops the playing or paused instance and frees its channel or queue slot, or with
    /// `let_finish` only ends its looping so that it plays out.
    pub fn stop(&mut self, h: &mut impl Host, instance_id: u32, let_finish: bool) -> i32 {
        if instance_id == NONE {
            return OK;
        }
        if let Some((n, channel)) = self.playing_instance(h, instance_id) {
            let s = sample_of(h, n);
            if !let_finish {
                h.stop_channel(h.sample_mixer_channel(s));
                h.set_sample_offset(s, 0);
                update(h, n, |d| d.channel = NONE);
                self.add_category_instances(h.node(n).category, -1);
                self.release_channel(h, channel);
            } else {
                h.sample_set_looping(s, false);
            }
        } else if let Some((n, index)) = self.paused_index(h, instance_id) {
            let d = h.node(n);
            match d.sample() {
                Some(s) if let_finish => h.sample_set_looping(s, false),
                Some(s) => {
                    h.stop_channel(h.sample_mixer_channel(s));
                    h.set_sample_offset(s, 0);
                    update(h, n, |d| d.channel = NONE);
                    self.add_category_instances(d.category, -1);
                    self.paused.remove(index);
                    self.paused_instances.remove(&instance_id);
                    h.node_dereference(n);
                    h.node_dereference(n);
                }
                None => {
                    self.add_category_instances(d.category, -1);
                    self.paused.remove(index);
                    self.paused_instances.remove(&instance_id);
                    h.node_dereference(n);
                    h.node_dereference(n);
                }
            }
        }
        OK
    }

    /// `StopAllOneShotSounds`: stops and unloads every remembered one-shot sound.
    pub fn stop_all_one_shots(&mut self, h: &mut impl Host) -> i32 {
        while !self.one_shots.is_empty() {
            let o = self.one_shots[0];
            self.stop(h, o.instance_id, false);
            self.unload(h, o.sample_id);
            self.one_shots.remove(0);
        }
        OK
    }

    /// `Unload`: one load of the sample; the last drops its paused instances, frees its channels and deletes its
    /// instances and samples. Fails for an unknown or unloaded sample.
    pub fn unload(&mut self, h: &mut impl Host, sample_id: u32) -> i32 {
        if self.sounds.is_empty()
            || sample_id as usize > self.sounds.len() - 1
            || self.sounds[sample_id as usize].instances.is_none()
        {
            return FAILED;
        }
        let sound = &mut self.sounds[sample_id as usize];
        if sound.load_count > 1 {
            sound.load_count -= 1;
            return OK;
        }
        sound.load_count = 0;
        let mut i = 0;
        while i < self.paused.len() {
            let n = self.paused[i];
            let d = h.node(n);
            if d.sample_id == sample_id {
                self.add_category_instances(d.category, -1);
                self.paused.remove(i);
                self.paused_instances.remove(&d.instance_id);
                h.node_dereference(n);
                h.node_dereference(n);
            } else {
                i += 1;
            }
        }
        for i in 0..self.available_channels {
            if let Some(n) = self.channels[i as usize] {
                if h.node(n).sample_id == sample_id {
                    self.dump_channel(h, i);
                }
            }
        }
        if let Some(insts) = self.sounds[sample_id as usize].instances.take() {
            for n in insts {
                h.sample_delete(h.node(n).sample());
                update(h, n, |d| d.sample = std::ptr::null_mut());
                h.node_dereference(n);
            }
        }
        OK
    }

    /// `UpdateOneShotSounds` (the scavenger, before each wait): unloads the one-shot sounds whose channel stopped.
    pub fn update_one_shots(&mut self, h: &mut impl Host) {
        let mut i = 0;
        while i < self.one_shots.len() {
            let o = self.one_shots[i];
            if h.channel_state(h.sample_mixer_channel(o.sample)) == CHANNEL_PLAYING {
                i += 1;
            } else {
                self.unload(h, o.sample_id);
                self.one_shots.remove(i);
            }
        }
    }

    // The public queries and setters by instance id (`find_instance`) or by node.

    /// `GetIsLooping(node)`: a node without a sample counts as looping.
    pub fn node_is_looping(h: &impl Host, n: Node) -> bool {
        h.node(n).sample().is_none_or(|s| h.sample_is_looping(s))
    }

    pub fn is_looping(&self, h: &impl Host, instance_id: u32) -> bool {
        if instance_id == NONE {
            return false;
        }
        self.find_instance(h, instance_id).is_some_and(|n| Self::node_is_looping(h, n))
    }

    /// `GetIsPlaying(node)`: its sample is on a mixer channel (channel 0 does not count, as in C++).
    pub fn node_is_playing(h: &impl Host, n: Node) -> bool {
        h.node(n).sample().is_some_and(|s| {
            let c = h.sample_mixer_channel(s);
            c != 0 && c != NONE
        })
    }

    pub fn is_playing(&self, h: &impl Host, instance_id: u32) -> bool {
        self.find_instance(h, instance_id).is_some_and(|n| Self::node_is_playing(h, n))
    }

    pub fn node_pan(h: &impl Host, n: Node) -> f32 {
        let d = h.node(n);
        d.sample().map_or(d.pan, |s| h.sample_pan(s))
    }

    pub fn pan(&self, h: &impl Host, instance_id: u32) -> f32 {
        self.find_instance(h, instance_id).map_or(0.0, |n| Self::node_pan(h, n))
    }

    pub fn node_pitch(h: &impl Host, n: Node) -> f32 {
        let d = h.node(n);
        d.sample().map_or(d.pitch, |s| h.sample_pitch(s))
    }

    /// `GetPitch(instance)`: 1 without such an instance (the node form gives 0 for a null node).
    pub fn pitch(&self, h: &impl Host, instance_id: u32) -> f32 {
        self.find_instance(h, instance_id).map_or(1.0, |n| Self::node_pitch(h, n))
    }

    pub fn sample_id(&self, h: &impl Host, instance_id: u32) -> u32 {
        self.find_instance(h, instance_id).map_or(NONE, |n| h.node(n).sample_id)
    }

    /// `GetSampleRate(instance)`: a sample-less instance takes the rate of its sound's first instance.
    pub fn sample_rate(&self, h: &impl Host, instance_id: u32) -> u32 {
        let Some(n) = self.find_instance(h, instance_id) else { return 0 };
        let d = h.node(n);
        match d.sample() {
            Some(s) => h.sample_rate(s),
            None => self.instances(d.sample_id).and_then(|i| h.node(i[0]).sample()).map_or(0, |s| h.sample_rate(s)),
        }
    }

    /// `GetTimeLeftInMilli(instance)`: -1 looping, 0 without such an instance (or without a sample, where the C++
    /// read through a null sample).
    pub fn time_left_of(&self, h: &mut impl Host, instance_id: u32) -> i32 {
        match self.find_instance(h, instance_id) {
            Some(n) if h.node(n).sample().is_some() => Self::time_left(h, n),
            _ => 0,
        }
    }

    /// `GetTotalPlayTimeInMilli(sample)`: the length of the sample's first instance while one of its instances
    /// plays or waits (else 0; -1 looping).
    pub fn total_play_time_of(&self, h: &mut impl Host, sample_id: u32) -> i32 {
        if self.number_of_instances(h, sample_id) == 0 {
            return 0;
        }
        let n = self.instances(sample_id).unwrap()[0];
        if h.node(n).sample().is_none() {
            return 0;
        }
        Self::total_play_time(h, n)
    }

    /// `GetVolume2(instance)`: 1 without such an instance.
    pub fn volume(&self, h: &impl Host, instance_id: u32) -> f32 {
        self.find_instance(h, instance_id).map_or(1.0, |n| Self::node_volume(h, n))
    }

    /// `SetVolume2(instance)`: on the sample and, unlike pan and pitch, on the node too.
    pub fn set_volume(&self, h: &mut impl Host, instance_id: u32, v: f32) -> i32 {
        let Some(n) = self.find_instance(h, instance_id) else { return OK };
        let r = match h.node(n).sample() {
            Some(s) => h.sample_set_volume(s, v),
            None => OK,
        };
        update(h, n, |d| d.volume = v);
        r
    }

    pub fn set_pan(&self, h: &mut impl Host, instance_id: u32, v: f32) -> i32 {
        self.find_instance(h, instance_id).map_or(OK, |n| Self::node_set_pan(h, n, v))
    }

    pub fn set_pitch(&self, h: &mut impl Host, instance_id: u32, v: f32) -> i32 {
        self.find_instance(h, instance_id).map_or(OK, |n| Self::node_set_pitch(h, n, v))
    }

    /// `SetSoundParameters(node)`: the node's volume, pitch and pan, then the sample's; the first sample error.
    pub fn node_set_parameters(h: &mut impl Host, n: Node, volume: f32, pan: f32, pitch: f32) -> i32 {
        update(h, n, |d| {
            d.volume = volume;
            d.pitch = pitch;
            d.pan = pan;
        });
        let Some(s) = h.node(n).sample() else { return OK };
        let mut r = h.sample_set_volume(s, volume);
        if r == OK {
            r = h.sample_set_pitch(s, pitch);
            if r == OK {
                r = h.sample_set_pan(s, pan);
            }
        }
        r
    }

    pub fn set_parameters(&self, h: &mut impl Host, instance_id: u32, volume: f32, pan: f32, pitch: f32) -> i32 {
        self.find_instance(h, instance_id).map_or(OK, |n| Self::node_set_parameters(h, n, volume, pan, pitch))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fake::Fake;

    fn loaded(h: &mut Fake, m: &mut SoundManager, name: &str, cat: u32, max: u32, len: u32) -> u32 {
        h.add_file(name, len, 1000);
        m.load(h, name.as_bytes(), cat, max)
    }

    #[test]
    fn new_gives_every_category_every_channel() {
        let m = SoundManager::new(3, 8);
        assert_eq!(m.channels.len(), 8);
        for c in &m.categories {
            let c = c.unwrap();
            assert_eq!((c.max_instances, c.number_of_instances, c.priority, c.percentage), (8, 0, NONE, 1.0));
        }
    }

    #[test]
    fn category_percentage_rounds_half_even_and_keeps_one_channel() {
        let mut m = SoundManager::new(1, 5);
        assert_eq!(m.set_category_percentage(0, 0.5), 2); // 2.5 -> 2
        assert_eq!(m.set_category_percentage(0, 0.7), 4); // 3.5 -> 4
        assert_eq!(m.set_category_percentage(0, 0.0), 1);
        assert_eq!(m.set_category_priority(0, 3), OK);
        assert_eq!(m.set_category_priority(7, 3), -1);
        assert_eq!(m.category_priority(0), 3);
        assert_eq!(m.category_priority(9), NONE);
    }

    #[test]
    fn load_refuses_bad_arguments_and_missing_files() {
        let mut h = Fake::default();
        let mut m = SoundManager::new(2, 4);
        assert_eq!(m.load(&mut h, b"none.wav", 0, 1), NONE);
        h.add_file("a.wav", 100, 1000);
        assert_eq!(m.load(&mut h, b"a.wav", 2, 1), NONE);
        assert_eq!(m.load(&mut h, b"a.wav", 0, 0), NONE);
        assert_eq!(m.load(&mut h, b"a.wav", 0, 9), 0);
        // at most one instance per channel; a reload adds a load, not a sound
        assert_eq!(m.sounds[0].instances.as_ref().unwrap().len(), 4);
        assert_eq!(m.load(&mut h, b"a.wav", 0, 1), 0);
        assert_eq!(m.sounds[0].load_count, 2);
        assert_eq!(h.live_nodes(), 4);
    }

    #[test]
    fn play_takes_free_channels_then_replaces_the_quietest_one_shot() {
        let mut h = Fake::default();
        let mut m = SoundManager::new(1, 4);
        let s = loaded(&mut h, &mut m, "a.wav", 0, 2, 100);
        let a = m.play(&mut h, s, 0.005, 0.0, 1.0, false).unwrap();
        let b = m.play(&mut h, s, 0.002, 0.0, 1.0, false).unwrap();
        h.node_dereference(a);
        h.node_dereference(b);
        assert_eq!(m.channels.iter().flatten().count(), 2);
        // both instances in use: the quieter one (b, channel 1) is dumped and replayed
        let c = m.play(&mut h, s, 0.5, 0.0, 1.0, false).unwrap();
        assert_eq!(h.node(c).channel, 1);
        assert_eq!(h.node(c).instance_id, 2);
        h.node_dereference(c);
        // a silent one-shot is not played at all
        assert!(m.play(&mut h, s, 0.0, 0.0, 1.0, false).is_none());
        assert!(m.play(&mut h, 9, 1.0, 0.0, 1.0, false).is_none());
    }

    #[test]
    fn looping_without_a_channel_waits_paused_and_the_scavenger_starts_it() {
        let mut h = Fake::default();
        let mut m = SoundManager::new(1, 1);
        let s = loaded(&mut h, &mut m, "a.wav", 0, 1, 100);
        let t = loaded(&mut h, &mut m, "b.wav", 0, 1, 100);
        let a = m.play(&mut h, s, 0.5, 0.0, 1.0, false).unwrap();
        let b = m.play(&mut h, t, 0.5, 0.0, 1.0, true).unwrap();
        assert_eq!(h.node(b).channel, PAUSED);
        assert!(h.node(b).sample().is_some());
        assert_eq!(m.paused, vec![b]);
        assert_eq!(m.paused_instances.get(&h.node(b).instance_id), Some(&b));
        // the one-shot ends: the scavenger frees its channel and starts the looping sound there
        h.set_state(h.sample_mixer_channel(h.node(a).sample().unwrap()), CHANNEL_IDLE);
        m.scavenge(&mut h, false);
        assert_eq!(h.node(b).channel, 0);
        assert!(m.paused.is_empty() && m.paused_instances.is_empty());
        assert_eq!(m.channels[0], Some(b));
        // muted: the looping sound goes back to the queue and stays there
        m.scavenge(&mut h, true);
        assert_eq!(m.paused, vec![b]);
        m.scavenge(&mut h, true);
        assert_eq!(m.paused, vec![b]);
        h.node_dereference(a);
        h.node_dereference(b);
    }

    #[test]
    fn stop_and_unload_release_every_reference() {
        let mut h = Fake::default();
        let mut m = SoundManager::new(1, 2);
        let s = loaded(&mut h, &mut m, "a.wav", 0, 2, 100);
        let a = m.play(&mut h, s, 0.5, 0.0, 1.0, true).unwrap();
        let id = h.node(a).instance_id;
        assert!(m.is_looping(&h, id));
        assert_eq!(m.stop(&mut h, id, false), OK);
        assert!(m.channels.iter().all(|c| c.is_none()));
        assert_eq!(m.categories[0].unwrap().number_of_instances, 0);
        h.node_dereference(a);
        assert_eq!(m.unload(&mut h, s), OK);
        assert_eq!(h.live_nodes(), 0);
        assert_eq!(h.live_samples(), 0);
        assert_eq!(m.unload(&mut h, s), FAILED);
        assert_eq!(m.unload(&mut h, 5), FAILED);
    }

    #[test]
    fn queries_without_an_instance_give_the_defaults() {
        let mut h = Fake::default();
        let m = SoundManager::new(1, 2);
        assert!(!m.is_looping(&h, 3) && !m.is_playing(&h, 3) && !m.is_looping(&h, NONE));
        assert_eq!((m.pan(&h, 3), m.pitch(&h, 3), m.volume(&h, 3)), (0.0, 1.0, 1.0));
        assert_eq!((m.sample_id(&h, 3), m.sample_rate(&h, 3)), (NONE, 0));
        assert_eq!((m.time_left_of(&mut h, 3), m.total_play_time_of(&mut h, 3)), (0, 0));
        assert_eq!(m.set_volume(&mut h, 3, 0.5), OK);
    }

    #[test]
    fn set_max_channels_rebuilds_the_channels() {
        let mut h = Fake::default();
        let mut m = SoundManager::new(2, 4);
        m.set_category_percentage(1, 0.5);
        assert_eq!(m.set_max_channels(&mut h, 0), FAILED);
        assert_eq!(m.set_max_channels(&mut h, 4), OK);
        assert_eq!(m.set_max_channels(&mut h, 10), OK);
        assert_eq!(m.channels.len(), 10);
        assert_eq!(m.categories[0].unwrap().max_instances, 10);
        assert_eq!(m.categories[1].unwrap().max_instances, 5);
    }

    #[test]
    fn random_offsets_follow_the_manager_s_own_series() {
        // URandom(0) -> seed 1, the generator's first value times the length, rounded half to even
        let mut h = Fake::default();
        let mut m = SoundManager::new(1, 1);
        let s = loaded(&mut h, &mut m, "a.wav", 0, 1, 1000);
        let n = m.play(&mut h, s, 1.0, 0.0, 1.0, true).unwrap();
        let mut r = URandom::new(0);
        let want = float_to_long(r.generate() * 1000.0) as u32;
        assert_eq!(h.offset_of(h.node(n).sample().unwrap()), Some(want));
        h.node_dereference(n);
    }
}
