use super::*;

#[test]
fn the_records_layouts_are_the_cpp_classes() {
    // EESETSEffectControls: mLabels[13] +0, mEditBoxes[2] +13, mEditBoxes2[2] +15, mTextEditBox +17,
    // mEditBox48 +18, mComboBoxes[4] +19, mEffectComboBoxes[2] +23, mScenarioComboBoxes[4] +25, mPlayers[3] +29,
    // mRadioButtons[6] +32, mSliders[4] +38, mCheckBoxes[2] +42, mListBoxes[2] +44 (pointers)
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_LABEL, 12), Some(12));
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_LABEL, 13), None);
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_TEXT, 5), Some(17));
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_EDIT48, 0), Some(18));
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_PLAYER, 2), Some(31));
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_LIST, 1), Some(45));
    assert_eq!(slot_index(false, EE_SCREENS_SEET_A_LIST, 1).unwrap() + 1, EE_SCREENS_SEET_EFFECT_SLOTS);
    // EESETSConditionControls: mLabels[5], mEditBoxes[2], mTextEditBox, mComboBoxes[3], mScenarioComboBoxes[4],
    // mCheckBoxes[3], mPlayers[2], mRadioButtons[2], mSlider
    assert_eq!(slot_index(true, EE_SCREENS_SEET_A_CHECK, 2), Some(17));
    assert_eq!(slot_index(true, EE_SCREENS_SEET_A_SLIDER, 3), Some(22));
    assert_eq!(slot_index(true, EE_SCREENS_SEET_A_SLIDER, 0).unwrap() + 1, EE_SCREENS_SEET_CONDITION_SLOTS);
    assert_eq!(slot_index(true, EE_SCREENS_SEET_A_LIST, 0), None);
    assert_eq!(slot_index(true, EE_SCREENS_SEET_A_EDIT2, 0), None);
}

#[test]
fn a_record_starts_empty_and_holds_its_slots() {
    let r = controls_new(false);
    let mut i = [EE_SCREENS_SEET_A_COMBO as i32, 3, 1, 0];
    let mut f = [0.0; 4];
    // SAFETY: a live record; the slot ops' buffers
    unsafe {
        assert_eq!(ee_screens_seet_slot(r as *mut c_void, false, EE_SCREENS_SEET_A_COMBO, 3), core::ptr::null_mut());
        assert_eq!(seet(false, EE_SCREENS_SEET_OP_SLOT, [r, 0, 0x1234], &mut i, &mut f), Some(0));
        assert_eq!(ee_screens_seet_slot(r as *mut c_void, false, EE_SCREENS_SEET_A_COMBO, 3) as usize, 0x1234);
        let mut out = 0usize;
        i[2] = 0;
        seet(false, EE_SCREENS_SEET_OP_SLOT, [r, 0, &mut out as *mut usize as usize], &mut i, &mut f);
        assert_eq!(out, 0x1234);
        // a member the record has not: nothing stored, NULL read
        i = [EE_SCREENS_SEET_A_COMBO as i32, 4, 0, 0];
        seet(false, EE_SCREENS_SEET_OP_SLOT, [r, 0, &mut out as *mut usize as usize], &mut i, &mut f);
        assert_eq!(out, 0);
        controls_free(r, false);
    }
}

#[test]
fn the_texts_are_written_as_the_host_did() {
    let mut buf = [0u8; 4];
    // SAFETY: the buffer holds 4 bytes
    unsafe {
        assert_eq!(write(buf.as_mut_ptr() as usize, 4, b"abcdef"), 6);
        assert_eq!(&buf, b"abcd");
        assert_eq!(write(buf.as_mut_ptr() as usize, -1, b"xy"), 2);
        assert_eq!(&buf, b"abcd");
        assert_eq!(bytes(b"hi\0there".as_ptr() as usize, 8), b"hi\0there".to_vec());
        assert_eq!(bytes(0, 3), Vec::<u8>::new());
    }
    assert!(is_combo(EE_SCREENS_SEET_A_PLAYER) && !is_combo(EE_SCREENS_SEET_A_LABEL));
    assert!(is_edit(false, EE_SCREENS_SEET_A_EDIT2) && !is_edit(true, EE_SCREENS_SEET_A_EDIT2));
}

#[test]
fn an_op_the_toolkit_does_not_answer_goes_on_to_the_host() {
    let mut i = [0; 4];
    let mut f = [0.0; 4];
    // SAFETY: no record is read by these ops
    unsafe {
        assert_eq!(seet(false, EE_SCREENS_SEET_OP_ALIGN, [0; 3], &mut i, &mut f), None);
        assert_eq!(seet(false, EE_SCREENS_SEET_OP_EVENT_NEW, [0; 3], &mut i, &mut f), None);
        assert_eq!(seet(true, EE_SCREENS_SEET_OP_LIST_DATA, [0; 3], &mut i, &mut f), None);
    }
}

#[test]
fn the_maker_s_texts_are_sepl_text_s() {
    use crate::screens::editor_players::*;
    assert_eq!(sepl_text(EE_SCREENS_SEPL_TEXT_NONE, 0, 5), b"".to_vec());
    assert_eq!(sepl_text(EE_SCREENS_SEPL_TEXT_NUMBER, 0, -12), b"-12".to_vec());
    assert_eq!(sepl_text(EE_SCREENS_SEPL_TEXT_CHAR, 0, (3 << 8) | b'x' as c_long), b"xxx".to_vec());
    assert_eq!(sepl_text(EE_SCREENS_SEPL_TEXT_CHAR, 0, b'y' as c_long), b"y".to_vec());
    assert_eq!(sepl_text(EE_SCREENS_SEPL_TEXT_CHAR, 0, 5 << 8), b"".to_vec());
    assert_eq!(sepl_text(99, 0, 5), b"".to_vec());
}

#[test]
fn the_action_names_lose_their_nul() {
    assert_eq!(action_name(0), b"Invalid");
    assert_eq!(action_name(-7), b"");
}
