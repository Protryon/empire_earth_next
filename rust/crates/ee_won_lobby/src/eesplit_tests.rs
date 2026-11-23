use super::*;
use std::cell::RefCell;

thread_local! {
    static LOG: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

fn take() -> Vec<String> {
    LOG.with(|l| std::mem::take(&mut *l.borrow_mut()))
}

fn xfer(o: X, a: isize, b: isize, c: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push(format!("{o:?} {a} {b} {c}")));
    match o {
        X::NewQueueSocket => 70,
        X::NewRecvBytes => 80,
        _ => 0,
    }
}

fn game(o: G, a: isize, b: isize, c: isize, d: isize) -> isize {
    LOG.with(|l| l.borrow_mut().push(format!("{o:?} {a} {b} {c} {d}")));
    0
}

#[test]
fn the_sockets_ops_in_the_originals_order() {
    take();
    assert_eq!(xfer_op(X::NewSocket, 0, 0, 0, &xfer), Some(70));
    assert_eq!(take(), ["NewQueueSocket 0 0 0", "AddRef 70 0 0"]);
    xfer_op(X::RepeatAccept, 5, 0, 0, &xfer);
    assert_eq!(
        take(),
        [format!("SetRepeatCompletion 5 {} 0", XC::OnClientAccepted as isize), "SetRepeatAccept 5 0 0".into()]
    );
    xfer_op(X::QueueSend, 5, 9, 3, &xfer);
    assert_eq!(
        take(),
        [
            format!("SetCloseCompletion 5 {} 0", XC::OnClientClosed as isize),
            "QueueSendFile 5 9 3".into(),
            "QueueCloseOp 5 0 0".into()
        ]
    );
    xfer_op(X::QueueDownload, 5, 9, 3, &xfer);
    assert_eq!(
        take(),
        [
            format!("SetCloseCompletion 5 {} 0", XC::OnClientClosed as isize),
            "NewRecvBytes 0 0 0".into(),
            "AddRef 80 0 0".into(),
            format!("OpSetCompletion 80 {} 0", XC::OnFileReceived as isize),
            "OpSetFilePath 80 9 3".into(),
            "OpSetProgress 80 0 0".into(),
            "QueueConnect 5 0 0".into(),
            "QueueOpOn 5 80 0".into(),
            "Release 80 0 0".into(),
        ]
    );
    assert_eq!(xfer_op(X::Bind, 5, 1, 0, &xfer), None);
}

#[test]
fn the_game_ops_split() {
    take();
    assert_eq!(game_op(G::PlayerAttach, 4, 6, 0, 0, &game), Some(0));
    assert_eq!(take(), ["PlayerSetSlotGame 4 0 0 0", "PlayerSetSlotIndex 4 6 0 0", "PlayerSetSlot 4 6 0 0"]);
    game_op(G::TransferSetHost, 4, 2300, 0, 0, &game);
    assert_eq!(take(), ["TransferHostFromGame 4 0 0 0", "TransferHostSetPort 4 2300 0 0"]);
    // the transfer's Rust object: none here (a null object's call answers 0)
    assert_eq!(game_op(G::TransferSetFileInfo, 4, 1, 2, 3, &game), Some(0));
    assert_eq!(take(), ["TransferSetName 4 1 0 0", "TransferRust 4 0 0 0"]);
    // EELobbyGetVersion's: the text with its reference, then the checksum written to the caller's
    let mut checksum: u32 = 7;
    let answer = |o: G, a: isize, b: isize, c: isize, d: isize| match o {
        G::VersionTextNew => 90,
        G::VersionChecksum => 0x1234,
        _ => game(o, a, b, c, d),
    };
    assert_eq!(game_op(G::Version, &mut checksum as *mut u32 as isize, 0, 0, 0, &answer), Some(90));
    assert_eq!((take(), checksum), (vec!["AddRef 90 0 0 0".to_string()], 0x1234));
    assert_eq!(game_op(G::StringsEqual, 0, 0, 0, 0, &game), None);
}
