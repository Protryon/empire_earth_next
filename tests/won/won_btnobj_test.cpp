// Button as a whole object (R6 WON round 30): the original Button.cpp, the statements copied from the C++ before the
// port with every call on a C++ object (the component's base, its virtuals, the parent, the siblings, the click sound,
// the event) replaced by the host op that makes it (Button.cpp's host runs the same calls in the same order), against
// ee_won's Rust Button (btnobj.rs). The original's two static default buttons are the orig side's two statics here and
// the Rust module's two on the Rust side; their calls out go through the host with a NULL component.
//
// The world is the container goldens' (won_ct_world.h) with fourteen components: 1..3 real Rust containers (one per
// side), 4..13 buttons and 14 a plain leaf. A call the world makes on a button (a container's mouse, key, focus and
// timer routing, AddChild's AddedToParent) runs the button's override on the side running ("do" lines with the
// container call's code). The radio walk's dynamic_cast is the side's own table (BtnOf). A button's click sound is the
// driver's ("sound" lines); HandleActivate answers from the world's answer stream; the event is traced as the type the
// listeners receive (the original's enum, the Rust's through ee_won_gui_event_type).
//
// Each of 20,000 sequences builds a tree, sets random button flags and makes 40 random calls on the buttons, the
// containers (routed to the buttons) and the static functions, with random changes of the world between them; then
// it clears the default buttons, frees the containers and the buttons. The traces must be the same and every
// reference given back. The first 40 sequences and every later one that makes an op or a call none before made are
// rust/crates/ee_won/src/golden_btnobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "rust/ee_won.h"
#include "ee/wonapi/WONEventTypes.h"

typedef EeWonGuiOp Op;
typedef EeWonCtVirt V;
typedef EeWonCtCall C;
typedef EeWonBtnCall B;
typedef EeWonBtnStatic S;

#define WON_CT_N 14
#define WON_CT_CT 3
#define WON_CT_OVERRIDES
static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c);
static intptr_t CtObjectOf(intptr_t ct);
static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r);
static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static intptr_t Host(void* inComp, Op inOp, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
static bool HasOverride(intptr_t c, C o);
#include "won_ct_world.h"

static const int kFirstBtn = 4, kLastBtn = 13;
static bool IsBtn(intptr_t c) { return c >= kFirstBtn && c <= kLastBtn; }
static EeWonContainer* sCt[kCt + 1];
static bool sSound[kN + 1];

// ---- the original (Button.cpp)

namespace orig {
struct Button;
static Button* sBtn[kN + 1];
static Button* BtnOf(intptr_t c) { return IsBtn(c) ? sBtn[c] : NULL; } // dynamic_cast<Button*>

// the static SmartPtr<Button>s and their functions
static intptr_t mDefaultButton, mDialogDefaultButton;
static intptr_t SH(Op op, intptr_t a) { return Host((void*)0, op, a, 0, 0, 0); }
static void Assign(intptr_t& p, intptr_t b) // SmartPtr::operator=
{
    if (p != b) {
        if (p != 0) SH(Op::ChildRelease, p);
        if (b != 0) SH(Op::ChildHold, b);
        p = b;
    }
}
static void SetDefaultButton(intptr_t button)
{
    if (button == mDefaultButton) return;
    if (mDefaultButton != 0) SH(Op::ChildInvalidate, mDefaultButton);
    Assign(mDefaultButton, button);
    if (button != 0) SH(Op::ChildInvalidate, button);
}
static void UnSetDefaultButton(intptr_t) { SetDefaultButton(mDialogDefaultButton); }
static void SetDialogDefaultButton(intptr_t button)
{
    if (button == mDialogDefaultButton) return;
    Assign(mDialogDefaultButton, button);
    SetDefaultButton(button);
}
static void UnSetDialogDefaultButton(intptr_t button)
{
    if (button != mDialogDefaultButton) return;
    Assign(mDialogDefaultButton, 0);
    UnSetDefaultButton(button);
}

struct Button {
    intptr_t me;
    unsigned long mButtonFlags;
    bool mKeyDown;
    int mDelayAcc; // (the original left it unset: 0 here as on the Rust side)
    unsigned char mMouseDownState;

    explicit Button(intptr_t id) : me(id), mButtonFlags(0), mKeyDown(false), mDelayAcc(0), mMouseDownState(0) {}
    intptr_t H(Op op, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0, intptr_t d = 0) { return Host((void*)me, op, a, b, c, d); }
    void SetComponentFlags(int f, bool v) { H(Op::ChildSetFlags, me, f, v); }
    void Invalidate() { H(Op::Invalidate); }
    bool IsDisabled() { return (H(Op::CtBits, me) & 0x80) != 0; }
    bool WantFocus() { return H(Op::CtWantFocus) != 0; }
    void RequestFocus() { H(Op::RequestFocus); }
    void RequestTimer(bool v) { H(Op::RequestTimer, v); }
    void Fn2F5380() { H(Op::FireEvent, -1, 0); } // Fn2F5380(ComponentEvent_ButtonPressed, 0): see ExtraLine
    bool IsRepeat() { return (mButtonFlags & 0x02) != 0; }
    bool IsChecked() { return (mButtonFlags & 0x04) ? true : false; }
    bool IsRadio() { return (mButtonFlags & 0x08) != 0; }
    bool IsPressed() { return (mButtonFlags & 0x20) != 0; }
    bool IsActivateOnPress() { return (mButtonFlags & 0x40) != 0; }
    bool IsNoActivate() { return (mButtonFlags & 0x200) != 0; }
    bool IsDefaultOnFocus() { return (mButtonFlags & 0x400) != 0; }

    bool HandleActivate() { return H(Op::BtnHandleActivate) != 0; } // (virtual)
    void SetText(intptr_t text, bool invalidate)
    {
        H(Op::BtnSetTextHook, text);
        if (invalidate) this->Invalidate();
    }
    void CopyAttributes(Button* from) { mButtonFlags = from->mButtonFlags; } // (the click sound: C++'s)
    bool IsMouseOver() { return mButtonFlags & 0x10 ? true : false; }
    bool IsDefaultButton() { return mDefaultButton == me; }
    void SetButtonFlags(unsigned long flags, bool set)
    {
        if (set) mButtonFlags |= flags;
        else mButtonFlags &= ~flags;
        if (this->IsRadio()) this->SetComponentFlags(0x8, this->IsChecked());
    }
    void AddedToParent()
    {
        H(Op::CtBase, (intptr_t)V::AddedToParent); // Component::AddedToParent()
        this->SetButtonFlags(0x30, false);
        if (!this->IsRadio())
            return;
        this->SetCheck(this->IsChecked(), false);
        if (this->WantFocus())
            return;
        Button* radio = this->GetEndRadio(false);
        while (radio != NULL) {
            if (radio->WantFocus()) return;
            radio = radio->GetNextRadio(true);
        }
        this->SetComponentFlags(0x8, true);
    }
    void Fn2F3440()
    {
        if (this->IsDisabled() || this->IsNoActivate()) return;
        if (this->IsRadio()) this->SetCheck(true, false);
        else this->SetCheck(!this->IsChecked(), false);
        if (this->HandleActivate()) this->Fn2F5380();
    }
    Button* GetNextRadio(bool forward)
    {
        intptr_t aSibling = H(Op::CtNextSibling, me, forward, 0); // GetNextSibling(forward, false)
        Button* button = BtnOf(aSibling);
        return button != NULL && button->IsRadio() ? button : NULL;
    }
    Button* GetEndRadio(bool forward)
    {
        Button* last = this;
        while (true) {
            Button* next = last->GetNextRadio(forward);
            if (next == NULL) break;
            last = next;
        }
        return last;
    }
    void SetCheck(bool checked, bool notify)
    {
        if (this->IsRadio()) {
            this->SetComponentFlags(0x8, checked);
            if (checked) {
                Button* radio = this->GetEndRadio(false);
                while (radio != NULL) {
                    if (radio != this) {
                        H(Op::ChildSetFlags, radio->me, 0x8, 0); // radio->SetComponentFlags(WantFocus, false)
                        if (radio->IsChecked()) { radio->SetButtonFlags(4, false); H(Op::ChildInvalidate, radio->me); }
                    }
                    radio = radio->GetNextRadio(true);
                }
                this->RequestFocus();
            }
        }
        if (notify) this->Fn2F5380();
        if (this->IsChecked() != checked) { this->SetButtonFlags(4, checked); this->Invalidate(); }
    }
    void MouseEnter(int x, int y)
    {
        H(Op::CtBase, (intptr_t)V::MouseEnter, x, y);
        this->SetButtonFlags(0x10, true);
        if (mButtonFlags & 1) this->Invalidate();
    }
    void MouseDown(int, int, int button)
    {
        if (button == 0) { this->mMouseDownState = 2; this->ButtonDown(); }
    }
    void MouseUp(int, int, int button)
    {
        if (button == 0) { this->mMouseDownState = 0; this->ButtonUp(true); }
    }
    void MouseExit()
    {
        H(Op::CtBase, (intptr_t)V::MouseExit);
        this->mMouseDownState = 0;
        this->ButtonUp(false);
        if (this->IsMouseOver() && (mButtonFlags & 1)) this->Invalidate();
        this->SetButtonFlags(0x10, false);
    }
    void ButtonDown()
    {
        if (this->IsPressed()) return;
        if (H(Op::BtnHasClickSound) != 0) H(Op::BtnPlayClick); // if (mClickSound.get() != NULL) mClickSound->Play()
        this->SetButtonFlags(0x20, true);
        this->Invalidate();
        if (this->IsRepeat()) { this->mDelayAcc = 0; this->RequestTimer(true); this->Fn2F3440(); }
        if (this->IsActivateOnPress()) this->Fn2F3440();
        this->RequestFocus();
    }
    void ButtonUp(bool activate)
    {
        EeWonButtonUp anUp = ee_won_gui_button_up((uint32_t)mButtonFlags, this->mMouseDownState, this->mKeyDown, activate);
        if (anUp == EeWonButtonUp::Ignore) return;
        this->SetButtonFlags(0x20, false);
        this->Invalidate();
        if (anUp == EeWonButtonUp::StopRepeat) { this->RequestTimer(false); return; }
        if (anUp == EeWonButtonUp::Activate) this->Fn2F3440();
    }
    void MouseDrag(int x, int y)
    {
        int aWhere = ee_won_gui_button_drag(this->mMouseDownState, x, y, (int)H(Op::ChildWidth, me), (int)H(Op::ChildHeight, me));
        if (aWhere < 0) return;
        if (aWhere == 0) {
            this->mMouseDownState = 2;
            if (!this->IsPressed()) { this->ButtonDown(); this->mDelayAcc = 500; }
        } else { this->mMouseDownState = 1; this->ButtonUp(false); }
    }
    void GotFocus()
    {
        H(Op::CtBase, (intptr_t)V::GotFocus);
        if (this->IsDefaultOnFocus()) SetDefaultButton(me);
        this->Invalidate();
    }
    void LostFocus()
    {
        H(Op::CtBase, (intptr_t)V::LostFocus);
        if (this->mKeyDown) { this->mKeyDown = false; this->ButtonUp(false); }
        UnSetDefaultButton(me);
        this->Invalidate();
    }
    void ParentSetHasUsedDialogKeys() // if (mParent != NULL) mParent->SetHasUsedDialogKeys(true)
    {
        intptr_t p = H(Op::CtParentOf, me);
        if (p != 0) ee_won_ct_call(sCt[p], C::SetField, (intptr_t)EeWonCtField::HasUsedDialogKeys, 1, 0);
    }
    bool KeyDown(int key)
    {
        EeWonButtonKey anAction = ee_won_gui_button_key(key, this->IsDefaultButton(), this->mKeyDown, this->IsRadio());
        switch (anAction) {
        case EeWonButtonKey::Activate:
            this->Fn2F3440();
            return true;
        case EeWonButtonKey::Press:
            this->mKeyDown = true;
            ParentSetHasUsedDialogKeys();
            this->ButtonDown();
            return true;
        case EeWonButtonKey::RadioBack:
        case EeWonButtonKey::RadioForward: {
            ParentSetHasUsedDialogKeys();
            Button* next = this->GetNextRadio(anAction == EeWonButtonKey::RadioForward);
            if (next != NULL) { next->Fn2F3440(); H(Op::CtChild, next->me, (intptr_t)V::RequestFocus); }
            return true;
        }
        default:
            return false;
        }
    }
    bool KeyUp(int key)
    {
        if (key == 32) {
            this->mKeyDown = false;
            this->ButtonUp(true);
            return true;
        }
        return false;
    }
    bool TimerEvent(int delta)
    {
        if (ee_won_gui_button_repeat(this->mDelayAcc, delta, &this->mDelayAcc)) this->Fn2F3440();
        return true;
    }
    void MouseHover(unsigned long delta) { H(Op::CtBase, (intptr_t)V::MouseHover, (intptr_t)delta); }
    intptr_t Field(int f)
    {
        switch ((EeWonBtnField)f) {
        case EeWonBtnField::Flags: return (intptr_t)mButtonFlags;
        case EeWonBtnField::KeyDown: return mKeyDown;
        case EeWonBtnField::DelayAcc: return mDelayAcc;
        case EeWonBtnField::MouseDownState: return mMouseDownState;
        }
        return 0;
    }
    intptr_t Call(B call, intptr_t a, intptr_t b, intptr_t c)
    {
        switch (call) {
        case B::Click: Fn2F3440(); return 0;
        case B::SetText: SetText(a, b != 0); return 0;
        case B::SetCheck: SetCheck(a != 0, b != 0); return 0;
        case B::SetButtonFlags: SetButtonFlags((unsigned long)a, b != 0); return 0;
        case B::CopyAttributes: CopyAttributes((Button*)a); return 0;
        case B::IsMouseOver: return IsMouseOver();
        case B::IsDefaultButton: return IsDefaultButton();
        case B::AddedToParent: AddedToParent(); return 0;
        case B::MouseEnter: MouseEnter((int)a, (int)b); return 0;
        case B::MouseDown: MouseDown((int)a, (int)b, (int)c); return 0;
        case B::MouseUp: MouseUp((int)a, (int)b, (int)c); return 0;
        case B::MouseExit: MouseExit(); return 0;
        case B::ButtonDown: ButtonDown(); return 0;
        case B::ButtonUp: ButtonUp(a != 0); return 0;
        case B::MouseDrag: MouseDrag((int)a, (int)b); return 0;
        case B::GotFocus: GotFocus(); return 0;
        case B::LostFocus: LostFocus(); return 0;
        case B::KeyDown: return KeyDown((int)a);
        case B::KeyUp: return KeyUp((int)a);
        case B::TimerEvent: return TimerEvent((int)a);
        case B::MouseHover: MouseHover((unsigned long)a); return 0;
        case B::GetNextRadio: { Button* r = GetNextRadio(a != 0); return r != NULL ? r->me : 0; }
        case B::GetEndRadio: return GetEndRadio(a != 0)->me;
        case B::GetField: return Field((int)a);
        case B::SetField: return 0;
        }
        return 0;
    }
};
static intptr_t StaticCall(S call, intptr_t a)
{
    switch (call) {
    case S::SetDialogDefault: SetDialogDefaultButton(a); return 0;
    case S::UnSetDialogDefault: UnSetDialogDefaultButton(a); return 0;
    case S::SetDefault: SetDefaultButton(a); return 0;
    case S::UnSetDefault: UnSetDefaultButton(a); return 0;
    case S::Default: return mDefaultButton;
    case S::DialogDefault: return mDialogDefaultButton;
    }
    return 0;
}
}

static EeWonButton* sRust[kN + 1];

// the side's object of a button (the radio walk's dynamic_cast, CopyAttributes' argument)
static intptr_t SideBtn(intptr_t c)
{
    if (!IsBtn(c)) return 0;
    return sRustSide ? (intptr_t)sRust[c] : (intptr_t)orig::sBtn[c];
}

static intptr_t BtnCall(intptr_t id, B call, intptr_t a, intptr_t b, intptr_t c)
{
    if (sRustSide) return ee_won_btn_call(sRust[id], call, a, b, c);
    return orig::sBtn[id]->Call(call, a, b, c);
}

static intptr_t StaticCall(S call, intptr_t a)
{
    if (sRustSide) return ee_won_btn_static(Host, call, a);
    return orig::StaticCall(call, a);
}

// the button's overrides of Component's virtuals, as the world's container calls
static B ButtonOverride(C o)
{
    switch (o) {
    case C::MouseEnter: return B::MouseEnter;
    case C::MouseExit: return B::MouseExit;
    case C::MouseDown: return B::MouseDown;
    case C::MouseUp: return B::MouseUp;
    case C::MouseDrag: return B::MouseDrag;
    case C::MouseHover: return B::MouseHover;
    case C::GotFocus: return B::GotFocus;
    case C::LostFocus: return B::LostFocus;
    case C::KeyDown: return B::KeyDown;
    case C::KeyUp: return B::KeyUp;
    case C::TimerEvent: return B::TimerEvent;
    case C::AddedToParent: return B::AddedToParent;
    default: return (B)0;
    }
}

static bool HasOverride(intptr_t c, C o)
{
    if ((int)o == 0) return false;
    if (IsCt(c)) return true;
    return IsBtn(c) && (int)ButtonOverride(o) != 0;
}

static intptr_t SideCall(int ct, C call, intptr_t a, intptr_t b, intptr_t c)
{
    if (IsBtn(ct)) return BtnCall(ct, ButtonOverride(call), a, b, c);
    return ee_won_ct_call(sCt[ct], call, a, b, c);
}
static intptr_t CtObjectOf(intptr_t ct) { return (intptr_t)sCt[ct]; }

static bool ExtraRead(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)b; (void)c; (void)d;
    switch (op) {
    case Op::BtnOf: *r = SideBtn(a); return true;
    case Op::BtnHasClickSound: *r = sSound[self]; return true;
    default: return false;
    }
}

static bool ExtraLine(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
    (void)c; (void)d;
    if (op != Op::FireEvent) return false;
    // the original fires ComponentEvent_ButtonPressed (the enum); the Rust side's event becomes a type through
    // ee_won_gui_event_type, the call Button.cpp's host makes
    long long type = a == -1 ? (long long)WONAPI::ComponentEvent_ButtonPressed : ee_won_gui_event_type((EeWonGuiEvent)a);
    Line("op %lld fire %lld %lld @%lld", (long long)op, type, b, self);
    return true;
}

static void ExtraOp(intptr_t self, Op op, intptr_t a, intptr_t b, intptr_t c, intptr_t d, intptr_t* r)
{
    (void)b; (void)c; (void)d;
    switch (op) {
    case Op::RequestTimer: Component_(self, V::RequestTimer, a, 0, false); break;
    case Op::BtnHandleActivate: *r = AR(4) != 0; break;
    default: break; // the text hook, the click sound, the event: traced only
    }
}

// ---- the driver

static void BtnTop(intptr_t id, B call, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0)
{
    Line("btn %lld %lld %lld %lld %lld", id, (long long)call, a, b, c);
    if (call == B::CopyAttributes) a = SideBtn(a);
    long long r = BtnCall(id, call, a, b, c);
    Line("-> %lld 0", r);
}

static void StaticTop(S call, intptr_t a)
{
    Line("bstatic %lld %lld", (long long)call, a);
    long long r = StaticCall(call, a);
    Line("-> %lld 0", r);
}

static intptr_t AnyBtn() { return kFirstBtn + DR(kLastBtn - kFirstBtn + 1); }

static unsigned long AnyFlags()
{
    static const unsigned long k[9] = {0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x200, 0x400};
    unsigned long f = 0;
    int n = 1 + (int)DR(3);
    while (n-- > 0) f |= k[DR(9)];
    return f;
}

static intptr_t AnyKey()
{
    static const int k[9] = {13, 32, 37, 38, 39, 40, 9, 65, 27};
    return k[DR(9)];
}

static void RandomButtonCall()
{
    intptr_t id = AnyBtn();
    int x = (int)DR(160) - 20, y = (int)DR(160) - 20;
    switch (DR(26)) {
    case 0: BtnTop(id, B::Click); break;
    case 1: BtnTop(id, B::SetText, 0x6200 + DR(4), DR(2)); break;
    case 2: case 3: BtnTop(id, B::SetCheck, DR(2), DR(3) == 0); break;
    case 4: case 5: BtnTop(id, B::SetButtonFlags, (intptr_t)AnyFlags(), DR(2)); break;
    case 6: BtnTop(id, B::CopyAttributes, AnyBtn()); break;
    case 7: BtnTop(id, B::IsMouseOver); break;
    case 8: BtnTop(id, B::IsDefaultButton); break;
    case 9: BtnTop(id, B::MouseEnter, x, y); break;
    case 10: BtnTop(id, B::MouseDown, x, y, DR(3) ? 0 : 1); break;
    case 11: BtnTop(id, B::MouseUp, x, y, DR(3) ? 0 : 2); break;
    case 12: BtnTop(id, B::MouseExit); break;
    case 13: BtnTop(id, B::ButtonDown); break;
    case 14: BtnTop(id, B::ButtonUp, DR(2)); break;
    case 15: case 16: BtnTop(id, B::MouseDrag, x, y); break;
    case 17: BtnTop(id, B::GotFocus); break;
    case 18: BtnTop(id, B::LostFocus); break;
    case 19: case 20: BtnTop(id, B::KeyDown, AnyKey()); break;
    case 21: BtnTop(id, B::KeyUp, DR(2) ? 32 : AnyKey()); break;
    case 22: BtnTop(id, B::TimerEvent, (intptr_t)DR(300)); break;
    case 23: BtnTop(id, B::MouseHover, (intptr_t)DR(1000)); break;
    case 24: BtnTop(id, DR(2) ? B::GetNextRadio : B::GetEndRadio, DR(2)); break;
    default: BtnTop(id, B::GetField, 1 + DR(4)); break;
    }
}

static void RandomCall()
{
    int x = (int)DR(260) - 30, y = (int)DR(260) - 30;
    switch (DR(20)) {
    case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: RandomButtonCall(); break;
    case 8: TopCall(1 + (int)DR(kCt), C::MouseMove, x, y, 0); break;
    case 9: {
        // a press on one of a container's children
        intptr_t ct = 1 + DR(kCt);
        intptr_t n = ee_won_ct_call(sCt[ct], C::GetField, (intptr_t)EeWonCtField::NumChildren, 0, 0);
        if (n > 0) {
            int k = (int)DR((unsigned)n);
            intptr_t ch = ee_won_ct_call(sCt[ct], C::GetField, (intptr_t)EeWonCtField::FrontChild, 0, 0);
            while (k-- > 0) ch = ee_won_ct_call(sCt[ct], C::NextChild, ch, 1, 0);
            x = sW[ch].x + (int)DR((unsigned)sW[ch].w);
            y = sW[ch].y + (int)DR((unsigned)sW[ch].h);
        }
        TopCall((int)ct, C::MouseDown, x, y, DR(4) ? 0 : 1);
        break;
    }
    case 10: TopCall(1 + (int)DR(kCt), C::MouseUp, x, y, DR(3) ? 0 : 2); break;
    case 11: TopCall(1 + (int)DR(kCt), C::MouseDrag, x, y, 0); break;
    case 12: TopCall(1 + (int)DR(kCt), DR(2) ? C::KeyDown : C::KeyUp, AnyKey(), 0, 0); break;
    case 13: TopCall(1 + (int)DR(kCt), C::TimerEvent, (intptr_t)DR(300), 0, 0); break;
    case 14: TopCall(1 + (int)DR(kCt), C::MouseExit, 0, 0, 0); break;
    case 15: TopCall(1 + (int)DR(kCt), C::TabFocus, DR(2), DR(2), 0); break;
    case 16: {
        intptr_t id = AnyBtn();
        if (sW[id].parent != 0) TopCall((int)sW[id].parent, C::SetFocusChild, id, 0, 0);
        break;
    }
    case 17: {
        static const S k[4] = {S::SetDialogDefault, S::UnSetDialogDefault, S::SetDefault, S::UnSetDefault};
        StaticTop(k[DR(4)], DR(4) ? AnyBtn() : 0);
        break;
    }
    case 18: { intptr_t id = AnyBtn(); sSound[id] = !sSound[id]; Line("sound %lld %lld", id, sSound[id]); break; }
    default: TopCall(1 + (int)DR(kCt), C::MouseEnter, x, y, 0); break;
    }
}

static std::vector<std::string> RunSide(bool inRust, unsigned inSeq, int inCalls)
{
    sTrace.clear();
    sCursorRefs.clear();
    sRustSide = inRust;
    sDriverSeed = inSeq * 40503u + 31u;
    sAnswerSeed = inSeq * 2654435761u + 13u;
    InitWorld();
    for (int ct = 1; ct <= kCt; ct++) {
        Line("new %lld", ct);
        sCt[ct] = ee_won_ct_new((void*)(intptr_t)ct, Host, false);
    }
    for (int id = kFirstBtn; id <= kLastBtn; id++) {
        Line("btn-new %lld", id);
        if (inRust) sRust[id] = ee_won_btn_new((void*)(intptr_t)id, Host);
        else orig::sBtn[id] = new orig::Button(id);
        sSound[id] = DR(2) != 0;
        Line("sound %lld %lld", id, sSound[id]);
        // the skins' flags: a radio more often than not
        if (DR(5) != 0) BtnTop(id, B::SetButtonFlags, (intptr_t)(AnyFlags() | (DR(3) ? 0x8 : 0)), 1);
    }
    // the containers under 1 (or 3 under 2), the buttons and the leaf in runs under one container
    TopCall(1, C::AddChild, 2, 0, 0);
    TopCall(DR(2) ? 1 : 2, C::AddChild, 3, 0, 0);
    int p = 1 + (int)DR(kCt);
    for (int id = kFirstBtn; id <= kN; id++) {
        if (DR(4) == 0) p = 1 + (int)DR(kCt);
        if (DR(8) == 0) continue;
        TopCall(p, C::AddChild, id, 0, 0);
    }
    Env(0, 1);
    for (int i = 0; i < inCalls; i++) {
        if (DR(3) == 0) RandomChange();
        RandomCall();
    }
    // the default buttons cleared (every reference given back)
    StaticTop(S::UnSetDialogDefault, StaticCall(S::DialogDefault, 0));
    StaticTop(S::SetDefault, 0);
    for (int ct = kCt; ct >= 1; ct--) {
        Line("free %lld", ct);
        ee_won_ct_free(sCt[ct]);
        sCt[ct] = NULL;
    }
    for (int id = kFirstBtn; id <= kLastBtn; id++) {
        Line("free-btn %lld", id);
        if (inRust) { ee_won_btn_free(sRust[id]); sRust[id] = NULL; }
        else { delete orig::sBtn[id]; orig::sBtn[id] = NULL; }
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
    CHECK(ee_won_gui_event_type(EeWonGuiEvent::ButtonPressed) == WONAPI::ComponentEvent_ButtonPressed);
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
            if (a[i].compare(0, 4, "btn ") == 0) {
                // the call's code
                size_t s1 = a[i].find(' ', 4);
                size_t s2 = a[i].find(' ', s1 + 1);
                std::string key = "btn " + a[i].substr(s1 + 1, s2 - s1 - 1);
                if (aSeen.insert(key).second) aNew = true;
            }
        if (aPrint && aNew) {
            printf("seq %d\n", seq);
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
        }
    }
    if (!aPrint)
        printf("won_btnobj_test: %d sequences, %u trace lines, %u ops and calls met, %d differing, %d checks, "
               "%d failures\n", kSequences, (unsigned)aLines, (unsigned)aSeen.size(), aDiffering, sChecks, sFailures);
    return sFailures;
}
