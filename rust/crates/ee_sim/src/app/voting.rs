//! The edict vote the world holds (`EEWorld::mVoting`, was EEVoting.cpp: moved whole in the game's leaves round 1).
//! A proposal opens a vote unless one is pending (`EEMEdictProposal`, the edict message). Nothing reads the vote's
//! other fields yet; they keep the original's places and names.

#![allow(non_snake_case)]

use std::os::raw::c_long;

/// `EEVoting`'s members (the class's base; zero when made).
#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct EeAppVoting {
    /// the proposed edict (0: none)
    pub mEdict: i16,
    pub mU06: i16,
    pub mB08: bool,
    pub mB09: bool,
    pub mB0A: bool,
}

/// `EEVoting::Propose`: opens a vote on `edict` unless one is already pending; 0 when accepted, 1 when busy.
pub fn propose(v: &mut EeAppVoting, edict: u16, arg: c_long) -> c_long {
    if v.mEdict != 0 {
        return 1;
    }
    v.mU06 = arg as i16;
    v.mEdict = edict as i16;
    v.mB08 = false;
    v.mB0A = false;
    0
}

/// `EEVoting::Propose`
///
/// # Safety
/// `v` is the world's live vote.
#[no_mangle]
pub unsafe extern "C" fn ee_app_voting_propose(v: *mut EeAppVoting, edict: u16, arg: c_long) -> c_long {
    // SAFETY: the caller's contract
    propose(unsafe { &mut *v }, edict, arg)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_proposal_opens_a_vote_once() {
        let mut v = EeAppVoting { mB08: true, mB09: true, mB0A: true, ..Default::default() };
        assert_eq!(propose(&mut v, 0x8003, 0x1_0015), 0);
        assert_eq!(v, EeAppVoting { mEdict: -0x7ffd, mU06: 0x15, mB08: false, mB09: true, mB0A: false });
        assert_eq!(propose(&mut v, 7, 1), 1);
        assert_eq!(v.mEdict, -0x7ffd);
    }
}
