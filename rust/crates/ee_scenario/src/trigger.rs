//! A trigger's firing (`EESMTrigger::Evaluate`): its AND/OR chain of conditions (`EESMTConditionWrapper::Evaluate`),
//! the duration the chain must hold, and the effects it schedules (all, or one random OR-separated group), as
//! outputs the C++ applies (`AddTimedEffect` in order, the trigger's state).

use crate::ftol;
use crate::host::*;

/// `kSMTriggerOperator*`.
pub const EE_SCENARIO_OPERATOR_END: i32 = 0;
pub const EE_SCENARIO_OPERATOR_AND: i32 = 1;
pub const EE_SCENARIO_OPERATOR_OR: i32 = 2;

/// One condition entry (`EESMTConditionWrapper`): the condition index, the operator after it, the negation.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioConditionEntry {
    pub condition: u32,
    pub op: i32,
    pub is_not: bool,
}

/// One effect entry (`EESMTEffectWrapper`): the operator after it, the delay in seconds, the effect index.
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioEffectEntry {
    pub op: i32,
    pub delay: u32,
    pub effect: u32,
}

/// A trigger's state (in and out).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioTriggerState {
    pub is_on: bool,
    pub has_fired: bool,
    pub is_looping: bool,
    pub is_in_cinematic: bool,
    pub condition_duration: u32,
    pub condition_start_time: u32,
}

/// One timed effect the trigger schedules (`AddTimedEffect(effect, time, the trigger's cinematic flag)`).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeScenarioTimedEffect {
    pub effect: u32,
    pub time: u32,
}

/// `EESMTConditionWrapper::Evaluate`: the entry's condition (negated if set); `next` whether the chain goes on (AND:
/// the result, OR: its negation, end: never; another operator leaves it).
pub fn entry<H: Host>(host: &mut H, e: &EeScenarioConditionEntry, next: &mut bool) -> bool {
    let mut result = host.read(EE_SCENARIO_READ_CONDITION, e.condition as i32, 0) == 1;
    if e.is_not {
        result = !result;
    }
    match e.op {
        EE_SCENARIO_OPERATOR_OR => *next = !result,
        EE_SCENARIO_OPERATOR_AND => *next = result,
        EE_SCENARIO_OPERATOR_END => *next = false,
        _ => {}
    }
    result
}

/// `EESMTrigger::Evaluate(inTime)`: the new state, and the effects to schedule in order.
pub fn evaluate<H: Host>(
    host: &mut H,
    state: &mut EeScenarioTriggerState,
    in_world_cinematic: bool,
    conditions: &[EeScenarioConditionEntry],
    effects: &[EeScenarioEffectEntry],
    time: f32,
    out: &mut Vec<EeScenarioTimedEffect>,
) {
    if !state.is_on || (in_world_cinematic && !state.is_in_cinematic) {
        return;
    }
    let count = conditions.len();
    let mut next = true;
    let mut result = false;
    let mut i = 0;
    while i < count && next {
        result = entry(host, &conditions[i], &mut next);
        if !next && conditions[i].op == EE_SCENARIO_OPERATOR_AND {
            // a false AND: on after the next OR entry (the loop's increment skips that entry itself)
            if let Some(j) = (i + 1..count).find(|&j| conditions[j].op == EE_SCENARIO_OPERATOR_OR) {
                i = j;
                next = true;
            }
        }
        i += 1;
    }
    if !result {
        state.condition_start_time = 0;
        return;
    }
    if state.condition_duration != 0 {
        if state.condition_start_time == 0 {
            state.condition_start_time = ftol(time) as u32;
            return;
        }
        if state.condition_duration as f32 > (time - state.condition_start_time as f32) * 0.001 {
            return;
        }
    }
    state.has_fired = true;
    state.condition_start_time = 0;
    if !state.is_looping {
        state.is_on = false;
    }
    let groups = effects.iter().filter(|e| e.op == EE_SCENARIO_OPERATOR_OR).count() as i32;
    let chosen = if groups > 0 { host.read(EE_SCENARIO_READ_RANDOM, 0, groups) as i32 } else { 0 };
    let mut group = 0i32;
    for e in effects {
        if group == chosen {
            out.push(EeScenarioTimedEffect { effect: e.effect, time: ftol(e.delay as f32 * 1000.0 + time) as u32 });
        }
        if e.op == EE_SCENARIO_OPERATOR_OR {
            group += 1;
        }
        if group > chosen {
            break;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Conditions by index with fixed results; a random stream; the reads in order.
    struct World {
        results: Vec<i64>,
        draws: Vec<i64>,
        log: Vec<(u32, i32, i32)>,
    }

    impl Host for World {
        fn read4(&mut self, op: u32, a: i32, b: i32, _c: i32, _d: i32) -> i64 {
            self.log.push((op, a, b));
            match op {
                EE_SCENARIO_READ_CONDITION => *self.results.get(a as usize).unwrap_or(&-1),
                EE_SCENARIO_READ_RANDOM => self.draws.remove(0),
                _ => panic!("unexpected read {op}"),
            }
        }
    }

    fn c(condition: u32, op: i32) -> EeScenarioConditionEntry {
        EeScenarioConditionEntry { condition, op, is_not: false }
    }

    fn e(op: i32, delay: u32, effect: u32) -> EeScenarioEffectEntry {
        EeScenarioEffectEntry { op, delay, effect }
    }

    fn on() -> EeScenarioTriggerState {
        EeScenarioTriggerState { is_on: true, ..EeScenarioTriggerState::default() }
    }

    fn evaluated(results: &[i64], conditions: &[EeScenarioConditionEntry]) -> (bool, Vec<i32>) {
        let mut w = World { results: results.to_vec(), draws: vec![], log: vec![] };
        let mut s = on();
        let mut out = vec![];
        evaluate(&mut w, &mut s, false, conditions, &[e(0, 0, 4)], 1000.0, &mut out);
        (s.has_fired, w.log.iter().map(|r| r.1).collect())
    }

    #[test]
    fn the_chain_skips_a_false_and_group_to_after_the_next_or() {
        // (0 AND 1) OR (2 AND 3) END: 0 false skips to after entry 1 (the OR), so 2 and 3 decide
        let chain = [c(0, 1), c(1, 2), c(2, 1), c(3, 0)];
        assert_eq!(evaluated(&[0, 1, 1, 1], &chain), (true, vec![0, 2, 3]));
        assert_eq!(evaluated(&[0, 1, 1, 0], &chain), (false, vec![0, 2, 3]));
        // a true OR entry ends the chain at once
        assert_eq!(evaluated(&[1, 1, 0, 0], &chain), (true, vec![0, 1]));
        // a false AND without an OR after it ends the chain false
        let all = [c(0, 1), c(1, 1), c(2, 0)];
        assert_eq!(evaluated(&[1, 0, 1], &all), (false, vec![0, 1]));
        // a missing condition is false; a negated one true
        let mut not = c(9, 0);
        not.is_not = true;
        assert_eq!(evaluated(&[], &[not]), (true, vec![9]));
    }

    #[test]
    fn an_unknown_operator_leaves_the_chain_going() {
        assert_eq!(evaluated(&[0, 1], &[c(0, 7), c(1, 0)]), (true, vec![0, 1]));
    }

    #[test]
    fn the_duration_starts_then_waits_then_fires() {
        let mut w = World { results: vec![1], draws: vec![], log: vec![] };
        let mut s = on();
        s.condition_duration = 2;
        let mut out = vec![];
        let chain = [c(0, 0)];
        let fx = [e(0, 3, 5)];
        evaluate(&mut w, &mut s, false, &chain, &fx, 1500.7, &mut out);
        assert_eq!((s.condition_start_time, s.has_fired, out.len()), (1500, false, 0));
        evaluate(&mut w, &mut s, false, &chain, &fx, 3499.0, &mut out);
        assert_eq!((s.has_fired, out.len()), (false, 0));
        evaluate(&mut w, &mut s, false, &chain, &fx, 3500.0, &mut out);
        assert_eq!((s.has_fired, s.is_on, s.condition_start_time), (true, false, 0));
        assert_eq!(out, vec![EeScenarioTimedEffect { effect: 5, time: 6500 }]);
        // a false chain clears the start time; an off trigger reads nothing
        let mut s = on();
        s.condition_start_time = 77;
        let mut w = World { results: vec![0], draws: vec![], log: vec![] };
        evaluate(&mut w, &mut s, false, &chain, &fx, 0.0, &mut out);
        assert_eq!(s.condition_start_time, 0);
        s.is_on = false;
        evaluate(&mut w, &mut s, false, &chain, &fx, 0.0, &mut out);
        assert_eq!(w.log.len(), 1);
        // a cinematic skips the triggers outside it
        s.is_on = true;
        evaluate(&mut w, &mut s, true, &chain, &fx, 0.0, &mut out);
        assert_eq!(w.log.len(), 1);
    }

    #[test]
    fn or_groups_of_effects_take_one_random_group() {
        let fx = [e(1, 0, 1), e(2, 0, 2), e(1, 1, 3), e(2, 0, 4), e(0, 2, 5)];
        for (draw, want) in [(0, vec![1, 2]), (1, vec![3, 4]), (2, vec![5])] {
            let mut w = World { results: vec![1], draws: vec![draw], log: vec![] };
            let mut s = on();
            s.is_looping = true;
            let mut out = vec![];
            evaluate(&mut w, &mut s, false, &[c(0, 0)], &fx, 10.0, &mut out);
            assert_eq!(out.iter().map(|t| t.effect).collect::<Vec<_>>(), want);
            assert_eq!(w.log.last(), Some(&(EE_SCENARIO_READ_RANDOM, 0, 2)));
            assert!(s.is_on && s.has_fired);
        }
        // a draw past the groups schedules nothing (the original's inclusive range)
        let mut w = World { results: vec![1], draws: vec![3], log: vec![] };
        let mut out = vec![];
        evaluate(&mut w, &mut on(), false, &[c(0, 0)], &fx, 10.0, &mut out);
        assert!(out.is_empty());
    }
}
