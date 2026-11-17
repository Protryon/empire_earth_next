#ifndef EE_WONAPI_WONLOBBYWONBUTTON_H
#define EE_WONAPI_WONLOBBYWONBUTTON_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// WONButton (R6 WON round 35): its ten states (a wrapped text, an image and a colour each), the wrapped text per font,
// the pads, the push, the colours, the flags, the alignments and every method are the Rust WONButton
// (rust/crates/ee_won_gui/src/wonbtnobj.rs); the C++ is the type the skins hold, its forwarders and the host
// (WONButton.cpp). The skin code allocates it with its constructor.
class WONButton : public Button {
public:
    WONButton(); // at 0x00717c70 (0x100 bytes)
    virtual ~WONButton();
    virtual void Virtual08() { this->WCall(EeWonWbCall::SizeChanged); } // SizeChanged (slot 0x08), at 0x00718100
    virtual void GetDesiredSize(int& w,int& h) { this->WCall(EeWonWbCall::DesiredSize,(intptr_t)&w,(intptr_t)&h); }
    virtual void Paint(Graphics& g) { this->WCall(EeWonWbCall::Paint,(intptr_t)&g); } // slot 0x18, at 0x00719080
    virtual void SetTextHook(const GUIString& t) { this->WCall(EeWonWbCall::SetTextHook,(intptr_t)&t); }
    virtual const GUIString& GetText(); // getter (+0xb8), at 0x007180e0
    virtual void GetTextSize(int& w,int& h) { this->WCall(EeWonWbCall::GetTextSize,(intptr_t)&w,(intptr_t)&h); }
    void AdjustHeightToFitText() { this->WCall(EeWonWbCall::AdjustHeightToFitText); } //3181a0
    void SetAdjustHeightToFitText(bool a) { this->WCall(EeWonWbCall::SetAdjustHeightToFitText,a); } // at 0x00718200
    void SetWrapText(bool w) { this->WCall(EeWonWbCall::SetWrapText,w); } //318220
    void SetTransparent(bool t) { this->WCall(EeWonWbCall::SetTransparent,t); } // at 0x007182a0
    void SetButtonState(int s,Image* i,int c,Font* f,bool checked=false) { ee_won_wonbtn_set_state(this->mWb,s,i,c,f,checked); } //3182c0
    void SetHorzAlign(HorzAlign a) { this->WCall(EeWonWbCall::SetHorzAlign,(intptr_t)a); } //3186e0
    void SetTextColor(int c) { this->WCall(EeWonWbCall::SetTextColor,c); } //318790
    void SetVertAlign(VertAlign a) { this->WSet(EeWonWbField::VAlign,(unsigned char)a); } // at 0x00718780 (stores the low byte)
    void SetScaleImage(bool v) { this->WSet(EeWonWbField::ScaleImage,v); }
    void SetHasCheck(bool v) { this->WSet(EeWonWbField::HasCheck,v); } // inline (used at 0x007160e9 and elsewhere)
    void SetPushOffsets(int x,int y) { this->WSet(EeWonWbField::PushDX,x); this->WSet(EeWonWbField::PushDY,y); } // inline
    void SetDefaultTextColor(int v) { this->WSet(EeWonWbField::DefaultTextColor,v); }
    void SetDrawGrayText(bool v) { this->WSet(EeWonWbField::DrawGrayText,v); }
    void SetFocusStyle(unsigned char v) { this->WSet(EeWonWbField::FocusStyle,v); }
    void SetTextPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftTextPad,l); this->WSet(EeWonWbField::TopTextPad,t); this->WSet(EeWonWbField::RightTextPad,r); this->WSet(EeWonWbField::BottomTextPad,b); }
    void SetFocusPadding(int l,int t,int r,int b) { this->WSet(EeWonWbField::LeftFocusPad,l); this->WSet(EeWonWbField::TopFocusPad,t); this->WSet(EeWonWbField::RightFocusPad,r); this->WSet(EeWonWbField::BottomFocusPad,b); }
    int GetRightTextPad() { return (int)this->WCall(EeWonWbCall::Get,(intptr_t)EeWonWbField::RightTextPad); }
    int GetLeftTextPad() { return (int)this->WCall(EeWonWbCall::Get,(intptr_t)EeWonWbField::LeftTextPad); }
    void SetHorzTextPad(int l,int r) { this->WSet(EeWonWbField::LeftTextPad,l); this->WSet(EeWonWbField::RightTextPad,r); }
    void SetFont(Font* f) { this->WCall(EeWonWbCall::SetFont,(intptr_t)f); } //3187e0
    WONButton* Duplicate(); // at 0x00718d40
    void CopyAttributes(WONButton* from) { ee_won_wonbtn_copy(this->mWb,from->mWb,from); } // copies text and images, at 0x00718970
    void DrawButtonText(Graphics& g,WrappedText* t,int c) { this->WCall(EeWonWbCall::DrawButtonText,(intptr_t)&g,(intptr_t)t,c); } //318db0
    void GetButtonState(Graphics& g,int* c,WrappedText** t=NULL,Image** i=NULL) { this->WCall(EeWonWbCall::GetButtonState,(intptr_t)&g,(intptr_t)c,(intptr_t)t,(intptr_t)i); } //318f90
    EeWonWonButton* mWb;
private:
    intptr_t WCall(EeWonWbCall c,intptr_t a=0,intptr_t b=0,intptr_t d=0,intptr_t e=0) { return ee_won_wonbtn_call(this->mWb,c,a,b,d,e); }
    void WSet(EeWonWbField f,int v) { this->WCall(EeWonWbCall::Set,(intptr_t)f,v); }
};
}
#endif
