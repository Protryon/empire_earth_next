// C:\EEX\Empire Earth\Graphics Thread\EEE3DGoal.cpp (no __FILE__; name from the RTTI class and its key
// "EEE3DGoal-Data"). 0xf1327..0xf1600: the key chain, both ctors, Load, Virtual0C, Save; ??_G and the slots it does
// not override are EEE3DIcon's. Unit flags: the exe's module set.
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
#include "lle/GERasterizer.h"
#include "lle/GEMaterial.h"
#include "lle/GEModel.h"
#include "lle/SESoundManager.h"

#include "ee/EmpireEarth.h"
#include "ee/EEMap.h"
#include "ee/EEUnit.h"
#include "ee/EEPlayer.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/RTSGraphics.h"
#include "ee/EEGraphics.h"
#include "ee/EEMedia.h"
#include "ee/EEGameSound.h"
#include "ee/EEGraphicEffect.h"
#include "ee/EEGraphicObject.h"
#include "ee/EERenderObject.h"
#include "ee/EEEffect.h"
#include "ee/EEEffect2D.h"
#include "ee/EEEffect3D.h"
#include "lle/GEText.h"
#include "lle/GERectangle.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "ee/EEUIManager.h"
#include "ee/EEUIFMainGame.h"
#include "ee/EEUIFScenarioEditor.h"
#include "ee/EEE3DIcon.h"
#include "ee/EEE3DGoal.h"

// the label's height over the goal (0x440804: this unit's .rdata, before the vtable) (provisional name)
const float kGoalLabelHeight = 1.2f;

// SYNTHETIC: EXE 0x4f1327
// SYNTHETIC: EXE 0x4f1331
// SYNTHETIC: EXE 0x4f1342
// SYNTHETIC: EXE 0x4f134e
const UString EEE3DGoal::sData("EEE3DGoal-Data");

// FUNCTION: EXE 0x4f136a
// Creates a scenario goal marker icon on a unit or at a location, with no countdown
EEE3DGoal::EEE3DGoal(EEUnit* inUnit, EEGraphicEffect* inGraphicEffect, const GE3DPoint* inLocation)
    : EEE3DIcon(inUnit, inGraphicEffect, inLocation, 0x3c), mTimeLeft(0)
{
}

// FUNCTION: EXE 0x4f1393
// Creates an empty goal marker for loading from a saved game
EEE3DGoal::EEE3DGoal()
    : EEE3DIcon(0x3c), mTimeLeft(0)
{
}

// FUNCTION: EXE 0x4f13ae
// Reads the icon and the countdown time left; returns the first error
long EEE3DGoal::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    long theResult = EEE3DIcon::Load(inMajorVersion, inMinorVersion, inFile);
    if (theResult == 0)
        theResult = inFile.Read(this->mTimeLeft);
    return theResult;
}

// FUNCTION: EXE 0x4f13db
// While a countdown runs, draws its m:ss time above the goal as white text with a black shadow, on the game or
// scenario editor form; returns 0
// NEAR: the original keeps 0 in ebx, stores the transparency before the sprintf pushes and pushes the draw
// arguments before it picks the form (an inline accessor?)
long EEE3DGoal::DrawPass3()
{
    GE3DPoint thePosition;
    GE2DPoint theScreenPoint;
    GERectangle theRectangle;
    GEText theText(gFontTahoma10);
    UWideString theString;
    GEViewport& theViewport = gRasterizer->GetViewport(gScreenIndex);
    if (this->mTimeLeft > 0) {
        unsigned long theMinutes;
        unsigned long theSeconds;
        gPhysics->GetMinutesAndSeconds(&theMinutes, &theSeconds, this->mTimeLeft);
        theRectangle = theViewport.GetViewRectangle();
        theText.SetIsTransparent(true);
        theText.SetFgColor(GEColor(0.0f, 0.0f, 0.0f));
        theText.SetAlignment(kTextAlignCenter);
        sprintf(theString, "%d:%.2d", theMinutes, theSeconds);
        theText.SetText(theString);
        thePosition = this->mLocation;
        thePosition.SetZ(thePosition.GetZ() + kGoalLabelHeight);
        Fn118676(thePosition, theScreenPoint);
        theText.SetLocation(theScreenPoint);
        (!gFreeCamera ? static_cast<UIForm*>(gUIManager->mMainGameForm) : static_cast<UIForm*>(gUIManager->mScenarioEditorForm))
            ->DrawClippedText(theRectangle, &theText, NULL);
        theScreenPoint.SetX(theScreenPoint.GetX() - 1.0f);
        theScreenPoint.SetY(theScreenPoint.GetY() - 1.0f);
        theText.SetLocation(theScreenPoint);
        theText.SetFgColor(GEColor());
        (!gFreeCamera ? static_cast<UIForm*>(gUIManager->mMainGameForm) : static_cast<UIForm*>(gUIManager->mScenarioEditorForm))
            ->DrawClippedText(theRectangle, &theText, NULL);
    }
    return 0;
}

// FUNCTION: EXE 0x4f15d0
// Writes the icon (passing the versions in order) and the countdown time left
long EEE3DGoal::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    long theResult = EEE3DIcon::Save(inMajorVersion, inMinorVersion, outFile);
    if (theResult == 0)
        theResult = outFile.Write(this->mTimeLeft);
    return theResult;
}
