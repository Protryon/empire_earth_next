// C:\EEX\Low-Level Engine\User Interface\UIFileListBox.h (RECONSTRUCTION). A string list box listing the files of an
// FSPath whose extension is one of the valid ones (listed without the extension). Vtable 0x10099574 (UILBString's, no
// new virtuals). The original's sizeof was 0x150.
//
// Whole in Rust (rust/crates/ee_ui/src/filelistobj.rs, R6 ui round 21): the valid extensions and the path are the
// Rust object mRust; every method forwards there (UIFileListBox.cpp, with UIFileComboBox's). Requires
// lle/UILBString.h.
#ifndef LLE_UIFILELISTBOX_H
#define LLE_UIFILELISTBOX_H

#include "lle/UILBString.h"
#include "rust/ui/filelistobj.h"
#include "rust/ui/lle_hosts.h"


class LLE_API UIFileListBox : public UILBString {
public:
    UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness)
        : UILBString(inId, std::vector<UWideString>(), inFont, inAutoSelect, inRectangle, inForm, inIndex,
                 inBorderThickness)
{
    this->mRust = ee_ui_file_list_new(this, ee_ui_file_list_host, false);
}
    virtual ~UIFileListBox() { ee_ui_file_list_free(this->mRust); }
    void AddValidExtension(const UString& inExtension) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::AddExtension, (intptr_t)(const char*)inExtension); }
    void CreateEntries(unsigned long inSortType) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::CreateEntries, (intptr_t)inSortType); }
    long DeleteSelectedFile() { return (long)ee_ui_file_list_call(this->mRust, EeUiFileListCall::DeleteSelected, 0); }
    void SetSearchPaths(FSPath* inPath) { ee_ui_file_list_call(this->mRust, EeUiFileListCall::SetSearchPaths, (intptr_t)inPath); }
    EeUiFileList* mRust;                    // the valid extensions (".xxx") and the path (referenced)
};

#endif
