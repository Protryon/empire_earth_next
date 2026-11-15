// C:\EEX\Low-Level Engine\Graphics Engine\GELine.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100166d3..0x10016a7b: the U2D key init chains (U2DRectangle.h), the two ctors, the dtor and SetTexture (the
// latter two ICF-folded with GEPoint's identical bodies). Every other GELine member is header-inline, emitted first
// by GEBoundingBox.cpp.
#include <windows.h>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/UReferenceCounter.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/U2DRectangle.h"
#include "lle/GERasterizer.h"
#include "lle/GELine.h"

// FUNCTION: LLE 0x100167f3
// Creates an untextured white line with both ends at the origin and one texture slot per screen
GELine::GELine()
{
    GEVector theNormal(0.0f, 0.0f, -1.0f);
    GE3DPoint thePoint;
    GEColor theColor;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    unsigned long i;
    this->mScreenCount = theRasterizer->GetScreenCount();
    this->mTextures = new GETexture*[this->mScreenCount];
    for (i = 0; i < this->mScreenCount; i++)
        this->mTextures[i] = NULL;
    this->mVertices = theRasterizer->CreateVertexArray(1, 2);
    this->mVertices->SetPoint(thePoint, 0);
    this->mVertices->SetPoint(thePoint, 1);
    this->mVertices->SetNormal(theNormal, 0);
    this->mVertices->SetNormal(theNormal, 1);
    this->mVertices->SetColor(theColor, 0);
    this->mVertices->SetColor(theColor, 1);
    this->mVertices->SetOpacity(1.0f, 0);
    this->mVertices->SetOpacity(1.0f, 1);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 1.0f), 1);
}

// FUNCTION: LLE 0x10016909
// Creates an untextured line between two screen points with a colour per end and one opacity
GELine::GELine(GE2DPoint& inStart, GE2DPoint& inEnd, GEColor& inStartColor, GEColor& inEndColor, float inOpacity)
{
    GE3DPoint theStart;
    GE3DPoint theEnd;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    unsigned long i;
    this->mScreenCount = theRasterizer->GetScreenCount();
    this->mTextures = new GETexture*[this->mScreenCount];
    for (i = 0; i < this->mScreenCount; i++)
        this->mTextures[i] = NULL;
    this->mVertices = theRasterizer->CreateVertexArray(1, 2);
    theStart.SetX(inStart.GetX());
    theStart.SetY(inStart.GetY());
    theEnd.SetX(inEnd.GetX());
    theEnd.SetY(inEnd.GetY());
    this->mVertices->SetPoint(theStart, 0);
    this->mVertices->SetPoint(theEnd, 1);
    this->mVertices->SetColor(inStartColor, 0);
    this->mVertices->SetColor(inEndColor, 1);
    this->mVertices->SetOpacity(inOpacity, 0);
    this->mVertices->SetOpacity(inOpacity, 1);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(1.0f, 1.0f), 1);
}

// FUNCTION: LLE 0x10016a11
// Destroys the line, deleting its vertex array and releasing the texture of every screen
GELine::~GELine()
{
    unsigned long i;
    delete this->mVertices;
    for (i = 0; i < this->mScreenCount; i++)
        if (this->mTextures[i])
            this->mTextures[i]->Dereference();
    delete[] this->mTextures;
}

// FUNCTION: LLE 0x10016a49
// Sets the line's texture for one screen, releasing the old one and referencing the new one
void GELine::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    if (this->mTextures[inScreen])
        this->mTextures[inScreen]->Dereference();
    this->mTextures[inScreen] = inTexture;
    if (inTexture)
        inTexture->Reference();
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100166dd
// SYNTHETIC: LLE 0x100166ed
// SYNTHETIC: LLE 0x1001670d
// SYNTHETIC: LLE 0x1001671d
// SYNTHETIC: LLE 0x1001673d
// SYNTHETIC: LLE 0x1001674d
// SYNTHETIC: LLE 0x1001676d
// SYNTHETIC: LLE 0x1001677d
// SYNTHETIC: LLE 0x1001679d
// SYNTHETIC: LLE 0x100167ad
// SYNTHETIC: LLE 0x100167cd
// SYNTHETIC: LLE 0x100167dd

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x100166d3
// SYNTHETIC: LLE 0x100166f9
// SYNTHETIC: LLE 0x10016703
// SYNTHETIC: LLE 0x10016729
// SYNTHETIC: LLE 0x10016733
// SYNTHETIC: LLE 0x10016759
// SYNTHETIC: LLE 0x10016763
// SYNTHETIC: LLE 0x10016789
// SYNTHETIC: LLE 0x10016793
// SYNTHETIC: LLE 0x100167b9
// SYNTHETIC: LLE 0x100167c3
// SYNTHETIC: LLE 0x100167e9
