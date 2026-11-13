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
#include "rust/ee_gfx.h"
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
#include "rust/ee_ge.h"   // ee_gfx view.rs: GetUnitOpacity's near fade (RUST_PLAN R4.4)

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
// Fn10C036's constants (.rdata 0x441334..0x4413c4: the view band, the clocks' rates, the moon's hours, the fill share):
// ee_gfx scene.rs
static uint32_t sSwayClock;                 // 0x529084 (+2 every 33 ms: FillRenderLists; the objects' sway; provisional name)
// 0x441344 (degrees to radians), 0x441354 (the objects' size step): ee_gfx capture.rs
bool GetTileHasTree(EETile* inTile);
static unsigned long sU529090;              // 0x529090 (the music check's last time; provisional name)
static uint32_t sU529094;                   // 0x529094 (the sway clock's last time; provisional name)
static unsigned long sL52908C;              // 0x52908c (the frame counter; provisional name)
EERenderUnit* gU529044;                     // 0x529044 (the picked enemy's render unit: FillUnit; EEROUnits.cpp reads it; provisional name)
static const float kEERenderObject_F441350 = 0.35f;   // 0x441350 (the see-through opacity)
// 0x44136c (0.5, the near fade's depth): ee_gfx view.rs NEAR_FADE_DEPTH
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

GEVector gU4E9210;                         // 0x4e9210 (the sun direction: Fn10C036, SetTerrainLighting)
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

// the light fade (0x4be368 sLightFade, 0x4be36c its start, 0x529070.. its direction, rate and time) and the
// letterbox (0x4be370 its direction, 0x4be378 its start, 0x52907c.. its rate and time, 0x529088 the bars to clear
// again): ee_gfx scene.rs (RUST_PLAN R4.4), which steps them; the C++ keeps the storage
static EeGfxSceneFade sSceneFade = {1.0f, 1.0f, 0, 0.0f, 0.0f, 1, 1.0f, 0.0f, 0.0f, false};
static float& sLightFade = sSceneFade.light_fade;
float gF4BE374 = 1.0f;                     // 0x4be374 the letterbox's zoom (EEScenarioManager.cpp reads it)
// the space-cliff and cliff-edge texture names (0x4be37c; .data, the literals follow it) (provisional name)
static const char* sCliffTextures[13] = {
    "textures\\spacecliff_1", "textures\\spacecliff_2", "textures\\spacecliff_3", "textures\\spacecliff_4",
    "textures\\spacecliff_5", "textures\\spacecliff_6", "textures\\spacecliff_7", "textures\\spacecliff_8",
    "textures\\cliffedge_1", "textures\\cliffedge_2", "textures\\cliffedge_3", "textures\\cliffedge_4",
    "textures\\cliffedge_5"
};
// EEInitialiseScene's state (provisional names)
static GEBitmap* sU529060;                  // 0x529060
static bool sSceneInitialised;              // 0x529064 (EEInitialiseScene ran)
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
    ee_gfx_scene_reset(&sSceneFade);
    gB529066 = false;
    gCameraFollowing = false;
    gB529068 = false;
    gF4BE374 = 1.0f;
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
    ee_gfx_scene_start_letterbox(&sSceneFade, true, (uint32_t)inFrames, gF4BE374);
}

// FUNCTION: EXE 0x50b36a
// Starts opening the letterbox bars over the given number of frames (at once for 0)
void EEStartLetterboxIn(unsigned long inFrames)
{
    ee_gfx_scene_start_letterbox(&sSceneFade, false, (uint32_t)inFrames, gF4BE374);
}

// FUNCTION: EXE 0x50b3b3
// Returns whether a unit is drawn this frame: seen by the current player (or the free camera) and in the
// view; a unit whose footprint spans several tiles counts once (ioDrawn)
bool EERenderObject::IsUnitDrawn(EEUnit* inUnit, EETile* inTile, std::set<EEUnit*>& ioDrawn)
{
    // the decisions are ee_gfx walk.rs's (RUST_PLAN R4.4); the drawn set and the view tests are answered here
    struct Ask {
        EEUnit* mUnit;
        std::set<EEUnit*>* mDrawn;
        static bool First(void* inAsk) { return static_cast<Ask*>(inAsk)->mDrawn->insert(static_cast<Ask*>(inAsk)->mUnit).second; }
        static bool Complex(void* inAsk)
        {
            return static_cast<Ask*>(inAsk)->mUnit->mComplexUnit->mPhysicalModel.Intersects(gViewport, gOrthographicView);
        }
        static bool Model(void* inAsk, float inX, float inY)
        {
            EEUnit* theUnit = static_cast<Ask*>(inAsk)->mUnit;
            GEPhysicalModel theModel;
            GETransformation theTransformation(false);
            float theZ = gWorld.mMap->mTerrainMesh.GetMeshZ(inX, inY);
            theTransformation.SetOrientationYPR(gRenderEnvironment.mCameraYaw, 0.0f, 0.0f);
            theTransformation.SetPosition(inX, inY, theZ);
            theTransformation.SetElement(3, 0, 0.0f);
            theTransformation.SetElement(3, 1, 0.0f);
            theTransformation.SetElement(3, 2, 0.0f);
            theTransformation.SetElement(3, 3, 1.0f);
            theModel.SetModel(theUnit->GetModel()->mModels[0]);
            theModel.SetFrame(0);
            theModel.SetLocationAndScale(theTransformation, theUnit->mStats->GetScale());
            return theModel.Intersects(gViewport, gOrthographicView);
        }
    } theAsk = {inUnit, &ioDrawn};
    EEComplexUnit* theComplex = inUnit->mComplexUnit;
    EEPlayer* thePlayer = gWorld.GetCurrentPlayer();
    EEUnitClass* theClass = inUnit->mUnitClass;
    EeGfxUnitFacts theFacts = {};
    theFacts.food = inUnit->mResourceType == kResourceFood && inUnit->mResourceAmount > 0;
    theFacts.has_complex = theComplex != NULL;
    theFacts.category = theClass->mCategory;
    theFacts.owner_id = inUnit->mPlayer->GetPlayerID();
    theFacts.seen = inTile->IsSeenBy(thePlayer);
    theFacts.free_camera = gFreeCamera;
    theFacts.scale = inUnit->mStats->GetScale();
    theFacts.is_tree = theClass->mIsTree;
    theFacts.unit_type = theClass->GetUnitType();
    theFacts.unit_count = EERenderLists::sInstance.mRenderUnitCount;
    theFacts.object_count = EERenderLists::sInstance.mRenderShadowCount;
    theFacts.tile_size = theClass->GetTileSize();
    theFacts.mine = inUnit->mPlayer == thePlayer;
    theFacts.marker_shown = inUnit->Fn27C0B4();
    theFacts.path_marker = inUnit->IsPathMarker();
    theFacts.id = inUnit->GetUnitID();
    theFacts.tile_x = inUnit->mTileX;
    theFacts.tile_y = inUnit->mTileY;
    if (theComplex != NULL) {
        theFacts.dead = theComplex->mCommand != NULL && theComplex->mCommand->mType == kActionDead;
        theFacts.complex_gate = theComplex->mUnitClass->mCategory == kUnitCategoryGate;
        theFacts.complex_path_point = theComplex->mUnitClass->GetUnitType() == kUnitTypePathPoint;
        theFacts.complex_flare = theComplex->mUnitClass->mIsFlare;
        theFacts.foundation = theComplex->IsFoundation();
    }
    EeGfxDrawnHost theHost = {&theAsk, Ask::First, Ask::Complex, Ask::Model};
    return ee_gfx_capture_unit_drawn(&theFacts, &theHost);
}

// FUNCTION: EXE 0x50b7e1
// Unloads the media of every pooled map object and empties the pool
void UnloadRenderShadows(EERenderShadow* inObjects, unsigned long& ioCount)
{
    for (unsigned long i = 0; i < ioCount; i++)
        inObjects[i].mObject->UnloadMedia();
    ioCount = 0;
}

// FUNCTION: EXE 0x50b80b
// Unloads the media of every pooled render unit, resets its bars and turning parts, clears the pointed-at
// unit and object, and empties the pool
void UnloadRenderUnits(EERenderUnit* inUnits, unsigned long& ioCount)
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
void UnloadGhostRecords()
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
    if (inMode == 1 || inMode != 2)
        return;
    gU529050.push_back(theLine);
    EERenderLine& theBack = gU529050[gU529050.size() - 1];
    theBack.mWidth = gU4E9330;
    theBack.mTexture = gAreaEffectTexture;
    gAreaEffectTexture->Reference();
    // the 50 points on the ground and the first again, the strip's width: ee_gfx capture.rs
    EeGe3DPoint thePoints[0x33];
    EeGfxParticleGround theGround = EEParticleGround();
    theVertex.second = ee_gfx_capture_ground_circle(&theGround, inLocation.GetX(), inLocation.GetY(), inRadius, thePoints);
    theBack.mPoints.resize(0x33, std::pair<GE3DPoint, float>());
    for (unsigned long i = 0; i < 0x33; i++) {
        theVertex.first.mX = thePoints[i].x;
        theVertex.first.mY = thePoints[i].y;
        theVertex.first.mZ = thePoints[i].z;
        theBack.mPoints[i] = theVertex;
    }
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
    GEVector theForward(0.0f, 1.0f, 0.0f);
    GEVector theDirection;
    float theTimeOfDay = gWorld.mTimeOfDay;
    static unsigned long sLastTime = TMGetTime();
    if (gB529066)
        return;
    unsigned long theNow = TMGetTime();
    float theElapsed = (float)(theNow - sLastTime);
    sLastTime = theNow;
    // the clocks, the fade and the letterbox: ee_gfx scene.rs (the progress in double, as the x87 build's: see there)
    gRenderEnvironment.mWindSpeed = gWorld.mEnvironmentManager->mWind;
    EeGfxSceneClocks theClocks = {gU4E9330, gRenderEnvironment.mWaterPhase, gRenderEnvironment.mCloudOffset};
    ee_gfx_scene_step_clocks(&theClocks, theElapsed, gRenderEnvironment.mWindSpeed);
    gU4E9330 = theClocks.grid_phase;
    gRenderEnvironment.mWaterPhase = theClocks.water_phase;
    gRenderEnvironment.mCloudOffset = theClocks.cloud_offset;
    EeGfxSceneStep theStep = ee_gfx_scene_step(&sSceneFade, theElapsed, gF4BE374);
    EeGfxLetterbox theBox = ee_gfx_scene_letterbox(theStep.zoom, (float)gGameWindowHeight);
    theRectangle.mLeft = 0.0f;
    theRectangle.mRight = (float)gGameWindowWidth;
    if (theStep.clear_bars) {
        theRectangle.mTop = 0.0f;
        theRectangle.mBottom = theBox.top_bar_end;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        theRectangle.mTop = theBox.bottom_bar_start;
        theRectangle.mBottom = (float)gGameWindowHeight;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
    }
    // the zoom changed: the view band, the bars cleared (one row more at the top), the UI's backgrounds at full view
    if (theStep.set_view) {
        GERectangle theView;
        theView.mLeft = theBox.left;
        theView.mRight = theBox.right;
        theView.mTop = theBox.top;
        theView.mBottom = theBox.bottom;
        EESetGameViewRectangle(theView);
        theRectangle.mTop = 0.0f;
        theRectangle.mBottom = theBox.top_bar_end + 1.0f;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        theRectangle.mTop = theBox.bottom_bar_start;
        theRectangle.mBottom = (float)gGameWindowHeight;
        gRasterizer->Clear(theBlack, theRectangle, gScreenIndex);
        if (theStep.show_backgrounds) {
            gUIManager->GetMainGameForm()->ShowAllBackgrounds();
            gUIManager->RedrawAll();
        }
        if (theStep.hide_backgrounds) {
            gUIManager->GetMainGameForm()->HideAllBackgrounds();
            gUIManager->RedrawAll();
        }
    }
    gF4BE374 = theStep.zoom;
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
        // the blend of the 17 key frames, the lit direction and the prelit colour: ee_gfx scene.rs
        EeGfxDaylightTables theTables;
        for (unsigned long i = 0; i < 17; i++) {
            theTables.colors[i] = {gU4E9348[i].mRed, gU4E9348[i].mGreen, gU4E9348[i].mBlue};
            theTables.light[i] = gU4E92EC[i];
            theTables.ambient[i] = gU4E92A8[i];
            theTables.unit_ambient[i] = gU4E9264[i];
        }
        theTables.ambient_color = {gU4E9338.mRed, gU4E9338.mGreen, gU4E9338.mBlue};
        for (unsigned long i = 0; i < 3; i++)
            theTables.fill[i] = {gU4E9240[i].mX, gU4E9240[i].mY, gU4E9240[i].mZ};
        theTables.sun = {gWorld.mSunDirection.mX, gWorld.mSunDirection.mY, gWorld.mSunDirection.mZ};
        theTables.moon = {gWorld.mMoonDirection.mX, gWorld.mMoonDirection.mY, gWorld.mMoonDirection.mZ};
        EeGfxDaylight theLight = ee_gfx_scene_daylight(&theTables, theTimeOfDay);
        gU4E9210.mX = theLight.direction.x;
        gU4E9210.mY = theLight.direction.y;
        gU4E9210.mZ = theLight.direction.z;
        GEColor theBlend(theLight.light.red, theLight.light.green, theLight.light.blue);
        GEColor thePrelit(theLight.prelit.red, theLight.prelit.green, theLight.prelit.blue);
        gRenderEnvironment.mTerrainLightColor = theBlend;
        gRenderEnvironment.mUnitLightColor = theBlend;
        gRenderEnvironment.mTerrainAmbientColor = gU4E9338;
        gRenderEnvironment.mUnitAmbientColor = gU4E9338;
        gRenderEnvironment.mTerrainLightIntensity = theLight.light_intensity;
        gRenderEnvironment.mUnitLightIntensity = theLight.light_intensity;
        gRenderEnvironment.mTerrainAmbientIntensity = theLight.ambient_intensity;
        gRenderEnvironment.mUnitAmbientIntensity = theLight.unit_ambient_intensity;
        gRenderEnvironment.mPrelitColor = thePrelit;
        gRenderEnvironment.mDimPrelitColor =
            GEColor(theLight.dim_prelit.red, theLight.dim_prelit.green, theLight.dim_prelit.blue);
        gRenderEnvironment.mColor54 = thePrelit;
    } else {
        gRenderEnvironment.mTimeOfDayChanged = false;
        if (!theStep.fade_changed)
            goto done;
    }
    if (gCameraFollowing) {
        SetTerrainLighting();
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
    // the place in its tiles, the size and the sway by id: ee_gfx capture.rs
    EeGe2DPoint thePlace = ee_gfx_capture_object_place((int32_t)theID, (int32_t)theSize, (int32_t)inUnit->mTileX,
                                                       (int32_t)inUnit->mTileY);
    outObject->mLocation.SetX(thePlace.x);
    outObject->mLocation.SetY(thePlace.y);
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
    outObject->mModelScale *= ee_gfx_capture_object_scale((int32_t)inUnit->GetUnitID());
    long theX = inUnit->mTileX;
    long theY = inUnit->mTileY;
    if (!GetTileHasTree(theMap->GetTile(theX - 1, theY - 1)) || !GetTileHasTree(theMap->GetTile(theX + 1, theY + 1))) {
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
        outObject->mFraction = ee_gfx_capture_sway_fraction(sSwayClock, (int32_t)inUnit->GetUnitID());
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
bool GetTileHasTree(EETile* inTile)
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
    EeGe2DPoint thePlace = ee_gfx_capture_object_place((int32_t)theID, (int32_t)theSize, (int32_t)inX, (int32_t)inY);
    outObject->mLocation.mX = thePlace.x;
    outObject->mLocation.mY = thePlace.y;
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
    outObject->mModelScale = ee_gfx_capture_object_scale((int32_t)inDoppelganger->mUnit->GetUnitID()) * theScale;
    outObject->mObject = inDoppelganger->GetModel();
    outObject->mObject->LoadMedia();
    if (!GetTileHasTree(theMap->GetTile(inX - 1, inY - 1)) || !GetTileHasTree(theMap->GetTile(inX + 1, inY + 1))) {
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
        outObject->mFraction = ee_gfx_capture_sway_fraction(sSwayClock, (int32_t)inDoppelganger->mUnit->GetUnitID());
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
    if (inDoppelganger->mUnit->mUnitClass->mFacesCamera == true)
        theTransformation.SetOrientationYPR(ioInfo.mYaw, 0.0f, 0.0f);
    if (inDoppelganger->mUnit->mUnitClass->mFacesCamera == true)
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

// the space-cliff textures (0x4e8ef0; loaded once by LoadCliffTextures, handed to the terrain mesh) (provisional name)
std::vector<GETexture*> gU4E8EF0;
// SYNTHETIC: EXE 0x50e899   (wrapper)
// SYNTHETIC: EXE 0x50e8a3   (ctor)
// SYNTHETIC: EXE 0x50e8bb   (registrar)
// SYNTHETIC: EXE 0x50e8c7   (dtor)

// FillUnit reads the goal's kind
#include "ee/EEUnitGoal.h"
#include "rust/ee_gfx.h"

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
    bool theFogged = ee_gfx_capture_owned_food(inUnit->mResourceType == kResourceFood && inUnit->mResourceAmount > 0,
                                               theComplex != NULL, inUnit->mUnitClass->mCategory,
                                               inUnit->GetPlayer()->GetPlayerID());
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
            EEUCGate* theGate = dynamic_cast<EEUCGate*>(theComplex->mUnitClass.Get());
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
                // the class's ring size (the original's never-set 0x52906c could pick the line of sight or the range)
                outUnit->mSelectionSize = inUnit->mUnitClass->mSelectionSize;
                if (theComplex->GetNumCargoUnits() > 0
                    && (theComplex->GetPlayer() == theCurrent || theComplex->GetPlayer()->mEnemies[theCurrent->GetPlayerID()] == 0)) {
                    outUnit->mHealthBars.resize(theComplex->GetNumCargoUnits() + 1, 0.0f);
                    for (unsigned long j = 0; j < theComplex->GetNumCargoUnits(); j++)
                        outUnit->mHealthBars[j + 1] = (float)theComplex->mCargo[j]->mHitPoints
                                               / theComplex->mCargo[j]->GetStats()->mMaxHitPoints;
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
                    outUnit->mHealthBars[j + 1] = (float)theComplex->mCargo[j]->GetHitPoints()
                                           / theComplex->mCargo[j]->GetStats()->GetMaxHitPoints();
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
        if (theComplex->mUnitClass->mFacesCamera) {
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
                EeGfxColor theTint = ee_gfx_capture_waypoint_tint((int32_t)theCarrier->GetUnitID());
                outUnit->mTintColor = GEColor(theTint.red, theTint.green, theTint.blue);
            }
        }
        inUnit->mUnitClass->GroundLocation(thePoint);
        if (inUnit->IsPathMarker() && theTile->GetHeight() < gRenderEnvironment.mSeaLevel)
            thePoint.SetZ(gRenderEnvironment.mSeaLevel);
        outUnit->mTransformation.SetOrientationYPR(inUnit->mUnitClass->mFacesCamera ? ioInfo.mYaw : 0.0f, 0.0f, 0.0f);
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
static void LoadCliffTextures()
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
                LoadCliffTextures();
            theMap->mTerrainMesh.SetSpaceCliffTextures(gU4E8EF0);
        }
        theMap->mTerrainMesh.UpdateVisibleTerrain(gWorld.mTransformation, theViewport, 2, 1, 3, gOrthographicView);
        SetTerrainLighting();
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
// (LoadCliffTextures, the static function above)

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
        ee_gfx_capture_sway_tick(&sSwayClock, &sU529094, (uint32_t)theTime);
        this->Fn10E8D1();
        UnloadRenderShadows(EERenderLists::sInstance.mRenderShadows, EERenderLists::sInstance.mRenderShadowCount);
        UnloadRenderUnits(EERenderLists::sInstance.mRenderUnits, EERenderLists::sInstance.mRenderUnitCount);
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
        UnloadGhostRecords();
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
        // port (R5a.3): no reference: the unit is only compared and read while the world's lock is held
        theInfo.mPickedUnit = !gFreeCamera ? gUnitPicker.PickUnitUnreferenced(gMouse->mPosition, 1) : NULL;
        if (theInfo.mPickedUnit != NULL && theInfo.mPickedUnit->GetHitPoints() <= 0)
            theInfo.mPickedUnit = NULL;
        gUnitPicker.GetLock()->Enter();
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
        gUnitPicker.GetLock()->Leave();
        this->FillGhostRecords();
        // port (R5a.3): what EEROUnits::DrawUnitModel read from the live unit after the lock was released, read here
        // with the same lookup (a pure one: the player's id table); the doppelgangers' render units keep the id their
        // pool slot had, as before
        for (unsigned long i = 0; i < EERenderLists::sInstance.mRenderUnitCount; i++) {
            EERenderUnit& theRenderUnit = EERenderLists::sInstance.mRenderUnits[i];
            EEUnit* theLive;
            theRenderUnit.mSecondFullOpacity = theRenderUnit.mUnitClassIndex == kUnitTypeRoboticFarm
                && ((theLive = static_cast<EEUnit*>(gWorld.GetUnit(theRenderUnit.mUnitId))) == NULL
                    || theLive->mHitPoints == 1);
        }
        gB529068 = true;
        if (gMusicFollowsBattle && theTime - sU529090 >= 0xfa) {
            sU529090 = theTime;
            gMusicManager->SetIntensity(ee_gfx_capture_music_intensity((uint32_t)gMusicManager->mIntensity,
                                                                       theInfo.mNumFightingUnits, theCount));
        }
        if (theInfo.mAmbient)
            gAmbientSoundManager.Update();
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
    UnloadRenderShadows(EERenderLists::sInstance.mRenderShadows, EERenderLists::sInstance.mRenderShadowCount);
    UnloadRenderUnits(EERenderLists::sInstance.mRenderUnits, EERenderLists::sInstance.mRenderUnitCount);
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
    UnloadGhostRecords();
    sU529060->Dereference();
    gUnitPicker.Reset();
    sSceneInitialised = false;
}

// FUNCTION: EXE 0x50f37d
// Starts fading the lights up to full over the given number of frames (at once for 0)
void EEStartLightFadeIn(unsigned long inFrames)
{
    ee_gfx_scene_start_fade(&sSceneFade, false, (uint32_t)inFrames);
}

// FUNCTION: EXE 0x50f3c6
// Starts fading the lights down to black over the given number of frames (at once for 0)
void EEStartLightFadeOut(unsigned long inFrames)
{
    ee_gfx_scene_start_fade(&sSceneFade, true, (uint32_t)inFrames);
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
        float theScale;
        unsigned long theFrame;
        GEModel* theModel;
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
        // the near fade: ee_gfx view.rs (RUST_PLAN R4.4)
        ee_gfx_unit_near_fade(reinterpret_cast<const EeGeViewport*>(&gViewport), reinterpret_cast<const EeGe3DPoint*>(&theCenter),
                              theModel->GetBoundingRadius(theFrame), theScale, &theResult, &outOpacity);
    }
    return theResult;
}

// The lighting set-ups below: their rasterizer calls are ee_gfx lighting.rs's (RUST_PLAN R4.4), applied in order
static void ApplyLighting(EeGfxLighting inKind)
{
    EeGfxLightingInputs theIn;
    EeGfxColor* theColors[] = {&theIn.fog_color, &theIn.prelit, &theIn.dim_prelit, &theIn.cloud, &theIn.terrain_ambient,
                                     &theIn.terrain_light, &theIn.unit_ambient, &theIn.unit_light};
    const GEColor* theSources[] = {&gU4E9220, &gRenderEnvironment.mPrelitColor, &gRenderEnvironment.mDimPrelitColor,
                                   &gRenderEnvironment.mColor54, &gRenderEnvironment.mTerrainAmbientColor,
                                   &gRenderEnvironment.mTerrainLightColor, &gRenderEnvironment.mUnitAmbientColor,
                                   &gRenderEnvironment.mUnitLightColor};
    for (int i = 0; i < 8; i++)
        *theColors[i] = EeGfxColor{theSources[i]->mRed, theSources[i]->mGreen, theSources[i]->mBlue};
    theIn.light_fade = sLightFade;
    theIn.orthographic = gOrthographicView;
    theIn.unit_lighting = gUnitLighting;
    theIn.rear_clipping = gViewport.GetRearClippingDistance();
    theIn.zoom_lock = gZoomLock;
    theIn.terrain_ambient_intensity = gRenderEnvironment.mTerrainAmbientIntensity;
    theIn.terrain_light_intensity = gRenderEnvironment.mTerrainLightIntensity;
    theIn.unit_ambient_intensity = gRenderEnvironment.mUnitAmbientIntensity;
    theIn.unit_light_intensity = gRenderEnvironment.mUnitLightIntensity;
    theIn.sun = EeGeVector{gU4E9210.mX, gU4E9210.mY, gU4E9210.mZ};
    EeGfxRasterOps theOps = ee_gfx_lighting(inKind, &theIn);
    unsigned long theScreen = gScreenIndex;
    for (uint32_t i = 0; i < theOps.count; i++) {
        const EeGfxRasterOp& theOp = theOps.ops[i];
        GEColor theColor(theOp.f[0], theOp.f[1], theOp.f[2]);
        switch (theOp.kind) {
        case EeGfxRasterOpKind::Fog: gRasterizer->SetFog(theOp.value, theScreen); break;
        case EeGfxRasterOpKind::FogColor: gRasterizer->SetFogColor(theColor, theScreen); break;
        case EeGfxRasterOpKind::FogRange: gRasterizer->SetFogRange(theOp.f[0], theOp.f[1], theScreen); break;
        case EeGfxRasterOpKind::FogType: gRasterizer->SetFogType(theOp.value, theScreen); break;
        case EeGfxRasterOpKind::AmbientColor: gRasterizer->SetAmbientColor(theColor, theScreen); break;
        case EeGfxRasterOpKind::AmbientIntensity: gRasterizer->SetAmbientIntensity(theOp.f[0], theScreen); break;
        case EeGfxRasterOpKind::RemoveLights: gRasterizer->RemoveAllLights(theScreen); break;
        case EeGfxRasterOpKind::Lighting: gRasterizer->SetLighting(theOp.value, theScreen); break;
        case EeGfxRasterOpKind::LightColor: gU4E9198.SetColor(theColor); break;
        case EeGfxRasterOpKind::LightIntensity: gU4E9198.SetIntensity(theOp.f[0]); break;
        case EeGfxRasterOpKind::LightDirection: gU4E9198.SetDirection(GEVector(theOp.f[0], theOp.f[1], theOp.f[2])); break;
        case EeGfxRasterOpKind::AddLight: gRasterizer->AddLight(gU4E9198, theScreen, gU4E9190); break;
        case EeGfxRasterOpKind::UpdateLight: gRasterizer->UpdateLight(gU4E9190, gU4E9198, theScreen); break;
        }
    }
}

// FUNCTION: EXE 0x50f60f
// Turns on linear fog in the fog colour dimmed by the light fade, ending at the rear clipping distance
// (off in the orthographic view)
void SetSceneFog() { ApplyLighting(EeGfxLighting::SceneFog); }

// FUNCTION: EXE 0x50f6c7
// Turns the fog off
void SetFogOff() { ApplyLighting(EeGfxLighting::FogOff); }

// FUNCTION: EXE 0x50f6de
// Turns lighting off (no ambient light, no lights)
void SetLightingOff() { ApplyLighting(EeGfxLighting::Off); }

// FUNCTION: EXE 0x50f71c
// Lights with the environment's prelit colour as ambient, dimmed by the light fade, and no lights
void SetPrelitLighting() { ApplyLighting(EeGfxLighting::Prelit); }

// FUNCTION: EXE 0x50f772
// Lights with the environment's second ambient colour (the clouds'), dimmed by the light fade, and no lights
void SetCloudLighting() { ApplyLighting(EeGfxLighting::Cloud); }

// FUNCTION: EXE 0x50f7c8
// Lights with white ambient light dimmed by the light fade, and no lights
void SetWhiteLighting() { ApplyLighting(EeGfxLighting::White); }

// FUNCTION: EXE 0x50f833
// Sets the terrain lighting: the environment's terrain ambient light and the sun light, dimmed by the fade
void SetTerrainLighting() { ApplyLighting(EeGfxLighting::Terrain); }

// FUNCTION: EXE 0x50f8de
// Sets the map objects' lighting: unit ambient and a flattened sun light when unit lighting is on, else the
// prelit colour at a fixed intensity
void SetMapObjectLighting() { ApplyLighting(EeGfxLighting::MapObject); }

// FUNCTION: EXE 0x50fa01
// Sets the units' lighting: unit ambient and the sun light when unit lighting is on, else the prelit colour
void SetUnitLighting() { ApplyLighting(EeGfxLighting::Unit); }

// FUNCTION: EXE 0x50fae8
// Restores the units' lighting at full strength for a unit in view (not fogged)
void RestoreUnitLighting() { ApplyLighting(EeGfxLighting::RestoreUnit); }

// FUNCTION: EXE 0x50fb74
// Halves the units' lighting for a fogged unit (the dim prelit colour without unit lighting)
void DimUnitLighting() { ApplyLighting(EeGfxLighting::DimUnit); }

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
