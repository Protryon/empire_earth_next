//! The active unit lists' tests over lists of their own.

use std::sync::Mutex;

use super::active::{ActiveLists, Lists};
use crate::units::order::UpdateOrder;

#[test]
fn units_start_and_stop_stepping_with_their_references() {
    let (l, o) = (Mutex::new(ActiveLists::new()), Mutex::new(UpdateOrder::new()));
    let s = Lists { lists: &l, order: &o };
    let mut refs = Vec::new();
    assert!(s.activate(7) && s.activate(3) && !s.activate(7));
    s.update(&mut |id, d| refs.push((id, d)));
    assert_eq!(refs, vec![(7, 2), (7, -1), (3, 2), (3, -1)]);
    assert_eq!(s.list(0), vec![7, 3]);
    // a unit already stepped only gives its queue reference back
    refs.clear();
    s.activate(7);
    s.update(&mut |id, d| refs.push((id, d)));
    assert_eq!(refs, vec![(7, -1)]);
    // stopping: in the set and the list -> two references go; one not stepped only its queue's
    refs.clear();
    assert!(s.deactivate(7) && s.deactivate(9) && !s.deactivate(9));
    s.update(&mut |id, d| refs.push((id, d)));
    assert_eq!(refs, vec![(7, -2), (7, -1), (9, -1)]);
    assert_eq!(s.list(0), vec![3]);
    assert!(l.lock().unwrap().active.contains(&3) && !l.lock().unwrap().active.contains(&7));
}

#[test]
fn clear_drops_every_reference_in_the_cs_order() {
    let (l, o) = (Mutex::new(ActiveLists::new()), Mutex::new(UpdateOrder::new()));
    let s = Lists { lists: &l, order: &o };
    s.start(20);
    s.start(10);
    s.activate(5);
    s.deactivate(6);
    let mut log = Vec::new();
    let between = std::cell::RefCell::new(0);
    s.clear(&mut |id, d| log.push((id, d)), &mut || *between.borrow_mut() += 1);
    // the queues, (the C++'s step), the stepped list in its order, the set by id
    assert_eq!(log, vec![(5, -1), (6, -1), (20, -1), (10, -1), (10, -1), (20, -1)]);
    assert_eq!(*between.borrow(), 1);
    assert!(s.list(0).is_empty() && s.list(1).is_empty() && s.list(2).is_empty());
    s.set_list(1, &[4, 5]);
    s.set_active(4, true);
    assert_eq!(s.list(1), vec![4, 5]);
    s.set_active(4, false);
    assert!(l.lock().unwrap().active.is_empty());
}
