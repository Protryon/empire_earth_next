// C:\EEX\Low-Level Engine\User Interface\UILBString.h (RECONSTRUCTION). A UIListBox<UWideString> that draws its lines
// with a GEText (the combo box's drop-down list, control type 5). The inline members (ctor, dtor, CopyData) and the
// implicit copy members are emitted first by UIComboBox.cpp (0x10060f64..); DrawTextA and UpdateTranslatedKey (the
// original's UILBString.cpp) are Rust's (listobj.rs). Vtable 0x100992cc. sizeof 0x13c (UIComboBox's operator new).
// Requires lle/UIListBox.h, lle/GEText.h, lle/GEFont.h.
#ifndef LLE_UILBSTRING_H
#define LLE_UILBSTRING_H
#include "lle/UIListBox.h"
#include "lle/GEText.h"

class LLE_API UILBString : public UIListBox<UWideString> {
public:
    UILBString(unsigned long inId, const std::vector<UWideString>& inData, GEFont* inFont, bool inAutoSelect,
               const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UIListBox<UWideString>(inId, inData, inFont->GetHeight() + 1, inAutoSelect, inRectangle, inForm, inIndex,
                                 inBorderThickness),
          mText(new GEText(inFont)), mCopiedText(new GEText(inFont)) {}
    virtual ~UILBString()
    {
        delete this->mText;
        delete this->mCopiedText;
    }
    virtual long CopyData()
    {
        *this->mCopiedText = *this->mText;
        return UIListBox<UWideString>::CopyData();
    }
    virtual long DrawTextA() { return ee_ui_string_list_draw(this->mRust); }   // listobj.rs
    GEText* GetGEText() const { return this->mText; }  // ICF-folded (0x1005966a)
    virtual long UpdateTranslatedKey(const char* inKey, short inRepeat) { return ee_ui_string_list_key(this->mRust, inKey); }
    GEText* mText;                          // +0x134
    GEText* mCopiedText;                    // +0x138 drawing
};

#endif
