// C:\EEX\Empire Earth\Graphics Thread\EEROGroundShadows.cpp (no __FILE__; name from the RTTI class). The ground
// shadows' render object (vtable 0x4413f0): 0x111dae..0x11251f (ctor, ??_G, dtor, Virtual00, the static shadow
// model's set-up, Virtual08, the two shadow passes, the space-tile test, the model's release); EEROTerrain.cpp's
// init chains start at 0x11251f. The two floats before the vtable (0x4413e4/0x4413e8) are this unit's named consts.
// Unit flags: the exe's module set.
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
#include "lle/GEModelManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EERenderLists.h"
#include "ee/EEROGroundShadows.h"
#include "LostDebug.h"

// the shadows' named consts (.rdata 0x4413e4/0x4413e8, reloaded per use) (provisional names)
const float kShadowHeight = 0.01f;
const float kShadowFade = 0.1f;

// the static shadow quad (0x529098) (provisional name)
static GEModel* sShadowModel;

// FUNCTION: EXE 0x511dae
// Creates the ground shadows' render object
EEROGroundShadows::EEROGroundShadows()
{
}

// SYNTHETIC: EXE 0x511dc0
// (the scalar deleting destructor, emitted with the vtable; slot EEROGroundShadows+0x10)

// FUNCTION: EXE 0x511ddc
// Destroys the ground shadows' render object
EEROGroundShadows::~EEROGroundShadows()
{
}

// FUNCTION: EXE 0x511de7
// Prepares the frame and fills the render lists of shadows and units
void EEROGroundShadows::FrameStarting()
{
    this->Fn10C036();
    this->FillRenderLists();
}

// FUNCTION: EXE 0x511df8
// Creates the shared shadow quad model (StaticShadow): a unit square in the ground plane
void EEROGroundShadows::CreateShadowModel()
{
    GETextureCoordinate theCoordinate0(0.0f, 0.0f);
    GETextureCoordinate theCoordinate1(1.0f, 0.0f);
    GETextureCoordinate theCoordinate2(0.0f, 1.0f);
    GETextureCoordinate theCoordinate3(1.0f, 1.0f);
    sShadowModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "StaticShadow", 1);
    sShadowModel->Reference();
    {
        GE3DPoint thePoint(-0.5f, 0.5f, 0.0f);

        sShadowModel->GetVertexArray(0)->SetPoint(thePoint, 0);
        thePoint.SetX(0.5f);
        thePoint.SetY(0.5f);
        thePoint.SetZ(0.0f);
        sShadowModel->GetVertexArray(0)->SetPoint(thePoint, 1);
        thePoint.SetX(-0.5f);
        thePoint.SetY(-0.5f);
        thePoint.SetZ(0.0f);
        sShadowModel->GetVertexArray(0)->SetPoint(thePoint, 2);
        thePoint.SetX(0.5f);
        thePoint.SetY(-0.5f);
        thePoint.SetZ(0.0f);
        sShadowModel->GetVertexArray(0)->SetPoint(thePoint, 3);
    }
    sShadowModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate0, 0);
    sShadowModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, 1);
    sShadowModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, 2);
    sShadowModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, 3);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex2(0, 3);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex3(0, 1);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex1(1, 0);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex2(1, 2);
    sShadowModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    sShadowModel->GetMaterial(0)->SetColor(GEColor());
    sShadowModel->GetMaterial(0)->SetOpacity(1.0f);
    sShadowModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    sShadowModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    sShadowModel->GetMaterial(0)->SetVertexStartingIndex(0);
    sShadowModel->GetMaterial(0)->SetVertexRangeLength(4);
    sShadowModel->ComputeModelNormals();
    sShadowModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x51207f
// Draws the static shadows and unit shadows (when ground shadows are on), then the effects' ground pass
// (queue 0) per texture, alpha-blended with z test only
void EEROGroundShadows::RenderScene()
{
    EEEffectQueue::iterator theIterator;
    std::deque<EEEffect*>::iterator theEffect;
    unsigned long theScreen = gScreenIndex;
    Fn10F7C8();
    Fn10F60F();
    float theAlphaRefValue = gRasterizer->GetAlphaTestingRefValue(gScreenIndex);
    gRasterizer->SetAlphaTestingRefValue(0.0f, gScreenIndex);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
    gRasterizer->SetShadingColorSource(0, theScreen);
    gRasterizer->SetCulling(kCullNone);
    if (gDrawGroundShadows) {
        this->DrawShadows(EERenderLists::sInstance.mShadows2);
        this->DrawUnitShadows(EERenderLists::sInstance.mUnits2);
    }
    for (theIterator = gEffectQueues[0].begin(); theIterator != gEffectQueues[0].end(); theIterator++) {
        Fn102724(theIterator->first);
        for (theEffect = theIterator->second.begin(); theEffect != theIterator->second.end(); theEffect++)
            (*theEffect)->DrawPass2();
        Fn103F64();
        Fn102F20();
    }
    gRasterizer->SetAlphaTestingRefValue(theAlphaRefValue, gScreenIndex);
}

// FUNCTION: EXE 0x5121c8
// Draws each static shadow with a positive scale as a textured quad just above its location
void EEROGroundShadows::DrawShadows(EERenderShadowMap& inShadows)
{
    GETransformation theTransformation;
    unsigned long theScreen = gScreenIndex;
    EERenderShadowMap::iterator theIterator;
    theIterator = inShadows.begin();
    while (theIterator != inShadows.end()) {
        EERenderShadow* theShadow = theIterator->second;
        if (theShadow->mScale > 0.0f) {
            theTransformation.SetX(theShadow->mLocation.GetX());
            theTransformation.SetY(theShadow->mLocation.GetY());
            theTransformation.SetZ(theShadow->mLocation.GetZ() + kShadowHeight);
            sShadowModel->SetGraphicsPostModelTransform(theTransformation);
            sShadowModel->SetGraphicsScale(theShadow->mScale);
            sShadowModel->PrepareTransforms();
            sShadowModel->GetMaterial(0)->SetTexture(theIterator->first, theScreen);
            sShadowModel->SetOpacity(theShadow->mOpacity);
            gRasterizer->Draw(sShadowModel, theScreen, 1.0f, gOrthographicView);
        }
        theIterator++;
    }
}

static bool Fn11246B(const GE3DPoint& inLocation);

// FUNCTION: EXE 0x5122bb
// Draws each unit's shadow (its own model or the quad) on the ground under it, turned to its heading and
// shrunk by height (fade 1 - 0.1 per unit of height; none from 10 up); skips submarines and units over space
void EEROGroundShadows::DrawUnitShadows(EERenderUnitMap& inUnits)
{
    GETransformation theTransformation;
    unsigned long theScreen = gScreenIndex;
    EERenderUnitMap::iterator theIterator;
    theIterator = inUnits.begin();
    while (theIterator != inUnits.end()) {
        EERenderUnit* theUnit = theIterator->second;
        if (theUnit->mCategory != kUnitCategorySubmergedNaval && theUnit->mShadowScale > 0.0f) {
            if (!Fn11246B(theUnit->mTransformation.GetPosition())) {
                GEModel* theModel = theUnit->mSource->mShadowModel;
                if (theModel == NULL)
                    theModel = sShadowModel;
                theTransformation.SetOrientationYPR(UMArcTan2(theUnit->mTransformation.GetElement(1, 0),
                                                              theUnit->mTransformation.GetElement(0, 0)), 0.0f, 0.0f);
                theTransformation.SetX(theUnit->mTransformation.GetX());
                theTransformation.SetY(theUnit->mTransformation.GetY());
                theTransformation.SetZ(kShadowHeight + theUnit->mGroundZ);
                theModel->SetGraphicsPostModelTransform(theTransformation);
                float theFade = 1.0f - (theUnit->mTransformation.GetZ() - theUnit->mGroundZ) * kShadowFade;
                theFade = min(1.0f, theFade);
                // A unit 10 or more units above the ground casts no shadow (a negative scale would mirror it).
                if (theFade > 0.0f) {
                    float theScale = theUnit->mScale / gUnitClassTable[theUnit->mUnitClassIndex]->GetScale() * theUnit->mShadowScale;
                    theModel->SetGraphicsScale(theScale * theFade);
                    theModel->SetOpacity(theUnit->mOpacity);
                    theModel->GetMaterial(0)->SetTexture(theUnit->mSource->mShadowTexture, theScreen);
                    theModel->PrepareTransforms();
                    gRasterizer->Draw(theModel, theScreen, 1.0f, gOrthographicView);
                }
            }
        }
        theIterator++;
    }
    // LOST-DEBUG: none: exact only (tools lostdebug_search)
    EE_LOST_DEBUG(3);
}

// FUNCTION: EXE 0x51246b
// Returns whether a point is over space on a space map (a space tile, or off the mesh) (inferred)
static bool Fn11246B(const GE3DPoint& inLocation)
{
    if (!gWorld.mMap->mTerrainMesh.GetIsSpaceMap())
        return false;
    unsigned long theX = UMFloorUnsigned(inLocation.GetX());
    unsigned long theY = UMFloorUnsigned(inLocation.GetY());
    EEMap* theMap = gWorld.mMap;
    if (theMap->mTerrainMesh.GetIsValidTile(theX, theY))
        return theMap->mTerrainMesh.GetWaterInfo(theX, theY) == 0x80;
    return true;
}

// FUNCTION: EXE 0x512513
// Releases the shared shadow quad model
void EEROGroundShadows::ReleaseShadowModel()
{
    sShadowModel->Dereference();
}
