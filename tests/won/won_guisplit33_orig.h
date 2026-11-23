// The GUI hosts' cases R6 WON round 33 composed in Rust (guisplit.rs), as they were (GuiHost.cpp's and
// WindowManager.cpp's at fb32b70d8, the statements copied), over won_guisplit33_world.h.
namespace GsOrig {
using namespace GsWorld;
typedef EeWonGuiOp Op;
static TextSegment* Seg(intptr_t a) { return (TextSegment*)a; }
static ListItem* Item(intptr_t a) { return (ListItem*)a; }

// ComponentOp's (the input box's host reaches it)
static intptr_t Input(Component* c, Op op, intptr_t a, intptr_t b)
{
    (void)c; (void)b;
    switch (op) {
    case Op::ClipboardTake: {
        GUIString aText = Clipboard::Get();
        ee_won_gui_sink_units((void*)a, aText.mData->mText, aText.length(), sizeof(wchar_t));
        return 0;
    }
    default: return 0;
    }
}

static intptr_t Text(TextArea* t, Op op, intptr_t a)
{
    switch (op) {
    case Op::LinkActivated:
        t->mLinkParam = Seg(a)->GetLinkParam();
        t->DeliverEvent(new TextLinkActivatedEvent(t, Seg(a)), false);
        return 0;
    default: return 0;
    }
}

static intptr_t List(ListArea* t, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    switch (op) {
    case Op::PointerOutside: {
        Window* window = t->GetRoot();
        if (window == NULL) return 0;
        int x = window->GetMouseX(), y = window->GetMouseY();
        Container* parent = t->mParent;
        if (parent == NULL) return 0;
        parent->RootToChild(x, y);
        return t->Contains(x, y) ? 0 : 1;
    }
    case Op::TipShow:
        if (t->mItemToolTip.get() == NULL) t->mItemToolTip = new ListAreaToolTip(t);
        t->mItemToolTip->DoTip(Item(a));
        return 0;
    case Op::TipHide: if (t->mItemToolTip.get() != NULL) t->mItemToolTip->DoTip(NULL); return 0;
    case Op::QueueSubMenu: {
        const int* at = (const int*)b;
        const EeWonRect4* q = (const EeWonRect4*)c;
        PopupParams params((PopupMenu*)a, (int)d, t, at[0], at[1]);
        params.mAvoidRect = WONRectangle(q->x, q->y, q->w, q->h);
        t->GetRoot()->QueueEvent(new DoPopupEvent(params));
        return 0;
    }
    case Op::QueueMouseUp: {
        Window* window = t->GetRoot();
        int x = (int)a, y = (int)b;
        t->LocalToRoot(x, y);
        window->QueueEvent(new MouseEvent(InputEvent_MouseUp, x, y, window->GetKeyMod(), (int)c));
        return 0;
    }
    default: return 0;
    }
}

static intptr_t WmEventCall(WindowEventBase* t, intptr_t call, intptr_t c)
{
    switch ((EeWonWmEvent)call) {
    case EeWonWmEvent::Size: {
        SizeEvent* z = static_cast<SizeEvent*>(t);
        EeWonRect4 r = { z->mx, z->my, z->mWidth, z->mHeight };
        *(EeWonRect4*)c = r;
        return 0;
    }
    case EeWonWmEvent::SetSize: {
        SizeEvent* z = static_cast<SizeEvent*>(t);
        const EeWonRect4& r = *(const EeWonRect4*)c;
        z->mx = r.x; z->my = r.y; z->mWidth = r.w; z->mHeight = r.h;
        return 0;
    }
    default: return 0;
    }
}

static intptr_t Wm(WindowManager* t, Op op, intptr_t a, intptr_t b, intptr_t c)
{
    switch (op) {
    case Op::WmNewPopupWindow: {
        // DoPopup's `aPopupWindow = CreateNativeWindow();` and its set-up; the Rust gives the reference back
        Window* aPopupWindow = CreateNativeWindow();
        aPopupWindow->AddRef();
        aPopupWindow->SetOneComponentWindow(true);
        aPopupWindow->SetStyle(0x20);
        aPopupWindow->SetParent((Window*)a);
        aPopupWindow->SetIsPopup(true);
        aPopupWindow->Create(*(const WONRectangle*)b);
        return (intptr_t)aPopupWindow;
    }
    case Op::WmDefaultWindowFont: {
        SmartPtr<Font> aFont = t->GetDefaultWindow()->GetFont(FontDescriptor("Tahoma", 0, 8));
        if (aFont.get() != NULL) aFont->AddRef();
        return (intptr_t)aFont.get();
    }
    case Op::WmEvent: return WmEventCall((WindowEventBase*)b, a, c);
    case Op::WmResolution:
        if (a == 0) { t->GetScreenResolutionHook(*(int*)b, *(int*)c); return 0; }
        return t->SetScreenResolutionHook((int)b, (int)c) ? 1 : 0;
    default: return 0;
    }
}
}
