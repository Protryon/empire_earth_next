// C:\EEX\Low-Level Engine\User Interface\UIBackground.h (RECONSTRUCTION). Abstract base of a form's background
// pieces. Vtable 0x10098a4c: Close, Open, RelayoutBackground, then three pure slots named from
// UIBFull/UIBitmapBackground. The original's sizeof was 0x44 (??_E stride). Requires lle/UILayoutUtility.h.
//
// The backgrounds are whole in Rust (rust/crates/ee_ui/src/bgobj.rs, R6 ui round 20): the members of every
// background class are EeUiBackgroundFields, this class's protected base (the field names the members'; the game's
// subclasses read mRectangle in place); every method forwards to Rust (UIBackground.cpp), which calls out through
// UIBackgroundHost::Op. The virtuals stay virtual: the game overrides DrawBackground, Open and LoadBackground.
#ifndef LLE_UIBACKGROUND_H
#define LLE_UIBACKGROUND_H

#include "rust/ui/bgobj.h"
#include "lle/UILayoutUtility.h"

class UIForm;

// winspool.h maps GetForm to GetFormA; the export is ?GetForm@UIBackground@@
#ifdef GetForm
#undef GetForm
#endif

class LLE_API UIBackground : protected EeUiBackgroundFields {
    friend struct UIBackgroundHost;         // UIBackground.cpp: the Rust object's calls out
public:
    UIBackground(unsigned long inBackgroundID, const UILayoutUtility& inLayout, UIForm* inForm,
                 unsigned long inDrawPriority, bool inVisible);
    UIBackground(const UIBackground&) = delete;
    UIBackground& operator=(const UIBackground&) = delete;
    virtual ~UIBackground();
    virtual void Close();                   // slot 0x04
    virtual void Open();
    virtual void RelayoutBackground();
    virtual long LoadBackground() = 0;      // slot 0x10
    virtual long DrawBackground(const GERectangle& inRectangle) = 0;
    virtual long UnloadBackground() = 0;    // slot 0x18
    unsigned long GetBackgroundID() const { return this->mBackgroundID; }
    unsigned long GetDrawPriority() const { return this->mDrawPriority; }
    UIForm* GetForm() { return static_cast<UIForm*>(this->mForm); }
    bool GetIsVisible() const { return this->mIsVisible; }
    const GERectangle& GetRectangle() const { return *reinterpret_cast<const GERectangle*>(&this->mRectangle); }
    void SetRectangle(const GERectangle& inRectangle);
    void SetVisibility(bool inVisible);
protected:
    intptr_t Call(EeUiBgCall inCall, intptr_t a = 0, intptr_t b = 0, float* f = NULL)
        { return ee_ui_bg_call(this, inCall, a, b, f); }
    intptr_t CallRect(EeUiBgCall inCall, const GERectangle& inRectangle);
};

#endif
