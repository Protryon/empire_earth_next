// C:\EEX\Empire Earth\User Interface Thread\EEUIFMGStatusBar.h (RECONSTRUCTION; file name inferred).
// An improvement button drawn as an icon, a status bar and two value labels (RTTI EEUIFMGStatusBar > EEUIBUpgrade;
// vtable 0x44acc4, no new slots; EEUIFMGStatusBar.cpp 0x2691b2..0x269ca8). Members past EEUIBUpgrade (sizeof 0x130)
// from the ctor 0x2691b2 (ESVERIFY names mBar, mValue) and the dtor 0x269808; other names are provisional. Requires
// ee/EEUIBUpgrade.h.
#ifndef EE_EEUIFMGSTATUSBAR_H
#define EE_EEUIFMGSTATUSBAR_H

class GEBitmap;
class GEText;
class UIBitmap;
class UIStatusBar;
class UITextLabel;
class UILayoutUtility;

class EEUIFMGStatusBar : public EEUIBUpgrade {
    friend class EEUIFMGStatusBox;           // actual external SetIconBitmap caller; access spelling provisional
public:
    // 0x2691b2: args3..5 unread; arg3 actual layout address, pointer-vs-reference spelling inferred
    EEUIFMGStatusBar(unsigned long inId, unsigned long inIndex, UILayoutUtility* inLayout, unsigned long inScreenWidth,
                     unsigned long inScreenHeight, const GERectangle& inRectangle, UIForm* inForm, bool inScenarioEditor,
                     bool inEnabled, bool inArg10);
    virtual ~EEUIFMGStatusBar();            // 0x269808 (??_G 0x2697cf)
    virtual long CopyData();                // +0x14 0x269896
    virtual long DrawPolygons();            // +0x18 0x26993f
    virtual long DrawTextA();               // +0x1c 0x26997f
protected:
    virtual long MouseLButtonDown();        // +0x34 0x269a40
    virtual long MouseLButtonUp();          // +0x38 0x269a9c
    virtual long MouseMove(const GE2DPoint& inLocation);   // +0x40 0x269afa
    virtual long MouseRButtonDown();        // +0x44 0x269b8b
    virtual long MouseRButtonUp();          // +0x48 0x269be6
    void MoveIcon(const GE2DPoint& inOffset);   // 0x269c43 (the icon follows the pressed state when it can upgrade)
    // 0x26c42a (EEUIFMGStatusBox.cpp's range, after its first user; provisional name): mIcon shows inBitmap
    void SetIconBitmap(GEBitmap* inBitmap, const GERectangle& inTextureRectangle);
public:
    // inline: the value labels' first lines (out of line after their first user, EEUIFMGStatusBox::Fn26CBE1:
    // 0x26dac9 / 0x26dad8, in this order) (provisional names)
    const UWideString& GetValueText() const { return this->mValue->GetText(0); }
    const UWideString& GetValue2Text() const { return this->mValue2->GetText(0); }
    UIBitmap* mIcon;                        // +0x130
    UITextLabel* mValue;                    // +0x134 ("this->mValue")
    UITextLabel* mValue2;                   // +0x138
    UIStatusBar* mBar;                      // +0x13c ("this->mBar")
    GEBitmap* mBitmap140;                   // +0x140 (dereferenced by the dtor)
    GEBitmap* mBitmap144;                   // +0x144
    GEText* mCount;                         // +0x148 mL128 as text (CopyData)
    bool mPressed;                          // +0x14c the icon is moved by (1, 1)
    bool mShowCount;                        // +0x14d
    bool mCopiedEnabled;                    // +0x14e mEnabled as of the last CopyData (DrawPolygons)
    bool mEnabled;                          // +0x14f
};

#endif
