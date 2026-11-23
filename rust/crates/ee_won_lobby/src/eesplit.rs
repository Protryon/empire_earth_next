//! The EE game host's and the saved game transfer host's compound cases composed in Rust (R6 WON round 34): an op
//! eegame.rs or eexfer.rs sends to the C++ host (EELobbyGameHost.cpp) that was a run of C++ statements is answered
//! here in the game, each C++ statement one case of the host (the references on the Rust objects, netops.rs). Reached
//! while the network core is bound (the game binds it before any lobby game exists); the drivers' scripted hosts never
//! reach it, so the games' and transfers' goldens see the ops as before. won_dlg34_test runs the deleted cases (as
//! they were) against these over one stand-in world.
//!
//! - PlayerAttach: the player's slot (its game and number: the original's slot pointer), then the slot number.
//! - TransferSetFileInfo: the name stored, then the transfer's Rust call; TransferSetHost: the game's address copied,
//!   then its port.
//! - The transfer's sockets: NewSocket (the reference the Rust's), RepeatAccept, QueueSend and QueueDownload (the
//!   completions, the receive op and its file and progress, the connect and the queued ops).

use crate::eegame::EeWonEeGameOp as G;
use crate::eexfer::{EeWonEeXferCall as XC, EeWonEeXferOp as X};

/// The calls out: an op to the host the object was made with.
pub(crate) type XferOut<'a> = &'a dyn Fn(X, isize, isize, isize) -> isize;
pub(crate) type GameOut<'a> = &'a dyn Fn(G, isize, isize, isize, isize) -> isize;

/// A transfer op answered here (`None`: the C++ host's).
pub(crate) fn xfer_op(op: X, a: isize, b: isize, c: isize, out: XferOut) -> Option<isize> {
    match op {
        X::NewSocket => {
            let socket = out(X::NewQueueSocket, 0, 0, 0);
            out(X::AddRef, socket, 0, 0);
            Some(socket)
        }
        X::RepeatAccept => {
            out(X::SetRepeatCompletion, a, XC::OnClientAccepted as isize, 0);
            out(X::SetRepeatAccept, a, 0, 0);
            Some(0)
        }
        X::QueueSend => {
            out(X::SetCloseCompletion, a, XC::OnClientClosed as isize, 0);
            out(X::QueueSendFile, a, b, c);
            out(X::QueueCloseOp, a, 0, 0);
            Some(0)
        }
        X::QueueDownload => {
            out(X::SetCloseCompletion, a, XC::OnClientClosed as isize, 0);
            // the original's local SmartPtr
            let recv = out(X::NewRecvBytes, 0, 0, 0);
            out(X::AddRef, recv, 0, 0);
            out(X::OpSetCompletion, recv, XC::OnFileReceived as isize, 0);
            out(X::OpSetFilePath, recv, b, c);
            out(X::OpSetProgress, recv, 0, 0);
            out(X::QueueConnect, a, 0, 0);
            out(X::QueueOpOn, a, recv, 0);
            out(X::Release, recv, 0, 0);
            Some(0)
        }
        _ => None,
    }
}

/// A game op answered here (`None`: the C++ host's).
pub(crate) fn game_op(op: G, a: isize, b: isize, c: isize, d: isize, out: GameOut) -> Option<isize> {
    match op {
        G::PlayerAttach => {
            out(G::PlayerSetSlotGame, a, 0, 0, 0);
            out(G::PlayerSetSlotIndex, a, b, 0, 0);
            out(G::PlayerSetSlot, a, b, 0, 0);
            Some(0)
        }
        G::TransferSetFileInfo => {
            out(G::TransferSetName, a, b, 0, 0);
            let rust = out(G::TransferRust, a, 0, 0, 0) as *const crate::eexfer::EeWonEeXfer;
            // SAFETY: the transfer's live Rust object (the shell's member)
            Some(unsafe {
                crate::eexfer::ee_won_eexfer_call(rust, XC::SetFileInfo, c as u32 as isize, d as u32 as isize)
            })
        }
        G::TransferSetHost => {
            out(G::TransferHostFromGame, a, 0, 0, 0);
            out(G::TransferHostSetPort, a, b, 0, 0);
            Some(0)
        }
        _ => None,
    }
}

/// `xfer_op` for the drivers (won_dlg34_test) over the host `host` with its object `obj`; `INTPTR_MIN` when the op
/// is not one of these.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)] // the object passes through to the driver's host
pub extern "C" fn ee_won_eexfer_split_op(
    obj: *mut std::ffi::c_void,
    op: crate::eexfer::EeWonEeXferOp,
    a: isize,
    b: isize,
    c: isize,
    host: crate::eexfer::EeWonEeXferHostFn,
) -> isize {
    let out = |o: X, x: isize, y: isize, z: isize| match host {
        // SAFETY: the driver's host with its object
        Some(f) => unsafe { f(obj, o, x, y, z) },
        None => 0,
    };
    xfer_op(op, a, b, c, &out).unwrap_or(isize::MIN)
}

/// `game_op` for the drivers over the host `host` with its game `game`; `INTPTR_MIN` when the op is not one of these.
#[no_mangle]
#[allow(clippy::not_unsafe_ptr_arg_deref)] // the object passes through to the driver's host
pub extern "C" fn ee_won_eegame_split_op(
    game: *mut std::ffi::c_void,
    op: crate::eegame::EeWonEeGameOp,
    a: isize,
    b: isize,
    c: isize,
    d: isize,
    host: crate::eegame::EeWonEeGameHostFn,
) -> isize {
    let out = |o: G, w: isize, x: isize, y: isize, z: isize| match host {
        // SAFETY: the driver's host with its game
        Some(f) => unsafe { f(game, o, w, x, y, z) },
        None => 0,
    };
    game_op(op, a, b, c, d, &out).unwrap_or(isize::MIN)
}

#[cfg(test)]
#[path = "eesplit_tests.rs"]
mod tests;
