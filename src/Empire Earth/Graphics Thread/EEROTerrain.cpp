// C:\EEX\Empire Earth\Graphics Thread\EEROTerrain.cpp (__FILE__ 0x4be5f4). The terrain's render object (vtable
// 0x441b7c): 0x11251f..0x114296 (four init chains, the ctor, ??_G, the dtor, Virtual00, the star/cloud/sky models,
// Virtual08, the sky index step, the release, the cloud layer, the sky colours, the coloured rectangles' erase
// COMDATs and RTSWorld's inline map accessor); EEROUnits.cpp's init chains start at 0x114296. The .rdata tables
// 0x41142c..0x441b98 are this unit's. Unit flags: the exe's module set.
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
#include "lle/GERhombus.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/TSCriticalSection.h"
#include "lle/U2DRectangle.h"
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
#include "ee/EEUIManager.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEUIFSEBrushGroup.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEROTerrain.h"

extern unsigned long& gTexturePixelDepth;    // 0x5308b4
extern unsigned long& gTextureMipLevelsToDrop; // 0x530894 (provisional name)
extern bool& gDrawClouds;                    // 0x5308d3 (the clouds are drawn; provisional name)
extern bool& gTrilinearFilter;               // 0x5308da (the sky textures use mip filtering; provisional name)

// one vertex of the cloud/star models: a corner of the map square (index into the corner table) and a height
// (.rdata 0x441a18.. / 0x441ab8..) (provisional name)
struct EEROTerrainVertex {
    long mCorner;
    float mZ;
};

// the cloud layer's ten triangles (0x44142c)
static const unsigned long sCloudIndices[30] = {
    0, 1, 4, 4, 1, 5, 2, 0, 6, 6, 0, 4, 3, 2, 7, 7, 2, 6, 1, 3, 5, 5, 3, 7, 4, 5, 6, 6, 5, 7,
};
const float kCloudHeight = 1500.0f;         // 0x441408
const float kCloudOverhang = 3000.0f;       // 0x44140c
const float kCloudZ = 200.0f;               // 0x441410
const float kCloudRepeat = 4000.0f;         // 0x441414
const float kHighlightOpacity = 0.5f;       // 0x441418
const float kEEROTerrainStarU = 6.0f;                  // 0x441428: original word 0000c040, verified

const float kSkyRearClipping = 20000.0f;    // 0x441b94
// 0x441b98: the sky floor -10000.0f is a literal (fld; fst ... fstp keeps it on the x87 stack across both corners)

// the brush highlight colour (0x5293e8) (provisional name)
GEColor gBrushHighlightColor;
// SYNTHETIC: EXE 0x51251f   (wrapper)
// SYNTHETIC: EXE 0x512529   (ctor)
// SYNTHETIC: EXE 0x512542   (registrar)
// SYNTHETIC: EXE 0x51254e   (dtor)

// 0x5293e0 (two floats) (provisional name; type inferred)
GETextureCoordinate gU5293E0;
// SYNTHETIC: EXE 0x51254f   (wrapper)
// SYNTHETIC: EXE 0x512559   (ctor)
// SYNTHETIC: EXE 0x51256a   (registrar)
// SYNTHETIC: EXE 0x512576   (dtor)

// the sky colours by time of day (0x5290b0) (provisional name)
GEColor gSkyColors[68];
// SYNTHETIC: EXE 0x512577   (wrapper)
// SYNTHETIC: EXE 0x512581   (ctor)
// SYNTHETIC: EXE 0x51259c   (registrar)
// SYNTHETIC: EXE 0x5125a8   (dtor)

// the map's tile runs as of the last frame, drawn as grid lines (0x5290a0) (provisional name)
std::vector<RTSMapTileRun> gGridRuns;
// SYNTHETIC: EXE 0x5125c3   (wrapper)
// SYNTHETIC: EXE 0x5125cd   (ctor)
// SYNTHETIC: EXE 0x5125e5   (registrar)
// SYNTHETIC: EXE 0x5125f1   (dtor)

GETexture* gAreaEffectTexture;
GETexture* gCircleAreaEffectTexture;
static GEModel* sSkyModel;                  // 0x5293fc
static GEModel* sCloudModel;                // 0x529400
static GETexture* sCloudTexture;            // 0x529404
static GEModel* sOuterStarModel;            // 0x529408
static GEModel* sInnerStarModel;            // 0x52940c
static GETexture* sStarTexture;             // 0x529410

// FUNCTION: EXE 0x5125fb
// Creates the terrain's render object with the terrain grid off
EEROTerrain::EEROTerrain()
{
    this->mGridMode = 0;
}

// SYNTHETIC: EXE 0x512611
// (the scalar deleting destructor, emitted with the vtable; slot EEROTerrain+0x10)

// FUNCTION: EXE 0x51262d
// Destroys the terrain's render object
EEROTerrain::~EEROTerrain()
{
}

static void Fn113EBD();
static void Fn11396F();

// FUNCTION: EXE 0x512638
// Prepares the frame: updates the sky colours and models, the clouds (black fog colour on space maps instead),
// the fog of war (all seen for see-all or the free camera) and copies the map's grid runs
void EEROTerrain::FrameStarting()
{
    this->Fn10C036();
    Fn113EBD();
    if (gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
        gU4E9220.SetRed(0.0f);
        gU4E9220.SetGreen(0.0f);
        gU4E9220.SetBlue(0.0f);
    } else if (gDrawClouds) {
        Fn11396F();
    }
    this->Fn10E8D1();
    EEMap* theMap = gWorld.mMap;
    theMap->mTerrainMesh.UpdateFogOfWar(gWorld.GetCurrentPlayer()->mSeesAll || gFreeCamera);

    gGridRuns = gWorld.mMap->mU1955DC;
}

// FUNCTION: EXE 0x5126ce
// Creates the outer and inner star models (space maps' backdrop) and loads their texture (provisional name)
static void Fn1126CE()
{
    GETextureDescription theDescription;
    unsigned long i;
    unsigned long j;
    sOuterStarModel = gModelManager->CreateModel(0xc, 0x18, 1, 0, 1, 0, "outer star model", 1);
    sInnerStarModel = gModelManager->CreateModel(0xc, 0x18, 1, 0, 1, 0, "inner star model", 1);
    sOuterStarModel->Reference();
    sInnerStarModel->Reference();
    sOuterStarModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sOuterStarModel->GetMaterial(0)->SetPolygonRangeLength(0xc, 0);
    sOuterStarModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sOuterStarModel->GetMaterial(0)->SetVertexRangeLength(0x18);
    sInnerStarModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sInnerStarModel->GetMaterial(0)->SetPolygonRangeLength(0xc, 0);
    sInnerStarModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sInnerStarModel->GetMaterial(0)->SetVertexRangeLength(0x18);
    for (i = 0, j = 2; j < 0x1a; j += 4, i += 2) {
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex1(i, j - 2);
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex2(i, j - 1);
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex3(i, j);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex1(i, j - 2);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex2(i, j - 1);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex3(i, j);
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex1(i + 1, j - 2);
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex2(i + 1, j);
        sOuterStarModel->GetPolygonArray(0)->SetVertexIndex3(i + 1, j + 1);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex1(i + 1, j - 2);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex2(i + 1, j);
        sInnerStarModel->GetPolygonArray(0)->SetVertexIndex3(i + 1, j + 1);
    }
    sOuterStarModel->ComputeModelNormals();
    sOuterStarModel->ComputeBoundingVolumes();
    GETextureCoordinate theCoordinate0(0.0f, 0.0f);
    GETextureCoordinate theCoordinate1(kEEROTerrainStarU, 0.0f);
    GETextureCoordinate theCoordinate3(0.0f, kEEROTerrainStarU);
    GETextureCoordinate theCoordinate2(kEEROTerrainStarU, kEEROTerrainStarU);
    for (j = 2; j < 0x1a; j += 4) {
        GEVertexArray* theVertices = sOuterStarModel->GetVertexArray(0);
        theVertices->SetTextureCoordinate(theCoordinate0, j - 2);
        theVertices->SetTextureCoordinate(theCoordinate1, j - 1);
        theVertices->SetTextureCoordinate(theCoordinate2, j);
        theVertices->SetTextureCoordinate(theCoordinate3, j + 1);
        theVertices = sInnerStarModel->GetVertexArray(0);
        theVertices->SetTextureCoordinate(theCoordinate2, j - 2);
        theVertices->SetTextureCoordinate(theCoordinate3, j - 1);
        theVertices->SetTextureCoordinate(theCoordinate0, j);
        theVertices->SetTextureCoordinate(theCoordinate1, j + 1);
    }
    for (j = 0; j < 0x18; j++) {
        sOuterStarModel->GetVertexArray(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f), j);
        sOuterStarModel->GetVertexArray(0)->SetOpacity(1.0f, j);
        sInnerStarModel->GetVertexArray(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f), j);
        sInnerStarModel->GetVertexArray(0)->SetOpacity(1.0f, j);
    }
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mNumMipLevelsToDrop = gTextureMipLevelsToDrop;
    theDescription.mPixelDepth = 0x20;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    theDescription.mPixelDepth = gRasterizer->GetScreen(gScreenIndex)->Get32BitTextureSupport() ? 0x20 : 0x10;
    sStarTexture = gTextureManager->LoadTexture("textures\\stars", theDescription);
    ESVERIFY_AT(sStarTexture != NULL, 246);
    sStarTexture->Reference();
    sStarTexture->SetStageAlphaMode(kStageOpModulate, -1);
    sStarTexture->SetStageColorMode(kStageOpModulate, -1);
    sStarTexture->SetStageMinFilterMode(1, -1);
    sStarTexture->SetStageMagFilterMode(1, -1);
    sStarTexture->SetStageMipMapBias(0.0f, -1);
    sStarTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    sStarTexture->SetStageMipFilterMode(gTrilinearFilter ? 2 : 0, -1);
    sOuterStarModel->SetTexture(sStarTexture, gScreenIndex);
    sInnerStarModel->SetTexture(sStarTexture, gScreenIndex);
}

// FUNCTION: EXE 0x512bc4
// Creates the cloud layer model (opaque inner square, transparent outer edge) and loads the cloud texture
// (provisional name)
static void Fn112BC4()
{
    GETextureDescription theDescription;
    unsigned long i;
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    sCloudModel = gModelManager->CreateModel(10, 8, 1, 0, 1, 0, "cloud layer model", 1);
    sCloudModel->Reference();
    sCloudModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sCloudModel->GetMaterial(0)->SetPolygonRangeLength(10, 0);
    sCloudModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sCloudModel->GetMaterial(0)->SetVertexRangeLength(8);
    unsigned long thePolygon;
    for (thePolygon = 0; thePolygon < 10; thePolygon++) {
        sCloudModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon, sCloudIndices[thePolygon * 3]);
        sCloudModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon, sCloudIndices[thePolygon * 3 + 1]);
        sCloudModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon, sCloudIndices[thePolygon * 3 + 2]);
    }
    for (i = 0; i < 8; i++)
        sCloudModel->GetVertexArray(0)->SetColor(GEColor(1.0f, 1.0f, 1.0f), i);
    for (i = 0; i < 4; i++)
        sCloudModel->GetVertexArray(0)->SetOpacity(0.0f, i);
    for (i = 4; i < 8; i++)
        sCloudModel->GetVertexArray(0)->SetOpacity(1.0f, i);
    theDescription.mNumMipLevelsToDrop = gTextureMipLevelsToDrop;
    theDescription.mPixelDepth = 0x20;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mPixelDepth = gRasterizer->GetScreen(gScreenIndex)->Get32BitTextureSupport() ? 0x20 : 0x10;
    sCloudTexture = gTextureManager->LoadTexture("textures\\fx_cloudtexture", theDescription);
    ESVERIFY_AT(sCloudTexture != NULL, 328);
    sCloudTexture->Reference();
    sCloudTexture->SetStageAlphaMode(kStageOpModulate, -1);
    sCloudTexture->SetStageColorMode(kStageOpModulate, -1);
    sCloudTexture->SetStageMinFilterMode(1, -1);
    sCloudTexture->SetStageMagFilterMode(1, -1);
    sCloudTexture->SetStageMipMapBias(0.0f, -1);
    sCloudTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    if (gTrilinearFilter)
        sCloudTexture->SetStageMipFilterMode(kMipFilterLinear, -1);
    else
        sCloudTexture->SetStageMipFilterMode(kMipFilterNone, -1);

    sCloudModel->SetTexture(sCloudTexture, gScreenIndex);
}

// the sky dome's 36 triangles (0x441524)
static const unsigned long sSkyIndices[108] = {
    0, 1, 5, 0, 5, 4, 4, 5, 9, 4, 9, 8, 8, 9, 13, 8, 13, 12, 12, 13, 17, 12, 17, 16, 1, 2, 6, 1, 6, 5, 5, 6, 10, 5,
    10, 9, 9, 10, 14, 9, 14, 13, 13, 14, 18, 13, 18, 17, 2, 3, 7, 2, 7, 6, 6, 7, 11, 6, 11, 10, 10, 11, 15, 10, 15,
    14, 14, 15, 19, 14, 19, 18, 3, 0, 4, 3, 4, 7, 7, 4, 8, 7, 8, 11, 11, 8, 12, 11, 12, 15, 15, 12, 16, 15, 16, 19,
    16, 17, 18, 16, 18, 19, 0, 3, 2, 0, 2, 1,
};

// the sky colours: 17 times of day x 4 bands (0x4416d4; copied into gSkyColors)
static const float sSkyColorTable[68][3] = {
    {0.773f, 0.255f, 0.051f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f},
    {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f},
    {0.773f, 0.255f, 0.051f}, {0.047f, 0.22f, 0.373f}, {0.023f, 0.11f, 0.185f}, {0.023f, 0.11f, 0.185f},
    {0.023f, 0.11f, 0.185f}, {0.023f, 0.11f, 0.185f}, {0.023f, 0.11f, 0.185f}, {0.047f, 0.22f, 0.373f},
    {0.773f, 0.255f, 0.051f}, {0.941f, 0.69f, 0.259f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f},
    {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f}, {0.698f, 0.894f, 0.984f},
    {0.698f, 0.894f, 0.984f}, {0.941f, 0.69f, 0.259f}, {0.023f, 0.11f, 0.185f}, {0.008f, 0.031f, 0.125f},
    {0.008f, 0.031f, 0.125f}, {0.008f, 0.031f, 0.125f}, {0.008f, 0.031f, 0.125f}, {0.008f, 0.031f, 0.125f},
    {0.023f, 0.11f, 0.185f}, {0.941f, 0.69f, 0.259f}, {0.302f, 0.267f, 0.349f}, {0.2f, 0.29f, 0.659f},
    {0.2f, 0.29f, 0.659f}, {0.2f, 0.29f, 0.659f}, {0.2f, 0.29f, 0.659f}, {0.2f, 0.29f, 0.659f}, {0.2f, 0.29f, 0.659f},
    {0.2f, 0.29f, 0.659f}, {0.302f, 0.267f, 0.349f}, {0.008f, 0.031f, 0.125f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.008f, 0.031f, 0.125f}, {0.302f, 0.267f, 0.349f},
    {0.008f, 0.031f, 0.125f}, {0.1f, 0.15f, 0.325f}, {0.1f, 0.15f, 0.325f}, {0.1f, 0.15f, 0.325f},
    {0.1f, 0.15f, 0.325f}, {0.1f, 0.15f, 0.325f}, {0.1f, 0.15f, 0.325f}, {0.1f, 0.15f, 0.325f},
    {0.008f, 0.031f, 0.125f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.008f, 0.031f, 0.125f},
};
// FUNCTION: EXE 0x512e7f
// Creates the sky dome model and its colour table, the star models, the clouds (when clouds are on) and
// loads the area-effect textures (square and circle)
void EEROTerrain::CreateSkyModel()
{
    GETextureDescription theDescription;
    long i;
    long j;
    theDescription.mPixelDepth = 0x10;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    sSkyModel = gModelManager->CreateModel(0x24, 0x14, 1, 0, 1, 0, "sky model", 1);
    sSkyModel->Reference();
    sSkyModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sSkyModel->GetMaterial(0)->SetPolygonRangeLength(0x24, 0);
    sSkyModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sSkyModel->GetMaterial(0)->SetVertexRangeLength(0x14);
    long thePolygon;
    for (thePolygon = 0; thePolygon < 36; thePolygon++) {
        sSkyModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon, sSkyIndices[thePolygon * 3]);
        sSkyModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon, sSkyIndices[thePolygon * 3 + 1]);
        sSkyModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon, sSkyIndices[thePolygon * 3 + 2]);
    }
    const float* theRow = &sSkyColorTable[0][1];
    for (i = 0; i < 4; i++) {
        const float* theEntry = theRow;
        theRow += 17 * 3;
        for (j = 0; j < 17; j++, theEntry += 3)
            gSkyColors[i * 17 + j] = GEColor(theEntry[-1], theEntry[0], theEntry[1]);
    }
    Fn1126CE();
    if (gDrawClouds)
        Fn112BC4();
    theDescription.mPixelDepth = 0x10;
    theDescription.mHasAlpha = false;
    theDescription.mCreateMIPMaps = false;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    gAreaEffectTexture = gTextureManager->LoadTexture("textures\\AreaEffect", theDescription);
    ESVERIFY_AT(gAreaEffectTexture != NULL, 417);
    gAreaEffectTexture->Reference();
    gAreaEffectTexture->SetStageAlphaMode(kStageOpTexture, -1);
    gAreaEffectTexture->SetStageColorMode(kStageOpTexture, -1);
    gAreaEffectTexture->SetStageMinFilterMode(0, -1);
    gAreaEffectTexture->SetStageMagFilterMode(0, -1);
    gAreaEffectTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    gAreaEffectTexture->SetStageMipMapBias(0.0f, -1);
    gAreaEffectTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
    theDescription.mPixelDepth = 0x10;
    theDescription.mHasAlpha = false;
    theDescription.mCreateMIPMaps = false;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    gCircleAreaEffectTexture = gTextureManager->LoadTexture("textures\\CircleAreaEffect", theDescription);

    ESVERIFY_AT(gCircleAreaEffectTexture != NULL, 440);
    gCircleAreaEffectTexture->Reference();
    gCircleAreaEffectTexture->SetStageAlphaMode(kStageOpModulate, -1);
    gCircleAreaEffectTexture->SetStageColorMode(kStageOpModulate, -1);
    gCircleAreaEffectTexture->SetStageMinFilterMode(0, -1);
    gCircleAreaEffectTexture->SetStageMagFilterMode(0, -1);
    gCircleAreaEffectTexture->SetStageMipFilterMode(kMipFilterNone, -1);
    gCircleAreaEffectTexture->SetStageMipMapBias(0.0f, -1);
    gCircleAreaEffectTexture->SetStageTextureAddressingMode(kAddressingWrap, -1);
}

// FUNCTION: EXE 0x5131a4
// Draws the sky (stars on space maps, the inner layer only with clouds on; else sky and clouds), the terrain,
// the editor brush highlight, the map's coloured rectangles (erasing faded ones), grid, lines and ground effects
void EEROTerrain::RenderScene()
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    U2DRectangle<long> theTiles;
    GERectangle theRectangle;
    unsigned long theScreen = gScreenIndex;
    float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(gScreenIndex);
    unsigned long theLineCount = gU529050.size();
    unsigned long theAlphaTesting = gRasterizer->GetAlphaTesting(gScreenIndex);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    gRasterizer->SetAlphaTesting(0, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F6DE();
    Fn10F60F();
    gRasterizer->SetFog(0, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendZero, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    Fn10F7C8();
    float theRearClipping = gU4E8F08.GetRearClippingDistance();
    gU4E8F08.SetRearClippingDistance(kSkyRearClipping);
    gU4E8F08.SetDepthRange(1.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    if (gWorld.mMap->mTerrainMesh.GetIsSpaceMap()) {
        gRasterizer->DrawSky(sOuterStarModel, theScreen, 1.0f, false);
        if (gDrawClouds) {
            gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
            gRasterizer->DrawSky(sInnerStarModel, theScreen, 1.0f, false);
        }
    } else {
        gRasterizer->DrawSky(sSkyModel, theScreen, 1.0f, gOrthographicView);
        if (gDrawClouds) {
            gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
            Fn10F772();
            gRasterizer->DrawSky(sCloudModel, theScreen, 1.0f, false);
        }
    }
    gU4E8F08.SetRearClippingDistance(theRearClipping);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetCulling(kCullCCW);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    gRasterizer->SetAlphaTesting(0, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    gRasterizer->SetCulling(kCullNone);
    Fn10F6DE();
    Fn10F60F();
    gRasterizer->Draw(gWorld.mMap->mTerrainMesh, theScreen, gOrthographicView);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, gScreenIndex);
    gRasterizer->SetZBuffer(kZBufferDisabled, gScreenIndex);
    if (gUIManager->mScenarioEditorForm != NULL) {
        theRectangle = gUIManager->mScenarioEditorForm->mBrushGroup->mRectangle;
        if (gUIManager->mScenarioEditorForm->mBrushGroup->mIsHighlightShown) {
            theTiles.mLeft = UMFloor(theRectangle.GetLeft());
            theTiles.mTop = UMFloor(theRectangle.GetTop());
            theTiles.mRight = UMFloor(theRectangle.GetRight());
            theTiles.mBottom = UMFloor(theRectangle.GetBottom());
            gRasterizer->DrawTerrainHighlight(gWorld.mMap->mTerrainMesh, theTiles, gScreenIndex, gOrthographicView,
                                              gBrushHighlightColor, kHighlightOpacity);
        }
    }
    // the map's coloured rectangles, back to front under their lock: a zero opacity erases the element (through the
    // inline accessors whose refused copies are 0x11428f and the ICF-folded 0x1d99fb)
    if (gWorld.mMap->mColoredRectangles.size() > 0) {
        gWorld.mMap->mCriticalSectionForColoredRectangles->Enter();
        for (long theRect = gWorld.mMap->mColoredRectangles.size() - 1; theRect >= 0; theRect--) {
            RTSColoredRectangle& theColored = gWorld.mMap->mColoredRectangles[theRect];
            if (theColored.mOpacity != 0.0f)
                gRasterizer->DrawTerrainHighlight(gWorld.mMap->mTerrainMesh, theColored.mRect, gScreenIndex,
                                                  gOrthographicView, theColored.mColor, theColored.mOpacity);
            else
                gWorld.GetMap()->GetColoredRectangles().erase(gWorld.GetMap()->GetColoredRectangles().begin()
                                                              + theRect);
        }
        gWorld.mMap->mCriticalSectionForColoredRectangles->Leave();
    }
    gU4E8F08.SetDepthRange(0.0f, 0.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetBlending(kBlendOne, kBlendZero, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetZCompare(kCompareLess, theScreen);
    if (this->mGridMode != 0)
        gRasterizer->DrawTerrainGrid
(gWorld.mMap->mTerrainMesh, theScreen, gOrthographicView);
    if (gGridRuns.size() != 0)
        gRasterizer->DrawGridLines(gWorld.mMap->mTerrainMesh, gGridRuns, gAreaEffectTexture, gU4E9330, theScreen,
                                   gOrthographicView);
    for (long i = 0; i < (long)theLineCount; i++) {
        EERenderLine& theLine = gU529050[i];
        gRasterizer->Draw3DLine(theLine.mPoints, theLine.mTexture, theLine.mWidth, theScreen, false);
    }
    gRasterizer->SetZCompare(kCompareLessEqual, theScreen);
    for (theIterator = gEffectQueues[2].begin(); theIterator != gEffectQueues[2].end(); theIterator++) {
        Fn102724(theIterator->first);
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass4();
        Fn103F64();
        Fn102F20();
    }
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetAlphaTesting(theAlphaTesting, theScreen);
    gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, gScreenIndex);
}

// FUNCTION: EXE 0x5138a6
// Steps the terrain grid mode forwards or backwards through 0..17, wrapping around
void EEROTerrain::StepGridMode(bool inBackwards)
{
    if (inBackwards) {
        if (this->mGridMode == 0)
            this->mGridMode = 0x11;
        else
            this->mGridMode--;
    } else {
        if (this->mGridMode == 0x11)
            this->mGridMode = 0;
        else
            this->mGridMode++;
    }
}

// FUNCTION: EXE 0x5138d2
// Releases the sky, cloud and star models and textures and the two area-effect textures
void EEROTerrain::ReleaseSkyModels()
{
    if (sSkyModel)
        sSkyModel->Dereference();
    sSkyModel = NULL;
    if (sCloudModel)
        sCloudModel->Dereference();
    sCloudModel = NULL;
    if (sCloudTexture)
        sCloudTexture->Dereference();
    sCloudTexture = NULL;
    if (sOuterStarModel)
        sOuterStarModel->Dereference();
    sOuterStarModel = NULL;
    if (sInnerStarModel)
        sInnerStarModel->Dereference();
    sInnerStarModel = NULL;
    if (sStarTexture)
        sStarTexture->Dereference();
    sStarTexture = NULL;
    gAreaEffectTexture->Dereference();
    gAreaEffectTexture = NULL;
    gCircleAreaEffectTexture->Dereference();
    gCircleAreaEffectTexture = NULL;
}

// the band of each group of four sky vertices (0x441a04)
static const unsigned long sSkyBands[5] = { 0, 0, 1, 2, 3 };
// the sky vertices (0x441a18)
static const EEROTerrainVertex sSkyVertices[20] = {
    {0, -4500.0f}, {1, -4500.0f}, {2, -4500.0f}, {3, -4500.0f}, {0, 0.0f}, {1, 0.0f}, {2, 0.0f}, {3, 0.0f},
    {0, 150.0f}, {1, 150.0f}, {2, 150.0f}, {3, 150.0f}, {0, 1000.0f}, {1, 1000.0f}, {2, 1000.0f}, {3, 1000.0f},
    {0, 10000.0f}, {1, 10000.0f}, {2, 10000.0f}, {3, 10000.0f},
};
// the star vertices (0x441ab8)
static const EEROTerrainVertex sStarVertices[24] = {
    {3, 5000.0f}, {2, 5000.0f}, {1, 5000.0f}, {0, 5000.0f}, {3, 5000.0f}, {0, 5000.0f}, {0, -4500.0f}, {3, -4500.0f},
    {1, 5000.0f}, {2, 5000.0f}, {2, -4500.0f}, {1, -4500.0f}, {0, 5000.0f}, {1, 5000.0f}, {1, -4500.0f},
    {0, -4500.0f}, {2, 5000.0f}, {3, 5000.0f}, {3, -4500.0f}, {2, -4500.0f}, {0, -4500.0f}, {1, -4500.0f},
    {2, -4500.0f}, {3, -4500.0f},
};
// FUNCTION: EXE 0x51396f
// Places the cloud layer over the map square and scrolls its texture by the environment's cloud offset
// (kept in 0..1) (provisional name)
static void Fn11396F()
{
    GETextureCoordinate theCoordinate4;
    GETextureCoordinate theCoordinate5;
    GETextureCoordinate theCoordinate6;
    GETextureCoordinate theCoordinate7;
    GETextureCoordinate theCoordinate0;
    GETextureCoordinate theCoordinate1;
    GETextureCoordinate theCoordinate2;
    GETextureCoordinate theCoordinate3;
    GE3DPoint thePoint0;
    GE3DPoint thePoint1;
    float theOffset = gRenderEnvironment.mCloudOffset;
    if (sCloudModel == NULL)
        Fn112BC4();
    float theSize = (float)gWorld.mMap->mSize;
    float theLow = theSize - (float)gWorld.mMap->mL195614 - 1.0f;
    float theHigh = (float)gWorld.mMap->mL195614 + 1.0f;
    float theOverhang = kCloudHeight + kCloudOverhang;
    {
        GE3DPoint thePoint(-theOverhang, theLow + 1.0f, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 0);
    }
    {
        GE3DPoint thePoint(theHigh - 1.0f, theSize + theOverhang, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 1);
    }
    {
        GE3DPoint thePoint(theLow + 1.0f, -theOverhang, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 2);
    }
    {
        GE3DPoint thePoint(theSize + theOverhang, theHigh - 1.0f, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 3);
    }
    {
        GE3DPoint thePoint(-kCloudOverhang, theLow + 1.0f, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 4);
    }
    {
        GE3DPoint thePoint(theHigh - 1.0f, theSize + kCloudOverhang, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 5);
    }
    {
        GE3DPoint thePoint(theLow + 1.0f, -kCloudOverhang, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 6);
    }
    {
        GE3DPoint thePoint(theSize + kCloudOverhang, theHigh - 1.0f, kCloudZ);
        sCloudModel->GetVertexArray(0)->SetPoint(thePoint, 7);
    }
    sCloudModel->ComputeModelNormals();
    sCloudModel->ComputeBoundingVolumes();
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint0, 0);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint1, 1);
    float theWidth = thePoint0.DistanceTo(thePoint1);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint0, 0);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint1, 2);
    float theHeight = thePoint0.DistanceTo(thePoint1);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint0, 4);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint1, 5);
    float theInnerU = (1.0f - thePoint0.DistanceTo(thePoint1) / theWidth) * 0.5f;
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint0, 4);
    sCloudModel->GetVertexArray(0)->GetPoint(thePoint1, 6);
    float theInnerV = (1.0f - thePoint0.DistanceTo(thePoint1) / theHeight) * 0.5f;
    theWidth = theWidth / kCloudRepeat;
    theHeight = theHeight / kCloudRepeat;
    theInnerU = theInnerU * theWidth;
    theInnerV = theInnerV * theHeight;
    if (theOffset >= 1.0f) {
        theOffset = theOffset - (float)UMFloor(theOffset);
        gRenderEnvironment.mCloudOffset = theOffset;
    }
    theCoordinate0 = GETextureCoordinate(theOffset, theOffset);
    theCoordinate1 = GETextureCoordinate(theWidth + theOffset, theOffset);
    theCoordinate2 = GETextureCoordinate(theOffset, theHeight + theOffset);
    theCoordinate3 = GETextureCoordinate(theWidth + theOffset, theHeight + theOffset);
    theCoordinate4 = GETextureCoordinate(theInnerU + theOffset, theInnerV + theOffset);
    theCoordinate5 = GETextureCoordinate(theWidth - theInnerU + theOffset, theInnerV + theOffset);
    theCoordinate6 = GETextureCoordinate(theInnerU + theOffset, theHeight - theInnerV + theOffset);
    theCoordinate7 = GETextureCoordinate(theWidth - theInnerU + theOffset,
                                            theHeight - theInnerV + theOffset);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 0);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 1);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 2);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 3);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate4, 4);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate5, 5);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate6, 6);
    sCloudModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate7, 7);
}

// FUNCTION: EXE 0x513ebd
// Blends the sky colours for the environment's time of day (17 steps) into the sky dome and fog colour, and
// stretches the sky dome and star models over the map square (provisional name)
static void Fn113EBD()
{
    float theTime = gRenderEnvironment.mTimeOfDay * 16.0f;
    float theFraction = theTime - (float)UMFloor(theTime);
    unsigned long theIndex = UMFloorUnsigned(theTime);
    unsigned long theNext = theIndex + 1;
    unsigned long i;
    if (theNext > 0x10)
        theNext = 0x10;
    GEColor theColors[4];
    float theComplement = 1.0f - theFraction;
    for (i = 0; i < 4; i++) {
        theColors[i].SetRed(gSkyColors[i * 17 + theIndex].GetRed() * theComplement + gSkyColors[i * 17 + theNext].GetRed() * theFraction);
        theColors[i].SetGreen(gSkyColors[i * 17 + theIndex].GetGreen() * theComplement + gSkyColors[i * 17 + theNext].GetGreen() * theFraction);
        theColors[i].SetBlue(gSkyColors[i * 17 + theIndex].GetBlue() * theComplement + gSkyColors[i * 17 + theNext].GetBlue() * theFraction);
    }
    gU4E9220 = theColors[0];
    for (i = 0; i < 0x14; i++)
        sSkyModel->GetVertexArray(0)->SetColor(theColors[sSkyBands[i >> 2]], i);
    GE2DPoint theCorners[4];
    EEMap* theMap = gWorld.mMap;
    // Keep the map diamond in left, top, right, bottom order before extending the sky.
    theCorners[0].SetX(0.0f);
    theCorners[0].SetY((float)theMap->mSize - theMap->mL195614 - 1.0f);
    theCorners[1].SetX((float)theMap->mL195614 + 1.0f);
    theCorners[1].SetY((float)theMap->mSize);
    theCorners[2].SetX((float)theMap->mSize);
    theCorners[2].SetY((float)theMap->mL195614 + 1.0f);
    theCorners[3].SetX((float)theMap->mSize - theMap->mL195614 - 1.0f);
    theCorners[3].SetY(0.0f);
    theCorners[0].SetY(theCorners[0].GetY() + 1.0f);
    theCorners[1].SetX(theCorners[1].GetX() - 1.0f);
    theCorners[2].SetY(theCorners[2].GetY() - 1.0f);
    theCorners[3].SetX(theCorners[3].GetX() + 1.0f);
    theCorners[0].SetX(-10000.0f);
    theCorners[1].SetY(theCorners[1].GetY() + 10000.0f);
    theCorners[2].SetX(theCorners[2].GetX() + 10000.0f);
    theCorners[3].SetY(-10000.0f);
    GEVertexArray* theSkyArray = sSkyModel->GetVertexArray(0);
    for (i = 0; i < 20; i++) {
        const GE2DPoint& theCorner = theCorners[sSkyVertices[i].mCorner];
        theSkyArray->SetPoint(GE3DPoint(theCorner.GetX(), theCorner.GetY(), sSkyVertices[i].mZ), i);
    }
    sSkyModel->ComputeModelNormals();
    sSkyModel->ComputeBoundingVolumes();
    GEVertexArray* theOuterArray = sOuterStarModel->GetVertexArray(0);
    GEVertexArray* theInnerArray = sInnerStarModel->GetVertexArray(0);
    for (i = 0; i < 24; i++) {
        const EEROTerrainVertex& theVertex = sStarVertices[i];
        const GE2DPoint& theCorner = theCorners[theVertex.mCorner];
        theOuterArray->SetPoint(GE3DPoint(theCorner.GetX(), theCorner.GetY(), theVertex.mZ), i);
        theInnerArray->SetPoint(GE3DPoint(theCorner.GetX() * 0.25f, theCorner.GetY() * 0.25f, theVertex.mZ * 0.25f), i);
    }
    sOuterStarModel->ComputeModelNormals();
    sOuterStarModel->ComputeBoundingVolumes();
    sInnerStarModel->ComputeModelNormals();
    sInnerStarModel->ComputeBoundingVolumes();
}

// the COMDATs closing the unit: the coloured rectangles' erase (vector<RTSColoredRectangle>::erase, its copy and the
// element's implicit operator=) and RTSWorld::GetMap's refused copy
// SYNTHETIC: EXE 0x5141c5
// SYNTHETIC: EXE 0x514229
// SYNTHETIC: EXE 0x514252
// SYNTHETIC: EXE 0x51428f
