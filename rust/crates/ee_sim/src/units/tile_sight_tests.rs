use super::*;

/// A 16 x 16 map of tiles (handle = 1 + y * 16 + x), three words a tile, players 1..=4 (handle = 100 + id).
struct Mock {
    words: Vec<[u32; 3]>,
    shared: [bool; 5],
    enemies: [[u64; 5]; 5],
    allies: Vec<Vec<u64>>,
    current: u64,
    missing: Option<(i64, i64)>,
    log: Vec<String>,
}

const SIZE: i64 = 16;

fn mock() -> Mock {
    Mock {
        words: vec![[0; 3]; (SIZE * SIZE) as usize],
        shared: [false; 5],
        enemies: [[1; 5]; 5],
        allies: vec![Vec::new(); 5],
        current: 101,
        missing: None,
        log: Vec::new(),
    }
}

fn h(x: i64, y: i64) -> u64 {
    (1 + y * SIZE + x) as u64
}

impl SightWorld for Mock {
    fn tile(&mut self, x: i64, y: i64) -> u64 {
        if !(0..SIZE).contains(&x) || !(0..SIZE).contains(&y) || self.missing == Some((x, y)) {
            0
        } else {
            h(x, y)
        }
    }
    fn map_size(&mut self) -> i64 {
        SIZE
    }
    fn map_players(&mut self) -> u64 {
        4
    }
    fn word(&mut self, tile: u64, w: usize) -> u32 {
        self.words[tile as usize - 1][w]
    }
    fn set_word(&mut self, tile: u64, w: usize, value: u32) {
        self.words[tile as usize - 1][w] = value;
    }
    fn player_id(&mut self, player: u64) -> u32 {
        (player - 100) as u32
    }
    fn shared_los(&mut self, player: u64) -> bool {
        self.shared[(player - 100) as usize]
    }
    fn enemy(&mut self, player: u64, id: u32) -> u64 {
        self.enemies[(player - 100) as usize][id as usize]
    }
    fn allies(&mut self, player: u64) -> usize {
        self.allies[(player - 100) as usize].len()
    }
    fn ally(&mut self, player: u64, i: usize) -> u64 {
        self.allies[(player - 100) as usize][i]
    }
    fn current_player(&mut self) -> u64 {
        self.current
    }
    fn world_player(&mut self, i: usize) -> u64 {
        100 + i as u64
    }
    fn tile_shown(&mut self, tile: u64, x: i64, y: i64, player: u64) {
        self.log.push(format!("shown {tile} {x} {y} {player}"));
    }
    fn tile_hidden(&mut self, tile: u64, player: u64) {
        self.log.push(format!("hidden {tile} {player}"));
    }
    fn first_sighting(&mut self, player: u64) {
        self.log.push(format!("first {player}"));
    }
    fn show(&mut self, on: bool, x: i64, y: i64) {
        self.log.push(format!("show {on} {x} {y}"));
    }
}

#[test]
fn the_rings_are_the_ceiling_distances() {
    let r = sight_rings();
    assert_eq!(r[0], vec![(0.0, 0.0)]);
    assert_eq!(r[1], vec![(-1.0, 0.0), (0.0, -1.0), (0.0, 1.0), (1.0, 0.0)]);
    // ring 2: distance in (1, 2]: the diagonals and the axis points at 2
    assert_eq!(r[2].len(), 8);
    let total: usize = r.iter().map(Vec::len).sum();
    let within = (-32i32..=32).flat_map(|x| (-32i32..=32).map(move |y| (x, y))).filter(|&(x, y)| x * x + y * y <= 1024);
    assert_eq!(total, within.count());
}

#[test]
fn a_tile_counts_each_player_in_its_half() {
    let mut m = mock();
    let t = h(2, 3);
    tile_sight(&mut m, true, t, 103); // first sighting: 2
    assert_eq!(m.words[t as usize - 1][1], 2 << 16);
    tile_sight(&mut m, true, t, 103);
    tile_sight(&mut m, true, t, 102);
    assert_eq!(m.words[t as usize - 1][1], (3 << 16) | 2);
    tile_sight(&mut m, false, t, 103);
    tile_sight(&mut m, false, t, 103); // at 2: becomes explored (1)
    tile_sight(&mut m, false, t, 103); // at 1: stays
    assert_eq!(m.words[t as usize - 1][1], (1 << 16) | 2);
    set_visibility(&mut m, t, 0, 102);
    assert_eq!(m.words[t as usize - 1][1], 1 << 16);
}

#[test]
fn shared_sight_reaches_the_allies_that_share_it_in_reverse_order() {
    let mut m = mock();
    m.allies[1] = vec![102, 103, 104];
    m.shared = [false, true, true, false, true];
    let t = h(4, 4);
    player_sight(&mut m, true, true, 101, t, 4, 4);
    assert_eq!(m.log, vec!["first 101", "shown 69 4 4 101", "shown 69 4 4 104", "shown 69 4 4 102"]);
    assert_eq!(m.words[t as usize - 1], [2 << 16, 2, 2]);
    m.log.clear();
    player_sight(&mut m, true, true, 101, t, 4, 4); // seen already: no first sighting
    player_sight(&mut m, false, false, 101, t, 4, 4); // own: no allies
    assert_eq!(m.log, vec!["shown 69 4 4 101", "shown 69 4 4 104", "shown 69 4 4 102", "hidden 69 101"]);
    m.log.clear();
    player_sight(&mut m, true, true, 101, 0, 0, 0);
    assert!(m.log.is_empty());
}

#[test]
fn a_circle_shows_its_tiles_to_the_current_player_only() {
    let mut m = mock();
    m.missing = Some((5, 6));
    circle(&mut m, true, 5.5, 5.5, 1.2, 101);
    let shown: Vec<_> = m.log.iter().filter(|l| l.starts_with("show ")).cloned().collect();
    assert_eq!(shown, vec!["show true 5 5", "show true 4 5", "show true 5 4", "show true 6 5"]);
    m.log.clear();
    circle(&mut m, false, 5.5, 5.5, 1.2, 102); // another, unallied player: no show
    assert!(m.log.iter().all(|l| !l.starts_with("show ")));
    // an ally sharing line of sight both ways is shown
    m.log.clear();
    m.enemies[1][2] = 0;
    m.enemies[2][1] = 0;
    m.shared[1] = true;
    m.shared[2] = true;
    circle(&mut m, true, 5.5, 5.5, 0.0, 102);
    assert_eq!(m.log.last().unwrap(), "show true 5 5");
}

#[test]
fn a_one_tile_step_touches_only_the_outer_rings() {
    let mut m = mock();
    circle(&mut m, true, 5.5, 5.5, 2.0, 101);
    move_circle(&mut m, 5.5, 5.5, 6.5, 5.5, 2.0, 101);
    let mut direct = mock();
    circle(&mut direct, true, 6.5, 5.5, 2.0, 101);
    // the same counts as the circle at the new centre (explored tiles left behind at 1)
    for (a, b) in m.words.iter().zip(&direct.words) {
        let (va, vb) = (a[0] >> 16 & 0xff, b[0] >> 16 & 0xff);
        assert!(va == vb || (va == 1 && vb == 0), "{va} {vb}");
    }
    // a jump redoes both circles
    move_circle(&mut m, 6.5, 5.5, 12.5, 12.5, 2.0, 101);
    assert_eq!(m.words[h(12, 12) as usize - 1][0] >> 16, 2);
    assert_eq!(m.words[h(6, 5) as usize - 1][0] >> 16, 1);
}

#[test]
fn fog_explore_and_the_save_pass() {
    let mut m = mock();
    tile_sight(&mut m, true, h(1, 1), 101);
    fog_or_explore(&mut m, true, 101);
    assert_eq!(m.words[h(1, 1) as usize - 1][0] >> 16, 1);
    fog_or_explore(&mut m, false, 102);
    assert!(m.words.iter().all(|w| w[1] & 0xff == 1));
    prepare_save(&mut m);
    // players 1..3 (the map's 4 players but player 0): every explored-only tile back to 0
    assert!(m.words.iter().all(|w| *w == [0, 0, 0]));
}
