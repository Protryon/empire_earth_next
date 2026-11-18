//! The music's reverb (dmusic's `Reverb.c`, its EE patch): the synthesizer reverb the original game's music
//! switches on with each song's four settings, on the finished 16-bit stereo mix, in place: four damped delay lines
//! in two all-pass pairs, with the original's delays, coefficients and Q15 fixed-point steps. A port of the C,
//! operation for operation (`golden_reverb.txt` is the C's output).

/// `DmReverb`.
pub struct Reverb {
    rate: f32,
    len: [i32; 4],
    ap: [i32; 2],
    dry: i32,
    wet: i32,
    golden: i32,
    ap_gain: [i32; 2],
    cur: [i32; 4],
    prev: [i32; 4],
    dmask: u32,
    amask: u32,
    delay: Vec<i32>,
    allpass: [Vec<i32>; 2],
    pos: u32,
    apos: u32,
}

/// The smallest power of two of at least `n` and at least `floor`.
fn pow2(n: u32, floor: u32) -> u32 {
    let mut v = floor;
    while v < n {
        v <<= 1;
    }
    v
}

fn q15(f: f32) -> i32 {
    let v = (f * 32768.0 + 0.5).clamp(-32768.0, 32767.0);
    v as i32
}

fn pow10(x: f32) -> f32 {
    10f64.powf(f64::from(x)) as f32
}

fn clamp16(v: i32) -> i16 {
    v.clamp(-32768, 32767) as i16
}

impl Reverb {
    /// `DmReverb_create`: the delays for the rate, the original's default settings.
    pub fn new(rate: f32) -> Reverb {
        let d1 = (f64::from(rate) * 0.045 + 0.5) as i32;
        let d2 = (f64::from(d1 as f32 * 1.189_207_1_f32) + 0.5) as i32;
        let d3 = (f64::from(d2 as f32 * 1.189_207_1_f32) + 0.5) as i32;
        let d4 = (f64::from(d3 as f32 * 1.189_207_1_f32) + 0.5) as i32;
        let a1 = (f64::from((d1 + d3) as f32) * 0.115466669202 + 0.5) as i32;
        let a2 = (f64::from((d2 + d4) as f32) * 0.115466669202 + 0.5) as i32;
        let len = [d1 - a1, d3 - a1, d2 - a2, d4 - a2];
        let longest = len.iter().copied().max().unwrap_or(0);
        let line_frames = pow2(longest.max(0) as u32 + 2, 0x1000);
        let ap_frames = pow2(a1.max(a2) as u32 + 1, 0x400);
        let mut r = Reverb {
            rate,
            len,
            ap: [a1, a2],
            dry: 0,
            wet: 0,
            golden: 0,
            ap_gain: [0; 2],
            cur: [0; 4],
            prev: [0; 4],
            dmask: 4 * line_frames - 1,
            amask: 2 * ap_frames - 1,
            delay: vec![0; 4 * line_frames as usize],
            allpass: [vec![0; 2 * ap_frames as usize], vec![0; 2 * ap_frames as usize]],
            pos: 0,
            apos: 0,
        };
        r.set(0.0, -10.0, 1000.0, 0.001);
        r
    }

    /// `DmReverb_set`: input gain and mix in dB, the decay time in ms, the high-frequency decay ratio.
    pub fn set(&mut self, gain_in_db: f32, mix_db: f32, time_ms: f32, hf_ratio: f32) {
        let mut hf = if hf_ratio < 0.999 { hf_ratio } else { 0.999 };
        if hf.is_nan() || hf <= 0.0 {
            hf = 0.001;
        }
        let time = if time_ms > 0.001 { time_ms } else { 0.001 };
        let hf_k = 1.0f32 / hf - 1.0;
        let g_in = if gain_in_db < 0.0 { gain_in_db } else { 0.0 };
        let mix = if mix_db < 0.0 { mix_db } else { 0.0 };
        if self.rate < 1.0 {
            self.rate = 22050.0;
        }
        let k = -3000.0f32 / (self.rate * time);
        let ap_g = [pow10(self.ap[0] as f32 * k), pow10(self.ap[1] as f32 * k)];
        let mut norm2 = 0.0f32;
        let mut cur = [0f32; 4];
        let mut prev = [0f32; 4];
        for i in 0..4 {
            let a = if i < 2 { 0 } else { 1 };
            let g = pow10(self.len[i] as f32 * k);
            let x = g * ap_g[a];
            let p = (((x + 1.0) * x + 1.0) * x + 1.0) * x + 1.0;
            norm2 += p * p;
            let c = pow10((self.len[i] + self.ap[a]) as f32 * k * hf_k);
            let d = (1.0 - c) * 0.5;
            prev[i] = g * d;
            cur[i] = (1.0 - d) * g;
        }
        let norm = f64::from(norm2).sqrt() as f32;
        let in_lin = 10f64.powf(f64::from(g_in * 0.05)) as f32;
        let mix_lin = 10f64.powf(f64::from(mix * 0.1)) as f32;
        let dry = f64::from(1.0 - mix_lin).sqrt() as f32 * in_lin;
        let wet = f64::from(mix_lin).sqrt() as f32 * (4.0 / norm * in_lin);
        self.dry = q15(dry);
        self.wet = q15(wet);
        self.golden = q15(f32::from_bits(0x3f1e_377a)); // 0.618034, as the original stores it
        self.ap_gain = [q15(ap_g[0]), q15(ap_g[1])];
        for i in 0..4 {
            self.cur[i] = q15(cur[i]);
            self.prev[i] = q15(prev[i]);
        }
    }

    /// `DmReverb_processStereo`: interleaved 16-bit frames, in place.
    pub fn process(&mut self, buf: &mut [i16]) {
        let frames = buf.len() / 2;
        let dm = self.dmask;
        let am = self.amask;
        let mut pos = self.pos;
        let mut apos = self.apos;
        let at = |l: i32, pos: u32| (4i32.wrapping_mul(l).wrapping_add(4).wrapping_add(pos as i32) as u32) & dm;
        let mut i1 = at(self.len[0], pos);
        let mut i2 = at(self.len[1], pos);
        let mut i3 = at(self.len[2], pos);
        let mut i4 = at(self.len[3], pos);
        let mut p1 = i1 as usize;
        let mut p2 = i2 as usize + 1;
        let mut p3 = i3 as usize + 2;
        let mut p4 = i4 as usize + 3;
        i1 = i1.wrapping_sub(4) & dm;
        i2 = i2.wrapping_sub(4) & dm;
        i3 = i3.wrapping_sub(4) & dm;
        i4 = i4.wrapping_sub(4) & dm;
        let g = self.golden;
        let mul = |a: i32, b: i32| a.wrapping_mul(b);
        let d = &mut self.delay;
        let [ap0, ap1] = &mut self.allpass;
        for n in 0..frames {
            let in_l = i32::from(buf[2 * n]);
            let in_r = i32::from(buf[2 * n + 1]);
            let mut acc_l = mul(self.dry, in_l) >> 15;
            let mut acc_r = mul(self.dry, in_r) >> 15;
            let w_l = mul(self.wet, in_l) >> 15;
            let w_r = mul(self.wet, in_r) >> 15;
            // the first all-pass pair (lines 1 and 2)
            let a = ((2 * self.ap[0]) as u32).wrapping_add(apos) & am;
            let (a0, a1) = (ap0[a as usize], ap0[a as usize + 1]);
            let o1 = d[i1 as usize];
            let o2 = d[i2 as usize + 1];
            let y1a = mul(o2, g).wrapping_add(mul(a0, self.ap_gain[0])) >> 15;
            let y1b = mul(a1, self.ap_gain[0]).wrapping_sub(mul(o1, g)) >> 15;
            ap0[(apos & am) as usize] = (mul(y1b, g) >> 15).wrapping_add(o1);
            ap0[(apos & am) as usize + 1] = o2.wrapping_sub(mul(y1a, g) >> 15);
            d[i1 as usize] = y1a;
            d[i2 as usize + 1] = y1b;
            // the second (lines 3 and 4)
            let b = ((2 * self.ap[1]) as u32).wrapping_add(apos) & am;
            let (b0, b1) = (ap1[b as usize], ap1[b as usize + 1]);
            let o3 = d[i3 as usize + 2];
            let o4 = d[i4 as usize + 3];
            let y2a = mul(o4, g).wrapping_add(mul(b0, self.ap_gain[1])) >> 15;
            let y2b = mul(b1, self.ap_gain[1]).wrapping_sub(mul(o3, g)) >> 15;
            ap1[(apos & am) as usize] = (mul(y2b, g) >> 15).wrapping_add(o3);
            ap1[(apos & am) as usize + 1] = o4.wrapping_sub(mul(y2a, g) >> 15);
            d[i3 as usize + 2] = y2a;
            d[i4 as usize + 3] = y2b;
            acc_l = acc_l.wrapping_add(y2a.wrapping_add(y1a));
            acc_r = acc_r.wrapping_add(y1b.wrapping_add(y2b));
            // the lines' damping (this frame's output and the last) and the wet input, written at the head
            let w = pos as usize;
            d[w] = (mul(self.prev[0], d[p1]).wrapping_add(mul(self.cur[0], y1a)) >> 15).wrapping_add(w_l);
            p1 = i1 as usize;
            i1 = i1.wrapping_sub(4) & dm;
            d[w + 1] =
                (mul(self.cur[1], d[i2 as usize + 1]).wrapping_add(mul(d[p2], self.prev[1])) >> 15).wrapping_add(w_r);
            p2 = i2 as usize + 1;
            i2 = i2.wrapping_sub(4) & dm;
            d[w + 2] =
                (mul(self.prev[2], d[p3]).wrapping_add(mul(self.cur[2], d[i3 as usize + 2])) >> 15).wrapping_sub(w_r);
            p3 = i3 as usize + 2;
            i3 = i3.wrapping_sub(4) & dm;
            d[w + 3] =
                (mul(self.prev[3], d[p4]).wrapping_add(mul(self.cur[3], d[i4 as usize + 3])) >> 15).wrapping_add(w_l);
            p4 = i4 as usize + 3;
            i4 = i4.wrapping_sub(4) & dm;
            apos = apos.wrapping_sub(2) & am;
            pos = pos.wrapping_sub(4) & dm;
            buf[2 * n] = clamp16(acc_l);
            buf[2 * n + 1] = clamp16(acc_r);
        }
        self.pos = pos;
        self.apos = apos;
    }

    /// The reverb over a float mix (full scale 1.0 = 32768), through 16 bits as the original runs it: 1024 frames at a
    /// time, clamped, truncated, and back.
    pub fn process_float(&mut self, f: &mut [f32]) {
        let mut tmp = [0i16; 2048];
        for block in f.chunks_mut(2048) {
            let n = block.len();
            for (t, v) in tmp[..n].iter_mut().zip(block.iter()) {
                let v = *v * 32768.0;
                *t = if v > 32767.0 {
                    32767
                } else if v < -32768.0 {
                    -32768
                } else {
                    v as i16
                };
            }
            self.process(&mut tmp[..n]);
            for (v, t) in block.iter_mut().zip(tmp[..n].iter()) {
                *v = f32::from(*t) / 32768.0;
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// The C's output (tests/sound/dm_golden.c `reverb`): per rate and setting, a hash of each 1024-frame block of
    /// its output over a test signal (12 blocks of noise, then an impulse and its tail).
    #[test]
    fn the_reverb_is_the_cs_sample_for_sample() {
        const SETTINGS: [[f32; 4]; 4] = [
            [0.0, -10.0, 1000.0, 0.001],
            [-3.0, -6.0, 2500.0, 0.5],
            [0.0, -1.0, 300.0, 0.9],
            [-10.0, -20.0, 4000.0, 0.2],
        ];
        const RATES: [f32; 3] = [22050.0, 48000.0, 96000.0];
        let golden = include_str!("golden_reverb.txt");
        let mut lines = golden.lines();
        for (r, rate) in RATES.iter().enumerate() {
            for (s, set) in SETTINGS.iter().enumerate() {
                let mut rv = Reverb::new(*rate);
                rv.set(set[0], set[1], set[2], set[3]);
                let mut x: u32 = 7;
                let mut line = format!("reverb {r} {s}");
                for b in 0..24 {
                    let mut buf = [0i16; 2048];
                    for (i, v) in buf.iter_mut().enumerate() {
                        x = x.wrapping_mul(1103515245).wrapping_add(12345);
                        let n = ((x >> 16) & 0xffff) as i32 - 32768;
                        *v = if b < 12 {
                            (n / (1 + (i as i32 % 7))) as i16
                        } else if b == 12 && i == 0 {
                            30000
                        } else {
                            0
                        };
                    }
                    rv.process(&mut buf);
                    let mut h: u32 = 2166136261;
                    for v in buf {
                        h = (h ^ u32::from(v as u16)).wrapping_mul(16777619);
                    }
                    line += &format!(" {h:08x}");
                }
                assert_eq!(line, lines.next().unwrap(), "rate {rate} setting {s}");
            }
        }
    }

    #[test]
    fn the_buffers_hold_every_delay_at_high_rates() {
        for rate in [8000.0, 44100.0, 192000.0] {
            let r = Reverb::new(rate);
            let line = (r.dmask + 1) / 4;
            assert!(r.len.iter().all(|&l| (l + 2) as u32 <= line), "{rate}");
            assert!(r.ap.iter().all(|&a| 2 * (a + 1) as u32 <= r.amask + 1), "{rate}");
        }
    }

    #[test]
    fn the_float_path_goes_through_16_bits() {
        let mut r = Reverb::new(48000.0);
        r.set(0.0, -100.0, 1000.0, 0.5); // a mix of -100 dB: dry only, (almost) unity
        let mut f = [0.5f32, -2.0, 0.25, 1.5];
        r.process_float(&mut f);
        assert!((f[0] - 0.5).abs() < 0.01 && f[1] <= -0.99 && f[3] <= 1.0);
    }
}
