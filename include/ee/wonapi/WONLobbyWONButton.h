#ifndef EE_WONAPI_WONLOBBYWONBUTTON_H
#define EE_WONAPI_WONLOBBYWONBUTTON_H
#include "ee/wonapi/WONLobbyGUI.h"
namespace WONAPI {
// WONButton (0x100 bytes), vtable 0x008506d4; the skin code allocates it with its constructor.
class WONButton : public Button {
public:
    struct ButtonState {
        SmartPtr<WrappedText> mText;
        SmartPtr<Image> mImage;
        int mColor; //08
        ButtonState() : mColor(-1) {} // default construction (0x00717dd0) before the map
    };
    typedef std::map<Font*,SmartPtr<WrappedText> > FontTextMap;
    WONButton(); // at 0x00717c70 (0x100 bytes)
    virtual void Virtual08(); // SizeChanged (slot 0x08), at 0x00718100
    virtual void GetDesiredSize(int&,int&); // GetDesiredSize (slot 0x0c), at 0x00718640
    virtual void Paint(Graphics&); // slot 0x18, at 0x00719080
    virtual void SetTextHook(const GUIString&); // SetTextHook (slot 0xb0), at 0x00718060
    virtual const GUIString& GetText(); // getter (+0xb8), at 0x007180e0
    virtual void GetTextSize(int&,int&); // first new slot (0xbc), at 0x00718580
    void AdjustHeightToFitText(); //3181a0
    void SetAdjustHeightToFitText(bool); // at 0x00718200
    void SetWrapText(bool); //318220
    void SetTransparent(bool); // at 0x007182a0
    void SetButtonState(int,Image*,int,Font*,bool=false); //3182c0
    void SetHorzAlign(HorzAlign); //3186e0
    void SetTextColor(int); //318790
    void SetVertAlign(VertAlign); // at 0x00718780 (stores the low byte)
    void SetScaleImage(bool v) { this->mScaleImage=v; }
    void SetHasCheck(bool v) { this->mHasCheck=v; } // inline (used at 0x007160e9 and elsewhere)
    void SetPushOffsets(int x,int y) { this->mPushDX=x; this->mPushDY=y; } // inline
    void SetDefaultTextColor(int v) { this->mDefaultTextColor=v; }
    void SetDrawGrayText(bool v) { this->mDrawGrayText=v; }
    void SetFocusStyle(unsigned char v) { this->mFocusStyle=v; }
    void SetTextPadding(int l,int t,int r,int b) { this->mLeftTextPad=l; this->mTopTextPad=t; this->mRightTextPad=r; this->mBottomTextPad=b; }
    void SetFocusPadding(int l,int t,int r,int b) { this->mLeftFocusPad=l; this->mTopFocusPad=t; this->mRightFocusPad=r; this->mBottomFocusPad=b; }
    int GetRightTextPad() { return this->mRightTextPad; }
    void SetHorzTextPad(int l,int r) { this->mLeftTextPad=l; this->mRightTextPad=r; }
    void SetFont(Font*); //3187e0
    WONButton* Duplicate(); // at 0x00718d40
    void CopyAttributes(WONButton*); // copies text and images, at 0x00718970
    void DrawButtonText(Graphics&,WrappedText*,int); //318db0
    void GetButtonState(Graphics&,int*,WrappedText** =NULL,Image** =NULL); //318f90
    ButtonState mStates[5][2]; // offsets 0x44..0xbb: ten 0x0c-byte records
    FontTextMap mFontTextMap; // offsets 0xbc..0xc7 (VC6 map, 0x0c bytes)
    bool mAdjustHeightToFitText; //c8; natural c9..cb padding
    int mLeftTextPad,mTopTextPad,mRightTextPad,mBottomTextPad; //cc..db
    int mLeftFocusPad,mTopFocusPad,mRightFocusPad,mBottomFocusPad; //dc..eb
    int mPushDX,mPushDY,mDefaultTextColor; //ec,f0,f4
    bool mHasCheck,mScaleImage,mWrapText,mDrawGrayText,mIsTransparent; //f8..fc
    unsigned char mHAlign,mVAlign,mFocusStyle; //fd,fe,ff
};
}
#endif
