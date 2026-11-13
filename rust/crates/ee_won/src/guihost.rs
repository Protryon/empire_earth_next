//! WONGUI's component objects' one outward call table (R6 WON round 21): every C++ call a Rust component object
//! makes (its base class's virtuals and fields, the component's services, the fonts and segments it holds, the
//! Graphics) is one op code of a single host function, `ee_won_guicpp_call` in `WONGUI/GuiHost.cpp`. As the lobby
//! logic's table (`lobbyhost`), an op is a run of C++ statements with no decision between them, so the host stays a
//! switch of short cases and the Rust keeps every branch and the call order.
//!
//! The pointers the ops take and return (segments, the Graphics, a draw operation) are opaque here: Rust only
//! passes them back. A segment Rust holds carries a reference (`SegHold`), given back with `SegRelease`.
//!
//! The base class's fields are read and written through `Get` / `Set` (`EeWonGuiField`): the base class's own code
//! (ScrollArea's clamp, the scroller) reads and changes them between the Rust object's statements, so the Rust
//! keeps no copy.

use std::ffi::c_void;

/// The host: `(component, op, a, b, c, d) -> result`. `component` is the C++ object the Rust one belongs to.
pub type EeWonGuiHostFn = Option<
    unsafe extern "C" fn(component: *mut c_void, op: EeWonGuiOp, a: isize, b: isize, c: isize, d: isize) -> isize,
>;

/// A base class field the Rust object reads or writes (`Get` / `Set`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq, PartialOrd, Ord)]
pub enum EeWonGuiField {
    /// `mScrollArea.width`.
    ScrollWidth = 0,
    /// `mScrollArea.height`.
    ScrollHeight,
    /// `mVertOffset`.
    VertOffset,
    /// `mHorzOffset`.
    HorzOffset,
    /// `mTotalWidth`.
    TotalWidth,
    /// `mTotalHeight`.
    TotalHeight,
    /// `mExtraHeight`.
    ExtraHeight,
    /// `mUserHasScrolled` (0 or 1).
    UserHasScrolled,
    /// `mUpScrollAmount`.
    UpScrollAmount,
    /// `mDownScrollAmount`.
    DownScrollAmount,
    /// `Width()` (read only).
    Width,
    /// `Height()` (read only).
    Height,
    /// `mScrollArea.x`.
    ScrollX,
    /// `mScrollArea.y`.
    ScrollY,
    /// `mLeftScrollAmount`.
    LeftScrollAmount,
    /// `mRightScrollAmount`.
    RightScrollAmount,
    /// `mLeftPad`.
    LeftPad,
    /// `mTopPad`.
    TopPad,
    /// `mRightPad`.
    RightPad,
    /// `mBottomPad`.
    BottomPad,
}

/// The cursor `SetParentCursor` sets on the parent.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiCursor {
    /// NULL (the parent's own).
    None = 0,
    /// `Cursor::GetStandardCursor(StandardCursor_Hand)`.
    Hand = 1,
    /// `Cursor::GetStandardCursor(StandardCursor_IBeam)`.
    IBeam = 2,
    /// `Cursor::GetStandardCursor(StandardCursor_NorthSouthEastWest)`.
    NorthSouthEastWest = 3,
    /// `Cursor::GetStandardCursor(StandardCursor_EastWest)`.
    EastWest = 4,
    /// `Cursor::GetStandardCursor(StandardCursor_NorthSouth)`.
    NorthSouth = 5,
}

/// The op codes. `a`..`d` are integers or pointers as each says.
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiOp {
    // ---- the base class (a ScrollArea for the text area)
    /// the field `a` (`EeWonGuiField`).
    Get = 1,
    /// the field `a` = `b`.
    Set,
    /// `ScrollArea::Virtual08()` (it calls the virtual AdjustPosition back).
    BaseSizeChanged,
    /// `ScrollArea::AdjustPosition(a != 0)`.
    BaseAdjustPosition,
    /// `ScrollArea::NotifyRootVisibility(a != 0)`.
    BaseNotifyRootVisibility,
    /// `ScrollArea::MouseDown(a, b, c)`.
    BaseMouseDown,
    /// `ScrollArea::MouseUp(a, b, c)`.
    BaseMouseUp,
    /// `ScrollArea::MouseExit()`.
    BaseMouseExit,
    /// `ScrollArea::MouseMove(a, b)`.
    BaseMouseMove,
    /// `ScrollArea::MouseEnter(a, b)`.
    BaseMouseEnter,
    /// `ScrollArea::MouseDrag(a, b)`.
    BaseMouseDrag,
    /// `ScrollArea::KeyDown(a)` (0 or 1).
    BaseKeyDown,
    /// `ScrollArea::KeyChar(a)` (0 or 1).
    BaseKeyChar,
    /// `ScrollTranslate(*a, *b)` (`a`, `b` point at i32s).
    ScrollTranslate,
    /// `Component::GotFocus()` (the input box's base).
    BaseGotFocus = 20,
    /// `Component::LostFocus()`.
    BaseLostFocus,
    /// `Component::Paint(*a)` (`a` the Graphics).
    BasePaint,
    // ---- the component
    /// `Invalidate()`.
    Invalidate = 30,
    /// `RequestTimer(a != 0)`.
    RequestTimer,
    /// `RequestFocus()`.
    RequestFocus,
    /// `GetMousePos(*a, *b)` (`a`, `b` point at i32s).
    GetMousePos,
    /// `GetKeyMod()`.
    KeyMod,
    /// `mParent->SetCursor(a)` (`EeWonGuiCursor`).
    SetParentCursor,
    /// the selection colours into `*a` (an `EeWonSelColor`).
    SelColor,
    /// `mLinkParam = a->GetLinkParam(); DeliverEvent(new TextLinkActivatedEvent(this, a), false)`.
    LinkActivated,
    /// the copied text (a GUIString the C++ object keeps) `.append(a->mText, b, c)`.
    SelTextAppend,
    /// the copied text `.append("\r\n")`.
    SelTextBreak,
    /// `Clipboard::Set(copied text)`, then the copied text emptied.
    SelTextCopy,
    /// `SetSize(a, b)`.
    SetSize = 45,
    /// `SetDefaultCursor(a)` (`EeWonGuiCursor`).
    SetDefaultCursor,
    /// `FireEvent(a, false)` (`EeWonGuiEvent`).
    FireEvent,
    /// `Clipboard::Set(text)`, the text `b` units (u32) at `a`.
    ClipboardSet,
    /// `Clipboard::Get()`'s units appended to the Rust buffer `a` with `ee_won_gui_sink_units`.
    ClipboardTake,
    // ---- a segment (`a` = its C++ object)
    /// `a->AddRef()`.
    SegHold = 60,
    /// `a->Release()`.
    SegRelease,
    /// `a->mLineBreak` (0 or 1).
    SegLineBreak,
    /// `a->GetLength()`.
    SegLength,
    /// `a->CalcDimensions()`'s measures into `*b` (three i32s: the text's width, the font's height and ascent).
    SegMeasure,
    /// `a->mFont->GetStringWidth(a->mText, b, c)`.
    SegStringWidth,
    /// `a->mFont->GetCharacterWidth(a->mText[b])`.
    SegCharWidth,
    /// `a`'s text units appended to the Rust buffer `b` with `ee_won_gui_sink_units`.
    SegTextTake,
    /// `a->IsLink()` (0 or 1).
    SegIsLink,
    /// `a->Pressed()`.
    SegPressed,
    /// `a->Released()` (0 or 1).
    SegReleased,
    /// `a->mColor`.
    SegColor,
    // ---- the Graphics
    /// executes the draw operation `*b` (an `EeWonDrawOp`) on the Graphics `a`, its font and string slots the
    /// segment `c`'s font and text (none when `c` is 0).
    Draw = 90,
    // ---- the input box (its font, icon, filter and the texts the C++ keeps for its readers)
    /// `mFont->GetHeight()`.
    FontHeight = 100,
    /// `mFont->GetCharacterWidth(a)`.
    FontCharWidth,
    /// `mFont->GetStringWidth(mText, a, b)` (both cast to unsigned short).
    TextWidth,
    /// `mFont->GetStringWidth(mText)`.
    TextWidthAll,
    /// `mFont->GetCharacterWidth(mText[a])`.
    TextCharWidth,
    /// `mFont->GetStringWidth(GUIString("M"))`.
    FontStringM,
    /// the C++ copy of a text (`a`: 0 the shown text `mText`, 1 the real one `GetText()`) = the `c` units (u32)
    /// at `b`.
    TextStore,
    /// `mInputFilter(this, s)` over the string `s` of the `b` units (u32) at `a`: 2 without a filter, else the
    /// filter's answer (0 or 1) with `s` after it appended to the Rust buffer `c`.
    Filter,
    /// `mIcon->mWidth`, or -1 without an icon.
    IconWidth,
    /// the C++ members of the paint into `*a` (an `EeWonInputPaint`: the disabled look, the icon and its height,
    /// the focus, the text colours, the font's height, the selection colours).
    PaintInputs,
    // ---- the label (its wrapped text and background)
    /// `mWrappedText->SetMaxWidth(a)`.
    WrapSetMaxWidth = 130,
    /// `mWrappedText->SetCentered(a != 0)`.
    WrapSetCentered,
    /// `mWrappedText->SetWrap(a != 0)`.
    WrapSetWrap,
    /// `mWrappedText->GetWidth()`.
    WrapWidth,
    /// `mWrappedText->GetHeight()`.
    WrapHeight,
    /// `mBackground.Paint(*a, 0, 0, b, c)`.
    PaintBackground,
    // ---- a container's children and events (`a` a child `Component*`, a `Button*` or a `ComponentEvent*`)
    /// `a->SetEnabled(b != 0)`.
    ChildSetEnabled = 150,
    /// `a->SetVisible(b != 0)`.
    ChildSetVisible,
    /// `a->SetPosSize(r.x, r.y, r.w, r.h)` with `r` the `EeWonRect4` at `b`.
    ChildSetPosSize,
    /// `a->IsVisible()` (0 or 1).
    ChildIsVisible,
    /// `a->Width()`.
    ChildWidth,
    /// `a->Height()`.
    ChildHeight,
    /// `a->Left()`.
    ChildLeft,
    /// `a->Top()`.
    ChildTop,
    /// `a->AddRef()` (a `SmartPtr` the Rust object keeps).
    ChildHold,
    /// `a->Release()`.
    ChildRelease,
    /// `a->ComponentFlagSet(b)` (0 or 1).
    ChildFlagSet,
    /// `a->SetComponentFlags(b, c != 0)`.
    ChildSetFlags,
    /// `a->GetDefaultCursor() != NULL` (0 or 1).
    ChildHasDefaultCursor,
    /// `a->SetDefaultCursor(b)` (`EeWonGuiCursor`).
    ChildSetDefaultCursor,
    /// `a->GetMouseChild()`.
    ChildMouseChild,
    /// `AddChild(a)`.
    AddChild,
    /// `mMouseChild`.
    MouseChild,
    /// `a->mControlId` (a button).
    ButtonControlId,
    /// `a->mControlId = b`.
    ButtonSetControlId,
    /// `a->SetCheck(b != 0, false)`.
    ButtonSetCheck,
    /// `a->IsChecked()` (0 or 1).
    ButtonIsChecked,
    /// a new `Component` laid over the container `a` (`Fn2F6140(c, 3, a, 0, 0, 0, 0, 0)`) and added to it
    /// (`AddChildEx(c, 0)`), the creator's reference gone (the container holds it).
    NewDragComponent,
    /// a new `Component` sized `a` x `a`, with the creator's reference (given back with `ChildRelease`).
    NewSizedComponent,
    /// `a->mType == ComponentEvent_ButtonPressed` (0 or 1).
    EventIsButtonPress,
    /// `a->GetControlId()`.
    EventControlId,
    /// `dynamic_cast<Button*>(a->mComponent)`.
    EventButton,
    /// `Container::HandleComponentEvent(a)`.
    BaseHandleEvent,
    // ---- a list (the C++ object's own virtuals, its scroll area and the item tool tip)
    /// `AdjustPosition(a != 0)` (virtual: a subclass's override).
    VirtAdjustPosition = 190,
    /// `Sort()` (virtual; 0 or 1).
    VirtSort,
    /// `NotifyMouseItemGone()` (virtual).
    VirtNotifyMouseItemGone,
    /// `ListSelChangedHook()` (virtual).
    VirtListSelChangedHook,
    /// `SetVertOffset(a)`.
    SetVertOffset,
    /// `OnBottom()` (0 or 1).
    OnBottom,
    /// `if (mFocusChild != NULL) { mFocusChild->LostFocus(); mFocusChild = NULL; }`.
    FocusChildLost,
    /// `GetRoot()->GetKeyMod()`.
    RootKeyMod,
    /// `SetCursor(GetDefaultCursor())`.
    SetCursorDefault,
    /// `GetViewport()`.
    GetViewport,
    /// the root's mouse point outside the list (`SetListFlags`: a window, a parent, and not `Contains`; 0 or 1).
    PointerOutside,
    /// `DeliverEvent(new ListItemClickedEvent(this, a, button, CheckDoubleClick(button), up, x, y), false)` with
    /// the `EeWonItemClick` at `b`.
    DeliverItemClicked,
    /// the item tool tip made when there is none, then `DoTip(a)`.
    TipShow,
    /// `if (mItemToolTip != NULL) mItemToolTip->DoTip(NULL)`.
    TipHide,
    /// `if (mItemToolTip != NULL) mItemToolTip->EndTip()`.
    TipEnd,
    /// `std::sort` of the `b` items at `a` with `LineSortPred(this, GetSortFunc(), c)`.
    SortItems,
    /// `std::lower_bound` of the item `c` in the `b` sorted items at `a` with `LineSortPred(this, GetSortFunc(), d)`.
    LowerBound,
    /// `LineSortPred(this, GetSortFunc(), c)(a, b)` (0 or 1).
    Less,
    // ---- a list item (`a` a `ListItem*`)
    /// `a->mTop`.
    ItemTop = 220,
    /// `a->mTop = b`.
    ItemSetTop,
    /// `a->mHeight`.
    ItemHeight,
    /// `a->mHeight = b`.
    ItemSetHeight,
    /// `a->mWidth`.
    ItemWidth,
    /// `a->mSelected` (0 or 1).
    ItemSelected,
    /// `a->mSelected = b != 0`.
    ItemSetSelected,
    /// `a->mSelectable` (0 or 1).
    ItemSelectable,
    /// `a->SetPos(0, b)`.
    ItemSetPos,
    /// `a->Contains(b, c)` (0 or 1).
    ItemContains,
    /// `a->CanSelect(this, b, c)` (0 or 1).
    ItemCanSelect,
    /// `a->CalcDimensions(this)`.
    ItemCalcDimensions,
    /// `a->SetViewport(this, b)` (`b` a `Container*` or NULL).
    ItemSetViewport,
    /// `a->Paint(*b, this)`.
    ItemPaint,
    /// `a->MouseEnter(this, b, c)`.
    ItemMouseEnter,
    /// `a->MouseExit(this)`.
    ItemMouseExit,
    /// `a->MouseMove(this, b, c)`.
    ItemMouseMove,
    /// `a->MouseDrag(this, b, c)`.
    ItemMouseDrag,
    /// `a->MouseDown(this, b, c, d)`.
    ItemMouseDown,
    /// `a->MouseUp(this, b, c, d)`.
    ItemMouseUp,
    /// `a->AddRef()` (the list's `SmartPtr`).
    ItemHold,
    /// `a->Release()`.
    ItemRelease,
    // ---- the multi-column list, its rows and header (`multiobj`)
    /// `a->Compare(b, this)` (a cell's own order).
    ItemCompare = 250,
    /// `((ListArea::SortFunc)a)(this, b, c)` (a column's sort function).
    CallSortFunc,
    /// `a->mWidth = b`.
    ItemSetWidth,
    /// `SetCursor(a)` (`EeWonGuiCursor`).
    SetCursor,
    /// `mCurrentCursor.get() == a` (`EeWonGuiCursor`; 0 or 1).
    CursorIs,
    /// `GetSelColor().GetBackgroundColor(*a)` (`a` the Graphics).
    SelBackground,
    /// a column's viewport: `v = new ScrollAreaViewport(this)` held by the column (`AddRef`), then
    /// `GetViewport()->AddChild(v)`; returns `v` (given back with `ChildRelease`).
    NewColumnViewport,
    /// `a->SetPos(b, c)`.
    ChildSetPos,
    /// `((MultiListHeader*)a)->NotifyColumnChange()`.
    HeaderNotify,
    /// `((MultiListHeader*)a)->SetSortColumn(b, c)`.
    HeaderSetSortColumn,
    /// `((MultiListItem*)a)->GetItem(b)`.
    RowCell,
    /// `((MultiListItem*)a)->SetItem(b, c)`.
    RowSetCell,
    /// `((MultiListArea*)a)->mMulti` (the Rust multi-column list of a header's list).
    MultiOf,
    /// `RemoveChild(a)`.
    RemoveChild,
    /// `a->mControlId`.
    ChildControlId,
    /// `a->mControlId = b`.
    ChildSetControlId,
    /// `a->Right()`.
    ChildRight,
    /// `a->GetDesiredSize(b[0], b[1])` (`b` points at two i32s).
    ChildDesiredSize,
    /// the event `a`'s component's ancestor whose parent is this container (`child = a->mComponent`, climbed while
    /// its parent is neither this nor NULL); 0 when the climb ends at NULL.
    EventChildUnder,
    // ---- the tree list and its rows (`treeobj`)
    /// `((TreeItem*)a)->mNode` (a row's Rust object).
    NodeOf = 280,
    /// a new `TreeItem` with the creator's reference (`AddRef`; given back with `ItemRelease`).
    NewTreeItem,
    /// `a->mSelectable = b != 0`.
    ItemSetSelectable,
    /// `a->SetPos(b, c)`.
    ItemSetPosXY,
    /// `a->mType` (a `ComponentEvent*`).
    EventType,
    /// `a->mComponent->mControlId`.
    EventCompControlId,
    /// `a->mComponent->Top()`.
    EventComponentTop,
    /// `CheckDoubleClick(a)` (0 or 1).
    CheckDoubleClick,
    /// `DeliverEvent(new TreeItemOpenedEvent(this, a), false)`.
    DeliverTreeOpened,
    // ---- the popup menu (`menuobj`; a submenu is a `PopupMenu*`)
    /// `GetRoot()->ClosePopup(a)`.
    ClosePopup = 300,
    /// `a->mComponent` (a `ComponentEvent*`).
    EventComponent,
    /// `a->GetSelItem()` (a submenu).
    MenuSelItem,
    /// `a->SetSelItem(b)` (a submenu; the event fired).
    MenuSetSelAt,
    /// `dynamic_cast<MenuItem*>(a)` (`a` a `ListItem*` or NULL).
    MenuItemOf,
    /// `a->mSubMenu.get()` (a `MenuItem*`).
    MenuItemSub,
    /// `a->mDisabled` (0 or 1).
    MenuItemDisabled,
    /// `ScrollUntranslate(*a, *b)` (`a`, `b` point at i32s).
    ScrollUntranslate,
    /// `LocalToRoot(*a, *b)` (`a`, `b` point at i32s).
    LocalToRoot,
    /// `a->Reset()` (a submenu).
    MenuReset,
    /// `GetRoot()->QueueEvent(new DoPopupEvent(PopupParams(a, d, this, b[0], b[1])))` with the avoided rectangle the
    /// `EeWonRect4` at `c` (`b` points at two i32s).
    QueueSubMenu,
    /// `LocalToRoot(a, b)`, then the root's `QueueEvent(new MouseEvent(InputEvent_MouseUp, a, b, GetKeyMod(), c))`.
    QueueMouseUp,
    /// `a->TryKeyDown(b)` (a submenu; 0 or 1).
    MenuTryKeyDown,
    // ---- the scroll bar (`barobj`; a part is a `Component*`)
    /// `a->SetSize(b, c)`.
    ChildSetSize = 320,
    /// `a->Invalidate()`.
    ChildInvalidate,
    /// `a->mParent == this` (0 or 1).
    ChildIsMine,
    /// `IsDisabled()` (0 or 1).
    IsDisabled,
    /// `new Button` (no reference: the caller holds it with `ChildHold`).
    NewButton,
    /// `((Button*)a)->SetButtonFlags(b, c != 0)`.
    ButtonSetFlags,
    /// `Container::AddedToParent()`.
    BaseAddedToParent,
    /// `Component::SetEnabled(a != 0)`.
    BaseSetEnabled,
    // ---- the scroller (`scrollerobj`; `a` the scroll area, a `ScrollArea*`, or a bar, a `Scrollbar*`)
    /// `((Scrollbar*)a)->mRust` (a bar's Rust object).
    BarOf = 340,
    /// `a->AddListener(new ComponentListenerComp(this))`.
    ListenTo,
    /// the area `a`'s field `b` (`EeWonGuiField`).
    AreaGet,
    /// the area `a`'s field `b` = `c`.
    AreaSet,
    /// `a->AdjustPosition(b != 0)` (virtual).
    AreaAdjustPosition,
    /// `a->RequestFocus()`.
    AreaRequestFocus,
    /// `a->MouseWheel(b)` (virtual; 0 or 1).
    AreaMouseWheel,
    /// `a->SetScroller(this)`.
    AreaSetScroller,
    /// `mFocusChild != NULL` (0 or 1).
    HasFocusChild,
    /// `Container::MouseWheel(a)` (0 or 1).
    BaseMouseWheel,
    /// `Component::HandleComponentEvent(a)`.
    ComponentHandleEvent,
    /// `Virtual08()` (virtual: a subclass's layout).
    VirtSizeChanged,
    /// `NotifyScrollChange(a != 0)` (virtual).
    VirtNotifyScrollChange,
    // ---- the scroll area (`paneobj`; `a` a `Scroller*` where it says)
    /// `((Scroller*)a)->NotifyScrollChange(b != 0)` (virtual).
    ScrollerNotify = 360,
    /// `((Scroller*)a)->SetScrollArea(this)`.
    ScrollerSetArea,
    /// `dynamic_cast<ScrollbarScroller*>((Scroller*)a)` (NULL for NULL).
    ScrollerAsBars,
    /// `new ScrollbarScroller` (no reference: the original's scroll area borrowed it).
    NewScrollbarScroller,
    /// `((ScrollbarScroller*)a)->SetScrollbar((Scrollbar*)b)`.
    ScrollerSetScrollbar,
    /// `ScrollSizeChanged()` (virtual).
    VirtScrollSizeChanged,
    /// `ScrollPaint(*a)` (virtual; `a` the Graphics).
    VirtScrollPaint,
    /// `Container::Paint(*a)`.
    ContainerPaint,
    /// `mBackground.Paint(*a, 0, 0, Width(), Height(), b, c)`.
    PaneBackground,
    // ---- the container (`ctobj`; `a` a `Component*` where it says, this container's own one included)
    /// `Component::X(...)` on this container (`a` an `EeWonCtVirt`, `b`, `c` its arguments; 0 or 1 where it answers).
    CtBase = 400,
    /// `a->X(...)` (virtual: `b` an `EeWonCtVirt`, `c`, `d` its arguments; 0 or 1 where it answers).
    CtChild,
    /// `a->MouseDown(b, c, d)`.
    CtChildMouseDown,
    /// `a->MouseUp(b, c, d)`.
    CtChildMouseUp,
    /// `mParent->X(this)` (`a` an `EeWonCtVirt`: SetFocusChild, AddTimerChild, ChildInvalidated, or SetCursor of the
    /// `Cursor*` `b`; 0 or 1 where it answers).
    CtParentCall,
    /// this container's own virtual (`a` an `EeWonCtVirt`: FindChildAt(b, c), SetCursor(b), TabFocus(b, c),
    /// RemoveChild(b), MouseMove(b, c); the answer where it has one).
    CtVirt,
    /// `a->mParent->RemoveChild(a)`.
    CtDetach,
    /// `a->mParent = b != 0 ? this : NULL`.
    CtSetParent,
    /// `a->mFocusId = b`.
    CtSetFocusId,
    /// `a->mInvalidState = b`.
    CtSetInvalidState,
    /// `mTimerFlags = a`.
    CtSetTimerFlags,
    /// `GetRootContainer()->AddBG((BackgroundGrabber*)a)`.
    CtAddBG,
    /// the child layout: `a` 0 `ExecuteInstructions(this)`, 1 `RemoveComponent(b)`, 2 `Clear()`, 3 the manager
    /// destroyed.
    CtLayout,
    /// `((Cursor*)a)->AddRef()` (the current cursor's `SmartPtr`).
    CtCursorHold,
    /// `((Cursor*)a)->Release()`.
    CtCursorRelease,
    /// `((Graphics*)a)->mColorScheme = (ColorScheme*)b`.
    CtSetScheme,
    // the container's pure reads
    /// `a->mComponentBits`.
    CtBits = 430,
    /// `a->mParent`.
    CtParentOf,
    /// `a->mTimerFlags`.
    CtTimerFlags,
    /// `a->mInvalidState`.
    CtInvalidState,
    /// `a->mFocusId`.
    CtFocusId,
    /// `a->Contains(b, c)` (0 or 1).
    CtContains,
    /// `a->Intersects((Component*)b)` (0 or 1).
    CtIntersects,
    /// `a->GetGrabBG()`.
    CtGrabBG,
    /// `a->GetOwnColorScheme()`.
    CtOwnScheme,
    /// `((Graphics*)a)->mColorScheme`.
    CtScheme,
    /// `GetRootContainer()` (as a `Component*`).
    CtRoot,
    /// `GetRoot()->mManager->mFocusWindow == GetRoot()` (0 or 1).
    CtRootFocused,
    /// `GetRoot()->mMouseIsInClientArea` (0 or 1).
    CtMouseInClient,
    /// `GetDefaultCursor()`.
    CtDefaultCursor,
    /// `WantFocus()` (0 or 1).
    CtWantFocus,
    /// `a->GetNextSibling(b != 0, c != 0)`.
    CtNextSibling,
    /// `dynamic_cast<Container*>(a)`'s Rust container (`EeWonContainer*`; 0 when `a` is no container).
    CtContainerOf,
    /// `((ScrollArea*)a)->IsRectVisible(*(WONRectangle*)b)` (0 or 1).
    VpAreaRectVisible,
    // ---- the dialog (`dialogobj`)
    /// `Button::SetDialogDefaultButton((Button*)b)` when `a`, else `Button::UnSetDialogDefaultButton((Button*)b)`.
    DlgDefaultButton = 460,
    /// `mParent->BringToTop(this)`.
    DlgBringToTop,
    /// `InvalidateRect(*(WONRectangle*)a)` (virtual; it may move the rectangle).
    DlgInvalidateRect,
    /// `GetRoot()->EndDialog(a, (Component*)b)`.
    DlgEndDialog,
    /// `((Button*)a)->Fn2F3440()` (the click).
    DlgClick,
    /// `GetRoot()->mManager->mModalComponent == this` (0 or 1; a pure read).
    DlgIsModal,
    /// `((Button*)a)->IsDefaultButton()` (0 or 1; a pure read).
    DlgIsDefault,
    /// `((Button*)a->mComponent)->mButtonFlags` (a `ComponentEvent*`; a pure read).
    DlgEventButtonFlags,
    // ---- the tab control and its bar (`tabobj`)
    /// `((Button*)a)->GetText() == *(const GUIString*)b` (0 or 1; a pure read).
    TabTextIs = 480,
    /// `AddLayoutPoint((LayoutPoint*)a)`.
    TabAddLayoutPoint,
    // ---- the combo box (`comboobj`; the parts are its C++ members: `mEdit`, `mButton`, `mListBox`, `mListArea`)
    /// the part `a` (0 the input box, 1 the button, 2 the list box, 3 the list area) as a `Component*` (a pure read).
    ComboPart = 490,
    /// `mListArea->InsertString(*(const GUIString*)a, b)` (the item).
    ComboInsertString,
    /// `mListArea->InsertItem((TextListItem*)a, b)`.
    ComboInsertItem,
    /// `SmartPtr<TextListItem> held = (TextListItem*)a;` (taken and released).
    ComboDropItem,
    /// `mListArea->DeleteItem(a)`.
    ComboDeleteItem,
    /// `mListArea->RefreshItem((TextListItem*)a)`.
    ComboRefreshItem,
    /// `mListArea->Clear(true)`.
    ComboListClear,
    /// `mListArea->GetSelItem()`.
    ComboSelItem,
    /// `mListArea->GetItemPos((ListItem*)a)`.
    ComboItemPos,
    /// `mListArea->SetSelItem(a, b != 0)`.
    ComboSetSel,
    /// `mListBox = (Scroller*)a`.
    ComboSetListBox,
    /// `((Scroller*)a)->GetScrollArea()` as a `Component*`.
    ComboBoxArea,
    /// `mListArea->SetScroller(mListBox)`.
    ComboSetScroller,
    /// `mListArea->SetListFlags(a, b != 0)`.
    ComboSetListFlags,
    /// the list area's `a`: 0 `GetRightPad()`, 1 `GetLeftPad()`, 2 `GetTotalWidth()`, 3 `GetTotalHeight()`.
    ComboListMetric,
    /// `mEdit->SetText(GUIString(""), a != 0)`.
    ComboEditClear,
    /// `mEdit->SetText(((TextListItem*)a)->mText, true)`.
    ComboEditItemText,
    /// `mEdit->SetText(*(const GUIString*)a, b != 0)`.
    ComboEditSetText,
    /// `mEdit->SetText(mUndoStr, true)`.
    ComboEditUndo,
    /// `mUndoStr = mEdit->GetText()`.
    ComboUndoSave,
    /// `mEdit->SelectRange(a, b)`.
    ComboEditSelect,
    /// `mEdit->SetAllowSelection(a != 0)`.
    ComboEditAllowSelection,
    /// `((TextListItem*)a)->mWidth`.
    ComboItemWidth,
    /// `((TextListItem*)a)->mText.length()`.
    ComboItemTextLength,
    /// `DeliverEvent(new ComboSelChangedEvent(this, (TextListItem*)a, b != 0), false)`.
    ComboDeliverSel,
    /// `mListArea->GetNumItems()`.
    ComboItemCount,
    /// `mListArea->GetItem(a)`.
    ComboItemAt,
    /// `((TextListItem*)a)->mText == *(const GUIString*)b`, or the input box's text when `b` is 0 (0 or 1).
    ComboItemTextIs,
    /// the list box opened as a popup below the combo box (`GetRoot()->OpenPopup(PopupParams(...))`).
    ComboOpenPopup,
    /// `mListArea->EnsureVisible((ListItem*)a)`.
    ComboEnsureVisible,
    /// `((ListItemClickedEvent*)a)->mButton`.
    ComboEventButton,
    /// `Relayout()`.
    ComboRelayout,
    /// `mListBox->AddListener(NULL)`.
    ComboUnlisten,
}

/// The virtuals a container calls on its children, on its parent, on itself and as `Component::X` (`CtBase`,
/// `CtChild`, `CtParentCall`, `CtVirt`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonCtVirt {
    NeedsPaint = 1,
    /// `PrePaint(*(Graphics*)arg)`.
    PrePaint,
    /// `Paint(*(Graphics*)arg)`.
    Paint,
    /// `EnableControlsById(*(const ControlIdSet*)arg1, arg2 != 0)`.
    EnableControlsById,
    NotifyRootVisibility,
    HitTest,
    MouseEnter,
    MouseExit,
    MouseMove,
    MouseDrag,
    MouseWheel,
    MouseHover,
    /// `HandleBroadcastEvent((ComponentEvent*)arg)`.
    HandleBroadcastEvent,
    AddedToParent,
    Invalidate,
    MarkInvalid,
    ClearGrabBG,
    TabFocus,
    KeyDown,
    KeyChar,
    KeyUp,
    CanClose,
    GotFocus,
    LostFocus,
    TimerEvent,
    /// `RequestTimer(arg != 0)` (not virtual).
    RequestTimer,
    /// `RequestFocus()` (not virtual).
    RequestFocus,
    /// `Virtual08()` (SizeChanged).
    SizeChanged,
    SetFocusChild,
    AddTimerChild,
    ChildInvalidated,
    /// `SetCursor((Cursor*)arg)`.
    SetCursor,
    FindChildAt,
    RemoveChild,
    /// `SetComponentFlags(ComponentFlag_HasFocus, true)` (not virtual).
    SetHasFocus,
}

/// The events a component object fires (`FireEvent`).
#[repr(u32)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum EeWonGuiEvent {
    /// `ComponentEvent_InputReturn`.
    InputReturn = 1,
    /// `ComponentEvent_InputTextChanged`.
    InputTextChanged = 2,
    /// `ComponentEvent_ListSelChanged`.
    ListSelChanged = 3,
    /// `ComponentEvent_Scroll`.
    Scroll = 4,
    /// `ComponentEvent_TabCtrlTabSelected`.
    TabCtrlTabSelected = 5,
}

/// The `WindowEventType` a `FireEvent` op delivers (`ee/wonapi/WONEventTypes.h`): the host fires exactly this, so
/// no event code needs a case of its own in the C++ (round 23's lists fired every event but InputReturn as
/// InputTextChanged through such a switch). The goldens trace the type listeners receive through this function and
/// compare it with the original's enum.
#[no_mangle]
pub extern "C" fn ee_won_gui_event_type(event: EeWonGuiEvent) -> i32 {
    match event {
        EeWonGuiEvent::Scroll => 16,
        EeWonGuiEvent::InputReturn => 20,
        EeWonGuiEvent::InputTextChanged => 21,
        EeWonGuiEvent::ListSelChanged => 22,
        EeWonGuiEvent::TabCtrlTabSelected => 29,
    }
}

/// Appends `count` text units of `unit_size` bytes (2 or 4: a `wchar_t`) to the Rust buffer of a take op.
///
/// # Safety
/// `sink` is the `b` of a take op; `units` holds `count` units of `unit_size` bytes.
#[no_mangle]
pub unsafe extern "C" fn ee_won_gui_sink_units(
    sink: *mut c_void,
    units: *const c_void,
    count: usize,
    unit_size: usize,
) {
    if sink.is_null() || units.is_null() || count == 0 {
        return;
    }
    // SAFETY: the contract
    let out = unsafe { &mut *(sink as *mut Vec<u32>) };
    for i in 0..count {
        // SAFETY: the contract (unaligned reads: the units are a wchar_t array)
        let u = unsafe {
            match unit_size {
                2 => u32::from((units as *const u16).add(i).read_unaligned()),
                4 => (units as *const u32).add(i).read_unaligned(),
                _ => u32::from((units as *const u8).add(i).read()),
            }
        };
        out.push(u);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn event_types_are_the_original_enum() {
        // WindowEventType: 14 input events after None, then ButtonPressed (15), Scroll, TextLinkActivated,
        // HyperLinkActivated, HyperLinkOver, InputReturn (20), InputTextChanged, ListSelChanged
        assert_eq!(ee_won_gui_event_type(EeWonGuiEvent::Scroll), 16);
        assert_eq!(ee_won_gui_event_type(EeWonGuiEvent::InputReturn), 20);
        assert_eq!(ee_won_gui_event_type(EeWonGuiEvent::InputTextChanged), 21);
        assert_eq!(ee_won_gui_event_type(EeWonGuiEvent::ListSelChanged), 22);
    }

    #[test]
    fn sink_reads_both_unit_sizes() {
        let mut v: Vec<u32> = Vec::new();
        let a: [u16; 3] = [0x41, 0x20, 0x263a];
        let b: [u32; 2] = [0x1f600, 0x42];
        // SAFETY: the buffers hold the counts given
        unsafe {
            ee_won_gui_sink_units(&mut v as *mut Vec<u32> as *mut c_void, a.as_ptr() as *const c_void, 3, 2);
            ee_won_gui_sink_units(&mut v as *mut Vec<u32> as *mut c_void, b.as_ptr() as *const c_void, 2, 4);
            ee_won_gui_sink_units(std::ptr::null_mut(), b.as_ptr() as *const c_void, 2, 4);
        }
        assert_eq!(v, vec![0x41, 0x20, 0x263a, 0x1f600, 0x42]);
    }
}
