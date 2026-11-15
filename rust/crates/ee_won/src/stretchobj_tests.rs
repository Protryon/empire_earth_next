//! stretchobj's StretchImage: unit tests of the settings and the rebuild flag (the golden replay of the original
//! statements is `golden_replay`, over tests/won/won_stretchobj_test.cpp's `--print`).

use super::*;

#[test]
fn a_new_image_tiles_draws_its_centre_and_must_be_built() {
    let o = EeWonStretchImg::new(1, 2, 3, 4);
    assert_eq!((o.left, o.top, o.right, o.bottom), (1, 2, 3, 4));
    assert!(o.tile_horz && o.tile_vert && !o.skip_center && o.need_recalc);
}

#[test]
fn only_a_size_change_marks_the_cache() {
    let mut o = EeWonStretchImg::new(0, 0, 0, 0);
    o.need_recalc = false;
    assert!(!o.set_size(10, 20, 10, 20));
    assert!(!o.need_recalc);
    assert!(o.set_size(10, 20, 11, 20));
    assert!(o.need_recalc);
}

#[test]
fn copies_and_duplicates_keep_the_settings() {
    let mut a = EeWonStretchImg::new(5, 6, 7, 8);
    a.tile_vert = false;
    a.skip_center = true;
    a.need_recalc = false;
    let mut b = EeWonStretchImg::new(0, 0, 0, 0);
    b.need_recalc = false;
    b.copy_attributes(&a);
    assert_eq!(b, EeWonStretchImg { need_recalc: false, ..a });
    let d = a.duplicate();
    assert_eq!(d, EeWonStretchImg { need_recalc: true, ..a });
    assert_eq!(d.get(EeWonStretchField::TileVert), 0);
    assert_eq!(d.get(EeWonStretchField::Bottom), 8);
}

/// The C++ shell's StretchImage over the Rust settings, as the driver's `port::StretchImage`.
struct Img {
    w: u16,
    h: u16,
    image: i32,
    cached: String,
    o: EeWonStretchImg,
}

impl Img {
    fn calc(&mut self, trace: &mut Vec<String>) {
        let o = &self.o;
        self.cached = format!(
            "{}/{},{},{},{},{},{},{},{},{}",
            self.image,
            self.w,
            self.h,
            o.left,
            o.top,
            o.right,
            o.bottom,
            o.tile_horz as i32,
            o.tile_vert as i32,
            o.skip_center as i32
        );
        trace.push(format!("calc {}", self.cached));
        self.o.need_recalc = false;
    }
    fn state(&self) -> String {
        let o = &self.o;
        format!(
            "{},{},{},{},{},{},{},{},{},{},{},{}",
            self.w,
            self.h,
            self.image,
            o.left,
            o.top,
            o.right,
            o.bottom,
            o.tile_horz as i32,
            o.tile_vert as i32,
            o.skip_center as i32,
            o.need_recalc as i32,
            self.cached
        )
    }
}

fn plan(ops: &[EeWonDrawOp]) -> String {
    ops.iter().map(|p| format!("{}:{},{},{},{},{}", p.kind, p.a, p.b, p.c, p.d, p.e)).collect::<Vec<_>>().join(" ")
}

#[test]
fn golden_replay() {
    let text = include_str!("golden_stretchobj.txt");
    let lines: Vec<&str> = text.lines().collect();
    let nums = |s: &str| s.split_whitespace().map(|x| x.parse::<i32>().unwrap()).collect::<Vec<_>>();
    let mut i = 0;
    let (mut seqs, mut calls) = (0, 0);
    while i < lines.len() {
        assert!(lines[i].starts_with("S "));
        let mut im: Vec<Img> = (0..3)
            .map(|k| {
                let v = nums(&lines[i + 1 + k][2..]);
                Img { w: 0, h: 0, image: v[0], cached: String::new(), o: EeWonStretchImg::new(v[1], v[2], v[3], v[4]) }
            })
            .collect();
        i += 4;
        while i < lines.len() && lines[i].starts_with("O ") {
            let v = nums(&lines[i][2..]);
            let (op, a, j, v) = (v[0], v[1] as usize, v[2] as usize, &v[3..8]);
            let mut tr: Vec<String> = Vec::new();
            match op {
                0 => {
                    let x = &mut im[a];
                    if x.o.set_size(x.w as i32, x.h as i32, v[0], v[1]) {
                        x.w = v[0] as u16;
                        x.h = v[1] as u16;
                    }
                }
                1 => {
                    let o = &mut im[a].o;
                    (o.left, o.top, o.right, o.bottom) = (v[0], v[1], v[2], v[3]);
                }
                2 => (im[a].o.tile_horz, im[a].o.tile_vert) = (v[0] & 1 != 0, v[1] & 1 != 0),
                3 => im[a].o.skip_center = v[0] & 1 != 0,
                4 => {
                    let f = im[j].o;
                    let (w, h) = (im[j].w, im[j].h);
                    im[a].o.copy_attributes(&f);
                    (im[a].w, im[a].h) = (w, h);
                }
                5 => {
                    let d = Img { w: 0, h: 0, image: im[j].image, cached: String::new(), o: im[j].o.duplicate() };
                    im[a] = d;
                }
                6 => {
                    if im[a].o.need_recalc {
                        im[a].calc(&mut tr);
                    }
                    tr.push(format!("native {}", im[a].cached));
                }
                7 => {
                    if im[a].o.need_recalc {
                        im[a].calc(&mut tr);
                    }
                    let x = &im[a];
                    tr.push(format!("paint {} {}", x.cached, plan(&x.o.paint(x.w as i32, x.h as i32))));
                }
                8 => im[a].image = v[0],
                _ => {
                    im[a] = Img {
                        w: 0,
                        h: 0,
                        image: v[0],
                        cached: String::new(),
                        o: EeWonStretchImg::new(v[1], v[2], v[3], v[4]),
                    }
                }
            }
            let mut s = tr.join("; ") + " |";
            for x in &im {
                s += " ";
                s += &x.state();
            }
            assert_eq!(s, &lines[i + 1][2..], "line {}", i + 1);
            i += 2;
            calls += 1;
        }
        seqs += 1;
    }
    assert_eq!((seqs, calls), (300, 300 * 60));
}
