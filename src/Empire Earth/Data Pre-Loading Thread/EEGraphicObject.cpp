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
#include "ee/EEWheel.h"
#include "ee/EESoundObject.h"
#include "ee/EEGlobalConstants.h"

extern unsigned long& gTexturePixelDepth;
extern unsigned long& gTextureMipLevelsToDrop;
extern bool& gTrilinearFilter;

// 0x4e8434..0x4e8440: four built-in graphic objects outside gMediaTable; LoadTable clears them, EEEvent EEUnloadBuiltInGraphicObjects
// 0x1ca6d9 unloads them and deletes those with mMediaIndex -1 (provisional names)
EEGraphicObject* gGraphicObjectU4E8434;
EEGraphicObject* gGraphicObjectU4E8438;
EEGraphicObject* gGraphicObjectU4E843C;
EEGraphicObject* gGraphicObjectU4E8440;

// Creates an empty, unloaded graphic object: no names, models, textures or sounds, drawn, mip maps on (the record's
// members: ee_data tables.rs)
EEGraphicObject::EEGraphicObject()
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
    this->mMediaCount = 0;
    this->mIsLoading = false;
}

// Destroys the graphic object: unloads its media if loaded, frees its file names and colour groups
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
    for (unsigned long thePart = 0; thePart < 14; thePart++)
        for (unsigned long j = 0; j < this->mColorGroups[thePart].size(); j++)
            delete this->mColorGroups[thePart][j];
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
    gGraphicObjectU4E8440 = NULL;
    gGraphicObjectU4E843C = NULL;
    gGraphicObjectU4E8438 = NULL;
    gGraphicObjectU4E8434 = NULL;
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

// Returns the tag points of a kind on a part's model, or NULL when it has none
EETagPointList* EEGraphicObject::GetTagPoints(unsigned long inPart, long inKind)
{
    EETagPointList* theResult = NULL;
    std::map<long, std::map<const GEModel*, EETagPointList*>*>::iterator theKind = this->mTagPointMaps.find(inKind);
    if (theKind != this->mTagPointMaps.end()) {
        std::map<const GEModel*, EETagPointList*>* theLists = theKind->second;
        const GEModel* theModel = this->mModels[inPart];
        std::map<const GEModel*, EETagPointList*>::iterator theList = theLists->find(theModel);
        if (theList != theLists->end())
            theResult = theList->second;
    }
    return theResult;
}

// Returns one of the attack points, picked by the game time, or the default point when there are none
const GE3DPoint& EEGraphicObject::PickPoint()
{
    if (this->mPoints.size() == 0)
        return this->mDefaultPoint;
    long theTime = gPhysics->mGameTime;
    return this->mPoints[theTime % this->mPoints.size()];
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
    long theResult = 0;
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 0) {
        this->mMediaCount = 1;
        this->mIsLoading = true;
        sDatabaseLock.mLock->Leave();
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
            if (this->mModels[thePart] != NULL)
                this->CollectTagPoints(thePart, this->mModels[thePart], theTransformation);
        }
        for (thePart = 0; thePart < 14; thePart++) {
            if (this->mSounds1[thePart] != NULL)
                this->mSounds1[thePart]->LoadMedia();
            if (this->mSounds2[thePart] != NULL)
                this->mSounds2[thePart]->LoadMedia();
        }
        if (this->mModel != NULL)
            this->CollectParts(this->mModel, theTransformation);
        sDatabaseLock.mLock->Enter();
        this->mIsLoading = false;
    } else {
        this->mMediaCount++;
        sDatabaseLock.mLock->Leave();
        while (this->mIsLoading == true)
            PlatSleep(1);
        sDatabaseLock.mLock->Enter();
    }
    sDatabaseLock.mLock->Leave();
    return theResult;
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

// Collects a part model's tag points, attack and status bar points and player-colour materials, recursively
// The tag points and colour groups of model part inPart's model inModel (recursive over the sub-models): each tag point
// named for a point kind goes into the kind's list for the root model (not for the sub-models of part 0's model when
// inPart > 0), "weapon_mount" into the part's list, "attack" gives the default point (and the top point at the frame
// mLastFrames[5]), "status bar" the bar offset (otherwise the top of the bounding box raises it); the materials named
// "player color" form the part's colour group
void EEGraphicObject::CollectTagPoints(unsigned long inPart, GEModel* inModel, const GETransformation& inParent)
{
    GETransformation theTransformation;
    GE3DPoint theTop;
    GE3DPoint theBarPoint;
    bool theFound = false;
    unsigned long theSubModelCount = inModel->GetSubModelCount();
    unsigned long theTagPointCount = inModel->GetTagPointCount();
    inParent.Apply(inModel->GetInternalTransform(0), theTransformation);
    GEModel* theRoot = inModel;
    while (theRoot->GetParentModel() != NULL)
        theRoot = theRoot->GetParentModel();
    unsigned long i;
    for (i = 0; i < theTagPointCount; i++) {
        if (theRoot != this->mModels[0] || inPart == 0) {
            long theKinds[0x48];
            size_t theKindCount = ee_data_tag_point_kinds(inModel->GetTagPointName(i), theKinds);
            for (size_t k = 0; k < theKindCount; k++) {
                std::map<const GEModel*, EETagPointList*>*& theLists = this->mTagPointMaps[theKinds[k]];
                if (theLists == NULL)
                    theLists = new std::map<const GEModel*, EETagPointList*>;
                EETagPointList*& theList = (*theLists)[theRoot];
                if (theList == NULL)
                    theList = new EETagPointList;
                theList->Add(inModel, i);
            }
        }
        if (strncmp(inModel->GetTagPointName(i), "weapon_mount", strlen("weapon_mount")) == 0) {
            if (this->mTagPoints[inPart] == NULL)
                this->mTagPoints[inPart] = new EETagPointList;
            this->mTagPoints[inPart]->Add(inModel, i);
        }
        if (strncmp(inModel->GetTagPointName(i), "attack", strlen("attack")) == 0) {
            theTransformation.Apply(inModel->GetTagPoint(i, 0), this->mDefaultPoint);
            this->mPoints.push_back(this->mDefaultPoint);
            if (this->mLastFrames[5] < inModel->GetFrameCount())
                theTransformation.Apply(inModel->GetTagPoint(i, this->mLastFrames[5]), this->mTopPoint);
        }
        if (strncmp(inModel->GetTagPointName(i), "status bar", strlen("status bar")) == 0) {
            theTransformation.Apply(inModel->GetTagPoint(i, 0), this->mBar);
            theFound = true;
        }
    }
    if (!theFound) {
        theTop.SetZ(inModel->GetBoundingBox(0).GetTopRightBack().GetZ());
        theTransformation.Apply(theTop, theBarPoint);
        this->mBar.SetZ(theBarPoint.GetZ() < this->mBar.GetZ() ? this->mBar.GetZ() : theBarPoint.GetZ());
    }
    bool theHasGroup = false;
    for (long theMaterial = 0; (unsigned long)theMaterial < inModel->GetMaterialCount(); theMaterial++) {
        if (inModel->GetMaterial(theMaterial)->GetName() == "player color") {
            if (!theHasGroup) {
                theHasGroup = true;
                EEColorGroup* theGroup = new EEColorGroup;
                theGroup->mModel = inModel;
                this->mColorGroups[inPart].push_back(theGroup);
            }
            this->mColorGroups[inPart][this->mColorGroups[inPart].size() - 1]->mMaterials.push_back(theMaterial);
        }
    }
    for (i = 0; i < theSubModelCount; i++)
        this->CollectTagPoints(inPart, inModel->GetSubModel(i), theTransformation);
}

// Sets the colour of every player-colour material of a part
void EEGraphicObject::SetGroupColor(const GEColor& inColor, unsigned long inPart)
{
    for (unsigned long i = 0; i < this->mColorGroups[inPart].size(); i++) {
        EEColorGroup* theGroup = this->mColorGroups[inPart][i];
        for (unsigned long j = 0; j < theGroup->mMaterials.size(); j++)
            theGroup->mModel->GetMaterial(theGroup->mMaterials[j])->SetColor(inColor);
    }
}

// Counts one use down; on the last use releases all the object's models, textures, sounds and parts.
// Returns 0
// The unload: waits while the object is being loaded (mIsLoading), then under the database lock, on the last use, releases
// the shadow model, each part's texture, tag points and model, the tag-point maps, the models, textures and sounds,
// and deletes the orbitals, turret parts, wheels and colour groups; an earlier use only counts down
long EEGraphicObject::UnloadMedia()
{
    std::map<long, std::map<const GEModel*, EETagPointList*>*>::iterator theKind;
    std::map<const GEModel*, EETagPointList*>::iterator theModel;
    while (this->mIsLoading == true)
        PlatSleep(1);
    sDatabaseLock.mLock->Enter();
    if (this->mMediaCount == 1) {
        this->mMediaCount = 0;
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
            if (this->mTagPoints[i] != NULL) {
                this->mTagPoints[i]->Clear();
                delete this->mTagPoints[i];
                this->mTagPoints[i] = NULL;
            }
            if (this->mModels[i] != NULL) {
                this->mModels[i]->Dereference();
                this->mModels[i] = NULL;
            }
        }
        for (theKind = this->mTagPointMaps.begin(); theKind != this->mTagPointMaps.end(); theKind++) {
            std::map<const GEModel*, EETagPointList*>* theLists = theKind->second;
            for (theModel = theLists->begin(); theModel != theLists->end(); theModel++) {
                theModel->second->Clear();
                delete theModel->second;
            }
            theLists->clear();
            delete theKind->second;
        }
        this->mTagPointMaps.clear();
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
        for (unsigned long thePart = 0; thePart < 14; thePart++) {
            for (i = 0; i < this->mColorGroups[thePart].size(); i++)
                delete this->mColorGroups[thePart][i];
            this->mColorGroups[thePart].clear();
        }
    } else if (this->mMediaCount > 1) {
        this->mMediaCount--;
    }
    sDatabaseLock.mLock->Leave();
    return 0;
}

// Creates a tag point referring to tag point inIndex of the model, holding a reference to the model
inline EETagPoint::EETagPoint(GEModel* inModel, unsigned long inIndex)
{
    this->mModel = inModel;
    inModel->Reference();
    this->mIndex = inIndex;
}

inline void EETagPointList::Add(GEModel* inModel, unsigned long inIndex)
{
    this->mPoints.push_back(new EETagPoint(inModel, inIndex));
}
