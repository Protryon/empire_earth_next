//! ServerListCtrl and its rows as lobbyctl.rs kind `ServerList` (R6 WONLobby round 13): the game lists of the LAN
//! screen, the direct-connect tab and the Internet screen. The control held its list, Join, Refresh and Remove buttons,
//! the summary label and a bold font, a column table (each column's paint, order and filter, its filter combo box, the
//! game types it shows for), the game type, the refilter timer and one row per known game (`std::map<SmartPtr<
//! LobbyGame>, SmartPtr<ServerDetailsItem>>`). All of that is here: the shared column definitions (the original's
//! static `mColumnMap`: InitColumnMap's six columns and the EE game's two), the filters (the numeric filter's parser
//! included), the orders and the paints of a row's columns.
//!
//! The rows stay C++ `ServerDetailsItem`s (the list calls their virtuals and the EE game subclasses them for its
//! details): their members are C++, their virtuals (Paint, CalcDimensions, Compare, CanSelect, MouseDown) one
//! forward each into [`EeWonLobbyCtl::row_call`]. Every call on a game or a game list is the model's (`MGet`,
//! `MSet`, `MCall`: answered by modelobj.rs in the game, by the host in the drivers).

use std::cell::{Cell, RefCell};
use std::collections::BTreeMap;
use std::sync::Mutex;

use crate::eeglue::EeWonEeGlueCall as GL;
use crate::lobbyctl::{
    pack, EeWonLobbyCtl, EeWonLobbyCtlEvent as Ev, EeWonLobbyCtlItemField as F, EeWonLobbyCtlOp as Op,
};
use crate::modelobj::{EeWonModelCall as MC, EeWonModelField as MF};
use crate::resource::EeWonLobbyRes as R;

/// A column's handlers (the original's paint, compare and filter function pointers).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub(crate) enum Col {
    /// A `SetNumColumns` default: no handler.
    None,
    Icon,
    Skill,
    GameName,
    Address,
    NumPlayers,
    Ping,
    /// The EE game's (EELobbyGlueHost.cpp's `SetColumnHandlers`).
    Epoch,
    MapType,
}

/// The column definitions by name (the original's `mColumnMap`, ignoring case; `GetColumnInfo` makes an entry).
const NAMES: [(&str, Col); 8] = [
    ("Icon", Col::Icon),
    ("Skill", Col::Skill),
    ("GameName", Col::GameName),
    ("Address", Col::Address),
    ("NumPlayers", Col::NumPlayers),
    ("Ping", Col::Ping),
    ("Epoch", Col::Epoch),
    ("MapType", Col::MapType),
];

/// Which definitions exist and their game type flags (index as `NAMES`).
static DEFINED: Mutex<[Option<u8>; 8]> = Mutex::new([None; 8]);

fn defined() -> std::sync::MutexGuard<'static, [Option<u8>; 8]> {
    DEFINED.lock().unwrap_or_else(|e| e.into_inner())
}

/// `ServerListCtrl::InitColumnMap`: Icon, Skill, GameName, Address (direct connect only), NumPlayers, Ping (not LAN).
pub fn init_column_map() {
    let mut d = defined();
    for i in 0..6 {
        d[i] = Some(d[i].unwrap_or(0xff));
    }
    d[3] = Some(4);
    d[5] = Some(d[5].unwrap_or(0xff) & !2);
}

/// The EE game's column `which` (0 Epoch, 1 MapType): `GetColumnInfo` and its three handlers.
pub fn define_ee_column(which: bool) {
    let i = if which { 7 } else { 6 };
    let mut d = defined();
    d[i] = Some(d[i].unwrap_or(0xff));
}

/// Forgets every definition (the drivers' fresh process).
pub fn reset_column_map() {
    *defined() = [None; 8];
}

/// The definition of a column name (ASCII case ignored), with its flags.
pub fn column_by_name(name: &[u8]) -> Option<(usize, u8)> {
    let i = NAMES.iter().position(|(n, _)| n.as_bytes().eq_ignore_ascii_case(name))?;
    defined()[i].map(|f| (i, f))
}

/// One entry of `mColumnInfo`.
#[derive(Clone, Copy, Debug)]
pub(crate) struct Column {
    pub col: Col,
    /// the definition's name row (`NAMES`; -1 a default entry: the empty name)
    pub name: i32,
    /// `mFilterCombo` (one reference)
    pub combo: isize,
}

impl Column {
    const DEFAULT: Column = Column { col: Col::None, name: -1, combo: 0 };
}

/// ServerListCtrl's members beyond its controls.
#[derive(Default)]
pub(crate) struct SrvState {
    pub columns: RefCell<Vec<Column>>,
    pub game_type: Cell<i32>,
    pub need_filter: Cell<bool>,
    pub filter_time: Cell<i32>,
    /// `mNumColumns` (4 from the constructor)
    pub num_columns: Cell<i32>,
    /// `mServerDetailsMap`: game -> row, each holding one reference, in the games' pointer order
    pub map: RefCell<BTreeMap<usize, isize>>,
}

// the members, in the original's member order: mBoldFont, mList, mJoinButton, mRefreshButton, mRemoveButton,
// mSummaryLabel
pub(crate) const L_FONT: usize = 0;
pub(crate) const L_LIST: usize = 1;
pub(crate) const L_JOIN: usize = 2;
pub(crate) const L_REFRESH: usize = 3;
pub(crate) const L_REMOVE: usize = 4;
pub(crate) const L_SUMMARY: usize = 5;

/// `gServerFilterItemPos`: the filter combo's add/remove row.
const FILTER_POS: isize = 1;
/// `StandardColor_GrayText`, `StandardColor_Text`.
pub(crate) const SC_GRAY_TEXT: isize = 6;
pub(crate) const SC_TEXT: isize = 14;

/// The calls a row makes (its virtuals), `EeWonLobbyCtlRowCall`.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlRowCall {
    /// `Paint(*(Graphics*)a, (ListArea*)b)`.
    Paint = 0,
    /// `CalcDimensions((ListArea*)a)`.
    CalcDimensions,
    /// `Compare((const ListItem*)a, (ListArea*)b)`; `c`: the address order the original fell back on
    /// (`(const ListItem*)this - other`).
    Compare,
    /// `CanSelect((ListArea*)a, b's point)` (0 or 1).
    CanSelect,
    /// `MouseDown((ListArea*)a, b's point, c)`.
    MouseDown,
    /// `SetGame((LobbyGame*)a)`.
    SetGame,
}

/// The row call numbered `v`.
pub(crate) fn row_call_of(v: isize) -> Option<EeWonLobbyCtlRowCall> {
    use EeWonLobbyCtlRowCall as RC;
    [RC::Paint, RC::CalcDimensions, RC::Compare, RC::CanSelect, RC::MouseDown, RC::SetGame].get(v as usize).copied()
}

/// The config call numbered `v`.
pub(crate) fn cfg_call_of(v: isize) -> Option<EeWonLobbyCtlCfg> {
    use EeWonLobbyCtlCfg as C;
    [
        C::SetGameType,
        C::SetNumColumns,
        C::ColumnCount,
        C::SetList,
        C::List,
        C::GameType,
        C::AddColumn,
        C::PushColumn,
        C::SelGame,
    ]
    .get(v as usize)
    .copied()
}

/// `ServerListCtrl::InitColumnMap` (WONLobbyPrv.cpp's start-up).
#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_init_columns() {
    init_column_map();
}

/// The EE game's column `which` (0 Epoch, 1 MapType) defined (EELobbyGlueHost.cpp's `SetColumnHandlers`).
#[no_mangle]
pub extern "C" fn ee_won_lobbyctl_ee_column(which: i32) {
    define_ee_column(which != 0);
}

fn lo(v: isize) -> i32 {
    v as i32
}

fn hi(v: isize) -> i32 {
    (v >> 32) as i32
}

// ---- the numeric filter (ServerDetailsItem::IsStartOfNumber, ParseIntFilter)

/// `IsStartOfNumber`: a digit (of a non-negative `char`) or a sign.
pub fn is_start_of_number(c: u8) -> bool {
    (c < 0x80 && c.is_ascii_digit()) || c == b'-' || c == b'+'
}

/// MSVC's `atoi`: leading white space, a sign, digits; out of range clamped.
pub fn atoi(s: &[u8]) -> i32 {
    let mut i = 0;
    while i < s.len() && matches!(s[i], b' ' | b'\t' | b'\n' | 0x0b | 0x0c | b'\r') {
        i += 1;
    }
    let mut negative = false;
    if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
        negative = s[i] == b'-';
        i += 1;
    }
    let mut v: i64 = 0;
    while i < s.len() && s[i].is_ascii_digit() {
        v = v * 10 + (s[i] - b'0') as i64;
        if v > 1 << 32 {
            v = 1 << 32;
        }
        i += 1;
    }
    let v = if negative { -v } else { v };
    v.clamp(i32::MIN as i64, i32::MAX as i64) as i32
}

/// `ParseIntFilter`: the bounds of the filter's tokens (`<n`, `<=n`, `>n`, `>=n`, `=n`, `n`; split at spaces, the
/// first 510 bytes), whether any parsed.
pub fn parse_int_filter(text: &[u8], lower: &mut i32, upper: &mut i32) -> bool {
    let mut buffer = text;
    if let Some(nul) = buffer.iter().position(|&b| b == 0) {
        buffer = &buffer[..nul];
    }
    if buffer.len() > 510 {
        buffer = &buffer[..510];
    }
    let mut parsed = false;
    for token in buffer.split(|&b| b == b' ').filter(|t| !t.is_empty()) {
        let at = |i: usize| token.get(i).copied().unwrap_or(0);
        let from = |i: usize| atoi(token.get(i..).unwrap_or(&[]));
        match token[0] {
            b'<' => {
                if at(1) == b'=' && is_start_of_number(at(2)) {
                    *upper = from(2);
                    parsed = true;
                } else if is_start_of_number(at(1)) {
                    parsed = true;
                    *upper = from(1).wrapping_sub(1);
                }
            }
            b'>' => {
                if at(1) == b'=' && is_start_of_number(at(2)) {
                    parsed = true;
                    *lower = from(2);
                } else if is_start_of_number(at(1)) {
                    parsed = true;
                    *lower = from(1).wrapping_add(1);
                }
            }
            b'=' => {
                if is_start_of_number(at(1)) {
                    *upper = from(1);
                    *lower = *upper;
                    parsed = true;
                }
            }
            c if is_start_of_number(c) => {
                *upper = from(0);
                *lower = *upper;
                parsed = true;
            }
            _ => {}
        }
    }
    parsed
}

impl EeWonLobbyCtl {
    fn srv(&self) -> &SrvState {
        &self.ext.srv
    }

    fn mget(&self, obj: isize, f: MF) -> isize {
        self.op(Op::MGet, obj, f as isize, 0)
    }

    fn mcall(&self, obj: isize, c: MC, a: isize) -> isize {
        self.op(Op::MCall, obj, c as isize, a)
    }

    fn row_game(&self, item: isize) -> isize {
        self.op(Op::RowGame, item, 0, 0)
    }

    fn item_get(&self, item: isize, f: F) -> isize {
        self.op(Op::ItemGet, item, f as isize, 0)
    }

    fn item_set(&self, item: isize, f: F, v: isize) {
        self.op(Op::ItemSet, item, f as isize, v);
    }

    fn can_join(&self, game: isize) -> bool {
        self.mcall(game, MC::GameJoinStatus, 0) as i16 == 0
    }

    /// A text's `std::string` bytes.
    pub(crate) fn text_bytes(&self, text: isize) -> Vec<u8> {
        let mut buffer = vec![0u8; 1024];
        let n = self.op(Op::TextBytes, text, buffer.as_mut_ptr() as isize, buffer.len() as isize);
        if n as usize > buffer.len() {
            buffer = vec![0u8; n as usize];
            self.op(Op::TextBytes, text, buffer.as_mut_ptr() as isize, buffer.len() as isize);
        }
        buffer.truncate(n.max(0) as usize);
        buffer
    }

    // ---- the rows' column handlers

    fn row_set_game(&self, item: isize, game: isize) {
        self.op(Op::RowSetGame, item, game, 0);
        if self.mget(self.row_game(item), MF::GameType) as i32 == 3 {
            self.op(Op::RowSetAddrText, item, 0, 0);
        }
    }

    fn calc_icon(&self, item: isize) {
        // each `this->mGame` read again, as the original's
        let g = || self.row_game(item);
        let icon = if self.mget(g(), MF::GameInProgress) != 0 {
            0x176
        } else if self.mget(g(), MF::GameInviteOnly) != 0 {
            let owned = self.mget(g(), MF::GameInvited) != 0 || self.mget(g(), MF::GameIsHost) != 0;
            if owned {
                0x171
            } else {
                0x175
            }
        } else if self.mget(g(), MF::GameAskToJoin) != 0 {
            0x173
        } else if self.mget(g(), MF::GameHasPassword) != 0 {
            0x174
        } else {
            0x172
        };
        self.item_set(item, F::IconId, icon);
    }

    fn game_num(&self, item: isize, f: MF) -> i32 {
        self.mget(self.row_game(item), f) as i32
    }

    fn compare_column(&self, col: Col, a: isize, b: isize) -> Option<i32> {
        Some(match col {
            Col::None => return None,
            Col::Icon => (self.item_get(a, F::IconId) as i32).wrapping_sub(self.item_get(b, F::IconId) as i32),
            Col::Skill => self.game_num(a, MF::GameSkill).wrapping_sub(self.game_num(b, MF::GameSkill)),
            Col::GameName => {
                let na = self.mget(self.row_game(a), MF::GameName);
                let nb = self.mget(self.row_game(b), MF::GameName);
                self.op(Op::TextCompareNoCase, na, nb, 0) as i32
            }
            Col::Address => self.op(Op::GameCompareAddress, self.row_game(a), self.row_game(b), 0) as i32,
            Col::NumPlayers => {
                let r = self.game_num(a, MF::GameNumPlayers).wrapping_sub(self.game_num(b, MF::GameNumPlayers));
                if r == 0 {
                    self.game_num(b, MF::GameOpenSlots).wrapping_sub(self.game_num(a, MF::GameOpenSlots))
                } else {
                    r
                }
            }
            Col::Ping => (self.mget(self.row_game(a), MF::GamePing) as u16 as i32)
                .wrapping_sub(self.mget(self.row_game(b), MF::GamePing) as u16 as i32),
            Col::Epoch => self.op(Op::EeColumn, GL::EpochCompare as isize, a, b) as i32,
            Col::MapType => self.op(Op::EeColumn, GL::MapTypeCompare as isize, a, b) as i32,
        })
    }

    fn draw_res_image(&self, g: isize, res: isize, item: isize, x: i32) {
        // DrawServerImage: centred in the summary line (int division)
        let h = self.item_get(item, F::SummaryHeight) as i32;
        let ih = self.op(Op::ResImageHeight, res, 0, 0) as i32;
        self.op(Op::GfxDrawResImage, g, res, pack(x, h.wrapping_sub(ih) / 2));
    }

    fn paint_column(&self, col: Col, g: isize, item: isize) {
        match col {
            Col::None => {}
            Col::Icon => {
                let game = self.row_game(item);
                if self.mget(game, MF::GameShowDetails) != 0 {
                    self.draw_res_image(g, R::ServerListCtrl_GameExpanded_Image as isize, item, 15);
                } else {
                    self.draw_res_image(g, R::ServerListCtrl_GameNotExpanded_Image as isize, item, 15);
                }
                let icon = self.item_get(item, F::IconId);
                self.draw_res_image(g, icon, item, 0);
            }
            Col::Skill => {
                let skill = self.game_num(item, MF::GameSkill) as u8;
                if skill < 5 {
                    self.draw_res_image(g, skill as isize + 0x177, item, 0);
                }
            }
            Col::GameName => {
                self.op(Op::GfxSetFont, g, self.m(L_FONT), 0);
                let name = self.mget(self.row_game(item), MF::GameName);
                self.op(Op::GfxDrawText, g, name, pack(5, 0));
            }
            Col::Address => {
                let text = self.op(Op::RowAddrText, item, 0, 0);
                self.op(Op::GfxDrawText, g, text, 0);
            }
            Col::NumPlayers => {
                let game = self.row_game(item);
                let s = if self.mget(game, MF::GameOpenSlots) as i32 > 0 {
                    // sprintf's arguments, right to left
                    let max = self.mget(self.row_game(item), MF::GameOpenSlots) as i32;
                    let count = self.mget(self.row_game(item), MF::GameNumPlayers) as i32;
                    format!("{count}/{max}")
                } else {
                    format!("{}", self.mget(self.row_game(item), MF::GameNumPlayers) as i32)
                };
                let text = self.op(Op::TextNew, s.as_ptr() as isize, s.len() as isize, 0);
                let width = self.op(Op::ListPaintWidth, self.m(L_LIST), 0, 0) as i32;
                let text_width = self.op(Op::GfxTextWidth, g, text, 0) as i32;
                self.op(Op::GfxDrawText, g, text, pack(width.wrapping_sub(text_width) / 2, 0));
                self.op(Op::Release, text, 0, 0);
            }
            Col::Ping => {
                let ping = self.mget(self.row_game(item), MF::GamePing) as u16;
                self.op(Op::GfxDrawPing, g, ping as isize, self.item_get(item, F::SummaryHeight));
            }
            Col::Epoch => {
                self.op(Op::EeColumn, GL::EpochPaint as isize, g, item);
            }
            Col::MapType => {
                self.op(Op::EeColumn, GL::MapTypePaint as isize, g, item);
            }
        }
    }

    /// A column's filter over the rows `list` (the original's `ServerDetailsList`).
    fn filter_column(&self, col: Col, text: isize, list: &mut Vec<isize>) {
        match col {
            Col::None | Col::Icon => {}
            Col::Skill => {
                let (mut lower, mut upper) = (-100000, 100000);
                if !parse_int_filter(&self.text_bytes(text), &mut lower, &mut upper) {
                    return;
                }
                list.retain(|&item| {
                    let skill = self.game_num(item, MF::GameSkill);
                    !(skill < lower || self.game_num(item, MF::GameSkill) > upper)
                });
            }
            Col::GameName => list.retain(|&item| {
                let name = self.mget(self.row_game(item), MF::GameName);
                self.op(Op::TextFindNoCase, name, text, 0) as i32 >= 0
            }),
            Col::Address => list.retain(|&item| {
                let addr = self.op(Op::RowAddrText, item, 0, 0);
                self.op(Op::TextFindNoCase, addr, text, 0) as i32 >= 0
            }),
            Col::NumPlayers => {
                let word = |res: R| {
                    let pos = self.op(Op::TextFindRes, text, res as isize, 0) as i32;
                    pos == 0 || (pos > 0 && self.op(Op::TextAt, text, (pos - 1) as isize, 0) == b' ' as isize)
                };
                let not_full = word(R::ServerListCtrl_NotFullFilter_String);
                let full = word(R::ServerListCtrl_FullFilter_String);
                let open = word(R::ServerListCtrl_OpenFilter_String);
                if full && not_full {
                    list.clear();
                    return;
                }
                let (mut lower, mut upper) = (-100000, 100000);
                if !parse_int_filter(&self.text_bytes(text), &mut lower, &mut upper) && !full && !not_full && !open {
                    return;
                }
                list.retain(|&item| {
                    let count = self.mget(self.row_game(item), MF::GameNumPlayers) as i32;
                    let mut max = self.mget(self.row_game(item), MF::GameOpenSlots) as i32;
                    if max == 0 {
                        max = 0x7ffffff;
                    }
                    !(count < lower
                        || count > upper
                        || (full && count < max)
                        || (not_full && count >= max)
                        || (open && !self.can_join(self.row_game(item))))
                });
            }
            Col::Ping => {
                let (mut lower, mut upper) = (-100000, 100000);
                if !parse_int_filter(&self.text_bytes(text), &mut lower, &mut upper) {
                    return;
                }
                list.retain(|&item| {
                    let ping = 10 - (self.mget(self.row_game(item), MF::GamePing) as u16 as i32) / 100;
                    !(ping < lower || ping > upper)
                });
            }
            Col::Epoch => list.retain(|&item| self.op(Op::EeColumn, GL::EpochKeep as isize, item, text) != 0),
            Col::MapType => list.retain(|&item| self.op(Op::EeColumn, GL::MapTypeKeep as isize, item, text) != 0),
        }
    }

    // ---- the rows' virtuals

    /// One row call (`EeWonLobbyCtlRowCall`) of the row `item`.
    pub fn row_call(&self, call: EeWonLobbyCtlRowCall, item: isize, a: isize, b: isize, c: isize) -> isize {
        use EeWonLobbyCtlRowCall as RC;
        match call {
            RC::Paint => self.row_paint(item, a, b),
            RC::CalcDimensions => self.row_calc_dimensions(item, a),
            RC::Compare => return self.row_compare(item, a, b, c) as isize,
            RC::CanSelect => {
                let ok = self.item_get(item, F::Selectable) != 0
                    && lo(b) > self.op(Op::ColumnWidth, a, 0, 0) as i32
                    && hi(b) <= self.item_get(item, F::SummaryHeight) as i32;
                return ok as isize;
            }
            RC::MouseDown => self.row_mouse_down(item, a, b, c as i32),
            RC::SetGame => self.row_set_game(item, a),
        }
        0
    }

    fn row_calc_dimensions(&self, item: isize, list: isize) {
        let font = self.op(Op::DefaultFont, list, 0, 0);
        self.calc_icon(item);
        self.item_set(item, F::SummaryHeight, self.op(Op::FontHeight, font, 0, 0) as u8 as isize);
        let image_height = self.op(Op::ResImageHeight, R::ServerListCtrl_GameExpanded_Image as isize, 0, 0) as u16;
        if image_height as isize > self.item_get(item, F::SummaryHeight) {
            self.item_set(item, F::SummaryHeight, image_height as u8 as isize);
        }
        let sh = self.item_get(item, F::SummaryHeight);
        self.item_set(item, F::SummaryHeight, (sh as u8).wrapping_add(2) as isize);
        self.item_set(item, F::Height, self.item_get(item, F::SummaryHeight));
        if self.mget(self.row_game(item), MF::GameShowDetails) == 0 {
            return;
        }
        let mut height = self.op(Op::RowDetailsHeight, item, list, 0) as i32;
        if height < self.op(Op::FontHeight, font, 0, 0) as i32 {
            height = self.op(Op::FontHeight, font, 0, 0) as i32;
        }
        let h = self.item_get(item, F::Height) as i32;
        self.item_set(item, F::Height, h.wrapping_add(height) as isize);
    }

    fn row_mouse_down(&self, item: isize, list: isize, point: isize, button: i32) {
        if button != 0 {
            return;
        }
        if lo(point) > self.op(Op::ColumnWidth, list, 0, 0) as i32
            || hi(point) > self.item_get(item, F::SummaryHeight) as i32
        {
            return;
        }
        let game = self.row_game(item);
        let show = self.mget(self.row_game(item), MF::GameShowDetails) == 0;
        self.op(Op::MSet, game, MF::GameShowDetails as isize, show as isize);
        self.op(Op::ListRefresh, list, item, 0);
        self.op(Op::ListEnsureVisible, list, item, 0);
        if self.mget(self.row_game(item), MF::GameShowDetails) != 0 {
            self.op(Op::ListDeliverPing, list, self.row_game(item), 0);
        }
    }

    fn row_paint(&self, item: isize, g: isize, list: isize) {
        let column = self.op(Op::ListPaintColumn, list, 0, 0) as i32;
        let game = self.row_game(item);
        let mut color = if self.mget(game, MF::GameInProgress) != 0 {
            self.op(Op::GfxStandardColor, g, SC_GRAY_TEXT, 0)
        } else {
            self.op(Op::GfxStandardColor, g, SC_TEXT, 0)
        };
        if self.item_get(item, F::Selected) != 0 {
            color = self.op(Op::GfxSelFore, g, list, color);
        }
        self.op(Op::GfxSetColor, g, color, 0);
        self.op(Op::GfxSetFont, g, self.op(Op::ListFont, list, 0, 0), 0);
        if column == -1 {
            if self.item_get(item, F::Selected) != 0 {
                let back = self.op(Op::GfxSelBack, g, list, 0);
                self.op(Op::GfxSetColor, g, back, 0);
                let width = self.op(Op::ListPaintWidth, list, 0, 0) as i32;
                let sh = self.item_get(item, F::SummaryHeight) as i32;
                self.op(Op::GfxFillRect, g, 0, pack(width, sh));
            }
            if self.mget(self.row_game(item), MF::GameShowDetails) == 0 {
                return;
            }
            let width = self.op(Op::ColumnWidth, list, 0, 0) as i32;
            self.op(Op::GfxTranslate, g, width as isize, self.item_get(item, F::SummaryHeight));
            let game = self.row_game(item);
            if self.mget(game, MF::GameHaveDetails) != 0 {
                self.op(Op::RowPaintDetails, item, g, list);
            } else {
                self.op(Op::GfxSetFont, g, self.op(Op::ListFont, list, 0, 0), 0);
                self.op(Op::GfxSetSysColor, g, SC_TEXT, 0);
                if self.mcall(self.row_game(item), MC::GameQuerying, 0) != 0 {
                    self.op(Op::GfxDrawResText, g, R::ServerListCtrl_Querying_String as isize, 0);
                } else {
                    self.op(Op::GfxDrawResText, g, R::ServerListCtrl_QueryFailed_String as isize, 0);
                }
            }
            let sh = self.item_get(item, F::SummaryHeight) as i32;
            self.op(Op::GfxTranslate, g, width.wrapping_neg() as isize, sh.wrapping_neg() as isize);
            return;
        }
        let col = self.srv().columns.borrow().get(column as usize).map(|c| c.col);
        if let Some(col) = col {
            self.paint_column(col, g, item);
        }
    }

    fn row_compare(&self, item: isize, other: isize, list: isize, fallback: isize) -> i32 {
        if item == other {
            return 0;
        }
        let secondary = self.op(Op::ListSortColumnPrev, list, 0, 0) as i32;
        let primary = self.op(Op::ListSortColumn, list, 0, 0) as i32;
        let col_at = |i: i32| self.srv().columns.borrow().get(i as usize).map(|c| c.col).unwrap_or(Col::None);
        if let Some(r) = self.compare_column(col_at(primary), item, other) {
            if r != 0 {
                return r;
            }
        }
        if let Some(r) = self.compare_column(col_at(secondary), item, other) {
            if r != 0 {
                return r;
            }
        }
        fallback as i32
    }

    // ---- ServerListCtrl

    fn sel_game(&self) -> isize {
        let item = self.op(Op::ListGetSel, self.m(L_LIST), 0, 0);
        if item == 0 {
            0
        } else {
            self.row_game(item)
        }
    }

    fn do_filter(&self, list: &mut Vec<isize>) {
        // the original's local SmartPtr<LobbyGame> of the selection
        let selected = self.sel_game();
        if selected != 0 {
            self.op(Op::AddRef, selected, 0, 0);
        }
        let mut i = 0;
        loop {
            let column = match self.srv().columns.borrow().get(i) {
                Some(c) => *c,
                None => break,
            };
            if !matches!(column.col, Col::None | Col::Icon) && column.combo != 0 {
                let text = self.op(Op::ComboEditText, column.combo, 0, 0);
                if self.op(Op::TextLen, text, 0, 0) != 0 {
                    let text = self.op(Op::ComboEditText, column.combo, 0, 0);
                    self.filter_column(column.col, text, list);
                }
            }
            i += 1;
        }
        if self.srv().game_type.get() != 3 && self.pref(crate::lobbyctl::EeWonLobbyCtlPref::ShowGamesInProgress) == 0 {
            list.retain(|&item| self.mget(self.row_game(item), MF::GameInProgress) == 0);
        }
        if selected != 0 {
            self.op(Op::Release, selected, 0, 0);
        }
    }

    fn fits_filter(&self, item: isize) -> bool {
        let mut list = vec![item];
        self.do_filter(&mut list);
        !list.is_empty()
    }

    fn update_join_button(&self) {
        let game = self.sel_game();
        let enabled = game != 0 && self.can_join(game);
        self.set_enabled(self.m(L_JOIN), enabled);
        if self.m(L_REMOVE) != 0 {
            self.set_enabled(self.m(L_REMOVE), game != 0);
        }
    }

    fn add_entries(&self, list: &[isize]) {
        let selected = self.sel_game();
        let l = self.m(L_LIST);
        self.op(Op::ListBeginUpdate, l, 0, 0);
        self.op(Op::ListClear, l, 1, 0);
        for &item in list {
            let l = self.m(L_LIST);
            self.op(Op::ListInsert, l, item, 0);
            self.op(Op::MSet, self.row_game(item), MF::GameFiltered as isize, 0);
            if self.row_game(item) == selected {
                let pos = self.op(Op::ListItemPos, l, item, 0);
                self.op(Op::ListSetSel, l, pos, 1);
            }
        }
        self.op(Op::ListEndUpdate, self.m(L_LIST), 0, 0);
        self.update_join_button();
        self.update_summary_label();
    }

    fn srv_timer(&self, delta: isize) -> isize {
        let s = self.srv();
        if s.need_filter.get() {
            s.filter_time.set(s.filter_time.get().wrapping_add(delta as i32));
            let t = s.filter_time.get();
            if t >= 100 && (t >= 500 || t as u32 as usize >= s.map.borrow().len() / 4) {
                self.add_filtered_all(true);
                s.need_filter.set(false);
            }
        }
        if !s.need_filter.get() {
            self.op(Op::ClearTimerFlag, 0, 0, 0);
        }
        (self.op(Op::BaseTimer, delta, 0, 0) != 0) as isize
    }

    fn add_filtered_all(&self, preserve: bool) {
        let entries: Vec<(usize, isize)> = self.srv().map.borrow().iter().map(|(&g, &i)| (g, i)).collect();
        let mut list = Vec::with_capacity(entries.len());
        for (_, item) in entries {
            self.op(Op::MSet, self.row_game(item), MF::GameFiltered as isize, 1);
            list.push(item);
        }
        self.add_filtered(preserve, list);
    }

    fn add_filtered_list(&self, preserve: bool) {
        let l = self.m(L_LIST);
        let count = self.op(Op::ListNumItems, l, 0, 0) as i32;
        let list = (0..count).map(|i| self.op(Op::ListItem, l, i as isize, 0)).collect();
        self.add_filtered(preserve, list);
    }

    fn add_filtered(&self, preserve: bool, mut list: Vec<isize>) {
        self.srv().need_filter.set(false);
        let mut old = Vec::new();
        if preserve {
            let l = self.m(L_LIST);
            let offset = self.op(Op::ListVertOffset, l, 0, 0) as i32;
            let bottom =
                (self.op(Op::ListVertOffset, l, 0, 0) as i32).wrapping_add(self.op(Op::ListViewHeight, l, 0, 0) as i32);
            let mut index = self.op(Op::ListItemAtY, l, offset as isize, 0) as i32;
            let item = self.op(Op::ListItem, l, index as isize, 0);
            if item != 0
                && (self.item_get(item, F::Height) as i32).wrapping_add(self.item_get(item, F::Top) as i32) / 2 < offset
            {
                index += 1;
            }
            loop {
                let item = self.op(Op::ListItem, l, index as isize, 0);
                index += 1;
                if item == 0 {
                    break;
                }
                if self.item_get(item, F::Top) as i32 > bottom {
                    break;
                }
                old.push((item, (self.item_get(item, F::Top) as i32).wrapping_sub(offset)));
            }
        }
        self.do_filter(&mut list);
        self.add_entries(&list);
        if preserve {
            for (item, delta) in old {
                let l = self.m(L_LIST);
                if self.op(Op::ListItemPos, l, item, 0) as i32 >= 0 {
                    let top = self.item_get(item, F::Top) as i32;
                    self.op(Op::ListSetVertOffset, l, top.wrapping_sub(delta) as isize, 0);
                    break;
                }
            }
        }
    }

    fn handle_refresh_button(&self) -> bool {
        if self.op(Op::PingsToDo, 0, 0, 0) as i32 > 0 {
            self.op(Op::KillPings, 0, 0, 0);
            self.add_filtered_list(true);
            return true;
        }
        false
    }

    fn srv_item_clicked(&self, event: isize) {
        if !self.ask(Op::EventIsSelectedItem, event) {
            return;
        }
        let item = self.op(Op::ListGetSel, self.m(L_LIST), 0, 0);
        if item == 0 {
            return;
        }
        let game = self.row_game(item);
        if self.op(Op::EventButton, event, 0, 0) as i32 == 2 {
            self.op(Op::DeliverGameAction, game, 0, 0);
        } else if self.ask(Op::EventDoubleClick, event)
            && self.op(Op::EventButton, event, 0, 0) as i32 == 0
            && self.op(Op::EventX, event, 0, 0) as i32 >= self.op(Op::ColumnWidth, self.m(L_LIST), 0, 0) as i32
            && self.can_join(game)
        {
            self.op(Op::DeliverJoinGame, game, 0, 0);
        }
    }

    /// `mServerDetailsMap[game] = item`: a new key referenced, the item assigned.
    fn map_put(&self, game: isize, item: isize) {
        let old = self.srv().map.borrow().get(&(game as usize)).copied();
        match old {
            None => {
                self.op(Op::AddRef, game, 0, 0);
                self.srv().map.borrow_mut().insert(game as usize, 0);
                self.op(Op::AddRef, item, 0, 0);
                self.srv().map.borrow_mut().insert(game as usize, item);
            }
            Some(o) if o != item => {
                if o != 0 {
                    self.op(Op::Release, o, 0, 0);
                }
                self.srv().map.borrow_mut().insert(game as usize, item);
                self.op(Op::AddRef, item, 0, 0);
            }
            Some(_) => {}
        }
    }

    /// An entry erased: its row released, then its game.
    fn map_drop(&self, game: usize, item: isize) {
        if item != 0 {
            self.op(Op::Release, item, 0, 0);
        }
        self.op(Op::Release, game as isize, 0, 0);
    }

    pub(crate) fn map_clear(&self) {
        let entries: Vec<(usize, isize)> = std::mem::take(&mut *self.srv().map.borrow_mut()).into_iter().collect();
        for (g, i) in entries {
            self.map_drop(g, i);
        }
    }

    fn new_row(&self, game: isize) -> isize {
        let item = self.op(Op::RowNew, 0, 0, 0);
        self.row_set_game(item, game);
        self.map_put(game, item);
        item
    }

    fn handle_game_changed(&self, event: isize) {
        let game = self.op(Op::EventGame, event, 0, 0);
        if self.mget(game, MF::GameType) as i32 != self.srv().game_type.get() {
            return;
        }
        if self.ask(Op::EventIsAdd, event) {
            let item = self.new_row(game);
            let fits = self.fits_filter(item);
            self.op(Op::MSet, game, MF::GameFiltered as isize, !fits as isize);
            if fits {
                self.op(Op::ListInsert, self.m(L_LIST), item, 0);
            }
            self.op(Op::Release, item, 0, 0);
        } else if self.ask(Op::EventIsDelete, event) {
            let found = self.srv().map.borrow().get(&(game as usize)).copied();
            if let Some(item) = found {
                self.op(Op::ListRemove, self.m(L_LIST), item, 0);
                self.srv().map.borrow_mut().remove(&(game as usize));
                self.map_drop(game as usize, item);
            }
        } else {
            let Some(item) = self.srv().map.borrow().get(&(game as usize)).copied() else {
                return;
            };
            let visible = self.op(Op::ListItemPos, self.m(L_LIST), item, 0) as i32 >= 0;
            let fits = self.fits_filter(item);
            self.op(Op::MSet, self.row_game(item), MF::GameFiltered as isize, !fits as isize);
            if visible {
                if fits {
                    self.op(Op::ListRefresh, self.m(L_LIST), item, 0);
                } else {
                    self.op(Op::ListRemove, self.m(L_LIST), item, 0);
                }
            } else if fits {
                self.op(Op::ListInsert, self.m(L_LIST), item, 0);
            }
        }
        self.update_join_button();
        self.update_summary_label();
    }

    fn handle_sync_game_list(&self, event: isize) {
        let games = self.op(Op::EventGames, event, 0, 0);
        if self.mget(games, MF::GameListType) as i32 != self.srv().game_type.get() {
            return;
        }
        self.mcall(games, MC::GameListRewind, 0);
        self.map_clear();
        while self.mcall(games, MC::GameListHasMore, 0) != 0 {
            let game = self.mcall(games, MC::GameListNext, 0);
            let item = self.new_row(game);
            self.op(Op::Release, item, 0, 0);
        }
        self.add_filtered_all(false);
    }

    fn update_summary_label(&self) {
        let label = self.m(L_SUMMARY);
        if label == 0 {
            return;
        }
        let total = self.srv().map.borrow().len() as isize;
        if self.op(Op::PingsToDo, 0, 0, 0) as i32 > 0 {
            let pings = self.op(Op::PingsToDo, 0, 0, 0);
            self.op(Op::LabelSetPingSummary, label, pings, 0);
            self.op(Op::ButtonSetRes, self.m(L_REFRESH), R::ServerListCtrl_StopRefresh_String as isize, 0);
        } else {
            let shown = self.op(Op::ListNumItems, self.m(L_LIST), 0, 0);
            self.op(Op::LabelSetListSummary, label, shown, total);
            self.op(Op::ButtonSetRes, self.m(L_REFRESH), R::ServerListCtrl_RefreshList_String as isize, 0);
        }
    }

    fn column_name_text(&self, column: &Column) -> isize {
        let name = if column.name < 0 { "" } else { NAMES[column.name as usize].0 };
        self.op(Op::TextNew, name.as_ptr() as isize, name.len() as isize, 0)
    }

    fn load_filter_combos(&self) {
        let mut i = 0;
        loop {
            let column = match self.srv().columns.borrow().get(i) {
                Some(c) => *c,
                None => break,
            };
            i += 1;
            let combo = column.combo;
            if combo == 0 {
                continue;
            }
            self.op(Op::ComboClear, combo, 1, 0);
            self.op(Op::ComboInsertNoFilter, combo, self.m(L_FONT), 0);
            let name = self.column_name_text(&column);
            self.op(Op::ComboInsertFilterRow, combo, name, self.m(L_FONT));
            self.op(Op::ComboAddFilterValues, combo, name, 0);
            self.op(Op::Release, name, 0, 0);
        }
    }

    fn handle_combo_text_changed(&self, component: isize) {
        if !self.ask(Op::IsCombo, component) {
            return;
        }
        let combo = component;
        self.srv().need_filter.set(true);
        self.srv().filter_time.set(0);
        self.op(Op::RequestTimer, 1, 0, 0);
        let text = self.op(Op::ComboEditText, combo, 0, 0);
        let filter = self.op(Op::ComboItem, combo, FILTER_POS, 0);
        if filter == 0 {
            return;
        }
        if self.op(Op::TextLen, text, 0, 0) == 0 {
            self.op(Op::FilterClear, filter, 0, 0);
            self.op(Op::ComboUpdateItem, combo, filter, 0);
            return;
        }
        let count = self.op(Op::ComboCount, combo, 0, 0) as i32;
        let mut pos = 0;
        while pos < count {
            let item = self.op(Op::ComboItem, combo, pos as isize, 0);
            if self.ask_with(Op::TextItemIs, item, text) {
                if pos as isize <= FILTER_POS {
                    return;
                }
                break;
            }
            pos += 1;
        }
        let is_add = pos >= count;
        self.op(Op::FilterSet, filter, (pos as u32 as isize) | ((is_add as isize) << 32), text);
        self.op(Op::ComboUpdateItem, combo, filter, 0);
    }

    fn handle_combo_sel_changed(&self, event: isize, component: isize) {
        if !self.ask(Op::IsCombo, component) {
            return;
        }
        let combo = component;
        let item = self.op(Op::EventItem, event, 0, 0);
        let pos = self.op(Op::ComboItemPos, combo, item, 0) as i32;
        if pos as isize > FILTER_POS {
            return;
        }
        if pos == 0 && self.ask(Op::EventActiveSel, event) {
            self.op(Op::ComboSetText, combo, 0, 0);
            return;
        }
        let filter = self.op(Op::ComboItem, combo, FILTER_POS, 0);
        if self.ask(Op::EventActiveSel, event) {
            if self.ask(Op::FilterIsAdd, filter) {
                let text = self.op(Op::FilterText, filter, 0, 0);
                if self.op(Op::TextLen, text, 0, 0) != 0 {
                    let added = self.op(Op::ComboAddString, combo, self.op(Op::FilterText, filter, 0, 0), 0);
                    let at = self.op(Op::ComboItemPos, combo, added, 0);
                    self.op(Op::ComboSetSel, combo, at, 1);
                    let column = self.op(Op::FilterColumn, filter, 0, 0);
                    self.op(Op::AddFilterValue, column, self.op(Op::FilterText, filter, 0, 0), 0);
                } else {
                    self.op(Op::ComboSetText, combo, 0, 0);
                }
            } else if self.op(Op::FilterSelPos, filter, 0, 0) > FILTER_POS {
                self.op(Op::ComboDeleteItem, combo, self.op(Op::FilterSelPos, filter, 0, 0), 0);
                self.op(Op::ComboSetText, combo, 0, 0);
                let column = self.op(Op::FilterColumn, filter, 0, 0);
                self.op(Op::RemoveFilterValue, column, self.op(Op::FilterText, filter, 0, 0), 0);
            }
        } else {
            self.op(Op::ComboSetText, combo, self.op(Op::FilterText, filter, 0, 0), 0);
            self.op(Op::ComboSelectEnd, combo, 0, 0);
        }
    }

    pub(crate) fn srv_event(&self, event: isize, kind: Ev, component: isize) {
        match kind {
            Ev::ButtonPressed => {
                if component == self.m(L_JOIN) {
                    let game = self.sel_game();
                    if game != 0 {
                        self.op(Op::DeliverJoinGame, game, 0, 0);
                    }
                    return;
                }
                if component == self.m(L_REFRESH) && self.handle_refresh_button() {
                    return;
                }
            }
            Ev::ListSelChanged => self.update_join_button(),
            Ev::InputTextChanged => return self.handle_combo_text_changed(component),
            Ev::ComboSelChanged => return self.handle_combo_sel_changed(event, component),
            Ev::ListItemClicked => return self.srv_item_clicked(event),
            Ev::GameChanged => return self.handle_game_changed(event),
            Ev::SyncGameList => return self.handle_sync_game_list(event),
            Ev::SyncOptions | Ev::FiltersChanged => return self.load_filter_combos(),
            Ev::RefilterAll => return self.add_filtered_all(true),
            Ev::RefilterShown => return self.add_filtered_list(true),
            _ => {}
        }
        self.op(Op::BaseEvent, event, 0, 0);
    }

    pub(crate) fn srv_init(&self, config: isize, local: &Cell<isize>) {
        self.find(config, crate::lobbyctl::SLOT_SERVER_LIST, local);
        let font = self.op(Op::DefaultFontMod, self.m(L_LIST), 1, 0);
        self.assign(&self.members[L_FONT], font);
        for slot in crate::lobbyctl::SLOT_SERVER_LIST + 1..=crate::lobbyctl::SLOT_SERVER_LIST + 3 {
            self.find(config, slot, local);
        }
        let remove = self.op(Op::FindOptional, config, crate::lobbyctl::SLOT_SERVER_LIST as isize + 4, 0);
        self.assign(&self.members[L_REMOVE], remove);
        self.update_join_button();
        self.update_summary_label();
        self.op(Op::SubscribeBroadcast, 1, 0, 0);
    }

    pub(crate) fn srv_timer_call(&self, delta: isize) -> isize {
        self.srv_timer(delta)
    }

    /// The config reader's calls (`EeWonLobbyCtlCfg`).
    pub(crate) fn srv_config(&self, call: EeWonLobbyCtlCfg, a: isize, b: isize) -> isize {
        use EeWonLobbyCtlCfg as C;
        let s = self.srv();
        match call {
            C::SetGameType => s.game_type.set(a as i32),
            C::SetNumColumns => {
                s.num_columns.set(a as i32);
                let n = a.max(0) as usize;
                let dropped: Vec<Column> = {
                    let mut c = s.columns.borrow_mut();
                    if c.len() > n {
                        c.split_off(n)
                    } else {
                        c.resize(n, Column::DEFAULT);
                        Vec::new()
                    }
                };
                for d in dropped.into_iter().rev() {
                    if d.combo != 0 {
                        self.op(Op::Release, d.combo, 0, 0);
                    }
                }
            }
            C::ColumnCount => return s.columns.borrow().len() as isize,
            C::SetList => self.assign(&self.members[L_LIST], a),
            C::List => return self.m(L_LIST),
            C::GameType => return s.game_type.get() as isize,
            C::AddColumn => {
                // a: the name's bytes, b: their count | the combo << ... (the combo is AddColumnCombo's)
                // SAFETY: the reader's bytes
                let name = unsafe { std::slice::from_raw_parts(a as *const u8, b as usize) };
                let Some((i, flags)) = column_by_name(name) else {
                    return -1;
                };
                let shown = match s.game_type.get() {
                    3 => flags & 4 != 0,
                    2 => flags & 1 != 0,
                    1 => flags & 2 != 0,
                    _ => false,
                };
                if !shown {
                    return 0;
                }
                return 1 + i as isize;
            }
            C::PushColumn => {
                // a: the definition (AddColumn's result - 1), b: the combo
                let i = a as usize;
                if b != 0 {
                    self.op(Op::AddRef, b, 0, 0);
                }
                s.columns.borrow_mut().push(Column { col: NAMES[i].1, name: i as i32, combo: b });
            }
            C::SelGame => return self.sel_game(),
        }
        0
    }

    /// The destructor's part beyond the controls: the rows, then (after the controls) the columns' combo boxes.
    pub(crate) fn srv_release_rows(&self) {
        self.map_clear();
    }

    pub(crate) fn srv_release_columns(&self) {
        let columns = std::mem::take(&mut *self.srv().columns.borrow_mut());
        for c in columns {
            if c.combo != 0 {
                self.op(Op::Release, c.combo, 0, 0);
            }
        }
    }
}

/// The game list's config reader's calls (ServerListCtrlConfig, LobbyCtrlHost.cpp).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonLobbyCtlCfg {
    /// `SetGameType(a)`.
    SetGameType = 0,
    /// `SetNumColumns(a)`.
    SetNumColumns,
    /// `mColumnInfo.size()`.
    ColumnCount,
    /// `mList = (MultiListArea*)a`.
    SetList,
    /// `mList.get()`.
    List,
    /// `mGameType`.
    GameType,
    /// The column named by `b` bytes at `a`: -1 not defined, 0 not for this game type, else 1 + its definition.
    AddColumn,
    /// `mColumnInfo.push_back` of the definition `a` with the combo box `b`.
    PushColumn,
    /// `GetSelGame()`.
    SelGame,
}

#[cfg(test)]
#[path = "srvlistctl_tests.rs"]
mod tests;
