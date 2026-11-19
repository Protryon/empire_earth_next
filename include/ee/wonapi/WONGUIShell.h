// WONGUI's generic widget shells (R6 WONLobby round 20): one C++ shell per base class (WONShellComp, WONShellButton,
// WONShellMsButton, WONShellInput), one Rust kind per deleted concrete class (rust/crates/ee_won_gui/src/shell.rs,
// shell_ms.rs: MSButton, MSCheckbox, MSArrowButton, MS3DFrame, ...). A shell's virtuals ask the kind when its mask
// says the kind overrides them, else run the base's; the kind calls the base's back through BaseCall. A deleted
// class's own methods are WONShell::Op(control, op); WONShellNew makes a kind's widget. The host is GuiShell.cpp.
#ifndef EE_WONAPI_WONGUISHELL_H
#define EE_WONAPI_WONGUISHELL_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/shell.h"

namespace WONAPI {
extern "C" intptr_t ee_won_guicpp_shell_call(void*,EeWonShellHostOp,intptr_t,intptr_t,intptr_t,intptr_t);
class WONShell {
public:
    // the shell of c, NULL for a widget that is not one
    static WONShell* Of(Component* c) { return c ? dynamic_cast<WONShell*>(c) : NULL; }
    // the kind's own method op on c (0 for a widget that is not a shell)
    static intptr_t Op(Component* c, EeWonShellOp op, intptr_t a = 0, intptr_t b = 0)
    {
        WONShell* s = Of(c);
        return s ? ee_won_shell_op(s->mShell, op, a, b) : 0;
    }
    static bool IsKind(Component* c, EeWonShellKind k) { WONShell* s = Of(c); return s && ee_won_shell_kind(s->mShell) == k; }
    const char* KindName() const { return ee_won_shell_name(this->mShell); }
    // the base class's virtual v, for the kind
    virtual intptr_t BaseCall(EeWonShellVirt v, intptr_t a, intptr_t b, intptr_t c) = 0;
    virtual ~WONShell() { ee_won_shell_free(this->mShell); }
    Component* mComp;
protected:
    void Make(Component* c, EeWonShellKind k, intptr_t a, intptr_t b, intptr_t d)
    {
        this->mComp = c;
        this->mShell = ee_won_shell_new(k, c, this, ee_won_guicpp_shell_call);
        this->mOver = ee_won_shell_overrides(this->mShell);
        ee_won_shell_init(this->mShell, a, b, d);
    }
    bool Over(EeWonShellVirt v) const { return ((this->mOver >> (uint32_t)v) & 1) != 0; }
    intptr_t Virt(EeWonShellVirt v, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) { return ee_won_shell_virtual(this->mShell, v, a, b, c); }
    EeWonShell* mShell = NULL;
    uint64_t mOver = 0;
};
// a new widget of kind k (its constructor's arguments a, b, c)
Component* WONShellNew(EeWonShellKind k, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0);
// SafeGetComponent of a deleted class: the named component must be a shell of kind k
inline void SafeGetShell(ComponentConfig* cfg, SmartPtr<Component>& out, EeWonShellKind k, const char* name, const char* file, int line)
{
    Component* c = ComponentConfigGet(cfg, name);
    if (!WONShell::IsKind(c, k)) { ComponentConfigMissing(name, file, line); c = NULL; }
    out = c;
}

// the overrides every shell has (S = WSO) and its base calls (S = WSB), from one list
#define WSO_V0(B,N,V) virtual void N() { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V); else B::N(); }
#define WSB_V0(B,N,V) case EeWonShellVirt::V: B::N(); return 0;
#define WSO_V1(B,N,V,T) virtual void N(T a) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,(intptr_t)a); else B::N(a); }
#define WSB_V1(B,N,V,T) case EeWonShellVirt::V: B::N((T)a); return 0;
#define WSO_VB(B,N,V) virtual void N(bool a) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,a); else B::N(a); }
#define WSB_VB(B,N,V) case EeWonShellVirt::V: B::N(a != 0); return 0;
#define WSO_VR(B,N,V,T) virtual void N(T& a) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,(intptr_t)&a); else B::N(a); }
#define WSB_VR(B,N,V,T) case EeWonShellVirt::V: B::N(*(T*)a); return 0;
#define WSO_V2(B,N,V) virtual void N(int a, int b) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,a,b); else B::N(a,b); }
#define WSB_V2(B,N,V) case EeWonShellVirt::V: B::N((int)a,(int)b); return 0;
#define WSO_V3(B,N,V) virtual void N(int a, int b, int c) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,a,b,c); else B::N(a,b,c); }
#define WSB_V3(B,N,V) case EeWonShellVirt::V: B::N((int)a,(int)b,(int)c); return 0;
#define WSO_B1(B,N,V) virtual bool N(int a) { return this->Over(EeWonShellVirt::V) ? this->Virt(EeWonShellVirt::V,a) != 0 : B::N(a); }
#define WSB_B1(B,N,V) case EeWonShellVirt::V: return B::N((int)a);
#define WSO_WH(B,N,V) virtual void N(int& a, int& b) { if (this->Over(EeWonShellVirt::V)) this->Virt(EeWonShellVirt::V,(intptr_t)&a,(intptr_t)&b); else B::N(a,b); }
#define WSB_WH(B,N,V) case EeWonShellVirt::V: B::N(*(int*)a,*(int*)b); return 0;
#define WON_SHELL_VIRTUALS(B,S) \
    S##_V0(B,Virtual08,SizeChanged) S##_WH(B,GetDesiredSize,DesiredSize) S##_VR(B,Paint,Paint,Graphics) \
    S##_V3(B,MouseDown,MouseDown) S##_V3(B,MouseUp,MouseUp) S##_V2(B,MouseMove,MouseMove) \
    S##_V2(B,MouseDrag,MouseDrag) S##_V2(B,MouseEnter,MouseEnter) S##_V0(B,MouseExit,MouseExit) \
    S##_B1(B,MouseWheel,MouseWheel) S##_B1(B,KeyDown,KeyDown) S##_B1(B,KeyChar,KeyChar) S##_B1(B,KeyUp,KeyUp) \
    S##_B1(B,TimerEvent,TimerEvent) S##_V0(B,GotFocus,GotFocus) S##_V0(B,LostFocus,LostFocus) \
    S##_V0(B,AddedToParent,AddedToParent) S##_V1(B,HandleComponentEvent,HandleEvent,ComponentEvent*) \
    S##_VB(B,SetEnabled,SetEnabled) S##_VB(B,SetVisible,SetVisible)
// one shell class over base B, made with the base's arguments BA (of a, b, c): its constructor takes the kind and up
// to three constructor arguments
#define WON_SHELL_CLASS(Name,B,BA) \
class Name : public B, public WONShell { \
public: \
    Name(EeWonShellKind k, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0) : B BA { this->Make(this, k, a, b, c); } \
    WON_SHELL_VIRTUALS(B,WSO) \
    virtual intptr_t BaseCall(EeWonShellVirt v, intptr_t a, intptr_t b, intptr_t c) \
    { (void)b; (void)c; switch (v) { WON_SHELL_VIRTUALS(B,WSB) default: return 0; } } \
};
WON_SHELL_CLASS(WONShellComp,Component,())
WON_SHELL_CLASS(WONShellButton,Button,())
WON_SHELL_CLASS(WONShellMsButton,MSButtonBase,())
WON_SHELL_CLASS(WONShellInput,InputBox,())
WON_SHELL_CLASS(WONShellContainer,Container,())
WON_SHELL_CLASS(WONShellCombo,ComboBox,())
WON_SHELL_CLASS(WONShellBar,Scrollbar,(a != 0))
WON_SHELL_CLASS(WONShellScroller,ScrollbarScroller,())
WON_SHELL_CLASS(WONShellHeader,MultiListHeader,((MultiListArea*)a))
WON_SHELL_CLASS(WONShellTab,TabCtrl,())
}
#endif
