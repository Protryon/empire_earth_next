// C:\EEX\Empire Earth\Graphics Thread\EERenderLists.h (RECONSTRUCTION; file name inferred; type and member names
// are provisional). The render objects' per-frame lists: one guarded static object at 0x4e9458 (init chain
// 0x10ab8a, guard 0x4e9454 bit 1; ctor 0x1117da, implicit dtor 0x10abc5), filled by EEROUnits and
// EERenderObject::QueueEffects and drawn by EEROGroundShadows/EEROUnits. Layouts from the ctor/dtor and the readers;
// PLACEHOLDER members (`_gap`) cover what is not decoded. Only the render units include it (the gap arrays would
// renumber other includers' `$E`). Requires lle/GETransformation.h, lle/UWideString, <map>, <vector>.
#ifndef EE_EERENDERLISTS_H
#define EE_EERENDERLISTS_H

#include <map>
#include <vector>

class GEModel;
class EEGraphicObject;

class GETexture;

class EEUnitClass;
class RTSRenderObject;


// one drawn unit (0x148 bytes; ctor 0x1118e9, dtor 0x111997): 0x200 of them in EERenderLists
// (implicit ctor: GETransformation's, then GEColor's three fld1 twice, the vectors, the name)
struct EERenderUnit {
    GETransformation mTransformation;       // +0x00 (the shadows read its position)
    GEColor mPlayerColor;                   // +0x40 (EEGraphicObject::SetGroupColor takes its address with the part)
    GEColor mTintColor;                     // +0x4c (class 0x104: the model's material 0 colour)

    std::vector<float> mHealthBars;         // +0x58 (the extra bars' fractions: EEROUnits::DrawUnitBars)
    // the turning parts (EEROUnits::DrawUnitModel): yaw angles for mTurretModels's models, roll for mOrbitalModels's, pitch for mWheelModels's
    std::vector<float> mTurretAngles;       // +0x68
    std::vector<GEModel*> mTurretModels;    // +0x78
    std::vector<float> mOrbitalAngles;      // +0x88
    std::vector<GEModel*> mOrbitalModels;   // +0x98
    std::vector<float> mWheelAngles;        // +0xa8
    std::vector<GEModel*> mWheelModels;     // +0xb8
    UWideString mName;                      // +0xc8 (0x14 bytes)

    EEUnitClass* mInfo;                     // +0xdc (its +0x178 ring size, +0x2d0 bit 0, Virtual54)

    EEGraphicObject* mSource;               // +0xe0 (its models, shadow model, bar offset)
    EEGraphicObject* mSource2;              // +0xe4 (Fn10B80B unloads it when set)
    float mScale;                           // +0xe8 (the shadow's size)
    float mSelectionSize;                   // +0xec (> 0: the selection ring is drawn)
    float mShadowScale;                     // +0xf0 (the shadow's opacity; > 0: drawn)
    float mFF4;                             // +0xf4 (the class's +0x170: fld/fstp, FillDoppelgangerUnit)
    float mGroundZ;                         // +0xf8
    float mOpacity;                         // +0xfc
    float mHealth;                          // +0x100 (>= 0: the health bar's fraction)
    float mFraction;                        // +0x104 (the frame fraction, when gInterpolateFrames)
    float mSubModelOffsetScale;             // +0x108 (scales sub model 0's internal offset)
    float mFogScale;                        // +0x10c (the fog band's start: kFogDepth times it before the centre)
    long mCategory;                         // +0x110 (10: no shadow; 0/1: no ring)
    unsigned long mUnitClassIndex;          // +0x114 (into gUnitClassTable)
    long mL118;                             // +0x118
    long mCommandType;                      // +0x11c
    long mCurrentResearch;                  // +0x120
    long mAnimation;                        // +0x124 (4, 5: no ring)
    long mGridSize;                         // +0x128 (the footprint grid's cells per side: EEROUnits::DrawPlacement)
    unsigned long mCurrentFrame;            // +0x12c
    unsigned long mNextFrame;               // +0x130
    long mL134;                             // +0x134
    unsigned long mUnitId;                  // +0x138 (class 0x33a: the unit whose +0x3c decides the opacity)
    long mPlayerID;                         // +0x13c
    unsigned long mAreaEffectStrength;      // +0x140 (the ring's highlighted segments)
    bool mCanPlace;                         // +0x144 (EEROUnits::DrawPlacement: drawn by DrawUnitModel without the fog pass)
    bool mIsFogged;                         // +0x145
    bool mIsTinted;                         // +0x146

    bool mHasSubModelOffset;                // +0x147 (sub model 0 is offset against its internal transform)

};

// an element of EERenderLists::mGhostRecords (0x420 bytes; its implicit dtor runs EERenderUnit's) PLACEHOLDER
// (its implicit operator= 0x10b919: EERenderUnit's 0x10b983, then the arrays element by element)
struct EERenderUnitRecord {
    EERenderUnit mUnit;                     // +0x000
    float mHeights[12][12];                 // +0x148 (the footprint grid's corner heights [x][y]: EEROUnits::DrawPlacement)
    unsigned char mCells[12][12];           // +0x388 (0: the cell's edges are drawn)

    long mTileX;                            // +0x418
    long mTileY;                            // +0x41c

};

// one placed object (0x3c bytes; also its ground shadow): 0x600 of them in EERenderLists
struct EERenderShadow {
    GE3DPoint mLocation;                    // +0x00
    EEGraphicObject* mObject;               // +0x0c
    float mModelScale;                      // +0x10
    float mSelectionSize;                   // +0x14 (> 0: EEROUnits draws its ring)
    float mScale;                           // +0x18
    float mF1C;                             // +0x1c (EERenderObject::FillObject: the class's +0x170, or 0)
    float mHealth;                          // +0x20 (the bar's fraction: EEROUnits::DrawObjectBars)

    float mOpacity;                         // +0x24
    float mFraction;                        // +0x28
    unsigned long mModelIndex;              // +0x2c
    unsigned long mCurrentFrame;            // +0x30
    unsigned long mNextFrame;               // +0x34
    bool mIsFogged;                         // +0x38 (EEROUnits::DrawObjectModels picks the lighting by it)
    unsigned char _gap39[0x3];              // +0x39

};

typedef std::multimap<GETexture*, EERenderUnit*> EERenderUnitMap;
typedef std::multimap<GETexture*, EERenderShadow*> EERenderShadowMap;

struct EERenderLists {
    // inline: its COMDAT 0x1117da closes EERenderObject.cpp with the template COMDATs (first user: the init chain)
    EERenderLists()
    {
        this->mRenderUnitCount = 0;
        this->mRenderShadowCount = 0;
    }
    static EERenderLists sInstance;         // 0x4e9458 (guard 0x4e9454 bit 1: a class static) (provisional name)

    EERenderUnitMap mUnits0;                // +0x00
    EERenderUnitMap mUnits1;                // +0x0c
    EERenderUnitMap mUnits2;                // +0x18 (EEROGroundShadows draws their shadows)
    EERenderShadowMap mShadows0;            // +0x24
    EERenderShadowMap mShadows1;            // +0x30
    EERenderShadowMap mShadows2;            // +0x3c (EEROGroundShadows draws them)
    std::vector<EERenderUnitRecord> mGhostRecords;  // +0x48
    std::vector<EERenderUnit*> mSelectedUnits;  // +0x58 (the units with a selection ring/bar)
    std::vector<EERenderShadow*> mSelectedObjects;  // +0x68
    EERenderUnit mRenderUnits[0x200];       // +0x78
    EERenderShadow mRenderShadows[0x600];   // +0x29078
    EERenderUnit* mSelectedUnit;            // +0x3f878 (0x528cd0: its bars are drawn outside mU58) (provisional name)
    EERenderShadow* mSelectedObject;        // +0x3f87c (0x528cd4) (provisional name)
    unsigned long mRenderUnitCount;         // +0x3f880
    unsigned long mRenderShadowCount;       // +0x3f884
};

// EERenderObject::FillRenderLists's frame state, passed to its per-unit helpers (a local at [ebp-0x3c]) (provisional
// names)
class EEUnit;
struct EERenderFrameInfo {
    EEUnit* mPickedUnit;                    // +0x00 (the unit under the mouse: EEUnitPicker::PickUnit, referenced)
    float mYaw;                             // +0x04 (the camera's yaw minus pi/2: the objects' orientation)
    float mNumFightingUnits;                // +0x08 (a sum FillRenderLists divides by its unit count for the music)
    unsigned long mFrame;                   // +0x0c (the frame counter)
    bool mAmbient;                          // +0x10 (the ambient sounds are updated this frame)
};
// a textured 3D line drawn by EEROTerrain::RenderScene (0x18 bytes) (provisional name)
struct EERenderLine {
    std::vector<std::pair<GE3DPoint, float> > mPoints;   // +0x00
    GETexture* mTexture;                    // +0x10
    float mWidth;                           // +0x14
};
// 0x529050 (a class static by its guard 0x929048 bit 1, defined with a UWideString 0x529030 bit 2 in the unit at
// 0x10aa88) (provisional name and owner)
extern std::vector<EERenderLine> gU529050;

// the lighting/sky environment (0x528ce0: ctor 0x111722, empty dtor; a class static by its guard 0x928d70 bit 2,
// shared with gUnitPicker's bit 1) (provisional names)
struct EERenderEnvironment {
    // inline: its COMDAT 0x111722 closes EERenderObject.cpp with the template COMDATs (first user: the init chain)
    EERenderEnvironment()
    {
        this->mB8C = false;
        this->mSeaLevel = 0.0f;
        this->mWindSpeed = 1.25e-6f;
        this->mTimeOfDayChanged = true;
        this->mShowHiddenUnits = 1;
        this->mTimeOfDay = 0.0f;
        this->mWaterPhase = 0.0f;
        this->mCloudOffset = 0.0f;
        this->mCameraYaw = 0.0f;
    }
    ~EERenderEnvironment() {}
    GEVector mDirection;                    // +0x00 (Fn10C036 copies gWorld.mWindDirection with movs: GEVector's operator=)
    // Fn10C036's colours (each GEColor's default 1, 1, 1): +0x0c/+0x24 gU4E9338, +0x18/+0x30 the time-of-day blend of
    // gU4E9348, +0x3c/+0x54 the lit colour and +0x48 half of it
    GEColor mTerrainAmbientColor;           // +0x0c (Fn10F833: the ambient colour)
    GEColor mTerrainLightColor;             // +0x18 (Fn10F833: the light's colour)
    GEColor mUnitAmbientColor;              // +0x24
    GEColor mUnitLightColor;                // +0x30
    GEColor mPrelitColor;                   // +0x3c (Fn10F71C: the ambient colour)
    GEColor mDimPrelitColor;                // +0x48
    GEColor mColor54;                       // +0x54 (Fn10F772: the ambient colour)
    float mUnitAmbientIntensity;            // +0x60 (the time-of-day blend of gU4E9264)
    float mUnitLightIntensity;              // +0x64 (of gU4E92EC)
    float mTerrainAmbientIntensity;         // +0x68 (of gU4E92A8; Fn10F833: the ambient intensity scale)
    float mTerrainLightIntensity;           // +0x6c (of gU4E92EC; Fn10F833: the light's intensity scale)
    float mSeaLevel;                        // +0x70 (0)
    float mTimeOfDay;                       // +0x74 (0; [0, 1): EEROTerrain's sky colours)
    float mWaterPhase;                      // +0x78 (0)
    float mCloudOffset;                     // +0x7c (0; EEROTerrain's cloud texture scroll)
    float mWindSpeed;                       // +0x80 (1.25e-6)
    float mCameraYaw;                       // +0x84 (0)
    long mShowHiddenUnits;                  // +0x88 (1)
    bool mB8C;                              // +0x8c (false)
    bool mTimeOfDayChanged;                 // +0x8d (true)
};
extern EERenderEnvironment gRenderEnvironment;   // 0x528ce0


// the fog/sky colour (0x4e9220, EERenderObject.cpp's chain 0x10ad9a; EEROTerrain sets it) (provisional name)
extern GEColor gU4E9220;
// the grid lines' width and texture coordinate step (0x4e9330; EERenderObject.cpp) (provisional name)
extern float gU4E9330;
// the wall and tower height offsets (0x4cc9fc 0.7, 0x4cca00 1.5: .data words EEUnitClass.cpp 0x230c36 reads too;
// EERenderObject adds them to the ghost and unit z for unit types 0x302/0x31f) (provisional names and owner)
extern float gSpaceTurretHeight;
extern float gSpaceStationHeight;
// EERenderObject::FillRenderLists's lock around the tile walk (0x529014; TSCriticalSection*) (provisional name and
// owner)
extern bool& gMusicFollowsBattle;            // 0x5308e0 (the music follows the battle intensity) (provisional name)
extern bool& gUnitLighting;                  // 0x5308e3 (the second light set: Fn10F8DE..Fn10FB74) (provisional name)
extern float& gZoomLock;                     // 0x530840 (Fn10F60F's fog band) (provisional name)
// the game's render objects (0x565350..0x565360; Empire Earth.cpp creates them; EEPostGraphicsAll/EEResetGraphicsValues push them)
// (provisional names)
extern RTSRenderObject* gU565350;
extern RTSRenderObject* gU565354;
extern RTSRenderObject* gU565358;
extern RTSRenderObject* gU56535C;
extern RTSRenderObject* gU565360;
extern RTSRenderObject* gU56534C;           // 0x56534c (the user interface's)
// 0x23cee2 (cdecl): a complex unit's own translucency (ioOpacity) (provisional name; EERenderObject::GetUnitOpacity)
class EEComplexUnit;
bool Fn23CEE2(EEComplexUnit* inUnit, bool inTranslucent, float& ioOpacity);
// the area effect texture (0x5293f4; EEROTerrain.cpp, also declared in ee/EEROTerrain.h; EERenderObject's circles)
extern GETexture* gAreaEffectTexture;


#endif

