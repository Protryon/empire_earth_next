// New (port), TEST ONLY: RFIL, the render fill's capture (ee/EERenderCapture.h; tools/rfil.py). Called by
// EERenderObject::FillRenderLists on the graphics thread under the world's read lock; reads the render lists and
// the frame's globals, writes nothing but its own file.
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/GETerrainMesh.h"
#include "lle/GESurface.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "rust/base/math.h"
#include "rust/render/ge.h"
#include "rust/render/gfx.h"
#include "rust/sim/units.h"
#include "lle/GEPolygonArray.h"
#include "lle/GELine.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GELight.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
#include "lle/GEText.h"
#include "lle/GERectangle.h"
#include "lle/UIManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderLists.h"
#include "ee/EEPhysics.h"
#include "ee/EERenderCapture.h"

extern std::vector<unsigned long> gU4E9230;   // EERenderObject.cpp: the rectangle's picked unit ids
extern EERenderUnit* gU529044;               // the picked enemy's render unit
extern UWideString gU529030;                 // its owner's name

namespace {

FILE* CaptureFile()
{
    // opened once, at the first fill (the game changes into its data root first: the path is absolute)
    static FILE* sFile = []() -> FILE* {
        const char* thePath = std::getenv("EE_RENDER_CAPTURE");
        if (thePath == NULL || thePath[0] == 0)
            return NULL;
        return std::fopen(thePath, "wb");
    }();
    return sFile;
}

const uint64_t kFnvBasis = 14695981039346656037ull;

uint64_t Fnv(const void* inData, size_t inSize, uint64_t inHash)
{
    const unsigned char* theBytes = static_cast<const unsigned char*>(inData);
    for (size_t i = 0; i < inSize; i++) {
        inHash ^= theBytes[i];
        inHash *= 1099511628211ull;
    }
    return inHash;
}

uint32_t Text32(const char* inText)
{
    if (inText == NULL)
        return 0;
    return (uint32_t)Fnv(inText, std::strlen(inText), kFnvBasis);
}

// The record being written and the frame's pointer names: a pointer is its order of first use in this record
// (high word) and its content's name (low word: a graphic object's file, a model's name, a class's type)
struct Writer {
    std::vector<unsigned char> mData;
    std::map<const void*, uint32_t> mOrder;

    void Bytes(const void* inData, size_t inSize)
    {
        const unsigned char* theBytes = static_cast<const unsigned char*>(inData);
        mData.insert(mData.end(), theBytes, theBytes + inSize);
    }
    void U32(uint32_t inValue) { this->Bytes(&inValue, 4); }
    void I32(int32_t inValue) { this->Bytes(&inValue, 4); }
    void F32(float inValue) { this->Bytes(&inValue, 4); }
    void U64(uint64_t inValue) { this->Bytes(&inValue, 8); }
    uint32_t Order(const void* inPointer)
    {
        if (inPointer == NULL)
            return 0;
        std::map<const void*, uint32_t>::iterator theFound = mOrder.find(inPointer);
        if (theFound != mOrder.end())
            return theFound->second;
        uint32_t theOrder = (uint32_t)mOrder.size() + 1;
        mOrder[inPointer] = theOrder;
        return theOrder;
    }
};

// A digest of fields in their order (each field's bytes)
struct Digest {
    uint64_t mHash = kFnvBasis;
    Writer* mWriter;
    explicit Digest(Writer* inWriter) : mWriter(inWriter) {}
    void Bytes(const void* inData, size_t inSize) { mHash = Fnv(inData, inSize, mHash); }
    void U32(uint32_t inValue) { this->Bytes(&inValue, 4); }
    void F32(float inValue) { this->Bytes(&inValue, 4); }
    void Floats(const void* inData, size_t inSize) { this->Bytes(inData, inSize); }
    void Pointer(const void* inPointer, uint32_t inName)
    {
        uint32_t theOrder = mWriter->Order(inPointer);
        this->U32(theOrder);
        this->U32(inPointer != NULL ? inName : 0);
    }
    void Object(const EEGraphicObject* inObject)
    {
        this->Pointer(inObject, inObject != NULL && inObject->mFileName != NULL ? Text32(*inObject->mFileName) : 0);
    }
    void Model(const GEModel* inModel)
    {
        this->Pointer(inModel, inModel != NULL ? Text32(inModel->GetName()) : 0);
    }
    void Class(const EEUnitClass* inClass)
    {
        this->Pointer(inClass, inClass != NULL ? (uint32_t)const_cast<EEUnitClass*>(inClass)->GetUnitType() : 0);
    }
    void Texture(const GETexture* inTexture) { this->Pointer(inTexture, 0); }
    void Text(const char* inText, unsigned long inBytes)
    {
        this->U32((uint32_t)inBytes);
        if (inText != NULL)
            this->Bytes(inText, inBytes);
    }
    void FloatVector(const std::vector<float>& inValues)
    {
        this->U32((uint32_t)inValues.size());
        if (!inValues.empty())
            this->Bytes(inValues.data(), inValues.size() * 4);
    }
    void ModelVector(const std::vector<GEModel*>& inModels)
    {
        this->U32((uint32_t)inModels.size());
        for (GEModel* theModel : inModels)
            this->Model(theModel);
    }
};

uint64_t UnitDigest(Writer& ioWriter, const EERenderUnit& inUnit)
{
    Digest d(&ioWriter);
    d.Floats(&inUnit.mTransformation, sizeof(inUnit.mTransformation));
    d.Floats(&inUnit.mPlayerColor, sizeof(inUnit.mPlayerColor));
    d.Floats(&inUnit.mTintColor, sizeof(inUnit.mTintColor));
    d.FloatVector(inUnit.mHealthBars);
    d.FloatVector(inUnit.mTurretAngles);
    d.ModelVector(inUnit.mTurretModels);
    d.FloatVector(inUnit.mOrbitalAngles);
    d.ModelVector(inUnit.mOrbitalModels);
    d.FloatVector(inUnit.mWheelAngles);
    d.ModelVector(inUnit.mWheelModels);
    d.Text((const char*)inUnit.mName, inUnit.mName.GetTCHARLength());
    d.Class(inUnit.mInfo);
    d.Object(inUnit.mSource);
    d.Object(inUnit.mSource2);
    d.F32(inUnit.mScale);
    d.F32(inUnit.mSelectionSize);
    d.F32(inUnit.mShadowScale);
    d.F32(inUnit.mFF4);
    d.F32(inUnit.mGroundZ);
    d.F32(inUnit.mOpacity);
    d.F32(inUnit.mHealth);
    d.F32(inUnit.mFraction);
    d.F32(inUnit.mSubModelOffsetScale);
    d.F32(inUnit.mFogScale);
    d.U32((uint32_t)inUnit.mCategory);
    d.U32((uint32_t)inUnit.mUnitClassIndex);
    d.U32((uint32_t)inUnit.mL118);
    d.U32((uint32_t)inUnit.mCommandType);
    d.U32((uint32_t)inUnit.mCurrentResearch);
    d.U32((uint32_t)inUnit.mAnimation);
    d.U32((uint32_t)inUnit.mGridSize);
    d.U32((uint32_t)inUnit.mCurrentFrame);
    d.U32((uint32_t)inUnit.mNextFrame);
    d.U32((uint32_t)inUnit.mL134);
    d.U32((uint32_t)inUnit.mUnitId);
    d.U32((uint32_t)inUnit.mPlayerID);
    d.U32((uint32_t)inUnit.mAreaEffectStrength);
    d.U32((inUnit.mCanPlace ? 1u : 0u) | (inUnit.mIsFogged ? 2u : 0u) | (inUnit.mIsTinted ? 4u : 0u)
          | (inUnit.mHasSubModelOffset ? 8u : 0u) | (inUnit.mSecondFullOpacity ? 16u : 0u));
    return d.mHash;
}

// every field but the sway fraction (the sway clock runs on wall time; the fraction is written beside the digest)
uint64_t ShadowDigest(Writer& ioWriter, const EERenderShadow& inShadow)
{
    Digest d(&ioWriter);
    d.Floats(&inShadow.mLocation, sizeof(inShadow.mLocation));
    d.Object(inShadow.mObject);
    d.F32(inShadow.mModelScale);
    d.F32(inShadow.mSelectionSize);
    d.F32(inShadow.mScale);
    d.F32(inShadow.mF1C);
    d.F32(inShadow.mHealth);
    d.F32(inShadow.mOpacity);
    d.U32((uint32_t)inShadow.mModelIndex);
    d.U32((uint32_t)inShadow.mCurrentFrame);
    d.U32((uint32_t)inShadow.mNextFrame);
    d.U32(inShadow.mIsFogged ? 1u : 0u);
    return d.mHash;
}

template <class T>
int32_t PoolIndex(const T* inEntry, const T* inPool, unsigned long inCount)
{
    if (inEntry == NULL)
        return -1;
    if (inEntry < inPool || inEntry >= inPool + inCount)
        return -2;
    return (int32_t)(inEntry - inPool);
}

// a texture map: its size, then (the key's name, the entry's pool index) by key, each key's entries in the map's
// order. The map orders its keys by address, which differs between runs: a key is named by its lowest pool index
// among its entries, and the keys are written in that order.
template <class Map, class T>
void TextureMap(Writer& ioWriter, const Map& inMap, const T* inPool, unsigned long inCount)
{
    std::vector<std::pair<int32_t, int32_t> > theEntries;   // (the key's name, the pool index)
    std::vector<const void*> theKeys;
    std::map<const void*, int32_t> theNames;
    for (typename Map::const_iterator it = inMap.begin(); it != inMap.end(); ++it) {
        int32_t theIndex = PoolIndex(it->second, inPool, inCount);
        std::map<const void*, int32_t>::iterator theName = theNames.find(it->first);
        if (theName == theNames.end())
            theNames[it->first] = theIndex;
        else if (theIndex < theName->second)
            theName->second = theIndex;
        theKeys.push_back(it->first);
        theEntries.push_back(std::make_pair(0, theIndex));
    }
    for (size_t k = 0; k < theEntries.size(); k++)
        theEntries[k].first = theNames[theKeys[k]];
    std::stable_sort(theEntries.begin(), theEntries.end(),
                     [](const std::pair<int32_t, int32_t>& a, const std::pair<int32_t, int32_t>& b) { return a.first < b.first; });
    ioWriter.U32((uint32_t)theEntries.size());
    for (const std::pair<int32_t, int32_t>& theEntry : theEntries) {
        ioWriter.I32(theEntry.first);
        ioWriter.I32(theEntry.second);
    }
}

// The fill's world, as far as two runs can differ in it (the frames' second key): every tile of the visible rows
// (its sight and remembered units) and every unit on them (its id, hit points, resource, and a complex unit's
// location, frame, animation, command, build state and selection), the current player and its viewed units. Two runs
// of the same seed reach the same world at a tick unless the harness's input changed it (a selection, an order).
uint64_t WorldKey()
{
    uint64_t theHash = kFnvBasis;
    auto theAdd = [&theHash](uint32_t inValue) { theHash = Fnv(&inValue, 4, theHash); };
    EEMap* theMap = gWorld.mMap;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    theAdd((uint32_t)thePlayer->GetPlayerID());
    theAdd(thePlayer->mViewedUnit != NULL ? (uint32_t)thePlayer->mViewedUnit->GetUnitID() : 0u);
    theAdd(thePlayer->mSelectedViewedUnit != NULL ? (uint32_t)thePlayer->mSelectedViewedUnit->GetUnitID() : 0u);
    unsigned long theRows = theMap->mTerrainMesh.GetNumVisibleTileRows();
    for (unsigned long theRow = 0; theRow < theRows; theRow++) {
        long theY;
        long theFirst;
        long theLast;
        theMap->mTerrainMesh.GetVisibleTileRow(theRow, theY, theFirst, theLast);
        for (long theX = theFirst; theX <= theLast; theX++) {
            EETile* theTile = theMap->GetTile(theX, theY);
            theAdd((uint32_t)theX);
            theAdd((uint32_t)theY);
            theAdd((theTile->IsVisibleTo(thePlayer) ? 1u : 0u) | (theTile->IsSeenBy(thePlayer) ? 2u : 0u));
            theAdd((uint32_t)theTile->GetNumDoppelgangers());
            for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                EEUnit* theUnit = theNode->mUnit;
                theAdd((uint32_t)theUnit->GetUnitID());
                theAdd((uint32_t)theUnit->mHitPoints);
                theAdd((uint32_t)theUnit->mResourceAmount);
                EEComplexUnit* theComplex = theUnit->mComplexUnit;
                if (theComplex == NULL)
                    continue;
                const GETransformation& theLocation = theComplex->mPhysicalModel.GetLocation();
                theHash = Fnv(&theLocation, sizeof(theLocation), theHash);
                theAdd((uint32_t)theComplex->mPhysicalModel.GetFrame());
                theAdd((uint32_t)theComplex->mNextFrame);
                theHash = Fnv(&theComplex->mAnimationFraction, 4, theHash);
                theAdd((uint32_t)theComplex->mAnimation);
                theAdd(theComplex->mCommand != NULL ? (uint32_t)theComplex->mCommand->mType : 0u);
                theAdd((theComplex->mIsBuilt ? 1u : 0u) | (thePlayer->mSelectedGroup->Contains(theComplex) ? 2u : 0u));
            }
        }
    }
    return theHash;
}

}   // namespace

bool EERenderCaptureActive()
{
    return CaptureFile() != NULL;
}

void EERenderCaptureFill(const EERenderFrameInfo& inInfo, uint32_t inSwayClock, float inUnitCount)
{
    FILE* theFile = CaptureFile();
    if (theFile == NULL)
        return;
    const EERenderLists& l = EERenderLists::sInstance;
    Writer w;
    // the keys
    w.U32((uint32_t)inInfo.mFrame);
    w.U32(gPhysics != NULL ? (uint32_t)gPhysics->mTicksRun : 0u);
    w.U32(inSwayClock);
    w.F32(inInfo.mYaw);
    w.U64(Fnv(&gWorld.mTransformation, sizeof(gWorld.mTransformation), kFnvBasis));
    w.U32((gFreeCamera ? 1u : 0u) | (gOrthographicView ? 2u : 0u) | (inInfo.mAmbient ? 4u : 0u));
    w.I32(inInfo.mPickedUnit != NULL ? (int32_t)inInfo.mPickedUnit->GetUnitID() : -1);
    w.U64(WorldKey());
    w.F32(inInfo.mNumFightingUnits);
    w.F32(inUnitCount);
    // the pools
    w.U32((uint32_t)l.mRenderUnitCount);
    for (unsigned long i = 0; i < l.mRenderUnitCount; i++) {
        const EERenderUnit& u = l.mRenderUnits[i];
        w.U32((uint32_t)u.mUnitId);
        w.F32(u.mOpacity);
        w.U64(UnitDigest(w, u));
    }
    w.U32((uint32_t)l.mRenderShadowCount);
    for (unsigned long i = 0; i < l.mRenderShadowCount; i++) {
        const EERenderShadow& s = l.mRenderShadows[i];
        w.F32(s.mOpacity);
        w.F32(s.mFraction);
        w.U64(ShadowDigest(w, s));
    }
    // the texture maps (their keys are ordered by address: the order within a key and the key's first use are kept)
    TextureMap(w, l.mUnits0, l.mRenderUnits, l.mRenderUnitCount);
    TextureMap(w, l.mUnits1, l.mRenderUnits, l.mRenderUnitCount);
    TextureMap(w, l.mUnits2, l.mRenderUnits, l.mRenderUnitCount);
    TextureMap(w, l.mShadows0, l.mRenderShadows, l.mRenderShadowCount);
    TextureMap(w, l.mShadows1, l.mRenderShadows, l.mRenderShadowCount);
    TextureMap(w, l.mShadows2, l.mRenderShadows, l.mRenderShadowCount);
    // the selected and picked lists
    w.U32((uint32_t)l.mSelectedUnits.size());
    for (const EERenderUnit* theUnit : l.mSelectedUnits)
        w.I32(PoolIndex(theUnit, l.mRenderUnits, l.mRenderUnitCount));
    w.U32((uint32_t)l.mSelectedObjects.size());
    for (const EERenderShadow* theObject : l.mSelectedObjects)
        w.I32(PoolIndex(theObject, l.mRenderShadows, l.mRenderShadowCount));
    w.I32(PoolIndex(l.mSelectedUnit, l.mRenderUnits, l.mRenderUnitCount));
    w.I32(PoolIndex(l.mSelectedObject, l.mRenderShadows, l.mRenderShadowCount));
    w.I32(PoolIndex(gU529044, l.mRenderUnits, l.mRenderUnitCount));
    w.U32(gU529044 != NULL ? Text32((const char*)gU529030) : 0u);
    w.U32((uint32_t)gU4E9230.size());
    for (unsigned long theID : gU4E9230)
        w.U32((uint32_t)theID);
    // the ghost records (the whole render unit, the grids and the tile)
    w.U32((uint32_t)l.mGhostRecords.size());
    for (const EERenderUnitRecord& r : l.mGhostRecords) {
        Digest d(&w);
        // the fields FillGhostRecords sets (the record is a local: its other scalars are indeterminate)
        const EERenderUnit& u = r.mUnit;
        d.Floats(&u.mTransformation, sizeof(u.mTransformation));
        d.Floats(&u.mPlayerColor, sizeof(u.mPlayerColor));
        d.Class(u.mInfo);
        d.Object(u.mSource);
        d.Object(u.mSource2);
        d.F32(u.mScale);
        d.F32(u.mFraction);
        d.U32((uint32_t)u.mCategory);
        d.U32((uint32_t)u.mAnimation);
        d.U32((uint32_t)u.mGridSize);
        d.U32((uint32_t)u.mCurrentFrame);
        d.U32((uint32_t)u.mNextFrame);
        d.U32((u.mCanPlace ? 1u : 0u) | (u.mIsFogged ? 2u : 0u) | (u.mIsTinted ? 4u : 0u) | (u.mHasSubModelOffset ? 8u : 0u));
        d.Floats(r.mHeights, sizeof(r.mHeights));
        d.Bytes(r.mCells, sizeof(r.mCells));
        d.U32((uint32_t)r.mTileX);
        d.U32((uint32_t)r.mTileY);
        w.U64(d.mHash);
    }
    // the ground circles
    w.U32((uint32_t)gU529050.size());
    for (const EERenderLine& theLine : gU529050) {
        Digest d(&w);
        d.U32((uint32_t)theLine.mPoints.size());
        for (const std::pair<GE3DPoint, float>& p : theLine.mPoints) {
            d.Floats(&p.first, sizeof(p.first));
            d.F32(p.second);
        }
        d.Texture(theLine.mTexture);
        d.F32(theLine.mWidth);
        w.U64(d.mHash);
    }
    uint32_t theHeader[2] = {0x4c494652u, (uint32_t)w.mData.size()};   // 'RFIL'
    std::fwrite(theHeader, 4, 2, theFile);
    std::fwrite(w.mData.data(), 1, w.mData.size(), theFile);
    std::fflush(theFile);
}
