//! The lobby controls' config readers (R6 WON round 33): ChatCtrlConfig's, RoomLabelCtrlConfig's, UserListCtrlConfig's
//! and ServerListCtrlConfig's Virtual24 (with HandleAddColumn and GetNamedValue; LobbyNetCtrlHost.cpp),
//! GameBrowserCtrlConfig's and GameUserListCtrlConfig's (LobbyCtrlHost.cpp), CrossPromotionButtonConfig's and
//! ImageTextButtonConfig's (CrossPromotionButton.cpp) and CreditsDialogConfig's (CreditsDialog.cpp), in Rust over
//! one-statement C++ ops (each unit's `CfgRdHost`, the reads LobbyNetCtrlHost.cpp's `LobbyCfgReadHost`), in the
//! original's order. The instruction keys and names are ee_won_gui's guiconf tables.
//!
//! A C++ exception never unwinds through this code: every read (and the base classes' readers) runs under
//! WONLobbyGuard.h's `WonGuard`, and the original's `throw ConfigObjectException(...)` is an op that makes the
//! exception and keeps it; the reader then answers `EE_WON_LOBBY_THROWN` (its texts released, as the unwinding
//! did) and the C++ entry rethrows it.
//!
//! Reach: the LAN game reads the shipped configs (the keys they hold); the error branches, the keys no shipped
//! config holds and the other readers' branches run only in won_lobby33_test's golden cases.

use crate::lobbyprv::EE_WON_LOBBY_THROWN as THROWN;
use ee_won_gui::guiconf as gc;
use std::ffi::c_void;

/// The host: `(config, op, a, b, c) -> result`.
pub type EeWonLobbyCfgRdHostFn =
    Option<unsafe extern "C" fn(config: *mut c_void, op: EeWonLobbyCfgRdOp, a: isize, b: isize, c: isize) -> isize>;

/// The host's ops: each one C++ statement (the reads under `WonGuard`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCfgRdOp {
    // ---- the reads (LobbyCfgReadHost: on the `ComponentConfig*`)
    /// `Held(c->ReadString(false))`.
    ReadString = 1,
    /// `c->ReadColor()`.
    ReadColor,
    /// `c->ReadInt()`.
    ReadInt,
    /// `c->EnsureComma()`.
    EnsureComma,
    /// `c->EndOfString()`.
    EndOfString,
    /// `c->Fn386860(std::string((const char*)a))`.
    FindNamed,
    /// `c->Fn386860((std::string)Txt(a))`.
    FindNamedText,
    /// `((GUIStringData*)a)->Release()`.
    TextRelease,
    /// `ee_won_gui_cfg_name((int)b, ((GUIStringData*)a)->mText, ((GUIStringData*)a)->mLength, sizeof(wchar_t))`.
    TextNameIndex,
    /// `Txt(a).CompareNoCase((const char*)b, 0) == 0`.
    TextIsNoCase,
    /// `throw ConfigObjectException(std::string((const char*)a))`.
    Throw,
    /// `throw ConfigObjectException(std::string((const char*)a) + (std::string)Txt(b))`.
    ThrowWithText,
    // ---- ChatCtrlConfig
    /// `cc->ContainerConfig::Virtual24(*(const std::string*)a)`.
    ChatBase,
    /// `cc->mChatCtrl->SetField((int)a, b)`.
    ChatSetField,
    // ---- RoomLabelCtrlConfig
    /// `rl->ContainerConfig::Virtual24(*(const std::string*)a)`.
    RoomBase,
    /// `rl->mRoomLabel->SetField(0, Dat(c->ReadString(false)))`.
    RoomSetPrefixRead,
    /// `rl->mRoomLabel->SetField((int)a, b)`.
    RoomSetField,
    /// `rl->mRoomLabel->SetField((int)a, c->ReadBool())`.
    RoomSetFieldBoolRead,
    // ---- UserListCtrlConfig
    /// `ul->ContainerConfig::Virtual24(*(const std::string*)a)`.
    UserListBase,
    // ---- ServerListCtrlConfig
    /// `sl->ContainerConfig::Virtual24(*(const std::string*)a)`.
    ServerBase,
    /// `sl->ComponentConfig::GetNamedValue(*(const std::string*)a, *(int*)b)`.
    ServerNamedBase,
    /// `sl->mServerList->Cfg((EeWonLobbyCtlCfg)a, b)`.
    ServerCfg,
    /// `sl->mServerList->Cfg(EeWonLobbyCtlCfg::PushColumn, a, b)`.
    ServerPushColumn,
    /// `CfgAddColumn(sl->mServerList, (std::string)Txt(a))` (`Cfg(AddColumn, name.data(), name.size())`).
    ServerAddColumnText,
    /// `LobbyAddGameOption(Txt(a), Txt(b), false)`.
    ServerAddGameOption,
    /// `dynamic_cast<MultiListArea*>((Component*)a)`.
    AsMultiList,
    /// `sl->mHeader = dynamic_cast<MultiListHeader*>((Component*)a)`.
    ServerHeaderFrom,
    /// `sl->mHeader == NULL`.
    ServerHeaderIsNull,
    /// `SafeConfigGetComponent<ComboBox*>(c->ReadComponent(), "ComboBox")`.
    ReadCombo,
    /// `((MultiListArea*)a)->SetNumColumns(++sl->mNumColumns)`.
    ServerColumnsInc,
    /// `sl->mNumColumns`.
    ServerNumColumns,
    /// `((MultiListArea*)a)->SetDynamicSizeColumn((int)b)`.
    ListDynamicColumn,
    /// `((MultiListArea*)a)->SetColumnWidth((int)b, (int)cc, false)`.
    ListColumnWidth,
    /// `sl->mHeader->SetColumnItem(sl->mNumColumns - 1, (Component*)a)`.
    ServerHeaderSetColumnItem,
    // ---- GameBrowserCtrlConfig
    /// `gb->ContainerConfig::Virtual24(*(const std::string*)a)`.
    BrowserBase,
    /// `sCfgRdPath = (std::string)c->ReadString(false)`.
    BrowserPathRead,
    /// `URL(sCfgRdPath).GetType() == URLType_File`.
    BrowserPathIsFile,
    /// `sCfgRdPath = ComponentConfig::GetResourceFile(sCfgRdPath)`.
    BrowserPathResolve,
    /// `gb->mBrowser->SetHomeLocation(sCfgRdPath)`.
    BrowserSetHome,
    // ---- GameUserListCtrlConfig
    /// `gu->ContainerConfig::Virtual24(*(const std::string*)a)`.
    GameUserListBase,
    // ---- CrossPromotionButtonConfig
    /// `cp->ComponentConfig::Virtual24(*(const std::string*)a)`.
    CrossBase,
    /// `cp->mCrossPromotionButton->SetDefaultImage(c->ReadImage(false))`.
    CrossDefaultImageRead,
    /// `cp->mCrossPromotionButton->SetDefaultBrowse(c->ReadString(false))`.
    CrossDefaultBrowseRead,
    // ---- ImageTextButtonConfig
    /// `it->ComponentConfig::Virtual24(*(const std::string*)a)`.
    ImageTextBase,
    /// `it->mImageTextButton->mFont = c->ReadFont()`.
    ImageTextFontRead,
    /// `it->mImageTextButton->mText = c->ReadString(false)`.
    ImageTextTextRead,
    /// `it->mImageTextButton->mBrowse = c->ReadString(false)`.
    ImageTextBrowseRead,
    // ---- CreditsDialogConfig
    /// `cr->ContainerConfig::Virtual24(*(const std::string*)a)`.
    CreditsBase,
    /// `CreditsAddLine(cr->mCredits, GUIString((GUIStringData*)a), c->Fn386860(std::string("DialogComponent")), b)` (`LCall(AddLine, &line, ...)`).
    CreditsAddLine,
    // ---- the configs' SetComponent statements (R6 WON round 35)
    /// `static_cast<ContainerConfig*>(c)->ContainerConfig::SetComponent((Component*)a)` (guarded).
    BaseSetComponent = 200,
    /// `rl->mRoomLabel = static_cast<RoomLabelCtrl*>((Component*)a)`.
    RoomSetLabel,
    /// `cc_->mChatCtrl = static_cast<ChatCtrl*>((Component*)a)`.
    ChatSetCtrl,
    /// `sl->mServerList = static_cast<ServerListCtrl*>((Component*)a)`.
    ServerSetList,
}

/// The calls in: `(key's bytes, its length, the key as a `const std::string*`, d)`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCfgRdCall {
    ChatCtrl = 1,
    RoomLabel,
    UserList,
    ServerList,
    /// `ServerListCtrlConfig::GetNamedValue(key, *(int*)d)`.
    ServerListNamed,
    /// `ServerListCtrlConfig::HandleAddColumn()` (no key).
    ServerListAddColumn,
    GameBrowser,
    GameUserList,
    CrossPromotion,
    ImageText,
    Credits,
    /// The configs' `SetComponent((Component*)d)` (no key): the room label's member then the base's; the chat's
    /// and the game list's the base's then the member.
    RoomLabelSetComponent,
    ChatSetComponent,
    ServerListSetComponent,
}

use EeWonLobbyCfgRdOp as Op;

/// A statement threw: its exception is kept for the C++ entry.
struct Thrown;
type R<T> = Result<T, Thrown>;

/// The ops, `(op, a, b, c) -> result`.
type Out<'a> = &'a dyn Fn(Op, isize, isize, isize) -> isize;

fn ok(r: isize) -> R<isize> {
    if r == THROWN {
        Err(Thrown)
    } else {
        Ok(r)
    }
}

/// A held text the reader releases at its scope's end (or as the unwinding did).
struct Text<'a> {
    o: Out<'a>,
    data: isize,
}

impl Drop for Text<'_> {
    fn drop(&mut self) {
        (self.o)(Op::TextRelease, self.data, 0, 0);
    }
}

fn read_string(o: Out) -> R<Text> {
    Ok(Text { o, data: ok(o(Op::ReadString, 0, 0, 0))? })
}

fn op0(o: Out, op: Op) -> R<isize> {
    ok(o(op, 0, 0, 0))
}

fn throw<T>(o: Out, message: &std::ffi::CStr) -> R<T> {
    o(Op::Throw, message.as_ptr() as isize, 0, 0);
    Err(Thrown)
}

fn throw_with<T>(o: Out, message: &std::ffi::CStr, text: &Text) -> R<T> {
    o(Op::ThrowWithText, message.as_ptr() as isize, text.data, 0);
    Err(Thrown)
}

fn key_index(table: i32, key: isize, len: isize) -> i32 {
    // SAFETY: the key's bytes (the call's contract)
    unsafe { gc::ee_won_gui_cfg_key(table, key as *const std::ffi::c_char, len as usize) }
}

const CHAT_COLORS: [isize; 11] = [0, 5, 6, 1, 3, 2, 4, 7, 8, 9, 10];

fn chat(o: Out, key: i32, k: isize) -> R<bool> {
    if ok(o(Op::ChatBase, k, 0, 0))? != 0 {
        return Ok(true);
    }
    match key {
        0 => {
            let group = read_string(o)?;
            let name =
                o(Op::TextNameIndex, group.data, gc::EE_WON_CFG_NAMES_CHATCTRLCONFIG_VIRTUAL24_GROUP as isize, 0);
            if !(0..=2).contains(&name) {
                return throw_with(o, c"Invalid Group Type: ", &group);
            }
            o(Op::ChatSetField, 20, [1, 2, 0x12][name as usize], 0);
        }
        1 => {}
        2..=12 => {
            let color = op0(o, Op::ReadColor)?;
            o(Op::ChatSetField, CHAT_COLORS[key as usize - 2], color, 0);
        }
        _ => return Ok(false),
    }
    Ok(true)
}

fn room_label(o: Out, key: i32, k: isize) -> R<bool> {
    match key {
        0 => {
            op0(o, Op::RoomSetPrefixRead)?;
        }
        1 => {
            let group = read_string(o)?;
            if o(Op::TextIsNoCase, group.data, c"CHAT".as_ptr() as isize, 0) != 0 {
                o(Op::RoomSetField, 1, 1, 0);
            } else if o(Op::TextIsNoCase, group.data, c"GAME".as_ptr() as isize, 0) != 0 {
                o(Op::RoomSetField, 1, 2, 0);
            } else {
                return throw_with(o, c"Invalid Group Type: ", &group);
            }
        }
        2 | 3 => {
            ok(o(Op::RoomSetFieldBoolRead, key as isize, 0, 0))?;
        }
        _ => return Ok(ok(o(Op::RoomBase, k, 0, 0))? != 0),
    }
    Ok(true)
}

/// EeWonLobbyCtlCfg's values the readers use (lobbyctl.rs).
use crate::srvlistctl::EeWonLobbyCtlCfg as Cfg;

fn server_list(o: Out, key: i32, k: isize) -> R<bool> {
    if ok(o(Op::ServerBase, k, 0, 0))? != 0 {
        return Ok(true);
    }
    let type_name =
        |t: &Text| o(Op::TextNameIndex, t.data, gc::EE_WON_CFG_NAMES_SERVERLISTCTRLCONFIG_VIRTUAL24_ATYPE as isize, 0);
    match key {
        0 => {
            let t = read_string(o)?;
            let name = type_name(&t);
            if !(0..=2).contains(&name) {
                return throw_with(o, c"Invalid server list id: ", &t);
            }
            o(Op::ServerCfg, Cfg::SetGameType as isize, [2, 1, 3][name as usize], 0);
        }
        1 => add_column(o)?,
        2 => {
            let column = read_string(o)?;
            op0(o, Op::EnsureComma)?;
            let filters = read_string(o)?;
            o(Op::ServerAddGameOption, column.data, filters.data, 0);
        }
        3 => {
            let n = op0(o, Op::ReadInt)?;
            o(Op::ServerCfg, Cfg::SetNumColumns as isize, n, 0);
        }
        4 => {
            op0(o, Op::ReadInt)?;
            op0(o, Op::EnsureComma)?;
            let t = read_string(o)?;
            let name = type_name(&t);
            if !(3..=5).contains(&name) {
                return throw_with(o, c"Unknown filter type: ", &t);
            }
        }
        _ => return Ok(false),
    }
    Ok(true)
}

fn find(o: Out, name: &std::ffi::CStr) -> R<isize> {
    ok(o(Op::FindNamed, name.as_ptr() as isize, 0, 0))
}

fn add_column(o: Out) -> R<()> {
    let mut list = o(Op::ServerCfg, Cfg::List as isize, 0, 0);
    if list == 0 {
        let c = find(o, c"ServerList")?;
        if c == 0 {
            return throw(o, c"Need ServerList before calling AddColumn");
        }
        list = o(Op::AsMultiList, c, 0, 0);
        if list == 0 {
            return throw(o, c"ServerList needs to be a MultiListArea");
        }
        o(Op::ServerCfg, Cfg::SetList as isize, list, 0);
        let c = find(o, c"ServerListHeader")?;
        if c == 0 {
            return throw(o, c"Need ServerList header before calling AddColumn");
        }
        o(Op::ServerHeaderFrom, c, 0, 0);
        if o(Op::ServerHeaderIsNull, 0, 0, 0) != 0 {
            return throw(o, c"ServerListHeader needs to be a MultiListHeader");
        }
    }
    let column = read_string(o)?;
    op0(o, Op::EnsureComma)?;
    let component = ok(o(Op::FindNamedText, column.data, 0, 0))?;
    if component == 0 {
        return throw_with(o, c"Component not found: ", &column);
    }
    let width = op0(o, Op::ReadInt)? as i32;
    let mut combo = 0;
    if op0(o, Op::EndOfString)? == 0 {
        op0(o, Op::EnsureComma)?;
        combo = op0(o, Op::ReadCombo)?;
    }
    let def = o(Op::ServerAddColumnText, column.data, 0, 0);
    if def < 0 {
        return throw_with(o, c"Column not defined: ", &column);
    }
    if def == 0 {
        return Ok(());
    }
    o(Op::ServerColumnsInc, list, 0, 0);
    let col = o(Op::ServerNumColumns, 0, 0, 0) - 1;
    if width == 0 {
        o(Op::ListDynamicColumn, list, col, 0);
    } else {
        o(Op::ListColumnWidth, list, col, width as isize);
    }
    o(Op::ServerHeaderSetColumnItem, component, 0, 0);
    o(Op::ServerPushColumn, def - 1, combo, 0);
    Ok(())
}

fn read(which: EeWonLobbyCfgRdCall, key_bytes: isize, len: isize, k: isize, d: isize, o: Out) -> R<isize> {
    type C = EeWonLobbyCfgRdCall;
    let base = |op: Op| -> R<isize> { Ok((ok(o(op, k, 0, 0))? != 0) as isize) };
    Ok(match which {
        C::RoomLabelSetComponent => {
            ok(o(Op::RoomSetLabel, d, 0, 0))?;
            ok(o(Op::BaseSetComponent, d, 0, 0))?;
            0
        }
        C::ChatSetComponent | C::ServerListSetComponent => {
            ok(o(Op::BaseSetComponent, d, 0, 0))?;
            ok(o(if which == C::ChatSetComponent { Op::ChatSetCtrl } else { Op::ServerSetList }, d, 0, 0))?;
            0
        }
        C::ChatCtrl => {
            chat(o, key_index(gc::EE_WON_CFG_KEYS_CHATCTRLCONFIG_VIRTUAL24_INKEY, key_bytes, len), k)? as isize
        }
        C::RoomLabel => {
            room_label(o, key_index(gc::EE_WON_CFG_KEYS_ROOMLABELCTRLCONFIG_VIRTUAL24_INKEY, key_bytes, len), k)?
                as isize
        }
        C::UserList => match key_index(gc::EE_WON_CFG_KEYS_USERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION, key_bytes, len) {
            0 | 1 => 1,
            _ => base(Op::UserListBase)?,
        },
        C::ServerList => server_list(
            o,
            key_index(gc::EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_VIRTUAL24_ININSTRUCTION, key_bytes, len),
            k,
        )? as isize,
        C::ServerListNamed => {
            if key_index(gc::EE_WON_CFG_KEYS_SERVERLISTCTRLCONFIG_GETNAMEDVALUE_INNAME, key_bytes, len) == 0 {
                let n = o(Op::ServerCfg, Cfg::ColumnCount as isize, 0, 0) as i32;
                // SAFETY: the caller's int (the call's contract)
                unsafe { *(d as *mut i32) = n };
                1
            } else {
                (ok(o(Op::ServerNamedBase, k, d, 0))? != 0) as isize
            }
        }
        C::ServerListAddColumn => {
            add_column(o)?;
            0
        }
        C::GameBrowser => {
            if ok(o(Op::BrowserBase, k, 0, 0))? != 0 {
                return Ok(1);
            }
            if key_index(gc::EE_WON_CFG_KEYS_GAMEBROWSERCTRLCONFIG_VIRTUAL24_INSTRUCTION, key_bytes, len) != 0 {
                return Ok(0);
            }
            op0(o, Op::BrowserPathRead)?;
            if o(Op::BrowserPathIsFile, 0, 0, 0) != 0 {
                op0(o, Op::BrowserPathResolve)?;
            }
            op0(o, Op::BrowserSetHome)?;
            1
        }
        C::GameUserList => {
            match key_index(gc::EE_WON_CFG_KEYS_GAMEUSERLISTCTRLCONFIG_VIRTUAL24_INSTRUCTION, key_bytes, len) {
                0 => 1,
                _ => base(Op::GameUserListBase)?,
            }
        }
        C::CrossPromotion => {
            match key_index(gc::EE_WON_CFG_KEYS_CROSSPROMOTIONBUTTONCONFIG_VIRTUAL24_KEY, key_bytes, len) {
                0 => op0(o, Op::CrossDefaultImageRead).map(|_| 1)?,
                1 => op0(o, Op::CrossDefaultBrowseRead).map(|_| 1)?,
                _ => base(Op::CrossBase)?,
            }
        }
        C::ImageText => match key_index(gc::EE_WON_CFG_KEYS_IMAGETEXTBUTTONCONFIG_VIRTUAL24_KEY, key_bytes, len) {
            0 => op0(o, Op::ImageTextFontRead).map(|_| 1)?,
            1 => op0(o, Op::ImageTextTextRead).map(|_| 1)?,
            2 => op0(o, Op::ImageTextBrowseRead).map(|_| 1)?,
            _ => base(Op::ImageTextBase)?,
        },
        C::Credits => {
            if key_index(gc::EE_WON_CFG_KEYS_CREDITSDIALOGCONFIG_VIRTUAL24_ININSTRUCTION, key_bytes, len) != 0 {
                return base(Op::CreditsBase);
            }
            let line = read_string(o)?;
            let mut color: u32 = u32::MAX;
            if op0(o, Op::EndOfString)? == 0 {
                op0(o, Op::EnsureComma)?;
                color = op0(o, Op::ReadColor)? as u32;
            }
            ok(o(Op::CreditsAddLine, line.data, color as isize, 0))?;
            1
        }
    })
}

/// One of the config readers on `config` (a `ComponentConfig*`) over the host `host`: 1 or 0 (handled or not), or
/// `EE_WON_LOBBY_THROWN` (the C++ entry rethrows the kept exception).
///
/// # Safety
/// `key` holds `len` bytes and `key_string` is the same key as a `const std::string*`; `d` as the call says; `host`
/// answers the ops for `config`.
#[no_mangle]
pub unsafe extern "C" fn ee_won_lobbycfgrd_call(
    config: *mut c_void,
    host: EeWonLobbyCfgRdHostFn,
    which: EeWonLobbyCfgRdCall,
    key: isize,
    len: isize,
    key_string: isize,
    d: isize,
) -> isize {
    let o = |op: Op, a: isize, b: isize, c: isize| match host {
        // SAFETY: the contract
        Some(f) => unsafe { f(config, op, a, b, c) },
        None => 0,
    };
    read(which, key, len, key_string, d, &o).unwrap_or(THROWN)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::cell::RefCell;

    fn run(which: EeWonLobbyCfgRdCall, key: &str, answer: &dyn Fn(Op) -> isize) -> (isize, Vec<String>) {
        let log = RefCell::new(Vec::new());
        let o = |op: Op, a: isize, _: isize, _: isize| {
            log.borrow_mut().push(format!("{op:?}"));
            let _ = a;
            answer(op)
        };
        let r = read(which, key.as_ptr() as isize, key.len() as isize, 0, 0, &o).unwrap_or(THROWN);
        (r, log.into_inner())
    }

    #[test]
    fn a_bad_group_throws_and_releases_its_text() {
        let (r, log) = run(EeWonLobbyCfgRdCall::ChatCtrl, "GROUPTYPE", &|op| match op {
            Op::ReadString => 0x40,
            Op::TextNameIndex => -1,
            _ => 0,
        });
        assert_eq!(
            (r, log.iter().map(|s| s.as_str()).collect::<Vec<_>>()),
            (THROWN, vec!["ChatBase", "ReadString", "TextNameIndex", "ThrowWithText", "TextRelease"])
        );
        let (r, log) = run(EeWonLobbyCfgRdCall::ChatCtrl, "COLOR_USER", &|_| 0);
        assert_eq!(
            (r, log.iter().map(|s| s.as_str()).collect::<Vec<_>>()),
            (1, vec!["ChatBase", "ReadColor", "ChatSetField"])
        );
        let (r, _) = run(EeWonLobbyCfgRdCall::ChatCtrl, "NOPE", &|_| 0);
        assert_eq!(r, 0);
    }

    #[test]
    fn a_column_needs_its_list_and_header() {
        let (r, log) = run(EeWonLobbyCfgRdCall::ServerList, "ADDCOLUMN", &|_| 0);
        assert_eq!((r, log.last().unwrap().as_str()), (THROWN, "Throw"));
        let (r, log) = run(EeWonLobbyCfgRdCall::ServerList, "ADDCOLUMN", &|op| match op {
            Op::ServerCfg => 0x50,
            Op::ReadString | Op::FindNamedText | Op::ServerAddColumnText => 0x60,
            Op::ReadInt => 120,
            Op::EndOfString => 1,
            Op::ServerNumColumns => 3,
            _ => 0,
        });
        assert_eq!(r, 1);
        // the column's text released last, at the reader's end
        assert_eq!(
            log[log.len() - 6..],
            [
                "ServerColumnsInc",
                "ServerNumColumns",
                "ListColumnWidth",
                "ServerHeaderSetColumnItem",
                "ServerPushColumn",
                "TextRelease"
            ]
        );
    }
}
