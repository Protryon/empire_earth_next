// C:\EEX\Low-Level Engine\User Interface\UIFileListBox.h (RECONSTRUCTION). A string list box listing the files of an
// FSPath whose extension is one of mValidExtensions (stored without the extension). UIFileListBox.cpp
// (0x1006fae7..0x1006ff58) holds the ctor and the out-of-line members; the inline members (GetFilePath,
// AddValidExtension 0x100669b6) and the implicit copy members are emitted first by UIControlsTable.cpp. Vtable
// 0x10099574 (UILBString's, no new virtuals). sizeof 0x150 (copy ctor 0x100669c8). Field names are provisional.
// Requires lle/UILBString.h, lle/FSPath.h.
#ifndef LLE_UIFILELISTBOX_H
#define LLE_UIFILELISTBOX_H

#include <vector>
#include "lle/UILBString.h"

class FSPath;

class LLE_API UIFileListBox : public UILBString {
public:
    UIFileListBox(unsigned long inId, GEFont* inFont, bool inAutoSelect, const GERectangle& inRectangle,
                  UIForm* inForm, unsigned long inIndex, unsigned long inBorderThickness);
    virtual ~UIFileListBox();
    void AddValidExtension(const UString& inExtension) { this->mValidExtensions.push_back(inExtension); }  // 0x100669b6
    void CreateEntries(unsigned long inSortType);
    long DeleteSelectedFile();
    FSPath* GetFilePath() const { return this->mFilePath; }  // ICF-folded (0x1005968d)
    void SetSearchPaths(FSPath* inPath);
private:
    std::vector<UString> mValidExtensions;  // +0x13c ".xxx" (4 characters, compared with the name's last 4)
    FSPath* mFilePath;                      // +0x14c referenced
};

#endif
