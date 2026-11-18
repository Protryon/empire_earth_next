// C:\EEX\Low-Level Engine\User Interface\UIButton.h (RECONSTRUCTION). A three-state bitmap button with an optional
// text label. UIButton.cpp (0x1005e47c..0x1005ef4b) holds the ctor and the out-of-line members; the inline members
// are emitted first by UIBlinkingButton.cpp (0x1005db0f..0x1005df23). Vtable 0x10098cac (37 slots: UIControl's
// 34, then ButtonDown, ButtonUp, MouseOver). sizeof 0x104 (padded from the last bool at +0x102).
//
// The class is whole in Rust (rust/crates/ee_ui/src/buttonobj.rs, R6 ui round 11): its state (the text rectangle,
// the bitmaps, the state, the texts, the sounds, the presses) and every method are the Rust object mRust; UIButton.cpp
// holds the forwarders, the ctor and dtor and the host (the op switch). The virtuals stay virtual: the toolkit's and
// the game's subclasses override them. Requires lle/UIControl.h, lle/GEBitmap.h.
#ifndef LLE_UIBUTTON_H
#define LLE_UIBUTTON_H

#include "lle/UIControl.h"
#include "lle/UIButtonState.h"

class GEBitmap;
class GEText;
struct EeUiButton;

class LLE_API UIButton : public UIControl {
    friend struct UIButtonRust;             // the Rust subclasses' logic (User Interface/UIButtonRust.h)
    friend struct UIButtonHost;             // UIButton.cpp: the Rust object's calls out
public:
    UIButton(unsigned long inId, GEBitmap* inNormalBitmap, GEBitmap* inFocusedBitmap, GEBitmap* inPressedBitmap,
             const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    UIButton(const UIButton&) = delete;
    UIButton& operator=(const UIButton&) = delete;
    virtual ~UIButton();
    virtual void ChangeFont(GEFont* inFont);
    virtual long CopyData();
    virtual long DrawPolygons();
    virtual long DrawTextA();
    GE2DPoint GetBitmapLocation() const;
    unsigned long GetState() const;         // UIButtonState
    GEBitmap* GetFocusedBitmap() const;
    GEText* GetGEText();
    GEBitmap* GetNormalBitmap() const;
    GEBitmap* GetPressedBitmap() const;
    void SetBitmapDrawOffset(const GE2DPoint& inOffset);
    void SetButtonDownSound(const UString& inName);
    void SetButtonUpSound(const UString& inName);
    void SetFocusedBitmap(GEBitmap* inBitmap);
    void SetFocusedTextColor(const GEColor& inColor);
    virtual void SetLocation(const GE2DPoint& inLocation);
    void SetNormalBitmap(GEBitmap* inBitmap);
    void SetPressedBitmap(GEBitmap* inBitmap);
    void SetText(const UWideString& inText, GEFont* inFont);
    void SetTextRectangle(const GERectangle& inRectangle);
    virtual long UpdateKeyboard();
protected:
    virtual void ButtonDown();              // slot 0x88
    virtual void ButtonUp();                // slot 0x8c
    virtual long MouseLButtonDown();
    virtual long MouseLButtonUp();
    virtual long MouseMove(const GE2DPoint& inLocation);
    virtual void MouseOver();               // slot 0x90
    virtual long MouseRButtonDown() { return kErrorOK; }
    virtual long MouseRButtonUp() { return kErrorOK; }
    virtual void TabbedTo();
    virtual void TabbedFrom();

    EeUiButton* mRust;                      // the state (buttonobj.rs)
};

#endif
