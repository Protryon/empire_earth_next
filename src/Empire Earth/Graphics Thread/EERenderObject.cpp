// C:\EEX\Empire Earth\Graphics Thread\EERenderObject.cpp (no __FILE__; name from the RTTI class). The render
// objects' base (RTTI EERenderObject > RTSRenderObject, vtable 0x441378) and the per-frame render lists:
// 0x10aa88..0x10e899. It opens with the init chains of three classes' statics (guard bytes 0x529048, 0x528d70,
// 0x4e9454: the lines vector and a UWideString, the unit picker and the environment, the render lists; owner
// classes inferred, defined as globals here), then the unit's own globals' chains (the effect queues, colours, the
// main viewport 0x4e8f08), the ctor, ??_G, RTSRenderObject's inline dtor, and the list builders. The chains
// 0x10aa88..0x10ac30 might instead be a unit of their own (no __FILE__ either way). Unit flags: the exe's module set.
// FillUnit (0x10d3ad, 4 KB) and FillDoppelgangerUnit (0x10e3c8, the same for a doppelganger) are declared in
// ee/EERenderObject.h and called by FillRenderLists. The tail
// 0x10e899..0x111d3d (the space-cliff textures, the frame builders, the lighting set-ups, the template COMDATs and the
// inline ctors) follows the ghost-unit pass.
#include <windows.h>

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
#include "lle/GEPolygonArray.h"
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GELight.h"
#include "lle/GEBitmap.h"
#include "lle/GERhombus.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIForm.h"
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
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEUnitPicker.h"
#include "ee/EEGhostUnit.h"
#include "ee/EEDoppelganger.h"
#include "ee/RTSRegionTable.h"
#include "ee/EEEnvironmentManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEAmbientSoundManager.h"
#include "ee/EEUICSelectUnits.h"
#include "ee/EESEUICTriggerSelectUnits.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEMainGroup.h"
#include "ee/EESEUnit.h"
#include "ee/EEMusicManager.h"
#include "ee/EEPhysics.h"
#include "ee/EEMouse.h"
#include "ee/EEAreaEffectKindIDs.h"

// the 3D lines (0x529050) and a name (0x529030): one guard byte (0x529048) (provisional names)
std::vector<EERenderLine> gU529050;
// SYNTHETIC: EXE 0x50aa88   (wrapper)
// SYNTHETIC: EXE 0x50aa92   (ctor)
// SYNTHETIC: EXE 0x50aaaa   (registrar)
// SYNTHETIC: EXE 0x50aab6   (dtor)
UWideString gU529030;
// SYNTHETIC: EXE 0x50aad1   (wrapper)
// SYNTHETIC: EXE 0x50aadb   (ctor)
// SYNTHETIC: EXE 0x50aae6   (registrar)
// SYNTHETIC: EXE 0x50aaf2   (dtor)

// the unit picker and the environment: one guard byte (0x528d70)
EEUnitPicker gUnitPicker(0x600);
// SYNTHETIC: EXE 0x50ab0e   (wrapper)
// SYNTHETIC: EXE 0x50ab18   (ctor)
// SYNTHETIC: EXE 0x50ab28   (registrar)
// SYNTHETIC: EXE 0x50ab34   (dtor)
EERenderEnvironment gRenderEnvironment;
// SYNTHETIC: EXE 0x50ab4f   (wrapper)
// SYNTHETIC: EXE 0x50ab59   (ctor)
// SYNTHETIC: EXE 0x50ab63   (registrar)
// SYNTHETIC: EXE 0x50ab6f   (dtor)

EERenderLists EERenderLists::sInstance;
// SYNTHETIC: EXE 0x50ab8a   (wrapper)
// SYNTHETIC: EXE 0x50ab94   (ctor)
// SYNTHETIC: EXE 0x50ab9e   (registrar)
// SYNTHETIC: EXE 0x50abaa   (dtor)
// SYNTHETIC: EXE 0x50abc5
// (EERenderLists' implicit dtor, emitted after its first user)

EEEffectQueue gEffectQueues[5];
// SYNTHETIC: EXE 0x50ac30   (wrapper)
// SYNTHETIC: EXE 0x50ac3a   (ctor)
// SYNTHETIC: EXE 0x50ac64   (registrar)
// SYNTHETIC: EXE 0x50ac70   (dtor)

// 0x4e9348 (17 colours; provisional name)
GEColor gU4E9348[17];
// SYNTHETIC: EXE 0x50ac9a   (wrapper)
// SYNTHETIC: EXE 0x50aca4   (ctor)
// SYNTHETIC: EXE 0x50acbf   (registrar)
// SYNTHETIC: EXE 0x50accb   (dtor)

GEColor gU4E9338(1.0f, 1.0f, 1.0f);
float gU4E9330;                            // 0x4e9330 (no init chain)
// three per-time-of-day tables (17 entries, like gU4E9348; EEInitialiseScene fills them, Fn10C036 blends them)
// (provisional names)
float gU4E92EC[17];                         // 0x4e92ec
float gU4E92A8[17];                         // 0x4e92a8
float gU4E9264[17];                         // 0x4e9264
// 0x4413ac: EEInitialiseScene's 0.549f is a literal (__real COMDAT; the fst chain of a literal, not a static const reload)
static const float kEERenderObject_F44134C = 50.0f;    // 0x44134c (the ground circles' segments)
// Fn10C036's constants (.rdata 0x441334..) (provisional names)
static const float kEERenderObject_F441334 = 0.0f;
static const float kEERenderObject_F441338 = 1600.0f;
static const float kEERenderObject_F44133C = 35.0f;
static const float kEERenderObject_F441340 = 1085.0f;
static const float kEERenderObject_F441348 = 0.8f;
static const float kEERenderObject_F441360 = 0.42553192f;
static const float kEERenderObject_F4413B0 = 0.333333f;
static const float kEERenderObject_F4413B4 = 0.9375f;
static const float kEERenderObject_F4413B8 = 0.5625f;
static const float kGameViewScaleY = 1.0f / 1200.0f;   // 0x4413bc
static const float kEERenderObject_F4413C0 = 1200.0f;
static const float kEERenderObject_F4413C4 = 5.0e-05f;
static bool sB529088;                       // 0x529088 (the letterbox is cleared again; provisional name)
static unsigned long sSwayClock;            // 0x529084 (+2 every 33 ms: FillRenderLists; the objects' sway; provisional name)
static const float kEERenderObject_F441344 = 0.017453292f;   // 0x441344 (degrees to radians)
static const float kEERenderObject_F441354 = 0.025f;         // 0x441354
bool Fn10CF98(EETile* inTile);
static unsigned long sU529090;              // 0x529090 (the music check's last time; provisional name)
static unsigned long sU529094;              // 0x529094 (the sway clock's last time; provisional name)
static unsigned long sL52908C;              // 0x52908c (the frame counter; provisional name)
EERenderUnit* gU529044;                     // 0x529044 (the picked enemy's render unit: FillUnit; EEROUnits.cpp reads it; provisional name)
// 0x52906c: how the selection ring is sized (0: the class, 1: GetLineOfSight, 2: the stats; only read: FillUnit)
// (provisional name)
static long sL52906C;
static const float kEERenderObject_F441350 = 0.35f;   // 0x441350 (the see-through opacity)
static const float kEERenderObject_F44136C = 0.5f;    // 0x44136c
static const float kEERenderObject_F441358 = 0.6f;    // 0x441358 (the second light set's ambient scale)
static const float kEERenderObject_F44135C = 10.0f;   // 0x44135c (its light's z scale)
static const float kEERenderObject_F441364 = 4.0f;    // 0x441364 (the fog band)
static const float kEERenderObject_F441368 = 4.0f;    // 0x441368
// SYNTHETIC: EXE 0x50ace6   (wrapper)

// SYNTHETIC: EXE 0x50acf0   (ctor)
// SYNTHETIC: EXE 0x50ad09   (registrar)
// SYNTHETIC: EXE 0x50ad15   (dtor)

// 0x4e9240 (3 vectors; provisional name)
GEVector gU4E9240[3];
// SYNTHETIC: EXE 0x50ad16   (wrapper)
// SYNTHETIC: EXE 0x50ad20   (ctor)
// SYNTHETIC: EXE 0x50ad3b   (registrar)
// SYNTHETIC: EXE 0x50ad47   (dtor)

std::vector<unsigned long> gU4E9230;
// SYNTHETIC: EXE 0x50ad62   (wrapper)
// SYNTHETIC: EXE 0x50ad6c   (ctor)
// SYNTHETIC: EXE 0x50ad84   (registrar)
// SYNTHETIC: EXE 0x50ad90   (dtor)

GEColor gU4E9220(1.0f, 1.0f, 1.0f);
// SYNTHETIC: EXE 0x50ad9a   (wrapper)
// SYNTHETIC: EXE 0x50ada4   (ctor)
// SYNTHETIC: EXE 0x50adbd   (registrar)
// SYNTHETIC: EXE 0x50adc9   (dtor)

GEVector gU4E9210;                         // 0x4e9210 (the sun direction: Fn10C036, Fn10F833)
// SYNTHETIC: EXE 0x50adca   (wrapper)
// SYNTHETIC: EXE 0x50add4   (ctor)
// SYNTHETIC: EXE 0x50aded   (registrar)
// SYNTHETIC: EXE 0x50adf9   (dtor)

GELight gU4E9198;
unsigned long gU4E9190;                     // 0x4e9190 (gU4E9198's index in the rasterizer: AddLight/UpdateLight)
// SYNTHETIC: EXE 0x50adfa   (wrapper)
// SYNTHETIC: EXE 0x50ae04   (ctor)
// SYNTHETIC: EXE 0x50ae0f   (registrar)
// SYNTHETIC: EXE 0x50ae1b   (dtor)

GEViewport gU4E8F08;
// SYNTHETIC: EXE 0x50ae1c   (wrapper)
// SYNTHETIC: EXE 0x50ae26   (ctor)
// SYNTHETIC: EXE 0x50ae31   (registrar)
// SYNTHETIC: EXE 0x50ae3d   (dtor)

// the lights' intensity scales (0x4be368, 0x4be36c; .data) (provisional names)
static float sLightFade = 1.0f;
static float sLightFadeStart = 1.0f;
// the frame interpolation (0x4be370..; .data) (provisional names)
static long sLetterboxDirection = 1;      // 0x4be370
float gF4BE374 = 1.0f;                     // (EEScenarioManager.cpp reads it)
static float sLetterboxStart = 1.0f;
// the space-cliff and cliff-edge texture names (0x4be37c; .data, the literals follow it) (provisional name)
static const char* sCliffTextures[13] = {
    "textures\\spacecliff_1", "textures\\spacecliff_2", "textures\\spacecliff_3", "textures\\spacecliff_4",
    "textures\\spacecliff_5", "textures\\spacecliff_6", "textures\\spacecliff_7", "textures\\spacecliff_8",
    "textures\\cliffedge_1", "textures\\cliffedge_2", "textures\\cliffedge_3", "textures\\cliffedge_4",
    "textures\\cliffedge_5"
};
static float sF52907C;                      // 0x52907c
static float sF529080;                      // 0x529080
// EEInitialiseScene's state (provisional names)
static GEBitmap* sU529060;                  // 0x529060
static bool sSceneInitialised;              // 0x529064 (EEInitialiseScene ran)
static long sL529070;                       // 0x529070
static float sF529074;                      // 0x529074
static float sF529078;                      // 0x529078
// the unit's other .bss globals (declared in ee/EERenderLists.h, ee/EEGraphics.h, ee/EmpireEarth.h; their address
// neighbours and references are this unit's) (provisional names)
bool gB529065;                              // 0x529065
bool gB529066;                              // 0x529066
bool gCameraFollowing;                      // 0x529067
bool gB529068;                              // 0x529068

// FUNCTION: EXE 0x50ae48
// Creates a game render object
EERenderObject::EERenderObject()
{
}

// SYNTHETIC: EXE 0x50ae51
// (the scalar deleting destructor, emitted with the vtable; slot EERenderObject+0x10)

// FUNCTION: EXE 0x50ae6d
// Destroys a render object (RTSRenderObject's inline destructor)
RTSRenderObject::~RTSRenderObject()
{
}

// FUNCTION: EXE 0x50ae74
// Sets the light and time-of-day colour tables to their defaults and creates the shared bitmap, once per scene
void EEInitialiseScene()
{
    if (sSceneInitialised)
        return;
    gU4E9198.SetAttenuation0(1.0f);
    gU4E9198.SetAttenuation1(0.0f);
    gRenderEnvironment.mTimeOfDay = -100.0f;
    gU4E9198.SetStyle(1);
    gU4E9198.SetAttenuation2(0.0f);
    gU4E9338.SetRed(0.663f);
    gU4E9338.SetGreen(0.784f);
    gU4E9338.SetBlue(1.0f);
    gU4E9348[0].SetRed(1.0f);
    gU4E9348[0].SetGreen(0.4f);
    gU4E9348[0].SetBlue(0.0f);
    gU4E9348[1].SetRed(1.0f);
    gU4E9348[1].SetGreen(1.0f);
    gU4E9348[1].SetBlue(1.0f);
    gU4E9348[2].SetRed(1.0f);
    gU4E9348[2].SetGreen(1.0f);
    gU4E9348[2].SetBlue(1.0f);
    gU4E9348[3].SetRed(1.0f);
    gU4E9348[3].SetGreen(1.0f);
    gU4E9348[3].SetBlue(1.0f);
    gU4E9348[4].SetRed(1.0f);
    gU4E9348[4].SetGreen(1.0f);
    gU4E9348[4].SetBlue(1.0f);
    gU4E9348[5].SetRed(1.0f);
    gU4E9348[5].SetGreen(1.0f);
    gU4E9348[5].SetBlue(1.0f);
    gU4E9348[6].SetRed(1.0f);
    gU4E9348[6].SetGreen(1.0f);
    gU4E9348[6].SetBlue(1.0f);
    gU4E9348[7].SetRed(1.0f);
    gU4E9348[7].SetGreen(1.0f);
    gU4E9348[7].SetBlue(1.0f);
    gU4E9348[8].SetRed(1.0f);
    gU4E9348[8].SetGreen(0.4f);
    gU4E9348[8].SetBlue(0.0f);
    gU4E9348[9].SetRed(0.775f);
    gU4E9348[9].SetGreen(0.475f);
    gU4E9348[9].SetBlue(0.5f);
    gU4E9348[10].SetRed(0.549f);
    gU4E9348[10].SetGreen(0.549f);
    gU4E9348[10].SetBlue(1.0f);
    gU4E9348[11].SetRed(0.549f);
    gU4E9348[11].SetGreen(0.549f);
    gU4E9348[11].SetBlue(1.0f);
    gU4E9348[12].SetRed(0.549f);
    gU4E9348[12].SetGreen(0.549f);
    gU4E9348[12].SetBlue(1.0f);
    gU4E9348[13].SetRed(0.549f);
    gU4E9348[13].SetGreen(0.549f);
    gU4E9348[13].SetBlue(1.0f);
    gU4E9348[14].SetRed(0.549f);
    gU4E9348[14].SetGreen(0.549f);
    gU4E9348[14].SetBlue(1.0f);
    gU4E9348[15].SetRed(0.775f);
    gU4E9348[15].SetGreen(0.475f);
    gU4E9348[15].SetBlue(0.5f);
    gU4E9348[16].SetRed(1.0f);
    gU4E9348[16].SetGreen(0.4f);
    gU4E9348[16].SetBlue(0.0f);
    gU4E92EC[0] = 1.0f;
    gU4E92EC[1] = 1.0f;
    gU4E92EC[2] = 1.0f;
    gU4E92EC[3] = 1.0f;
    gU4E92EC[4] = 1.0f;
    gU4E92EC[5] = 1.0f;
    gU4E92EC[6] = 1.0f;
    gU4E92EC[7] = 1.0f;
    gU4E92EC[8] = 1.0f;
    gU4E92EC[9] = 0.0f;
    gU4E92EC[10] = 0.9f;
    gU4E92EC[11] = 0.9f;
    gU4E92EC[12] = 0.9f;
    gU4E92EC[13] = 0.9f;
    gU4E92EC[14] = 0.9f;
    gU4E92EC[15] = 0.0f;
    gU4E92EC[16] = 1.0f;
    gU4E92A8[0] = 0.5f;
    gU4E92A8[1] = 0.5f;
    gU4E92A8[2] = 0.5f;
    gU4E92A8[3] = 0.5f;
    gU4E92A8[4] = 0.5f;
    gU4E92A8[5] = 0.5f;
    gU4E92A8[6] = 0.5f;
    gU4E92A8[7] = 0.5f;
    gU4E92A8[8] = 0.5f;
    gU4E92A8[9] = 0.7f;
    gU4E92A8[10] = 0.3f;
    gU4E92A8[11] = 0.3f;
    gU4E92A8[12] = 0.3f;
    gU4E92A8[13] = 0.3f;
    gU4E92A8[14] = 0.3f;
    gU4E92A8[15] = 0.7f;
    gU4E92A8[16] = 0.5f;
    gU4E9264[0] = 0.7f;
    gU4E9264[1] = 0.6f;
    gU4E9264[2] = 0.6f;
    gU4E9264[3] = 0.6f;
    gU4E9264[4] = 0.6f;
    gU4E9264[5] = 0.6f;
    gU4E9264[6] = 0.6f;
    gU4E9264[7] = 0.6f;
    gU4E9264[8] = 0.6f;
    gU4E9264[9] = 0.7f;
    gU4E9264[10] = 0.7f;
    gU4E9264[11] = 0.6f;
    gU4E9264[12] = 0.6f;
    gU4E9264[13] = 0.6f;
    gU4E9264[14] = 0.6f;
    gU4E9264[15] = 0.7f;
    gU4E9264[16] = 0.7f;
    gU4E9240[0].SetX(0.0f);
    gU4E9240[0].SetY(0.0f);
    gU4E9240[0].SetZ(-1.0f);
    gU4E9240[1].SetX(1.0f);
    gU4E9240[1].SetY(1.0f);
    gU4E9240[1].SetZ(-1.0f);
    gU4E9240[2].SetX(-1.0f);
    gU4E9240[2].SetY(-1.0f);
    gU4E9240[2].SetZ(-1.0f);
    gU4E9240[0].Normalize();
    gU4E9240[1].Normalize();
    gU4E9240[2].Normalize();
    sLightFade = 1.0f;
    sL529070 = 0;
    gB529066 = false;
    sF529074 = 0.0f;
    gCameraFollowing = false;
    gB529068 = false;
    sLetterboxDirection = 1;
    sF529078 = 0.0f;
    sLightFadeStart = 1.0f;
    gF4BE374 = 1.0f;
    sF52907C = 0.0f;
    sF529080 = 0.0f;
    sLetterboxStart = 1.0f;
    for (unsigned long i = 0; i < 5; i++)
        gEffectQueues[i].clear();
    for (unsigned long j = 0; j < 0x200; j++)
        EERenderLists::sInstance.mRenderUnits[j].mHealthBars.resize(1, 0.0f);
    sSceneInitialised = true;
    sU529060 = new GEBitmap;
}

// SYNTHETIC: EXE 0x50b301
// (GEBitmap's local scalar deleting destructor, emitted for the `new GEBitmap` above)


// FUNCTION: EXE 0x50b31e
// Starts closing the letterbox bars over the given number of frames (at once for 0)
void EEStartLetterboxOut(unsigned long inFrames)
{
    sLetterboxStart = gF4BE374;
    sF529080 = 0.0f;
    sLetterboxDirection = 1;
    if (inFrames == 0) {
        sF52907C = 3.4028235e+38f;
        return;
    }
    sF52907C = 1.0f / (float)inFrames;
}

// FUNCTION: EXE 0x50b36a
// Starts opening the letterbox bars over the given number of frames (at once for 0)
void EEStartLetterboxIn(unsigned long inFrames)
{
    sLetterboxDirection = 0;
    sF529080 = 0.0f;
    sLetterboxStart = gF4BE374;
    if (inFrames == 0) {
        sF52907C = 3.4028235e+38f;
        return;
    }
    sF52907C = 1.0f / (float)inFrames;
}

// FUNCTION: EXE 0x50b3b3
// Returns whether a unit is drawn this frame: seen by the current player (or the free camera) and in the
// view; a unit whose footprint spans several tiles counts once (ioDrawn)
bool EERenderObject::IsUnitDrawn(EEUnit* inUnit, EETile* inTile, std::set<EEUnit*>& ioDrawn)
{
    GEPhysicalModel theModel;
    GETransformation theTransformation(false);
    GETerrainMesh* theMesh = &gWorld.mMap->mTerrainMesh;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    float theX;
    float theY;
    float theZ;
    bool theResult = true;
    bool theOwnedTarget;
    if (inUnit->mResourceType == kResourceFood && inUnit->mResourceAmount > 0 && theComplexUnit != NULL && inUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen
        && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen && inUnit->mPlayer->GetPlayerID() == 0)
        theOwnedTarget = true;
    else
        theOwnedTarget = false;
    if ((inTile->IsSeenBy(thePlayer) || theOwnedTarget || gFreeCamera) && theStats->GetScale() != 0.0f) {
        EEUnitClass* theClass = inUnit->mUnitClass;
        if (!theClass->mIsTree && theClass->GetUnitType() != kUnitTypePathPoint) {
            if (EERenderLists::sInstance.mRenderUnitCount < 0x200
                && (theComplexUnit == NULL
                    || (theComplexUnit->mCommand == NULL ? 0 : theComplexUnit->mCommand->mType) != kActionDead)) {
                if (theClass->GetTileSize() > 1) {
                    theResult = false;
                    if (ioDrawn.find(inUnit) != ioDrawn.end())
                        return theResult;
                    ioDrawn.insert(inUnit);
                    theResult = true;
                }
                if (theComplexUnit != NULL) {
                    if (theComplexUnit->mUnitClass->mCategory != kUnitCategoryGate) {
                        theResult = theComplexUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView);
                        if (!theResult)
                            return theResult;
                    }
                    if (!gFreeCamera) {
                        if (theComplexUnit->mUnitClass->GetUnitType() == kUnitTypePathPoint) {
                            if (inUnit->mPlayer == thePlayer && inUnit->Fn27C0B4())
                                theResult = true;
                            else
                                theResult = false;
                        } else if (theComplexUnit->IsFoundation()) {
                            theResult = inUnit->mPlayer == thePlayer;
                        } else if (theComplexUnit->mUnitClass->mIsFlare) {
                            theResult = false;
                        }
                    }
                } else {
                    float theHalf = theClass->GetTileSize() * 0.5f;
                    theX = inUnit->mTileX + theHalf;
                    theY = inUnit->mTileY + theHalf;
                    theZ = theMesh->GetMeshZ(theX, theY);
                    theTransformation.SetOrientationYPR(gRenderEnvironment.mCameraYaw, 0.0f, 0.0f);
                    theTransformation.SetPosition(theX, theY, theZ);
                    theTransformation.SetElement(3, 0, 0.0f);
                    theTransformation.SetElement(3, 1, 0.0f);
                    theTransformation.SetElement(3, 2, 0.0f);
                    theTransformation.SetElement(3, 3, 1.0f);
                    theModel.SetModel(inUnit->GetModel()->mModels[0]);
                    theModel.SetFrame(0);
                    theModel.SetLocationAndScale(theTransformation, theStats->GetScale());
                    theResult = theModel.Intersects(gViewport, gOrthographicView);
                    if (theResult) {
                        if (inUnit->IsPathMarker() && inUnit->mPlayer != thePlayer)
                            theResult = false;
                        else
                            theResult = true;
                    }
                }
            } else {
                theResult = false;
            }
        } else if (theClass->mIsTree && EERenderLists::sInstance.mRenderShadowCount < 0x600) {
            long theID = inUnit->GetUnitID();
            long theSize = theClass->GetTileSize();
            float theHalf = theSize * 0.5f;
            theX = ((theID % 3 - 1) * theSize) * 0.125f + inUnit->mTileX + theHalf;
            theY = ((theID / 3 % 3 - 1) * theSize) * 0.125f + inUnit->mTileY + theHalf;
            theZ = theMesh->GetMeshZ(theX, theY);
            theTransformation.SetOrientationYPR(gRenderEnvironment.mCameraYaw, 0.0f, 0.0f);
            theTransformation.SetPosition(theX, theY, theZ);
            theTransformation.SetElement(3, 0, 0.0f);
            theTransformation.SetElement(3, 1, 0.0f);
            theTransformation.SetElement(3, 2, 0.0f);
            theTransformation.SetElement(3, 3, 1.0f);
            theModel.SetModel(inUnit->GetModel()->mModels[0]);
            theModel.SetFrame(0);
            theModel.SetLocationAndScale(theTransformation, theStats->GetScale());
            theResult = theModel.Intersects(gViewport, gOrthographicView);
        } else {
            theResult = false;
        }
    } else {
        theResult = false;
    }
    return theResult;
}

// FUNCTION: EXE 0x50b7e1
// Unloads the media of every pooled map object and empties the pool
void Fn10B7E1(EERenderShadow* inObjects, unsigned long& ioCount)
{
    for (unsigned long i = 0; i < ioCount; i++)
        inObjects[i].mObject->UnloadMedia();
    ioCount = 0;
}

// FUNCTION: EXE 0x50b80b
// Unloads the media of every pooled render unit, resets its bars and turning parts, clears the pointed-at
// unit and object, and empties the pool
void Fn10B80B(EERenderUnit* inUnits, unsigned long& ioCount)
{
    for (unsigned long i = 0; i < ioCount; i++) {
        inUnits[i].mSource->UnloadMedia();
        if (inUnits[i].mSource2 != NULL)
            inUnits[i].mSource2->UnloadMedia();
        if (inUnits[i].mHealthBars.size() != 1)
            inUnits[i].mHealthBars.resize(1, 0.0f);
        inUnits[i].mTurretAngles.clear();
        inUnits[i].mTurretModels.clear();
        inUnits[i].mOrbitalAngles.clear();
        inUnits[i].mOrbitalModels.clear();
        inUnits[i].mWheelAngles.clear();
        inUnits[i].mWheelModels.clear();
    }
    EERenderLists::sInstance.mSelectedUnit = NULL;
    EERenderLists::sInstance.mSelectedObject = NULL;
    ioCount = 0;
}

// FUNCTION: EXE 0x50b8ad
// Empties the ghost render records from the back, unloading each one's media
void Fn10B8AD()
{
    EERenderUnitRecord theRecord;
    while (EERenderLists::sInstance.mGhostRecords.size() > 0) {
        theRecord = EERenderLists::sInstance.mGhostRecords.back();
        EERenderLists::sInstance.mGhostRecords.pop_back();
        theRecord.mUnit.mSource->UnloadMedia();
    }
}

// SYNTHETIC: EXE 0x50b919
// (EERenderUnitRecord's implicit operator=)

// SYNTHETIC: EXE 0x50b983
// (EERenderUnit's implicit operator=)

// FUNCTION: EXE 0x50bba4
// Queues a 50-segment ground circle of a radius around a location as 3D lines with the area-effect
// texture (mode 2 only)
void EERenderObject::DrawGroundCircle(const GETransformation& inLocation, float inRadius, long inMode)
{
    EERenderLine theLine;
    std::pair<GE3DPoint, float> theVertex;
    GETerrainMesh* theMesh = &gWorld.mMap->mTerrainMesh;
    if (inMode == 1 || inMode != 2)
        return;
    gU529050.push_back(theLine);
    EERenderLine& theBack = gU529050[gU529050.size() - 1];
    theBack.mWidth = gU4E9330;
    theBack.mTexture = gAreaEffectTexture;
    gAreaEffectTexture->Reference();
    theBack.mPoints.resize(0x33, std::pair<GE3DPoint, float>());
    float theX = inLocation.GetX();
    float theY = inLocation.GetY();
    theVertex.second = UMSin(6.2831855f / kEERenderObject_F44134C) * inRadius;
    for (unsigned long i = 0; i < 0x32; i++) {
        float theAngle = i / kEERenderObject_F44134C * 6.2831855f - 3.1415927f;
        theVertex.first.mX = UMCos(theAngle) * inRadius + theX;
        theVertex.first.mY = UMSin(theAngle) * inRadius + theY;
        if (theMesh->GetIsValidTile(UMFloorUnsigned(theVertex.first.GetX()), UMFloorUnsigned(theVertex.first.GetY()))) {
            theVertex.first.mZ = theMesh->GetMeshZ(theVertex.first.GetX(), theVertex.first.GetY());
            if (theVertex.first.GetZ() < 0.0f)
                theVertex.first.mZ = 0.0f;
        } else {
            theVertex.first.mZ = 0.0f;
        }
        theBack.mPoints[i] = theVertex;
    }
    theBack.mPoints[0x32] = theBack.mPoints[0];
}

// FUNCTION: EXE 0x50bdd2
// Rebuilds the effect render queues: drops last frame's, then queues the effects of every map region
// under a visible tile row
void EERenderObject::QueueEffects()
{
    std::map<long, std::pair<long, long> > theRows;
    EEMap* theMap = gWorld.mMap;
    std::map<long, std::pair<long, long> >::iterator theRow;
    EEEffectQueue::iterator theEntry;
    std::deque<EEEffect*>::iterator theEffect;
    RTSRegionTable<EEEffect*>* theTable = theMap->mEffectRegions;
    for (unsigned long i = 0; i < 5; i++) {
        for (theEntry = gEffectQueues[i].begin(); theEntry != gEffectQueues[i].end(); theEntry++) {
            for (theEffect = theEntry->second.begin(); theEffect != theEntry->second.end(); theEffect++)
                (*theEffect)->Release();
        }
        gEffectQueues[i].clear();
    }
    GETerrainMesh* theMesh = &theMap->mTerrainMesh;
    unsigned long theCount = theMesh->GetNumVisibleTileRows();
    for (unsigned long j = 0; j < theCount; j++) {
        long theY;
        long theFirst;
        long theLast;
        theMesh->GetVisibleTileRow(j, theY, theFirst, theLast);
        theFirst /= theTable->mRegionSize;
        long theKey = theY / theTable->mRegionSize;
        theLast /= theTable->mRegionSize;
        theKey = theY / theTable->mRegionSize;
        theRow = theRows.find(theKey);
        if (theRow != theRows.end()) {
            theRow->second.first = min((*theRow).second.first, theFirst);
            (*theRow).second.second = max(theRow->second.second, theLast);
        } else {
            theRows.insert(std::map<long, std::pair<long, long> >::value_type(theKey,
                std::pair<long, long>(theFirst, theLast)));
        }
    }
    for (theRow = theRows.begin(); theRow != theRows.end(); theRow++) {
        long theKey = theRow->first;
        for (long theX = theRow->second.first; theX <= theRow->second.second; theX++) {
            std::vector<EEEffect*>* theRegion = theTable->GetRegion(theX, theKey);
            if (theRegion == NULL)
                continue;
            unsigned long theSize = theRegion->size();
            for (unsigned long k = 0; k < theSize; k++) {
                EEEffect* theOne = *(theRegion->begin() + k);
                if (!(!theOne->mIsLocalOnly || theOne->mIsLocalOnly == true && theOne->mPlayer == gWorld.mCurrentPlayer))
                    continue;
                theOne->PrepareQueue();
                if (theOne->Queue(gEffectQueues) == 0)
                    theOne->Reference();
            }
        }
    }
}

// FUNCTION: EXE 0x50c036
// Updates the per-frame render state: grid animation, cloud and sky clocks, light fade, letterbox and
// view zoom, camera and viewport, and the light colours on a time-of-day change
// (the view zoom covers the letterbox and the UI backgrounds; light colours blend the 17 key frames)
void EERenderObject::Fn10C036()
{
    GERectangle theRectangle;
    GEColor theBlack(0.0f, 0.0f, 0.0f);
    GERectangle theView;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    GEColor theColor;
    GEColor theBlend;
    float theTimeOfDay = gWorld.mTimeOfDay;
    static unsigned long sLastTime = TMGetTime();
    bool theChanged = false;
    if (gB529066)
        return;
    unsigned long theNow = TMGetTime();
    float theElapsed = (float)(theNow - sLastTime);
    sLastTime = theNow;
    gU4E9330 += theElapsed * kEERenderObject_F441348 / 1000.0f;
    if (gU4E9330 >= 1.0f)
        gU4E9330 -= 1.0f;
    gRenderEnvironment.mWindSpeed = gWorld.mEnvironmentManager->mWind;
    gRenderEnvironment.mWaterPhase += theElapsed * kEERenderObject_F4413C4 * 6.2831855f;
    gRenderEnvironment.mCloudOffset += theElapsed * gRenderEnvironment.mWindSpeed;
    sF529078 += theElapsed;
    float theFade;
    if (sL529070 == 0)
        theFade = sLightFadeStart + (1.0f - sLightFadeStart) * (sF529074 * sF529078);
    else
        theFade = (1.0f - sF529074 * sF529078) * sLightFadeStart;
    if (theFade > 1.0f)
        theFade = 1.0f;
    else if (theFade < 0.0f)
        theFade = 0.0f;
    if (theFade != sLightFade)
        theChanged = true;
    sLightFade = theFade;
    sF529080 += theElapsed;
    float theZoom;
    if (sLetterboxDirection == 1)
        theZoom = sLetterboxStart + (1.0f - sLetterboxStart) * (sF52907C * sF529080);
    else
        theZoom = (1.0f - sF52907C * sF529080) * sLetterboxStart;
    if (theZoom > 1.0f)
        theZoom = 1.0f;
    else if (theZoom < 0.0f)
        theZoom = 0.0f;
    if (sB529088 == true || theZoom == 0.0f) {
        float theHalf = (kEERenderObject_F4413C0 - (kEERenderObject_F441338 - kEERenderObject_F441334)
                         * kEERenderObject_F441360) * 0.5f;
        float theInverse = 1.0f - theZoom;
        theView.mTop = theInverse * theHalf + kEERenderObject_F44133C * theZoom;
        theView.mBottom = (kEERenderObject_F4413C0 - theHalf) * theInverse + kEERenderObject_F441340 * theZoom;
        theRectangle.mLeft = 0.0f;
        theRectangle.mRight = (float)gGameWindowWidth;
        theRectangle.mTop = 0.0f;
        theRectangle.mBottom = gGameWindowHeight * kGameViewScaleY * theView.GetTop();
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        theRectangle.mTop = gGameWindowHeight * kGameViewScaleY * theView.mBottom;
        theRectangle.mBottom = (float)gGameWindowHeight;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        sB529088 = false;
    }
    if (theZoom != gF4BE374) {
        theView.SetLeft(kEERenderObject_F441334);
        theView.SetRight(kEERenderObject_F441338);
        float theHalf = (kEERenderObject_F4413C0 - (kEERenderObject_F441338 - kEERenderObject_F441334)
                         * kEERenderObject_F441360) * 0.5f;
        float theInverse = 1.0f - theZoom;
        theView.mTop = theInverse * theHalf + kEERenderObject_F44133C * theZoom;
        theView.mBottom = (kEERenderObject_F4413C0 - theHalf) * theInverse + kEERenderObject_F441340 * theZoom;
        EESetGameViewRectangle(theView);
        theRectangle.mLeft = 0.0f;
        theRectangle.mRight = (float)gGameWindowWidth;
        theRectangle.mTop = 0.0f;
        theRectangle.mBottom = gGameWindowHeight * kGameViewScaleY * theView.GetTop() + 1.0f;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        theRectangle.mTop = gGameWindowHeight * kGameViewScaleY * theView.mBottom;
        theRectangle.mBottom = (float)gGameWindowHeight;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        if (theZoom == 0.0f)
            sB529088 = true;
        if (theZoom == 1.0f) {
            gUIManager->GetMainGameForm()->ShowAllBackgrounds();
            gUIManager->RedrawAll();
        }
        if (gF4BE374 == 1.0f) {
            gUIManager->GetMainGameForm()->HideAllBackgrounds();
            gUIManager->RedrawAll();
        }
    }
    gF4BE374 = theZoom;
    gViewport.SetCamera(gWorld.mTransformation);
    gRasterizer->SetViewport(gViewport, gScreenIndex);
    gU4E8F08 = gViewport;
    gWorld.mTransformation.Apply(theForward, theDirection);
    gRenderEnvironment.mCameraYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY());
    gRenderEnvironment.mDirection = gWorld.mWindDirection;
    gRenderEnvironment.mSeaLevel = 0.0f;
    gRenderEnvironment.mB8C = false;
    if (gRenderEnvironment.mTimeOfDay != theTimeOfDay) {
        gRenderEnvironment.mTimeOfDay = theTimeOfDay;
        gRenderEnvironment.mTimeOfDayChanged = true;
        if (theTimeOfDay >= kEERenderObject_F4413B8 && theTimeOfDay <= kEERenderObject_F4413B4)
            gU4E9210 = gWorld.mMoonDirection;
        else
            gU4E9210 = gWorld.mSunDirection;
        float theTime = theTimeOfDay * 16.0f;
        float theFraction = theTime - UMFloor(theTime);
        unsigned long theIndex = UMFloorUnsigned(theTime);
        unsigned long theNext = theIndex + 1;
        if (theNext > 16)
            theNext = 16;
        float theRest = 1.0f - theFraction;
        theBlend.SetRed(gU4E9348[theNext].GetRed() * theFraction + gU4E9348[theIndex].GetRed() * theRest);
        theBlend.SetGreen(gU4E9348[theNext].GetGreen() * theFraction + gU4E9348[theIndex].GetGreen() * theRest);
        theBlend.SetBlue(gU4E9348[theNext].GetBlue() * theFraction + gU4E9348[theIndex].GetBlue() * theRest);
        gRenderEnvironment.mTerrainLightColor = theBlend;
        gRenderEnvironment.mUnitLightColor = theBlend;
        gRenderEnvironment.mTerrainAmbientColor = gU4E9338;
        gRenderEnvironment.mUnitAmbientColor = gU4E9338;
        gRenderEnvironment.mTerrainLightIntensity = theFraction * gU4E92EC[theNext] + theRest * gU4E92EC[theIndex];
        gRenderEnvironment.mUnitLightIntensity = gRenderEnvironment.mTerrainLightIntensity;
        gRenderEnvironment.mTerrainAmbientIntensity = theFraction * gU4E92A8[theNext] + theRest * gU4E92A8[theIndex];
        float theScale = theFraction * gU4E9264[theNext] + theRest * gU4E9264[theIndex];
        gRenderEnvironment.mUnitAmbientIntensity = theScale;
        theColor.SetRed(gU4E9338.GetRed() * theScale);
        theColor.SetGreen(gU4E9338.GetGreen() * theScale);
        theColor.SetBlue(gU4E9338.GetBlue() * theScale);
        for (unsigned long i = 0; i < 3; i++) {
            float theDot = gU4E9240[i].DotProduct(gU4E9210);
            if (theDot > 0.0f) {
                theColor.SetRed(theColor.GetRed() + theBlend.GetRed() * theScale * theDot * kEERenderObject_F4413B0);
                theColor.SetGreen(theColor.GetGreen() + theBlend.GetGreen() * theScale * theDot * kEERenderObject_F4413B0);
                theColor.SetBlue(theColor.GetBlue() + theBlend.GetBlue() * theScale * theDot * kEERenderObject_F4413B0);
            }
        }
        theColor.Normalize();
        gRenderEnvironment.mPrelitColor = theColor;
        gRenderEnvironment.mDimPrelitColor.SetRed(theColor.GetRed() * 0.5f);
        gRenderEnvironment.mDimPrelitColor.SetGreen(theColor.GetGreen() * 0.5f);
        gRenderEnvironment.mDimPrelitColor.SetBlue(theColor.GetBlue() * 0.5f);
        gRenderEnvironment.mColor54 = theColor;
    } else {
        gRenderEnvironment.mTimeOfDayChanged = false;
        if (!theChanged)
            goto done;
    }
    if (gCameraFollowing) {
        Fn10F833();
        gRasterizer->Light(gWorld.mMap->mTerrainMesh, gScreenIndex);
    }
done:
    gRenderEnvironment.mShowHiddenUnits = gHiddenUnitsMode;
    gB529066 = true;
}

// FUNCTION: EXE 0x50c952
// Queues a render record for every ghost unit, when the ghost units changed
void EERenderObject::FillGhostRecords()
{
    EERenderUnitRecord theRecord;
    gWorld.mGhostUnitsLock->Enter();
    if (gGhostUnitsChanged) {
        for (unsigned long i = 0; i < gWorld.GetNumGhostUnits(); i++) {
            EEGhostUnit* theGhost = gWorld.mGhostUnits[i];
            theRecord.mUnit.mSource = theGhost->GetModel();
            theRecord.mUnit.mSource->LoadMedia();
            theRecord.mUnit.mFraction = 0.0f;
            theRecord.mUnit.mCurrentFrame = theGhost->mFrame;
            theRecord.mUnit.mNextFrame = theGhost->mFrame;
            theRecord.mUnit.mAnimation = theGhost->mAnimation;
            theRecord.mUnit.mSource2 = NULL;
            theRecord.mUnit.mIsFogged = false;
            theRecord.mUnit.mScale = theGhost->GetScale();
            theRecord.mUnit.mCanPlace = theGhost->mCanPlace;
            theRecord.mUnit.mCategory = theGhost->mUnitClass->mCategory;
            theRecord.mUnit.mGridSize = theGhost->mUnitClass->mTileSize;
            theRecord.mUnit.mPlayerColor = theGhost->mPlayer->mColor;
            theRecord.mUnit.mInfo = theGhost->mUnitClass;
            theRecord.mUnit.mCategory = theGhost->mUnitClass->mCategory;
            theRecord.mUnit.mIsTinted = false;
            theRecord.mUnit.mHasSubModelOffset = false;
            theRecord.mTileX = theGhost->mTileX;
            theRecord.mTileY = theGhost->mTileY;
            memcpy(theRecord.mCells, theGhost->mFootprint, sizeof(theRecord.mCells));
            memcpy(theRecord.mHeights, theGhost->mHeights, sizeof(theRecord.mHeights));
            theRecord.mUnit.mTransformation = theGhost->mTransformation;
            theRecord.mUnit.mTransformation.SetZ(theRecord.mUnit.mTransformation.GetZ() + 0.001f);
            if (theGhost->mUnitClass->GetUnitType() == kUnitTypeSpaceTurret)
                theRecord.mUnit.mTransformation.SetZ(theRecord.mUnit.mTransformation.GetZ() + gSpaceTurretHeight);
            else if (theGhost->mUnitClass->GetUnitType() == kUnitTypeOrbitalSpaceStation)
                theRecord.mUnit.mTransformation.SetZ(theRecord.mUnit.mTransformation.GetZ() + gSpaceStationHeight);
            EERenderLists::sInstance.mGhostRecords.push_back(theRecord);
        }
    }
    gWorld.mGhostUnitsLock->Leave();
}

// FUNCTION: EXE 0x50cb62
// Returns the ghost unit's model through its player's upgrade of its unit class
// (inline in the real header)
EEModel* EEGhostUnit::GetModel()
{
    return this->mPlayer->mUnitClassUpgrades[this->mUnitClass->GetUnitType()]->mModel;
}

// FUNCTION: EXE 0x50cb81
// Returns the ghost unit's scale through its player's upgrade of its unit class
float EEGhostUnit::GetScale()
{
    return this->mPlayer->mUnitClassUpgrades[this->mUnitClass->GetUnitType()]->GetScale();
}

// FUNCTION: EXE 0x50cba3
// Fills a render object for a map object (a class with mIsTree: trees, rocks ...) and files it by texture,
// in the picker and in the shadow lists
void EERenderObject::FillObject(EEUnit* inUnit, EERenderShadow* outObject, EERenderShadowMap& ioMap,
                              EERenderFrameInfo& ioInfo)
{
    GEPhysicalModel theModel;
    GETransformation theTransformation(false);
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEMap* theMap = gWorld.mMap;
    long theID = inUnit->GetUnitID();
    long theSize = inUnit->mUnitClass->mTileSize;
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    outObject->mLocation.SetX((theID % 3 - 1) * theSize * 0.125f + theSize * 0.5f + inUnit->mTileX);
    outObject->mLocation.SetY((theID / 3 % 3 - 1) * theSize * 0.125f + inUnit->mUnitClass->mTileSize * 0.5f
                              + inUnit->mTileY);
    outObject->mLocation.SetZ(theMap->mTerrainMesh.GetMeshZ(outObject->mLocation.GetX(), outObject->mLocation.GetY()));
    theTransformation.SetOrientationYPR(ioInfo.mYaw, 0.0f, 0.0f);
    theTransformation.SetPosition(outObject->mLocation);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    if (ioInfo.mAmbient)
        gAmbientSoundManager.mAnalyzer.CountTree(inUnit);
    outObject->mHealth = (float)inUnit->GetHitPoints() / theStats->mMaxHitPoints;
    outObject->mIsFogged = false;
    outObject->mObject = inUnit->GetModel();
    outObject->mObject->LoadMedia();
    outObject->mModelScale = theStats->GetScale();
    outObject->mModelScale *= (unsigned long)inUnit->GetUnitID() % 11 * kEERenderObject_F441354 + 1.0f;
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    if (!Fn10CF98(theMap->GetTile(theX - 1, theY - 1)) || !Fn10CF98(theMap->GetTile(theX + 1, theY + 1))) {
        outObject->mScale = inUnit->mUnitClass->GetShadowScale();
        outObject->mF1C = inUnit->mUnitClass->GetF170();
    } else {
        outObject->mScale = 0.0f;
        outObject->mF1C = 0.0f;
    }
    if (inUnit == thePlayer->mViewedUnit
        || gFreeCamera && (inUnit->mIsEditorSelected
                           || std::find(gU4E9230.begin(), gU4E9230.end(), inUnit->GetUnitID()) != gU4E9230.end())) {
        outObject->mSelectionSize = inUnit->mUnitClass->mSelectionSize;
        EERenderLists::sInstance.mSelectedObjects.push_back(outObject);
    }
    outObject->mModelIndex = 0;
    GEModel* theOne = outObject->mObject->mModels[outObject->mModelIndex];
    outObject->mCurrentFrame = 0;
    outObject->mNextFrame = outObject->mCurrentFrame;
    if (theOne->GetFrameCount() > 1) {
        outObject->mCurrentFrame++;
        long theAngle = (sSwayClock + inUnit->GetUnitID() * 8) % 360 - 180;
        float theFraction = (UMSin(theAngle * kEERenderObject_F441344) + 1.0f) * 0.5f;
        if (theFraction < 0.0f)
            theFraction = 0.0f;
        else if (theFraction > 1.0f)
            theFraction = 1.0f;
        outObject->mFraction = theFraction;
    } else {
        outObject->mFraction = 0.0f;
    }
    ioMap.insert(EERenderShadowMap::value_type(outObject->mObject->mTextures[outObject->mModelIndex], outObject));
    gUnitPicker.AddUnit(inUnit, theTransformation, outObject->mOpacity);
    EERenderLists::sInstance.mShadows2.insert(EERenderShadowMap::value_type(outObject->mObject->mShadowTexture,
                                                                            outObject));
}

// FUNCTION: EXE 0x50cf98
// Returns whether any unit on a tile is a map object (its class has mIsTree)
bool Fn10CF98(EETile* inTile)
{
    bool theResult = false;
    if (inTile != NULL) {
        for (RTSTileUnitNode* theNode = inTile->mUnits; theNode != NULL && !theResult; theNode = theNode->mNext)
            theResult = theNode->mUnit->mUnitClass->mIsTree;
    }
    return theResult;
}

// FUNCTION: EXE 0x50cfbc
// Fills a render object for a remembered map object on a tile the current player cannot see, when the
// pool has room and the model is in the view
void EERenderObject::FillDoppelgangerObject(EEDoppelganger* inDoppelganger, EERenderShadow* outObject, unsigned long& ioCount,
                              EERenderShadowMap& ioMap, unsigned long inMaximum, long inX, long inY,
                              EERenderFrameInfo& ioInfo)
{
    GEPhysicalModel theModel;
    GETransformation theTransformation(false);
    EEMap* theMap = gWorld.mMap;
    float theScale = inDoppelganger->mUnit->mUnitClass->mScale;
    long theID = inDoppelganger->mUnit->GetUnitID();
    long theSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    outObject->mLocation.mX = (theID % 3 - 1) * theSize * 0.125f + theSize * 0.5f + inX;
    outObject->mLocation.mY = (theID / 3 % 3 - 1) * theSize * 0.125f
                              + inDoppelganger->mUnit->mUnitClass->mTileSize * 0.5f + inY;
    outObject->mLocation.mZ = theMap->mTerrainMesh.GetMeshZ(outObject->mLocation.GetX(), outObject->mLocation.GetY());
    theTransformation.SetOrientationYPR(ioInfo.mYaw, 0.0f, 0.0f);
    theTransformation.SetPosition(outObject->mLocation.GetX(), outObject->mLocation.GetY(), outObject->mLocation.mZ);
    theTransformation.SetElement(3, 0, 0.0f);
    theTransformation.SetElement(3, 1, 0.0f);
    theTransformation.SetElement(3, 2, 0.0f);
    theTransformation.SetElement(3, 3, 1.0f);
    theModel.SetModel(inDoppelganger->GetModel()->mModels[inDoppelganger->GetAnimation()]);
    theModel.SetFrame(0);
    theModel.SetLocationAndScale(theTransformation, theScale);
    if (!(ioCount < inMaximum) || !theModel.Intersects(gViewport, gOrthographicView))
        return;
    long theMaximum = inDoppelganger->GetPlayer()->GetUpgrade(inDoppelganger->mUnit->mUnitClass->GetUnitType())
                          ->mMaxHitPoints;
    outObject->mModelScale = theScale;
    outObject->mHealth = (float)inDoppelganger->GetHitPoints() / theMaximum;
    outObject->mIsFogged = true;
    outObject->mOpacity = 1.0f;
    outObject->mModelScale = ((unsigned long)inDoppelganger->mUnit->GetUnitID() % 11 * kEERenderObject_F441354 + 1.0f)
                             * theScale;
    outObject->mObject = inDoppelganger->GetModel();
    outObject->mObject->LoadMedia();
    if (!Fn10CF98(theMap->GetTile(inX - 1, inY - 1)) || !Fn10CF98(theMap->GetTile(inX + 1, inY + 1))) {
        outObject->mScale = inDoppelganger->mUnit->mUnitClass->GetShadowScale();
        outObject->mF1C = inDoppelganger->mUnit->mUnitClass->GetF170();
    } else {
        outObject->mScale = 0.0f;
        outObject->mF1C = 0.0f;
    }
    outObject->mModelIndex = inDoppelganger->GetAnimation();
    unsigned long theFrame = inDoppelganger->GetFrame();
    outObject->mCurrentFrame = theFrame;
    outObject->mNextFrame = theFrame;
    if (theFrame + 1 < outObject->mObject->mModels[outObject->mModelIndex]->GetFrameCount()) {
        outObject->mCurrentFrame = theFrame + 1;
        long theAngle = (sSwayClock + inDoppelganger->mUnit->GetUnitID() * 8) % 360 - 180;
        float theFraction = (UMSin(theAngle * kEERenderObject_F441344) + 1.0f) * 0.5f;
        if (theFraction < 0.0f)
            theFraction = 0.0f;
        else if (theFraction > 1.0f)
            theFraction = 1.0f;
        outObject->mFraction = theFraction;
    } else {
        outObject->mFraction = 0.0f;
    }
    if (ioInfo.mAmbient)
        gAmbientSoundManager.mAnalyzer.CountTree(inDoppelganger->mUnit);
    ioMap.insert(EERenderShadowMap::value_type(outObject->mObject->mTextures[0], outObject));
    gUnitPicker.AddUnit(inDoppelganger->mUnit, theTransformation, outObject->mOpacity);
    EERenderLists::sInstance.mShadows2.insert(EERenderShadowMap::value_type(outObject->mObject->mShadowTexture,
                                                                            outObject));
    ioCount++;
}

// FillUnit's classes, included here (after the unit's init chains) so their statics do not renumber the chains
#include "ee/EEWheel.h"
#include "ee/EEUCBuilding.h"
#include "ee/EEUCGate.h"
#include "ee/EEAreaEffect.h"
#include "ee/EEAreaEffectTemplate.h"

// (0x50d3ad FillUnit: defined after the space-cliff textures' chain below: it needs ee/EEUnitGoal.h, whose class static
// renumbers that chain when included earlier)

// FUNCTION: EXE 0x50e3c8
// Fills a render unit for a remembered unit on a tile the current player cannot see, when the pool has
// room and the model is in the view
// Uses the remembered location (or the tile's centre on the ground), the class's orientation flag and the model;
// sets the render unit's fields, the ground height (0 over water) and the build progress scale
void EERenderObject::FillDoppelgangerUnit(EEDoppelganger* inDoppelganger, EERenderUnit* outUnit, unsigned long& ioCount,
                              EERenderUnitMap& ioMap, unsigned long inMaximum, long inX, long inY,
                              EERenderFrameInfo& ioInfo)
{
    GE3DPoint thePoint;
    GE3DPoint theGround;
    GEPhysicalModel theModel;
    GETransformation theTransformation(false);
    EEMap* theMap = gWorld.mMap;
    float theScale = inDoppelganger->GetScale();
    if (inDoppelganger->IsComplex() == true) {
        inDoppelganger->GetTransformation(theTransformation);
    } else {
        theTransformation.MakeIdentity();
        thePoint.mX = inDoppelganger->mUnit->mUnitClass->mTileSize * 0.5f + inX;
        thePoint.mY = inDoppelganger->mUnit->mUnitClass->mTileSize * 0.5f + inY;
        thePoint.mZ = theMap->mTerrainMesh.GetMeshZ(thePoint.GetX(), thePoint.GetY());
        theTransformation.SetPosition(thePoint.GetX(), thePoint.GetY(), thePoint.mZ);
    }
    if (inDoppelganger->mUnit->mUnitClass->mB2F6 == true)
        theTransformation.SetOrientationYPR(ioInfo.mYaw, 0.0f, 0.0f);
    if (inDoppelganger->mUnit->mUnitClass->mB2F6 == true)
        theTransformation.SetOrientationYPR(ioInfo.mYaw, 0.0f, 0.0f);
    theModel.SetModel(inDoppelganger->GetModel()->mModels[inDoppelganger->GetAnimation()]);
    theModel.SetFrame(0);
    theModel.SetLocationAndScale(theTransformation, theScale);
    if (!(ioCount < inMaximum) || !theModel.Intersects(gViewport, gOrthographicView))
        return;
    outUnit->mInfo = inDoppelganger->mUnit->mUnitClass;
    outUnit->mCategory = inDoppelganger->mUnit->mUnitClass->mCategory;
    outUnit->mUnitClassIndex = inDoppelganger->mUnit->mUnitClass->GetUnitType();
    outUnit->mL118 = inDoppelganger->mUnit->mUnitClass->mFamily;
    outUnit->mShadowScale = !inDoppelganger->IsFoundation() ? inDoppelganger->mUnit->mUnitClass->GetShadowScale() : 0.0f;
    outUnit->mFF4 = inDoppelganger->mUnit->mUnitClass->GetF170();
    outUnit->mName.Clear();
    outUnit->mSource2 = NULL;
    outUnit->mPlayerColor = inDoppelganger->GetPlayer()->mColor;
    outUnit->mAreaEffectStrength = 0;
    outUnit->mScale = theScale;
    outUnit->mOpacity = 1.0f;
    outUnit->mSource2 = NULL;
    outUnit->mIsTinted = false;
    outUnit->mHasSubModelOffset = false;
    outUnit->mGridSize = inDoppelganger->mUnit->mUnitClass->mTileSize;
    outUnit->mIsFogged = true;
    outUnit->mSource = inDoppelganger->GetModel();
    if (!inDoppelganger->IsBuilt() && inDoppelganger->GetHitPoints() > 1) {
        if (outUnit->mInfo->mScaffoldingClass != NULL)
            outUnit->mSource2 = gWorld.mPlayers[outUnit->mPlayerID]->GetUpgrade(outUnit->mInfo->mScaffoldingClass->GetUnitType())
                                  ->mModel;
        if (outUnit->mSource2 != NULL)
            outUnit->mSource2->LoadMedia();
    }
    outUnit->mSource->LoadMedia();
    outUnit->mTransformation = theTransformation;
    if (theMap->mTerrainMesh.GetIsWaterTile(UMFloor(theTransformation.GetX()), UMFloor(theTransformation.GetY()))
        && outUnit->mTransformation.GetZ() >= 0.0f) {
        outUnit->mGroundZ = 0.0f;
    } else {
        thePoint.mX = outUnit->mTransformation.GetX();
        thePoint.mY = outUnit->mTransformation.GetY();
        thePoint.mZ = outUnit->mTransformation.GetZ();
        theMap->GetGroundPoint(thePoint, theGround);
        outUnit->mGroundZ = theGround.GetZ();
    }
    if (!inDoppelganger->IsBuilt() && inDoppelganger->GetHitPoints() != 1) {
        long theMaximum = inDoppelganger->GetPlayer()->GetUpgrade(inDoppelganger->mUnit->mUnitClass->GetUnitType())
                              ->mMaxHitPoints;
        outUnit->mTransformation.Scale(1.0f, 1.0f, (float)inDoppelganger->GetHitPoints() / theMaximum);
    }
    outUnit->mCurrentFrame = inDoppelganger->GetFrame();
    outUnit->mNextFrame = inDoppelganger->GetFrame();
    outUnit->mFraction = 0.0f;
    outUnit->mAnimation = inDoppelganger->GetAnimation();
    outUnit->mCommandType = 0;
    gUnitPicker.AddUnit(inDoppelganger->mUnit, outUnit->mTransformation, outUnit->mOpacity);
    ioMap.insert(EERenderUnitMap::value_type(outUnit->mSource->mTextures[0], outUnit));
    EERenderLists::sInstance.mUnits2.insert(EERenderUnitMap::value_type(outUnit->mSource->mShadowTexture,
                                                                        outUnit));
    ioCount++;
}

// the space-cliff textures (0x4e8ef0; loaded once by Fn10EA27, handed to the terrain mesh) (provisional name)
std::vector<GETexture*> gU4E8EF0;
// SYNTHETIC: EXE 0x50e899   (wrapper)
// SYNTHETIC: EXE 0x50e8a3   (ctor)
// SYNTHETIC: EXE 0x50e8bb   (registrar)
// SYNTHETIC: EXE 0x50e8c7   (dtor)

// FillUnit reads the goal's kind
#include "ee/EEUnitGoal.h"

// FUNCTION: EXE 0x50d3ad
// Fills a render unit for a unit and files it in the picker and the texture maps
// (out of address order: see the note at 0x50d3ad above)
// The class's fields, the team colour, the ring and shadow sizes, then for a complex unit its scale, flash colour,
// gate offset, the selection bars (selected, the player's own pick, or a listed unit), the picked unit's name and
// owner, the animation frames, the orientation on slopes, the hit area-effect ring, the build-progress scale and
// the wall heights; for a placed object its footprint centre (or its carrier's waypoint) and a debug colour;
// finally the ground height (0 over water)
void EERenderObject::FillUnit(EEUnit* inUnit, EERenderUnit* outUnit, EERenderUnitMap& ioMap, EERenderFrameInfo& ioInfo)
{
    EEMap* theMap = gWorld.mMap;
    GE3DPoint thePoint;
    GETerrainMesh* theMesh = &theMap->mTerrainMesh;
    GE3DPoint theGround;
    EETile* theTile = static_cast<EETile*>(inUnit->mTile);
    EEComplexUnit* theComplex = inUnit->GetComplexUnit();
    EEUnitClassUpgrade* theStats = inUnit->mStats;
    EEPlayer* theCurrent = gWorld.GetCurrentPlayer();
    long theTileX = inUnit->GetTileX();
    long theTileY = inUnit->mTileY;
    GEVector theUp(0.0f, 1.0f, 0.0f);
    GEVector theForward;
    bool theFogged = inUnit->mResourceType == kResourceFood && inUnit->mResourceAmount > 0 && theComplex != NULL
                     && inUnit->mUnitClass->mCategory != kUnitCategoryLandCitizen && inUnit->mUnitClass->mCategory != kUnitCategoryNavalCitizen
                     && inUnit->GetPlayer()->GetPlayerID() == 0;
    if (theComplex != NULL) {
        if (ioInfo.mAmbient)
            gAmbientSoundManager.mAnalyzer.CountUnit(theComplex);
    } else if (ioInfo.mAmbient) {
        gAmbientSoundManager.mAnalyzer.CountTree(inUnit);
    }
    outUnit->mInfo = inUnit->mUnitClass;
    outUnit->mGridSize = inUnit->mUnitClass->mTileSize;
    outUnit->mPlayerColor = inUnit->GetPlayer()->mColor;
    outUnit->mCategory = inUnit->mUnitClass->mCategory;
    outUnit->mUnitClassIndex = inUnit->mUnitClass->GetUnitType();
    outUnit->mL118 = inUnit->mUnitClass->mFamily;
    outUnit->mShadowScale = inUnit->GetHitPoints() > 0 ? inUnit->mUnitClass->GetShadowScale() : 0.0f;
    outUnit->mFF4 = inUnit->GetHitPoints() > 0 ? inUnit->mUnitClass->GetF170() : 0.0f;
    outUnit->mSelectionSize = inUnit->mUnitClass->mSelectionSize;
    outUnit->mPlayerID = inUnit->GetPlayer()->GetPlayerID();
    outUnit->mCurrentResearch = kTechNone;
    outUnit->mUnitId = inUnit->GetUnitID();
    outUnit->mSource2 = NULL;
    outUnit->mIsTinted = false;
    outUnit->mHasSubModelOffset = false;
    outUnit->mName.Clear();
    outUnit->mAreaEffectStrength = 0;
    if (!gFreeCamera && theFogged)
        outUnit->mIsFogged = !theTile->IsSeenBy(theCurrent);
    else
        outUnit->mIsFogged = false;
    outUnit->mSource = inUnit->GetModel();
    outUnit->mSource->LoadMedia();
    if (theComplex != NULL) {
        outUnit->mScale = theComplex->mPhysicalModel.GetScale();
        outUnit->mIsTinted = theComplex->mIsTinted;
        if (outUnit->mIsTinted) {
            outUnit->mTintColor = theComplex->mColor;
            outUnit->mFogScale = 0.6f;
        }
        if (theComplex->mUnitClass->mCategory == kUnitCategoryGate) {
            EEUCGate* theGate = dynamic_cast<EEUCGate*>(theComplex->mUnitClass);
            if (theComplex->GetHitPoints() > 0 && theGate->IsSidePart(theComplex)) {
                outUnit->mSubModelOffsetScale = theComplex->GetOpenFraction();
                outUnit->mHasSubModelOffset = true;
            }
        }
        if (gMusicFollowsBattle && theComplex->GetHitPoints() != 0 && theComplex->mActiveGoal != NULL
            && (theComplex->mActiveGoal->mKind == kGoalAttackUnit || theComplex->mActiveGoal->mKind == kGoalAttackGround
                || theComplex->mActiveGoal->mKind == kGoalCastCalamity))
            ioInfo.mNumFightingUnits += 1.0f;
        if (theComplex->mUnitClass->GetUnitType() == kUnitTypePriestTower && theComplex->mIsBuilt
            && theCurrent->mSelectedGroup->Contains(theComplex))
            this->DrawGroundCircle(theComplex->mPhysicalModel.GetLocation(), theStats->mRange, 2);
        if (!theComplex->IsFoundation()) {
            unsigned long theCount = theComplex->GetModel()->mTurretParts.size();
            outUnit->mTurretAngles.resize(theCount, 0.0f);
            outUnit->mTurretModels.resize(theCount, 0);
            unsigned long i;
            for (i = 0; i < theCount; i++) {
                outUnit->mTurretAngles[i] = theComplex->mTurretAngles[i];
                outUnit->mTurretModels[i] = outUnit->mSource->mTurretParts[i]->mModel;
            }
            theCount = theComplex->mNumOrbitals;
            outUnit->mOrbitalAngles.resize(theCount, 0.0f);
            outUnit->mOrbitalModels.resize(theCount, 0);
            for (i = 0; i < theCount; i++) {
                outUnit->mOrbitalAngles[i] = theComplex->mOrbitals[i]->mAngle;
                outUnit->mOrbitalModels[i] = theComplex->mOrbitals[i]->mModel;
            }
            theCount = theComplex->mNumWheels;
            outUnit->mWheelAngles.resize(theCount, 0.0f);
            outUnit->mWheelModels.resize(theCount, 0);
            for (i = 0; i < theCount; i++) {
                EEWheel* theWheel = theComplex->mWheels[i];
                outUnit->mWheelAngles[i] = theWheel->mAngle;
                outUnit->mWheelModels[i] = theWheel->mModel;
            }
        }
        if ((!gFreeCamera && (theCurrent->mSelectedGroup->Contains(theComplex) || theCurrent->mSelectedViewedUnit == inUnit))
            || (gFreeCamera && inUnit->mIsEditorSelected)
            || std::find(gU4E9230.begin(), gU4E9230.end(), inUnit->GetUnitID()) != gU4E9230.end()) {
            if (!inUnit->mUnitClass->IsLandBuilding() || inUnit->GetHitPoints() != 0) {
                switch (sL52906C) {
                case 0:
                    outUnit->mSelectionSize = inUnit->mUnitClass->mSelectionSize;
                    break;
                case 1:
                    outUnit->mSelectionSize = theComplex->GetLineOfSight() * 2.0f;
                    break;
                case 2:
                    outUnit->mSelectionSize = inUnit->mStats->mRange * 2.0f;
                    break;
                }
                if (theComplex->GetNumCargoUnits() > 0
                    && (theComplex->GetPlayer() == theCurrent || theComplex->GetPlayer()->mEnemies[theCurrent->GetPlayerID()] == 0)) {
                    outUnit->mHealthBars.resize(theComplex->GetNumCargoUnits() + 1, 0.0f);
                    for (unsigned long j = 0; j < theComplex->GetNumCargoUnits(); j++)
                        outUnit->mHealthBars[j + 1] = (float)theComplex->mCargo[j]->mHitPoints
                                               / theComplex->mCargo[j]->mStats->mMaxHitPoints;
                }
                outUnit->mHealthBars[0] = (float)inUnit->GetHitPoints() / inUnit->mStats->mMaxHitPoints;
                outUnit->mHealth = inUnit->mStats->mPower == 0 ? -1.0f
                                                              : (float)theComplex->mPower / theStats->mPower;
                EERenderLists::sInstance.mSelectedUnits.push_back(outUnit);
            }
        } else if (ioInfo.mPickedUnit == inUnit) {
            outUnit->mName = theComplex->mStats->mName;
            if (theComplex->GetNumCargoUnits() > 0
                && (theComplex->GetPlayer() == theCurrent || theComplex->GetPlayer()->mEnemies[theCurrent->GetPlayerID()] == 0)) {
                outUnit->mHealthBars.resize(theComplex->GetNumCargoUnits() + 1, 0.0f);
                for (unsigned long j = 0; j < theComplex->GetNumCargoUnits(); j++)
                    outUnit->mHealthBars[j + 1] = (float)theComplex->mCargo[j]->mHitPoints
                                           / theComplex->mCargo[j]->mStats->mMaxHitPoints;
            }
            outUnit->mHealthBars[0] = (float)inUnit->GetHitPoints() / inUnit->mStats->mMaxHitPoints;
            outUnit->mHealth = inUnit->mStats->mPower == 0 ? -1.0f : (float)theComplex->mPower / theStats->mPower;
            EERenderLists::sInstance.mSelectedUnit = outUnit;
            if (inUnit->GetPlayer() != gWorld.mPlayers[0] && inUnit->GetPlayer() != gWorld.GetCurrentPlayer()) {
                gU529044 = outUnit;
                if (!inUnit->GetPlayer()->IsHuman() && !inUnit->GetPlayer()->mCivilization.GetUnchanged())
                    gU529030 = inUnit->GetPlayer()->mCivilization.mName;
                else
                    gU529030 = inUnit->GetPlayer()->mName;
            }
        } else if (gCollisionSphereMode != 0) {
            outUnit->mName = theComplex->mStats->mName;
        }
        if (theComplex->mCurrentResearch != kTechNone && theComplex->mProductionTime > 0)
            outUnit->mCurrentResearch = theComplex->mCurrentResearch;
        if (!theComplex->IsFoundation()) {
            outUnit->mCurrentFrame = theComplex->mPhysicalModel.GetFrame();
            outUnit->mNextFrame = theComplex->mNextFrame;
            outUnit->mFraction = theComplex->mAnimationFraction;
        } else {
            outUnit->mCurrentFrame = 0;
            outUnit->mNextFrame = 0;
            outUnit->mFraction = 0.0f;
        }
        outUnit->mTransformation = theComplex->mPhysicalModel.GetLocation();
        if (theComplex->mUnitClass->mB2F6) {
            outUnit->mTransformation.Apply(theUp, theForward);
            outUnit->mTransformation.SetOrientationYPR(ioInfo.mYaw, UMArcCos(-theForward.mZ) - 1.5707964f, 0.0f);
        }
        outUnit->mCommandType = theComplex->mCommand != NULL ? theComplex->mCommand->mType : 0;
        outUnit->mAnimation = theComplex->GetAnimation();
        for (std::vector<EEAreaEffect*>::iterator it = theComplex->mAffectingAreaEffects.mItems.begin();
             it != theComplex->mAffectingAreaEffects.mItems.end(); it++) {
            if (!(inUnit->GetHitPoints() > 0))
                continue;
            long theKind = (*it)->mTemplate->mKind;
            if (theKind == kAreaEffectMoraleFromBuildings || theKind == kAreaEffectMoraleFromMilitaryHeroes || theKind == kAreaEffectMorale) {
                outUnit->mAreaEffectStrength = (*it)->GetStrength();
                break;
            }
        }
        if (!theComplex->mIsBuilt && !theComplex->IsFoundation()) {
            if (outUnit->mInfo->mScaffoldingClass != NULL)
                outUnit->mSource2 = gWorld.mPlayers[outUnit->mPlayerID]->GetUpgrade(outUnit->mInfo->mScaffoldingClass->GetUnitType())
                                      ->mModel;
            if (outUnit->mSource2 != NULL)
                outUnit->mSource2->LoadMedia();
            float theZ = outUnit->mTransformation.mMatrix[2][3];
            outUnit->mTransformation.mMatrix[2][3] = 0.0f;
            outUnit->mTransformation.Scale(1.0f, 1.0f, (float)inUnit->GetHitPoints() / inUnit->mStats->mMaxHitPoints);
            outUnit->mTransformation.mMatrix[2][3] = theZ;
        } else if (theComplex->IsFoundation()) {
            if (theComplex->mUnitClass->GetUnitType() == kUnitTypeSpaceTurret)
                outUnit->mTransformation.mMatrix[2][3] = outUnit->mTransformation.mMatrix[2][3] - gSpaceTurretHeight;
            else if (theComplex->mUnitClass->GetUnitType() == kUnitTypeOrbitalSpaceStation)
                outUnit->mTransformation.mMatrix[2][3] = outUnit->mTransformation.mMatrix[2][3] - gSpaceStationHeight;
        }
    } else {
        outUnit->mScale = theStats->GetScale();
        if (inUnit == theCurrent->mSelectedViewedUnit || (gFreeCamera && inUnit->mIsEditorSelected)
            || std::find(gU4E9230.begin(), gU4E9230.end(), inUnit->GetUnitID()) != gU4E9230.end()) {
            outUnit->mHealth = -1.0f;
            outUnit->mHealthBars.clear();
            EERenderLists::sInstance.mSelectedUnits.push_back(outUnit);
        }
        thePoint.SetX(outUnit->mGridSize * 0.5f + inUnit->GetTileX());
        thePoint.SetY(outUnit->mGridSize * 0.5f + inUnit->mTileY);
        thePoint.SetZ(theMesh->GetMeshZ(thePoint.GetX(), thePoint.GetY()));
        EEComplexUnitWaypoint* theWaypoint;
        if (inUnit->IsPathMarker() && gWorld.GetUnit(inUnit->GetHitPoints()) != NULL) {
            EEComplexUnit* theCarrier = gWorld.GetUnit(inUnit->GetHitPoints())->GetComplexUnit();
            if (theCarrier != NULL
                && gWorld.GetUnit(inUnit->GetHitPoints())->GetComplexUnit()->FindWaypointAtTile(inUnit->GetTileX(), inUnit->mTileY,
                                                                                theWaypoint)) {
                thePoint = theWaypoint->mLocation;
                if (theMesh->GetIsValidTile(UMFloor(thePoint.GetX()), UMFloor(thePoint.GetY())))
                    thePoint.SetZ(theMesh->GetMeshZ(thePoint.GetX(), thePoint.GetY()));
                unsigned long theID = theCarrier->GetUnitID() & 0xffffff;
                switch (theID % 3) {
                case 0:
                    outUnit->mTintColor.mRed = theID % 6 / 5.0f;
                    outUnit->mTintColor.mGreen = ((theCarrier->GetUnitID() & 0xffffff) + 1) / 5.0f;
                    outUnit->mTintColor.mBlue = ((theCarrier->GetUnitID() & 0xffffff) + 2) / 5.0f;
                    break;
                case 1:
                    outUnit->mTintColor.mRed = (theID + 1) / 5.0f;
                    outUnit->mTintColor.mGreen = (theCarrier->GetUnitID() & 0xffffff) % 6 / 5.0f;
                    outUnit->mTintColor.mBlue = ((theCarrier->GetUnitID() & 0xffffff) + 2) / 5.0f;
                    break;
                case 2:
                    outUnit->mTintColor.mRed = (theID + 2) / 5.0f;
                    outUnit->mTintColor.mGreen = ((theCarrier->GetUnitID() & 0xffffff) + 1) / 5.0f;
                    outUnit->mTintColor.mBlue = (theCarrier->GetUnitID() & 0xffffff) % 6 / 5.0f;
                    break;
                }
            }
        }
        inUnit->mUnitClass->GroundLocation(thePoint);
        if (inUnit->IsPathMarker() && theTile->GetHeight() < gRenderEnvironment.mSeaLevel)
            thePoint.SetZ(gRenderEnvironment.mSeaLevel);
        outUnit->mTransformation.SetOrientationYPR(inUnit->mUnitClass->mB2F6 ? ioInfo.mYaw : 0.0f, 0.0f, 0.0f);
        outUnit->mTransformation.mMatrix[0][3] = thePoint.GetX();
        outUnit->mTransformation.mMatrix[1][3] = thePoint.GetY();
        outUnit->mTransformation.mMatrix[2][3] = thePoint.mZ;
        outUnit->mCurrentFrame = 0;
        outUnit->mNextFrame = 0;
        outUnit->mFraction = 0.0f;
        outUnit->mCommandType = 0;
        outUnit->mAnimation = 0;
    }
    if (theMesh->GetIsWaterTile(theTileX, theTileY) && outUnit->mTransformation.GetZ() >= 0.0f) {
        outUnit->mGroundZ = 0.0f;
    } else {
        thePoint.SetX(outUnit->mTransformation.GetX());
        thePoint.SetY(outUnit->mTransformation.GetY());
        thePoint.SetZ(outUnit->mTransformation.GetZ());
        theMap->GetGroundPoint(thePoint, theGround);
        outUnit->mGroundZ = theGround.mZ;
    }
    gUnitPicker.AddUnit(inUnit, outUnit->mTransformation, outUnit->mOpacity);
    ioMap.insert(EERenderUnitMap::value_type(outUnit->mSource->mTextures[outUnit->mAnimation], outUnit));
    if (theComplex != NULL && !theComplex->IsFoundation())
        EERenderLists::sInstance.mUnits2.insert(EERenderUnitMap::value_type(outUnit->mSource->mShadowTexture, outUnit));
}


// loads the space-cliff and cliff-edge textures into gU4E8EF0 (a static function: emitted after its user)
static void Fn10EA27()
{
    GETextureDescription theDescription;
    theDescription.mPixelDepth = 16;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    for (long i = 0; i < 13; i++) {
        GETexture* theTexture = gTextureManager->LoadTexture(sCliffTextures[i], theDescription);
        theTexture->SetStageAlphaMode(kStageOpModulate, -1);
        theTexture->SetStageColorMode(kStageOpModulate, -1);
        theTexture->Reference();
        gU4E8EF0.push_back(theTexture);
    }
}

// FUNCTION: EXE 0x50e8d1
// Sets up the terrain pass: frame state, space-cliff textures, the visible terrain and its lighting, then
// the camera transform (at most every 100 ms while the camera follows a unit)
void EERenderObject::Fn10E8D1()
{
    static unsigned long sLastTime = TMGetTime();
    GEViewport& theViewport = gRasterizer->GetViewport(gScreenIndex);
    EEMap* theMap = gWorld.mMap;
    unsigned long theNow = TMGetTime();
    unsigned long theElapsed = theNow - sLastTime;
    if (!gCameraFollowing) {
        this->Fn10C036();
        if (theMap->mTerrainMesh.GetIsSpaceMap() && theMap->mTerrainMesh.NumSpaceCliffTextures() < 1) {
            if (gU4E8EF0.size() == 0)
                Fn10EA27();
            theMap->mTerrainMesh.SetSpaceCliffTextures(gU4E8EF0);
        }
        theMap->mTerrainMesh.UpdateVisibleTerrain(gWorld.mTransformation, theViewport, 2, 1, 3, gOrthographicView);
        Fn10F833();
        gRasterizer->Light(theMap->mTerrainMesh, gScreenIndex);
    }
    if (!gCameraFollowing || theElapsed >= 100) {
        gRasterizer->TransformToCamera(theMap->mTerrainMesh, theViewport, theElapsed / 1000.0f, gCameraFollowing,
                                       gOrthographicView);
        sLastTime = theNow;
    }
    gCameraFollowing = true;
}

// SYNTHETIC: EXE 0x50ea27
// (Fn10EA27, the static function above)

// FUNCTION: EXE 0x50eabe
// Builds the frame's render lists: resets the pools, picks units, walks the visible tiles (units, map
// objects, remembered ones) and the ghost units, and updates the music intensity and ambient sounds
void EERenderObject::FillRenderLists()
{
    std::set<EEUnit*> theUnits;
    std::set<EEDoppelganger*> theDoppelgangers;
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    EEMap* theMap = gWorld.mMap;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEUICSelectUnits* theSelection
        = dynamic_cast<EEUICSelectUnits*>(gUIManager->GetMainGameForm()->GetControl(kMainGameSelectUnits));
    EERenderFrameInfo theInfo;
    float theCount = 0.0f;
    unsigned long theTime = TMGetTime();
    if (!gB529068) {
        if (theTime - sU529094 > 0x21) {
            sSwayClock += 2;
            sU529094 = theTime;
        }
        this->Fn10E8D1();
        Fn10B7E1(EERenderLists::sInstance.mRenderShadows, EERenderLists::sInstance.mRenderShadowCount);
        Fn10B80B(EERenderLists::sInstance.mRenderUnits, EERenderLists::sInstance.mRenderUnitCount);
        for (unsigned long i = 0; i < gU529050.size(); i++)
            gU529050[i].mTexture->Dereference();
        gU529050.clear();
        EERenderLists::sInstance.mShadows0.clear();
        EERenderLists::sInstance.mUnits0.clear();
        EERenderLists::sInstance.mShadows1.clear();
        EERenderLists::sInstance.mUnits1.clear();
        EERenderLists::sInstance.mShadows2.clear();
        EERenderLists::sInstance.mUnits2.clear();
        EERenderLists::sInstance.mSelectedObjects.clear();
        EERenderLists::sInstance.mSelectedUnits.clear();
        Fn10B8AD();
        gU529044 = NULL;
        if (!gFreeCamera) {
            if (theSelection->GetIsDrawn())
                gUnitPicker.PickUnits(theSelection->GetSelectionRectangle(), thePlayer, 0, gU4E9230);
            else
                gU4E9230.clear();
        } else {
            EESEUnit* theScreen = gUIManager->mScenarioEditorForm != NULL ?
                gUIManager->mScenarioEditorForm->GetUnitScreen() : NULL;
            EEUICSelectUnits* theEditorSelection = theScreen != NULL ? theScreen->mSelectUnits : NULL;
            EESEUICTriggerSelectUnits* theTriggerSelection = dynamic_cast<EESEUICTriggerSelectUnits*>(
                gUIManager->mScenarioEditorForm->GetControlById(kSETSObjectSelectUnits));
            if (theEditorSelection != NULL && theEditorSelection->GetIsDrawn())
                gUnitPicker.PickUnitsInRectangle(theEditorSelection->GetSelectionRectangle(), 0, gU4E9230);
            else if (theTriggerSelection != NULL && theTriggerSelection->GetIsDrawn())
                gUnitPicker.PickUnitsInRectangle(theTriggerSelection->GetSelectionRectangle(), 0, gU4E9230);
            else
                gU4E9230.clear();
        }
        theInfo.mNumFightingUnits = 0.0f;
        if (!gFreeCamera) {
            gAmbientSoundManager.mFrameCount++;
            if (gAmbientSoundManager.IsUpdateDue())
                theInfo.mAmbient = true;
            else
                theInfo.mAmbient = false;
        } else {
            theInfo.mAmbient = false;
        }
        gWorld.mTransformation.Apply(theForward, theDirection);
        theInfo.mYaw = UMArcTan2(-theDirection.GetX(), theDirection.GetY()) - 0.78539819f;
        sL52908C = (gB529065 ? sL52908C : gPhysics->mTicksRun) + 1;
        theInfo.mFrame = sL52908C;
        theInfo.mPickedUnit = !gFreeCamera ? gUnitPicker.PickUnit(gMouse->mPosition, 1) : NULL;
        if (theInfo.mPickedUnit != NULL && theInfo.mPickedUnit->GetHitPoints() <= 0) {
            theInfo.mPickedUnit->Dereference();
            theInfo.mPickedUnit = NULL;
        }
        gUnitPickerLock->Enter();
        gUnitPicker.Reset();
        unsigned long theRows = theMap->mTerrainMesh.GetNumVisibleTileRows();
        for (unsigned long theRow = 0; theRow < theRows; theRow++) {
            long theY;
            long theFirst;
            long theLast;
            theMap->mTerrainMesh.GetVisibleTileRow(theRow, theY, theFirst, theLast);
            for (long theX = theFirst; theX <= theLast; theX++) {
                EETile* theTile = theMap->GetTile(theX, theY);
                if (!theTile->IsVisibleTo(thePlayer) && !gFreeCamera)
                    continue;
                for (RTSTileUnitNode* theNode = theTile->mUnits; theNode != NULL; theNode = theNode->mNext) {
                    EEUnit* theUnit = theNode->mUnit;
                    if (theUnit->mComplexUnit != NULL) {
                        theUnit = theUnit->mComplexUnit;
                        while (static_cast<EEComplexUnit*>(theUnit)->mPreviousPart != NULL)
                            theUnit = static_cast<EEComplexUnit*>(theUnit)->mPreviousPart;
                    }
                    do {
                        if (this->IsUnitDrawn(theUnit, theTile, theUnits) && !gUnitPicker.Contains(theUnit)) {
                            if (!theUnit->GetUnitClass()->mIsTree) {
                                if (theUnit->mComplexUnit != NULL)
                                    theCount += 1.0f;
                                EERenderUnit* theRenderUnit
                                    = &EERenderLists::sInstance.mRenderUnits[EERenderLists::sInstance.mRenderUnitCount];
                                this->FillUnit(theUnit, theRenderUnit,
                                         this->GetUnitOpacity(theUnit, theRenderUnit->mOpacity) ? EERenderLists::sInstance.mUnits1
                                                                                     : EERenderLists::sInstance.mUnits0,
                                         theInfo);
                                EERenderLists::sInstance.mRenderUnitCount++;
                            } else {
                                EERenderShadow* theObject
                                    = &EERenderLists::sInstance.mRenderShadows[EERenderLists::sInstance.mRenderShadowCount];
                                this->FillObject(theUnit, theObject,
                                         this->GetUnitOpacity(theUnit, theObject->mOpacity) ? EERenderLists::sInstance.mShadows1
                                                                                 : EERenderLists::sInstance.mShadows0,
                                         theInfo);
                                EERenderLists::sInstance.mRenderShadowCount++;
                            }
                        }
                        theUnit = theUnit->mComplexUnit != NULL ? theUnit->mComplexUnit->mNextPart : NULL;
                    } while (theUnit != NULL);
                }
                if (!theTile->IsSeenBy(thePlayer) && !gFreeCamera) {
                    for (unsigned long k = 0; k < theTile->GetNumDoppelgangers(); k++) {
                        EEDoppelganger* theDoppelganger = theTile->mDoppelgangers[k];
                        if (theDoppelganger->mPlayer != thePlayer)
                            continue;
                        if (theDoppelganger->mUnit->mTileNode != NULL && theDoppelganger->mUnit->mComplexUnit->IsFoundation()
                            && theDoppelganger->GetPlayer() != thePlayer)
                            continue;
                        if (theDoppelganger->mUnit->GetUnitClass()->mTileSize > 1) {
                            if (theDoppelgangers.find(theDoppelganger) != theDoppelgangers.end())
                                continue;
                            theDoppelgangers.insert(theDoppelganger);
                        }
                        if (!theDoppelganger->mUnit->GetUnitClass()->mIsTree) {
                            this->FillDoppelgangerUnit(theDoppelganger,
                                     &EERenderLists::sInstance.mRenderUnits[EERenderLists::sInstance.mRenderUnitCount],
                                     EERenderLists::sInstance.mRenderUnitCount,
                                     theDoppelganger->mUnit->GetUnitClass()->mIsObstacle ? EERenderLists::sInstance.mUnits1
                                                                                : EERenderLists::sInstance.mUnits0,
                                     0x200, theX, theY, theInfo);
                        } else {
                            this->FillDoppelgangerObject(theDoppelganger,
                                     &EERenderLists::sInstance.mRenderShadows[EERenderLists::sInstance.mRenderShadowCount],
                                     EERenderLists::sInstance.mRenderShadowCount, EERenderLists::sInstance.mShadows0,
                                     0x600, theX, theY, theInfo);
                        }
                    }
                }
            }
        }
        gUnitPickerLock->Leave();
        this->FillGhostRecords();
        gB529068 = true;
        if (gMusicFollowsBattle && theTime - sU529090 >= 0xfa) {
            sU529090 = theTime;
            float theAverage = theCount != 0.0f ? theInfo.mNumFightingUnits / theCount : 0.0f;
            unsigned long theIntensity;
            switch (gMusicManager->mIntensity) {
            case 3:
                if (theAverage < -0.0001f || theAverage > 0.0001f)
                    theIntensity = 3;
                else
                    theIntensity = 1;
                break;
            case 2:
                if (theAverage < -0.0001f || theAverage > 0.0001f)
                    theIntensity = theAverage >= 0.65f ? 3 : 2;
                else
                    theIntensity = 1;
                break;
            case 0:
            case 1:
                if (theAverage < -0.0001f || theAverage > 0.0001f)
                    theIntensity = theAverage < 0.5f ? 2 : 3;
                else
                    theIntensity = 1;
                break;
            default:
                theIntensity = 1;
                break;
            }
            gMusicManager->SetIntensity(theIntensity);
        }
        if (theInfo.mAmbient)
            gAmbientSoundManager.Update();
        if (theInfo.mPickedUnit != NULL)
            theInfo.mPickedUnit->Dereference();
    }
}

// FUNCTION: EXE 0x50f239
// Releases the scene: drops the effect queues' references, empties the render pools and lists, releases
// the shared bitmap and resets the unit picker (only after EEInitialiseScene)
void EEReleaseScene()
{
    EEEffectQueue::iterator theEntry;
    std::deque<EEEffect*>::iterator theEffect;
    if (!sSceneInitialised)
        return;
    gWorld.mLock->ReadLock();
    for (unsigned long i = 0; i < 5; i++) {
        for (theEntry = gEffectQueues[i].begin(); theEntry != gEffectQueues[i].end(); theEntry++) {
            for (theEffect = theEntry->second.begin(); theEffect != theEntry->second.end(); theEffect++)
                (*theEffect)->Release();
        }
        gEffectQueues[i].clear();
    }
    gWorld.mLock->ReadUnlock();
    Fn10B7E1(EERenderLists::sInstance.mRenderShadows, EERenderLists::sInstance.mRenderShadowCount);
    Fn10B80B(EERenderLists::sInstance.mRenderUnits, EERenderLists::sInstance.mRenderUnitCount);
    EERenderLists::sInstance.mShadows0.clear();
    EERenderLists::sInstance.mUnits0.clear();
    EERenderLists::sInstance.mShadows1.clear();
    EERenderLists::sInstance.mUnits1.clear();
    EERenderLists::sInstance.mShadows2.clear();
    EERenderLists::sInstance.mUnits2.clear();
    EERenderLists::sInstance.mSelectedObjects.clear();
    EERenderLists::sInstance.mSelectedUnits.clear();
    EERenderLists::sInstance.mRenderUnitCount = 0;
    EERenderLists::sInstance.mRenderShadowCount = 0;
    Fn10B8AD();
    sU529060->Dereference();
    gUnitPicker.Reset();
    sSceneInitialised = false;
}

// FUNCTION: EXE 0x50f37d
// Starts fading the lights up to full over the given number of frames (at once for 0)
void EEStartLightFadeIn(unsigned long inFrames)
{
    sL529070 = 0;
    sF529078 = 0.0f;
    sLightFadeStart = sLightFade;
    if (inFrames == 0) {
        sF529074 = 3.4028235e+38f;
        return;
    }
    sF529074 = 1.0f / (float)inFrames;
}

// FUNCTION: EXE 0x50f3c6
// Starts fading the lights down to black over the given number of frames (at once for 0)
void EEStartLightFadeOut(unsigned long inFrames)
{
    sLightFadeStart = sLightFade;
    sF529078 = 0.0f;
    sL529070 = 1;
    if (inFrames == 0) {
        sF529074 = 3.4028235e+38f;
        return;
    }
    sF529074 = 1.0f / (float)inFrames;
}

// FUNCTION: EXE 0x50f412
// Computes a unit's opacity this frame (see-through modes, its own fade, and buildings fading in front of
// the near clipping plane); returns whether it is drawn translucent
bool EERenderObject::GetUnitOpacity(EEUnit* inUnit, float& outOpacity)
{
    EEComplexUnit* theComplexUnit = inUnit->mComplexUnit;
    bool theResult = false;
    outOpacity = 1.0f;
    if (gRenderEnvironment.mShowHiddenUnits == 1) {
        if (inUnit->mUnitClass->mIsObstacle || (inUnit->mHiddenFlags & kHiddenFlagMoved) == kHiddenFlagMoved) {
            // only when the bool Virtual40 returns is set (0x10f452 `test al, al`)
            if (inUnit->UpdateHidden()) {
                theResult = true;
                outOpacity = kEERenderObject_F441350;
            }
        }
    } else if (gRenderEnvironment.mShowHiddenUnits == 2) {
        if (inUnit->mUnitClass->mIsObstacle) {
            theResult = true;
            outOpacity = kEERenderObject_F441350;
        }
    }
    if (theComplexUnit != NULL)
        theResult = Fn23CEE2(theComplexUnit, theResult, outOpacity);
    if (inUnit->mUnitClass->IsLandBuilding() && !gWorld.mInCinematic && !gOrthographicView) {
        GE3DPoint theCenter;
        GE3DPoint theView;
        float theScale;
        unsigned long theFrame;
        GEModel* theModel;
        float theRange = 1.0f / (gViewport.GetRearClippingDistance() - gViewport.GetFrontClippingDistance());
        float theLimit = kEERenderObject_F44136C * theRange;
        if (theComplexUnit != NULL) {
            theModel = inUnit->GetModel()->mModels[theComplexUnit->GetAnimation()];
            theScale = theComplexUnit->mPhysicalModel.GetScale();
            theFrame = theComplexUnit->mPhysicalModel.GetFrame();
            theComplexUnit->mPhysicalModel.GetLocation().Apply(theModel->GetCenterPoint(), theCenter);
        } else {
            GETransformation theLocation;
            theLocation.SetX(inUnit->mTileX + 0.5f);
            theLocation.SetY(inUnit->GetTileY() + 0.5f);
            theLocation.SetZ(gWorld.mMap->mTerrainMesh.GetMeshZ(theLocation.GetX(), theLocation.GetY()));
            theModel = inUnit->GetModel()->mModels[0];
            theFrame = 0;
            theScale = inUnit->mStats->GetScale();
            theLocation.Apply(theModel->GetCenterPoint(), theCenter);
        }
        gViewport.GetCameraInverse().Apply(theCenter, theView);
        float theDepth = (theView.GetY() - theModel->GetBoundingRadius(theFrame) * theScale
                          - gViewport.GetFrontClippingDistance()) * theRange;
        if (theDepth < theLimit) {
            theDepth /= theLimit;
            theResult = true;
            if (theDepth < 0.0f)
                theDepth = 0.0f;
            if (theDepth < outOpacity)
                outOpacity = theDepth;
        }
    }
    return theResult;
}

// FUNCTION: EXE 0x50f60f
// Turns on linear fog in the fog colour dimmed by the light fade, ending at the rear clipping distance
// (off in the orthographic view)
void Fn10F60F()
{
    GEColor theColor;
    theColor.SetRed(gU4E9220.GetRed() * sLightFade);
    theColor.SetGreen(gU4E9220.GetGreen() * sLightFade);
    theColor.SetBlue(gU4E9220.GetBlue() * sLightFade);
    unsigned long theScreen = gScreenIndex;
    if (!gOrthographicView) {
        gRasterizer->SetFog(1, theScreen);
        gRasterizer->SetFogColor(theColor, theScreen);
        gRasterizer->SetFogRange(gViewport.GetRearClippingDistance()
                                 - (kEERenderObject_F441364 * gZoomLock + kEERenderObject_F441368),
                                 gViewport.GetRearClippingDistance(), theScreen);
        gRasterizer->SetFogType(1, theScreen);
    } else {
        gRasterizer->SetFog(0, theScreen);
    }
}

// FUNCTION: EXE 0x50f6c7
// Turns the fog off
void Fn10F6C7()
{
    gRasterizer->SetFog(0, gScreenIndex);
}

// FUNCTION: EXE 0x50f6de
// Turns lighting off (no ambient light, no lights)
void Fn10F6DE()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetAmbientIntensity(0.0f, theScreen);
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(0, theScreen);
}

// FUNCTION: EXE 0x50f71c
// Lights with the environment's prelit colour as ambient, dimmed by the light fade, and no lights
void Fn10F71C()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetAmbientColor(gRenderEnvironment.mPrelitColor, theScreen);
    gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
}

// FUNCTION: EXE 0x50f772
// Lights with the environment's second ambient colour (the clouds'), dimmed by the light fade, and no lights
void Fn10F772()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetAmbientColor(gRenderEnvironment.mColor54, theScreen);
    gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
}

// FUNCTION: EXE 0x50f7c8
// Lights with white ambient light dimmed by the light fade, and no lights
void Fn10F7C8()
{
    GEColor theWhite(1.0f, 1.0f, 1.0f);
    unsigned long theScreen = gScreenIndex;
    gRasterizer->SetAmbientColor(theWhite, theScreen);
    gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
}

// FUNCTION: EXE 0x50f833
// Sets the terrain lighting: the environment's terrain ambient light and the sun light, dimmed by the fade
void Fn10F833()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
    gRasterizer->SetAmbientColor(gRenderEnvironment.mTerrainAmbientColor, theScreen);
    gRasterizer->SetAmbientIntensity(gRenderEnvironment.mTerrainAmbientIntensity * sLightFade, theScreen);
    gU4E9198.SetColor(gRenderEnvironment.mTerrainLightColor);
    gU4E9198.SetIntensity(gRenderEnvironment.mTerrainLightIntensity * sLightFade);
    gU4E9198.SetDirection(gU4E9210);
    gRasterizer->AddLight(gU4E9198, theScreen, gU4E9190);
}

// FUNCTION: EXE 0x50f8de
// Sets the map objects' lighting: unit ambient and a flattened sun light when unit lighting is on, else the
// prelit colour at a fixed intensity
void Fn10F8DE()
{
    GEVector theDirection = gU4E9210;
    unsigned long theScreen = gScreenIndex;
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
    if (gUnitLighting) {
        theDirection.SetZ(gU4E9210.GetZ() * kEERenderObject_F44135C);
        theDirection.Normalize();
        gRasterizer->SetAmbientColor(gRenderEnvironment.mUnitAmbientColor, theScreen);
        gRasterizer->SetAmbientIntensity(gRenderEnvironment.mUnitAmbientIntensity * (sLightFade * kEERenderObject_F441358), theScreen);
        gU4E9198.SetColor(gRenderEnvironment.mUnitLightColor);
        gU4E9198.SetIntensity(gRenderEnvironment.mUnitLightIntensity * sLightFade);
        gU4E9198.SetDirection(theDirection);
        gRasterizer->AddLight(gU4E9198, theScreen, gU4E9190);
    } else {
        gRasterizer->SetAmbientColor(gRenderEnvironment.mPrelitColor, theScreen);
        gRasterizer->SetAmbientIntensity(kEERenderObject_F441358, theScreen);
    }
}

// FUNCTION: EXE 0x50fa01
// Sets the units' lighting: unit ambient and the sun light when unit lighting is on, else the prelit colour
void Fn10FA01()
{
    unsigned long theScreen = gScreenIndex;
    gRasterizer->RemoveAllLights(theScreen);
    gRasterizer->SetLighting(1, theScreen);
    if (gUnitLighting) {
        gRasterizer->SetAmbientColor(gRenderEnvironment.mUnitAmbientColor, theScreen);
        gRasterizer->SetAmbientIntensity(gRenderEnvironment.mUnitAmbientIntensity * sLightFade, theScreen);
        gU4E9198.SetColor(gRenderEnvironment.mUnitLightColor);
        gU4E9198.SetIntensity(gRenderEnvironment.mUnitLightIntensity * sLightFade);
        gU4E9198.SetDirection(gU4E9210);
        gRasterizer->AddLight(gU4E9198, theScreen, gU4E9190);
    } else {
        gRasterizer->SetAmbientColor(gRenderEnvironment.mPrelitColor, theScreen);
        gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    }
}

// FUNCTION: EXE 0x50fae8
// Restores the units' lighting at full strength for a unit in view (not fogged)
void Fn10FAE8()
{
    unsigned long theScreen = gScreenIndex;
    if (gUnitLighting) {
        gRasterizer->SetAmbientIntensity(gRenderEnvironment.mUnitAmbientIntensity * sLightFade, theScreen);
        gU4E9198.SetIntensity(gRenderEnvironment.mUnitLightIntensity * sLightFade);
        gRasterizer->UpdateLight(gU4E9190, gU4E9198, theScreen);
    } else {
        gRasterizer->SetAmbientColor(gRenderEnvironment.mPrelitColor, theScreen);
        gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    }
}

// FUNCTION: EXE 0x50fb74
// Halves the units' lighting for a fogged unit (the dim prelit colour without unit lighting)
void Fn10FB74()
{
    unsigned long theScreen = gScreenIndex;
    if (gUnitLighting) {
        gRasterizer->SetAmbientIntensity(gRenderEnvironment.mUnitAmbientIntensity * sLightFade * 0.5f, theScreen);
        gU4E9198.SetIntensity(gRenderEnvironment.mUnitLightIntensity * sLightFade * 0.5f);
        gRasterizer->UpdateLight(gU4E9190, gU4E9198, theScreen);
    } else {
        gRasterizer->SetAmbientColor(gRenderEnvironment.mDimPrelitColor, theScreen);
        gRasterizer->SetAmbientIntensity(sLightFade, theScreen);
    }
}

// FUNCTION: EXE 0x50fc0c
// Posts the game's render objects, the user interface's last, to the graphics thread
void EEPostGraphicsAll()
{
    gGraphics->PostClearRenderObjects();
    gGraphics->PostPushRenderObject(gU565360);
    gGraphics->PostPushRenderObject(gU56535C);
    gGraphics->PostPushRenderObject(gU565358);
    gGraphics->PostPushRenderObject(gU565354);
    gGraphics->PostPushRenderObject(gU565350);
    gGraphics->PostPushRenderObject(gU56534C);
}

// FUNCTION: EXE 0x50fc7e
// Posts the game's render objects without the user interface's to the graphics thread
void EEResetGraphicsValues()
{
    gGraphics->PostClearRenderObjects();
    gGraphics->PostPushRenderObject(gU565360);
    gGraphics->PostPushRenderObject(gU56535C);
    gGraphics->PostPushRenderObject(gU565358);
    gGraphics->PostPushRenderObject(gU565354);
    gGraphics->PostPushRenderObject(gU565350);
}

// the template and inline COMDATs closing the unit (std containers, the inline ctors, the ICF-kept twins):
// SYNTHETIC: EXE 0x50fcdf
// SYNTHETIC: EXE 0x50fd0b
// SYNTHETIC: EXE 0x50fd21
// SYNTHETIC: EXE 0x50fd35
// SYNTHETIC: EXE 0x50fd49
// SYNTHETIC: EXE 0x50fd75
// SYNTHETIC: EXE 0x50fda9
// SYNTHETIC: EXE 0x50fdd5
// SYNTHETIC: EXE 0x50fe01
// SYNTHETIC: EXE 0x50fe2d
// SYNTHETIC: EXE 0x50fe68
// SYNTHETIC: EXE 0x50fe93
// SYNTHETIC: EXE 0x50fea7
// SYNTHETIC: EXE 0x50fed2
// SYNTHETIC: EXE 0x50ff88
// SYNTHETIC: EXE 0x50ffb3
// SYNTHETIC: EXE 0x51005e
// SYNTHETIC: EXE 0x5100bd
// SYNTHETIC: EXE 0x51011a
// SYNTHETIC: EXE 0x510196
// SYNTHETIC: EXE 0x5101ad
// SYNTHETIC: EXE 0x510207
// SYNTHETIC: EXE 0x510270
// SYNTHETIC: EXE 0x5102aa
// SYNTHETIC: EXE 0x5102e2
// SYNTHETIC: EXE 0x510303
// SYNTHETIC: EXE 0x510374
// SYNTHETIC: EXE 0x5103e5
// SYNTHETIC: EXE 0x510456
// SYNTHETIC: EXE 0x5106e5
// SYNTHETIC: EXE 0x510702
// SYNTHETIC: EXE 0x5109fe
// SYNTHETIC: EXE 0x510a13
// SYNTHETIC: EXE 0x510cf9
// SYNTHETIC: EXE 0x510faf
// SYNTHETIC: EXE 0x510fe9
// SYNTHETIC: EXE 0x51101b
// SYNTHETIC: EXE 0x51105c
// SYNTHETIC: EXE 0x51108e
// SYNTHETIC: EXE 0x5110d0
// SYNTHETIC: EXE 0x511125
// SYNTHETIC: EXE 0x511338
// SYNTHETIC: EXE 0x51137f
// SYNTHETIC: EXE 0x5113ad
// SYNTHETIC: EXE 0x5113d4
// SYNTHETIC: EXE 0x511405
// SYNTHETIC: EXE 0x51143a
// SYNTHETIC: EXE 0x51146e
// SYNTHETIC: EXE 0x51148e
// SYNTHETIC: EXE 0x5114bf
// SYNTHETIC: EXE 0x5114e7
// SYNTHETIC: EXE 0x511517
// SYNTHETIC: EXE 0x51154f
// SYNTHETIC: EXE 0x511580
// SYNTHETIC: EXE 0x511592
// SYNTHETIC: EXE 0x5115b7
// SYNTHETIC: EXE 0x511709
// SYNTHETIC: EXE 0x511722
// SYNTHETIC: EXE 0x5117da
// SYNTHETIC: EXE 0x5118e9
// SYNTHETIC: EXE 0x511997
// SYNTHETIC: EXE 0x5119ec
// SYNTHETIC: EXE 0x511b00
// SYNTHETIC: EXE 0x511b10
// SYNTHETIC: EXE 0x511b3b
// SYNTHETIC: EXE 0x511b66
// SYNTHETIC: EXE 0x511bd7
// SYNTHETIC: EXE 0x511c48
// SYNTHETIC: EXE 0x511c82
// SYNTHETIC: EXE 0x511cbd
// SYNTHETIC: EXE 0x511cef
// SYNTHETIC: EXE 0x511d21
