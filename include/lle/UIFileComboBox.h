// C:\EEX\Low-Level Engine\User Interface\UIFileComboBox.h (RECONSTRUCTION). A combo box listing the files of an FSPath
// whose extension is one of the valid ones (full names). Vtable 0x100994d0 (UIComboBox's, no new virtuals). The
// original's sizeof was 0xf4.
//
// Whole in Rust (rust/crates/ee_ui/src/filelistobj.rs, R6 ui round 21): the valid extensions and the path are the
// Rust object mRust; every method forwards there (UIFileListBox.cpp holds the forwarders and the host). Requires
// lle/UIComboBox.h.
#ifndef LLE_UIFILECOMBOBOX_H
#define LLE_UIFILECOMBOBOX_H

#include <vector>
#include "lle/UIComboBox.h"
#include "rust/ui/filelistobj.h"
#include "rust/ui/lle_hosts.h"


class LLE_API UIFileComboBox : public UIComboBox {
public:
    UIFileComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData, GEFont* inFont,
                   const GE2DPoint& inLocation, float inWidth, unsigned long inMaxListBoxHeight, UIForm* inForm,
                   unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness)
        : UIComboBox(inId, inTextBoxType, std::vector<UWideString>(), inFont,
                 GERectangle(inLocation, GE2DPoint(inLocation.GetX() + inWidth, inLocation.GetY() + kScrollBarWidth)),
                 inMaxListBoxHeight, inForm, inIndex, inMaxChars, inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, ee_ui_file_combo_host, true);
}
    virtual ~UIFileComboBox() { ee_ui_file_list_free(this->mRust); }
    void AddValidExtension(const UString& inExtension) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::AddExtension, (intptr_t)(const char*)inExtension); }
    void CreateEntries(unsigned long inSortType) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::CreateEntries, (intptr_t)inSortType); }
    void SetSearchPaths(FSPath* inPath) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::SetSearchPaths, (intptr_t)inPath); }
    EeUiFileList* mRust;                    // the valid extensions (".xxx") and the path (referenced)
};

#endif
