//! WONGUI's pure-table resource-config handlers as one table reader (R6 WON round 39).
//!
//! A config handler (`MSButtonConfig::Virtual24` and 44 others: the MS controls' configs, the scroll and list
//! configs, the skinned WON configs, the simple components') compared the instruction with its keys (guiconf's
//! tables) and, for the key that matched, read its arguments in order (`ReadInt`, `EnsureComma`, `ReadBool`, ...)
//! and called one setter or a few. Those reads and calls are the programs here, one per key of each handler, in the
//! handlers' order. The C++ host keeps what needs the live objects: each read (`ReadInt`'s expressions over live
//! components, the font, image, sound and component lookups, the typed component checks) fills a frame of values on
//! the C++ side, and each setter is one case of the host's apply over that frame (`table << 8 | setter`). The name
//! reads (flags, states, quadrants, sort types, ...) match the last string read against guiconf's name tables here
//! and put the value the original assigned into the frame, or have the host throw the original's message.
//!
//! A read, a comma or a setter that throws in the C++ stops the program: the host keeps the exception and rethrows
//! it after `ee_won_cfg_tab_run` returns, so no C++ exception crosses this code.
//!
//! `ButtonConfig::HandleButtonFlags` tested the name index 5 twice (WantDefault); the second test could never match.
//! The flag table maps each name once, so the program keeps the original's behaviour without the dead test.

use std::ffi::c_void;

use crate::guiconf::*;

/// A read the host makes into its frame.
#[repr(i32)]
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum EeWonCfgRead {
    /// `ReadInt()` (an expression) into the ints.
    Int = 0,
    /// `ReadBool()` into the ints.
    Bool = 1,
    /// `ReadColor()` into the ints.
    Color = 2,
    /// `ReadChar()` into the ints.
    Char = 3,
    /// `ReadVertAlign()` into the ints.
    VertAlign = 4,
    /// `ReadHorzAlign()` into the ints.
    HorzAlign = 5,
    /// `ReadString(false)` into the strings.
    Str = 6,
    /// `ReadFlag(&on)` (on starting true): the flag into the strings, on into the ints.
    Flag = 7,
    /// `ReadFont()` into the fonts.
    Font = 8,
    /// `ReadImage(false)` into the images.
    Image = 9,
    /// `ReadSound()`.
    Sound = 10,
    /// `ReadBackground()`.
    Background = 11,
    /// `ReadSelectionColor()`.
    SelColor = 12,
    /// `ReadComponent()` into the components.
    Comp = 13,
    /// `SafeConfigGetComponent<Container*>(ReadComponent(),"Container")` into the components.
    Container = 14,
    /// The same as a Button.
    Button = 15,
    /// The same as a Scroller.
    Scroller = 16,
    /// The same as a MultiListArea.
    MultiListArea = 17,
    /// `dynamic_cast<ScrollArea*>(ReadComponent())`, "Expecting ScrollArea" when it is not one.
    ScrollArea = 18,
    /// WONFrameValueConfig's frame value (an image, one or four borders).
    Frame = 19,
}

/// The host: the C++ config's reads, its frame and its setters. Every callback that can throw catches and keeps
/// the exception and answers false.
#[repr(C)]
pub struct EeWonCfgTabHost {
    pub ctx: *mut c_void,
    /// A read into the frame.
    pub read: extern "C" fn(ctx: *mut c_void, kind: EeWonCfgRead) -> bool,
    /// `EnsureComma()`.
    pub comma: extern "C" fn(ctx: *mut c_void) -> bool,
    /// `EndOfString()` (it skips whitespace).
    pub end: extern "C" fn(ctx: *mut c_void) -> bool,
    /// The frame's string `slot`: its units (`mText`), their count and size.
    pub text: extern "C" fn(ctx: *mut c_void, slot: i32, len: *mut u16, unit: *mut u32) -> *const c_void,
    /// A value into the frame's ints.
    pub push: extern "C" fn(ctx: *mut c_void, value: i32),
    /// The setter `table << 8 | n` over the frame.
    pub apply: extern "C" fn(ctx: *mut c_void, setter: i32) -> bool,
    /// Keep a ConfigObjectException of `len` bytes of `msg`, then the frame's string `slot` narrowed (none if < 0).
    pub fail: extern "C" fn(ctx: *mut c_void, msg: *const u8, len: usize, slot: i32),
    /// Empty the frame.
    pub clear: extern "C" fn(ctx: *mut c_void),
}

/// What a name read does with a name its table does not hold.
#[derive(Clone, Copy, Debug)]
pub enum Miss {
    /// Throw the message followed by the name.
    Throw(&'static [u8]),
    /// Throw the message alone.
    Bare(&'static [u8]),
    /// Do nothing more (the entry is handled).
    Skip,
}

/// One step of a program.
#[derive(Clone, Copy, Debug)]
pub enum Op {
    /// A read.
    R(EeWonCfgRead),
    /// `EnsureComma()`.
    C,
    /// The setter `n` of the program's table.
    S(u8),
    /// A constant into the ints.
    K(i32),
    /// The last string read, by the name table: the value at its place into the ints.
    N(i32, &'static [i32], Miss),
    /// To the step if `EndOfString()` (255: the end).
    Je(u8),
    /// To the step.
    J(u8),
    /// To the step if the last string read is empty.
    Jz(u8),
    /// Empty the frame (a loop's next turn).
    X,
}

use EeWonCfgRead as Rd;
use Op::*;

const END: u8 = 255;
const I: Op = R(Rd::Int);
const B: Op = R(Rd::Bool);
const COL: Op = R(Rd::Color);
const STR: Op = R(Rd::Str);
const IMG: Op = R(Rd::Image);
const COMP: Op = R(Rd::Comp);

/// A text-item list: `while(!EndOfString()) { add(ReadString(false)); if(!EndOfString()) EnsureComma(); }`.
const fn items(s: u8) -> [Op; 7] {
    [Je(END), X, STR, S(s), Je(0), C, J(0)]
}
/// A flag list: `for(;;) { on=true; f=ReadFlag(&on); if(f.length()==0) break; set(value(f),on); }`.
const fn flags(t: i32, v: &'static [i32], m: &'static [u8], s: u8) -> [Op; 6] {
    [X, R(Rd::Flag), Jz(END), N(t, v, Miss::Throw(m)), S(s), J(0)]
}

const LIST_FLAGS: &[i32] = &[2, 4, 8, 0x10, 0x20, 0x40, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000];
const SORT_TYPES: &[i32] = &[0, 1, 2];
const SORT_TYPE: Op =
    N(EE_WON_CFG_NAMES_LISTAREACONFIG_READSORTTYPE_NAME, SORT_TYPES, Miss::Bare(b"Invalid list sort type."));
const QUADRANT: Op = N(
    EE_WON_CFG_NAMES_SPLITTERCONFIG_READQUADRANT_ASTR,
    &[1, 4, 2, 8, 3, 6, 9, 12],
    Miss::Throw(b"Invalid quadrant: "),
);
const SPLITTER: Op = N(
    EE_WON_CFG_NAMES_SPLITTERCONFIG_READSPLITTERTYPE_ASTR,
    &[5, 10, 0x15, 0x25, 0x4a, 0x8a, 0x10f],
    Miss::Throw(b"Invalid splitter type: "),
);
const CONDITION: Op = N(
    EE_WON_CFG_NAMES_SCROLLBARSCROLLERCONFIG_GETSCROLLBARCONDITION_CONDITION,
    &[0, 1, 2],
    Miss::Throw(b"Invalid scrollbar condition: "),
);

/// The handlers: (key table, the program of each key in the table's order).
#[rustfmt::skip]
static PROGRAMS: &[(i32, &[&[Op]])] = &[
    // ButtonConfig::Virtual24: BUTTONFLAGS TEXT SETCHECK CLICKSOUND FITTEXT
    (EE_WON_CFG_KEYS_BUTTONCONFIG_VIRTUAL24_ININSTRUCTION, &[
        &flags(EE_WON_CFG_NAMES_BUTTONCONFIG_HANDLEBUTTONFLAGS_FLAG, &[1, 2, 4, 8, 0x40, 0x400, 0x100, 0x200], b"Unknown button flag", 0),
        &[STR, S(1)], &[B, S(2)], &[R(Rd::Sound), S(3)], &[S(4)],
    ]),
    // MSButtonConfig: FONT TEXTCOLOR VERTALIGN HORZALIGN TRANSPARENT ADJUSTHEIGHTTOFITTEXT WRAP INCREMENTHORZTEXTPAD INCREMENTVERTEXTPAD
    (EE_WON_CFG_KEYS_MSBUTTONCONFIG_VIRTUAL24_KEY, &[
        &[R(Rd::Font), S(0)], &[COL, S(1)], &[R(Rd::VertAlign), S(2)], &[R(Rd::HorzAlign), S(3)], &[B, S(4)], &[B, S(5)],
        &[B, S(6)], &[I, C, I, S(7)], &[I, C, I, S(8)],
    ]),
    // MSArrowButtonConfig: DIRECTION (Up Down Left Right; another word is ignored)
    (EE_WON_CFG_KEYS_MSARROWBUTTONCONFIG_VIRTUAL24_KEY, &[
        &[STR, N(EE_WON_CFG_NAMES_MSARROWBUTTONCONFIG_VIRTUAL24_DIRECTION, &[2, 3, 0, 1], Miss::Skip), S(0)],
    ]),
    // MSTabButtonConfig: LOWERAMOUNT
    (EE_WON_CFG_KEYS_MSTABBUTTONCONFIG_VIRTUAL24_KEY, &[&[I, S(0)]]),
    // MS3DFrameConfig: FRAMETYPE UP
    (EE_WON_CFG_KEYS_MS3DFRAMECONFIG_VIRTUAL24_KEY, &[&[I, S(0)], &[B, S(1)]]),
    // MSComboBoxConfig: DRAWFRAME
    (EE_WON_CFG_KEYS_MSCOMBOBOXCONFIG_VIRTUAL24_KEY, &[&[B, S(0)]]),
    // MSScrollbarConfig: TRANSPARENT
    (EE_WON_CFG_KEYS_MSSCROLLBARCONFIG_VIRTUAL24_KEY, &[&[B, S(0)]]),
    // MSEtchedFrameGroupConfig: TITLE CONTAINEDCOMPONENT
    (EE_WON_CFG_KEYS_MSETCHEDFRAMEGROUPCONFIG_VIRTUAL24_KEY, &[&[STR, S(0)], &[COMP, S(1)]]),
    // MSTabCtrlConfig: TRANSPARENT ADDSTANDARDTAB
    (EE_WON_CFG_KEYS_MSTABCTRLCONFIG_VIRTUAL24_KEY, &[&[B, S(0)], &[STR, C, R(Rd::Container), S(1)]]),
    // MSScrollContainerConfig: DRAWFRAME DRAW3DFRAME TRANSPARENT
    (EE_WON_CFG_KEYS_MSSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, &[&[B, S(0)], &[B, S(1)], &[B, S(2)]]),
    // MSHeaderControlConfig: SETSTANDARDCOLUMN
    (EE_WON_CFG_KEYS_MSHEADERCONTROLCONFIG_VIRTUAL24_KEY, &[&[I, C, STR, S(0)]]),
    // MSListCtrlConfig: HEADERHEIGHT
    (EE_WON_CFG_KEYS_MSLISTCTRLCONFIG_VIRTUAL24_KEY, &[&[I, S(0)]]),
    // PercentLayoutConfig: REF LEFT RIGHT TOP BOTTOM (a reference per edge), PX PY PW PH LP TP RP BP (a parameter)
    (EE_WON_CFG_KEYS_PERCENTLAYOUTCONFIG_VIRTUAL24_INSTRUCTION, &[
        &[K(0), COMP, S(0)], &[K(0), COMP, S(0)], &[K(1), COMP, S(0)], &[K(2), COMP, S(0)], &[K(3), COMP, S(0)],
        &[K(0), I, S(1)], &[K(1), I, S(1)], &[K(2), I, S(1)], &[K(3), I, S(1)],
        &[K(4), I, S(1)], &[K(5), I, S(1)], &[K(6), I, S(1)], &[K(7), I, S(1)],
    ]),
    // ColorSchemeConfig: the 17 standard colours
    (EE_WON_CFG_KEYS_COLORSCHEMECONFIG_VIRTUAL24_INSTRUCTION, &[
        &[K(0), COL, S(0)], &[K(1), COL, S(0)], &[K(2), COL, S(0)], &[K(3), COL, S(0)], &[K(4), COL, S(0)],
        &[K(5), COL, S(0)], &[K(6), COL, S(0)], &[K(7), COL, S(0)], &[K(8), COL, S(0)], &[K(9), COL, S(0)],
        &[K(10), COL, S(0)], &[K(11), COL, S(0)], &[K(12), COL, S(0)], &[K(13), COL, S(0)], &[K(14), COL, S(0)],
        &[K(15), COL, S(0)], &[K(16), COL, S(0)],
    ]),
    // RectangleComponentConfig: SOLIDCOLOR OUTLINECOLOR
    (EE_WON_CFG_KEYS_RECTANGLECOMPONENTCONFIG_VIRTUAL24_KEY, &[&[COL, S(0)], &[COL, S(1)]]),
    // RectangleComponent3DConfig: SOLIDCOLOR LIGHTCOLOR DARKCOLOR UP THICKNESS
    (EE_WON_CFG_KEYS_RECTANGLECOMPONENT3DCONFIG_VIRTUAL24_KEY, &[&[COL, S(0)], &[COL, S(1)], &[COL, S(2)], &[B, S(3)], &[I, S(4)]]),
    // ImageComponentConfig: IMAGE TILE STRETCH
    (EE_WON_CFG_KEYS_IMAGECOMPONENTCONFIG_VIRTUAL24_KEY, &[&[IMG, S(0)], &[B, S(1)], &[B, S(2)]]),
    // ScreenContainerConfig: ADDSCREEN SHOWSCREEN
    (EE_WON_CFG_KEYS_SCREENCONTAINERCONFIG_VIRTUAL24_KEY, &[&[COMP, S(0)], &[COMP, S(1)]]),
    // MultiListAreaConfig: SORTCOLUMN COLUMNWIDTH NUMCOLUMNS DYNAMICSIZECOLUMN SORTCOLUMNFUNC AUTOGROWCOLUMNS COLUMNHALIGN
    (EE_WON_CFG_KEYS_MULTILISTAREACONFIG_VIRTUAL24_INSTRUCTION, &[
        &[I, C, STR, SORT_TYPE, S(0)], &[I, C, I, S(1)], &[I, S(2)], &[I, S(3)],
        &[I, C, STR, N(EE_WON_CFG_NAMES_MULTILISTAREACONFIG_VIRTUAL24_NAME, &[0], Miss::Throw(b"Unknown sort func: ")), S(4)],
        &[B, S(5)], &[I, C, R(Rd::HorzAlign), S(6)],
    ]),
    // MultiListHeaderConfig: SETCOLUMN MULTILISTAREA
    (EE_WON_CFG_KEYS_MULTILISTHEADERCONFIG_VIRTUAL24_INSTRUCTION, &[&[I, C, COMP, S(0)], &[R(Rd::MultiListArea), S(1)]]),
    // ScrollbarConfig: VERTICAL RESIZETHUMB MINTHUMBSIZE
    (EE_WON_CFG_KEYS_SCROLLBARCONFIG_VIRTUAL24_INSTRUCTION, &[&[B, S(0)], &[B, S(1)], &[I, S(2)]]),
    // TabCtrlConfig: ADDTAB TABBARONRIGHT ARROWSONLEFT TABBARHEIGHT TABOVERLAP TABBAROVERLAP TABVERTOFFSET TABHORZOFFSET
    (EE_WON_CFG_KEYS_TABCTRLCONFIG_VIRTUAL24_KEY, &[
        &[R(Rd::Button), C, R(Rd::Container), S(0)], &[B, S(1)], &[B, S(2)], &[I, S(3)], &[I, S(4)], &[I, S(5)],
        &[I, S(6)], &[I, S(7)],
    ]),
    // BrowserComponentConfig: TRANSPARENT (no argument) SCROLLER FONTSIZE
    (EE_WON_CFG_KEYS_BROWSERCOMPONENTCONFIG_VIRTUAL24_INSTRUCTION, &[
        &[S(0)], &[R(Rd::Scroller), S(1)],
        &[STR, N(EE_WON_CFG_NAMES_BROWSERCOMPONENTCONFIG_VIRTUAL24_VALUE, &[0, 1, 2, 3, 4], Miss::Throw(b"Invalid font size: ")), S(2)],
    ]),
    // ComboBoxConfig: LISTBOXWIDTH LISTBOXHEIGHT LISTBOXSIZE (no comma) TEXT TEXTITEM SELITEM NOTYPE MAXCHARS SHOWBUTTONWHENDISABLED
    (EE_WON_CFG_KEYS_COMBOBOXCONFIG_VIRTUAL24_KEY, &[
        &[I, S(0)], &[I, S(1)], &[I, I, S(2)], &[STR, S(3)], &items(4), &[I, S(5)], &[B, S(6)], &[I, S(7)], &[B, S(8)],
    ]),
    // DialogConfig: ALLOWDRAG KEEPINROOT ENDONESCAPE DONTENDONESCAPE DEFAULTBUTTON
    (EE_WON_CFG_KEYS_DIALOGCONFIG_VIRTUAL24_INSTRUCTION, &[&[B, S(0)], &[B, S(1)], &[I, S(2)], &[S(3)], &[R(Rd::Button), S(4)]]),
    // ImageButtonConfig: NORMALIMAGE OVERIMAGE DOWNIMAGE DISABLEDIMAGE CHECKEDIMAGE
    (EE_WON_CFG_KEYS_IMAGEBUTTONCONFIG_VIRTUAL24_INSTRUCTION, &[
        &[K(0), IMG, S(0)], &[K(1), IMG, S(0)], &[K(2), IMG, S(0)], &[K(3), IMG, S(0)], &[K(4), IMG, S(0)],
    ]),
    // LabelConfig: TEXT FONT TEXTCOLOR BACKGROUND VERTALIGN HORZALIGN WRAP ADJUSTHEIGHTTOFITTEXT
    (EE_WON_CFG_KEYS_LABELCONFIG_VIRTUAL24_INSTRUCTION, &[
        &[STR, S(0)], &[R(Rd::Font), S(1)], &[COL, S(2)], &[R(Rd::Background), S(3)], &[R(Rd::VertAlign), S(4)],
        &[R(Rd::HorzAlign), S(5)], &[B, S(6)], &[B, S(7)],
    ]),
    // ListAreaConfig: LISTFLAGS TEXTITEM FONT TEXTCOLOR SELCOLOR LISTSORTTYPE
    (EE_WON_CFG_KEYS_LISTAREACONFIG_VIRTUAL24_ININSTRUCTION, &[
        &flags(EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME, LIST_FLAGS, b"Unknown list flag: ", 0),
        &items(1), &[R(Rd::Font), S(2)], &[COL, S(3)], &[R(Rd::SelColor), S(4)], &[STR, SORT_TYPE, S(5)],
    ]),
    // InputBoxConfig: TEXT PASSWORDCHAR ALLOWSELECTION MAXCHARS CHARWIDTH BACKGROUND DISABLEDBACKGROUND ICON TEXTCOLOR
    // DISABLEDTEXTCOLOR SELCOLOR SELONFOCUS MAXINPUTHISTORY USEDISABLEDCOLORS CURSORBLINKRATE (on[,off])
    (EE_WON_CFG_KEYS_INPUTBOXCONFIG_VIRTUAL24_ININSTRUCTION, &[
        &[STR, S(0)], &[R(Rd::Char), S(1)], &[B, S(2)], &[I, S(3)], &[I, S(4)], &[R(Rd::Background), S(5)],
        &[R(Rd::Background), S(6)], &[IMG, S(7)], &[COL, S(8)], &[COL, S(9)], &[R(Rd::SelColor), S(10)], &[B, S(11)],
        &[I, S(12)], &[B, S(13)], &[I, Je(4), C, I, S(14)],
    ]),
    // TextAreaConfig: TEXTFLAGS TEXT FORMATTEDTEXT FONT TEXTCOLOR SELCOLOR LINKFONT MAXCHARS MAXLINES WRAPINDENTSIZE HORZALIGN
    (EE_WON_CFG_KEYS_TEXTAREACONFIG_VIRTUAL24_KEY, &[
        &flags(EE_WON_CFG_NAMES_TEXTAREACONFIG_VIRTUAL24_FLAG, &[1, 2, 4, 8, 0x10, 0x20], b"Unknown text flag: ", 0),
        &[STR, S(1)], &[STR, S(2)], &[R(Rd::Font), S(3)], &[COL, S(4)], &[R(Rd::SelColor), S(5)], &[R(Rd::Font), S(6)],
        &[I, S(7)], &[I, S(8)], &[I, S(9)], &[R(Rd::HorzAlign), S(10)],
    ]),
    // TreeAreaConfig: LISTSORTTYPE
    (EE_WON_CFG_KEYS_TREEAREACONFIG_VIRTUAL24_INSTRUCTION, &[&[STR, SORT_TYPE, S(0)]]),
    // WONSimpleButtonConfig: SCALEIMAGE TYPE BUTTONSTATE BUTTONSTATES (up to four images) CHECKMOUSEBUTTONDOWN
    (EE_WON_CFG_KEYS_WONSIMPLEBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, &[
        &[B, S(0)], &[STR, S(1)],
        &[STR, C, N(EE_WON_CFG_NAMES_WONSIMPLEBUTTONCONFIG_HANDLEBUTTONSTATE_ASTATESTR, &[0, 1, 2, 3], Miss::Throw(b"Invalid button state: ")), IMG, S(2)],
        &[X, K(0), IMG, S(2), Je(END), C, X, K(1), IMG, S(2), Je(END), C,
          X, K(2), IMG, S(2), Je(END), C, X, K(3), IMG, S(2), Je(END), C],
        &[B, S(3)],
    ]),
    // WONButtonConfig: FONT TYPE TEXTCOLOR DEFAULTTEXTCOLOR VERTALIGN HORZALIGN TRANSPARENT ADJUSTHEIGHTTOFITTEXT WRAP
    // DRAWGRAYTEXT TEXTPADDING FOCUSPADDING HORZTEXTPAD (left[,right]) INCREMENTHORZTEXTPAD and INCREMENTVERTTEXTPAD
    // (accepted, ignored) BUTTONSTATE (state, image[, colour[, font]]) SCALEIMAGE FOCUSSTYLE
    (EE_WON_CFG_KEYS_WONBUTTONCONFIG_VIRTUAL24_THEINSTRUCTION, &[
        &[R(Rd::Font), S(0)], &[STR, S(1)], &[COL, S(2)], &[COL, S(3)], &[R(Rd::VertAlign), S(4)], &[R(Rd::HorzAlign), S(5)],
        &[B, S(6)], &[B, S(7)], &[B, S(8)], &[B, S(9)], &[I, C, I, C, I, C, I, S(10)], &[I, C, I, C, I, C, I, S(11)],
        &[I, Je(4), C, I, S(12)], &[], &[],
        // the state's value: the state, and 0x100 for the checked states
        &[STR, C, N(EE_WON_CFG_NAMES_WONBUTTONCONFIG_HANDLESETBUTTONSTATE_ASTATESTR,
                    &[0, 1, 2, 3, 4, 0x100, 0x101, 0x102, 0x103, 0x104], Miss::Throw(b"Invalid button state: ")),
          IMG, Je(10), C, COL, Je(10), C, R(Rd::Font), S(15)],
        &[B, S(16)], &[B, S(17)],
    ]),
    // SplitterConfig: ADDQUADRANT ADDSPLITTER ADDINVISIBLESPLITTER (type[,size]) ADDCOMPLEXSPLITTER (container,type[,drag])
    // MINLEFTRIGHT MINTOPBOTTOM SPLITTERPERCENTAGES MAXQUADRANT UNMAXQUADRAN SPLITTERPOS REGISTERMAXBUTTON
    (EE_WON_CFG_KEYS_SPLITTERCONFIG_VIRTUAL24_THEINSTRUCTION, &[
        &[COMP, C, STR, QUADRANT, S(0)], &[COMP, C, STR, SPLITTER, S(1)], &[STR, SPLITTER, Je(5), C, I, S(2)],
        &[R(Rd::Container), C, STR, SPLITTER, Je(7), C, COMP, S(3)], &[I, C, I, S(4)], &[I, C, I, S(5)], &[I, C, I, S(6)],
        &[STR, QUADRANT, S(7)], &[S(8)], &[I, C, I, S(9)], &[R(Rd::Button), C, STR, QUADRANT, S(10)],
    ]),
    // WONScrollbarConfig: TRANSPARENT
    (EE_WON_CFG_KEYS_WONSCROLLBARCONFIG_VIRTUAL24_KEY, &[&[B, S(0)]]),
    // WONHeaderControlConfig: SETSTANDARDCOLUMN (after the list header config's keys)
    (EE_WON_CFG_KEYS_WONHEADERCONTROLCONFIG_VIRTUAL24_KEY, &[&[I, C, STR, S(0)]]),
    // WONListCtrlConfig: HEADERHEIGHT NUMCOLUMNS DYNAMICSIZECOLUMN LISTFLAGS SETSTANDARDCOLUMN SETCOLUMN
    (EE_WON_CFG_KEYS_WONLISTCTRLCONFIG_VIRTUAL24_KEY, &[
        &[I, S(0)], &[I, S(1)], &[I, S(2)], &flags(EE_WON_CFG_NAMES_LISTAREACONFIG_READLISTFLAGS_NAME, LIST_FLAGS, b"Unknown list flag: ", 3),
        &[I, C, STR, C, I, S(4)], &[I, C, COMP, C, I, S(5)],
    ]),
    // WONFrameComponentConfig: BORDERS (each set as read) IMAGE
    (EE_WON_CFG_KEYS_WONFRAMECOMPONENTCONFIG_VIRTUAL24_KEY, &[&[I, S(0), C, I, S(1), C, I, S(2), C, I, S(3)], &[IMG, S(4)]]),
    // WONComboBoxConfig: DRAWFRAME BUTTONINFRAME FRAME
    (EE_WON_CFG_KEYS_WONCOMBOBOXCONFIG_VIRTUAL24_KEY, &[&[B, S(0)], &[B, S(1)], &[R(Rd::Frame), S(2)]]),
    // WONTabCtrlConfig: TRANSPARENT ADDSTANDARDTAB FRAME
    (EE_WON_CFG_KEYS_WONTABCTRLCONFIG_VIRTUAL24_KEY, &[&[B, S(0)], &[STR, C, R(Rd::Container), S(1)], &[R(Rd::Frame), S(2)]]),
    // WONScrollContainerConfig: DRAWFRAME TRANSPARENT FRAME CORNER
    (EE_WON_CFG_KEYS_WONSCROLLCONTAINERCONFIG_VIRTUAL24_KEY, &[&[B, S(0)], &[B, S(1)], &[R(Rd::Frame), S(2)], &[IMG, S(3)]]),
    // ScrollAreaConfig: BACKGROUND BORDERPADDING
    (EE_WON_CFG_KEYS_SCROLLAREACONFIG_VIRTUAL24_INSTRUCTION, &[&[R(Rd::Background), S(0)], &[I, C, I, C, I, C, I, S(1)]]),
    // ScrollerConfig: SCROLLAREA
    (EE_WON_CFG_KEYS_SCROLLERCONFIG_VIRTUAL24_INSTRUCTION, &[&[R(Rd::ScrollArea), S(0)]]),
    // ScrollbarScrollerConfig: SCROLLBARCONDITIONS (horizontal, vertical)
    (EE_WON_CFG_KEYS_SCROLLBARSCROLLERCONFIG_VIRTUAL24_INSTRUCTION, &[&[STR, CONDITION, C, STR, CONDITION, S(0)]]),
    // StretchCenterImageConfig: STRETCHIMAGE CENTERIMAGE
    (EE_WON_CFG_KEYS_STRETCHCENTERIMAGECONFIG_VIRTUAL24_INSTRUCTION, &[&[IMG, S(0)], &[IMG, S(1)]]),
];

/// The program of `key` in the handler of `table`; None for a key the handler does not hold (or no handler).
pub fn program(table: i32, key: &[u8]) -> Option<&'static [Op]> {
    let (_, progs) = PROGRAMS.iter().find(|(t, _)| *t == table)?;
    let i = key_index(table, key);
    if i < 0 {
        return None;
    }
    progs.get(i as usize).copied()
}

/// What the interpreter asks of its host (the C++ host, or the tests').
pub trait CfgHost {
    fn read(&mut self, kind: EeWonCfgRead) -> bool;
    fn comma(&mut self) -> bool;
    fn end(&mut self) -> bool;
    /// The name index of the frame's string `slot` in the name table `t`.
    fn name(&mut self, t: i32, slot: i32) -> i32;
    fn empty(&mut self, slot: i32) -> bool;
    fn push(&mut self, v: i32);
    fn apply(&mut self, setter: i32) -> bool;
    fn fail(&mut self, msg: &[u8], slot: i32);
    fn clear(&mut self);
}

/// Runs a program: true when it ran to its end, false when a read, a comma, a setter or a name threw (the host
/// holds the exception).
pub fn run(table: i32, prog: &[Op], h: &mut impl CfgHost) -> bool {
    let mut pc = 0usize;
    // the frame's strings so far (the name reads take the last one)
    let mut strings = 0i32;
    h.clear();
    while pc < prog.len() {
        let mut next = pc + 1;
        match prog[pc] {
            R(k) => {
                if !h.read(k) {
                    return false;
                }
                if matches!(k, Rd::Str | Rd::Flag) {
                    strings += 1;
                }
            }
            C => {
                if !h.comma() {
                    return false;
                }
            }
            S(n) => {
                if !h.apply(table << 8 | i32::from(n)) {
                    return false;
                }
            }
            K(v) => h.push(v),
            N(t, values, miss) => {
                let i = h.name(t, strings - 1);
                match values.get(i as usize).filter(|_| i >= 0) {
                    Some(v) => h.push(*v),
                    None => match miss {
                        Miss::Throw(m) => {
                            h.fail(m, strings - 1);
                            return false;
                        }
                        Miss::Bare(m) => {
                            h.fail(m, -1);
                            return false;
                        }
                        Miss::Skip => return true,
                    },
                }
            }
            Je(t) => {
                if h.end() {
                    next = usize::from(t);
                }
            }
            J(t) => next = usize::from(t),
            Jz(t) => {
                if h.empty(strings - 1) {
                    next = usize::from(t);
                }
            }
            X => {
                h.clear();
                strings = 0;
            }
        }
        pc = next;
    }
    true
}

struct Ffi<'a>(&'a EeWonCfgTabHost);

impl Ffi<'_> {
    fn units(&mut self, slot: i32) -> (*const c_void, u16, u32) {
        let (mut len, mut unit) = (0u16, 0u32);
        let p = (self.0.text)(self.0.ctx, slot, &mut len, &mut unit);
        (p, len, unit)
    }
}

impl CfgHost for Ffi<'_> {
    fn read(&mut self, kind: EeWonCfgRead) -> bool {
        (self.0.read)(self.0.ctx, kind)
    }
    fn comma(&mut self) -> bool {
        (self.0.comma)(self.0.ctx)
    }
    fn end(&mut self) -> bool {
        (self.0.end)(self.0.ctx)
    }
    fn name(&mut self, t: i32, slot: i32) -> i32 {
        let (p, len, unit) = self.units(slot);
        // SAFETY: the host's units of the string (its contract)
        unsafe { ee_won_gui_cfg_name(t, p, len, unit) }
    }
    fn empty(&mut self, slot: i32) -> bool {
        self.units(slot).1 == 0
    }
    fn push(&mut self, v: i32) {
        (self.0.push)(self.0.ctx, v)
    }
    fn apply(&mut self, setter: i32) -> bool {
        (self.0.apply)(self.0.ctx, setter)
    }
    fn fail(&mut self, msg: &[u8], slot: i32) {
        (self.0.fail)(self.0.ctx, msg.as_ptr(), msg.len(), slot)
    }
    fn clear(&mut self) {
        (self.0.clear)(self.0.ctx)
    }
}

/// The handler of `table` for the instruction `key` (`len` bytes): -1 when the handler does not hold the key (the
/// caller asks its base), 0 when it was handled, 1 when it threw (the host holds the exception).
///
/// # Safety
/// `host` is valid and its callbacks do not unwind; `key` holds `len` bytes (or is NULL with 0).
#[no_mangle]
pub unsafe extern "C" fn ee_won_cfg_tab_run(
    host: *const EeWonCfgTabHost,
    table: i32,
    key: *const u8,
    len: usize,
) -> i32 {
    let k: &[u8] = if len == 0 || key.is_null() {
        &[]
    } else {
        // SAFETY: len bytes (the contract)
        unsafe { std::slice::from_raw_parts(key, len) }
    };
    let Some(prog) = program(table, k) else {
        return -1;
    };
    // SAFETY: valid (the contract)
    let mut h = Ffi(unsafe { &*host });
    if run(table, prog, &mut h) {
        0
    } else {
        1
    }
}

/// Whether `table` is one of the reader's handlers, for the drivers.
#[no_mangle]
pub extern "C" fn ee_won_cfg_tab_has(table: i32) -> bool {
    PROGRAMS.iter().any(|(t, _)| *t == table)
}

#[cfg(test)]
#[path = "cfgtab_tests.rs"]
mod tests;
