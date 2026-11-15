// C:\EEX\Low-Level Engine\Graphics Engine\GEPoint.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x10022fca..0x10023209: the U2D key init chains and the two ctors. The dtor and SetTexture are ICF-folded with
// GELine's identical bodies (kept at 0x10016a11/0x10016a49); every other member is header-inline.
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
#include "lle/GEPoint.h"

// FUNCTION: LLE 0x100230ea
// Creates a single-vertex point primitive with no textures, fully opaque
GEPoint::GEPoint()
{
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    unsigned long i;
    this->mScreenCount = theRasterizer->GetScreenCount();
    this->mTextures = new GETexture*[this->mScreenCount];
    for (i = 0; i < this->mScreenCount; i++)
        this->mTextures[i] = NULL;
    this->mVertices = theRasterizer->CreateVertexArray(1, 1);
    this->mVertices->SetOpacity(1.0f, 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 0);
}

// FUNCTION: LLE 0x1002315d
// Creates a point primitive at a 2D position with a colour and opacity, with no textures
GEPoint::GEPoint(GE2DPoint& inPoint, GEColor& inColor, float inOpacity)
{
    GE3DPoint thePoint;
    GERasterizer* theRasterizer = GERasterizer::GetActiveRasterizer();
    unsigned long i;
    this->mScreenCount = theRasterizer->GetScreenCount();
    this->mTextures = new GETexture*[this->mScreenCount];
    for (i = 0; i < this->mScreenCount; i++)
        this->mTextures[i] = NULL;
    this->mVertices = theRasterizer->CreateVertexArray(1, 1);
    thePoint.SetX(inPoint.GetX());
    thePoint.SetY(inPoint.GetY());
    this->mVertices->SetPoint(thePoint, 0);
    this->mVertices->SetColor(inColor, 0);
    this->mVertices->SetOpacity(inOpacity, 0);
    this->mVertices->SetTextureCoordinate(GETextureCoordinate(0.0f, 0.0f), 0);
}

// Not in this unit's range (ICF keeps GELine.cpp's identical bodies), but defined here.
GEPoint::~GEPoint()
{
    unsigned long i;
    delete this->mVertices;
    for (i = 0; i < this->mScreenCount; i++)
        if (this->mTextures[i])
            this->mTextures[i]->Dereference();
    delete[] this->mTextures;
}

void GEPoint::SetTexture(GETexture* inTexture, unsigned long inScreen)
{
    if (this->mTextures[inScreen])
        this->mTextures[inScreen]->Dereference();
    this->mTextures[inScreen] = inTexture;
    if (inTexture)
        inTexture->Reference();
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// SYNTHETIC: LLE 0x10022fca
// SYNTHETIC: LLE 0x10022fd4
// SYNTHETIC: LLE 0x10022fe4
// SYNTHETIC: LLE 0x10022ff0
// SYNTHETIC: LLE 0x10022ffa
// SYNTHETIC: LLE 0x10023004
// SYNTHETIC: LLE 0x10023014
// SYNTHETIC: LLE 0x10023020
// SYNTHETIC: LLE 0x1002302a
// SYNTHETIC: LLE 0x10023034
// SYNTHETIC: LLE 0x10023044
// SYNTHETIC: LLE 0x10023050
// SYNTHETIC: LLE 0x1002305a
// SYNTHETIC: LLE 0x10023064
// SYNTHETIC: LLE 0x10023074
// SYNTHETIC: LLE 0x10023080
// SYNTHETIC: LLE 0x1002308a
// SYNTHETIC: LLE 0x10023094
// SYNTHETIC: LLE 0x100230a4
// SYNTHETIC: LLE 0x100230b0
// SYNTHETIC: LLE 0x100230ba
// SYNTHETIC: LLE 0x100230c4
// SYNTHETIC: LLE 0x100230d4
// SYNTHETIC: LLE 0x100230e0
