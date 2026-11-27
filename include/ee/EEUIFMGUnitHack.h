// The main game form's unit-hack panel (RECONSTRUCTION; class and header names inferred: no RTTI, not polymorphic).
// Owned by EEUIFMainGame (+0x4e8, new'd 0x2c bytes by its ctor 0x262e73, deleted by its dtor 0x2647e1);
// all members were in EEUIFMainGame.cpp (0x2628e5..0x262e73). It lists the creatable unit classes by name in the
// form's combo box 98 and sends EEMHackANewUnit (place a unit) / EEMLoadUnitData (reload a class).
#ifndef EE_EEUIFMGUNITHACK_H
#define EE_EEUIFMGUNITHACK_H
#include "rust/game/records.h"
#include "rust/game/screens.h"

class EEUIFMainGame;
struct EeScreensUnitHack;

// Whole in Rust (ee_screens unit_hack.rs: the names, class ids and mode; the original's +0x00 mNames, +0x10
// mClassIDs, +0x24 mMode, +0x28 mL28 are its record's): this shim holds the record and forwards.
class EEUIFMGUnitHack : public EeScreensUnitHackFields {
public:
    EEUIFMGUnitHack(EEUIFMainGame* inForm);     // 0x2628e5 (the sorted class names)
    ~EEUIFMGUnitHack() { ee_screens_unit_hack_free(static_cast<EeScreensUnitHack*>(this->mRecord)); }
    void PlaceUnit(const GE2DPoint& inTile)
    {
        ee_screens_unit_hack_call(static_cast<EeScreensUnitHack*>(this->mRecord), static_cast<EEUIFMainGame*>(this->mForm), EE_SCREENS_UH_PLACE_UNIT, inTile.GetX(), inTile.GetY());
    }    // 0x262aa5 (EEMHackANewUnit for the selected player)
    void FillComboBox()
    {
        ee_screens_unit_hack_call(static_cast<EeScreensUnitHack*>(this->mRecord), static_cast<EEUIFMainGame*>(this->mForm), EE_SCREENS_UH_FILL_COMBO_BOX, 0.0f, 0.0f);
    }
    void LoadUnitData()
    {
        ee_screens_unit_hack_call(static_cast<EeScreensUnitHack*>(this->mRecord), static_cast<EEUIFMainGame*>(this->mForm), EE_SCREENS_UH_LOAD_UNIT_DATA, 0.0f, 0.0f);
    }                        // 0x262cdc (EEMLoadUnitData for the selected class)
    void Update(const GE2DPoint& inLocation)
    {
        ee_screens_unit_hack_call(static_cast<EeScreensUnitHack*>(this->mRecord), static_cast<EEUIFMainGame*>(this->mForm), EE_SCREENS_UH_UPDATE, inLocation.GetX(), inLocation.GetY());
    }   // 0x262d66 (the left mouse button over the game view)
    void Fn157269(const GE2DPoint& inTile) {}   // (ICF-folded into 0x157269: an empty body; provisional name)

};

#endif
