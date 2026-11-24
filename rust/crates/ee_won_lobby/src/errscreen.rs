//! The lobby's top container and its "Lobby Error" dialog (LobbyErrorScreen.cpp's LobbyGetContainer, R6 WON lobby and
//! network round 35) composed in Rust: the lobby's container while the lobby is up; after a failed start the error
//! dialog, built once (a background, a wrapping centred label with the start's error, an Ok button, their layouts)
//! and kept in `gLobbyFailureDialog`; NULL otherwise. Each C++ statement is one op of the host (LobbyErrorScreen.cpp's
//! `ErrorScreenHost`); the original's locals are the host's statics, made where the locals were and released in the
//! reverse order at the end of their scope.
//!
//! Reach: every LAN game takes the container (the first branch); the error dialog shows only after a failed lobby
//! start, which no LAN game makes: won_logic35_test runs the original against this over a stand-in world.

/// The host's ops: each one C++ statement.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonErrScreenOp {
    /// `gLobbyData.get() != NULL` (0 or 1).
    PrvExists = 1,
    /// `(Container*)gLobbyData->mLobbyContainer.get()`.
    PrvContainer,
    /// `gLobbyInitFailed` (0 or 1).
    InitFailed,
    /// `gLobbyFailureDialog.get() != NULL` (0 or 1).
    FailureExists,
    /// `(Container*)gLobbyFailureDialog.get()`.
    FailureGet,
    /// `sErrDialog = new Dialog`.
    DialogNew,
    /// `sErrDialog->SetEndOnEscape(0)`.
    DialogNoEscape,
    /// `sErrBackground = WONShellNew(EeWonShellKind::MsDlgBack)`.
    BackgroundNew,
    /// `sErrLabel = new Label`.
    LabelNew,
    /// `sErrLabel->SetWrap(true)`.
    LabelWrap,
    /// `sErrLabel->SetHorzAlign(HorzAlign_Center)`.
    LabelCenter,
    /// `sErrLabel->SetAdjustHeightToFitText(true)`.
    LabelFit,
    /// `sErrText = GUIString("Lobby Error\n\n")`.
    TextNew,
    /// `sErrText.append(gLobbyInitError)`.
    TextAppendError,
    /// `sErrLabel->SetText(sErrText, true)`.
    LabelSetText,
    /// `sErrOkText = GUIString("Ok")`.
    OkTextNew,
    /// `sErrButton = static_cast<Button*>(WONShellNew(EeWonShellKind::MsButton, (intptr_t)&sErrOkText))`.
    ButtonNew,
    /// `sErrButton->SetButtonFlags(0x100, true)`.
    ButtonFlags,
    /// `sErrButton->SetSizeAtLeast(100, 0)`.
    ButtonSize,
    /// `sErrButton->RequestFocus()`.
    ButtonFocus,
    /// `sErrDialog->Fn2F6140(sErrBackground.get(), 3, sErrDialog.get(), 0, 0, 0, 0, 0)`.
    LayoutBackground,
    /// `sErrDialog->Fn2F6140(sErrLabel.get(), 0x1001, sErrDialog.get(), 10, 0, -20, 0, 0)`.
    LayoutLabel,
    /// `sErrDialog->AddChildLayout(new CenterLayout(sErrDialog.get(), sErrButton.get(), true, false), 0)`.
    CenterButton,
    /// `sErrDialog->Fn2F6140(sErrButton.get(), 0x200, sErrLabel.get(), 0, 20, 0, 0, 0)`.
    LayoutButton,
    /// `sErrVertical = new CenterLayout(sErrDialog.get(), sErrLabel.get(), false, true)`.
    VerticalNew,
    /// `sErrVertical->Add(sErrButton.get())`.
    VerticalAdd,
    /// `sErrDialog->AddChildLayout(sErrVertical, 0)`.
    VerticalLayout,
    /// `sErrDialog->AddChild(sErrBackground.get())`.
    AddBackground,
    /// `sErrDialog->AddChild(sErrLabel.get())`.
    AddLabel,
    /// `sErrDialog->AddChild(sErrButton.get())`.
    AddButton,
    /// `gLobbyFailureDialog = sErrDialog`.
    FailureSet,
    /// `sErrButton = NULL`.
    ButtonDrop,
    /// `sErrOkText = GUIString()`.
    OkTextDrop,
    /// `sErrText = GUIString()`.
    TextDrop,
    /// `sErrLabel = NULL`.
    LabelDrop,
    /// `sErrBackground = NULL`.
    BackgroundDrop,
    /// `sErrDialog = NULL`.
    DialogDrop,
}

/// The host: `(op) -> result`.
pub type EeWonErrScreenHostFn = Option<unsafe extern "C" fn(op: EeWonErrScreenOp) -> isize>;

/// LobbyGetContainer over `out`.
fn container(out: &dyn Fn(EeWonErrScreenOp) -> isize) -> isize {
    use EeWonErrScreenOp as O;
    if out(O::PrvExists) != 0 {
        return out(O::PrvContainer);
    }
    if out(O::InitFailed) == 0 {
        return 0;
    }
    if out(O::FailureExists) == 0 {
        // the dialog and its parts, in the original's statement order; then its locals' ends, last made first
        let build = [
            O::DialogNew,
            O::DialogNoEscape,
            O::BackgroundNew,
            O::LabelNew,
            O::LabelWrap,
            O::LabelCenter,
            O::LabelFit,
            O::TextNew,
            O::TextAppendError,
            O::LabelSetText,
            O::OkTextNew,
            O::ButtonNew,
            O::ButtonFlags,
            O::ButtonSize,
            O::ButtonFocus,
            O::LayoutBackground,
            O::LayoutLabel,
            O::CenterButton,
            O::LayoutButton,
            O::VerticalNew,
            O::VerticalAdd,
            O::VerticalLayout,
            O::AddBackground,
            O::AddLabel,
            O::AddButton,
            O::FailureSet,
            O::ButtonDrop,
            O::OkTextDrop,
            O::TextDrop,
            O::LabelDrop,
            O::BackgroundDrop,
            O::DialogDrop,
        ];
        for op in build {
            out(op);
        }
    }
    out(O::FailureGet)
}

/// `LobbyGetContainer()`: the lobby's container, the error dialog after a failed start, or NULL (0).
///
/// # Safety
/// `host` answers the ops (LobbyErrorScreen.cpp's statements).
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobby_error_container(host: EeWonErrScreenHostFn) -> isize {
    let Some(f) = host else { return 0 };
    // SAFETY: the caller's host
    container(&|op| unsafe { f(op) })
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(prv: bool, failed: bool, exists: bool) -> (isize, Vec<EeWonErrScreenOp>) {
        let log = RefCell::new(Vec::new());
        let r = container(&|op| {
            log.borrow_mut().push(op);
            match op {
                EeWonErrScreenOp::PrvExists => prv as isize,
                EeWonErrScreenOp::PrvContainer => 11,
                EeWonErrScreenOp::InitFailed => failed as isize,
                EeWonErrScreenOp::FailureExists => exists as isize,
                EeWonErrScreenOp::FailureGet => 22,
                _ => 0,
            }
        });
        (r, log.into_inner())
    }

    #[test]
    fn the_container_the_dialog_or_nothing() {
        assert_eq!(run(true, true, false).0, 11);
        assert_eq!(run(false, false, false), (0, vec![EeWonErrScreenOp::PrvExists, EeWonErrScreenOp::InitFailed]));
        let (r, log) = run(false, true, true);
        assert_eq!(r, 22);
        assert_eq!(log.len(), 4);
        let (r, log) = run(false, true, false);
        assert_eq!(r, 22);
        assert_eq!(log.len(), 4 + 32);
        assert_eq!(log[3], EeWonErrScreenOp::DialogNew);
        assert_eq!(log[log.len() - 2], EeWonErrScreenOp::DialogDrop);
    }
}
