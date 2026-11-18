//! The performance's message queues (dmusic's `Message.c`): a binary heap ordered by time, then by kind, then a
//! note's end before a note's start. Equal messages have no order in a heap, so this is dmusic's heap, step for step:
//! the order in which equal messages come out (and so the music) is the one dmusic gave.

use std::cmp::Ordering;

use super::files::{Kind, Message};

/// What `add` does with a message of the same kind within 10 ticks of the new one.
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum Conflict {
    Keep,
    Replace,
    Append,
}

/// `DmMessage_sort`.
fn sort(a: &Message, b: &Message) -> Ordering {
    a.time.cmp(&b.time).then(a.kind.rank().cmp(&b.kind.rank())).then_with(|| match (&a.kind, &b.kind) {
        (Kind::Note { on: x, .. }, Kind::Note { on: y, .. }) if x != y => {
            if *x {
                Ordering::Greater
            } else {
                Ordering::Less
            }
        }
        _ => Ordering::Equal,
    })
}

fn sign(o: Ordering) -> i32 {
    match o {
        Ordering::Less => -1,
        Ordering::Equal => 0,
        Ordering::Greater => 1,
    }
}

/// `DmMessageQueue`.
#[derive(Default)]
pub struct Queue {
    heap: Vec<Message>,
}

impl Queue {
    pub fn len(&self) -> usize {
        self.heap.len()
    }

    pub fn is_empty(&self) -> bool {
        self.heap.is_empty()
    }

    /// `DmMessageQueue_add`: the message at `time`.
    pub fn add(&mut self, message: &Message, time: u32, conflict: Conflict) {
        if conflict != Conflict::Append {
            let rank = message.kind.rank();
            let near = self.heap.iter_mut().find(|m| {
                let d = (i64::from(m.time) - i64::from(time)).abs();
                d < 10 && m.kind.rank() == rank
            });
            if let Some(m) = near {
                if conflict == Conflict::Replace {
                    *m = Message { time, kind: message.kind.clone() };
                }
                return;
            }
        }
        self.heap.push(Message { time, kind: message.kind.clone() });
        // DmMessageQueue_heapInsert
        let mut i = self.heap.len() - 1;
        while i > 0 {
            let parent = (i - 1) / 2;
            if sign(sort(&self.heap[i], &self.heap[parent])) >= 0 {
                break;
            }
            self.heap.swap(i, parent);
            i = parent;
        }
    }

    /// `DmMessageQueue_get`: the first message.
    pub fn first(&self) -> Option<&Message> {
        self.heap.first()
    }

    /// `DmMessageQueue_pop`.
    pub fn pop(&mut self) {
        if self.heap.is_empty() {
            return;
        }
        let last = self.heap.len() - 1;
        self.heap.swap(0, last);
        self.heap.pop();
        let len = self.heap.len();
        if len == 0 {
            return;
        }
        // DmMessageQueue_heapRemove (its loop runs while i < length - 1)
        let mut i = 0usize;
        while i + 1 < len {
            let c1 = 2 * i + 1;
            let c2 = 2 * i + 2;
            let s1 = if c1 < len { sign(sort(&self.heap[i], &self.heap[c1])) } else { -2 };
            let s2 = if c2 < len { sign(sort(&self.heap[i], &self.heap[c2])) } else { -2 };
            if (s1 == -2 && s2 == -2) || (s1 <= 0 && s2 <= 0) {
                break;
            }
            let swap = if s1 == -2 && s2 > 0 {
                c2
            } else if s2 == -1 && s1 > 0 {
                c1
            } else if c2 >= len || sign(sort(&self.heap[c1], &self.heap[c2])) < 0 {
                // (past the end dmusic compared with the slot the moved message came from: the message at i,
                // which sorts after its child c1 here)
                c1
            } else {
                c2
            };
            self.heap.swap(i, swap);
            i = swap;
        }
    }

    /// `DmMessageQueue_clear`.
    pub fn clear(&mut self) {
        self.heap.clear();
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::dmusic::files::Command;

    fn note(on: bool, n: u8) -> Message {
        Message { time: 0, kind: Kind::Note { on, note: n, velocity: 1, channel: 0 } }
    }

    fn drain(q: &mut Queue) -> Vec<(u32, u8, bool, u8)> {
        let mut out = Vec::new();
        while let Some(m) = q.first() {
            let (on, n) = match m.kind {
                Kind::Note { on, note, .. } => (on, note),
                _ => (false, 255),
            };
            out.push((m.time, m.kind.rank(), on, n));
            q.pop();
        }
        out
    }

    #[test]
    fn time_then_kind_then_a_note_end_first() {
        let mut q = Queue::default();
        q.add(&note(true, 1), 10, Conflict::Append);
        q.add(&note(false, 1), 10, Conflict::Append);
        q.add(&Message { time: 0, kind: Kind::Tempo(100.0) }, 10, Conflict::Append);
        q.add(&note(true, 2), 5, Conflict::Append);
        let out = drain(&mut q);
        assert_eq!(out, vec![(5, 0, true, 2), (10, 0, false, 1), (10, 0, true, 1), (10, 6, false, 255)]);
    }

    #[test]
    fn keep_and_replace_look_ten_ticks_around() {
        let mut q = Queue::default();
        let cmd =
            |level| Message { time: 0, kind: Kind::Command(Command { groove_level: level, ..Command::default() }) };
        q.add(&cmd(1), 100, Conflict::Keep);
        q.add(&cmd(2), 109, Conflict::Keep);
        assert_eq!(q.len(), 1);
        q.add(&cmd(3), 91, Conflict::Replace);
        assert_eq!(q.len(), 1);
        assert_eq!(q.first().unwrap().time, 91);
        assert!(matches!(q.first().unwrap().kind, Kind::Command(Command { groove_level: 3, .. })));
        q.add(&cmd(4), 110, Conflict::Keep);
        assert_eq!(q.len(), 2, "ten ticks away is no conflict");
    }

    #[test]
    fn equal_messages_come_out_in_the_heaps_order() {
        // 9 equal note-ons (by note number): a heap gives them in an order of its own; this is dmusic's
        let mut q = Queue::default();
        for n in 0..9 {
            q.add(&note(true, n), 0, Conflict::Append);
        }
        let order: Vec<u8> = drain(&mut q).iter().map(|m| m.3).collect();
        assert_eq!(order, vec![0, 8, 7, 6, 5, 4, 3, 2, 1]);
    }
}
