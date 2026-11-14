//! The toolkit's whole objects (R6 ui round 10): a control class whose state and every method live in a Rust object,
//! the C++ class a shim of one-line forwarders, its constructor and destructor and one host function (the op
//! switch). The pattern of ee_won's widgets (`ctobj.rs`, `guihost.rs`).
//!
//! Every call out of a Rust object is one [`EeUiOp`] on its C++ object: `UIControl`'s members (the base class stays
//! C++: the form, the trigger events, the background rhombus, the capture), the virtuals a subclass may override
//! (they go through the C++ object so the override runs), and the C++ objects the class owns (bitmaps, texts,
//! rhombi, the scroll bar's parts) named by their address. Integers travel in `a`..`d`; floats (points, rectangles,
//! colours) in a four-float buffer `f`, read by the host for inputs and written for outputs. A rectangle is
//! `GERectangle`'s member order (bottom, left, right, top), a point (x, y), a colour (red, green, blue).
//!
//! The ops marked *pure* only read; the goldens answer them from the world's state, the rest are traced call by call.

use core::ffi::c_void;

use crate::rect::{UiPoint, UiRect};

/// `kControlTriggerRangeChanged` (`UISlider::ChangeRange` fires it).
pub(crate) const TRIGGER_RANGE_CHANGED: isize = 0x3e8;

/// The calls out of a whole object (numbered in ranges per class; the numbers appear in the goldens).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeUiOp {
    // ---- UIControl (the base class, on the object itself)
    /// `UIControl::CopyData()` -> its result.
    BaseCopyData = 1,
    /// `UIControl::DrawPolygons()` -> its result.
    BaseDrawPolygons = 2,
    /// `UIControl::DrawTextA()` -> its result.
    BaseDrawText = 3,
    /// `UIControl::SetLocation(f[0], f[1])` (non-virtual).
    BaseSetLocation = 4,
    /// The virtual `SetDirty(a != 0)`.
    SetDirty = 5,
    /// *Pure*: `mRectangle` -> `f`.
    Rect = 6,
    /// *Pure*: `GetMouseLocation()` -> `f[0]`, `f[1]`.
    MouseLocation = 7,
    /// `CaptureMouse()`.
    CaptureMouse = 8,
    /// The virtual `ReleaseCapturedMouse()`.
    ReleaseMouse = 9,
    /// `FireTriggerEvent(a)`.
    FireTrigger = 10,
    /// *Pure*: `mBorderThickness`.
    BorderThickness = 11,
    /// *Pure*: `GetWidth()`.
    Width = 12,
    /// *Pure*: `mIndex`.
    Index = 13,
    /// *Pure*: `mBorderType`.
    BorderType = 14,
    /// `mBorderType = a`.
    SetBorderType = 15,
    /// *Pure*: the form's `GetTabStop()`.
    TabStop = 16,
    /// *Pure*: the form's `GetTabbingOn()`.
    TabbingOn = 17,
    /// The form's `SetTabStop(a)`.
    SetTabStop = 18,
    /// *Pure*: the virtual `GetTabable()`.
    Tabable = 19,

    // ---- UISlider
    /// The virtual `ChangeRange(a)` (`EEUIFSEESElevation` overrides it).
    SliderChangeRange = 20,
    /// The virtual `ButtonDown()` (the slider's, the button's).
    ButtonDown = 21,
    /// The virtual `ButtonUp()` (the slider's, the button's).
    ButtonUp = 22,
    /// The manager's `CreateBitmapForScaledControl(drag bar, f)` -> the thumb bitmap.
    ThumbCreate = 23,
    /// Bitmap `a`'s `Dereference()`.
    BitmapRelease = 24,
    /// Bitmap `a`'s `SetLocation(f[0], f[1])`.
    BitmapSetLocation = 25,
    /// The rasterizer's `Draw(bitmap a, GetScreenIndex())` -> its result.
    BitmapDraw = 26,
    /// *Pure*: the manager's drag bar bitmap's `GetXScale()`, `GetYScale()` -> `f[0]`, `f[1]`.
    DragBarScale = 27,
    /// Bitmap `a`'s `SetXYScale(f[0], f[1])` (the slider's thumb, the check box's and radio button's box).
    BitmapSetScale = 28,

    // ---- UIScrollBar (its parts: the slider and the two arrow buttons, by address)
    /// *Pure*: the manager's bitmap `a`: the scroll bar's (0 down normal, 1 up normal, 2 down pressed, 3 up pressed,
    /// 4 right normal, 5 left normal, 6 right pressed, 7 left pressed), the check box's (8 off, 9 on, 10 down off,
    /// 11 down on, 12 not, 13 down not) and the radio button's (14 off, 15 on, 16 down off, 17 down on).
    ManagerBitmap = 40,
    /// `new UISlider(GetId(), f, style a, form, GetIndex(), border b)` -> the slider.
    CreateSlider = 41,
    /// `new UIScrollButton(GetId(), normal a, pressed b, (f[0], f[1]), form, slider c, decrement d, GetIndex())`.
    CreateButton = 42,
    /// `delete` part `a`.
    DeletePart = 43,
    /// Part `a`'s virtual `CopyData()` -> its result.
    PartCopyData = 44,
    /// Part `a`'s virtual `DrawPolygons()` -> its result.
    PartDrawPolygons = 45,
    /// Part `a`'s virtual `DrawTextA()` -> its result.
    PartDrawText = 46,
    /// *Pure*: part `a`'s `GetRectangle()` -> `f`.
    PartRect = 47,
    /// Part `a`'s virtual `MouseLButtonDown()` -> its result.
    PartMouseDown = 48,
    /// Part `a`'s virtual `MouseLButtonUp()` -> its result.
    PartMouseUp = 49,
    /// The slider `a`'s virtual `MouseMove(f[0], f[1])` -> its result.
    PartMouseMove = 50,
    /// The arrow button `a`'s virtual `ButtonDown()`.
    PartButtonDown = 51,
    /// Part `a`'s virtual `SetLocation(f[0], f[1])`.
    PartSetLocation = 52,
    /// Part `a`'s virtual `SetRectangle(f)`.
    PartSetRectangle = 53,
    /// *Pure*: the slider `a`'s `GetSliderStyle()`.
    SliderStyle = 54,
    /// *Pure*: the slider `a`'s `GetCurrentRangeValue()`.
    SliderValue = 55,
    /// The slider `a`'s `SetCurrentRangeValue(b)`.
    SliderSetValue = 56,
    /// The slider `a`'s `DecrementByPageSize()`.
    SliderDecrementPage = 57,
    /// The slider `a`'s `IncrementByPageSize()`.
    SliderIncrementPage = 58,
    /// The slider `a`'s `SetPageSize(b)`.
    SliderSetPageSize = 59,
    /// The slider `a`'s `SetRange(b, c)`.
    SliderSetRange = 60,
    /// Part `a`'s virtual `SetBgColor(f[0..3])`.
    PartSetBgColor = 61,
    /// Part `a`'s virtual `SetBgOpacity(f[0])`.
    PartSetBgOpacity = 62,

    // ---- UIStatusBar (its two rhombi stay C++ members: 0 the bar, 1 its copy; its texts by address)
    /// *Pure*: rhombus `a`'s `GetRectangle()` -> `f`.
    RhombusRect = 70,
    /// Rhombus `a`'s `SetRectangle(f)`.
    RhombusSetRect = 71,
    /// The copy rhombus `= ` the bar's.
    RhombusCopy = 72,
    /// Rhombus `a`'s `SetOpacity(f[0])`.
    RhombusSetOpacity = 73,
    /// Rhombus `a`'s `SetColor(f[0..3])`.
    RhombusSetColor = 74,
    /// The rasterizer's `Draw(rhombus a, GetScreenIndex())` -> its result.
    RhombusDraw = 75,
    /// `new GEText(font a)` -> the text.
    TextCreate = 76,
    /// `delete` text `a`.
    TextDelete = 77,
    /// Text `a`'s `SetFont(font b)`.
    TextSetFont = 78,
    /// *Pure*: text `a`'s `GetText().GetLength()`.
    TextLength = 79,
    /// *Pure*: text `a`'s `GetTextExtent()` width and height -> `f[0]`, `f[1]`.
    TextExtent = 80,
    /// `*text a = *text b`.
    TextCopy = 81,
    /// Text `a`'s `SetText(*(const UWideString*)b)`.
    TextSetText = 82,
    /// Text `a`'s `SetLocation(f[0], f[1])`.
    TextSetLocation = 83,
    /// The form's `DrawClippedText(f, text a, this)` -> its result.
    DrawClippedText = 84,

    // ---- UIButton (its bitmaps, texts and sounds by address or id)
    /// *Pure*: `GetHasMouseCapture()`.
    HasMouseCapture = 90,
    /// *Pure*: `GetRightMouseButtonDown()`.
    RightButtonDown = 91,
    /// *Pure*: a key's state: `a` 0 Space `JustPressed()`, 1 Enter's, 2 Space `JustUnpressed()`, 3 Enter's.
    Key = 92,
    /// `CaptureKeyboard()`.
    CaptureKeyboard = 93,
    /// The virtual `ReleaseCapturedKeyboard()`.
    ReleaseKeyboard = 94,
    /// The virtual `MouseOver()`.
    ButtonMouseOver = 95,
    /// The virtual `MouseMove(f[0], f[1])`.
    ButtonMouseMove = 96,
    /// Bitmap `a`'s `Reference()`.
    BitmapReference = 97,
    /// The sound manager's `Load(*(const UString*)a, 0, 1)` -> the sample id.
    SoundLoad = 98,
    /// The sound manager's `Unload(a)`.
    SoundUnload = 99,
    /// The sound manager's `Play(a, 1, 0, 1, false, NULL)`.
    SoundPlay = 100,
    /// Text `a`'s `SetFgColor(f[0..3])`.
    TextSetFgColor = 101,

    // ---- the button subclasses (R6 ui round 12): the check box's, radio button's and tri-state box's label (a part)
    /// `new UITextLabel(*(const UWideString*)a, font b, GetId(), f, GetForm(), 0, GetIndex(), 0)` -> the label.
    LabelCreate = 110,
    /// The label `a`'s `SetText(*(const UWideString*)b)` (the public one: it dirties the label).
    LabelSetText = 111,
    /// *Pure*: `((const UWideString*)a)->GetLength()`.
    StringLength = 112,
    /// *Pure*: bitmap `a`'s `GetXScale()`, `GetYScale()` -> `f[0]`, `f[1]`.
    BitmapScale = 113,
    /// *Pure*: `GetCopiedDrawRectangle()` -> `f`.
    CopiedDrawRect = 114,
    /// `UIControl::SetRectangle(f)` (non-virtual).
    BaseSetRectangle = 115,
    /// The virtual `Check()` (the radio button's; the editor's tabs and tool buttons override it).
    RadioCheck = 116,
    /// The form's control `a`'s virtual `UnCheck()` (a radio button: `RadioOf(a)` answered it).
    RadioUnCheckAt = 117,
    /// *Pure*: the Rust button of the form's control `a`, if it exists and is a radio button (`GetType()`), else 0.
    RadioOf = 118,
    /// *Pure*: the form's `GetControlCount()`.
    ControlCount = 119,

    // ---- the polls a button holds by address (the scroll arrow's delay and repeat, the blinking button's phase)
    /// `new TMPoll(a, 0, false)` -> the poll.
    PollCreate = 130,
    /// `delete` poll `a`.
    PollDelete = 131,
    /// *Pure*: poll `a`'s `GetIsRunning()`.
    PollRunning = 132,
    /// Poll `a`'s `Poll()` -> its result (nonzero: the time ran out).
    PollPoll = 133,
    /// Poll `a`'s `Start()`.
    PollStart = 134,
    /// Poll `a`'s `Stop()`.
    PollStop = 135,
    /// Poll `a`'s `GetTimeLeft()` (the clock's: traced, not pure).
    PollTimeLeft = 136,
    /// Poll `a`'s `SetTimeDelay(b)`.
    PollSetDelay = 137,

    // ---- the blinking button's fade (UIControl's members, the bitmaps')
    /// *Pure*: `GetAlwaysDirty()`.
    AlwaysDirty = 140,
    /// `SetAlwaysDirty(a != 0)`.
    SetAlwaysDirty = 141,
    /// The virtual `SetDisplayMode(a)`.
    SetDisplayMode = 142,
    /// *Pure*: `GetBgOpacity()` -> `f[0]`.
    BgOpacity = 143,
    /// *Pure*: `GetBorderOpacity()` -> `f[0]`.
    BorderOpacity = 144,
    /// The virtual `SetBgOpacity(f[0])`.
    SetBgOpacity = 145,
    /// `SetBorderOpacity(f[0])`.
    SetBorderOpacity = 146,
    /// Bitmap `a`'s `SetOpacity(f[0])`.
    BitmapSetOpacity = 147,
    /// *Pure*: bitmap `a`'s `GetTexture(GetScreenIndex())->GetWidth()`.
    BitmapTextureWidth = 148,
    /// Bitmap `a`'s `ComputeStretchedCoordsAndScale(f, f, source, GetScreenIndex())`: `f` the destination, the source
    /// rectangle's bottom and left float bits in `c` (high, low), its right and top in `d`.
    BitmapStretch = 149,

    // ---- UITextLabel and UITLScrolling (R6 ui round 13): their strings, texts and scroll bar by address
    /// `new UWideString((const char*)a)` -> the string (the goldens trace the bytes' hash, not the address).
    StringNew = 150,
    /// `delete` string `a`.
    StringDelete = 151,
    /// `*string a = *string b`.
    StringAssign = 152,
    /// Text `a`'s `SetText(UWideString((const char*)b))` (the goldens trace the bytes' hash).
    TextSetBytes = 153,
    /// *Pure*: text `a`'s `GetFont()->GetHeight()`.
    TextFontHeight = 154,
    /// `mDrawCount--`.
    DrawCountDecrement = 155,
    /// `SetBgRectangle(f)`.
    SetBgRectangle = 156,
    /// The form's `RedrawIntersectControls(f)`.
    RedrawIntersect = 157,
    /// `new UIScrollBar(GetId(), (f[0], f[1]), f[2], 1, GetForm(), GetIndex(), GetBorderThickness())` -> the bar.
    CreateScrollBar = 158,
    /// The scroll bar `a`'s `SetRange(0, b)`.
    BarSetRange = 159,
    /// The scroll bar `a`'s `SetPageSize(b)`.
    BarSetPageSize = 160,
    /// *Pure*: the scroll bar `a`'s `GetSlider()->GetCurrentRangeValue()`.
    BarValue = 161,
    /// The scroll bar `a`'s `IncrementBy(b)`.
    BarIncrementBy = 162,
    /// The scroll bar `a`'s `SetCurrentRangeValue(b)`.
    BarSetValue = 163,
    /// *Pure*: part `a`'s `GetCopiedDrawRectangle()` -> `f`.
    PartCopiedDrawRect = 164,
    /// Poll `a`'s `Pause()`.
    PollPause = 165,
    /// Poll `a`'s `Resume()`.
    PollResume = 166,
    /// The virtual `CheckScrollBar()`.
    VirtCheckScrollBar = 167,
    /// The virtual `RecomputeDisplayRange()`.
    VirtRecompute = 168,
    /// The virtual `SetLocation(f[0], f[1])`.
    VirtSetLocation = 169,
    /// The virtual `SetRectangle(f)`.
    VirtSetRectangle = 170,

    // ---- UIEditBox (R6 ui round 14): its lines, rhombus, texts, scroll bar and polls by address
    /// *Pure*: the edit box's key question `a` (`edit_keys::EditKey`).
    EditKey = 171,
    /// *Pure*: the scroll bar `a`'s `GetSlider()->GetMaxRange()`.
    BarMax = 172,
    /// The virtual `ProcessEnterKey()` (the scenario editor's `EESETSAEditBox` overrides it).
    VirtProcessEnterKey = 173,
    /// `PlatClipboardSetText((const char*)a)` (the goldens trace the bytes' hash).
    ClipboardSet = 174,
    /// *Pure*: `PlatClipboardGetText()`: NUL-terminated bytes the host keeps until the next, or 0.
    ClipboardGet = 175,
    /// The form's manager's `UpdateTranslatedKey((const char*)a, 1)` (the goldens trace the bytes' hash).
    TranslatedKey = 176,
    /// `PlatTextInputCancel` on the control's window (the original's `ImmNotifyIME` cancel).
    TextInputCancel = 177,
    /// `UIControl::ReleaseCapturedKeyboard()` (non-virtual).
    BaseReleaseKeyboard = 178,
    /// `UIControl::MouseLButtonDoublePressed()` -> its result.
    BaseMouseDoublePressed = 179,
    /// *Pure*: the form's `GetMouseLocation()` -> `f[0]`, `f[1]`.
    FormMouseLocation = 180,
    /// `new GELine` -> the line: `a` 0 the default one, 1 the edit box's caret (zero points, black, width 1).
    LineCreate = 181,
    /// `delete` line `a`.
    LineDelete = 182,
    /// Line `a`'s `SetStartPoint(f[0], f[1])`, `SetEndPoint(f[2], f[3])`.
    LineSetPoints = 183,
    /// Line `a`'s `SetColor(f[0..3])`.
    LineSetColor = 184,
    /// The rasterizer's `Draw(line a, GetScreenIndex())` -> its result.
    LineDraw = 185,
    /// `new GERhombus` -> the rhombus (the edit box's `RhombusSetRect`, `RhombusSetColor`, `RhombusDraw` take it).
    RhombusCreate = 186,
    /// `delete` rhombus `a`.
    RhombusDelete = 187,
    /// *Pure*: text `a`'s `GetFgColor()` -> `f[0..3]`.
    TextFgColor = 188,
    /// Text `a`'s `SetBgColor(f[0..3])`.
    TextSetBgColor = 189,
    /// Text `a`'s `SetIsTransparent(b != 0)`.
    TextSetTransparent = 190,

    // ---- UIListBox<T> (R6 ui round 15): the per-`T` host's ops (`UIListBoxRust<T>::Op`, lle/UIListBox.h); the list
    // box's rhombi take their address (`RhombusRect`, `RhombusSetRect`, `RhombusDraw` too)
    /// *Pure*: the virtual `GetNumLines()`.
    ListNumLines = 191,
    /// The virtual `ChangeSelectionIndex(a, b != 0, c != 0)`.
    ListChangeSelection = 192,
    /// The virtual `ClearSelection()`.
    ListClearSelection = 193,
    /// The visible lines copied: `mCopyLineArray` grown to `a` lines if shorter, then lines `b .. c` of `mData` copied
    /// to its start.
    ListCopyLines = 194,
    /// The virtual `ProcessDown()` (`a` 0), `ProcessUp()` (1), `ProcessHome()` (2), `ProcessEnd()` (3).
    ListProcess = 195,
    /// `*rhombus a = *rhombus b`.
    RhombusAssign = 196,

    // ---- UIComboBox (R6 ui round 15): its questions and actions, numbered as the record FFI's host calls were
    /// *Pure*: the combo box's question `a >> 24` (`comboobj::EeUiComboQuery`) with its argument `a & 0xffffff`.
    ComboQuery = 197,
    /// The combo box's action `a` (`comboobj::EeUiComboAct`) with `b`, `c`, `d`.
    ComboAct = 198,

    // ---- UIForm (R6 ui round 16): its controls, groups, backgrounds, hot keys, events and the manager by address
    /// *Pure*: control `a`'s virtual `GetType()`.
    FormCtlType = 200,
    /// *Pure*: control `a`'s `UIEntity::GetType()`.
    FormCtlEntityType = 201,
    /// *Pure*: control `a`'s `GetDisplayMode()`.
    FormCtlDisplayMode = 202,
    /// *Pure*: control `a`'s `GetIsDirty()`.
    FormCtlDirty = 203,
    /// *Pure*: control `a`'s `GetDrawCount()`.
    FormCtlDrawCount = 204,
    /// *Pure*: control `a`'s `GetIndex()`.
    FormCtlIndex = 205,
    /// *Pure*: control `a`'s `GetId()`.
    FormCtlId = 206,
    /// *Pure*: control `a`'s virtual `GetTabable()`.
    FormCtlTabable = 207,
    /// *Pure*: radio button `a`'s `GetIsChecked()`.
    FormCtlChecked = 208,
    /// *Pure*: control `a`'s virtual `GetIMEEnabled()`.
    FormCtlImeEnabled = 209,
    /// *Pure*: group `a`'s `GetIsEnabled()`.
    FormGroupEnabled = 210,
    /// *Pure*: group `a`'s `GetEntityCount()`.
    FormGroupCount = 211,
    /// *Pure*: background `a`'s `GetDrawPriority()`.
    FormBgPriority = 212,
    /// *Pure*: background `a`'s `GetBackgroundID()`.
    FormBgId = 213,
    /// *Pure*: background `a`'s `GetIsVisible()`.
    FormBgVisible = 214,
    /// *Pure*: background `a`'s `GetRectangle()` -> `f`.
    FormBgRect = 215,
    /// *Pure*: hot key `a`'s `GetAlwaysFire()`.
    FormHotKeyAlways = 216,
    /// *Pure*: hot key `a`'s `GetKeyCount()`.
    FormHotKeyKeys = 217,
    /// *Pure*: the manager's `GetNumberOfSurfaces()`.
    FormSurfaces = 218,
    /// *Pure*: the manager's screen window's `GetWidth()`.
    FormWindowWidth = 219,
    /// *Pure*: the manager's screen window's `GetHeight()`.
    FormWindowHeight = 220,
    /// *Pure*: `UIControlsTable::GetUIControlsTable(a)`.
    FormTable = 221,
    /// *Pure*: table `a`'s `GetFormIndex()`.
    FormTableFormIndex = 222,
    /// *Pure*: table `a`'s `GetGroup()`.
    FormTableGroup = 223,
    /// *Pure*: table `a`'s `GetIsVisible() == true`.
    FormTableVisible = 224,
    /// *Pure*: table `a`'s `GetControlType()`.
    FormTableType = 225,
    /// *Pure*: table `a`'s `GetStringTableID()`.
    FormTableStringId = 226,
    /// *Pure*: the manager's `GetDrawCandidateWindow()`.
    FormCandidateShown = 227,
    /// *Pure*: the candidate window's `GetIMECopyCandidateRectangle()` -> `f`.
    FormCandidateRect = 228,
    /// *Pure*: label `a`'s `GetFitRectToText()`.
    FormLabelFit = 229,
    /// *Pure*: `UIManager::GetMouseWheel() != NULL`.
    FormMouseWheel = 230,
    /// *Pure*: `UIForm::sTabKey != NULL`.
    FormKeysReady = 231,
    /// *Pure*: bitmap `a`'s texture on the manager's screen: `(float)GetWidth()`, `(float)GetHeight()` -> `f[0]`, `f[1]`.
    FormBitmapTexSize = 232,
    /// *Pure*: `gISKeyboard->GetEntity(a)`.
    FormKeyEntity = 233,
    /// *Pure*: hot key `a`'s `mKey`.
    FormHotKeyKey = 234,
    /// *Pure*: hot key `a`'s `mModifiers[0]`.
    FormHotKeyMod0 = 235,
    /// *Pure*: hot key `a`'s `mModifiers[1]`.
    FormHotKeyMod1 = 236,
    /// *Pure*: hot key `a`'s `mModifiers[2]`.
    FormHotKeyMod2 = 237,
    /// Group `a`'s `GetEntity(b)` as a control.
    FormGroupEntity = 240,
    /// `new UIEntityGroup` -> the group.
    FormGroupNew = 241,
    /// `delete` group `a`.
    FormGroupDelete = 242,
    /// Group `a`'s `AddEntity(control b)`.
    FormGroupAdd = 243,
    /// Group `a`'s `RemoveEntity(control b)`.
    FormGroupRemove = 244,
    /// Control `a`'s virtual `SetDirty(b != 0)`.
    FormCtlSetDirty = 245,
    /// Control `a`'s virtual `SetDisplayMode(b)`.
    FormCtlSetDisplayMode = 246,
    /// Control `a`'s virtual `HideControl()`.
    FormCtlHide = 247,
    /// Control `a`'s `SetStringTableID(b)`.
    FormCtlSetStringId = 248,
    /// Control `a`'s virtual `TabbedFrom()`.
    FormCtlTabbedFrom = 249,
    /// Control `a`'s virtual `TabbedTo()`.
    FormCtlTabbedTo = 250,
    /// Control `a`'s virtual `UpdateKeyboard()` -> its result.
    FormCtlUpdateKeyboard = 251,
    /// Control `a`'s `UpdateMouse((f[0], f[1]))` -> its result.
    FormCtlUpdateMouse = 252,
    /// Control `a`'s virtual `MouseWheelTurned(b)` -> its result.
    FormCtlWheelTurned = 253,
    /// Control `a`'s `FireTriggerEvent(b)`.
    FormCtlFireTrigger = 254,
    /// Control `a`'s virtual `ReleaseCapturedMouse()`.
    FormCtlReleaseMouse = 255,
    /// Control `a`'s virtual `ReleaseCapturedKeyboard()`.
    FormCtlReleaseKeyboard = 256,
    /// Control `a`'s virtual `GetHelpString` into the host's help string (emptied first) -> its result.
    FormHelpGet = 257,
    /// Label `a`'s `SetText(the host's help string, true)`.
    FormHelpSetText = 258,
    /// Background `a`'s virtual `Open()`.
    FormBgOpen = 259,
    /// Background `a`'s virtual `Close()`.
    FormBgClose = 260,
    /// `delete` background `a`.
    FormBgDelete = 261,
    /// Background `a`'s virtual `DrawBackground(f)` -> its result.
    FormBgDraw = 262,
    /// Background `a`'s virtual `LoadBackground()`.
    FormBgLoad = 263,
    /// Background `a`'s virtual `UnloadBackground()`.
    FormBgUnload = 264,
    /// Background `a`'s virtual `RelayoutBackground()`.
    FormBgRelayout = 265,
    /// `new UIHotKey(this, *hot key a)` -> the copy.
    FormHotKeyCopy = 266,
    /// `delete` hot key `a`.
    FormHotKeyDelete = 267,
    /// Hot key `a`'s `mEnabled = b != 0`.
    FormHotKeySetEnabled = 268,
    /// Hot key `a`'s virtual `FireEvents()` -> whether one fired.
    FormHotKeyFire = 269,
    /// `new UIHotKey` for `AddHotKey`: `mForm = this`, `GetEntity(a, mKey)`, `CalculateKeyCount()` -> it.
    FormHotKeyTemp = 270,
    /// Hot key `a`'s `SetModifier(b, slot c)`.
    FormHotKeySetModifier = 271,
    /// Hot key `a`'s event `b` (0 just pressed, 1 pressed, 2 just unpressed, 3 double pressed) set to `c`.
    FormHotKeySetEvent = 272,
    /// Hot key `a`'s `mAlwaysFire = b != 0`.
    FormHotKeySetAlways = 273,
    /// Event `a`'s `b`: 0 `Uninitialize()`, 1 `Dereference()`, 2 `Reference()`, 3 `Initialize()`.
    FormEventCall = 274,
    /// Event `a`'s `ProcessEvent(this, NULL)` -> its result.
    FormEventProcess = 275,
    /// `new UIEMouseWheelScroll` -> the event.
    FormWheelEventNew = 276,
    /// The manager's `ProcessAllEventsForForm(this)`.
    FormManagerProcessEvents = 277,
    /// The manager's `AddDirtyRectangle(f, this)`.
    FormManagerAddDirty = 278,
    /// The manager's `StopMusic()`.
    FormMusicStop = 279,
    /// The manager's `StartMusic(UWideString((const char*)a))` (the goldens trace the bytes' hash).
    FormMusicStart = 280,
    /// The manager's `CreateFormEvent(a)` -> the event.
    FormCreateFormEvent = 281,
    /// Table `a`'s `CreateControl(the manager)` -> the control.
    FormTableCreateControl = 282,
    /// Table `a`'s `CreateControlEvent(this)`.
    FormTableCreateEvent = 283,
    /// Table `a`'s `RelayoutControl(control b, the manager)`.
    FormTableRelayout = 284,
    /// `delete` form manager `a`.
    FormManagerDelete = 285,
    /// `TSScheduler::CreateCriticalSection` -> the lock.
    FormLockNew = 286,
    /// `TSScheduler::DestroyCriticalSection(a)`.
    FormLockDelete = 287,
    /// Lock `a`'s `Enter()`.
    FormLockEnter = 288,
    /// Lock `a`'s `Leave()`.
    FormLockLeave = 289,
    /// `gISKeyboard->GetEntity(kKeyTab, sTabKey)`, then `kKeyShift`'s into `sShiftKey`.
    FormKeysFetch = 290,
    /// `sTabKey->JustPressed()`.
    FormTabJustPressed = 291,
    /// `sShiftKey->GetValue()`.
    FormShiftValue = 292,
    /// `UIManager::GetMouseWheelScrollAmount()`.
    FormWheelAmount = 293,
    /// `gISMouse->GetEntity(a)` -> the entity.
    FormMouseEntity = 294,
    /// Mouse entity `a`'s query `b` (`mouse::EntityQuery`).
    FormMouseAsk = 295,
    /// The virtual `SetHelpTextPopupLocation()`.
    FormVirtHelpLocation = 296,
    /// The virtual `DrawBackground(f)` -> its result.
    FormVirtDrawBackground = 297,
    /// Bitmap `a`'s `SetTextureCoordinates`: top-left `(f[0], f[1])`, top-right `(f[2], f[1])`, bottom-left `(f[0], f[3])`, bottom-right `(f[2], f[3])`.
    FormBitmapSetTexCoords = 298,
    /// The rasterizer's `Draw(bitmap a, the manager's screen)` -> its result.
    FormBitmapDraw = 299,
    /// The rasterizer's `Draw(text a, f, the manager's drawing surface)` -> its result.
    FormTextDraw = 300,
    /// The rasterizer's `Draw(rhombus a, the manager's screen)` -> its result.
    FormRhombusDraw = 301,
    /// Rhombus `a`'s `SetColor(label b's GetGEText()->GetFgColor())`.
    FormRhombusLabelColor = 302,
    /// `TMGetTime()`.
    FormNow = 303,

    // ---- UIManager (R6 ui round 17, managerobj.rs); on the manager's C++ object
    /// *Pure*: form `a`'s `GetRectangle()` -> `f`.
    MgrFormRect = 400,
    /// *Pure*: form `a`'s `GetFormID()`.
    MgrFormId = 401,
    /// *Pure*: form `a`'s `GetUseGDIForText()`.
    MgrFormUseGdi = 402,
    /// *Pure*: form `a`'s `GetKeyboardCaptured()` -> the control.
    MgrFormKeyboard = 403,
    /// *Pure*: form `a`'s `GetMouseCaptured()` -> the control.
    MgrFormMouse = 404,
    /// *Pure*: control `a`'s virtual `GetIMEEnabled()`.
    MgrCtlIme = 405,
    /// *Pure*: `gISKeyboard->GetShouldUpdate()`.
    MgrKeyboardShouldUpdate = 406,
    /// *Pure*: `gISMouse->GetShouldUpdate()`.
    MgrMouseShouldUpdate = 407,
    /// *Pure*: the window of the manager's screen: 0 none, else 1 and its client origin in `f[0]`, `f[1]`.
    MgrWindow = 408,
    /// *Pure*: that window's width | height << 32.
    MgrWindowSize = 409,
    /// *Pure*: texture `a`'s `(float)GetWidth()`, `(float)GetHeight()` -> `f[0]`, `f[1]`.
    MgrTextureSize = 410,
    /// *Pure*: the rasterizer's `GetBlending(source, destination, screen)` -> source | destination << 32.
    MgrBlending = 411,
    /// `TSScheduler::CreateCriticalSection` (assert line `a`) -> the lock.
    MgrLockNew = 420,
    /// `TSScheduler::DestroyCriticalSection(a)`.
    MgrLockDelete = 421,
    /// `LoadTexture("textures\UIControls")` (after the rasterizer's `GetScreen(screen)`) -> the referenced texture.
    MgrTextureLoad = 422,
    /// Texture `a`'s `Dereference()`.
    MgrTextureRelease = 423,
    /// `new GEBitmap` with `SetTexture(texture a, screen)` -> the bitmap.
    MgrBitmapNew = 424,
    /// `CreateBitmap`'s first steps on bitmap `a`: `SetOpacity(1)`, `SetTextureCoordinates` from `(f[0], f[1])` to
    /// `(f[2], f[3])` (top-left, top-right, bottom-left, bottom-right).
    MgrBitmapCoords = 425,
    /// `CreateBitmapForScaledControl`'s copy of bitmap `a`: a new bitmap on its texture with its four coordinates.
    MgrScaledBitmapNew = 426,
    /// Form `a`'s call `b`: 0 `Open`, 1 `Close`, 2 `Activate`, 3 `Deactivate`, 4 `ClearAllClipRectangles`,
    /// 5 `SetAllControlsDirty`, 6 `CopyData`, 7 `DrawPolygons`, 8 `DrawTextA`, 9 `UpdateKeyboard`, 10 `UpdateMouse`,
    /// 11 `delete` -> its result.
    MgrFormCall = 427,
    /// Form `a`'s call `b` with `f`: 0 `AddDirtyRectangle`, 1 `RedrawIntersectControls`, 2 `AddClipRectangle`,
    /// 3 `RelayoutForm`, 4 `UpdateMouseLocation(f[0], f[1])`.
    MgrFormRectCall = 428,
    /// Form `a`'s IME call `b` (`manager_media::ImeCall`) with the C++ call's arguments `c` -> its result.
    MgrFormIme = 429,
    /// Form `a`'s `UpdateTranslatedKey(the one character at b, repeat c)` -> its result.
    MgrTranslatedKey = 430,
    /// Event `a`'s `ProcessEvent(form b, entity c)`.
    MgrEventProcess = 431,
    /// Event factory `a`: `b` 0 a form event factory called `(manager, c)`, 1 a control one `(form d, c)` -> the event.
    MgrCallback = 432,
    /// `new UIIMECandidate(manager, 0.2, 0.35, font a)` -> the candidate window.
    MgrCandidateNew = 433,
    /// Candidate window `a`'s call `b`: 0 `DrawPolygons`, 1 `DrawTextA`, 2 `CloseCandidateWindow`, 3 `CopyData`,
    /// 4 `delete` -> its result.
    MgrCandidateCall = 434,
    /// Candidate window `a`'s `SetCandidateList(*(UIIMECandidateList*)b, f[0])`.
    MgrCandidateSetList = 435,
    /// Candidate window `a`'s `OpenCandidateWindow(f, control)`: the control rectangle's bottom and left as float bits
    /// in `b` (low, high), its right and top in `c`.
    MgrCandidateOpen = 436,
    /// The rasterizer's `SetBlending(a, b, screen)`.
    MgrSetBlending = 437,
    /// `UIHotKey::UpdatePressedPollAmount()`.
    MgrHotKeyPoll = 438,
    /// `UIHotKey::DeleteHotKeys()`.
    MgrHotKeysDelete = 439,
    /// The manager's virtual `UpdateKeyboard()` -> its result.
    MgrVirtUpdateKeyboard = 440,
    /// The manager's virtual `UpdateMouse(f[0], f[1])` -> its result.
    MgrVirtUpdateMouse = 441,
    /// The sound manager's `Stop(instance a, false)`.
    MgrSoundStop = 442,
    /// The sound manager's `Unload(a)`.
    MgrSoundUnload = 443,
    /// The sound manager's `Load(name a, 0, 1)` -> the id (-1: failed).
    MgrSoundLoad = 444,
    /// The sound manager's `PlayAndReturnReferencedNode(a, f[0], 0, 1, true, NULL)` -> the instance.
    MgrSoundPlay = 445,
    /// The sound manager's `SetSoundParameters(instance a, f[0], 0, 1)`.
    MgrSoundParameters = 446,
    /// Sound instance `a`'s `Reference()` (`b` 0) or `Dereference()` (1).
    MgrInstanceRef = 447,
}

impl EeUiOp {
    /// Whether the op only reads (the goldens answer those from the world's state).
    pub fn is_pure(self) -> bool {
        use EeUiOp::*;
        matches!(
            self,
            Rect | MouseLocation
                | BorderThickness
                | Width
                | Index
                | BorderType
                | TabStop
                | TabbingOn
                | Tabable
                | HasMouseCapture
                | RightButtonDown
                | Key
                | DragBarScale
                | ManagerBitmap
                | StringLength
                | BitmapScale
                | CopiedDrawRect
                | RadioOf
                | ControlCount
                | PollRunning
                | AlwaysDirty
                | BgOpacity
                | BorderOpacity
                | BitmapTextureWidth
                | PartRect
                | SliderStyle
                | SliderValue
                | RhombusRect
                | TextLength
                | TextExtent
                | TextFontHeight
                | BarValue
                | PartCopiedDrawRect
                | EditKey
                | BarMax
                | ClipboardGet
                | FormMouseLocation
                | TextFgColor
                | ListNumLines
                | ComboQuery
                | FormCtlType
                | FormCtlEntityType
                | FormCtlDisplayMode
                | FormCtlDirty
                | FormCtlDrawCount
                | FormCtlIndex
                | FormCtlId
                | FormCtlTabable
                | FormCtlChecked
                | FormCtlImeEnabled
                | FormGroupEnabled
                | FormGroupCount
                | FormBgPriority
                | FormBgId
                | FormBgVisible
                | FormBgRect
                | FormHotKeyAlways
                | FormHotKeyKeys
                | FormSurfaces
                | FormWindowWidth
                | FormWindowHeight
                | FormTable
                | FormTableFormIndex
                | FormTableGroup
                | FormTableVisible
                | FormTableType
                | FormTableStringId
                | FormCandidateShown
                | FormCandidateRect
                | FormLabelFit
                | FormMouseWheel
                | FormKeysReady
                | FormBitmapTexSize
                | FormKeyEntity
                | FormHotKeyKey
                | FormHotKeyMod0
                | FormHotKeyMod1
                | FormHotKeyMod2
                | MgrFormRect
                | MgrFormId
                | MgrFormUseGdi
                | MgrFormKeyboard
                | MgrFormMouse
                | MgrCtlIme
                | MgrKeyboardShouldUpdate
                | MgrMouseShouldUpdate
                | MgrWindow
                | MgrWindowSize
                | MgrTextureSize
                | MgrBlending
        )
    }
}

/// A whole object's host: `(the C++ object, op, a, b, c, d, f)` -> the op's result.
pub type EeUiHostFn = Option<
    unsafe extern "C" fn(obj: *mut c_void, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: *mut f32) -> isize,
>;

/// A Rust object's way back into its C++ object.
#[derive(Clone, Copy, Debug)]
pub struct Host {
    pub obj: *mut c_void,
    pub f: EeUiHostFn,
}

impl Host {
    /// An op with integers only.
    pub fn op(&self, op: EeUiOp, a: isize, b: isize, c: isize, d: isize) -> isize {
        let mut f = [0.0f32; 4];
        self.op_f(op, a, b, c, d, &mut f)
    }

    /// An op with the float buffer.
    pub fn op_f(&self, op: EeUiOp, a: isize, b: isize, c: isize, d: isize, f: &mut [f32; 4]) -> isize {
        match self.f {
            // SAFETY: the host was given with its object; the buffer is four live floats; no borrow of a Rust
            // object's state is live across the call (callers copy their state out first)
            Some(h) => unsafe { h(self.obj, op, a, b, c, d, f.as_mut_ptr()) },
            None => 0,
        }
    }

    /// An op taking a point.
    pub fn op_point(&self, op: EeUiOp, a: isize, p: UiPoint) -> isize {
        let mut f = [p.x, p.y, 0.0, 0.0];
        self.op_f(op, a, 0, 0, 0, &mut f)
    }

    /// An op taking a rectangle.
    pub fn op_rect(&self, op: EeUiOp, a: isize, b: isize, r: UiRect) -> isize {
        let mut f = rect_buf(r);
        self.op_f(op, a, b, 0, 0, &mut f)
    }

    /// An op answering a rectangle.
    pub fn get_rect(&self, op: EeUiOp, a: isize) -> UiRect {
        let mut f = [0.0f32; 4];
        self.op_f(op, a, 0, 0, 0, &mut f);
        buf_rect(&f)
    }

    /// An op answering a point.
    pub fn get_point(&self, op: EeUiOp, a: isize) -> UiPoint {
        let mut f = [0.0f32; 4];
        self.op_f(op, a, 0, 0, 0, &mut f);
        UiPoint { x: f[0], y: f[1] }
    }

    /// `mRectangle`.
    pub fn rect(&self) -> UiRect {
        self.get_rect(EeUiOp::Rect, 0)
    }

    /// `GetMouseLocation()`.
    pub fn mouse(&self) -> UiPoint {
        self.get_point(EeUiOp::MouseLocation, 0)
    }
}

/// A rectangle in the float buffer.
pub fn rect_buf(r: UiRect) -> [f32; 4] {
    [r.bottom, r.left, r.right, r.top]
}

/// The float buffer as a rectangle.
pub fn buf_rect(f: &[f32; 4]) -> UiRect {
    UiRect { bottom: f[0], left: f[1], right: f[2], top: f[3] }
}

/// Reads the four floats a call's `f` points at (zeros for NULL).
///
/// # Safety
/// `f` is NULL or points at four live floats.
pub unsafe fn read_buf(f: *const f32) -> [f32; 4] {
    if f.is_null() {
        return [0.0; 4];
    }
    // SAFETY: the contract
    unsafe { [*f, *f.add(1), *f.add(2), *f.add(3)] }
}

/// Writes the four floats back to a call's `f` (nothing for NULL).
///
/// # Safety
/// `f` is NULL or points at four live floats.
pub unsafe fn write_buf(f: *mut f32, v: [f32; 4]) {
    if f.is_null() {
        return;
    }
    for (i, x) in v.iter().enumerate() {
        // SAFETY: the contract
        unsafe { *f.add(i) = *x };
    }
}

#[cfg(test)]
pub mod replay;
