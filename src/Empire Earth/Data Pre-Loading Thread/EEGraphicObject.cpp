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

// Destroys the wheel (nothing to release)
EEWheel::~EEWheel()
{
}

// Returns whether a model part is a wheel (its name starts with "wheel", any case)
bool EEIsWheelModel(GEModel* inModel)
{
    return inModel->GetName().EqualsIgnoreCase(kWheelName, kWheelName.GetLength());
}

extern unsigned long& gTexturePixelDepth;
extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gTrilinearFilter;

// Creates an empty, unloaded graphic object: no names, models, textures or sounds, drawn, mip maps on (the record's
// members: ee_data tables.rs)
EEGraphicObject::EEGraphicObject() : EeDataMediaUse()
{
    ee_data_graphic_object_default(this);
}

// Destroys the graphic object: unloads its media if loaded, frees its file names
EEGraphicObject::~EEGraphicObject()
{
    if (this->mMediaCount > 0) {
        this->mMediaCount = 1;
        this->UnloadMedia();
    }
    delete this->mFileName;
    delete this->mTextureName;
    delete this->mShadowModelName;
    delete this->mShadowTextureName;
    for (unsigned long i = 0; i < 14; i++) {
        delete this->mModelNames[i];
        delete this->mTextureNames[i];
    }
}

// Reads the graphics table into the media table (each record after its 32 names; the record and its copy are ee_data
// tables.rs': the animation states, the sounds by id, alpha and mip settings); entries with media index 0 are dropped.
// Returns the last read result
long EEGraphicObject::LoadTable(FSFile* inFile)
{
    UString theName;
    EEGraphicObject* theObject = NULL;
    auto theNextName = [&]() { theName.Load(*inFile); return new UString((const char*)theName); };
    return EEDbReadLeadRecords(inFile, ee_data_graphic_object_record_size(),
        [](unsigned long inCount) { gMediaTable.resize(inCount, NULL); return true; },
        [&](unsigned long inIndex) {
            theObject = new EEGraphicObject;
            gMediaTable[inIndex] = theObject;
            theName.Load(*inFile);
            theObject->mFileName = theNextName();
            theObject->mTextureName = theNextName();
            for (unsigned long j = 0; j < 14; j++) {
                theObject->mTextureNames[j] = theNextName();
                theObject->mModelNames[j] = theNextName();
            }
            theObject->mShadowModelName = theNextName();
            theObject->mShadowTextureName = theNextName();
        },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            EeDataGraphicObjectSounds theSounds = ee_data_graphic_object_from_record(inRecord, theObject);
            for (unsigned long i = 0; i < 14; i++) {
                theObject->mSounds1[i] = EESoundObject::Get(theSounds.sound1[i]);
                if (theObject->mSounds1[i] == NULL)
                    theObject->mSound1Frames[i] = -1;
                theObject->mSounds2[i] = EESoundObject::Get(theSounds.sound2[i]);
                if (theObject->mSounds2[i] == NULL)
                    theObject->mSound2Frames[i] = -1;
            }
            if (theObject->mMediaIndex == 0) {
                delete theObject;
                gMediaTable[inIndex] = NULL;
            }
        });
}

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

// Returns the tag points of a kind on a part's model, or NULL when it has none (ee_gfx tags.rs)
EETagPointList* EEGraphicObject::GetTagPoints(unsigned long inPart, long inKind)
{
    return reinterpret_cast<EETagPointList*>(ee_gfx_tags_find(this->mTags, this->mModels[inPart], inKind));
}

// Finds the turret part for a model and returns its index; returns 0 when found, else 3 (preload_media.rs)
long EEGraphicObject::FindTurretPart(GEModel* inModel, unsigned long& outIndex)
{
    return ee_preload_find_turret_part(this, inModel, &outIndex);
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
    [](void* inObject) {
        EEGraphicObject* theObject = static_cast<EEGraphicObject*>(inObject);
        for (unsigned long i = 0; i < theObject->mOrbitals.size(); i++)
            delete theObject->mOrbitals[i];
        theObject->mOrbitals.clear();
        for (unsigned long i = 0; i < theObject->mTurretParts.size(); i++)
            delete theObject->mTurretParts[i];
        theObject->mTurretParts.clear();
        for (unsigned long i = 0; i < theObject->mWheels.size(); i++)
            delete theObject->mWheels[i];
        theObject->mWheels.clear();
    },
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

void EETagPoint::GetOffset(unsigned long inFrame, GE3DPoint& outPoint)
{
    ee_gfx_tag_offset(reinterpret_cast<const EeGfxTagPoint*>(this), inFrame, NULL, GERust(outPoint));
}

void EETagPoint::GetOffset(unsigned long inFrame, const GETransformation& inPart, GE3DPoint& outPoint)
{
    ee_gfx_tag_offset(reinterpret_cast<const EeGfxTagPoint*>(this), inFrame, GERust(inPart), GERust(outPoint));
}

void EETagPointList::Scale(GETransformation& ioTransformation, float inScale)
{
    ee_gfx_tags_scale(GERust(ioTransformation), inScale);
}
