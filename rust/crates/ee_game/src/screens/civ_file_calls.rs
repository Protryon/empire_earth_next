//! `EECBCivilizationFiles.cpp`'s host whole in Rust over one-statement shims (UI lane round 11): the old
//! `EECivFileCall`, the C++ side of a civilization file's application to a player and of the server's grant of an
//! upgrade request (`ee_sim::tech::civ_file`): the file's load, the name and upgrades read, the player's
//! civilization reset and free buys, the checks, the grant's message and the refusal's chat lines. `ctx` is the
//! C++ `EECivFileContext` (the file, the player, the name, the upgrades).

use core::ffi::{c_char, c_ulong};

use ee_sim::tech::civ_file::*;

use crate::screens::game_cpp::Cpp;

/// One call of the civilization file host on the context `ctx`: its reply in `r`.
pub(crate) fn host<C: Cpp + ?Sized>(c: &mut C, ctx: usize, call: &EeTechCivFileCall, r: &mut EeTechCivFileReply) {
    match call.op {
        EE_TECH_CIVF_OP_LOAD => r.value = c.civf_load(ctx, call.a != 0),
        EE_TECH_CIVF_OP_STATE => {
            r.text = c.civf_name(ctx) as *const c_char;
            r.characters = c.civf_name_characters(ctx);
            r.upgrades = c.civf_upgrades(ctx) as *const u32;
            r.count = c.civf_upgrade_count(ctx);
        }
        EE_TECH_CIVF_OP_NAME_STRING => c.civf_name_string(ctx, call.a),
        EE_TECH_CIVF_OP_CIV_RESET => {
            c.civf_civ_clear(ctx);
            c.civf_civ_points_to_max(ctx);
            c.civf_civ_set_points_and_max(ctx, 0);
            c.civf_civ_set_name(ctx);
        }
        EE_TECH_CIVF_OP_CIV_BUY_FREE => {
            c.civf_civ_can_afford(ctx, call.a as u32);
            let cost = c.civf_cost() as c_ulong;
            let max = c.civf_civ_max_points(ctx) as c_ulong;
            c.civf_civ_set_points_and_max(ctx, max.wrapping_add(cost) as u64);
            c.civf_civ_buy(ctx, call.a as u32);
        }
        EE_TECH_CIVF_OP_CIV_DONE => c.civf_civ_set_points(ctx),
        EE_TECH_CIVF_OP_CAN_BUY => r.value = c.civf_can_buy() as i64,
        EE_TECH_CIVF_OP_PURCHASED => r.value = c.civf_purchased(ctx) as i64,
        EE_TECH_CIVF_OP_ANY_CIV => {
            r.value = (c.civf_custom_civs() || c.civf_scenario_game() || c.civf_in_scenario_editor()) as i64
        }
        EE_TECH_CIVF_OP_CAN_AFFORD_ALL => r.value = c.civf_can_afford_all(ctx) as i64,
        EE_TECH_CIVF_OP_GRANT => {
            let message = c.civf_grant_new(ctx, call.a);
            if c.civf_multiplayer() {
                c.civf_send(message);
            } else {
                c.civf_execute(message);
                c.civf_release(message);
            }
        }
        // the chat line naming the refused civilization, then the original's random draw
        EE_TECH_CIVF_OP_REFUSE => {
            c.civf_strings_clear();
            c.civf_text_clear();
            c.civf_refuse_format();
            c.civf_refuse_print(ctx);
            c.civf_chat_type();
            c.civf_chat();
            c.civf_random();
        }
        _ => {}
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::screens::game_cpp::Fake;

    fn call(op: u32, a: i64) -> EeTechCivFileCall {
        EeTechCivFileCall { op, a }
    }

    #[test]
    fn a_free_buy_raises_the_maximum_by_the_cost() {
        let mut fake = Fake::default();
        fake.answer("civf_cost", &[30]).answer("civf_civ_max_points", &[100]);
        host(&mut fake, 7, &call(EE_TECH_CIVF_OP_CIV_BUY_FREE, 0x20003), &mut EeTechCivFileReply::default());
        assert_eq!(
            fake.calls(),
            "civf_civ_can_afford(7, 131075)\ncivf_cost()\ncivf_civ_max_points(7)\ncivf_civ_set_points_and_max(7, 130)\n\
             civf_civ_buy(7, 131075)"
        );
    }

    #[test]
    fn the_state_hands_the_contexts_name_and_upgrades() {
        let mut fake = Fake::default();
        fake.answer("civf_name", &[0x1000]).answer("civf_name_characters", &[4]).answer("civf_upgrade_count", &[2]);
        let mut r = EeTechCivFileReply::default();
        host(&mut fake, 7, &call(EE_TECH_CIVF_OP_STATE, 0), &mut r);
        assert_eq!((r.text as usize, r.characters, r.count), (0x1000, 4, 2));
    }

    #[test]
    fn any_civ_and_the_grant_alone() {
        let mut fake = Fake::default();
        fake.answer("civf_scenario_game", &[1]).answer("civf_grant_new", &[5]);
        let mut r = EeTechCivFileReply::default();
        host(&mut fake, 7, &call(EE_TECH_CIVF_OP_ANY_CIV, 0), &mut r);
        assert_eq!(r.value, 1);
        host(&mut fake, 7, &call(EE_TECH_CIVF_OP_GRANT, 9), &mut r);
        assert_eq!(
            fake.calls(),
            "civf_custom_civs()\ncivf_scenario_game()\ncivf_grant_new(7, 9)\ncivf_multiplayer()\ncivf_execute(5)\n\
             civf_release(5)"
        );
    }

    #[test]
    fn the_refusal_chats_then_draws() {
        let mut fake = Fake::default();
        host(&mut fake, 7, &call(EE_TECH_CIVF_OP_REFUSE, 0), &mut EeTechCivFileReply::default());
        assert!(fake.calls().ends_with("civf_chat_type()\ncivf_chat()\ncivf_random()"));
    }
}
