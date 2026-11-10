// storegate (tools/eemodel/storegate.py): loads every model and texture of a data folder through the game's own
// GEModelManager / GETextureManager code (the units of the game build, linked as they are) and prints a digest of
// what each stores: the GEModel (counts, names, transforms, boxes, materials, every frame's vertex array and tag
// points, the polygon arrays, the sub-models) and the GETexture (frames, sizes, MIP levels, every surface's pixels).
// The renderer is fakes.h's memory objects. Run against two builds, the outputs must be equal; the gate keeps the
// digests of the last accepted build.
//   storegate <Data folder> <output> [--models] [--textures] [--subset N]
#include "fakes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <new>

#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/File System.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"

// Every allocation zero-filled: what a loader leaves unset reads the same in every build
void* operator new(size_t inSize)
{
    void* p = calloc(1, inSize ? inSize : 1);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void* operator new[](size_t inSize) { return operator new(inSize); }
void operator delete(void* p) noexcept { free(p); }
void operator delete[](void* p) noexcept { free(p); }
void operator delete(void* p, size_t) noexcept { free(p); }
void operator delete[](void* p, size_t) noexcept { free(p); }

// ---------------------------------------------------------------------------------------------------- surfaces --

StoreSurface::StoreSurface(GEScreen* inScreen, const GESurfaceDescription& inDescription, long inLevel)
    : mScreen(inScreen), mDescription(inDescription), mNext(NULL), mLocked(false), mLockCount(0), mLevel(inLevel)
{
    unsigned long theBytes = mDescription.mPixelDepth / 8;
    mBytesPerRow = mDescription.mWidth * theBytes + 8;   // 8 bytes past each row: a loader's row skip shows
    mPixels.assign((size_t)mBytesPerRow * mDescription.mHeight + 1, 0);
    if (mDescription.mType == kSurfaceMipMappedTexture && inLevel + 1 < mDescription.mMipLevelCount) {
        GESurfaceDescription theNext = mDescription;
        theNext.mWidth = mDescription.mWidth > 1 ? mDescription.mWidth / 2 : 1;
        theNext.mHeight = mDescription.mHeight > 1 ? mDescription.mHeight / 2 : 1;
        mNext = new StoreSurface(inScreen, theNext, inLevel + 1);
    }
}

StoreSurface::~StoreSurface()
{
    if (mNext)
        mNext->Dereference();
}

unsigned long StoreSurface::GetAllocatedSize()
{
    return (unsigned long)mPixels.size() + (mNext ? mNext->GetAllocatedSize() : 0);
}

// A logical rectangle -> pixel bounds within the surface
static void StorePixelRect(const StoreSurface* inSurface, const GERectangle& inRect, long& outLeft, long& outTop,
                           long& outRight, long& outBottom)
{
    long theShift = (long)inSurface->GetLogicalShift();
    outLeft = (long)inRect.GetLeft() << theShift;
    outTop = (long)inRect.GetTop() << theShift;
    outRight = (long)inRect.GetRight() << theShift;
    outBottom = (long)inRect.GetBottom() << theShift;
}

// Copies a rectangle into another, nearest pixel (inBox: the average of the source block per byte instead)
static long StoreCopy(StoreSurface* inSource, GESurface* inTarget, GERectangle& inFrom, GERectangle& inTo, bool inBox)
{
    StoreSurface* theTarget = dynamic_cast<StoreSurface*>(inTarget);
    if (!theTarget || theTarget->mDescription.mPixelDepth != inSource->mDescription.mPixelDepth)
        return kErrorFailed;
    long sl, st, sr, sb, dl, dt, dr, db;
    StorePixelRect(inSource, inFrom, sl, st, sr, sb);
    StorePixelRect(theTarget, inTo, dl, dt, dr, db);
    long theBytes = (long)inSource->mDescription.mPixelDepth / 8;
    long sw = sr - sl, sh = sb - st, dw = dr - dl, dh = db - dt;
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
        return kErrorOK;
    for (long y = 0; y < dh; y++) {
        long ty = dt + y;
        if (ty < 0 || ty >= (long)theTarget->mDescription.mHeight)
            continue;
        long y0 = st + y * sh / dh, y1 = inBox ? st + (y + 1) * sh / dh : y0 + 1;
        if (y1 <= y0)
            y1 = y0 + 1;
        for (long x = 0; x < dw; x++) {
            long tx = dl + x;
            if (tx < 0 || tx >= (long)theTarget->mDescription.mWidth)
                continue;
            long x0 = sl + x * sw / dw, x1 = inBox ? sl + (x + 1) * sw / dw : x0 + 1;
            if (x1 <= x0)
                x1 = x0 + 1;
            for (long k = 0; k < theBytes; k++) {
                unsigned long theSum = 0, theCount = 0;
                for (long yy = y0; yy < y1; yy++)
                    for (long xx = x0; xx < x1; xx++) {
                        if (yy < 0 || xx < 0 || yy >= (long)inSource->mDescription.mHeight
                            || xx >= (long)inSource->mDescription.mWidth)
                            continue;
                        theSum += inSource->mPixels[(size_t)yy * inSource->mBytesPerRow + xx * theBytes + k];
                        theCount++;
                    }
                theTarget->mPixels[(size_t)ty * theTarget->mBytesPerRow + tx * theBytes + k] =
                    (unsigned char)(theCount ? theSum / theCount : 0);
            }
        }
    }
    return kErrorOK;
}

long StoreSurface::CopyTo(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination, bool inTransparent)
{
    return StoreCopy(this, inTarget, inSource, inDestination, false);
}

long StoreSurface::CopyWithFilter(GESurface* inTarget, GERectangle& inSource, GERectangle& inDestination)
{
    return StoreCopy(this, inTarget, inSource, inDestination, true);
}

long StoreScreen::CreateSurface(GESurfaceDescription& inDescription, GESurface** outSurface)
{
    if (inDescription.mWidth == 0 || inDescription.mHeight == 0 || inDescription.mWidth > 65536
        || inDescription.mHeight > 65536) {
        *outSurface = NULL;
        return kErrorFailed;
    }
    *outSurface = new StoreSurface(this, inDescription, 0);
    return kErrorOK;
}

// ------------------------------------------------------------------------------------------------------ arrays --

StoreVertexArray::StoreVertexArray(unsigned long inType, unsigned long inCount)
    : mType(inType), mPoints(3 * (size_t)inCount), mNormals(3 * (size_t)inCount), mUVs(2 * (size_t)inCount),
      mColors(3 * (size_t)inCount), mOpacities(inCount), mOutOfRange(0)
{
}

void StoreVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    const StoreVertexArray* theArray = dynamic_cast<const StoreVertexArray*>(&inArray);
    if (theArray) {
        mPoints = theArray->mPoints;
        mNormals = theArray->mNormals;
        mUVs = theArray->mUVs;
        mColors = theArray->mColors;
        mOpacities = theArray->mOpacities;
    }
}

#define STORE_IN_RANGE(n) if ((size_t)inIndex >= mOpacities.size()) { n; return; }
void StoreVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const
{
    STORE_IN_RANGE(outColor = GEColor(0.0f, 0.0f, 0.0f))
    outColor = GEColor(mColors[3 * inIndex], mColors[3 * inIndex + 1], mColors[3 * inIndex + 2]);
}
void StoreVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const
{
    STORE_IN_RANGE(outNormal = GEVector(0.0f, 0.0f, 0.0f))
    outNormal = GEVector(mNormals[3 * inIndex], mNormals[3 * inIndex + 1], mNormals[3 * inIndex + 2]);
}
float StoreVertexArray::GetOpacity(unsigned long inIndex) const
{
    return (size_t)inIndex < mOpacities.size() ? mOpacities[inIndex] : 0.0f;
}
void StoreVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const
{
    STORE_IN_RANGE(outPoint = GE3DPoint(0.0f, 0.0f, 0.0f))
    outPoint = GE3DPoint(mPoints[3 * inIndex], mPoints[3 * inIndex + 1], mPoints[3 * inIndex + 2]);
}
void StoreVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    STORE_IN_RANGE(outCoordinate = GETextureCoordinate(0.0f, 0.0f))
    outCoordinate = GETextureCoordinate(mUVs[2 * inIndex], mUVs[2 * inIndex + 1]);
}
#undef STORE_IN_RANGE
#define STORE_IN_RANGE if ((size_t)inIndex >= mOpacities.size()) { mOutOfRange++; return; }
void StoreVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex)
{
    STORE_IN_RANGE
    mColors[3 * inIndex] = inColor.GetRed();
    mColors[3 * inIndex + 1] = inColor.GetGreen();
    mColors[3 * inIndex + 2] = inColor.GetBlue();
}
void StoreVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex)
{
    STORE_IN_RANGE
    mNormals[3 * inIndex] = inNormal.GetX();
    mNormals[3 * inIndex + 1] = inNormal.GetY();
    mNormals[3 * inIndex + 2] = inNormal.GetZ();
}
void StoreVertexArray::SetOpacity(float inOpacity, unsigned long inIndex)
{
    STORE_IN_RANGE
    mOpacities[inIndex] = inOpacity;
}
void StoreVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex)
{
    STORE_IN_RANGE
    mPoints[3 * inIndex] = inPoint.GetX();
    mPoints[3 * inIndex + 1] = inPoint.GetY();
    mPoints[3 * inIndex + 2] = inPoint.GetZ();
}
void StoreVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    STORE_IN_RANGE
    mUVs[2 * inIndex] = inCoordinate.GetU();
    mUVs[2 * inIndex + 1] = inCoordinate.GetV();
}
void StoreVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                                 const GETextureCoordinate& inCoordinate, float inOpacity, unsigned long inIndex)
{
    STORE_IN_RANGE
    SetColor(inColor, inIndex);
    SetPoint(inPoint, inIndex);
    SetNormal(inNormal, inIndex);
    SetTextureCoordinate(inCoordinate, inIndex);
    SetOpacity(inOpacity, inIndex);
}
#undef STORE_IN_RANGE

// -------------------------------------------------------------------------------------------------------- main --

// The stems of the files in a folder with one of the extensions (sorted, case as on disk, no duplicates)
static std::vector<std::string> StoreStems(const std::string& inFolder, const std::vector<std::string>& inExtensions,
                                           bool inSkipFrames)
{
    std::vector<std::string> theStems;
    WIN32_FIND_DATAA theData;
    HANDLE theFind = FindFirstFileA((inFolder + "\\*").c_str(), &theData);
    if (theFind == INVALID_HANDLE_VALUE)
        return theStems;
    do {
        std::string theName = theData.cFileName;
        size_t theDot = theName.rfind('.');
        if (theDot == std::string::npos)
            continue;
        std::string theExtension = theName.substr(theDot);
        for (char& c : theExtension)
            c = (char)tolower((unsigned char)c);
        if (std::find(inExtensions.begin(), inExtensions.end(), theExtension) == inExtensions.end())
            continue;
        std::string theStem = theName.substr(0, theDot);
        if (inSkipFrames && theStem.find('@') != std::string::npos)
            continue;
        theStems.push_back(theStem);
    } while (FindNextFileA(theFind, &theData));
    FindClose(theFind);
    std::sort(theStems.begin(), theStems.end());
    theStems.erase(std::unique(theStems.begin(), theStems.end()), theStems.end());
    return theStems;
}

static bool sTrace = getenv("STOREGATE_TRACE") != NULL;   // each load's name to stderr before it starts

static void StoreTrace(const std::string& inName)
{
    if (sTrace) {
        fprintf(stderr, "%s\n", inName.c_str());
        fflush(stderr);
    }
}

static void StoreLine(FILE* inOut, const char* inFormat, ...)
{
    va_list theArgs;
    va_start(theArgs, inFormat);
    vfprintf(inOut, inFormat, theArgs);
    va_end(theArgs);
    fputc('\n', inOut);
}

static const char* const kSharingNames[] = {"none", "odd", "all"};

// The frames a model shares (EEGraphicObject's maps): none, every odd frame with the one before (offset -1), every
// frame but the first with the one before
static std::map<unsigned long, long> StoreSharing(int inMode)
{
    std::map<unsigned long, long> theFrames;
    for (unsigned long f = 1; f < 512 && inMode > 0; f++)
        if (inMode == 2 || f % 2 == 1)
            theFrames[f] = -1;
    return theFrames;
}

struct StoreDescription {
    const char* mName;
    GETextureDescription mDescription;
    bool mPack;
};

int main(int argc, char** argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: storegate <Data folder> <output> [--models] [--textures] [--subset N]\n");
        return 2;
    }
    std::string theData = argv[1];
    while (!theData.empty() && (theData.back() == '\\' || theData.back() == '/'))
        theData.pop_back();
    for (char& c : theData)
        if (c == '/')
            c = '\\';
    bool theModels = false, theTextures = false;
    unsigned long theSubset = 1;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "--models"))
            theModels = true;
        else if (!strcmp(argv[i], "--textures"))
            theTextures = true;
        else if (!strcmp(argv[i], "--subset") && i + 1 < argc)
            theSubset = strtoul(argv[++i], NULL, 10);
    }
    if (!theModels && !theTextures)
        theModels = theTextures = true;
    if (theSubset == 0)
        theSubset = 1;
    FILE* theOut = fopen(argv[2], "w");
    if (!theOut) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 2;
    }

    FSInitialize(UString((theData + "\\").c_str()));
    FSPath* thePath = new FSPath;
    FSFileSpec theDirectory;   // the startup volume and directory: the Data folder
    thePath->AddDirectory(theDirectory, true);
    StoreRasterizer* theRasterizer = new StoreRasterizer;
    theRasterizer->Activate();
    StoreScreen* theScreen = new StoreScreen;

    unsigned long theCount = 0;
    if (theModels) {
        std::vector<std::string> theStems = StoreStems(theData + "\\Models", {".gltf", ".glb"}, false);
        for (int theMode = 0; theMode < 3; theMode++) {
            GEModelManager* theManager = new GEModelManager(thePath);
            for (size_t i = 0; i < theStems.size(); i += theSubset) {
                std::map<unsigned long, long> theShared = StoreSharing(theMode);
                StoreTrace(theStems[i]);
                GEModel* theModel = theManager->LoadModel(UString(("models\\" + theStems[i]).c_str()), theShared);
                if (theModel)
                    StoreLine(theOut, "model %s %s %016llx", theStems[i].c_str(), kSharingNames[theMode],
                              StoreDigestModel(theModel));
                else
                    StoreLine(theOut, "model %s %s null", theStems[i].c_str(), kSharingNames[theMode]);
                theCount++;
            }
            StoreLine(theOut, "models %s footprint %lu", kSharingNames[theMode], theManager->GetMemoryFootprint());
            theManager->RemoveAllModels();
            delete theManager;
        }
    }
    if (theTextures) {
        std::vector<std::string> theStems = StoreStems(theData + "\\Textures", {".png"}, true);
        // {volatility, frame format, depth, drop, 3D, transparency, alpha, MIP maps, pack}
        const StoreDescription kDescriptions[] = {
            {"f0-32-alpha", {1, 0, 32, 0, true, false, true, false, true}, false},
            {"f0-32-mip", {1, 0, 32, 0, true, false, false, true, true}, false},
            {"f0-16-alpha-mip-drop1", {1, 0, 16, 1, true, false, true, true, true}, false},
            {"f0-16-drop2", {1, 0, 16, 2, true, true, false, false, true}, false},
            {"f1-32-alpha", {1, 1, 32, 0, false, false, true, false, true}, false},
            {"f1-16", {1, 1, 16, 0, false, false, false, false, true}, false},
            {"f0-32-pack", {1, 0, 32, 0, true, false, true, false, true}, true},
        };
        for (const StoreDescription& theCase : kDescriptions) {
            if (theCase.mPack)
                GETextureManager::SetTexturePack("textures", 1, 1);
            GETextureManager* theManager = new GETextureManager(thePath);
            theManager->SetScreen(theScreen);
            std::vector<GETexture*> theLoaded;
            for (size_t i = 0; i < theStems.size(); i += theSubset) {
                StoreTrace(theStems[i]);
                GETexture* theTexture =
                    theManager->LoadTexture(UString(("textures\\" + theStems[i]).c_str()), theCase.mDescription);
                if (theTexture)
                    StoreLine(theOut, "texture %s %s %016llx", theStems[i].c_str(), theCase.mName,
                              StoreDigestTexture(theTexture));
                else
                    StoreLine(theOut, "texture %s %s null", theStems[i].c_str(), theCase.mName);
                theLoaded.push_back(theTexture);
                theCount++;
            }
            StoreLine(theOut, "textures %s footprint %lu count %lu", theCase.mName, theManager->GetMemoryFootprint(),
                      theManager->GetTextureCount());
            if (&theCase == &kDescriptions[0] || theCase.mPack) {
                // the cache's names lead back to the files: a reload of each, then of all
                for (size_t i = 0, k = 0; i < theStems.size(); i += theSubset, k++) {
                    if (!theLoaded[k])
                        continue;
                    long theResult =
                        theManager->ReloadTexture(UString(("textures\\" + theStems[i]).c_str()), theCase.mDescription);
                    StoreLine(theOut, "reload %s %s %ld %016llx", theStems[i].c_str(), theCase.mName, theResult,
                              StoreDigestTexture(theLoaded[k]));
                }
                long theResult = theManager->RefreshAllTextures();
                StoreLine(theOut, "refresh %s %ld %016llx", theCase.mName, theResult,
                          StoreDigestTextureManager(theManager));
            }
            theManager->RemoveAllTextures();
            delete theManager;
            if (theCase.mPack)
                GETextureManager::SetTexturePack(NULL, 0, 0);
        }
    }
    fclose(theOut);
    printf("storegate: %lu loads -> %s\n", theCount, argv[2]);
    return 0;
}
