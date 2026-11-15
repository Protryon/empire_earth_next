// C:\EEX\Empire Earth\User Interface Thread\EEUIFileComboBox.h (RECONSTRUCTION; file name inferred). The game's file
// combo box: the files of an FSPath matching one pattern, shown without their 4-character
// extension, after a "none" line (RTTI EEUIFileComboBox > UIComboBox, vtable 0x44a7c4; EEUIFileComboBox.cpp
// 0x2624a6..0x26272b; sizeof 0xf4: the scenario editor news 0xf4 bytes, 0x1707ff). Names are provisional. Requires
// lle/UIComboBox.h, lle/FSPath.h.
#ifndef EE_EEUIFILECOMBOBOX_H
#define EE_EEUIFILECOMBOBOX_H

class EEUIFileComboBox : public UIComboBox {
public:
    // the last argument is unused (the scenario editor passes 0 or 1)
    EEUIFileComboBox(FSPath* inPath, const UString& inPattern, unsigned long inId, unsigned long inTextBoxType,
                     std::vector<UWideString> inData, GEFont* inFont, const GE2DPoint& inLocation,
                     unsigned long inWidth, unsigned long inMaxListBoxHeight, UIForm* inForm, unsigned long inIndex,
                     unsigned long inMaxChars, unsigned long inBorderThickness, bool inUnused);   // 0x2624a6
    virtual ~EEUIFileComboBox();            // +0x00 (??_G 0x262567, dtor 0x262583)
    void CreateEntries();                   // 0x2625ae
    long SelectFile(const UWideString& inName);   // 0x262699 (kErrorNotFound and line 0 when it is not listed)

    FSPath* mPath;                          // +0xe0 referenced
    UString mPattern;                       // +0xe4
};

#endif
