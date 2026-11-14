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
