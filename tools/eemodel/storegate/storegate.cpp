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
#include "rust/render/ge.h"

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

static long sStoreLevel = 0;   // the MIP level of the surface ee_dx7sf_new makes next

StoreSurface::StoreSurface(GESurface* inOwner, GEScreen* inScreen, const GESurfaceDescription& inDescription, long inLevel)
    : mOwner(inOwner), mScreen(inScreen), mDescription(inDescription), mNext(NULL), mLocked(false), mLockCount(0),
      mLevel(inLevel)
{
    unsigned long theBytes = mDescription.mPixelDepth / 8;
    mBytesPerRow = mDescription.mWidth * theBytes + 8;   // 8 bytes past each row: a loader's row skip shows
    mPixels.assign((size_t)mBytesPerRow * mDescription.mHeight + 1, 0);
    if (mDescription.mType == kSurfaceMipMappedTexture && inLevel + 1 < mDescription.mMipLevelCount) {
        GESurfaceDescription theNext = mDescription;
        theNext.mWidth = mDescription.mWidth > 1 ? mDescription.mWidth / 2 : 1;
        theNext.mHeight = mDescription.mHeight > 1 ? mDescription.mHeight / 2 : 1;
        sStoreLevel = inLevel + 1;
        mNext = new GESurface(NULL, theNext, inScreen);
        sStoreLevel = 0;
    }
}

StoreSurface::~StoreSurface()
{
    if (mNext)
        mNext->Dereference();
}

// A logical rectangle -> pixel bounds within the surface
static void StorePixelRect(const StoreSurface* inSurface, const float* inRect, long& outLeft, long& outTop,
                           long& outRight, long& outBottom)
{
    const GERectangle& theRect = *reinterpret_cast<const GERectangle*>(inRect);
    long theShift = (long)inSurface->mOwner->GetLogicalShift();
    outLeft = (long)theRect.GetLeft() << theShift;
    outTop = (long)theRect.GetTop() << theShift;
    outRight = (long)theRect.GetRight() << theShift;
    outBottom = (long)theRect.GetBottom() << theShift;
}

// Copies a rectangle into another, nearest pixel (inBox: the average of the source block per byte instead)
static long StoreCopy(StoreSurface* inSource, void* inTarget, const float* inFrom, const float* inTo, bool inBox)
{
    StoreSurface* theTarget = inTarget ? StoreSurface::Of(static_cast<GESurface*>(inTarget)) : NULL;
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

// The surface's and the screen's Rust functions as the memory objects (the members GESurface and GEScreen call; the
// link takes these before the Rust library's, and storegate.py checks that its duplicates are exactly these). The
// ones the store never reaches answer zero.
#define SS(p) reinterpret_cast<StoreSurface*>(p)
extern "C" {
EeDx7Surface* ee_dx7sf_new(EeDx7ObjHostFn, void* inOwner, void*, const EeDx7SurfaceDescription* inDescription, void* inParent)
{
    return reinterpret_cast<EeDx7Surface*>(new StoreSurface(static_cast<GESurface*>(inOwner), static_cast<GEScreen*>(inParent),
                                                            *reinterpret_cast<const GESurfaceDescription*>(inDescription), sStoreLevel));
}
void ee_dx7sf_destroy(EeDx7Surface* p) { delete SS(p); }
int32_t ee_dx7sf_attach(EeDx7Surface*, void*) { return kErrorFailed; }
int32_t ee_dx7sf_clear(EeDx7Surface*, const float*, const float*, float) { return 0; }
int32_t ee_dx7sf_copy_to(EeDx7Surface* p, void* inTarget, const float* inSource, const float* inDestination, bool)
    { return StoreCopy(SS(p), inTarget, inSource, inDestination, false); }
int32_t ee_dx7sf_copy_with_filter(EeDx7Surface* p, void* inTarget, const float* inSource, const float* inDestination)
    { return StoreCopy(SS(p), inTarget, inSource, inDestination, true); }
int32_t ee_dx7sf_detach(EeDx7Surface*, unsigned long) { return kErrorFailed; }
int32_t ee_dx7sf_flip(EeDx7Surface*, bool) { return 0; }
unsigned long ee_dx7sf_allocated_size(EeDx7Surface* p)
    { return (unsigned long)SS(p)->mPixels.size() + (SS(p)->mNext ? SS(p)->mNext->GetAllocatedSize() : 0); }
int32_t ee_dx7sf_lock(EeDx7Surface* p) { SS(p)->mLocked = true; SS(p)->mLockCount++; return kErrorOK; }
int32_t ee_dx7sf_unlock(EeDx7Surface* p) { SS(p)->mLocked = false; return kErrorOK; }
int32_t ee_dx7sf_restore(EeDx7Surface*) { return kErrorOK; }
void ee_dx7sf_set_transparency_color(EeDx7Surface* p, const float* inColor)
    { SS(p)->mTransparency = *reinterpret_cast<const GEColor*>(inColor); }
int32_t ee_dx7sf_text_lock(EeDx7Surface*, bool) { return kErrorOK; }
float ee_dx7sf_render_scale(EeDx7Surface*) { return 1.0f; }
unsigned long ee_dx7sf_logical_size(EeDx7Surface* p, unsigned long, unsigned long inShift, bool inHeight)
    { return (inHeight ? SS(p)->mDescription.mHeight : SS(p)->mDescription.mWidth) >> inShift; }
const EeDx7SurfaceDescription* ee_dx7sf_description(EeDx7Surface* p)
    { return reinterpret_cast<const EeDx7SurfaceDescription*>(&SS(p)->mDescription); }
const float* ee_dx7sf_transparency(EeDx7Surface* p) { return reinterpret_cast<const float*>(&SS(p)->mTransparency); }
void* ee_dx7sf_attached(EeDx7Surface* p, unsigned long inIndex) { return inIndex == 0 ? SS(p)->mNext : NULL; }
unsigned long ee_dx7sf_attached_count(EeDx7Surface* p) { return SS(p)->mNext ? 1 : 0; }
void* ee_dx7sf_gx(EeDx7Surface*) { return NULL; }
void* ee_dx7sf_parent(EeDx7Surface* p) { return SS(p)->mScreen; }
uint8_t* ee_dx7sf_memory(EeDx7Surface* p) { return SS(p)->mLocked ? SS(p)->mPixels.data() : NULL; }
unsigned long ee_dx7sf_bytes_per_row(EeDx7Surface* p) { return SS(p)->mBytesPerRow; }
bool ee_dx7sf_text_locked(EeDx7Surface*) { return false; }

EeDx7Screen* ee_dx7sc_new(EeDx7ObjHostFn, void* inOwner, void*, void*, const void*, bool, bool)
    { return reinterpret_cast<EeDx7Screen*>(new StoreScreen{static_cast<GEScreen*>(inOwner)}); }
void ee_dx7sc_start(EeDx7Screen*) {}
void ee_dx7sc_destroy(EeDx7Screen* p) { delete reinterpret_cast<StoreScreen*>(p); }
int32_t ee_dx7sc_create_surface(EeDx7Screen* p, EeDx7SurfaceDescription* inDescription, void** outSurface)
{
    const GESurfaceDescription& theDescription = *reinterpret_cast<const GESurfaceDescription*>(inDescription);
    if (theDescription.mWidth == 0 || theDescription.mHeight == 0 || theDescription.mWidth > 65536
        || theDescription.mHeight > 65536) {
        *outSurface = NULL;
        return kErrorFailed;
    }
    *outSurface = new GESurface(NULL, theDescription, reinterpret_cast<StoreScreen*>(p)->mOwner);
    return kErrorOK;
}
int32_t ee_dx7sc_capture(EeDx7Screen*, void*, bool) { return 0; }
int32_t ee_dx7sc_release(EeDx7Screen*, void*) { return 0; }
const float* ee_dx7sc_desktop_rectangle(EeDx7Screen*) { static float s[4]; return s; }
int32_t ee_dx7sc_restore_all(EeDx7Screen*) { return 0; }
int32_t ee_dx7sc_set_mode(EeDx7Screen*, unsigned long) { return 0; }
const EeDx7VideoMode* ee_dx7sc_mode(EeDx7Screen*, unsigned long) { static EeDx7VideoMode s; return &s; }
const EeDx7VideoMode* ee_dx7sc_current_mode(EeDx7Screen*) { static EeDx7VideoMode s; return &s; }
unsigned long ee_dx7sc_current_mode_index(EeDx7Screen*) { return 0; }
unsigned long ee_dx7sc_mode_count(EeDx7Screen*) { return 0; }
const void* ee_dx7sc_name(EeDx7Screen*) { static UString s; return &s; }
void* ee_dx7sc_direct_draw(EeDx7Screen*) { return NULL; }
bool ee_dx7sc_flag(EeDx7Screen*, uint32_t) { return false; }
}

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
static std::vector<EeGeSharedFrame> StoreSharing(int inMode)
{
    std::vector<EeGeSharedFrame> theFrames;
    for (unsigned long f = 1; f < 512 && inMode > 0; f++)
        if (inMode == 2 || f % 2 == 1)
            theFrames.push_back({(uint32_t)f, -1});
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
    ee_ge_store_capture(true);   // the arrays' shadows (digest.cpp)
    FILE* theOut = fopen(argv[2], "w");
    if (!theOut) {
        fprintf(stderr, "cannot write %s\n", argv[2]);
        return 2;
    }

    FSInitialize(UString((theData + "\\").c_str()));
    FSPath* thePath = new FSPath;
    FSFileSpec theDirectory;   // the startup volume and directory: the Data folder
    thePath->AddDirectory(theDirectory, true);
    GERasterizer* theRasterizer = new GERasterizer(UString("storegate"));   // no device (LLE round 12)
    theRasterizer->Activate();
    GEScreen* theScreen = new GEScreen(NULL, NULL, UString("storegate"), false, false);

    unsigned long theCount = 0;
    if (theModels) {
        std::vector<std::string> theStems = StoreStems(theData + "\\Models", {".gltf", ".glb"}, false);
        for (int theMode = 0; theMode < 3; theMode++) {
            GEModelManager* theManager = new GEModelManager(thePath);
            for (size_t i = 0; i < theStems.size(); i += theSubset) {
                std::vector<EeGeSharedFrame> theShared = StoreSharing(theMode);
                StoreTrace(theStems[i]);
                GEModel* theModel = theManager->LoadModel(UString(("models\\" + theStems[i]).c_str()),
                                                          EeGeSharedFrames{theShared.data(), theShared.size()});
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
