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

extern unsigned long& gTexturePixelDepth;
extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gTrilinearFilter;

// Creates an empty, unloaded graphic object: no names, models, textures or sounds, drawn, mip maps on (the record's
// members: ee_data tables.rs)
EEGraphicObject::EEGraphicObject() : EeDataMediaUse()
{
    ee_data_graphic_object_default(this);
    for (unsigned long i = 0; i < 14; i++) {
        this->mTextures[i] = NULL;
        this->mModels[i] = NULL;
        this->mSounds1[i] = NULL;
        this->mSounds2[i] = NULL;
        this->mTagPoints[i] = NULL;
        this->mModelNames[i] = NULL;
        this->mTextureNames[i] = NULL;
    }
    this->mFileName = NULL;
    this->mTextureName = NULL;
    this->mShadowModelName = NULL;
    this->mShadowTextureName = NULL;
    this->mModel = NULL;
    this->mTexture = NULL;
    this->mShadowModel = NULL;
    this->mShadowTexture = NULL;
    this->mTags = NULL;
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

// Collects the model's turret parts, orbitals and wheels, recursing into its sub-models
// The part scan of inModel under inParent: a "turret_" model becomes a turret part at its transformed origin, an
// orbiting model an EEOrbital, a wheel model an EEWheel; then the sub-models under the combined transformation
void EEGraphicObject::CollectParts(GEModel* inModel, const GETransformation& inParent)
{
    GE3DPoint theOrigin;
    GE3DPoint thePosition;
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    if (inModel == NULL)
        return;
    GETransformation theTransformation(false);
    inParent.Apply(inModel->GetInternalTransform(0), theTransformation);
    theTransformation.Apply(theUp, theDirection);
    if (strncmp(inModel->GetName(), "turret_", strlen("turret_")) == 0) {
        theTransformation.Apply(theOrigin, thePosition);
        this->mTurretParts.push_back(new EETurretPart(thePosition, theTransformation, inModel));
    } else if (EEOrbital::IsOrbitalModel(inModel)) {
        this->mOrbitals.push_back(new EEOrbital(inModel));
    } else if (EEIsWheelModel(inModel)) {
        this->mWheels.push_back(new EEWheel(inModel));
    }
    for (unsigned long i = 0; i < inModel->GetSubModelCount(); i++)
        this->CollectParts(inModel->GetSubModel(i), theTransformation);
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

// Deletes every entry of the media table and empties it
void EEDeleteMediaTable()
{
    for (unsigned long i = 0; i < gMediaTable.size(); i++)
        delete gMediaTable[i];
    gMediaTable.resize(0);
}

// Writes to the file each graphic object that is still loaded, with its file name and use count
void EEDumpGraphicObjectTable(FSFileSpec& inFile)
{
    fprintf(inFile, "\n\n**********Graphic Object Table*********\n\n");
    for (unsigned long i = 0; i < gMediaTable.size(); i++) {
        EEGraphicObject* theObject = static_cast<EEGraphicObject*>(gMediaTable[i]);
        if (theObject != NULL && theObject->mMediaCount > 0)
            fprintf(inFile, "\t%s (filename) has a media count of %ld\n",(const char*)*theObject->mFileName,
                    theObject->mMediaCount);
    }
}

// Returns the tag points of a kind on a part's model, or NULL when it has none (ee_gfx tags.rs)
EETagPointList* EEGraphicObject::GetTagPoints(unsigned long inPart, long inKind)
{
    return reinterpret_cast<EETagPointList*>(ee_gfx_tags_find(this->mTags, this->mModels[inPart], inKind));
}

// Returns one of the attack points, picked by the game time, or the default point when there are none
const GE3DPoint& EEGraphicObject::PickPoint()
{
    const EeGe3DPoint* thePoint = ee_gfx_tags_pick(this->mTags, gPhysics->mGameTime);
    return thePoint != NULL ? *reinterpret_cast<const GE3DPoint*>(thePoint) : this->mDefaultPoint;
}

// Finds the turret part for a model and returns its index; returns 0 when found, else 3
long EEGraphicObject::FindTurretPart(GEModel* inModel, unsigned long& outIndex)
{
    long theResult = 3;
    unsigned long theCount = this->mTurretParts.size();
    for (unsigned long i = 0; i < theCount; i++) {
        if (this->mTurretParts[i]->mModel == inModel) {
            outIndex = i;
            theResult = 0;
            break;
        }
    }
    return theResult;
}

// Loads the object's models, textures, sounds and parts on first use (counting later uses); returns 9 if a
// model failed
// The load (vtable 0x43f788 +0x04): on the first use, under the database lock, the frame ranges of each animated part
// into a per-model texture-frame map (the model names numbered in first-seen order; 14: no model), then the shared
// model, the shadow model and texture, each part's texture and model (and its part scan), the sounds, and the part
// scan of the shared model; a later use waits until the first load is done
long EEGraphicObject::LoadMedia()
{
    GETransformation theTransformation;
    GETextureDescription theShadowDescription;
    theShadowDescription.mPixelDepth = gTexturePixelDepth;
    theShadowDescription.mNumMipLevelsToDrop = 0;
    theShadowDescription.mSurfaceVolatility = 1;
    theShadowDescription.mFrameFormat = 0;
    theShadowDescription.mIs3DRenderable = false;
    theShadowDescription.mIsTransparencyEnabled = false;
    theShadowDescription.mHasAlpha = true;
    theShadowDescription.mCreateMIPMaps = true;
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = this->mHasAlpha;
    theDescription.mCreateMIPMaps = true;
    std::map<unsigned long, long> theFrames[15];
    std::map<unsigned long, long> theNoFrames;
    long theSlots[14];
    return EEMediaAcquire(this, sDatabaseLock.mLock, [&]() -> long {
        long theResult = 0;
        if (this->mTags == NULL)
            this->mTags = ee_gfx_tags_new();
        // the texture-frame maps of the animated parts that share frames, by model (ee_data tables.rs)
        EeDataGraphicFramePart theParts[14];
        for (unsigned long i = 0; i < 14; i++)
            theParts[i] = {this->mFrameCounts[i], this->mStartFrames[i], this->mLastFrames[i],
                           this->mModelNames[i] != NULL ? (const char*)*this->mModelNames[i] : NULL, this->mSharesFrames[i]};
        ee_data_graphic_frame_maps(theParts, 14, theSlots,
            [](void* inFrames, long inSlot, unsigned long inFrame, long inValue) {
                static_cast<std::map<unsigned long, long>*>(inFrames)[inSlot].insert(std::make_pair(inFrame, inValue));
            }, theFrames);
        if (this->mModel == NULL && this->mFileName != NULL && this->mFileName->GetLength() > 0) {
            this->mModel = gModelManager->LoadModel(*this->mFileName, theFrames[14]);
            if (this->mModel != NULL)
                this->mModel->Reference();
        }
        if (this->mShadowModel == NULL && this->mShadowModelName != NULL && this->mShadowModelName->GetLength() > 0) {
            this->mShadowModel = gModelManager->LoadModel(*this->mShadowModelName, theNoFrames);
            if (this->mShadowModel != NULL)
                this->mShadowModel->Reference();
            else
                theResult = 9;
        }
        theDescription.mCreateMIPMaps = this->mCreateMipMaps;
        theDescription.mNumMipLevelsToDrop = (this->mNumMipLevelsToDrop + gTextureMipLevelsToDrop < this->mMaxMipLevelsToDrop) ? this->mNumMipLevelsToDrop + gTextureMipLevelsToDrop : this->mMaxMipLevelsToDrop;
        theShadowDescription.mNumMipLevelsToDrop = 0;
        theShadowDescription.mCreateMIPMaps = true;
        if (this->mTextureName != NULL)
            this->LoadTexture(*this->mTextureName, this->mTexture, theDescription, this->mIsDrawn);
        if (this->mShadowTextureName != NULL)
            this->LoadTexture(*this->mShadowTextureName, this->mShadowTexture, theShadowDescription, this->mIsDrawn);
        unsigned long thePart;
        for (thePart = 0; thePart < 14; thePart++) {
            if (this->mTextureNames[thePart] != NULL && this->mTextureNames[thePart]->GetLength() > 0)
                this->LoadTexture(*this->mTextureNames[thePart], this->mTextures[thePart], theDescription, this->mIsDrawn);
            else if (this->mTexture != NULL) {
                this->mTexture->Reference();
                this->mTextures[thePart] = this->mTexture;
            }
            if (this->mModelNames[thePart] != NULL && this->mModelNames[thePart]->GetLength() > 0) {
                if (this->mFrameCounts[thePart] > 4 && this->mSharesFrames[thePart])
                    this->mModels[thePart] = gModelManager->LoadModel(*this->mModelNames[thePart], theFrames[theSlots[thePart]]);
                else
                    this->mModels[thePart] = gModelManager->LoadModel(*this->mModelNames[thePart], theNoFrames);
                if (this->mModels[thePart] != NULL)
                    this->mModels[thePart]->Reference();
                else
                    theResult = 9;
            } else if (this->mModel != NULL) {
                this->mModel->Reference();
                this->mModels[thePart] = this->mModel;
            } else
                this->mModels[thePart] = NULL;
            // the part's tag points, colour groups and points (ee_gfx tags.rs: CollectTagPoints 0xd8bb2)
            if (this->mModels[thePart] != NULL)
                ee_gfx_tags_collect(this->mTags, thePart, this->mModels[thePart], reinterpret_cast<const EeGeTransformation*>(&theTransformation),
                                    this->mModels[0], this->mLastFrames[5], reinterpret_cast<EeGe3DPoint*>(&this->mDefaultPoint),
                                    reinterpret_cast<EeGe3DPoint*>(&this->mTopPoint), reinterpret_cast<EeGe3DPoint*>(&this->mBar),
                                    reinterpret_cast<EeGfxTagList**>(this->mTagPoints));
        }
        for (thePart = 0; thePart < 14; thePart++) {
            if (this->mSounds1[thePart] != NULL)
                this->mSounds1[thePart]->LoadMedia();
            if (this->mSounds2[thePart] != NULL)
                this->mSounds2[thePart]->LoadMedia();
        }
        if (this->mModel != NULL)
            this->CollectParts(this->mModel, theTransformation);
        return theResult;
    });
}

// Loads a texture by name into the slot if it is empty, with modulate, wrap and the object's mip settings
void EEGraphicObject::LoadTexture(const UString& inName, GETexture*& ioTexture, const GETextureDescription& inDescription,
                              bool inUnused)
{
    if (ioTexture == NULL && inName.GetLength() > 0) {
        ioTexture = gTextureManager->LoadTexture(inName, inDescription);
        if (ioTexture != NULL) {
            ioTexture->Reference();
            ioTexture->SetStageAlphaMode(kStageOpModulate, -1);
            ioTexture->SetStageColorMode(kStageOpModulate, -1);
            ioTexture->SetStageMinFilterMode(1, -1);
            ioTexture->SetStageMagFilterMode(1, -1);
            ioTexture->SetStageMipMapBias(this->mMipMapBias, -1);
            ioTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
            if (gTrilinearFilter) ioTexture->SetStageMipFilterMode(kMipFilterLinear, -1); else ioTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        }
    }
}

// Sets the colour of every player-colour material of a part
void EEGraphicObject::SetGroupColor(const GEColor& inColor, unsigned long inPart)
{
    ee_gfx_tags_set_color(this->mTags, inPart, &inColor.mRed);
}

// Counts one use down; on the last use releases all the object's models, textures, sounds and parts.
// Returns 0
// The unload: waits while the object is being loaded (mIsLoading), then under the database lock, on the last use, releases
// the shadow model, each part's texture, tag points and model, the tag-point maps, the models, textures and sounds,
// and deletes the orbitals, turret parts, wheels and colour groups; an earlier use only counts down
long EEGraphicObject::UnloadMedia()
{
    EEMediaRelease(this, sDatabaseLock.mLock, [&] {
        if (this->mShadowModel != NULL) {
            this->mShadowModel->Dereference();
            this->mShadowModel = NULL;
        }
        unsigned long i;
        for (i = 0; i < 14; i++) {
            if (this->mTextures[i] != NULL) {
                this->mTextures[i]->Dereference();
                this->mTextures[i] = NULL;
            }
            ee_gfx_tags_clear_weapon(this->mTags, i);
            this->mTagPoints[i] = NULL;
            if (this->mModels[i] != NULL) {
                this->mModels[i]->Dereference();
                this->mModels[i] = NULL;
            }
        }
        // the kinds' lists (by kind, then by model), the colour groups and the attack points (ee_gfx tags.rs)
        ee_gfx_tags_free(this->mTags);
        this->mTags = NULL;
        if (this->mModel != NULL) {
            this->mModel->Dereference();
            this->mModel = NULL;
        }
        if (this->mTexture != NULL) {
            this->mTexture->Dereference();
            this->mTexture = NULL;
        }
        if (this->mShadowTexture != NULL) {
            this->mShadowTexture->Dereference();
            this->mShadowTexture = NULL;
        }
        for (i = 0; i < 14; i++) {
            if (this->mSounds1[i] != NULL)
                this->mSounds1[i]->UnloadMedia();
            if (this->mSounds2[i] != NULL)
                this->mSounds2[i]->UnloadMedia();
        }
        for (i = 0; i < this->mOrbitals.size(); i++)
            delete this->mOrbitals[i];
        this->mOrbitals.clear();
        for (i = 0; i < this->mTurretParts.size(); i++)
            delete this->mTurretParts[i];
        this->mTurretParts.clear();
        for (i = 0; i < this->mWheels.size(); i++)
            delete this->mWheels[i];
        this->mWheels.clear();
    });
    return 0;
}

// The tag points' geometry and the scale (ee_gfx tags.rs), over the Rust layouts
static_assert(sizeof(EETagPoint) == sizeof(EeGfxTagPoint) && sizeof(EETagPointList) == sizeof(EeGfxTagList), "tags.rs");
static_assert(sizeof(GETransformation) == sizeof(EeGeTransformation) && sizeof(GE3DPoint) == sizeof(EeGe3DPoint), "GE");
static const EeGeTransformation* GERust(const GETransformation& inValue) { return reinterpret_cast<const EeGeTransformation*>(&inValue); }
static EeGeTransformation* GERust(GETransformation& inValue) { return reinterpret_cast<EeGeTransformation*>(&inValue); }
static EeGe3DPoint* GERust(GE3DPoint& inValue) { return reinterpret_cast<EeGe3DPoint*>(&inValue); }

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
