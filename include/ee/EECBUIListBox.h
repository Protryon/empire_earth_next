// C:\EEX\Empire Earth\Civilization Builder\EECBUIListBox.h (RECONSTRUCTION; file name inferred; provisional member
// names). The civilization builder's list box (RTTI EECBUIListBox > UILBString > UIListBox<UWideString> > UIControl >
// UIEntity). EECBUIListBox.cpp 0x27cfdb..0x27efca. Its logic is Rust's (ee_screens civ_list.rs: the lines'
// tokens, order, categories, selection, formatting and drawing); the class keeps the toolkit's base and its members,
// its overrides forward (the list's host ops are Rust's: civ_host.rs). Requires
// lle/UILBString.h.
#ifndef EE_EECBUILISTBOX_H
#define EE_EECBUILISTBOX_H
#include <stdint.h>

#include "ee/EECivUpgrade.h"
#include "rust/game/screens.h"
#include "lle/UILBString.h"


class EECBUIListBox : public UILBString {
public:
    // 0x27d187 (UIControlsTable's user-defined control factory passes itself first)
    EECBUIListBox(UIControlsTable& inTable, unsigned long inId, std::vector<UWideString>& inData, GEFont* inFont,
                  const GERectangle& inRectangle, UIForm* inForm, unsigned long inIndex,
                  unsigned long inBorderThickness, bool inIsTabable);
    virtual ~EECBUIListBox() {}

    // the overrides (civ_list.rs)
    virtual void AppendStringArray(std::vector<UWideString>& inLines)
    {
        ee_screens_cbb_append((uintptr_t)static_cast<UIControl*>(this), (uintptr_t)&inLines);
    }
    virtual void ChangeSelectionIndex(unsigned long inIndex, bool inFireEvent, bool inScrollTo)
    {
        ee_screens_cbl_change_selection(this, (uint32_t)inIndex, inFireEvent, inScrollTo);
    }
    // Snapshots the list for drawing: no highlight on line 0, the header colour and the show-points flag
    virtual long CopyData() { return (long)ee_screens_cbb_copy_data((uintptr_t)static_cast<UIControl*>(this)); }
    virtual void DeleteLine(unsigned long inLine) { ee_screens_cbl_delete_line(this, (uint32_t)inLine); }
    virtual long DrawTextA() { return ee_screens_cbl_draw_text(this); }
    virtual bool GetTabable() const { return this->mIsTabable; }
    virtual void TabbedTo() { ee_screens_cbl_tabbed_to(this); }
    virtual void AddLine(const UWideString& inLine)
    {
        ee_screens_cbl_add_line(this, reinterpret_cast<const uint8_t*>((const char*)inLine), inLine.GetTCHARLength());
    }
    // the civilization's own upgrades at their refunds (the predefined civilizations' form's statistics)
    virtual void SetCivilizationUpgrades(std::vector<EECivUpgrade>& inUpgrades)
    {
        ee_screens_cbl_set_civilization_upgrades(this, inUpgrades.data(), inUpgrades.size());
    }
    // line 0 selected without an event
    virtual void ResetSelection() { UIListBox<UWideString>::ChangeSelectionIndex(0, false, false); }

    void SetSortLines(bool inSortLines) { this->mSortLines = inSortLines; }

    // (the base's members the builder host's shims call are public: UIListBox.h)

    GEColor mC13C;                          // the header colour (kBlack; the choice form's: (0, 0, 0.39), (0.18, 0.18, 0.28))
    GEColor mC148;                          // the header colour as of the last CopyData
    bool mSelectUpgradesOnly;               // upgrade lines only selectable (true)
    bool mCopiedShowPoints;                 // the points shown as of the last CopyData
    bool mShowPoints;                       // the points shown (true)
    bool mSortLines;                        // the lines sorted (true)
    bool mIsTabable;                        // the ctor's last argument
};

#endif
