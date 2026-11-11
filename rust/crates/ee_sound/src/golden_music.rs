//! The music manager's golden scenarios: `golden_music.txt` is what the real C++ `EEMusicManager` (the unit as it
//! was before the move) printed when its thread function ran over fakes: the events' wait results, the time, the
//! random generator, the songs, the conductor and the preferences, with other threads' `SetIntensity` calls and
//! player changes made inside the wait. The fakes printed every answer they gave ("name args -> answer"); here the
//! fake host takes the same answers from the golden text, the thread's loop runs as the C++ one does around
//! [`music::step`], and the whole text must come out the same.

use std::collections::BTreeMap;
use std::ffi::c_void;
use std::fmt::Write;

use crate::conductor::Score;
use crate::music::{self, Host, Song};

struct Fake<'g> {
    golden: Vec<&'g str>,
    out: Vec<String>,
    music: Option<Song>,
    score_id: i32,
    state: i32,
    intensity_delay: u32,
    intensity: u32,
    time_to_next_song: u32,
    requested_intensity: u32,
    volume: f32,
    music_volume: f32,
    epoch: i32,
    techs: bool,
    player: bool,
    song_ids: BTreeMap<usize, i32>,
}

fn song_label(s: Option<Song>) -> usize {
    s.map_or(0, |s| s.0 as usize)
}

impl<'g> Fake<'g> {
    fn print(&mut self, s: String) {
        self.out.push(s);
    }
    /// The golden's answer to the call printed as `prefix -> answer` at this point.
    fn answer(&mut self, prefix: &str) -> String {
        let want = self.golden.get(self.out.len()).copied().unwrap_or("");
        let p = format!("{prefix} -> ");
        match want.strip_prefix(&p) {
            Some(a) => {
                self.out.push(want.to_string());
                a.to_string()
            }
            None => panic!("line {}: wanted `{want}`, the Rust asked `{prefix}`", self.out.len() + 1),
        }
    }
    fn next_line(&self) -> &'g str {
        self.golden.get(self.out.len()).copied().unwrap_or("")
    }
}

impl Host for Fake<'_> {
    fn music(&self) -> Option<Song> {
        self.music
    }
    fn set_music(&mut self, s: Option<Song>) {
        self.music = s;
    }
    fn score_id(&self) -> i32 {
        self.score_id
    }
    fn set_score_id(&mut self, id: i32) {
        self.score_id = id;
    }
    fn state(&self) -> i32 {
        self.state
    }
    fn set_state(&mut self, s: i32) {
        self.state = s;
    }
    fn intensity_delay(&self) -> u32 {
        self.intensity_delay
    }
    fn set_intensity_delay(&mut self, d: u32) {
        self.intensity_delay = d;
    }
    fn intensity(&self) -> u32 {
        self.intensity
    }
    fn set_intensity(&mut self, i: u32) {
        self.intensity = i;
    }
    fn time_to_next_song(&self) -> u32 {
        self.time_to_next_song
    }
    fn set_time_to_next_song(&mut self, t: u32) {
        self.time_to_next_song = t;
    }
    fn requested_intensity(&self) -> u32 {
        self.requested_intensity
    }
    fn set_requested_intensity(&mut self, i: u32) {
        self.requested_intensity = i;
    }
    fn random(&mut self) -> f32 {
        f32::from_bits(u32::from_str_radix(&self.answer("random"), 16).unwrap())
    }
    fn request_stop(&mut self) {
        self.print("set stop".into());
    }
    fn last_error(&mut self) {
        self.print("last_error".into());
    }
    fn current_epoch(&mut self) -> Option<i32> {
        (self.player && self.techs).then_some(self.epoch)
    }
    fn music_volume(&self) -> f32 {
        self.music_volume
    }
    fn sleep(&mut self, ms: u32) {
        self.print(format!("sleep {ms}"));
    }
    fn random_song(&mut self, epoch: i32) -> Option<Song> {
        let a = self.answer(&format!("random_song {epoch}"));
        let k: usize = a.strip_prefix('M').unwrap().parse().unwrap();
        Some(Song(k as *mut c_void))
    }
    fn song_load(&mut self, s: Song) {
        let k = song_label(Some(s));
        let id: i32 = self.answer(&format!("load M{k}")).parse().unwrap();
        self.song_ids.insert(k, id);
    }
    fn song_score_id(&self, s: Song) -> i32 {
        self.song_ids[&song_label(Some(s))]
    }
    fn song_unload(&mut self, s: Song) -> i32 {
        self.answer(&format!("unload M{}", song_label(Some(s)))).parse().unwrap()
    }
    fn song_reverb(&self, s: Song) -> [f32; 4] {
        let i = song_label(Some(s)) as f32;
        [0.1f32 * i, 0.25f32 + i, -1.5f32 * i, 1000.0f32 + i]
    }
    fn volume(&self) -> f32 {
        self.volume
    }
    fn set_volume(&mut self, v: f32) {
        self.print(format!("volume {:08x}", v.to_bits()));
        self.volume = v;
    }
    fn stop(&mut self) {
        self.print("stop".into());
    }
    fn score(&mut self, id: i32) -> Option<Score> {
        match self.answer(&format!("get_score {id}")).as_str() {
            "S" => Some(Score((id as isize + 1) as *mut c_void)),
            "none" => None,
            a => panic!("get_score {a}"),
        }
    }
    fn set_repeat_count(&mut self, s: Score, count: u32) {
        self.print(format!("repeat S{} {count}", s.0 as isize - 1));
    }
    fn conductor_set_intensity(&mut self, intensity: u32, immediate: bool) {
        self.print(format!("intensity {intensity} {}", immediate as i32));
    }
    fn set_reverb_style(&mut self, r: [f32; 4]) {
        self.print(format!(
            "reverb {:08x} {:08x} {:08x} {:08x}",
            r[0].to_bits(),
            r[1].to_bits(),
            r[2].to_bits(),
            r[3].to_bits()
        ));
    }
    fn play(&mut self, id: i32) {
        self.print(format!("play {id}"));
    }
    fn transition_to(&mut self, id: i32, command: u32, flags: u32) {
        self.print(format!("transition {id} {command} {flags}"));
    }
    fn global_set_intensity(&mut self, intensity: u32, immediate: bool) {
        self.conductor_set_intensity(intensity, immediate);
    }
}

fn field<'a>(t: &[&'a str], name: &str) -> &'a str {
    t[t.iter().position(|w| *w == name).unwrap() + 1]
}

/// `MusicManagerMain` around `music::step`, as the C++ loop runs it.
fn thread(h: &mut Fake) {
    let time = |h: &mut Fake| -> u32 { h.answer("time").parse().unwrap() };
    let mut last = time(h);
    h.print("set created".into());
    loop {
        // the other threads, inside the wait
        while let Some(c) = h.next_line().strip_prefix("call ") {
            h.print(format!("call {c}"));
            let t: Vec<&str> = c.split(' ').collect();
            let v: i32 = t[1].parse().unwrap();
            match t[0] {
                "intensity" => {
                    music::request_intensity(h, v as u32);
                    h.print("set intensity".into());
                }
                "epoch" => h.epoch = v,
                "player" => {
                    h.techs = v != 0;
                    h.player = v != 1;
                }
                k => panic!("call {k}"),
            }
        }
        let wait: i32 = h.answer(&format!("wait 5 {}", h.time_to_next_song)).parse().unwrap();
        let now = time(h);
        let elapsed = now.wrapping_sub(last);
        last = now;
        if music::step(h, wait, elapsed) {
            break;
        }
    }
    h.print("set done".into());
}

#[test]
fn music_manager_equals_the_cpp() {
    let text = include_str!("golden_music.txt");
    let golden: Vec<&str> = text.lines().collect();
    let mut scenarios = 0;
    let mut f = Fake {
        golden: golden.clone(),
        out: Vec::new(),
        music: None,
        score_id: 0,
        state: 0,
        intensity_delay: 0,
        intensity: 0,
        time_to_next_song: 600000,
        requested_intensity: 0,
        volume: 0.0,
        music_volume: 0.0,
        epoch: 0,
        techs: true,
        player: true,
        song_ids: BTreeMap::new(),
    };
    while f.out.len() < golden.len() {
        let line = f.next_line();
        let t: Vec<&str> = line.strip_prefix("> scenario ").expect(line).split(' ').collect();
        f.print(line.to_string());
        f.music = None;
        f.score_id = 0;
        f.state = 0;
        f.intensity_delay = 0;
        f.intensity = 0;
        f.time_to_next_song = 600000;
        f.requested_intensity = 0;
        f.volume = f32::from_bits(u32::from_str_radix(field(&t, "cvol"), 16).unwrap());
        f.music_volume = f32::from_bits(u32::from_str_radix(field(&t, "mvol"), 16).unwrap());
        f.epoch = field(&t, "epoch").parse().unwrap();
        f.techs = field(&t, "techs") == "1";
        f.player = field(&t, "player") == "1";
        // the songs keep their score ids across scenarios (the C++ fakes' objects do)
        thread(&mut f);
        let mut s = String::new();
        write!(
            s,
            "= state {} music M{} score {} delay {} int {} next {} req {}",
            f.state,
            song_label(f.music),
            f.score_id,
            f.intensity_delay,
            f.intensity,
            f.time_to_next_song,
            f.requested_intensity
        )
        .unwrap();
        f.print(s);
        scenarios += 1;
        let n = f.out.len();
        assert_eq!(f.out[n - 1], golden[n - 1], "line {n}");
    }
    assert_eq!(scenarios, 80);
    for (i, (w, g)) in golden.iter().zip(&f.out).enumerate() {
        assert_eq!(w, g, "line {}", i + 1);
    }
    assert_eq!(golden.len(), f.out.len());
}

#[test]
fn a_song_missing_for_the_epoch_stops_instead_of_crashing() {
    struct NoSong<'g>(Fake<'g>);
    let mut f = Fake {
        golden: Vec::new(),
        out: Vec::new(),
        music: None,
        score_id: 0,
        state: 0,
        intensity_delay: 0,
        intensity: 0,
        time_to_next_song: 600000,
        requested_intensity: 0,
        volume: 0.5,
        music_volume: 0.5,
        epoch: 0,
        techs: true,
        player: true,
        song_ids: BTreeMap::new(),
    };
    // a host whose epoch has no songs: DoStart silences and asks to stop
    impl Host for NoSong<'_> {
        fn music(&self) -> Option<Song> {
            self.0.music()
        }
        fn set_music(&mut self, s: Option<Song>) {
            self.0.set_music(s)
        }
        fn score_id(&self) -> i32 {
            self.0.score_id()
        }
        fn set_score_id(&mut self, id: i32) {
            self.0.set_score_id(id)
        }
        fn state(&self) -> i32 {
            self.0.state()
        }
        fn set_state(&mut self, s: i32) {
            self.0.set_state(s)
        }
        fn intensity_delay(&self) -> u32 {
            self.0.intensity_delay()
        }
        fn set_intensity_delay(&mut self, d: u32) {
            self.0.set_intensity_delay(d)
        }
        fn intensity(&self) -> u32 {
            self.0.intensity()
        }
        fn set_intensity(&mut self, i: u32) {
            self.0.set_intensity(i)
        }
        fn time_to_next_song(&self) -> u32 {
            self.0.time_to_next_song()
        }
        fn set_time_to_next_song(&mut self, t: u32) {
            self.0.set_time_to_next_song(t)
        }
        fn requested_intensity(&self) -> u32 {
            self.0.requested_intensity()
        }
        fn set_requested_intensity(&mut self, i: u32) {
            self.0.set_requested_intensity(i)
        }
        fn random(&mut self) -> f32 {
            0.5
        }
        fn request_stop(&mut self) {
            self.0.request_stop()
        }
        fn last_error(&mut self) {}
        fn current_epoch(&mut self) -> Option<i32> {
            Some(7)
        }
        fn music_volume(&self) -> f32 {
            0.5
        }
        fn sleep(&mut self, _: u32) {}
        fn random_song(&mut self, _: i32) -> Option<Song> {
            None
        }
        fn song_load(&mut self, _: Song) {
            unreachable!()
        }
        fn song_score_id(&self, _: Song) -> i32 {
            unreachable!()
        }
        fn song_unload(&mut self, _: Song) -> i32 {
            unreachable!()
        }
        fn song_reverb(&self, _: Song) -> [f32; 4] {
            unreachable!()
        }
        fn volume(&self) -> f32 {
            self.0.volume()
        }
        fn set_volume(&mut self, v: f32) {
            self.0.set_volume(v)
        }
        fn stop(&mut self) {
            self.0.stop()
        }
        fn score(&mut self, _: i32) -> Option<Score> {
            unreachable!()
        }
        fn set_repeat_count(&mut self, _: Score, _: u32) {}
        fn conductor_set_intensity(&mut self, _: u32, _: bool) {}
        fn set_reverb_style(&mut self, _: [f32; 4]) {}
        fn play(&mut self, _: i32) {}
        fn transition_to(&mut self, _: i32, _: u32, _: u32) {}
        fn global_set_intensity(&mut self, _: u32, _: bool) {}
    }
    f.out.clear();
    let mut h = NoSong(f);
    music::do_start(&mut h);
    assert_eq!(h.0.out, ["volume 00000000", "set stop"]);
    assert_eq!(h.0.state, music::STOPPED);
    assert_eq!(h.0.music, None);
    // NextSong when neither draw gives a song (the same "none" twice): only the time is drawn
    h.0.out.clear();
    h.0.state = music::PLAYING;
    music::next_song(&mut h);
    assert_eq!(h.0.state, music::PLAYING);
    assert_eq!(h.0.time_to_next_song, 360000);
}
