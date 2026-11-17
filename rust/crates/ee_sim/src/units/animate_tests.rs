use super::*;

/// A walking state: frames 10..17 (8 frames), 100 ms each, looping, sounds at 12 and 15.
fn walk() -> EeUnitsAnimateIn {
    EeUnitsAnimateIn {
        loops: true,
        state: 1,
        tick_ms: 125,
        frame: 10,
        hold_time: 0,
        start_frame: 10,
        last_frame: 17,
        frame_count: 8,
        frame_time: 100,
        sound1_frame: 12,
        sound2_frame: 15,
        ..Default::default()
    }
}

#[test]
fn a_tick_shorter_than_a_frame_only_moves_the_clock() {
    let i = EeUnitsAnimateIn { tick_ms: 30, time: 40, ..walk() };
    let o = animate(&i);
    assert_eq!((o.set_time, o.time, o.set_frame, o.steps), (true, 70, false, 0));
    assert_eq!(o.fraction, 0.7);
    assert_eq!(o.next_frame, 11);
}

#[test]
fn a_loop_steps_and_wraps() {
    // 125 + 80 ms: two frames, 5 ms left; 16 -> 18 wraps to 10
    let i = EeUnitsAnimateIn { time: 80, frame: 16, ..walk() };
    let o = animate(&i);
    assert_eq!((o.steps, o.set_frame, o.frame, o.time), (2, true, 10, 5));
    assert_eq!(o.sound, 0);
    assert_eq!(o.next_frame, 11);
    assert_eq!(o.fraction, 0.05);
    // onto the first sound frame
    let o = animate(&EeUnitsAnimateIn { frame: 11, ..walk() });
    assert_eq!((o.frame, o.sound), (12, 1));
}

#[test]
fn a_held_loop_pauses_at_its_wrap() {
    let i = EeUnitsAnimateIn { hold_time: 500, frame: 17, ..walk() };
    let o = animate(&i);
    assert_eq!((o.set_paused, o.paused, o.frame, o.time, o.stop_sound), (true, true, 10, 0, true));
    assert_eq!(o.sound, 0);
}

#[test]
fn a_one_shot_ends_on_its_last_frame() {
    let i = EeUnitsAnimateIn { loops: false, frame: 16, tick_ms: 250, ..walk() };
    let o = animate(&i);
    assert_eq!((o.paused, o.frame, o.time, o.stop_sound, o.steps), (true, 17, 0, true, 2));
    // the last frame does not loop back
    assert_eq!(o.next_frame, 17);
    // a foundation keeps its frame
    let o = animate(&EeUnitsAnimateIn { foundation: true, ..i });
    assert!(!o.set_frame && !o.set_time && !o.set_paused);
    assert_eq!((o.steps, o.next_frame), (2, 17));
}

#[test]
fn a_zero_frame_time_steps_once() {
    let i = EeUnitsAnimateIn { frame_time: 0, ..walk() };
    let o = animate(&i);
    assert_eq!((o.steps, o.frame, o.time), (1, 11, 0));
    assert_eq!(o.fraction, 0.0);
}

#[test]
fn speed_scaling_takes_the_larger_of_speed_and_turn() {
    // speed 0.8 of the maximum, turning at 0.25 (raised to 0.5): 100 / 0.8 = 125 ms a frame
    let i = EeUnitsAnimateIn {
        speed_scaled: true,
        velocity: 4.0,
        max_velocity: 5.0,
        yaw_velocity: -0.5,
        max_angular_velocity: 2.0,
        tick_ms: 120,
        ..walk()
    };
    let o = animate(&i);
    assert_eq!((o.set_time, o.time, o.steps), (true, 120, 0));
    assert_eq!(o.fraction, 120.0 / 125.0);
    // standing still and not turning: both 0.5, 200 ms a frame
    let o = animate(&EeUnitsAnimateIn { velocity: 0.0, yaw_velocity: 0.0, tick_ms: 199, ..i });
    assert_eq!((o.time, o.steps), (199, 0));
    // turning fast: 100 / 1.5 = 66.67 -> 67
    let o = animate(&EeUnitsAnimateIn { yaw_velocity: 3.0, tick_ms: 67, ..i });
    assert_eq!((o.steps, o.time), (1, 0));
}

#[test]
fn shared_frames_step_by_two_and_halve_the_fraction() {
    // 6 frames (even) 0..5, looping
    let i = EeUnitsAnimateIn {
        shares_frames: true,
        frame_count: 6,
        start_frame: 0,
        last_frame: 5,
        frame: 0,
        tick_ms: 150,
        sound1_frame: 99,
        sound2_frame: 99,
        ..walk()
    };
    let o = animate(&i);
    // frame 1 (odd offset): next 3, fraction 0.5 * 0.5 + 0.5
    assert_eq!((o.frame, o.next_frame, o.fraction), (1, 3, 0.75));
    // frame 4: next would be 5 = last, so the start (even and looping)
    let o = animate(&EeUnitsAnimateIn { frame: 3, ..i });
    assert_eq!((o.frame, o.next_frame, o.fraction), (4, 0, 0.25));
    // 5 frames (odd): on the last frame the fraction is not halved
    let odd = EeUnitsAnimateIn { frame_count: 5, last_frame: 4, ..i };
    let o = animate(&EeUnitsAnimateIn { frame: 3, ..odd });
    assert_eq!((o.frame, o.next_frame, o.fraction), (4, 0, 0.5));
    // four frames or fewer never share
    let o = animate(&EeUnitsAnimateIn { frame_count: 4, last_frame: 3, ..i });
    assert_eq!((o.frame, o.next_frame, o.fraction), (1, 2, 0.5));
}

#[test]
fn a_paused_animation_waits_out_its_hold() {
    let p = EeUnitsAnimateIn { paused: true, hold_time: 1000, time: 500, ..walk() };
    let o = animate(&p);
    assert_eq!((o.set_time, o.time), (true, 625));
    assert!(!o.set_frame && o.next_frame == 0);
    // the hold over: a loop restarts at its start frame
    let o = animate(&EeUnitsAnimateIn { time: 900, start_frame: 12, ..p });
    assert_eq!((o.set_paused, o.paused, o.frame, o.time, o.sound), (true, false, 12, 0, 1));
    // the idle states pick their next idle (7 always; 8 unless the unit fires over walls)
    for (state, walls, pick) in [(7, false, true), (7, true, true), (8, false, true), (8, true, false)] {
        let o = animate(&EeUnitsAnimateIn { time: 900, state, can_fire_over_walls: walls, ..p });
        assert_eq!(o.idle_pick, pick, "state {state} walls {walls}");
        assert_eq!(o.set_frame, !pick);
    }
    // a one-shot's hold over: its sound stops
    let o = animate(&EeUnitsAnimateIn { time: 900, loops: false, ..p });
    assert_eq!((o.stop_sound, o.set_time, o.steps), (true, false, 0));
}

#[test]
fn the_clock_wraps_unsigned() {
    let i = EeUnitsAnimateIn { time: u32::MAX, tick_ms: 2, ..walk() };
    let o = animate(&i);
    assert_eq!((o.set_time, o.time), (true, 1));
}
