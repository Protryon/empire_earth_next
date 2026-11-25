// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEGraphicObject.cpp (name from RTTI EEGraphicObject; no __FILE__).
// 0xd72c1..0xd9b2b: the graphics of the database (EEGraphicObject : EEMedia). The media table's init chain
// (gMediaTable, 0x4e8448) and EEDatabase.h's lock chain (0x4e8430) first, then the class (ctor, ??_G, dtor, the
// orbital/turret part scan, LoadTable, the table cleanup and dump statics, the record loader, the tag-point and model
// lookups, the texture helper, the colour setter), then the deferred COMDATs (vector/map members, EEOrbital's ??_G,
// EETurretPart's ctor). EEGraphicEffect.cpp ends at 0xd72c1, EEMusicObject.cpp starts at 0xd9b2b. Unit flags: the
// exe's module set.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"

#include "ee/EEMedia.h"
#include "EEDatabase.h"
#include "lle/GERenderState.h"

std::vector<EEMedia*> gMediaTable;          // 0x4e8448 (EEMedia::Get/GetCount read it)

#include "lle/UString.h"
#include "lle/GEColor.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "ee/EEPhysics.h"
#include "ee/EmpireEarth.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEDbRecords.h"
#include "ee/EEMediaUse.h"
#include "ee/EEWheel.h"
#include "ee/EESoundObject.h"
#include "ee/EEGlobalConstants.h"
#include "rust/render/gfx.h"
#include "rust/game/preload_media.h"

// The wheels of a unit model, as the graphic object makes them from its model's parts (were EEWheel.cpp's, which
// keeps their Update: the graphic object named no unit code for them; PORT_LEARNINGS "Cycle cuts")
const UString kWheelName("wheel");

// Creates a wheel for a model part, its radius half the part's bounding-box height, unturned
EEWheel::EEWheel(GEModel* inModel)
{
    this->mModel = inModel;
    this->mRadius = inModel->GetBoundingBox(0).GetHeight() * 0.5f;
    this->mAngle = 0.0f;
}

// Creates a copy of a wheel (model part, radius and angle)
EEWheel::EEWheel(const EEWheel& inWheel)
{
    this->mModel = inWheel.mModel;
    this->mRadius = inWheel.mRadius;
    this->mAngle = inWheel.mAngle;
}

// Returns whether a model part is a wheel (its name starts with "wheel", any case)
bool EEIsWheelModel(GEModel* inModel)
{
    return inModel->GetName().EqualsIgnoreCase(kWheelName, kWheelName.GetLength());
}

extern unsigned long& gTexturePixelDepth;
extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gTrilinearFilter;

// Deletes every entry of the media table and empties it (ee_game preload_media.rs)
void EEDeleteMediaTable()
{
    ee_preload_delete_media_table();
}

// Writes to the file each graphic object that is still loaded, with its file name and use count (preload_media.rs)
void EEDumpGraphicObjectTable(FSFileSpec& inFile)
{
    ee_preload_dump_graphic_objects(&inFile);
}

// The calls ee_gfx gobj_media.rs makes back into the C++: the sounds' loads (the EEMedia virtuals) and the part
// lists (C++ objects other code reads)
static const EeGfxGobjHost kGobjHost = {
    [](void* inSound) { static_cast<EESoundObject*>(inSound)->LoadMedia(); },
    [](void* inSound) { static_cast<EESoundObject*>(inSound)->UnloadMedia(); },
    [](void* inObject, const EeGe3DPoint* inPosition, const EeGeTransformation* inAt, void* inModel) {
        static_cast<EEGraphicObject*>(inObject)->mTurretParts.push_back(new EETurretPart(
            *reinterpret_cast<const GE3DPoint*>(inPosition), *reinterpret_cast<const GETransformation*>(inAt),
            static_cast<GEModel*>(inModel)));
    },
    [](void* inObject, void* inModel) {
        static_cast<EEGraphicObject*>(inObject)->mOrbitals.push_back(new EEOrbital(static_cast<GEModel*>(inModel)));
    },
    [](void* inObject, void* inModel) -> bool {
        if (!EEIsWheelModel(static_cast<GEModel*>(inModel)))
            return false;
        static_cast<EEGraphicObject*>(inObject)->mWheels.push_back(new EEWheel(static_cast<GEModel*>(inModel)));
        return true;
    },
    ee_preload_graphic_clear_parts,   // (preload_media.rs)
};
static_assert(sizeof(GEModelManager) == sizeof(void*) && sizeof(GETextureManager) == sizeof(void*),
              "the managers are their Rust objects (gobj_media.rs reads them in place)");

// Loads the object's models, textures, sounds and parts on its first use (a later use waits for the first load);
// 9 when a model failed. The load is ee_gfx gobj_media.rs' (0xd85ed: the shared-frame maps, the models, the
// textures and their stages, the tag points, the sounds, the part scan) under the database lock
long EEGraphicObject::LoadMedia()
{
    EeGfxGobjLoad theLoad = {gModelManager, gTextureManager, gTexturePixelDepth, gTextureMipLevelsToDrop,
                             gTrilinearFilter, &kGobjHost};
    return EEMediaAcquire(this, sDatabaseLock.mLock, [&]() -> long { return ee_gfx_gobj_load(this, &theLoad); });
}

// Counts one use down; on the last use (under the database lock, once any load is done) releases the models,
// textures, tag points, sounds and parts (ee_gfx gobj_media.rs, 0xd907e). Returns 0
long EEGraphicObject::UnloadMedia()
{
    EEMediaRelease(this, sDatabaseLock.mLock, [&] { ee_gfx_gobj_unload(this, &kGobjHost); });
    return 0;
}

// The tag points' geometry and the scale (ee_gfx tags.rs), over the Rust layouts
static_assert(sizeof(EETagPoint) == sizeof(EeGfxTagPoint) && sizeof(EETagPointList) == sizeof(EeGfxTagList), "tags.rs");
static_assert(sizeof(GETransformation) == sizeof(EeGeTransformation) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint), "GE");

void EETagPoint::GetPoint(unsigned long inFrame, const GETransformation& inTransformation, GE3DPoint& outPoint)
{
    ee_gfx_tag_point(reinterpret_cast<const EeGfxTagPoint*>(this), inFrame, GERust(inTransformation), NULL, GERust(outPoint));
}

void EETagPoint::GetPoint(unsigned long inFrame, const GETransformation& inTransformation, const GETransformation& inPart,
                          GE3DPoint& outPoint)
{
    ee_gfx_tag_point(reinterpret_cast<const EeGfxTagPoint*>(this), inFrame, GERust(inTransformation), GERust(inPart),
                     GERust(outPoint));
}
