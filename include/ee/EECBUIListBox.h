// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.h (RECONSTRUCTION; file name inferred; provisional member
// names). The civilization builder's list box (RTTI EECBUIListBox > UILBString > UIListBox<UWideString> > UIControl >
// UIEntity; vtable 0x44bed0, 53 slots). EECBUIListBox.cpp 0x27cfdb..0x27efca (__FILE__ 0x27e426). Layout proven:
// EECBUIListBox sizeof 0x15c = its new-site size, ctor 0x27d187 exact in EECBUIListBox.cpp.
// Only what the decompiled units use. Requires lle/UILBString.h.
#ifndef EE_EECBUILISTBOX_H
#define EE_EECBUILISTBOX_H
#include "ee/EECivUpgrade.h"

class EECBUIListBox : public UILBString {
public:
    // the two class statics (EECBUIListBox.cpp's first init chains; guard byte 0x96545c). Unguarded
    // a class static only renumbers an includer's `$E` symbols, never its bytes
    static const UWideString sSeparator;    // "/" (0x965460)
    static const UWideString sCategory;     // "Category" (0x965448)
    // the token indices of a formatted line (EECBUIListBox.cpp's ESVERIFY text names keecbuiFormatIndexUpgradeName; the
    // other names are provisional): an upgrade line is "category/name/bonus/id/cost/points/value text", a category
    // header "category/cost/Category"
    enum {
        kCivTokenCategory = 0,
        kCivTokenUpgradeName = 1,
        keecbuiFormatIndexUpgradeName = kCivTokenUpgradeName,  // spelled this way in the original's assert strings
        kCivTokenBonus = 2,
        kCivTokenUpgrade = 3,
        kCivTokenCost = 4,
        kCivTokenPoints = 5,
        kCivTokenValue = 6
    };

    // 0x27d187 (UIControlsTable's user-defined control factory 0x2749ee passes itself first)
    EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData, GEFont* inFont,
                  const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                  unsigned long inBorderThickness, bool inIsTabable);
    virtual ~EECBUIListBox();                           // 0x27d278

    // overrides (slot offsets from the vtable 0x44bed0)
    virtual void AppendStringArray(std::vector<UWideString>& inLines);   // +0x8c 0x27d3ad
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo);   // +0x90 0x27d3f9
    virtual long CopyData();                            // +0x14 0x27d6ae
    virtual void DeleteLine(unsigned long inLine);      // +0x9c 0x27d6f7
    virtual long DrawPolygons();                        // +0x18 0x27dac2
    virtual long DrawTextA();                           // +0x1c 0x27dac8
    virtual bool GetTabable() const { return this->mIsTabable; }  // +0x28 0x27ef57
    virtual long MouseLButtonUp();                      // +0x38 0x27e378
    virtual long MouseMove(const GE2DPoint& inLocation);   // +0x40 0x27e37e
    virtual void TabbedTo();                            // +0x74 0x27e55a

    // the new virtuals after UIListBox's (slots 0xbc..0xd0; vtable order = declaration order; names by slot offset,
    // argument types from `ret N` and the callers; provisional names)
    virtual void AddLine(const UWideString& inLine);    // 0x27d386 (AppendLine, then 0x27d284)
    // 0x27e116: the selected upgrade line's id, bonus and cost (3 without a selection)
    virtual long GetSelectedUpgrade(EECivUpgrade* outUpgrade, unsigned long* outBonus, unsigned long* outCost);
    virtual void SetUpgradesExcept(const std::vector<EECivUpgrade>& inExcluded);  // 0x27e2bb (every upgrade but these)
    virtual void SetCivilizationUpgrades(std::vector<EECivUpgrade>& inUpgrades);  // 0x27e346 (EEUIEPDCSLoadCiv passes the civ's upgrades)
    virtual void ReformatUpgrades(bool inFlag);         // 0x27dbca (re-formats the upgrade lines)
    virtual void ResetSelection();                      // 0x27e503: UIListBox::ChangeSelectionIndex(0, false, false)

    // the unit's own helpers (provisional names)
    bool AddCategory(const UWideString& inLine);        // 0x27d284 (a category's header line, once)
    void SetUpgrades(std::vector<EECivUpgrade>& inUpgrades, bool inFlag);   // 0x27d4dc
    long DrawCategoryLine(const GERectangle& inRectangle, GEText* inText, const UWideString& inLine);   // 0x27d761
    long DrawUpgradeLine(const GERectangle& inRectangle, GEText* inText, const UWideString& inLine);    // 0x27d8c7
    bool GetCategoryAt(unsigned long inIndex, UWideString& outCategory);   // 0x27dc4b
    static bool GetCategory(const UWideString& inLine, UWideString& outCategory);   // 0x27dc95
    static bool GetCost(const UWideString& inLine, unsigned long* outCost);   // 0x27dca8
    static bool GetUpgradeName(const UWideString& inLine, UWideString& outName);   // 0x27dcbb
    long FindUpgrade(EECivUpgrade inUpgrade, unsigned long* outIndex);   // 0x27dcce
    bool FormatUpgrade(EECivUpgrade inUpgrade, UWideString& outLine, bool inFlag, long inIndex);  // 0x27dd4f
    bool FindCategory(const UWideString& inCategory, unsigned long* outIndex);   // 0x27df7f
    bool IsUpgradeAt(unsigned long inIndex, UWideString* outLine);   // 0x27e056
    static bool IsUpgradeLine(const UWideString& inLine);   // 0x27e0af
    static bool GetBonus(const UWideString& inLine, unsigned long* outBonus);   // 0x27e0e6
    bool GetLine(unsigned long inIndex, UWideString& outLine) const;   // 0x27e19e
    static bool GetToken(const UWideString& inFormattedString, unsigned long inIndex, UWideString& outEntry);   // 0x27e1d4
    static bool GetNumber(const UWideString& inLine, unsigned long inIndex, unsigned long* outValue);   // 0x27e23c
    static bool GetUpgrade(const UWideString& inLine, EECivUpgrade* outUpgrade);   // 0x27e297
    bool RemoveCategory(const UWideString& inCategory);   // 0x27e38b
    bool RemoveCategoryIfEmpty(const UWideString& inLine);   // 0x27e426
    void SortLines();                                   // 0x27e50f (when mSortLines)

    // inline setter (EEUIEPDCSFormOpen 0x1e5e2e calls it on both lists: the two expansions make VC6 refuse the
    // UIEvent ctor inside its last two `new`s, as the original does; provisional name)
    void SetSortLines(bool inSortLines) { this->mSortLines = inSortLines; }

    // ctor 0x27d187 (the PDCS form open event sets them on controls 3 and 4; provisional names)
    GEColor mC13C;                          // +0x13c (kBlack in the ctor; the PDCS form: (0, 0, 0.39), (0.18, 0.18, 0.28))
    GEColor mC148;                          // +0x148 (GEColor() in the ctor)
    bool mSelectUpgradesOnly;               // +0x154 (true in the ctor)
    bool mCopiedShowPoints;                 // +0x155 (not set by the ctor)
    bool mShowPoints;                       // +0x156 (true in the ctor)
    bool mSortLines;                        // +0x157 (true in the ctor)
    bool mIsTabable;                        // +0x158 (the ctor's last argument)
};

#endif
