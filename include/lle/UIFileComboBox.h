// C:\EEX\Low-Level Engine\User Interface\UIFileComboBox.h (RECONSTRUCTION). A combo box listing the files of an FSPath
// whose extension is one of mValidExtensions (stored without the extension). UIFileComboBox.cpp
// (0x1006f4ef..0x1006fae7) holds the ctor and the out-of-line members; the inline members (GetFilePath,
// AddValidExtension 0x100668d7) and the implicit copy members are emitted first by UIControlsTable.cpp. Vtable
// 0x100994d0 (UIComboBox's, no new virtuals). sizeof 0xf4 (copy ctor 0x100668e9). Field names are provisional.
// Requires lle/UIComboBox.h, lle/FSPath.h.
#ifndef LLE_UIFILECOMBOBOX_H
#define LLE_UIFILECOMBOBOX_H

#include <vector>
#include "lle/UIComboBox.h"

class FSPath;

class LLE_API UIFileComboBox : public UIComboBox {
public:
    UIFileComboBox(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString> inData, GEFont* inFont,
                   const GE2DPoint& inLocation, float inWidth, unsigned long inMaxListBoxHeight, UIForm* inForm,
                   unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness);
    virtual ~UIFileComboBox();
    void AddValidExtension(const UString& inExtension) { this->mValidExtensions.push_back(inExtension); }  // 0x100668d7
    void CreateEntries(unsigned long inSortType);
    long DeleteSelectedFile();
    FSPath* GetFilePath() const { return this->mFilePath; }  // ICF-folded (0x1006608d)
    void SetSearchPaths(FSPath* inPath);
private:
    std::vector<UString> mValidExtensions;  // +0x0e0 ".xxx" (4 characters, compared with the name's last 4)
    FSPath* mFilePath;                      // +0x0f0 referenced
};

#endif
