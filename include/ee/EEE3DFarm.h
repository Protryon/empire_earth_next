// C:\EEX\Empire Earth\Graphics Thread\EEE3DFarm.h (RECONSTRUCTION; file name inferred).
// A farm's field: a ground model over the farm's tiles ("farm ground model") and crops whose vertices sway along a
// circle table (vtable 0x4407a8; RTTI EEE3DFarm > EEEffect3D; EEE3DFarm.cpp 0xee5c0..0xf1327, key chains
// "EEE3DFarm-Data" and the circle table; kind 0x29). Names are provisional. Requires ee/EEEffect3D.h,
// lle/GERectangle.h,
// lle/U2DPoint.h, <vector>.
#ifndef EE_EEE3DFARM_H
#define EE_EEE3DFARM_H

#include <vector>

class EEGraphicObject;
class GEModel;

class EEComplexUnit;
// 0x23cee2 (also declared in ee/EERenderLists.h, which this unit does not include: its statics would renumber the
// unit's $E chain): the unit's draw opacity (Virtual08)
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity);

class EEE3DFarm : public EEEffect3D {
public:
    EEE3DFarm(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect);   // 0xee64c (kind 0x29)
    EEE3DFarm();                            // 0xee702
    virtual ~EEE3DFarm();                   // +0x00 0xee775 (??_G 0xee6e6)
    virtual long DrawPass2();               // +0x08 0xf0ce2 (the draw)
    virtual long DrawPass4();               // +0x10 (`return 0`: ICF-folded into 0xfca98)
    virtual long DrawPass5();               // +0x14 (likewise)
    virtual long DrawPass6();               // +0x18 (likewise)
    virtual void ReplaceUnit(EEUnit* inOldUnit, EEUnit* inNewUnit);  // +0x20 (empty: ICF-folded into 0x163d6f)
    virtual long Queue(EEEffectQueue* inQueues);       // +0x24 0xf02e7 (copies the swaying vertices; queue 4)
    virtual void PrepareQueue();            // +0x2c (empty: ICF-folded into 0xe123d)
    virtual long SetUp();                   // +0x30 0xf0c04 (loads the graphic, builds the models)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);    // +0x38 0xf0c71
    virtual long RepairPointers();          // +0x3c 0xf0f26 (after a load: the area, the models)
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);   // +0x40 0xf0fe9
    virtual long Step();                    // +0x4c 0xf1043 (sways the crops; 7 when the farm is gone)
    virtual void ElevationChanged(long inLeft, long inBottom, long inRight, long inTop);  // +0x50 0xf128a (rebuild if hit)
    virtual bool IsTileVisible(EEMapTile* inTile);  // +0x54 0xf0b1c (any of the farm's tiles, or a doppelganger)

    long BuildModels();                     // 0xee7c8 (`jmp` BuildGroundModel) (provisional name)
    long BuildGroundModel();                // 0xee7cd: the "farm ground model" over the farm's tiles (provisional name)
    long BuildCrops();                      // 0xef5fa (provisional name)
    // 0xf01cb (static): the sway table, a circle of radius inRadius / inSize in steps of 0.0670 (provisional name)
    static void BuildCircle(float inSize, float inRadius);
    void PlaceArea();                       // 0xf0247: the area and centre from the unit's tile and size (provisional name)
    void BuildGroundModels();               // 0xf04ee (provisional name)
    bool HasMoved();                        // 0xf0aa1: true unless the centre is still on the unit's centre tile
    long GetCropKind();                     // 0xf0bdc: 0x4bd960[the unit's mCloaked] (provisional name)
    long GetCropFrame();                    // 0xf0bf0: 0x4bd974[the unit's mCloaked] (provisional name)

    static const UString sData;             // "EEE3DFarm-Data" (0x4e8b98; provisional name)
    static std::vector<U2DPoint<float> > sCircle;   // 0x4e8b80 (guard bit 2 of 0x4e8b90) (provisional name)
    static bool sCircleBuilt;               // 0x4e8ba8 (provisional name)

    GERectangle mArea;                      // +0x74 the farm's tiles (saved)
    GE3DPoint mCenter;                      // +0x84 (saved)
    EEGraphicObject* mGraphicObject;        // +0x90
    GEModel* mGroundModel;                  // +0x94 ("farm ground model"; its sub-model 0: the crops)
    long mL98;                              // +0x98
    GEModel* mGroundCopy;                   // +0x9c (Virtual24 copies +0x94's vertices into it)
    GEModel* mCropsCopy;                    // +0xa0 (Virtual24 copies the crops' vertices into it)
    float mPhase;                           // +0xa4 0..1 (the sway step)
    long mSwayA;                            // +0xa8 0..63 (odd vertices)
    long mSwayB;                            // +0xac 0..63 (even vertices)
    long mRowLength;                        // +0xb0 the unit's size + 1
    bool mBB4;                              // +0xb4 (saved; cleared by Load)
    bool mRebuild;                          // +0xb5 (saved; BuildCrops is due)
    bool mHasCrops;                         // +0xb6 (the complex unit's mHasLeftGame)
    bool mDrawCrops;                        // +0xb7 (Virtual24: the doppelganger's, else mHasCrops)
    bool mDoppelganger;                     // +0xb8 (Virtual24: drawn as a doppelganger)
    // port (R5a.3; past the original object): what DrawPass2 read from the live farm unit, taken by Queue under the
    // world's lock: the unit's draw opacity (Fn23CEE2) and whether the field is drawn (alive or in a game, on the map)
    float mDrawOpacity;
    bool mDrawShown;
};

#endif
