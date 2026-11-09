// ee/EESETSUICBPlayer.h: layout only as far as the verifier-exact
// functions that include it prove it (member names by offset are placeholders).
#ifndef EE_EESETSUICBPLAYER_H
#define EE_EESETSUICBPLAYER_H

#include "lle/UIComboBox.h"

// RTTI EESETSUICBPlayer > UIComboBox; vtable 0x44489c; sizeof 0xe8 (members from the ctor 0x196ebe)
class EESETSUICBPlayer : public UIComboBox {
public:
    // 0x196ebe (the shape of EESETSCComboBox's ctor: GERectangle(inLocation, inWidth, 20); EESETSConditionControls)
    EESETSUICBPlayer(unsigned long inId, unsigned long inTextBoxType, std::vector<UWideString>& inData, GEFont* inFont,
                     const GE2DPoint& inLocation, unsigned long inWidth, unsigned long inMaxListBoxHeight,
                     UIForm* inForm, unsigned long inIndex, unsigned long inMaxChars, unsigned long inBorderThickness);
    virtual ~EESETSUICBPlayer();
    unsigned long GetSelectedPlayer();      // 0x196f5e (the player of the selection; EESETSCondition::Virtual44)
    void SetSelectedPlayer(unsigned long inPlayer);   // 0x196fbd (EESETSCTDiplomacy::Virtual0C)
    void FillList();                        // 0x197014 (one entry per mFlags bit; provisional name)
    // inline (EESETSCTDiplomacy::Virtual10 stores the flags and calls FillList through one load; provisional name)
    void SetFlags(unsigned long inFlags) { this->mFlags = inFlags; this->FillList(); }
    void SetPlayerCount(unsigned long inCount) { this->mPlayerCount = inCount; }

protected:
    virtual void ChangeSelection();         // +0x8c 0x19718d (the base's, then the trigger screen's Virtual3C)

public:

    unsigned long mFlags;                   // +0xe0 (which entries FillList shows: bit 1 -> string 0x717a, ...)
    unsigned long mPlayerCount;             // +0xe4 (EESETSCondition::Virtual20 stores the player screen's player)
};

#endif
