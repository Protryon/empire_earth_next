// WONGUI's generic widget shells (R6 WONLobby round 20): one C++ shell per base class (WONShellComp, WONShellButton,
// WONShellMsButton, WONShellInput), one Rust kind per deleted concrete class (rust/crates/ee_won_gui/src/shell.rs,
// shell_ms.rs: MSButton, MSCheckbox, MSArrowButton, MS3DFrame, ...). A shell's virtuals ask the kind when its mask
// says the kind overrides them, else run the base's; the kind calls the base's back through BaseCall. A deleted
// class's own methods are WONShell::Op(control, op); WONShellNew makes a kind's widget. The host is GuiShell.cpp.
#ifndef EE_WONAPI_WONGUISHELL_H
#define EE_WONAPI_WONGUISHELL_H
#include "ee/wonapi/WONLobbyGUI.h"
#include "rust/won_gui/wonbtnobj.h"
#include "ee/wonapi/WONComponentFlags.h"
#include "rust/won_gui/shell.h"
#include "rust/won_gui/shell_won.h"
#include "rust/won_gui/skinobj.h"

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
// a new widget of a kind, for the resource table's allocators (the skins' configs' CompFactory statics)
template<EeWonShellKind K, intptr_t A = 0, intptr_t B = 0, intptr_t C = 0> Component* NewShellOf() { return WONShellNew(K, A, B, C); }
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
    S##_V0(B,Virtual08,SizeChanged) S##_WH(B,GetDesiredSize,DesiredSize) S##_VR(B,Paint,Paint,Graphics) WON_SHELL_REST_VIRTUALS(B,S)
#define WON_SHELL_REST_VIRTUALS(B,S) \
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
WON_SHELL_CLASS(WONShellDialog,Dialog,())
// WONButton (R6 WONLobby round 24): the shell kind WonButton over Button (shell_won.rs) and the shell of the
// WONHeaderButton kind. Its own state is the Rust WONButton of round 35 (wonbtnobj.rs: the ten states, the wrapped
// text per font, the pads, colours, flags, alignments and every method), which its five virtuals run when the kind
// does not override them (a kind's base call runs them too); the setters are the configs' and skins' calls on it.
extern "C" intptr_t ee_won_guicpp_wonbtn_call(void* inComp, EeWonGuiOp op, intptr_t a, intptr_t b, intptr_t c, intptr_t d);
class WONButton : public Button, public WONShell {
public:
    // the Rust button (8/4 text pads, centred text, scaled images, grey disabled text, a focus rectangle) wanting
    // focus, then the kind
    explicit WONButton(EeWonShellKind k = EeWonShellKind::WonButton) : mWb(ee_won_wonbtn_new(this, ee_won_guicpp_wonbtn_call))
    {
        this->SetButtonFlags(1, true);
        this->SetComponentFlags(ComponentFlag_WantFocus, true);
        this->Make(this, k, 0, 0, 0);
    }
    virtual ~WONButton() { ee_won_wonbtn_free(this->mWb); }
    virtual void Virtual08() { if (this->Over(EeWonShellVirt::SizeChanged)) this->Virt(EeWonShellVirt::SizeChanged); else this->WCall(EeWonWbCall::SizeChanged); }
    virtual void GetDesiredSize(int& w, int& h)
    { if (this->Over(EeWonShellVirt::DesiredSize)) this->Virt(EeWonShellVirt::DesiredSize, (intptr_t)&w, (intptr_t)&h); else this->WCall(EeWonWbCall::DesiredSize, (intptr_t)&w, (intptr_t)&h); }
    virtual void Paint(Graphics& g) { if (this->Over(EeWonShellVirt::Paint)) this->Virt(EeWonShellVirt::Paint, (intptr_t)&g); else this->WCall(EeWonWbCall::Paint, (intptr_t)&g); }
    WON_SHELL_REST_VIRTUALS(Button,WSO)
    virtual void SetTextHook(const GUIString& t) { this->WCall(EeWonWbCall::SetTextHook, (intptr_t)&t); }
    virtual const GUIString& GetText() // empty while the button has no font
    { const GUIString* s = (const GUIString*)this->WCall(EeWonWbCall::GetText); return s ? *s : gEmptyGUIString; }
    virtual intptr_t BaseCall(EeWonShellVirt v, intptr_t a, intptr_t b, intptr_t c)
    {
        (void)c;
        switch (v) {
            case EeWonShellVirt::SizeChanged: this->WCall(EeWonWbCall::SizeChanged); return 0;
            case EeWonShellVirt::DesiredSize: this->WCall(EeWonWbCall::DesiredSize, a, b); return 0;
            case EeWonShellVirt::Paint: this->WCall(EeWonWbCall::Paint, a); return 0;
            WON_SHELL_REST_VIRTUALS(Button,WSB)
            default: return 0;
        }
    }
    void SetAdjustHeightToFitText(bool a) { this->WCall(EeWonWbCall::SetAdjustHeightToFitText,a); }
    void SetWrapText(bool w) { this->WCall(EeWonWbCall::SetWrapText,w); }
    void SetTransparent(bool t) { this->WCall(EeWonWbCall::SetTransparent,t); }
    void SetButtonState(int s,Image* i,int c,Font* f,bool checked=false) { ee_won_wonbtn_set_state(this->mWb,s,i,c,f,checked); }
    void SetHorzAlign(HorzAlign a) { this->WCall(EeWonWbCall::SetHorzAlign,(intptr_t)a); }
    void SetTextColor(int c) { this->WCall(EeWonWbCall::SetTextColor,c); }
    void SetVertAlign(VertAlign a) { this->WSet(EeWonWbField::VAlign,(unsigned char)a); } // stores the low byte
    void SetScaleImage(bool v) { this->WSet(EeWonWbField::ScaleImage,v); }
    void SetDefaultTextColor(int v) { this->WSet(EeWonWbField::DefaultTextColor,v); }
    void SetDrawGrayText(bool v) { this->WSet(EeWonWbField::DrawGrayText,v); }
    void SetFocusStyle(unsigned char v) { this->WSet(EeWonWbField::FocusStyle,v); }
    void SetTextPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftTextPad,l); this->WSet(EeWonWbField::TopTextPad,t); this->WSet(EeWonWbField::RightTextPad,r); this->WSet(EeWonWbField::BottomTextPad,b); }
    void SetFocusPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftFocusPad,l); this->WSet(EeWonWbField::TopFocusPad,t); this->WSet(EeWonWbField::RightFocusPad,r); this->WSet(EeWonWbField::BottomFocusPad,b); }
    int GetRightTextPad() { return (int)this->WCall(EeWonWbCall::Get,(intptr_t)EeWonWbField::RightTextPad); }
    void SetHorzTextPad(int l,int r) { this->WSet(EeWonWbField::LeftTextPad,l); this->WSet(EeWonWbField::RightTextPad,r); }
    void SetFont(Font* f) { this->WCall(EeWonWbCall::SetFont,(intptr_t)f); }
    void CopyAttributes(WONButton* from) { ee_won_wonbtn_copy(this->mWb,from->mWb,from); } // the text and images
    EeWonWonButton* mWb; // owned
private:
    intptr_t WCall(EeWonWbCall c,intptr_t a=0,intptr_t b=0,intptr_t d=0,intptr_t e=0) { return ee_won_wonbtn_call(this->mWb,c,a,b,d,e); }
    void WSet(EeWonWbField f,int v) { this->WCall(EeWonWbCall::Set,(intptr_t)f,v); }
};
// a frame value as the WON skin's kinds take it (shell_won.rs EeWonFrameIn: the image borrowed)
inline EeWonFrameIn WONFrameArg(const WONFrame& f)
{
    EeWonFrameIn r;
    r.left = f.mLeft; r.top = f.mTop; r.right = f.mRight; r.bottom = f.mBottom;
    r.image = (intptr_t)f.mImage.get();
    return r;
}
// the WON skin's kind's own method with a frame value
inline intptr_t WONShellFrameOp(Component* c, EeWonShellOp op, const WONFrame& f)
{
    EeWonFrameIn a = WONFrameArg(f);
    return WONShell::Op(c, op, (intptr_t)&a);
}
// SafeConfigGetComponent of a WON skin class by its name: the component must be a shell of its kind (or a derived
// one: a scroll thumb is a simple button, a list control a scroll container)
inline Component* SafeConfigGetSkinPart(Component* c, const char* name)
{
    static const struct { const char* n; EeWonShellKind k1, k2; } kKinds[] = {
        {"WONSimpleButton", EeWonShellKind::WonSimpleButton, EeWonShellKind::WonScrollThumb},
        {"WONHeaderButton", EeWonShellKind::WonHeaderButton, EeWonShellKind::WonHeaderButton},
        {"WONScrollbar", EeWonShellKind::WonScrollbar, EeWonShellKind::WonScrollbar},
        {"WONScrollContainer", EeWonShellKind::WonScrollContainer, EeWonShellKind::WonListCtrl},
        {"WONListCtrl", EeWonShellKind::WonListCtrl, EeWonShellKind::WonListCtrl},
        {"WONTabCtrl", EeWonShellKind::WonTabCtrl, EeWonShellKind::WonTabCtrl},
        {"WONComboBox", EeWonShellKind::WonComboBox, EeWonShellKind::WonComboBox},
        {"WONPopupBack", EeWonShellKind::WonFrameComponent, EeWonShellKind::WonFrameComponent},
        {"WONDialogBack", EeWonShellKind::WonFrameComponent, EeWonShellKind::WonFrameComponent},
    };
    for (size_t i = 0; i < sizeof kKinds / sizeof kKinds[0]; i++)
        if (strcmp(kKinds[i].n, name) == 0 && (WONShell::IsKind(c, kKinds[i].k1) || WONShell::IsKind(c, kKinds[i].k2)))
            return c;
    throw ConfigObjectException(std::string("Invalid ") + name);
}
// the game's skin (the original's gSkin; made by the first WONSkin group's PreParse)
inline EeWonSkin* WONSkinGlobal() { return ee_won_skin_global(); }
// a template slot of the game's skin (skinobj.rs EeWonSkinSlot; i the index into its array) = c
inline void WONSkinSet(EeWonSkinSlot s, int i, Component* c) { ee_won_skin_set(WONSkinGlobal(), s, i, c); }
// MSMessageBox::Msg (R6 WONLobby round 22: the box is the kind MsMessageBox, shell_msgbox.rs): a modal message box over
// the parent window; the id (1-3) of the button pressed
int MSMessageBoxMsg(Window* inParent, const GUIString& inTitle, const GUIString& inMessage, const GUIString& inButton1,
                    const GUIString& inButton2, const GUIString& inButton3);
}
#endif
