//! The driver's unit tests: the options, the lobby's choices, the samples' counts and the report's text (the
//! formats tools/sim.py and the harness read).

use super::driver::{count_unit, is_building_category, SampleUnit};
use super::lobby::{lobby_options, lower, resolve};
use super::options::*;
use super::report::*;

fn s(v: &[&str]) -> Vec<String> {
    v.iter().map(|x| x.to_string()).collect()
}

#[test]
fn the_c_library_numbers() {
    assert_eq!(c_atol("  42xyz"), 42);
    assert_eq!(c_atol("-7"), -7);
    assert_eq!(c_atol("abc"), 0);
    assert_eq!(c_atol("+3"), 3);
    assert_eq!(c_strtoul0("0x1f"), 31);
    assert_eq!(c_strtoul0("017"), 15);
    assert_eq!(c_strtoul0("12"), 12);
    assert_eq!(c_strtoul0("0"), 0);
    assert_eq!(c_strtoul0("-1"), std::os::raw::c_ulong::MAX);
    assert_eq!(c_atof("1.5:3"), 1.5);
    assert_eq!(c_atof("2700"), 2700.0);
    assert_eq!(c_atof(".5"), 0.5);
    assert_eq!(c_atof("3."), 3.0);
    assert_eq!(c_atof("1e3x"), 1000.0);
    assert_eq!(c_atof("1e"), 1.0);
    assert_eq!(c_atof("x"), 0.0);
}

#[test]
fn the_game_options() {
    let mut o = Options::default();
    let args =
        s(&["--seed", "5", "--ai", "3", "--lobby", "183=#5", "--flare", "90.5:2", "--keep-human", "--human-defeat"]);
    let mut i = 0;
    while i < args.len() {
        assert_eq!(game_option(&args, &mut i, &mut o, &mut |_| {}), Taken::Yes);
        i += 1;
    }
    assert_eq!((o.seed, o.computers), (5, 3));
    assert_eq!(o.lobby, vec![(183, "#5".to_string())]);
    assert_eq!((o.flare_ms, o.flare_player), (90500, 2));
    assert!(!o.human_out && !o.no_human_defeat);
    let mut i = 0;
    assert_eq!(
        game_option(&s(&["--lobby", "183"]), &mut i, &mut o, &mut |_| {}),
        Taken::Bad("--lobby CONTROL=VALUE".into())
    );
    let mut i = 0;
    assert_eq!(game_option(&s(&["--seed"]), &mut i, &mut o, &mut |_| {}), Taken::Bad("--seed needs a value".into()));
    let mut i = 0;
    assert_eq!(game_option(&s(&["--quiet"]), &mut i, &mut o, &mut |_| {}), Taken::No);
    let mut i = 0;
    assert_eq!(
        game_option(&s(&["--volcano", "12"]), &mut i, &mut o, &mut |_| {}),
        Taken::Bad("--volcano SECONDS:PLAYER".into())
    );
}

#[test]
fn a_games_line() {
    assert_eq!(
        words(r#"--seed 3  --id "a b" --map-type \"x"#),
        s(&["--seed", "3", "--id", "a b", "--map-type", "\"x"])
    );
    assert_eq!(words("  \t\r\n"), Vec::<String>::new());
    let mut base = Options { report: "r".into(), trace: "t".into(), sample_seconds: 10.0, ..Options::default() };
    base.cheats = "none".into();
    let g = game_of_line("--seed 9 --ai 4", 3, &base, &mut |_| {}).unwrap().unwrap();
    assert_eq!((g.seed, g.computers, g.id.as_str(), g.report.as_str()), (9, 4, "3", ""));
    assert_eq!(g.cheats, "none");
    assert_eq!(g.sample_seconds, 10.0);
    assert_eq!(game_of_line("# a comment", 0, &base, &mut |_| {}), Ok(None));
    assert_eq!(game_of_line("", 0, &base, &mut |_| {}), Ok(None));
    assert_eq!(game_of_line("--ai 9", 0, &base, &mut |_| {}), Err("an option is out of range".into()));
    assert_eq!(
        game_of_line("--pt-rate 9", 0, &base, &mut |_| {}),
        Err("--pt-rate is not a game's option (--help)".into())
    );
    let mut made = Vec::new();
    let g = game_of_line("--out dir", 0, &base, &mut |d| made.push(d.to_string())).unwrap().unwrap();
    assert_eq!(made.len(), 1);
    assert!(g.report.ends_with("report.json") && g.trace.ends_with("trace.txt") && g.err.ends_with("err.txt"));
}

#[test]
fn the_command_line() {
    let mut o = Options::default();
    let mut p = Process::default();
    let mut sets = Vec::new();
    let args = s(&[
        "--seed",
        "3",
        "--pt-rate",
        "50",
        "--set",
        "a.b=1",
        "--save-at",
        "900=b",
        "--save-at",
        "100=a",
        "--dump-at",
        "5=x",
        "--ai-trace",
        "--quiet",
        "--effect-sweep",
        "--campaign",
        "roman",
    ]);
    let c = parse_command_line(
        &args,
        &mut o,
        &mut p,
        &mut |v| {
            sets.push(v.to_string());
            Ok(())
        },
        &mut |_| {},
    );
    assert_eq!(c, Command::Run);
    assert_eq!((o.seed, o.pt_rate), (3, 50));
    assert_eq!(o.sets, vec!["a.b=1".to_string()]);
    assert_eq!(sets, vec!["a.b=1".to_string()]);
    assert_eq!(o.saves.iter().map(|s| s.0).collect::<Vec<_>>(), vec![100, 900]);
    assert_eq!(p.dumps.len(), 1);
    assert!(p.ai_trace && o.quiet && o.effect_sweep);
    assert_eq!(o.campaign, "roman");
    let bad = |v: &[&str]| {
        parse_command_line(&s(v), &mut Options::default(), &mut Process::default(), &mut |_| Ok(()), &mut |_| {})
    };
    assert_eq!(bad(&["--help"]), Command::Help);
    assert_eq!(bad(&["--nope"]), Command::Bad("unknown option --nope (--help)".into(), 64));
    assert_eq!(bad(&["--campaign", "x"]), Command::Bad("--campaign learning|roman|pacific|asian".into(), 64));
    assert_eq!(bad(&["--save-at", "=x"]), Command::Bad("--save-at MS=FILE".into(), 64));
    assert_eq!(bad(&["--pt-rate", "0"]), Command::Bad("an option is out of range (--help)".into(), 64));
    assert_eq!(bad(&["--data"]), Command::Bad("--data needs a value".into(), 64));
    let c = parse_command_line(
        &s(&["--set", "q"]),
        &mut Options::default(),
        &mut Process::default(),
        &mut |_| Err("no such key".into()),
        &mut |_| {},
    );
    assert_eq!(c, Command::Bad("--set no such key".into(), 64));
}

#[test]
fn a_line_by_name() {
    let lines = s(&["Small", "Medium", "Large", "Small Islands"]);
    assert_eq!(resolve("#2", &lines, "x"), Ok(2));
    assert_eq!(resolve(" SMALL ", &lines, "x"), Ok(0));
    assert_eq!(resolve("med", &lines, "x"), Ok(1));
    assert_eq!(resolve("isl", &lines, "x"), Ok(3));
    assert_eq!(resolve("#9", &lines, "x"), Err("x: index #9 outside the combo's 4 lines".into()));
    assert_eq!(resolve("a", &lines, "x"), Err("x: 'a' matches 3 of the lines Small|Medium|Large|Small Islands".into()));
    assert_eq!(lower(" \tAb C\t"), "ab c");
}

#[test]
fn the_lobby_choices() {
    let o = Options {
        computers: 2,
        map_type: "Islands".into(),
        engine: "boom,classic".into(),
        lobby: vec![(62, "1".into()), (300, "y".into())],
        ..Options::default()
    };
    let l = lobby_options(&o, &mut |name| Ok(if name == "boom" { 3 } else { 0 })).unwrap();
    let get = |c| l.iter().find(|x| x.0 == c).map(|x| x.1.as_str());
    assert_eq!(get(177), Some("Small"));
    assert_eq!(get(179), Some("Islands"));
    assert_eq!(get(207), Some("Hard"));
    assert_eq!(get(62), Some("1"));
    assert_eq!(get(63), Some("-"));
    assert_eq!(get(128), Some("5"));
    assert_eq!(get(129), None);
    assert_eq!(get(276), Some("#3"));
    assert_eq!(get(277), Some("#0"));
    assert_eq!(get(300), Some("y"));
    // the order: the defaults, the --lobby boxes, the teams and citizens by slot, the engines
    assert_eq!((l[11].0, l[12].0), (62, 300));
    assert_eq!((l[13].0, l[14].0), (126, 63));
    assert_eq!(lobby_options(&o, &mut |_| Err("no".into())), Err("no".into()));
}

#[test]
fn a_sample_counts_units() {
    let mut p = PlayerSample::default();
    let unit =
        |category, family, wall, tower, attack| SampleUnit { category, family, wall, tower, hit_points: 10, attack };
    assert!(!count_unit(&mut p, &unit(9, 0, false, false, Some(1.0)), 50));
    assert!(count_unit(&mut p, &unit(6, 0, false, false, None), 50));
    assert!(count_unit(&mut p, &unit(2, 0x25, true, true, None), 100));
    assert!(count_unit(&mut p, &unit(0x19, 0, true, false, None), 10));
    assert!(count_unit(&mut p, &unit(4, 0, false, false, Some(2.5)), 1));
    assert!(count_unit(&mut p, &unit(7, 0, false, false, None), 1));
    assert_eq!((p.citizens, p.buildings, p.towers, p.walls), (1, 2, 1, 1));
    assert_eq!((p.army, p.naval, p.air, p.strength, p.value), (2, 1, 1, 25, 162));
    assert!(is_building_category(0x20) && !is_building_category(3));
}

#[test]
fn the_report() {
    assert_eq!(json_string("a\"b\\c\n\u{7f}"), "\"a\\\"b\\\\c\\u000a\\u007f\"");
    let o = Options { sets: vec!["x=1".into()], ..Options::default() };
    let p1 = PlayerSample { index: 1, human: true, epoch: 1, victory_units: 3, ..PlayerSample::default() };
    let p2 = PlayerSample {
        index: 2,
        epoch: 2,
        victory_units: 5,
        result: 1,
        resources: [1, 2, 3, 4, 5],
        ..PlayerSample::default()
    };
    let samples = vec![
        Sample { game_ms: 0, pt: 0, wall: 0.0, players: vec![p1.clone()] },
        Sample { game_ms: 30000, pt: 600, wall: 1.234, players: vec![p1, p2] },
    ];
    let chosen =
        vec![Chosen { control: 30, label: "slot 0 human".into(), value: "#0".into(), line: "Human".into(), index: 0 }];
    let text = report(&ReportInput {
        options: &o,
        chosen: &chosen,
        outcome: "last-computer-standing",
        error: "",
        wall_seconds: 1.005,
        wall_seconds_game: None,
        phases: &[("startup".into(), 0.5)],
        no_human_defeat_hits: 0,
        samples: &samples,
        defeats: &[Defeat { player: 3, game_ms: 5, pt: 7 }],
        epoch_ms: &[vec![], vec![0, 100], vec![]],
        exit_code: 0,
    });
    let want = "{\n  \"test_only\": true,\n  \"kind\": \"ee-sim-v1\",\n  \"setup\": {\"seed\": 2, \"computers\": 2, \"game_seconds\": 2700, \"pt_rate\": 40, \"no_human_defeat\": true, \"human_out\": true, \"cheats\": \"all\", \"personality\": \"\", \"engine\": \"classic\", \"sets\": [\"x=1\"],\n    \"lobby\": [{\"control\": 30, \"value\": \"#0\", \"index\": 0, \"line\": \"Human\"}]},\n  \"outcome\": \"last-computer-standing\",\n";
    assert!(text.starts_with(want), "{text}");
    assert!(text.contains("  \"wall_seconds_game\": 0.00,\n  \"wall_phases\": {\"startup\": 0.50},\n  \"game_ms\": 30000,\n  \"pt\": 600,\n"));
    assert!(text.contains("  \"alive_computers\": [2],\n  \"winners\": [2],\n  \"winner\": 2,\n  \"defeat_order\": [{\"player\": 3, \"game_ms\": 5, \"pt\": 7}],\n"));
    assert!(text.contains("\"epoch_ms\": {\"1\": [0, 100]},\n"));
    assert!(text.contains("{\"index\": 2, \"human\": false, \"result\": 1, \"epoch\": 2, \"population\": 0, \"victory_units\": 5, \"citizens\": 0, \"army\": 0, \"buildings\": 0, \"naval\": 0, \"air\": 0, \"towers\": 0, \"walls\": 0, \"wonders\": 0, \"strength\": 0, \"value\": 0, \"resources\": {\"food\": 1, \"wood\": 2, \"stone\": 3, \"gold\": 4, \"iron\": 5}}"));
    assert!(text.contains("    {\"game_ms\": 30000, \"pt\": 600, \"wall\": 1.23, \"players\": [{"));
    assert!(text.ends_with("}]}\n  ],\n  \"exit_code\": 0\n}\n"));
    let failed = report(&ReportInput {
        error: "boom",
        outcome: "",
        samples: &[],
        ..ReportInput {
            options: &o,
            chosen: &[],
            outcome: "",
            error: "",
            wall_seconds: 0.0,
            wall_seconds_game: Some(2.0),
            phases: &[],
            no_human_defeat_hits: 4,
            samples: &[],
            defeats: &[],
            epoch_ms: &[],
            exit_code: 2,
        }
    });
    assert!(failed.contains("  \"outcome\": \"error\",\n  \"error\": \"boom\",\n"));
    assert!(
        failed.contains("\"players\": [],\n")
            && failed.contains("\"winner\": null,")
            && failed.contains("\"no_human_defeat_hits\": 4,")
    );
    assert_eq!(sample_line(&samples[1]), " p1 e1 c0 a0 b0 p2 e2 c0 a0 b0");
}

#[test]
fn a_text_file_writes_the_c_librarys_new_lines() {
    let dir = std::env::temp_dir().join(format!("ee_sim_driver_text_{}", std::process::id()));
    let _ = std::fs::create_dir_all(&dir);
    let path = dir.join("t.txt");
    let mut f = super::driver::TextFile::create(path.to_str().unwrap()).unwrap();
    f.write_all(b"a\nb\r\nc").unwrap();
    f.flush().unwrap();
    drop(f);
    let want: &[u8] = if cfg!(windows) { b"a\r\nb\r\r\nc" } else { b"a\nb\r\nc" };
    assert_eq!(std::fs::read(&path).unwrap(), want);
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn an_editor_script_runs_line_by_line() {
    use super::editor::{lines, run, words, Step};
    assert_eq!(words(" tab\t2\r"), s(&["tab", "2"]));
    assert_eq!(lines(b"a\nb\r\n\nc"), s(&["a", "b\r", "", "c"]));
    assert_eq!(lines(b"a\n"), s(&["a"]));
    let dir = std::env::temp_dir().join(format!("ee_sim_editor_{}", std::process::id()));
    let _ = std::fs::create_dir_all(&dir);
    let path = dir.join("s.txt");
    std::fs::write(&path, b"tab 1 # the map\n\n# nothing\ndigest x\nnope\n").unwrap();
    let path = path.to_str().unwrap().to_string();
    let mut seen = Vec::new();
    let mut out = Vec::new();
    let r = run(
        &path,
        &mut |w| {
            seen.push(w.to_vec());
            match w[0].as_str() {
                "tab" => Step::Done(vec![]),
                "digest" => Step::Done(vec!["  map none".into()]),
                _ => Step::Unknown,
            }
        },
        &mut |b| out.extend_from_slice(b),
    );
    assert_eq!(r, Err((2, format!("{path}:5: unknown command nope"))));
    assert_eq!(seen, vec![s(&["tab", "1"]), s(&["digest", "x"]), s(&["nope"])]);
    assert_eq!(String::from_utf8(out).unwrap(), "step 1 tab 1 # the map\nstep 4 digest x\n  map none\n");
    let r = run(&path, &mut |_| Step::Unfinished, &mut |_| {});
    assert_eq!(r, Err((3, format!("{path}:1: 'tab' did not finish"))));
    assert_eq!(
        run("/no/such/script", &mut |_| Step::Unknown, &mut |_| {}),
        Err((2, "cannot read /no/such/script".into()))
    );
    let _ = std::fs::remove_dir_all(&dir);
}
