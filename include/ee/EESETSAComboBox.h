// C:\EEX\Empire Earth\Scenario Editor\EESETSAComboBox.h (RECONSTRUCTION; file name inferred).
// A combo box of the trigger screen's object screen (RTTI EESETSAComboBox > UIComboBox > UIControl > UIEntity; vtable
// 0x4446b4). Out-of-line ctor 0x191840 (ret 0x2c: 11 arguments): builds the rectangle (inLocation.x, inLocation.y,
// x + inWidth - 1, y + 19) and a copy of inStrings, and forwards the rest to UIComboBox's ctor; no members of its own
// stored (sizeof assumed = UIComboBox's 0xe0, not checked at a `new`). ChangeSelection (+0x8c) 0x1918b8:
// UIComboBox::ChangeSelection, then the object screen's UpdateSelection. ??_G 0x1824df (ICF-shared). Argument names
// and the GE2DPoint type inferred. Requires lle/UIComboBox.h, lle/GE2DPoint.h.
#ifndef EE_EESETSACOMBOBOX_H
#define EE_EESETSACOMBOBOX_H

class EESETSAComboBox : public UIComboBox {
public:
    EESETSAComboBox(unsigned long inID, unsigned long inTextBoxType, const std::vector<UWideString>& inStrings, GEFont* inFont,
                    const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inMaxListBoxHeight, UIForm* inForm,
                    unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness);  // 0x191840
protected:
    virtual void ChangeSelection();         // +0x8c 0x1918b8
};

#endif
