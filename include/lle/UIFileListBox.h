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

class FSPath;

class LLE_API UIFileListBox : public UILBString {
    friend struct UIFileListHost;           // UIFileListBox.cpp: the Rust object's calls out
public:
    UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    UIFileListBox(const UIFileListBox&) = delete;
    UIFileListBox& operator=(const UIFileListBox&) = delete;
    virtual ~UIFileListBox();
    void AddValidExtension(const UString& inExtension);
    void CreateEntries(unsigned long inSortType);
    long DeleteSelectedFile();
    FSPath* GetFilePath() const;
    void SetSearchPaths(FSPath* inPath);
private:
    EeUiFileList* mRust;                    // the valid extensions (".xxx") and the path (referenced)
};

#endif
