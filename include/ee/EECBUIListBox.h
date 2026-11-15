// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.h (RECONSTRUCTION; file name inferred; provisional member
// names). The civilization builder's list box (RTTI EECBUIListBox > UILBString > UIListBox<UWideString> > UIControl >
// UIEntity). EECBUIListBox.cpp 0x27cfdb..0x27efca. Its logic is Rust's (ee_screens civ_list.rs: the lines'
// tokens, order, categories, selection, formatting and drawing); the class keeps the toolkit's base and its members,
// its overrides forward, and HostOp is the list's half of the builder's host (EE_SCREENS_CBW_L_*). Requires
// lle/UILBString.h.
#ifndef EE_EECBUILISTBOX_H
#define EE_EECBUILISTBOX_H
#include <stdint.h>

#include "ee/EECivUpgrade.h"

struct EeScreensCbOut;

class EECBUIListBox : public UILBString {
public:
    // the two class statics (a line's separator; a category header's last token)
    static const UWideString sSeparator;    // "/"
    static const UWideString sCategory;     // "Category"

    // 0x27d187 (UIControlsTable's user-defined control factory passes itself first)
    EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData, GEFont* inFont,
                  const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                  unsigned long inBorderThickness, bool inIsTabable);
    virtual ~EECBUIListBox();

    // the overrides (civ_list.rs)
    virtual void AppendStringArray(std::vector<UWideString>& inLines);
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo);
    virtual long CopyData();
    virtual void DeleteLine(unsigned long inLine);
    virtual long DrawTextA();
    virtual bool GetTabable() const { return this->mIsTabable; }
    virtual void TabbedTo();
    virtual void AddLine(const UWideString& inLine);
    // the civilization's own upgrades at their refunds (the predefined civilizations' form's statistics)
    virtual void SetCivilizationUpgrades(std::vector<EECivUpgrade>& inUpgrades);
    // line 0 selected without an event
    virtual void ResetSelection() { UIListBox<UWideString>::ChangeSelectionIndex(0, false, false); }

    void SetSortLines(bool inSortLines) { this->mSortLines = inSortLines; }

    // the list's host ops (EE_SCREENS_CBW_L_*) on the list p
    static int64_t HostOp(uint32_t inOp, EECBUIListBox* p, int64_t inA, int64_t inB, int64_t inC,
                          const UWideString& inText, EeScreensCbOut* outText);

    GEColor mC13C;                          // the header colour (kBlack; the choice form's: (0, 0, 0.39), (0.18, 0.18, 0.28))
    GEColor mC148;                          // the header colour as of the last CopyData
    bool mSelectUpgradesOnly;               // upgrade lines only selectable (true)
    bool mCopiedShowPoints;                 // the points shown as of the last CopyData
    bool mShowPoints;                       // the points shown (true)
    bool mSortLines;                        // the lines sorted (true)
    bool mIsTabable;                        // the ctor's last argument
};

#endif
