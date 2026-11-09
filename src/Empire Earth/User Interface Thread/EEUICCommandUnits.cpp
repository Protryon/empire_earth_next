// C:\EEX\Empire Earth\User Interface Thread\EEUICCommandUnits.cpp (file name inferred: no __FILE__; RTTI class
// EEUICCommandUnits). The main game form's right-drag command control: 0x257881..0x258464 (ctor, ??_G, dtor, then
// the members in alphabetical order; EEUICPing's ctor at 0x258464). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UIButton.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPlayer.h"
#include "ee/EEUnit.h"
#include "ee/EEMessage.h"
#include "ee/EECommunications.h"
#include "lle/UResourceFile.h"
#include "lle/GEFont.h"
#include "lle/GEText.h"
#include "lle/UIScrollBar.h"
#include "lle/UITextLabel.h"
#include "lle/UIEditBox.h"
#include "ee/EmpireEarth.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEGraphics.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "lle/GEMaterial.h"
#include "lle/GEPolygonArray.h"
#include "lle/GE3DLine.h"
#include "ee/EEAction.h"
#include "ee/EEAMouseClick.h"
#include "ee/EEUICCommandUnits.h"

extern unsigned long& gTexturePixelDepth;    // 0x5308b4
extern GEViewport gU4E8F08;                 // 0x4e8f08 the game view viewport (provisional name)
extern float& gCameraZoom;                   // 0x5317f0 (the arrow scale less 0.5) (provisional name)

static const float kCommandUnitsDragFraction = 0.02f;   // 0x449e84 (a unit-local constant)

// FUNCTION: EXE 0x657881
// Creates the main game view's right-drag command control with its facing arrow model and texture, hidden
EEUICCommandUnits::EEUICCommandUnits(UIForm* inForm, unsigned long inId, unsigned long inIndex,
                                     EEAMouseClick* inListener)
    : UIControl(kControlTypeCustom, inId, GERectangle(gViewport.GetViewRectangle().GetTopLeft(), 2.0f, 2.0f), inForm, inIndex, 0),
      mFacing(0.0f)
{
    this->mCanFace = false;
    this->mHasFacing = false;
    this->mDisplayMode = kDisplayModeHidden;
    this->mListener = inListener;
    this->mListener->Reference();
    this->SetAlwaysDirty(true);
    this->CreateModel();
    this->LoadTexture();
    this->mModel->GetMaterial(0)->SetTexture(this->mTexture, gScreenIndex);
}

// SYNTHETIC: EXE 0x657955
// (the scalar deleting destructor, emitted with the vtable; slot EEUICCommandUnits+0x0)

// FUNCTION: EXE 0x657971
// Destroys the command control, its arrow model and texture
EEUICCommandUnits::~EEUICCommandUnits()
{
    this->mListener->Dereference();
    this->mModel->Dereference();
    this->mTexture->Dereference();
}

// FUNCTION: EXE 0x6579a8
// Returns the heading from one point to another in the ground plane
float EEUICCommandUnits::ComputeAngle(const GE3DPoint& inFrom, const GE3DPoint& inTo)
{
    return UMArcTan2(-(inTo.GetX() - inFrom.GetX()), inTo.GetY() - inFrom.GetY());
}

// FUNCTION: EXE 0x6579e5
// Snapshots whether and where the facing arrow is drawn
long EEUICCommandUnits::CopyData()
{
    this->mDrawnHasFacing = this->mHasFacing;
    this->mDrawnFacing = this->mFacing;
    return UIControl::CopyData();
}

// FUNCTION: EXE 0x657a03
// Builds the facing arrow's textured quad model
void EEUICCommandUnits::CreateModel()
{
    GE3DPoint thePoint1(-0.25f, 0.5f, 0.0f);
    GE3DPoint thePoint2(0.25f, 0.5f, 0.0f);
    GE3DPoint thePoint4(-0.25f, 0.0f, 0.0f);
    GE3DPoint thePoint3(0.25f, 0.0f, 0.0f);
    GETextureCoordinate theTopLeft(0.0f, 0.0f);
    GETextureCoordinate theTopRight(1.0f, 0.0f);
    GETextureCoordinate theBottomLeft(0.0f, 1.0f);
    GETextureCoordinate theBottomRight(1.0f, 1.0f);
    GEVector theNormal(0.0f, 0.0f, 1.0f);
    GEModel* theModel = gModelManager->CreateModel(2, 4, 1, 0, 1, 0, "Unit Direction Model", 1);
    this->mModel = theModel;
    theModel->Reference();
    theModel->GetVertexArray(0)->SetPoint(thePoint1, 0);
    theModel->GetVertexArray(0)->SetPoint(thePoint2, 1);
    theModel->GetVertexArray(0)->SetPoint(thePoint3, 2);
    theModel->GetVertexArray(0)->SetPoint(thePoint4, 3);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 0);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 1);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 2);
    theModel->GetVertexArray(0)->SetNormal(theNormal, 3);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theTopLeft, 0);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theTopRight, 1);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theBottomRight, 2);
    theModel->GetVertexArray(0)->SetTextureCoordinate(theBottomLeft, 3);
    theModel->GetPolygonArray(0)->SetVertexIndex1(0, 0);
    theModel->GetPolygonArray(0)->SetVertexIndex2(0, 1);
    theModel->GetPolygonArray(0)->SetVertexIndex3(0, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex1(1, 0);
    theModel->GetPolygonArray(0)->SetVertexIndex2(1, 2);
    theModel->GetPolygonArray(0)->SetVertexIndex3(1, 3);
    theModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    theModel->GetMaterial(0)->SetPolygonRangeLength(2, 0);
    theModel->GetMaterial(0)->SetVertexStartingIndex(0);
    theModel->GetMaterial(0)->SetVertexRangeLength(4);
    theModel->ComputeBoundingVolumes();
}

// FUNCTION: EXE 0x657c4d
// Draws the facing arrow at the command point over everything, restoring the rasterizer state afterwards
long EEUICCommandUnits::DrawPolygons()
{
    GEColor theWhite(1.0f, 1.0f, 1.0f);
    GEColor theAmbientColor = gRasterizer->GetAmbientColor();
    GETransformation theTransformation;
    GEViewport theViewport(gRasterizer->GetViewport(gScreenIndex));
    unsigned long theScreen = gScreenIndex;
    float theAmbientIntensity = gRasterizer->GetAmbientIntensity();
    unsigned long theLighting = gRasterizer->GetLighting(theScreen);
    unsigned long theZBuffer = gRasterizer->GetZBuffer(theScreen);
    unsigned long theZCompare = gRasterizer->GetZCompare(theScreen);
    if (this->mDrawnHasFacing) {
        gU4E8F08.SetDepthRange(0.0f, 0.0f);
        gRasterizer->SetViewport(gU4E8F08, theScreen);
        gRasterizer->SetZBuffer(kZBufferTestOnly, theScreen);
        gRasterizer->SetZCompare(kCompareLess, theScreen);
        gRasterizer->SetAmbientColor(theWhite, theScreen);
        gRasterizer->SetAmbientIntensity(1.0f, theScreen);
        gRasterizer->SetLighting(0, theScreen);
        theTransformation.SetOrientationYPR(this->mDrawnFacing, 0.0f, 0.0f);
        const GE3DPoint& thePosition = this->mPosition;
        theTransformation.SetPosition(thePosition.GetX(), thePosition.GetY(), thePosition.GetZ());
        this->mModel->SetGraphicsCurrentFrame(0);
        float theZoom = gCameraZoom;
        this->mModel->SetGraphicsScale(theZoom + 0.5f);
        this->mModel->SetGraphicsPostModelTransform(theTransformation);
        this->mModel->PrepareTransforms();
        gRasterizer->Draw(this->mModel, theScreen, 1.0f, gOrthographicView);
        gU4E8F08.SetDepthRange(0.0f, 1.0f);
        gRasterizer->SetViewport(theViewport, theScreen);
        gRasterizer->SetZBuffer(theZBuffer, theScreen);
        gRasterizer->SetZCompare(theZCompare, theScreen);
        gRasterizer->SetAmbientColor(theAmbientColor, theScreen);
        gRasterizer->SetAmbientIntensity(theAmbientIntensity, theScreen);
        gRasterizer->SetLighting(theLighting, theScreen);
    }
    return UIControl::DrawPolygons();
}

// FUNCTION: EXE 0x657ece
// Loads the "Unit Facing Arrow" texture with clamped, alpha-blended stage settings
void EEUICCommandUnits::LoadTexture()
{
    UString theName("textures\\Unit Facing Arrow");
    GETextureDescription theDescription;
    theDescription.mPixelDepth = gTexturePixelDepth;
    theDescription.mNumMipLevelsToDrop = 0;
    theDescription.mSurfaceVolatility = 1;
    theDescription.mFrameFormat = 0;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mHasAlpha = true;
    theDescription.mCreateMIPMaps = true;
    this->mTexture = gTextureManager->LoadTexture(theName, theDescription);
    if (this->mTexture) {
        this->mTexture->Reference();
        this->mTexture->SetStageAlphaMode(kStageOpTexture, -1);
        this->mTexture->SetStageColorMode(kStageOpTexture, -1);
        this->mTexture->SetStageMinFilterMode(1, -1);
        this->mTexture->SetStageMagFilterMode(1, -1);
        this->mTexture->SetStageMipFilterMode(kMipFilterPoint, -1);
        this->mTexture->SetStageMipMapBias(-1.0f, -1);
        this->mTexture->SetStageTextureAddressingMode(kAddressingClamp, -1);
    }
}

// FUNCTION: EXE 0x657fa9
// While right-dragging, turns the facing arrow toward the ground point under the mouse once the drag passes
// 2% of the window width
// NEAR: registers (the original keeps 0 in ebx for the bool tests and the unsigned conversion; the theFar.x copy is a mov)
long EEUICCommandUnits::MouseMove(const GE2DPoint& inLocation)
{
    GE3DPoint theNear;
    GE3DPoint theFar;
    GE3DPoint theNearWorld;
    GE3DPoint theFarWorld;
    GE3DPoint theTarget;
    GE3DLine theLine;
    float theT;
    float theThreshold = gGameWindow->GetWidth() * kCommandUnitsDragFraction;
    float theDistance = inLocation.DistanceToSquared(this->mDownLocation);
    float theScale = 1.0f / gU4E8F08.GetHalfWidth();
    theNear.SetX((inLocation.GetX() - gU4E8F08.GetXCenter()) * theScale);
    theFar.SetX(theNear.GetX());
    theNear.SetY(-((inLocation.GetY() - gU4E8F08.GetYCenter()) * theScale));
    theNear.SetZ(0.0f);
    theFar.SetY(theNear.GetY());
    theFar.SetZ(1.0f);
    if (!gOrthographicView) {
        gU4E8F08.DenormalizePerspectivePoint(theNear, gU4E8F08.GetFrontClippingDistance(), theNearWorld);
        gU4E8F08.DenormalizePerspectivePoint(theFar, gU4E8F08.GetRearClippingDistance(), theFarWorld);
    } else {
        gU4E8F08.DenormalizeOrthographicPoint(theNear, theNearWorld);
        gU4E8F08.DenormalizeOrthographicPoint(theFar, theFarWorld);
    }
    theLine.Define(theNearWorld, theFarWorld);
    theLine.SolveForZ(this->mPosition.GetZ(), theT, theTarget);
    if (this->mCanFace && !this->mHasFacing)
        this->mHasFacing = theThreshold * theThreshold < theDistance;
    if (this->mHasFacing)
        this->mFacing = this->ComputeAngle(this->mPosition, theTarget);
    return 0;
}

// FUNCTION: EXE 0x658170
// On release, sends the command to the clicked point with the dragged facing (or facing away from the group's
// centre when units can face) and hides the control
long EEUICCommandUnits::MouseRButtonUp()
{
    GE2DPoint theCenter;
    GE3DPoint theTarget;
    GE3DPoint thePoint;
    GERectangle theBounds;
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    bool theWasDragging = this->mHasFacing;
    EETile* theLocation;
    bool theHit = EEPointUnitOrTile(this->mDownLocation, theLocation, thePoint, true);
    if (this->mCanFace && !this->mHasFacing) {
        gWorld.GetCurrentPlayer()->mSelectedGroup->GetBounds(theBounds);
        theBounds.GetCenter(theCenter);
        theTarget.SetX(theCenter.GetX());
        theTarget.SetY(theCenter.GetY());
        theTarget.SetZ(this->mPosition.GetZ());
        this->mFacing = this->ComputeAngle(theTarget, this->mPosition);
        this->mHasFacing = true;
    }
    this->mListener->Execute(thePoint.GetX(), thePoint.GetY(), theHit, theModifiers, this->mHasFacing, this->mFacing, theWasDragging);
    this->SetDisplayMode(kDisplayModeHidden);
    this->ReleaseCapturedMouse();
    return 0;
}

// FUNCTION: EXE 0x65829c
// Starts a right-drag command at the point under the mouse, noting whether any selected unit can face; with
// no unit or tile there it sends the command at once without a target
void EEUICCommandUnits::StartCommand(const GE2DPoint& inLocation)
{
    EESelectedGroup* theGroup = gWorld.GetCurrentPlayer()->mSelectedGroup;
    unsigned long theModifiers = gISKeyboard->GetModifierState();
    unsigned long theCount = theGroup->GetNumUnits();
    bool theCanFace = false;
    EETile* theLocation;
    if (EEPointUnitOrTile(inLocation, theLocation, this->mPosition, true)) {
        for (unsigned long i = 0; i < theCount && !theCanFace; i++) {
            long theKind = theGroup->GetUnit(i)->GetUnitClass()->mCategory;
            if (theKind == kUnitCategoryNaval || theKind == kUnitCategoryLand || theKind == kUnitCategoryLandCitizen || theKind == kUnitCategoryHelicopter || theKind == kUnitCategorySubmergedNaval || theKind == kUnitCategoryLandPriest ||
                theKind == kUnitCategoryNavalPriest || theKind == kUnitCategoryNavalCitizen || theKind == kUnitCategoryLand8 || theKind == kUnitCategoryLand15 || theKind == kUnitCategoryLandMilitaryHero || theKind == kUnitCategoryLandTransport ||
                theKind == kUnitCategoryHelicopterTransport || theKind == kUnitCategoryNavalTransport || theKind == kUnitCategorySpaceTransport || theKind == kUnitCategorySpaceship || theKind == kUnitCategoryHelicopterPriest || theKind == kUnitCategoryNavalBuilding)
                theCanFace = true;
            else
                theCanFace = false;
        }
        this->mCanFace = theCanFace;
        this->mHasFacing = false;
        this->mDownLocation = inLocation;
        this->mDownLocation.SetX((float)UMFloatToLong(this->mDownLocation.GetX()));
        this->mDownLocation.SetY((float)UMFloatToLong(this->mDownLocation.GetY()));
        this->SetRectangle(GERectangle(this->mDownLocation, this->mDownLocation));
        this->GetForm()->RedrawIntersectControls(this->GetRectangle());
        this->CaptureKeyboard();
        this->CaptureMouse();
        this->ReleaseCapturedKeyboard();
        this->UpdateMouse(inLocation);
        this->SetDisplayMode(kDisplayModeShown);
    } else
        this->mListener->Execute(0.0f, 0.0f, false, theModifiers, false, 0.0f, false);
}
