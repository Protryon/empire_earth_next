// C:\EEX\Empire Earth\Database\EEGraphicObject.h (RECONSTRUCTION; file name inferred).
// One graphic of the database (RTTI EEGraphicObject > EEMedia; EEGraphicObject.cpp 0xd72c1..0xd9b2b). The objects
// of EEGraphicEffect's per-kind tables (0x4e7f9c, 0x48 vectors) and EEGraphicEffect::mGraphicObject: LoadMedia
// (+0x04) returns a result code (EEE2DMarchingIcons::Load tests it), UnloadMedia (+0x08) releases it (its dtor).
// Only what the 2D effects read; the rest is PLACEHOLDER (layout not decoded). Requires ee/EEMedia.h.
#ifndef EE_EEGRAPHICOBJECT_H
#define EE_EEGRAPHICOBJECT_H
class FSFile;

#include <vector>
#include <map>                      // mTagPointMaps
#include "ee/EEMedia.h"
#include "lle/GE3DPoint.h"          // mDefaultPoint, mTopPoint

class GETexture;
class GETransformation;
class GE3DPoint;
class GEColor;
class GEModel;
class EEWheel;
class EESoundObject;
class UString;
struct GETextureDescription;

#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"

// Actual0x50 turret part from ctorD9AB4, allocationD76A1 and two class readers.
// No vptr; nominal name provisional. Position0, transform0xc, model0x4c.
struct EETurretPart {
    GE3DPoint mPosition;
    GETransformation mTransformation;
    GEModel* mModel;

    // inline (COMDAT 0xd9ab4, emitted by EEGraphicObject.cpp: the members' default ctors, then the assignments)
    EETurretPart(const GE3DPoint& inPosition, const GETransformation& inTransformation, GEModel* inModel)
        { this->mPosition = inPosition; this->mTransformation = inTransformation; this->mModel = inModel; }
};

// One orbiting part of a graphic (RTTI EEOrbital, vtable 0x43f798: one slot, ??_G 0xd9a97 in EEGraphicObject.cpp, a
// user-declared empty virtual dtor: it stores the vptr first). sizeof 0x10 (EEE3DDisease::SetUp copies one with
// operator new(0x10) and the inline copy ctor). Names are provisional.
class EEOrbital {
public:
    // inline (EEGraphicObject::CollectParts 0xd76a1 news one: +0x08, the vptr, +0x0c, +0x04) (provisional name)
    EEOrbital(GEModel* inModel) { this->mSpeed = 0.0f; this->mAngle = 0.0f; this->mModel = NULL; this->Init(inModel); }
    // user-declared (EEE3DDisease::Load: the vptr store comes before the field copies, i.e. body assignments)
    EEOrbital(const EEOrbital& inOrbital)
        { this->mModel = inOrbital.mModel; this->mSpeed = inOrbital.mSpeed; this->mAngle = inOrbital.mAngle; }
    virtual ~EEOrbital() {}
    // 0x208c9b (cdecl): the model is an orbiting part (its name starts with "orbital_cw_" or "orbital_ccw_", any case;
    // EEGraphicObject 0xd76a1 tests each part) (provisional name)
    static bool IsOrbitalModel(GEModel* inModel);
    // 0x208ce6 (ret 4; after EEGraphicObject 0xd76a1 news one inline: vptr, mAngle 0, mModel 0): the model and the
    // speed from the name's degrees after the prefix, negative for clockwise (provisional name)
    void Init(GEModel* inModel);
    void Update();                          // 0x208e28: mAngle += frame time * mSpeed, wrapped to [-pi, pi]

    GEModel* mModel;                        // +0x04 (EEE3DDisease::DrawPass5 sets its pre-model transform)
    float mSpeed;                           // +0x08
    float mAngle;                           // +0x0c
};

// A point tagged on one of a graphic's models: the model and the point's index in its per-frame point arrays (+0x10c).
// The effects that start at a unit's weapon or body point (EEE3DBlastWave, EEE3DShield, EEE3DPulseCannonBall,
// EEE3DTracers) look these up through the unit's model. Bodies in another unit's range (0xdaccd/0xdad54, listed
// MISSING by EEStartingResources.cpp). Names are provisional.
class EETagPoint {
public:
    // inline (EETagPointList::Add 0xd9a5d news one: the model, its reference, the index) (provisional name)
    EETagPoint(GEModel* inModel, unsigned long inIndex);
    // 0xdaccd: the point at inFrame through the model's part transformations, then inTransformation
    void GetPoint(unsigned long inFrame, const GETransformation& inTransformation, GE3DPoint& outPoint);
    // 0xdad54: likewise with the part's own transformation inPart (a separately oriented part) first
    void GetPoint(unsigned long inFrame, const GETransformation& inTransformation, const GETransformation& inPart,
                  GE3DPoint& outPoint);
    void GetOffset(unsigned long inFrame, GE3DPoint& outPoint);   // 0xdab97 (the point at inFrame in the model's space; EEUCLandable::IsBombOnTarget) (provisional name)
    // 0xdac22: GetOffset with the part's own transformation inPart first (provisional name)
    void GetOffset(unsigned long inFrame, const GETransformation& inPart, GE3DPoint& outPoint);
    // 0xdadfb: inPart through the model's part transformations, then inTransformation (provisional name)
    void GetTransformation(unsigned long inFrame, const GETransformation& inTransformation,
                           const GETransformation& inPart, GETransformation& outTransformation);

    GEModel* mModel;                        // +0x00
    unsigned long mIndex;                   // +0x04
};

// The tag points of one kind (EEModel::FnD84EC's result; EEGraphicObject +0x114[part]). The vector is at +0
// (the callers' size() is vector<long>'s ICF twin). Names are provisional.
class EETagPointList {
public:
    // 0xe1814: scales the orientation rows of ioTransformation by inScale (does not read this)
    void Scale(GETransformation& ioTransformation, float inScale);
    // inline (EEE3DCitizenBeam::Aim 0xe9606 calls vector::operator[] out of line through it: the ICF twin 0x11e2a5)
    // (provisional name)
    EETagPoint* GetTagPoint(unsigned long inIndex) { return this->mPoints[inIndex]; }
    // inline (EEE3DDamageVent::Step 0xeafab calls vector::size out of line through it: the ICF twin 0x7c428)
    // (provisional name)
    unsigned long GetNumTagPoints() const { return this->mPoints.size(); }
    // 0xdab52 (in EEStartingResources.cpp's range): dereferences each point's model, deletes the points, empties
    // the list; the inline dtor runs it (UnloadMedia: Clear, Clear, the vector, delete) (provisional name)
    void Clear();
    ~EETagPointList() { this->Clear(); }
    // inline (COMDAT 0xd9a5d, emitted by EEGraphicObject.cpp; CollectTagPoints calls it): a new point for tag point inIndex of
    // inModel (provisional name)
    void Add(GEModel* inModel, unsigned long inIndex);

    std::vector<EETagPoint*> mPoints;       // +0x00
};

// The materials of one model part that take the player colour (EEGraphicObject::mColorGroups; SetGroupColor 0xd9008
// sets each one's colour; the dtor 0xd7568 deletes them: no vptr). sizeof 0x14. Names are provisional
struct EEColorGroup {
    GEModel* mModel;                        // +0x00
    std::vector<long> mMaterials;           // +0x04 (indices into the model's materials)
};

class EEGraphicObject : public EEMedia {
public:
    EEGraphicObject();                      // 0xd7354 (LoadTable 0xd7833 news 0x4f0 bytes)
    virtual ~EEGraphicObject();             // 0xd7568 (??_G 0xd754c)
    // 0xd85ed (vtable 0x43f788 +0x04; +0x08 is UnloadMedia): loads the models, textures and sounds on the first use
    virtual long LoadMedia();
    // 0xd8bb2 (ret 0xc): the tag points and colour groups of model part inPart (LoadMedia passes each loaded part
    // model) (provisional name)
    void CollectTagPoints(unsigned long inPart, GEModel* inModel, const GETransformation& inParent);
    // 0xd907e (vtable 0x43f788 +0x08: the EEMedia override): releases the loaded models, textures and sounds when the
    // load count is 1 (provisional name)
    virtual long UnloadMedia();
    // 0xd76a1: the part scan of inModel under inParent (turret parts, orbitals, wheels; recursive) (provisional name)
    void CollectParts(GEModel* inModel, const GETransformation& inParent);
    static long LoadTable(FSFile* inFile); // original-backed 0xd7833; caller1414a3, FSFile IO proof
    // +0x004 not decoded (the ctor 0xd7354: vectors at +0x04/+0x14/+0x24, 14 more at +0x34, a map at +0x14c, ...)
    std::vector<EETurretPart*> mTurretParts; // actual original third-party-free record/vector at+4
    std::vector<EEOrbital*> mOrbitals;      // +0x014 (EEE3DDisease copies each) (provisional name)
    // 0x138e4b (ICF-folded with EECalamityManager::GetCalamityClassCount): mOrbitals.size() (provisional name)
    unsigned long GetOrbitalCount() const { return this->mOrbitals.size(); }
    unsigned long GetWheelCount() const;    // 0x200292 (out-of-line COMDAT, defined in EEComplexUnit.cpp): mWheels.size() (provisional name)
    // inline (EEE3DDisease::Load expands it with the const operator[]: lea, then the load; Virtual30 calls the const
    // begin() and operator+ out of line) (provisional name)
    EEOrbital* GetOrbital(unsigned long inIndex) const { return this->mOrbitals[inIndex]; }
    // inline; out of line after their first user EEUnitClass::TurretsFacePoint (0x23a59b / 0x23a5ad, in this order)
    // (provisional names)
    GEModel* GetTurretPartModel(unsigned long inIndex) const { return this->mTurretParts[inIndex]->mModel; }
    const GETransformation& GetTurretPartTransformation(unsigned long inIndex) const { return this->mTurretParts[inIndex]->mTransformation; }
    // The members below were EEUnit.h's `EEModel` (what EEUnit/EEComplexUnit::Virtual54 return: the same object;
    // EEModel is now a typedef of this class)
    // 0xd84ec: the tag-point list of the model part inPart (its +0x244 model) for the point kind inKind (0x12: the
    // blast wave's and the shield's; null without one) (provisional name)
    EETagPointList* GetTagPoints(unsigned long inPart, long inKind);
    // 0xd85aa: the index of inModel among the parts (0: found; 3: not) (provisional name)
    long FindTurretPart(GEModel* inModel, unsigned long& outIndex);
    // 0xd8546: the model point (EEE3DTracers ctor: the target unit's) (provisional name)
    const GE3DPoint& PickPoint();
    // the part count (ICF-folded into 0x91f11, a vector size; EEUnitClass 0x2364f3) (provisional name)
    unsigned long GetNumParts() const { return this->mTurretParts.size(); }
    // 0xd9008 (ret 8): inColor into the colour groups of model part inPart (EEROUnits::DrawUnitModel passes the render
    // unit's +0x40 colour) (provisional name)
    void SetGroupColor(const GEColor& inColor, unsigned long inPart);
    // 0xd8b09 (ret 0x10): loads inName into ioTexture when it is still empty and sets its stage modes; the loader
    // 0xd85ed passes mIsDrawn as the fourth argument, which the body does not read (provisional name)
    void LoadTexture(const UString& inName, GETexture*& ioTexture, const GETextureDescription& inDescription,
                 bool inUnused);

    // Third original vector, ctor0xd7354; model replacement0x1ff928 copies EEWheel elements.
    std::vector<EEWheel*> mWheels;         // +0x024 (0x10 bytes, source identity provisional)
    std::vector<EEColorGroup*> mColorGroups[14];    // +0x034 per model part (SetGroupColor; the dtor) (provisional name)
    // +0x114: the tag-point lists per model part (EEE3DTracers ctor 0xfd46e / EEE3DPulseCannonBall ctor 0xf6af0 index
    // it by the complex unit's mFirstFrame2; the ctor's 14-iteration loop zeroes it) (provisional name)
    EETagPointList* mTagPoints[14];
    // +0x14c: the tag-point lists by point kind, then by model (GetTagPoints 0xd84ec; the dtor destroys it after mPoints)
    // (provisional name)
    std::map<long, std::map<const GEModel*, EETagPointList*>*> mTagPointMaps;
    // +0x158: the default point (a GE3DPoint: the tag-point users Apply() it when there is no list)
    GE3DPoint mDefaultPoint;                // +0x158 (PickPoint returns it when the point list is empty; the ctor
                                            // 0xd7354's three fldz; EEE2DSmokeMuzzleBlast 0xe6664 copies it)
                                            // (provisional name)
    std::vector<GE3DPoint> mPoints;         // +0x164 (PickPoint 0xd8546: _First +0x168, _Last +0x16c, 0xc each) (provisional name)
    GE3DPoint mTopPoint;                    // +0x174 (EEE3DCommandConfirmation::Step 0xea5f9 copies it) (provisional name)
    // +0x180: the bars' offset from the object (EEROUnits 0x1147f8/0x1157b6 scale each by the unit's size and project
    // it) (provisional names)
    GE3DPoint mBar;                         // +0x180 (CollectTagPoints 0xd8bb2 passes &+0x180 to
                                            // GETransformation::Apply as the "status bar" point's out-arg)
    UString* mFileName;                     // +0x18c (EEDumpGraphicObjectTable 0xd7acf prints it as the filename; the dtor deletes it) (provisional name)
    // +0x190..+0x20c: the names the loader 0xd85ed loads from (each tested by GetLength(); the dtor deletes them)
    // (provisional names)
    UString* mTextureName;                  // +0x190 (texture name for mTexture)
    UString* mShadowModelName;              // +0x194 (model name for mShadowModel)
    UString* mShadowTextureName;            // +0x198 (texture name for mShadowTexture)
    UString* mModelNames[14];               // +0x19c (per part: mModels)
    UString* mTextureNames[14];             // +0x1d4 (per part: mTextures)

    // +0x20c: one texture per screen (EEE2DMarchingIcons::Queue indexes it by gScreenIndex, Virtual18 takes [0]);
    // 14 entries (the ctor's 14-iteration loop zeroes +0x20c+4i together with 12 other per-screen arrays)
    GETexture* mTextures[14];
    // +0x244: 14 models (the ctor's 14-iteration loop 0xd7433 zeroes +0x244+4i with mTextures; EEE3DTeleport's ctor
    // reads [0]'s bounding box; EEE3DAdditivePass::DrawPass5 indexes it by the unit's +0x264) (provisional name)
    GEModel* mModels[14];
    GEModel* mModel;                        // +0x27c (the 3D effects draw it: EEE3DBlastWave::DrawPass6 0xe9187)
                                            // (provisional name)
    GETexture* mTexture;                    // +0x280 (EEROUnits::DrawUnitModel sets it on mModel's material 0 for the
                                            // unit's second object) (provisional name)
    GEModel* mShadowModel;                  // +0x284 (0: the static quad; EEROGroundShadows 0x1122bb) (provisional name)
    GETexture* mShadowTexture;              // +0x288 (EEROGroundShadows 0x1122bb) (provisional name)
    EESoundObject* mSounds1[14];   // actual original ctor zero loop and sample-ID reader
    EESoundObject* mSounds2[14];   // +0x2c4
    long mL2FC;                             // +0x2fc (the loader 0xd7b37 copies record +0x29c) (provisional name)
    unsigned char _gap300[4];               // +0x300 not decoded
    unsigned long mKind;                    // +0x304 (SortMediaByKind 0x4d70b7: cmp 0x48; jae, unsigned; EEGraphicEffect::SortMediaByKind files the object under it)
    float mMipMapBias;                      // +0x308 (0xd8b09: GETexture::SetStageMipMapBias) (provisional name)
    float mOpacity;                         // +0x30c (EEROUnits' model pass 0x115674 passes it to Draw) (provisional name)
    float mAlphaReference;                  // +0x310 (ctor 0; the 2D effects' alpha-test reference value) (provisional name)
    // +0x314..: the animation the 3D effects run (EEE3DDisease::SetUp/Virtual4C) (provisional names)
    long mMediaIndex;                       // +0x314 (EEComplexDoppelganger::Save 0x1f23bd writes it; RepairPointers 0x1f2329: EEMedia::Get) (provisional name)
    long mL318;                             // +0x318 (ctor 0) (provisional name)
    long mL31C;                             // +0x31c (ctor 0)
    long mL320;                             // +0x320 (ctor 0)
    long mNumMipLevelsToDrop;               // +0x324 (ctor 0; 0xd85ed adds gTextureMipLevelsToDrop)
    unsigned long mHoldTimes[14];           // +0x328 by unit state (EEUnitClass::Virtual74 action 5000: GetRandom(0, [state])) (provisional name)
    // +0x360..+0x4b0: six per-model-part animation arrays (the ctor's 14-iteration loop zeroes them; the 2D effects
    // read [0], the 3D effects [13]) (provisional names)
    unsigned long mStartFrames[14];         // +0x360 (the frame at which the sound starts; [13] the first frame)
    unsigned long mLastFrames[14];          // +0x398 (the 2D effects compare the new frame `jbe` against it)
    unsigned long mFrameTimes[14];          // +0x3d0 (the time per frame)
    unsigned long mSound1Frames[14];        // +0x408 (EEUGAttackGround/EEUGAttackUnit::Step index it by set)
    unsigned long mSound2Frames[14];        // +0x440
    unsigned long mFrameCounts[14];         // +0x478 (the frame count)
    long mL4B0;                             // +0x4b0 (ctor 0) (provisional name)
    long mL4B4;                             // +0x4b4 (ctor 0)
    unsigned long mMediaCount;                       // +0x4b8 (0: EEGraphicEffect::RecordNotPreloaded records the effect's id)
    unsigned long mMaxMipLevelsToDrop;      // +0x4bc (ctor 2; 0xd85ed caps the mip level count with it) (provisional name)
    bool mIsSpeedScaled[14]; // actual independent14-element animation flags
    bool mB4CE; // +4ce, original explicit zero
    bool mIsDrawn;                          // +0x4cf (EEROUnits::DrawUnitModel draws the object only when set)
                                            // (provisional name)
    bool mHasAlpha;                         // +0x4d0
    bool mLoops[14];                        // +0x4d1 per model part (EEE3DFarm::Step 0xf1043 skips the sway when set)
    bool mSharesFrames[14]; // +4df, actual producer loop and reader
    bool mCreateMipMaps; // original initialized true
    volatile bool mIsLoading; // original initialized false; a load is under way (spun on with Sleep(1))

};

#endif
