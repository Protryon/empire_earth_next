// The GUI hosts' one-statement cases the Rust compositions of R6 WON round 33 call (guisplit.rs), as they are in
// GuiHost.cpp and WindowManager.cpp (each case line copied from the working tree), over won_guisplit33_world.h; the
// driver registers them behind the game hosts (ee_won_gui_split_register). An op of no case here is logged as
// unexpected. sMutate (the driver's) makes the case of that number answer wrong (the mutations' check).
namespace GsNew {
using namespace GsWorld;
typedef EeWonGuiOp Op;
static TextSegment* Seg(intptr_t a) { return (TextSegment*)a; }
static ListItem* Item(intptr_t a) { return (ListItem*)a; }
static GUIString sHostText;
static PopupParams sHostParams(NULL, 0, NULL);
static SmartPtr<Font> sHostFont;
static int sMutate = -1; // the op (EeWonGuiOp's value) whose statement is skipped and answer is off by one
static bool Unexpected(Op op) { Log("unexpected op " + N((long long)op)); return false; }

static intptr_t Input(void* inComp, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)inComp; (void)b; (void)c; (void)d;
    switch (op) {
    case Op::HostTextClipboard: sHostText = Clipboard::Get(); return 0;
    case Op::HostTextSink: ee_won_gui_sink_units((void*)a, sHostText.mData->mText, sHostText.length(), sizeof(wchar_t)); return 0;
    case Op::HostTextClear: sHostText = GUIString(); return 0;
    default: Unexpected(op); return 0;
    }
}

static intptr_t Text(void* inComp, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)b; (void)c; (void)d;
    TextArea* t = (TextArea*)inComp;
    switch (op) {
    case Op::TextSetLinkParam: t->mLinkParam = Seg(a)->GetLinkParam(); return 0;
    case Op::TextDeliverLinkEvent: t->DeliverEvent(new TextLinkActivatedEvent(t, Seg(a)), false); return 0;
    default: Unexpected(op); return 0;
    }
}

static intptr_t List(void* inComp, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    ListArea* t = (ListArea*)inComp;
    switch (op) {
    case Op::ListRoot: return (intptr_t)t->GetRoot();
    case Op::WinMouseX: return ((Window*)a)->GetMouseX();
    case Op::WinMouseY: return ((Window*)a)->GetMouseY();
    case Op::ListParent: return (intptr_t)(Component*)t->mParent;
    case Op::ParentRootToChild: static_cast<Container*>((Component*)a)->RootToChild(((int*)b)[0], ((int*)b)[1]); return 0;
    case Op::ListContains: return t->Contains((int)a, (int)b) ? 1 : 0;
    case Op::TipExists: return t->mItemToolTip.get() != NULL ? 1 : 0;
    case Op::TipNew: t->mItemToolTip = new ListAreaToolTip(t); return 0;
    case Op::TipDo: t->mItemToolTip->DoTip(Item(a)); return 0;
    case Op::ParamsMake: sHostParams = PopupParams((PopupMenu*)a, (int)b, t, (int)c, (int)d); return 0;
    case Op::ParamsAvoid: sHostParams.mAvoidRect = WONRectangle(((const EeWonRect4*)a)->x, ((const EeWonRect4*)a)->y, ((const EeWonRect4*)a)->w, ((const EeWonRect4*)a)->h); return 0;
    case Op::QueueDoPopup: t->GetRoot()->QueueEvent(new DoPopupEvent(sHostParams)); return 0;
    case Op::ListLocalToRoot: t->LocalToRoot(((int*)a)[0], ((int*)a)[1]); return 0;
    case Op::WinQueueMouseUp: ((Window*)a)->QueueEvent(new MouseEvent(InputEvent_MouseUp, (int)b, (int)c, ((Window*)a)->GetKeyMod(), (int)d)); return 0;
    default: Unexpected(op); return 0;
    }
}

static intptr_t WmWinCall(Window* t, intptr_t call, intptr_t c, intptr_t d)
{
    (void)d;
    switch ((EeWonWmWin)call) {
    case EeWonWmWin::Hold: t->AddRef(); return 0;
    case EeWonWmWin::SetIsPopup: t->SetIsPopup(c != 0); return 0;
    default: Log("unexpected window call " + N((long long)call)); return 0;
    }
}

static intptr_t Wm(void* inWm, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    WindowManager* t = (WindowManager*)inWm;
    switch (op) {
    case Op::WmWin: return WmWinCall((Window*)b, a, c, d);
    case Op::WmFontHold: ((Font*)a)->AddRef(); return 0;
    case Op::WmNativeWindowNew: return (intptr_t)CreateNativeWindow();
    case Op::WinSetOneComponent: ((Window*)a)->SetOneComponentWindow(true); return 0;
    case Op::WinSetStyle: ((Window*)a)->SetStyle((int)b); return 0;
    case Op::WinSetParent: ((Window*)a)->SetParent((Window*)b); return 0;
    case Op::WinCreate: ((Window*)a)->Create(*(const WONRectangle*)b); return 0;
    case Op::WmFontKeep: sHostFont = t->GetDefaultWindow()->GetFont(FontDescriptor("Tahoma", 0, 8)); return 0;
    case Op::WmHostFont: return (intptr_t)sHostFont.get();
    case Op::WmHostFontClear: sHostFont = SmartPtr<Font>(); return 0;
    case Op::WmScreenResGet: t->GetScreenResolutionHook(*(int*)a, *(int*)b); return 0;
    case Op::WmScreenResSet: return t->SetScreenResolutionHook((int)a, (int)b) ? 1 : 0;
    case Op::WmInputRec: return (intptr_t)&static_cast<SizeEvent*>((WindowEventBase*)a)->mEv;
    default: Unexpected(op); return 0;
    }
}
}
