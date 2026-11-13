// Dialog as a whole object (R6 WON round 26): the original DialogBehavior.cpp and the constructor's state, the
// statements copied from the C++ before the port with every call on a C++ object (the window, the parent, the default
// button, the event, the dialog's own virtuals) replaced by the host op that makes it (Container.cpp's dialog host
// runs the same calls in the same order) and every Container::X call made on the dialog's real Rust container (each
// side has its own), against ee_won's Rust Dialog (dialogobj.rs).
//
// The world is the container goldens' (won_ct_world.h): component 1 is the dialog, 2..4 plain Rust containers, the
// rest leaves. A call the world makes on the dialog runs its overrides on the side running. The window's modal
// component, the dialog default button, the event's type, button flags and control id are the driver's.
// InvalidateRect is traced with the rectangle it is given: Component::InvalidateRect moves it by the component's
// place, and the original's drag gives the same rectangle to both of its calls.
//
// Each of 20,000 sequences builds a tree and makes 40 random calls on the dialog (and a few on the containers), with
// random changes of the world between them, then frees the dialog and the containers. The traces must be the same,
// every reference given back. The first 40 sequences and every later one that makes an op or a call none before made
// are rust/crates/ee_won/src/golden_dialogobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;
typedef EeWonDlgCall D;

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
#include "won_ct_world.h"

static intptr_t sModal, sDialogDefault, sEventType, sEventFlags, sEventControl;
static const intptr_t kDialog = 1;
static EeWonContainer* sCt[kCt + 1];

// ---- the original (DialogBehavior.cpp and the constructor; Container::X on the dialog's Rust container)

namespace orig {
struct WONRectangle { int x, y, width, height; };
struct Dialog {
    int mDragX, mDragY;
    bool mDragging, mAllowDrag, mKeepInRoot, mEndOnEscape;
    int mEscapeId;
    intptr_t mDefaultButton;
    intptr_t H(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)kDialog, op, a, b, c, d); }
    intptr_t Ct(C call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_ct_call(sCt[kDialog], call, a, b, c); }
    int Left() { return (int)H(Op::ChildLeft, kDialog); }
    int Top() { return (int)H(Op::ChildTop, kDialog); }
    int Width() { return (int)H(Op::ChildWidth, kDialog); }
    int Height() { return (int)H(Op::ChildHeight, kDialog); }

    Dialog()
    {
        this->mDragX = this->mDragY = 0;
        this->mDragging = false; // (the original left it unset)
        this->mAllowDrag = true;
        this->mKeepInRoot = true;
        this->mEndOnEscape = false;
        this->mEscapeId = -1;
        this->mDefaultButton = 0;
    }
    void SetButtonField(intptr_t b)
    {
        // SmartPtr<Button>::operator=: the old one released, then the new one held
        if (this->mDefaultButton != b) {
            if (this->mDefaultButton != 0) H(Op::ChildRelease, this->mDefaultButton);
            this->mDefaultButton = b;
            if (b != 0) H(Op::ChildHold, b);
        }
    }
    void Destroy() { SetButtonField(0); }
    void SetEndOnEscape(int id)
    {
        this->mEndOnEscape = true;
        this->mEscapeId = id;
    }
    void SetDontEndOnEscape() { this->mEndOnEscape = false; }
    void SetDefaultButton(intptr_t button) { SetButtonField(button); }
    void GotFocus()
    {
        H(Op::DlgDefaultButton, 1, this->mDefaultButton); // Button::SetDialogDefaultButton
        Ct(C::GotFocus);
    }
    void LostFocus()
    {
        H(Op::DlgDefaultButton, 0, this->mDefaultButton); // Button::UnSetDialogDefaultButton
        Ct(C::LostFocus);
    }
    void Virtual08()
    {
        this->mDragging = false;
        Ct(C::SizeChanged);
    }
    void MouseDown(int x, int y, int button)
    {
        Ct(C::MouseDown, x, y, button);
        if (button != 0) return;
        H(Op::RequestFocus);
        intptr_t aMouse = Ct(C::GetField, (intptr_t)EeWonCtField::MouseChild); // GetMouseChild()
        if (!this->mAllowDrag || aMouse == 0 || !(H(Op::CtBits, aMouse) & 0x20))
            return;
        if (H(Op::CtNextSibling, kDialog, 1, 0) != 0) H(Op::DlgBringToTop);
        this->mDragging = true;
        this->mDragX = x;
        this->mDragY = y;
    }
    void MouseUp(int x, int y, int button)
    {
        Ct(C::MouseUp, x, y, button);
        if (button == 0) this->mDragging = false;
    }
    void MouseExit()
    {
        Ct(C::MouseExit);
        this->mDragging = false;
    }
    void MouseDrag(int x, int y)
    {
        if (!this->mDragging) {
            Ct(C::MouseDrag, x, y);
            return;
        }
        intptr_t root = H(Op::CtRoot);
        int newX = this->Left() + (x - this->mDragX);
        int newY = this->Top() + (y - this->mDragY);
        if (this->mKeepInRoot) {
            if (newX + this->Width() > (int)H(Op::ChildWidth, root)) newX = (int)H(Op::ChildWidth, root) - this->Width();
            if (newX < 0) newX = 0;
            if (newY + this->Height() > (int)H(Op::ChildHeight, root)) newY = (int)H(Op::ChildHeight, root) - this->Height();
            if (newY < 0) newY = 0;
        }
        if (newX == this->Left() && newY == this->Top()) return;
        WONRectangle r = {0, 0, this->Width(), this->Height()};
        H(Op::CtParentCall, (intptr_t)V::ChildInvalidated);
        H(Op::DlgInvalidateRect, (intptr_t)&r);
        H(Op::ChildSetPos, kDialog, newX, newY);
        H(Op::DlgInvalidateRect, (intptr_t)&r);
        H(Op::Invalidate);
    }
    bool KeyDown(int key)
    {
        if (Ct(C::KeyDown, key)) return true;
        if (key == 27 && this->mEndOnEscape) {
            H(Op::DlgEndDialog, this->mEscapeId, kDialog);
            return true;
        } else if (key == 13) return this->ActivateDefaultButton();
        return false;
    }
    bool CanClose()
    {
        if (!Ct(C::CanClose)) return false;
        if (H(Op::DlgIsModal)) {
            if (this->mEndOnEscape) H(Op::DlgEndDialog, this->mEscapeId, 0);
            else return false;
        }
        return true;
    }
    bool ActivateDefaultButton()
    {
        if (this->mDefaultButton != 0 && !(H(Op::CtBits, this->mDefaultButton) & 0x80) && H(Op::DlgIsDefault, this->mDefaultButton)) {
            H(Op::DlgClick, this->mDefaultButton);
            return true;
        }
        return false;
    }
    void HandleComponentEvent(intptr_t event)
    {
        if (H(Op::EventType, event) == 20) { this->ActivateDefaultButton(); return; }
        if (H(Op::EventType, event) == 15 && (H(Op::DlgEventButtonFlags, event) & 0x100)) {
            H(Op::DlgEndDialog, H(Op::EventCompControlId, event), 0);
            return;
        }
        H(Op::ComponentHandleEvent, event);
    }
    intptr_t Field(int f)
    {
        switch ((EeWonDlgField)f) {
        case EeWonDlgField::AllowDrag: return mAllowDrag;
        case EeWonDlgField::KeepInRoot: return mKeepInRoot;
        case EeWonDlgField::EndOnEscape: return mEndOnEscape;
        case EeWonDlgField::EscapeId: return mEscapeId;
        case EeWonDlgField::Dragging: return mDragging;
        case EeWonDlgField::DragX: return mDragX;
        case EeWonDlgField::DragY: return mDragY;
        case EeWonDlgField::DefaultButton: return mDefaultButton;
        }
        return 0;
    }
    void SetField(int f, intptr_t v)
    {
        if ((EeWonDlgField)f == EeWonDlgField::AllowDrag) mAllowDrag = v != 0;
        else if ((EeWonDlgField)f == EeWonDlgField::KeepInRoot) mKeepInRoot = v != 0;
        else if ((EeWonDlgField)f == EeWonDlgField::Dragging) mDragging = v != 0;
    }
    intptr_t Call(D call, intptr_t a, intptr_t b, intptr_t c)
    {
        switch (call) {
        case D::SetEndOnEscape: SetEndOnEscape((int)a); return 0;
        case D::SetDontEndOnEscape: SetDontEndOnEscape(); return 0;
        case D::SetDefaultButton: SetDefaultButton(a); return 0;
        case D::GotFocus: GotFocus(); return 0;
        case D::LostFocus: LostFocus(); return 0;
        case D::SizeChanged: Virtual08(); return 0;
        case D::MouseDown: MouseDown((int)a, (int)b, (int)c); return 0;
        case D::MouseUp: MouseUp((int)a, (int)b, (int)c); return 0;
        case D::MouseExit: MouseExit(); return 0;
        case D::MouseDrag: MouseDrag((int)a, (int)b); return 0;
        case D::KeyDown: return KeyDown((int)a);
        case D::CanClose: return CanClose();
        case D::ActivateDefaultButton: return ActivateDefaultButton();
        case D::HandleComponentEvent: HandleComponentEvent(a); return 0;
        case D::GetField: return Field((int)a);
        case D::SetField: SetField((int)a, b); return 0;
        }
        return 0;
    }
};
}

static orig::Dialog* sOrig;
static EeWonDialog* sRust;

static intptr_t DialogCall(D call, intptr_t a, intptr_t b, intptr_t c)
{
    if (sRustSide) return ee_won_dlg_call(sRust, call, a, b, c);
    return sOrig->Call(call, a, b, c);
}

// the dialog's overrides of the container's virtuals
static D DialogOverride(C call)
{
    switch (call) {
    case C::SizeChanged: return D::SizeChanged;
    case C::MouseDown: return D::MouseDown;
    case C::MouseUp: return D::MouseUp;
    case C::MouseExit: return D::MouseExit;
    case C::MouseDrag: return D::MouseDrag;
    case C::GotFocus: return D::GotFocus;
    case C::LostFocus: return D::LostFocus;
    case C::KeyDown: return D::KeyDown;
    case C::CanClose: return D::CanClose;
    default: return (D)0;
    }
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (ct == kDialog && (int)DialogOverride(call) != 0) return DialogCall(DialogOverride(call), a, b, c);
    return ee_won_ct_call(sCt[ct], call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return (intptr_t)sCt[ct]; }

static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)self; (void)b; (void)c; (void)d;
    switch (op) {
    case Op::DlgIsModal: *r = sModal == self; return true;
    case Op::DlgIsDefault: *r = sDialogDefault == a; return true;
    case Op::DlgEventButtonFlags: *r = sEventFlags; return true;
    case Op::EventType: *r = sEventType; return true;
    case Op::EventCompControlId: *r = sEventControl; return true;
    default: return false;
    }
}

static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)b; (void)c; (void)d;
    if (op != Op::DlgInvalidateRect) return false;
    const orig::WONRectangle* q = (const orig::WONRectangle*)a;
    Line("op %lld rect %lld %lld %lld %lld @%lld", (long long)op, q->x, q->y, q->width, q->height, self);
    return true;
}

static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)c; (void)d; (void)r;
    World& w = sW[self];
    switch (op) {
    case Op::DlgDefaultButton:
        if (a) sDialogDefault = b;
        else if (sDialogDefault == b) sDialogDefault = 0;
        Line("env 9 %lld", sDialogDefault);
        break;
    case Op::DlgBringToTop: if (w.parent != 0) Do(w.parent, C::BringToTop, self); break;
    case Op::DlgInvalidateRect:
        // Component::InvalidateRect: moved by the component's place when it has a parent
        if (w.parent != 0) {
            orig::WONRectangle* q = (orig::WONRectangle*)a;
            q->x += w.x;
            q->y += w.y;
        }
        break;
    case Op::ChildSetPos: Set((int)a, 3, b); Set((int)a, 4, c); break;
    default: break;
    }
}

// ---- the driver

static void DialogTop(D call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    Line("dlg %lld %lld %lld %lld", (long long)call, a, b, c);
    long long r = DialogCall(call, a, b, c);
    Line("-> %lld 0", r);
}

static void RandomDialogCall()
{
    int x = (int)DR(260) - 30, y = (int)DR(260) - 30;
    switch (DR(22)) {
    case 0: DialogTop(D::SetEndOnEscape, (int)DR(9) - 1); break;
    case 1: DialogTop(D::SetDontEndOnEscape); break;
    case 2: DialogTop(D::SetDefaultButton, DR(4) ? 5 + DR(kN - 4) : 0); break;
    case 3: DialogTop(D::GotFocus); break;
    case 4: DialogTop(D::LostFocus); break;
    case 5: DialogTop(D::SizeChanged); break;
    case 6: case 7: {
        // half of the presses on one of the dialog's children
        intptr_t n = SideCall(kDialog, C::GetField, (intptr_t)EeWonCtField::NumChildren, 0, 0);
        if (n > 0 && DR(2)) {
            int k = (int)DR((unsigned)n);
            intptr_t ch = SideCall(kDialog, C::GetField, (intptr_t)EeWonCtField::FrontChild, 0, 0);
            while (k-- > 0) ch = SideCall(kDialog, C::NextChild, ch, 1, 0);
            x = sW[ch].x + (int)DR((unsigned)sW[ch].w);
            y = sW[ch].y + (int)DR((unsigned)sW[ch].h);
        }
        DialogTop(D::MouseDown, x, y, DR(4) ? 0 : 1);
        break;
    }
    case 8: DialogTop(D::MouseUp, x, y, DR(3) ? 0 : 2); break;
    case 9: DialogTop(D::MouseExit); break;
    case 10: case 11: case 12: DialogTop(D::MouseDrag, x, y); break;
    case 13: { static const int kKeys[4] = {27, 13, 9, 65}; DialogTop(D::KeyDown, kKeys[DR(4)]); break; }
    case 14: DialogTop(D::CanClose); break;
    case 15: DialogTop(D::ActivateDefaultButton); break;
    case 16: {
        static const int kTypes[3] = {20, 15, 3};
        sEventType = kTypes[DR(3)]; sEventFlags = DR(2) ? 0x100 : 0x40; sEventControl = (int)DR(7);
        Line("env 10 %lld %lld %lld", sEventType, sEventFlags, sEventControl);
        DialogTop(D::HandleComponentEvent, kEvent);
        break;
    }
    case 17: DialogTop(D::GetField, 1 + DR(8)); break;
    case 18: { static const int kSet[3] = {1, 2, 5}; DialogTop(D::SetField, kSet[DR(3)], DR(2)); break; }
    case 19: TopCall(kDialog, C::AddChild, 5 + DR(kN - 4), 0, 0); break;
    case 20: TopCall(kDialog, C::MouseMove, x, y, 0); break;
    default:
        sModal = DR(2) ? kDialog : 0;
        sDialogDefault = DR(2) ? 5 + DR(kN - 4) : 0;
        Line("env 8 %lld", sModal);
        Line("env 9 %lld", sDialogDefault);
        break;
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 13u;
    sAnswerSeed = inSeq * 2654435761u + 7u;
    InitWorld();
    // drag handles: a third of the leaves
    for (int id = kCt + 1; id <= kN; id++)
        if (DR(3) == 0) Set(id, 0, sW[id].bits | 0x20);
    sModal = 0; sDialogDefault = 0; sEventType = 0; sEventFlags = 0; sEventControl = 0;
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        sCt[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
    }
    Line("dialog");
    if (inRust) sRust = ee_won_dlg_new((void*)kDialog, Host, sCt[kDialog]);
    else sOrig = new orig::Dialog;
    // the dialog under 2 (or not), the containers under 2, the leaves anywhere
    if (DR(4)) TopCall(2, C::AddChild, kDialog, 0, 0);
    for (int id = 3; id <= kN; id++) {
        if (DR(5) == 0) continue;
        int p = id <= kCt ? 2 : 1 + (int)DR(kCt);
        TopCall(p, C::AddChild, id, 0, 0);
    }
    Env(0, DR(2) ? 2 : 3);
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomDialogCall();
    }
    Line("free-dialog");
    if (inRust) { ee_won_dlg_free(sRust); sRust = NULL; }
    else { sOrig->Destroy(); delete sOrig; sOrig = NULL; }
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        ee_won_ct_free(sCt[ct]);
        sCt[ct] = NULL;
    }
    for (int id = 1; id <= kN; id++) CHECK(sW[id].refs == 0);
    for (std::map<intptr_t, int>::iterator i = sCursorRefs.begin(); i != sCursorRefs.end(); ++i) CHECK(i->second == 0);
    return sTrace;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    const int kSequences = 20000;
    const int kCallsPer = aPrint ? 16 : 40;
    size_t aLines = 0;
    std::set<std::string> aSeen;
    int aDiffering = 0;
    for (int seq = 0; seq < kSequences; seq++) {
        sOpsMet.clear();
        std::vector<std::string> a = RunSide(false, (unsigned)seq, kCallsPer);
        std::vector<std::string> b = RunSide(true, (unsigned)seq, kCallsPer);
        aLines += a.size();
        bool same = a == b;
        CHECK(same);
        if (!same) {
            aDiffering++;
            if (aDiffering <= 3) {
                printf("sequence %d differs\n", seq);
                for (size_t i = 0; i < a.size() || i < b.size(); i++) {
                    const char* x = i < a.size() ? a[i].c_str() : "<end>";
                    const char* y = i < b.size() ? b[i].c_str() : "<end>";
                    if (strcmp(x, y) != 0) {
                        printf("  line %u\n", (unsigned)i);
                        for (size_t j = i > 6 ? i - 6 : 0; j < i; j++) printf("  both: %s\n", a[j].c_str());
                        printf("  orig: %s\n  rust: %s\n", x, y);
                        break;
                    }
                }
            }
        }
        bool aNew = seq < 40;
        for (std::set<int>::const_iterator i = sOpsMet.begin(); i != sOpsMet.end(); ++i)
            if (aSeen.insert("op" + std::to_string(*i)).second) aNew = true;
        for (size_t i = 0; i < a.size(); i++)
            if (a[i].compare(0, 4, "dlg ") == 0) {
                std::string key = a[i].substr(0, a[i].find(' ', 4));
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_dialogobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
