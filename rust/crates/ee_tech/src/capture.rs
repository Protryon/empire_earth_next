//! The simulation capture's tech tree and effect records (`ee/EETechCapture.h`, written with `EE_SIM_CAPTURE`) and
//! their replay: the decisions this crate makes are recomputed from each record's inputs and compared with what the
//! game did (`tools/sim_capture.py replay`, over captures recorded from the C++ before it moved here).
//!
//! Replayed: TNRS (a node's or an epoch's research decision), TNPR (a prerequisite), TSOL (an epoch's counter and
//! availability), TSWP (every node's availability and obsolescence, the prerequisite closure, the tree's checksum)
//! and EVHP (a rescaled unit). The other records (costs, effects' before/after values, calamities) carry no
//! replayable inputs; the digests of `sim_capture.py check` gate them.

use crate::effects;
use crate::tree::{self, EeTechChecksumEntry, EeTechEpochCounter, EeTechNode, EeTechTreeState, PrerequisiteView};
use std::collections::BTreeMap;

pub const TAG_NODE_RESEARCH: u32 = u32::from_le_bytes(*b"TNRS");
pub const TAG_PREREQUISITE: u32 = u32::from_le_bytes(*b"TNPR");
pub const TAG_SOL: u32 = u32::from_le_bytes(*b"TSOL");
pub const TAG_SWEEP: u32 = u32::from_le_bytes(*b"TSWP");
pub const TAG_RESCALE: u32 = u32::from_le_bytes(*b"EVHP");

/// One record of a capture file: its tag and payload.
#[derive(Clone, Debug, PartialEq)]
pub struct Record<'a> {
    pub tag: u32,
    pub payload: &'a [u8],
}

/// Splits a capture file into records; `Err` names the offset of a truncated one.
pub fn records(data: &[u8]) -> Result<Vec<Record<'_>>, String> {
    let mut out = Vec::new();
    let mut at = 0usize;
    while at < data.len() {
        if data.len() - at < 8 {
            return Err(format!("truncated record header at {at}"));
        }
        let tag = u32::from_le_bytes(data[at..at + 4].try_into().unwrap());
        let size = u32::from_le_bytes(data[at + 4..at + 8].try_into().unwrap()) as usize;
        at += 8;
        if data.len() - at < size {
            return Err(format!("truncated record payload at {at}"));
        }
        out.push(Record { tag, payload: &data[at..at + size] });
        at += size;
    }
    Ok(out)
}

struct Reader<'a> {
    data: &'a [u8],
    at: usize,
}

impl<'a> Reader<'a> {
    fn new(data: &'a [u8]) -> Self {
        Reader { data, at: 0 }
    }
    fn u32(&mut self) -> Result<u32, String> {
        let bytes = self.data.get(self.at..self.at + 4).ok_or_else(|| format!("short record at {}", self.at))?;
        self.at += 4;
        Ok(u32::from_le_bytes(bytes.try_into().unwrap()))
    }
    fn i32(&mut self) -> Result<i32, String> {
        Ok(self.u32()? as i32)
    }
    fn f32(&mut self) -> Result<f32, String> {
        Ok(f32::from_bits(self.u32()?))
    }
    fn skip(&mut self, words: usize) -> Result<(), String> {
        for _ in 0..words {
            self.u32()?;
        }
        Ok(())
    }
}

/// `EETechCaptureNode`'s fields.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct CapturedNode {
    pub node: EeTechNode,
    pub tree: EeTechTreeState,
}

fn node(r: &mut Reader) -> Result<CapturedNode, String> {
    let id = r.i32()?;
    let epoch = r.i32()?;
    let obsolete_epoch = r.i32()?;
    let flags = r.u32()?;
    let research_delay = r.i32()?;
    let prerequisite_flags = r.u32()? as u8;
    let tree = EeTechTreeState {
        current_epoch: r.i32()?,
        start_epoch: r.i32()?,
        end_epoch: r.i32()?,
        epochs_before_wonders: r.i32()?,
        epoch_count: r.i32()?,
    };
    let kind = r.i32()?;
    let data = r.u32()?;
    let prerequisites = [r.i32()?, r.i32()?, r.i32()?, r.i32()?];
    Ok(CapturedNode {
        node: EeTechNode {
            id,
            kind,
            epoch,
            obsolete_epoch,
            research_delay,
            prerequisites,
            prerequisite_flags,
            researched: flags & 1 != 0,
            obsolete: flags & 2 != 0,
            available: flags & 4 != 0,
            editor_obsoleted: flags & 8 != 0,
            makes_unit: data & 1 != 0,
            counts_toward_epoch: data & 2 != 0,
            pad: 0,
        },
        tree,
    })
}

fn epoch(r: &mut Reader) -> Result<(CapturedNode, EeTechEpochCounter), String> {
    let n = node(r)?;
    let required = r.i32()?;
    let current = r.i32()?;
    let counting = r.u32()? != 0;
    Ok((n, EeTechEpochCounter { required, current, counting, available: n.node.available, pad: [0; 2] }))
}

/// What a replay checked: records per tag, and the first mismatches (record index and text).
#[derive(Debug, Default)]
pub struct Replay {
    pub checked: BTreeMap<String, u64>,
    pub mismatches: Vec<String>,
    pub mismatch_count: u64,
}

impl Replay {
    fn check(&mut self, tag: &str, index: usize, ok: bool, what: impl FnOnce() -> String) {
        *self.checked.entry(tag.to_string()).or_default() += 1;
        if !ok {
            self.mismatch_count += 1;
            if self.mismatches.len() < 20 {
                self.mismatches.push(format!("record {index} {tag}: {}", what()));
            }
        }
    }
}

struct SweepTree<'a>(&'a BTreeMap<i32, [i32; 4]>);

impl PrerequisiteView for SweepTree<'_> {
    fn prerequisites(&self, id: i32) -> Option<[i32; 4]> {
        self.0.get(&id).copied()
    }
}

/// One sweep of one player's tree: the head's checksum, then each node's record.
struct SweepNode {
    index: usize,
    node: CapturedNode,
    available: bool,
    obsolete: bool,
    bits: Vec<u32>,
}

fn check_sweep(out: &mut Replay, checksum_index: usize, checksum: u32, nodes: &[SweepNode]) {
    let prerequisites: BTreeMap<i32, [i32; 4]> =
        nodes.iter().map(|n| (n.node.node.id, n.node.node.prerequisites)).collect();
    let ids: Vec<i32> = nodes.iter().map(|n| n.node.node.id).collect();
    let view = SweepTree(&prerequisites);
    for n in nodes {
        let (node, tree) = (&n.node.node, &n.node.tree);
        let epoch = tree::is_epoch_id(node.id, tree.epoch_count);
        let available = if epoch { tree::epoch_is_available(node) } else { tree::node_is_available(node, tree) };
        let obsolete = if epoch { node.obsolete } else { tree::node_is_obsolete(node, tree) };
        out.check("TSWP", n.index, available == n.available && obsolete == n.obsolete, || {
            format!(
                "node {} available {} obsolete {} (game: {} {})",
                node.id, available, obsolete, n.available, n.obsolete
            )
        });
        let mut bits = vec![0u32; ids.len().div_ceil(32)];
        for (j, id) in ids.iter().enumerate() {
            if tree::depends_on(&view, node.id, &node.prerequisites, *id) {
                bits[j / 32] |= 1 << (j % 32);
            }
        }
        out.check("TSWP", n.index, bits == n.bits, || format!("node {} DependsOn closure differs", node.id));
    }
    let entries: Vec<EeTechChecksumEntry> = nodes
        .iter()
        .map(|n| EeTechChecksumEntry {
            id: n.node.node.id,
            researched: n.node.node.researched,
            obsolete: n.obsolete,
            pad: [0; 2],
        })
        .collect();
    let sum = tree::checksum(&entries);
    out.check("TSWP", checksum_index, sum == checksum, || format!("checksum {sum:#x} (game: {checksum:#x})"));
}

/// Replays every replayable record of a capture.
pub fn replay(data: &[u8]) -> Result<Replay, String> {
    let mut out = Replay::default();
    let all = records(data)?;
    // the sweep in progress: (head record index, checksum, ids expected, nodes so far)
    let mut sweep: Option<(usize, u32, usize, Vec<SweepNode>)> = None;
    for (index, rec) in all.iter().enumerate() {
        let mut r = Reader::new(rec.payload);
        match rec.tag {
            TAG_NODE_RESEARCH => {
                let kind = r.u32()?;
                r.i32()?; // the player
                if kind == 0 {
                    let before = node(&mut r)?;
                    let result = r.i32()?;
                    let after = node(&mut r)?;
                    let want = tree::research_node(&before.node);
                    let delay =
                        if want.research { None } else { Some(tree::research_delay_after(before.node.research_delay)) };
                    out.check(
                        "TNRS",
                        index,
                        want.result == result && (!want.research || after.node.researched),
                        || format!("node {} result {} (game {result})", before.node.id, want.result),
                    );
                    // without research no event ran: the delay counts down from the one before
                    if let Some(delay) = delay {
                        out.check("TNRS", index, after.node.research_delay == delay, || {
                            format!("node {} delay {delay} (game {})", before.node.id, after.node.research_delay)
                        });
                    }
                } else {
                    let id = r.i32()?;
                    let (before, _) = epoch(&mut r)?;
                    let result = r.i32()?;
                    let (after, _) = epoch(&mut r)?;
                    if id == before.node.id {
                        let want = tree::research_epoch(&before.node, &before.tree);
                        out.check(
                            "TNRS",
                            index,
                            want.result == result && (!want.research || after.node.researched),
                            || format!("epoch {id} result {} (game {result})", want.result),
                        );
                    }
                }
            }
            TAG_PREREQUISITE => {
                r.i32()?;
                let before = node(&mut r)?;
                let id = r.i32()?;
                let after = node(&mut r)?;
                let want = tree::set_prerequisite(&before.node, id, before.tree.epoch_count);
                let available = before.node.available || want.becomes_available;
                out.check(
                    "TNPR",
                    index,
                    want.flags == after.node.prerequisite_flags && available == after.node.available,
                    || {
                        format!(
                            "node {} prerequisite {id}: flags {:#x} available {} (game {:#x} {})",
                            before.node.id, want.flags, available, after.node.prerequisite_flags, after.node.available
                        )
                    },
                );
            }
            TAG_SOL => {
                let kind = r.u32()?;
                r.i32()?;
                let (before, counter) = epoch(&mut r)?;
                let (after, after_counter) = epoch(&mut r)?;
                let want = if kind == 0 {
                    tree::add_sol(before.node.id, &counter, &before.tree)
                } else {
                    tree::update_availability(before.node.id, &counter, &before.tree)
                };
                let available = before.node.available || want.becomes_available;
                out.check(
                    "TSOL",
                    index,
                    want.current == after_counter.current && available == after.node.available,
                    || {
                        format!(
                            "epoch {} counter {} available {} (game {} {})",
                            before.node.id, want.current, available, after_counter.current, after.node.available
                        )
                    },
                );
            }
            TAG_SWEEP => {
                let kind = r.u32()?;
                if kind == 0 {
                    if let Some((head, checksum, _, nodes)) = sweep.take() {
                        check_sweep(&mut out, head, checksum, &nodes);
                    }
                    r.skip(5)?; // the tree
                    let count = r.u32()? as usize;
                    let checksum = r.u32()?;
                    sweep = Some((index, checksum, count, Vec::new()));
                } else {
                    r.i32()?; // the player
                    let n = node(&mut r)?;
                    let available = r.u32()? != 0;
                    let obsolete = r.u32()? != 0;
                    r.i32()?; // the unit class
                    if r.u32()? != 0 {
                        r.skip(21)?; // the costs
                    }
                    if r.u32()? != 0 {
                        r.skip(7)?; // SumCosts
                    }
                    let mut bits = Vec::new();
                    while r.at < r.data.len() {
                        bits.push(r.u32()?);
                    }
                    let Some((_, _, count, nodes)) = sweep.as_mut() else {
                        return Err(format!("record {index}: a sweep node without its head"));
                    };
                    if bits.len() != count.div_ceil(32) {
                        return Err(format!("record {index}: {} closure words for {count} ids", bits.len()));
                    }
                    nodes.push(SweepNode { index, node: n, available, obsolete, bits });
                }
            }
            TAG_RESCALE => {
                r.i32()?;
                r.u32()?;
                let value = r.i32()?;
                let old_maximum = r.u32()?;
                let new_maximum = r.i32()?;
                let amount = r.f32()?;
                let want = effects::rescale(value, old_maximum, new_maximum);
                out.check("EVHP", index, want.to_bits() == amount.to_bits(), || {
                    format!("{value} of {old_maximum} -> {new_maximum}: {want} (game {amount})")
                });
            }
            _ => {}
        }
    }
    if let Some((head, checksum, _, nodes)) = sweep.take() {
        check_sweep(&mut out, head, checksum, &nodes);
    }
    Ok(out)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn put(out: &mut Vec<u8>, tag: &[u8; 4], words: &[u32]) {
        out.extend_from_slice(tag);
        out.extend_from_slice(&(words.len() as u32 * 4).to_le_bytes());
        for w in words {
            out.extend_from_slice(&w.to_le_bytes());
        }
    }

    // id, epoch, obsolete epoch, flags, delay, prerequisite flags, cur, start, end, wonders, epochs, kind, data, p1..p4
    fn node_words(id: i32, flags: u32, prerequisite_flags: u32, p: [i32; 4]) -> Vec<u32> {
        let mut w = vec![id as u32, 2, 14, flags, 0, prerequisite_flags, 3, 0, 14, 1, 15, 0, 0];
        w.extend(p.iter().map(|x| *x as u32));
        w
    }

    #[test]
    fn records_split_and_refuse_truncation() {
        let mut data = Vec::new();
        put(&mut data, b"TEST", &[1, 2]);
        put(&mut data, b"NEXT", &[]);
        let r = records(&data).unwrap();
        assert_eq!(r.len(), 2);
        assert_eq!(r[0].payload.len(), 8);
        assert!(records(&data[..data.len() - 1]).is_err());
        assert!(records(&data[..10]).is_err());
    }

    #[test]
    fn replays_a_prerequisite_and_a_rescale() {
        let mut data = Vec::new();
        let mut w = vec![1u32];
        w.extend(node_words(2001, 0, 0x1d, [1500, 0, 0, 0]));
        w.push(1500);
        w.extend(node_words(2001, 4, 0x1f, [1500, 0, 0, 0]));
        put(&mut data, b"TNPR", &w);
        put(&mut data, b"EVHP", &[0, 7, 33, 100, 120, 40.0f32.to_bits()]);
        let out = replay(&data).unwrap();
        assert_eq!(out.mismatch_count, 0, "{:?}", out.mismatches);
        assert_eq!(out.checked["TNPR"], 1);
        assert_eq!(out.checked["EVHP"], 1);
        // a wrong outcome is found
        let mut bad = Vec::new();
        put(&mut bad, b"EVHP", &[0, 7, 33, 100, 120, 39.0f32.to_bits()]);
        assert_eq!(replay(&bad).unwrap().mismatch_count, 1);
    }

    #[test]
    fn replays_a_sweep() {
        let mut data = Vec::new();
        // two nodes: 1000 (researched), 1001 depending on 1000 (available)
        let checksum = tree::checksum(&[
            EeTechChecksumEntry { id: 1000, researched: true, obsolete: false, pad: [0; 2] },
            EeTechChecksumEntry { id: 1001, researched: false, obsolete: false, pad: [0; 2] },
        ]);
        put(&mut data, b"TSWP", &[0, 1, 3, 0, 14, 1, 2, checksum]);
        let mut a = vec![1u32, 1];
        a.extend(node_words(1000, 1 | 4, 0x1f, [0, 0, 0, 0]));
        a.extend([0, 0, 0, 0, 0, 0b01]); // not available (researched), not obsolete, class, no costs, no sum, closure
        put(&mut data, b"TSWP", &a);
        let mut b = vec![1u32, 1];
        b.extend(node_words(1001, 4, 0x1f, [1000, 0, 0, 0]));
        b.extend([1, 0, 0, 0, 0, 0b11]);
        put(&mut data, b"TSWP", &b);
        let out = replay(&data).unwrap();
        assert_eq!(out.mismatch_count, 0, "{:?}", out.mismatches);
        assert_eq!(out.checked["TSWP"], 5);
    }
}
