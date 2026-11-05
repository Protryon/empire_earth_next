// C:\EEX\Empire Earth\Civilization Builder\EECBEditableCivilization.h (RECONSTRUCTION; file name inferred;
// provisional names). The civilization being edited in the civilization builder (RTTI EECBEditableCivilization,
// vtable 0x44beac, sizeof 0x34, ctor 0x27c9db; EECBEditableCivilization.cpp 0x27c9db..0x27cd52) and its subclass
// EECBCivilizationCreator (RTTI, vtable 0x44be88, sizeof 0x35, ctor 0x27c8b9; EECBCivilizationCreator.cpp
// 0x27c8b9..0x27c9db). Not a class with a destructor: slot +0x00 is the "commit" (it sends the upgrades message,
// 0x27ca2b), so the list box and the form events reach it through the global only.
#ifndef EE_EECBEDITABLECIVILIZATION_H
#define EE_EECBEDITABLECIVILIZATION_H
#include "ee/EECivUpgrade.h"

#include <vector>

class EECivilization;

class EECBEditableCivilization {
public:
    // the second argument is the player id (0x680bb1 pushes gWorld.mCurrentPlayer raw; the ctor keeps its low byte in
    // mPlayer, which Virtual00 sends as EEMCivUpgradesRequested's player)
    EECBEditableCivilization(EECivilization* inCivilization, unsigned char inPlayer);   // 0x27c9db
    // +0x00 0x27ca2b: sends EEMCivUpgradesRequested (the editable's upgrades and name) when 0x27cc4e says there is
    // something to send; false when not
    virtual bool Apply();
    virtual long GetCost(EECivUpgrade inUpgrade);  // +0x04 0x27cab7
    virtual long GetRefund(EECivUpgrade inUpgrade);  // +0x08 0x27caf1
    // +0x0c..+0x1c (signatures from the bodies, provisional names)
    virtual bool CanChange();                       // +0x0c 0x27cbb0 (multiplayer: asks mCivilization 0x12c72c)
    virtual unsigned long GetPointsLeft();                 // +0x10 (defined in the .cpp; its COMDAT is ICF-shared 0xb6fee `mov eax, [ecx+0x14]`)
    virtual long GetMaxPoints();                    // +0x14 0x27cc84 (a tail call on mCivilization, 0x12c734)
    virtual bool AddUpgrade(EECivUpgrade inCount);  // +0x18 0x27cc98 (calls GetCost inCount times)
    virtual bool RemoveUpgrade(EECivUpgrade inUpgrade);  // +0x1c 0x27ccd6 (finds inUpgrade in mUpgrades)

    // non-virtual members, in address order
    void ResetToStart();                            // 0x27cb30 (back to the civilization's start: name string 0x7454, mL14, no upgrades)
    unsigned long PickedInCategory(EECivUpgrade inUpgrade);  // 0x27cb83
    std::vector<EECivUpgrade> CopyUpgradeIDs();   // 0x27cbe6 (mUpgrades, then the civilization's own ids)
    bool HasChoices();                              // 0x27cc4e (there is something to commit)
    bool GetIsUnchanged();                          // 0x27cc7c (mCivilization's flag +0x3d)
    UWideString* GetName();                         // 0x27cc8c (&mName)
    std::vector<EECivUpgrade>& GetCivilizationUpgrades();  // 0x27cc90 (the civilization's own ids; non-const: the builder fills the lists from it)
    void SetName(const UWideString& inName);        // 0x27cd52

    // members from the ctor 0x27c9db (inCivilization, inB30): sizeof 0x34 (new site)
    std::vector<EECivUpgrade> mUpgrades;  // +0x04 (the 2-byte _Buy 0x18bdf)
    long mPointsLeft;                       // +0x14 (points left; untouched by the ctor; GetPointsLeft returns it)
    EECivilization* mCivilization;          // +0x18 (ctor argument 1; mName = its +0x20 name)
    UWideString mName;                      // +0x1c
    unsigned char mPlayer;                  // +0x30 (ctor argument 2; its address goes to EEMCivUpgradesRequested as a `const unsigned char&`)
};

extern EECBEditableCivilization* gEditableCivilization;    // 0x565560

#endif
