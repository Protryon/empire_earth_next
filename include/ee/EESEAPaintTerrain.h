// C:\EEX\Empire Earth\Scenario Editor Actions\EESEAPaintTerrain.h (RECONSTRUCTION; file name inferred). The editor's
// terrain painting action (RTTI EESEAPaintTerrain > EESEAPaint > EEAction; vtable 0x445914, sizeof 0x28). The inline
// ctor is emitted expanded (EESEACreateBlankMap::Virtual4 0x1a5999, with EEAction's ctor refused); the ??_G 0x1a5ab5
// follows that first user (a user-declared empty dtor: its own vptr first, then the deque).
// Virtual4 0x1a8e6c and SetArea 0x1a8f45 are in EESEAPaintTerrain.cpp. Names are provisional.
// Requires ee/EESEAPaint.h.
#ifndef EE_EESEAPAINTTERRAIN_H
#define EE_EESEAPAINTTERRAIN_H

class EESEAPaintTerrain : public EESEAPaint {
public:
    EESEAPaintTerrain(long inTerrain, long inTexture)
        : mPlan(ee_screens_sea_paint_new(kSEAPaintTerrain, inTerrain, inTexture)), mTerrain(inTerrain), mTexture(inTexture) {}
    virtual ~EESEAPaintTerrain() { ee_screens_sea_paint_free(this->mPlan); }   // +0x00 (??_G 0x1a5ab5)
    virtual void Virtual4();                // +0x04 0x1a8e6c
    virtual void Virtual8();                // +0x08 (0xe123d: an empty body)
    virtual bool SetArea(long inLeft, long inTop, long inRight, long inBottom);   // +0x0c 0x1a8f45

    // (0x1a8d3e PlantTerrainUnits, the ambient units' scatter: ee_screens editor_actions.rs)

    EeScreensSeaPaint* mPlan;               // the areas still to paint (+0x0c was their deque; editor_actions.rs)
    long mTerrain;                          // +0x20 (EETerrainTable index)
    long mTexture;                          // +0x24 (the table entry's mL68)
};

#endif
