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

#include "rust/render/gfx.h"   // EeGfxFillUnit, EeGfxFillObject (ee_gfx fill.rs)

class GEModel;
class EEGraphicObject;

class GETexture;

class EEUnitClass;
class RTSRenderObject;


// one drawn unit (the original's 0x148 bytes; ctor 0x1118e9, dtor 0x111997): 0x200 of them in EERenderLists. Port
// (Lane R): the head is ee_gfx fill.rs's EeGfxFillUnit itself (the members fill.rs writes: the transformation, the
// player and tint colours, the class, the sources, the scales, the frames, the flags), its implicit ctor's values
// (GETransformation's identity, GEColor's white twice) kept; the vectors and the name after it
struct EERenderUnit : EeGfxFillUnit {
    EERenderUnit() : EeGfxFillUnit{{{{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}}}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f, 1.0f}} {}

    // the C++ members (the original record's +0x58..+0xdc; port: after the head)
    std::vector<float> mHealthBars;         // (the extra bars' fractions: EEROUnits::DrawUnitBars)
    // the turning parts (EEROUnits::DrawUnitModel): yaw angles for mTurretModels's models, roll for mOrbitalModels's, pitch for mWheelModels's
    std::vector<float> mTurretAngles;
    std::vector<GEModel*> mTurretModels;
    std::vector<float> mOrbitalAngles;
    std::vector<GEModel*> mOrbitalModels;
    std::vector<float> mWheelAngles;
    std::vector<GEModel*> mWheelModels;
    UWideString mName;
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

// one placed object (0x3c bytes; also its ground shadow): 0x600 of them in EERenderLists. Port: ee_gfx fill.rs's
// EeGfxFillObject itself (location, object, model scale, selection size, scale, the class's +0x170, health, opacity,
// fraction, model index, frames, fogged)
typedef EeGfxFillObject EERenderShadow;

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
    EEUnit* mPickedUnit;                    // +0x00 (the unit under the mouse: EEUnitPicker::PickUnitUnreferenced; port
                                            // R5a.3: the original took a reference, the world's lock is held throughout)
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

// the lighting/sky environment (0x528ce0; gRenderEnvironment): gamecpp render_object.rs' RENDER_ENV


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
extern bool& gUnitLighting;                  // 0x5308e3 (the second light set: SetMapObjectLighting..DimUnitLighting) (provisional name)
extern float& gZoomLock;                     // 0x530840 (SetSceneFog's fog band) (provisional name)
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

