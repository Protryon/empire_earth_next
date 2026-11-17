//! The list box's tests: the line format's tokens and numbers, the order, and the list's operations over the
//! builder's fake host (`civ_whole::tests::Fake`).

use super::*;
use crate::screens::civ_whole::tests::{Fake, FakeList};

fn lines_of(h: &Fake, l: usize) -> Vec<String> {
    h.lists[&l].lines.iter().map(|t| String::from_utf8_lossy(t).into_owned()).collect()
}

fn fake_with(lines: &[&str]) -> (Fake, usize) {
    let mut h = Fake::new();
    let mut list = FakeList::new();
    list.lines = lines.iter().map(|l| l.as_bytes().to_vec()).collect();
    h.lists.insert(0x500, list);
    (h, 0x500)
}

#[test]
fn tokens_and_numbers() {
    let line = b"Archers/Attack/25/65537/2/7/ 20.00%";
    assert_eq!(token(line, TOKEN_CATEGORY).unwrap(), b"Archers");
    assert_eq!(token(line, TOKEN_VALUE).unwrap(), b" 20.00%");
    assert_eq!(token(line, 7), None);
    assert_eq!(number(line, TOKEN_UPGRADE), Some(65537));
    assert!(is_upgrade_line(line));
    assert!(!is_upgrade_line(b"Archers/2/Category"));
    // empty tokens are skipped (Tokenize): "a//b" has two
    assert_eq!(token(b"a//b", 1).unwrap(), b"b");
    assert_eq!(token(b"/a/", 0).unwrap(), b"a");
    // the predefined civilizations' lines: a blank upgrade token (an upgrade line that does not scan)
    let civ = b"Middle Ages/Austria/ / / / / ";
    assert!(is_upgrade_line(civ));
    assert_eq!(number(civ, TOKEN_UPGRADE), None);
    assert_eq!(scan_unsigned(b"  +12x"), Some(12));
    assert_eq!(scan_unsigned(b"-1"), Some(u32::MAX));
    assert_eq!(scan_unsigned(b" "), None);
    assert_eq!(scan_unsigned(b"x1"), None);
    assert_eq!(header(b"Cat", b"3"), b"Cat/3/Category");
}

#[test]
fn line_order_puts_the_special_powers_first() {
    let sp = b"Special Powers";
    assert!(line_less(sp, b"special powers/x/1", b"Archers/2/Category"));
    assert!(!line_less(sp, b"Archers/2/Category", b"SPECIAL POWERS/15/Category"));
    assert!(line_less(sp, b"archers/2/Category", b"Archers/Attack/1/1/1/1/ 1%"));
    assert!(line_less(sp, b"Archers/Attack", b"archers/hit"));
    assert!(!line_less(sp, b"b", b"A"));
    assert_eq!(line_order(sp, b"abc", b"ABC"), Ordering::Equal);
    let sorted = sorted(sp, &[b"Cav/1".to_vec(), b"Special Powers/Z".to_vec(), b"arch/1".to_vec()]);
    assert_eq!(sorted, vec![b"Special Powers/Z".to_vec(), b"arch/1".to_vec(), b"Cav/1".to_vec()]);
    // an empty name: no line is special
    assert!(line_less(b"", b"a", b"b"));
}

#[test]
fn add_and_delete_keep_one_header_per_category() {
    let (mut h, l) = fake_with(&[]);
    add_line(&mut h, l, b"Cav/Attack/1/65537/3/3/ 1%");
    add_line(&mut h, l, b"Arch/Attack/1/1/2/2/ 1%");
    add_line(&mut h, l, b"Cav/Armor/1/65539/3/3/ 1%");
    assert_eq!(
        lines_of(&h, l),
        [
            "Arch/2/Category",
            "Arch/Attack/1/1/2/2/ 1%",
            "Cav/3/Category",
            "Cav/Armor/1/65539/3/3/ 1%",
            "Cav/Attack/1/65537/3/3/ 1%"
        ]
    );
    assert_eq!(find_category(&mut h, l, b"Cav"), Some(2));
    assert_eq!(find_upgrade(&mut h, l, 65537), Some(4));
    delete_line(&mut h, l, 4);
    assert_eq!(lines_of(&h, l).len(), 4, "the header stays while an armor line is left");
    delete_line(&mut h, l, 3);
    assert_eq!(lines_of(&h, l), ["Arch/2/Category", "Arch/Attack/1/1/2/2/ 1%"]);
    delete_line(&mut h, l, 9);
    assert_eq!(lines_of(&h, l).len(), 2);
    // unsorted: appended in order, the header after its first line
    h.lists.get_mut(&l).unwrap().flags[2] = false;
    add_line(&mut h, l, b"Zed/X/1/131073/9/9/ 1%");
    assert_eq!(lines_of(&h, l)[2..], ["Zed/X/1/131073/9/9/ 1%".to_string(), "Zed/9/Category".to_string()]);
}

#[test]
fn selection_moves_off_headers() {
    let (mut h, l) = fake_with(&["A/0/Category", "A/x/ / / / / ", "A/y/ / / / / ", "B/0/Category", "B/z/ / / / / "]);
    // auto select: a header is refused
    h.lists.get_mut(&l).unwrap().auto = true;
    change_selection(&mut h, l, 0, false, false);
    assert!(!h.lists[&l].has);
    h.lists.get_mut(&l).unwrap().auto = false;
    // no selection (0): a header after it goes forward
    change_selection(&mut h, l, 3, true, false);
    assert_eq!(h.lists[&l].selection, 4);
    // from 4, the header 3 goes backward
    change_selection(&mut h, l, 3, false, false);
    assert_eq!(h.lists[&l].selection, 2);
    // from 2, header 0: backward finds none, forward the first line
    change_selection(&mut h, l, 0, false, false);
    assert_eq!(h.lists[&l].selection, 1);
    change_selection(&mut h, l, 2, false, false);
    assert_eq!(h.lists[&l].selection, 2);
    // upgrade-only off: the selection reset to 0
    h.lists.get_mut(&l).unwrap().flags[0] = false;
    change_selection(&mut h, l, 4, false, false);
    assert_eq!(h.lists[&l].selection, 0);
    // tabbed to without a selection on a header: moved to the first upgrade line
    h.lists.get_mut(&l).unwrap().flags[0] = true;
    h.lists.get_mut(&l).unwrap().has = false;
    tabbed_to(&mut h, l);
    assert_eq!(h.lists[&l].selection, 1);
    assert!(h.lists[&l].dirty);
}

#[test]
fn the_selected_upgrade_reads_as_the_original() {
    let (mut h, l) = fake_with(&["A/2/Category", "A/x/25/7/2/2/ 1%", "A/y/ / / / / ", "A/z/9/8"]);
    assert_eq!(selected_upgrade(&mut h, l, (0, 0, 0)), Err(0xd));
    {
        let list = h.lists.get_mut(&l).unwrap();
        list.has = true;
        list.selection = 0;
    }
    assert_eq!(selected_upgrade(&mut h, l, (0, 0, 0)), Err(3));
    h.lists.get_mut(&l).unwrap().selection = 1;
    assert_eq!(selected_upgrade(&mut h, l, (0, 0, 0)), Ok((7, 25, 2)));
    // a blank upgrade token: Ok with nothing written
    h.lists.get_mut(&l).unwrap().selection = 2;
    assert_eq!(selected_upgrade(&mut h, l, (5, 6, 7)), Ok((5, 6, 7)));
    // no cost token: 0 written for it
    h.lists.get_mut(&l).unwrap().selection = 3;
    assert_eq!(selected_upgrade(&mut h, l, (5, 6, 7)), Ok((8, 9, 0)));
}

#[test]
fn draws_upgrades_and_headers() {
    let (mut h, l) = fake_with(&["Special Powers/15/Category", "A/x/25/7/2/3/ 1%"]);
    h.lists.get_mut(&l).unwrap().flags[3] = true;
    let before = h.log.len();
    assert_eq!(draw_text(&mut h, l), 0);
    let draws: Vec<&String> =
        h.log[before..].iter().filter(|s| s.starts_with(&format!("{} ", EE_SCREENS_CBW_L_DRAW))).collect();
    assert_eq!(draws.len(), 3);
    assert!(
        draws[0].ends_with(&format!("0 {} {} Special Powers", EE_SCREENS_CBW_DRAW_BOLD, EE_SCREENS_CBW_COLOUR_HEADER))
    );
    assert!(draws[1].ends_with(&format!("1 0 {}         1% x", EE_SCREENS_CBW_COLOUR_BLACK)));
    assert!(draws[2].ends_with(&format!("1 {} {} 3  ", EE_SCREENS_CBW_DRAW_RIGHT, EE_SCREENS_CBW_COLOUR_DARK_BLUE)));
}
