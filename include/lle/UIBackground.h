// C:\EEX\Low-Level Engine\User Interface\UIBackground.h (RECONSTRUCTION). Abstract base of a form's background
// pieces. UIBackground.cpp holds the ctor and the out-of-line members (0x1005a76e..0x1005aa1b); the inline members
// are emitted first by UIBackground.cpp's inline block (0x10058d9b..0x10058e86). Vtable 0x10098a4c: Close, Open,
// RelayoutBackground, then three pure slots named from UIBFull/UIBitmapBackground. sizeof 0x44 (??_E stride).
// Requires lle/UILayoutUtility.h.
#ifndef LLE_UIBACKGROUND_H
#define LLE_UIBACKGROUND_H

#include "lle/UILayoutUtility.h"

class UIForm;

// winspool.h maps GetForm to GetFormA; the export is ?GetForm@UIBackground@@
#ifdef GetForm
#undef GetForm
#endif

class LLE_API UIBackground {
    friend struct UIBackgroundRust;         // UIBackground.cpp: the Rust moves' view (ee_ui background.rs)
public:
    UIBackground(unsigned long inBackgroundID, const UILayoutUtility& inLayout, UIForm* inForm,
                 unsigned long inDrawPriority, bool inVisible);
    virtual ~UIBackground();
    virtual void Close();                   // slot 0x04
    virtual void Open();
    virtual void RelayoutBackground();
    virtual long LoadBackground() = 0;      // slot 0x10
    virtual long DrawBackground(const GERectangle& inRectangle) = 0;
    virtual long UnloadBackground() = 0;    // slot 0x18
    unsigned long GetBackgroundID() const { return this->mBackgroundID; }
    unsigned long GetDrawPriority() const { return this->mDrawPriority; }
    UIForm* GetForm() { return this->mForm; }
    bool GetIsVisible() const { return this->mIsVisible; }
    const GERectangle& GetRectangle() const { return this->mRectangle; }
    void SetRectangle(const GERectangle& inRectangle);
    void SetVisibility(bool inVisible);
protected:
    UILayoutUtility mLayout;                // +0x04
    GERectangle mRectangle;                 // +0x24
    unsigned long mBackgroundID;            // +0x34
    unsigned long mDrawPriority;            // +0x38
    UIForm* mForm;                          // +0x3c
    bool mIsVisible;                        // +0x40
};

#endif
