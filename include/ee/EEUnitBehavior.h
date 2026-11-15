// C:\EEX\Empire Earth\Database\EEUnitBehavior.h (RECONSTRUCTION; header and provisional names).
// The unit behaviour flags of db\dbUnitBehavior.dat (RTTI EEUnitBehavior, no base; vtable 0x43f808, one slot: ??_G
// 0xdc950; sizeof 0xc from Load's new). EEUnitBehavior.cpp 0xdc8e7..0xdd160 (with EEWorldTable's and EEToHitTable's
// loaders).
#ifndef EE_EEUNITBEHAVIOR_H
#define EE_EEUNITBEHAVIOR_H

#include <vector>

class EEUnit;
class FSFileSpec;

class EEUnitBehavior {
public:
    EEUnitBehavior();                       // 0xdc930
    virtual ~EEUnitBehavior();              // +0x00 0xdc96c (??_G 0xdc950)
    static long Load(FSFileSpec* inFile);   // 0xdc973 (records with a negative +0x68 stay NULL)
    static void Unload();                   // 0xdca3d
    bool CanMoveFreely(EEUnit* inUnit);     // 0xdca7d (by the unit class's +0x318 flag; provisional name)
    // cdecl COMDAT 0x1f5430, called out of line by EEComplexUnit::FinishLoad 0x1f353f (x2) and ResetEEComplexUnit
    // 0x1fac37
    static EEUnitBehavior* Get(unsigned long inIndex) { return EEUnitBehavior::sTable[inIndex]; }  // 0x1f5430 (provisional name)

    bool mCanMoveFreely;                    // +0x04 (record +0x6c)
    bool mMeleeCanMoveFreely;               // +0x05 (record +0x73)
    bool mRangedCanMoveFreely;              // +0x06 (record +0x74)
    bool mCanPursuePastInitialContactLOS;   // +0x07 (record +0x6d)
    bool mCanTargetEnemies;                 // +0x08 (record +0x6e)
    bool mReturnsToInitialContactLocation;  // +0x09 (record +0x6f)
    bool mCanFlee;                          // +0x0a (record +0x70)

    // 0x8e8740 (guard 0x8e8738 bit 1; only its unit sees it: other units read its _First as gUnitBehaviors)
    static std::vector<EEUnitBehavior*> sTable;
};

// 0x4e8750: the weapon to-hit factors, defined in EEUnitBehavior.cpp (provisional name; EEPhysicsModel::Push)
extern float gWeaponToHitFactors[6][6];

#endif
