// C:\EEX\Empire Earth\Graphics Thread\EEE3DVolcano.cpp (__FILE__ at BuildModel 0xfe79d). 0xfe553..0x101c34: the key
// chain, both ctors, the dtor, BuildModel, the virtuals, then the template COMDATs of BuildModel's vectors
// (0x100d4d..0x101c34). Unit flags: the exe's module set.
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
#include "lle/GERectangle.h"
#include "lle/GEPolygonArray.h"
#include "lle/GEModelManager.h"
#include "lle/Graphics Engine.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "ee/EEDatabaseTables.h"
#include "ee/EEE3DVolcano.h"
#include "LostDebug.h"
static const float kVolcanoPixelScale = 1.0f / 255.0f; // original pixel readers use 0x43f7dc
// the terrain colour blend (unit .rdata before the vtable: named constants)
const float kVolcanoColorScale = 0.65f;
const float kVolcanoColorBias = 0.35f;
// the sub-cell steps (unit .rdata after the vtable)
const float kVolcanoSixth = 1.0f / 6.0f;
const float kVolcanoThird = 1.0f / 3.0f;

// SYNTHETIC: EXE 0x4fe553
// SYNTHETIC: EXE 0x4fe55d
// SYNTHETIC: EXE 0x4fe56e
// SYNTHETIC: EXE 0x4fe57a
const UString EEE3DVolcano::sData("EEE3DVolcano-Data");

// FUNCTION: EXE 0x4fe596
// Creates a volcano effect over the 6x6 area around a point (its centre offset one tile in y), not yet grown
EEE3DVolcano::EEE3DVolcano(GE3DPoint inLocation, EEGraphicEffect* inGraphicEffect)
    : EEEffect3D(0, inGraphicEffect, kEffect3DVolcano)
{
    this->mArea = GERectangle(inLocation.GetX() - 3.0f, inLocation.GetY() + 3.0f, inLocation.GetX() + 3.0f,
                        inLocation.GetY() - 3.0f);
    this->mCenter.SetX(this->mArea.GetWidth() * 0.5f + this->mArea.GetLeft());
    this->mCenter.SetY(this->mArea.GetHeight() * 0.5f + this->mArea.GetTop() + 1.0f);
    this->mGrowth = 0.0f;
    this->mGrown = false;
    this->mDone = false;
    this->mTime = 0;
    this->mTimeLeft = 0;
}

// SYNTHETIC: EXE 0x4fe6d1
// (the scalar deleting destructor, emitted with the vtable)

// FUNCTION: EXE 0x4fe6ed
// Creates an empty volcano effect (for loading)
EEE3DVolcano::EEE3DVolcano()
    : EEEffect3D(kEffect3DVolcano)
{
}

// FUNCTION: EXE 0x4fe757
// Destroys the volcano, unloading its media and releasing its model (dereferenced without a null test)
EEE3DVolcano::~EEE3DVolcano()
{
    if (this->mGraphicObject)
        this->mGraphicObject->UnloadMedia();
    this->mModel->Dereference();
}

// FUNCTION: EXE 0x4fe79d
// Builds the volcano mesh over its area (each tile split 3x3, following the terrain, coloured by terrain and lighting
// and blended into textures\Volcano.tga near the centre) and the cone heights from the image's alpha
// NEAR: the locals' frame layout and statement order are not matched yet
long EEE3DVolcano::BuildModel()
{
    UString theFileName;
    std::vector<GEColor> theColors;
    std::vector<std::vector<GEColor> > theCellColors;
    std::vector<std::vector<float> > theCellHeights;
    GEColor theColor0(1.0f, 1.0f, 1.0f);
    GEColor theColor1(1.0f, 1.0f, 1.0f);
    GEColor theColor2(1.0f, 1.0f, 1.0f);
    GEColor theColor3(1.0f, 1.0f, 1.0f);
    GETransformation theTransformation;
    GE3DPoint theTop;
    GE3DPoint thePoint1;
    GE3DPoint thePoint2;
    GE3DPoint thePoint3;
    GE3DPoint thePoint4;
    GETextureCoordinate theCoordinate1;
    GETextureCoordinate theCoordinate2;
    GETextureCoordinate theCoordinate3;
    GETextureCoordinate theCoordinate4;
    EEMap* theMap = ::gWorld.mMap;
    FSFileSpec* theFile = NULL;
    GESurface* theSurface = NULL;
    float theMaximum = 1.1754944e-38f;
    GESurfaceDescription theDescription;
    float theHeightScale = this->mGraphicEffect->mSize;
    float theV = 0.0f;
    long theRight = UMFloor(this->mArea.GetRight());
    long theBottom = UMFloor(this->mArea.GetBottom());
    long theVertex = UMFloor(this->mArea.GetWidth());
    long theCells = UMFloor(this->mArea.GetWidth()) * theVertex;
    float theWidth = this->mArea.GetWidth();
    long theTiles = UMFloor(theWidth);
    long thePolygon = 0;
    long theSide;
    long thePolygons;
    long theVertices;
    long i;
    long j;
    theVertex = 0;
    theSide = theTiles * 3 + 1;
    thePolygons = theCells * 18;
    theVertices = theSide * theSide;
    this->mPoints.resize(theVertices, GE3DPoint());
    this->mHeights.resize(theVertices, 0.0f);
    theColors.resize(theVertices, GEColor());
    theCellColors.resize(4);
    for (unsigned long theCell = 0; theCell < 4; theCell++)
        theCellColors[theCell].resize(4, GEColor());
    theCellHeights.resize(4);
    for (unsigned long theRowCell = 0; theRowCell < 4; theRowCell++)
        theCellHeights[theRowCell].resize(4, 0.0f);
    theDescription.mWidth = 19;
    theDescription.mHeight = 19;
    theDescription.mIs3DRenderable = false;
    theDescription.mIsTransparencyEnabled = false;
    theDescription.mPixelFormat = kPixelFormatA8R8G8B8;
    theDescription.mSurfaceVolatility = 0;
    theDescription.m14 = 0;
    theDescription.mMipLevelCount = 0;
    theDescription.mType = kSurfaceSystemMemory;
    theDescription.mPixelDepth = 32;
    long theResult = gRasterizer->GetScreen(gScreenIndex)->CreateSurface(theDescription, &theSurface);
    ESVERIFY_AT(theResult == keseOK, 233);

    theFileName = UString("textures\\Volcano.tga");
    theResult = gTextureManager->GetTexturePath()->FindFile(theFileName, theFile);
    ESVERIFY_AT(theResult == keseOK, 238);
    ESVERIFY_AT(theFile != NULL, 239);
    theSurface->Load(*theFile, kImageFileTGA);
    ESVERIFY_AT(theResult == keseOK, 241);
    if (theFile != NULL)
        delete theFile;
    theSurface->Lock();
    unsigned long* thePixels = reinterpret_cast<unsigned long*>(theSurface->GetMemoryAddress());
    unsigned long theRowLength = theSurface->GetBytesPerRow() / 4;
    for (i = 0; i < 19; i++) {
        unsigned long* thePixel = thePixels + i;
        long theIndex = i;
        for (j = 19; j != 0; j--) {
            unsigned long theValue = *thePixel;
            float theBlue = (GEUnpackBlue(0, theValue) & 0xff) * kVolcanoPixelScale;
            float theGreen = (GEUnpackGreen(kPixelFormatA8R8G8B8, theValue) & 0xff) * kVolcanoPixelScale;
            float theRed = (GEUnpackRed(0, theValue) & 0xff) * kVolcanoPixelScale;
            GEColor& theColor = theColors[theIndex];
            theColor.mRed = theRed;
            theColor.mGreen = theGreen;
            theColor.mBlue = theBlue;
            this->mHeights[theIndex] = (GEUnpackAlpha(0, theValue) & 0xff) * theHeightScale * kVolcanoPixelScale;
            thePixel += theRowLength;
            theIndex += theSide;
        }
    }
    theSurface->Unlock();
    theSurface->Dereference();
    this->mModel = gModelManager->CreateModel(thePolygons, theVertices, 1, 0, 1, 0, "volcano model", 1);
    this->mModel->Reference();
    this->mModel->GetMaterial(0)->SetPolygonStartingIndex(0, 0);
    this->mModel->GetMaterial(0)->SetPolygonRangeLength(thePolygons, 0);
    this->mModel->GetMaterial(0)->SetVertexStartingIndex(0);
    this->mModel->GetMaterial(0)->SetVertexRangeLength(theVertices);
    theTop.SetX(this->mCenter.GetX());
    theTop.SetY(this->mArea.GetTop() + 1.0f);
    float theFade = 1.0f / this->mCenter.DistanceTo(theTop) * 4.0f;
    theTop.SetX(this->mCenter.GetX());
    theTop.SetY(this->mArea.GetTop() + 1.0f);
    float theFade2 = 1.0f / this->mCenter.DistanceTo(theTop) * 4.0f;
    long theY = UMFloor(this->mArea.GetTop());
    long theRowBase = 0;
    long theRowStep = theSide * 3;
    long theX;
    while (theBottom < theY) {
        theX = UMFloor(this->mArea.GetLeft());
        float theU = 0.0f;
        long theCellBase = 0;
        for (; theX < theRight; theX++) {
            theMap->GetTile(theX, theY);
            float theHeight00 = theMap->mTerrainMesh.GetZ(theX, theY);
            float theHeight01 = theMap->mTerrainMesh.GetZ(theX, theY + 1);
            float theHeight11 = theMap->mTerrainMesh.GetZ(theX + 1, theY + 1);
            float theHeight10 = theMap->mTerrainMesh.GetZ(theX + 1, theY);
            if (theMaximum <= theHeight00)
                theMaximum = theHeight00;
            if (theMaximum <= theHeight01)
                theMaximum = theHeight01;
            if (theMaximum <= theHeight11)
                theMaximum = theHeight11;
            if (theMaximum <= theHeight10)
                theMaximum = theHeight10;
            theColor0 = EETerrainTable::Get(theMap->mTerrainMesh.GetTextureSetID(theX, theY))->mColor;
            if (theMap->mTerrainMesh.GetIsValidTile(theX, theY + 1))
                theColor1 = EETerrainTable::Get(theMap->mTerrainMesh.GetTextureSetID(theX, theY + 1))->mColor;
            else
                theColor1 = theColor0;
            if (theMap->mTerrainMesh.GetIsValidTile(theX + 1, theY + 1))
                theColor2 = EETerrainTable::Get(theMap->mTerrainMesh.GetTextureSetID(theX + 1, theY + 1))->mColor;
            else
                theColor2 = theColor0;
            if (theMap->mTerrainMesh.GetIsValidTile(theX + 1, theY))
                theColor3 = EETerrainTable::Get(theMap->mTerrainMesh.GetTextureSetID(theX + 1, theY))->mColor;
            else
                theColor3 = theColor0;
            GEColor theLight2 = theMap->GetVertexColor(theX, theY, 2);
            GEColor theLight0 = theMap->GetVertexColor(theX, theY, 0);
            GEColor theLight1 = theMap->GetVertexColor(theX, theY, 1);
            GEColor theLight3 = theMap->GetVertexColor(theX, theY, 3);
            theColor0.mRed = theLight2.mRed * theColor0.mRed * kVolcanoColorScale + kVolcanoColorBias;
            theColor0.mGreen = theColor0.GetGreen() * theLight2.GetGreen() * kVolcanoColorScale + kVolcanoColorBias;
            theColor0.mBlue = theColor0.GetBlue() * theLight2.GetBlue() * kVolcanoColorScale + kVolcanoColorBias;
            theColor1.mRed = theLight0.mRed * theColor1.mRed * kVolcanoColorScale + kVolcanoColorBias;
            theColor1.mGreen = theColor1.GetGreen() * theLight0.GetGreen() * kVolcanoColorScale + kVolcanoColorBias;
            theColor1.mBlue = theColor1.GetBlue() * theLight0.GetBlue() * kVolcanoColorScale + kVolcanoColorBias;
            theColor2.mRed = theLight1.mRed * theColor2.mRed * kVolcanoColorScale + kVolcanoColorBias;
            theColor2.mGreen = theColor2.GetGreen() * theLight1.GetGreen() * kVolcanoColorScale + kVolcanoColorBias;
            theColor2.mBlue = theColor2.GetBlue() * theLight1.GetBlue() * kVolcanoColorScale + kVolcanoColorBias;
            theColor3.mRed = theLight3.mRed * theColor3.mRed * kVolcanoColorScale + kVolcanoColorBias;
            theColor3.mGreen = theColor3.GetGreen() * theLight3.GetGreen() * kVolcanoColorScale + kVolcanoColorBias;
            theColor3.mBlue = theColor3.GetBlue() * theLight3.GetBlue() * kVolcanoColorScale + kVolcanoColorBias;
            long theRow;
            long theColumn;
            for (theRow = 0; theRow <= 3; theRow++) {
                float theT = theRow * kVolcanoThird;
                float theS = 1.0f - theT;
                GEColor theLeft(theColor0.mRed * theT + theS * theColor1.mRed,
                                theColor1.GetGreen() * theS + theColor0.GetGreen() * theT,
                                theColor1.GetBlue() * theS + theColor0.GetBlue() * theT);
                GEColor theRightColor(theColor3.mRed * theT + theS * theColor2.mRed,
                                      theColor3.GetGreen() * theT + theColor2.GetGreen() * theS,
                                      theColor3.GetBlue() * theT + theColor2.GetBlue() * theS);
                float theLeftHeight = theS * theHeight01 + theT * theHeight00;
                float theRightHeight = theS * theHeight11 + theT * theHeight10;
                for (theColumn = 0; theColumn <= 3; theColumn++) {
                    float theRightWeight = theColumn * kVolcanoThird;
                    float theW = 1.0f - theRightWeight;
                    theCellColors[theColumn][theRow] =
                        GEColor(theRightColor.mRed * theRightWeight + theLeft.mRed * theW,
                                theRightColor.GetGreen() * theRightWeight + theLeft.GetGreen() * theW,
                                theRightColor.GetBlue() * theRightWeight + theLeft.GetBlue() * theW);
                    theCellHeights[theColumn][theRow] = theLeftHeight * theW + theRightWeight * theRightHeight;
                }
            }
            float theTileX = (float)theX;
            long theBase = theVertex;
            long theColorRow = theRowBase + theCellBase;
            float theTileY = (float)theY + 1.0f;
            for (theRow = 0; theRow < 3; theRow++) {
                long theNextRow = theRow + 1;
                float theRowT = theRow * kVolcanoThird;
                long theColorNext = theSide + theColorRow;
                long theColorIndex = theColorRow;
                long theNextPolygon = thePolygon + 1;
                float theNextY = theTileY - theNextRow * kVolcanoThird;
                float theNextV = theNextRow * kVolcanoSixth + theV;
                long theIndex = theBase;
                for (theColumn = 1; theColumn <= 3; theColumn++) {
                    theIndex++;
                    long thePrevious = theColumn - 1;
                    float theColumnT = thePrevious * kVolcanoThird;
                    thePoint1.SetX(theColumnT + theTileX);
                    thePoint1.SetY(theNextY);
                    thePoint1.SetZ(theCellHeights[thePrevious][theNextRow]);
                    thePoint2.SetX(theColumnT + theTileX);
                    thePoint2.SetY(theTileY - theRowT);
                    thePoint2.SetZ(theCellHeights[thePrevious][theRow]);
                    thePoint3.SetX(theColumn * kVolcanoThird + theTileX);
                    thePoint3.SetY(theTileY - theRowT);
                    thePoint3.SetZ(theCellHeights[theColumn][theRow]);
                    thePoint4.SetX(theColumn * kVolcanoThird + theTileX);
                    thePoint4.SetY(theNextY);
                    thePoint4.SetZ(theCellHeights[theColumn][theNextRow]);
                    this->mPoints[theSide - 1 + theIndex] = thePoint1;
                    this->mPoints[theBase] = thePoint2;
                    this->mPoints[theIndex] = thePoint3;
                    this->mPoints[theSide + theIndex] = thePoint4;
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint1, theSide - 1 + theIndex);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint2, theBase);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint3, theIndex);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint4, theSide + theIndex);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex1(thePolygon, theSide - 1 + theIndex);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex2(thePolygon, theBase);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex3(thePolygon, theSide + theIndex);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex1(theNextPolygon, theBase);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex2(theNextPolygon, theIndex);
                    this->mModel->GetPolygonArray(0)->SetVertexIndex3(theNextPolygon, theSide + theIndex);
                    theCoordinate1.SetU(theColumnT * 0.5f + theU);
                    theCoordinate1.SetV(theRowT * 0.5f + theV);
                    theCoordinate2.SetU(theColumn * kVolcanoSixth + theU);
                    theCoordinate2.SetV(theRowT * 0.5f + theV);
                    theCoordinate3.SetU(theColumnT * 0.5f + theU);
                    theCoordinate3.SetV(theNextV);
                    theCoordinate4.SetU(theColumn * kVolcanoSixth + theU);
                    theCoordinate4.SetV(theNextV);
                    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate1, theBase);
                    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate2, theIndex);
                    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate3, theSide - 1 + theIndex);
                    this->mModel->GetVertexArray(0)->SetTextureCoordinate(theCoordinate4, theSide + theIndex);
                    float theOpacity1 = 4.0f - this->mCenter.DistanceTo(thePoint1) * theFade;
                    if (theOpacity1 >= 1.0f)
                        theOpacity1 = 1.0f;
                    else if (theOpacity1 <= 0.0f)
                        theOpacity1 = 0.0f;
                    float theOpacity2 = 4.0f - this->mCenter.DistanceTo(thePoint2) * theFade;
                    if (theOpacity2 >= 1.0f)
                        theOpacity2 = 1.0f;
                    else if (theOpacity2 <= 0.0f)
                        theOpacity2 = 0.0f;
                    float theOpacity3 = 4.0f - this->mCenter.DistanceTo(thePoint3) * theFade;
                    if (theOpacity3 >= 1.0f)
                        theOpacity3 = 1.0f;
                    else if (theOpacity3 <= 0.0f)
                        theOpacity3 = 0.0f;
                    float theOpacity4 = 4.0f - this->mCenter.DistanceTo(thePoint4) * theFade;
                    if (theOpacity4 >= 1.0f)
                        theOpacity4 = 1.0f;
                    else if (theOpacity4 <= 0.0f)
                        theOpacity4 = 0.0f;
                    this->mModel->GetVertexArray(0)->SetOpacity(theOpacity1, theSide - 1 + theIndex);
                    this->mModel->GetVertexArray(0)->SetOpacity(theOpacity2, theBase);
                    this->mModel->GetVertexArray(0)->SetOpacity(theOpacity3, theIndex);
                    this->mModel->GetVertexArray(0)->SetOpacity(theOpacity4, theSide + theIndex);
                    float theBlend1;
                    float theBlend2;
                    float theBlend3;
                    float theBlend4;
                    this->mCenter.SetZ(thePoint1.GetZ());
                    theBlend1 = 4.0f - this->mCenter.DistanceTo(thePoint1) * theFade2 - 1.0f;
                    if (theBlend1 >= 1.0f)
                        theBlend1 = 1.0f;
                    else if (theBlend1 <= 0.0f)
                        theBlend1 = 0.0f;
                    this->mCenter.SetZ(thePoint2.GetZ());
                    theBlend2 = 4.0f - this->mCenter.DistanceTo(thePoint2) * theFade2 - 1.0f;
                    if (theBlend2 >= 1.0f)
                        theBlend2 = 1.0f;
                    else if (theBlend2 <= 0.0f)
                        theBlend2 = 0.0f;
                    this->mCenter.SetZ(thePoint3.GetZ());
                    theBlend3 = 4.0f - this->mCenter.DistanceTo(thePoint3) * theFade2 - 1.0f;
                    if (theBlend3 >= 1.0f)
                        theBlend3 = 1.0f;
                    else if (theBlend3 <= 0.0f)
                        theBlend3 = 0.0f;
                    this->mCenter.SetZ(thePoint4.GetZ());
                    theBlend4 = 4.0f - this->mCenter.DistanceTo(thePoint4) * theFade2 - 1.0f;
                    if (theBlend4 >= 1.0f)
                        theBlend4 = 1.0f;
                    else if (theBlend4 <= 0.0f)
                        theBlend4 = 0.0f;
                    float theRest = 1.0f - theBlend1;
                    theColor0.mRed = theBlend1 * theColors[theColorNext].mRed
                                     + theCellColors[thePrevious][theNextRow].mRed * theRest;
                    theColor0.mGreen = theBlend1 * theColors[theColorNext].GetGreen()
                                       + theCellColors[thePrevious][theNextRow].GetGreen() * theRest;
                    theColor0.mBlue = theBlend1 * theColors[theColorNext].GetBlue()
                                      + theCellColors[thePrevious][theNextRow].GetBlue() * theRest;
                    theRest = 1.0f - theBlend2;
                    theColor1.mRed = theBlend2 * theColors[theColorIndex].mRed
                                     + theCellColors[thePrevious][theRow].mRed * theRest;
                    theColor1.mGreen = theBlend2 * theColors[theColorIndex].GetGreen()
                                       + theCellColors[thePrevious][theRow].GetGreen() * theRest;
                    theColor1.mBlue = theBlend2 * theColors[theColorIndex].GetBlue()
                                      + theCellColors[thePrevious][theRow].GetBlue() * theRest;
                    theColorIndex++;
                    theRest = 1.0f - theBlend3;
                    theColor2.mRed = theBlend3 * theColors[theColorIndex].mRed
                                     + theCellColors[theColumn][theRow].mRed * theRest;
                    theColor2.mGreen = theBlend3 * theColors[theColorIndex].GetGreen()
                                       + theCellColors[theColumn][theRow].GetGreen() * theRest;
                    theColor2.mBlue = theBlend3 * theColors[theColorIndex].GetBlue()
                                      + theCellColors[theColumn][theRow].GetBlue() * theRest;
                    theColorNext++;
                    theRest = 1.0f - theBlend4;
                    theColor3.mRed = theBlend4 * theColors[theColorNext].mRed
                                     + theCellColors[theColumn][theNextRow].mRed * theRest;
                    theColor3.mGreen = theBlend4 * theColors[theColorNext].GetGreen()
                                       + theCellColors[theColumn][theNextRow].GetGreen() * theRest;
                    theColor3.mBlue = theBlend4 * theColors[theColorNext].GetBlue()
                                      + theCellColors[theColumn][theNextRow].GetBlue() * theRest;
                    this->mModel->GetVertexArray(0)->SetColor(theColor0, theSide - 1 + theIndex);
                    this->mModel->GetVertexArray(0)->SetColor(theColor1, theBase);
                    this->mModel->GetVertexArray(0)->SetColor(theColor2, theIndex);
                    this->mModel->GetVertexArray(0)->SetColor(theColor3, theSide + theIndex);
                    thePolygon += 2;
                    theNextPolygon += 2;
                    theBase++;
                }
                theColorRow += theSide;
                theBase = theBase - 3 + theSide;
            }
            theVertex += 3;
            theU += 0.5f;
            theCellBase += 3;
        }
        theV += 0.5f;
        theY--;
        theVertex += 1 + theSide * 2;
        theRowBase += theRowStep;
    }
    float theHalf = (theSide - 1.0f) * 0.5f;
    GE2DPoint theMiddle(theHalf, theHalf);
    GE2DPoint theCorner(theHalf, 0.0f);
    float theScale = 1.0f / theMiddle.DistanceToSquared(theCorner);
    for (i = 0; i < theSide; i++) {
        long theIndex = i;
        for (j = 0; j < theSide; j++) {
            GE2DPoint theGrid((float)j, (float)i);
            float theBlend = theMiddle.DistanceToSquared(theGrid) * theScale;
            if (theBlend > 1.0f)
                theBlend = 1.0f;
            this->mHeights[theIndex] = theBlend * this->mPoints[theIndex].GetZ() + (1.0f - theBlend) * (theMaximum + this->mHeights[theIndex]);
            theIndex += theSide;
        }
    }
    this->mModel->ComputeModelNormals();
    this->mModel->ComputeBoundingVolumes();
    this->mModel->SetGraphicsPostModelTransform(theTransformation);
    this->mModel->PrepareTransforms();
    theTransformation.SetPosition(this->mModel->GetCenterPoint().GetX(), this->mModel->GetCenterPoint().GetY(),
                                  this->mModel->GetCenterPoint().GetZ());
    this->SetTransformation(theTransformation);
    theTop.SetX(this->mModel->GetCenterPoint().GetX());
    theTop.SetY(this->mModel->GetCenterPoint().GetY());
    theTop.SetZ(theHeightScale + theMaximum);
    float theRadius = this->mModel->GetCenterPoint().DistanceTo(theTop);
    if (theRadius <= this->mModel->GetBoundingRadius(0))
        theRadius = this->mModel->GetBoundingRadius(0);
    this->mBoundingRadius = theRadius;
    if (theResult == kErrorOK)
        this->mModel->GetMaterial(0)->SetTexture(this->mGraphicObject->mTextures[0], gScreenIndex);
    return theResult;
}

// FUNCTION: EXE 0x50068a
// Morphs the mesh between the terrain and the cone by the growth fraction, jittering vertices near the centre every
// effect time 2 while rising, then queues the volcano in render queue 2
long EEE3DVolcano::Queue(EEEffectQueue* inQueues)
{
    static URandom sRandom;
    GE3DPoint thePoint;
    long theResult = EEEffect3D::Queue(inQueues);
    if (theResult == 0) {
        if (!this->mDone) {
            unsigned long i;
            if (this->mTime > this->mGraphicEffect->mTime2) {
                this->mTime = 0;
                thePoint.SetX(this->mCenter.mX);
                thePoint.SetY(this->mArea.GetTop() - 0.2f);
                thePoint.SetZ(this->mCenter.mZ);
                float theAmplitude = this->mGraphicEffect->mStartSize;
                float theScale = 1.0f / this->mCenter.DistanceToSquared(thePoint);
                for (i = 0; i < this->mModel->GetVertexArray(0)->GetSize(); i++) {
                    this->mModel->GetVertexArray(0)->GetPoint(thePoint, i);
                    this->mCenter.SetZ(this->mPoints[i].GetZ());
                    float theFactor = 1.0f - this->mCenter.DistanceToSquared(this->mPoints[i]) * theScale;
                    if (theFactor >= 1.0f)
                        theFactor = 1.0f;
                    else if (theFactor <= 0.0f)
                        theFactor = 0.0f;
                    float theX = this->mPoints[i].GetX();
                    thePoint.SetX((sRandom.GenerateUnit() * 2.0f - 1.0f) * (1.0f - this->mGrowth) * theFactor * theAmplitude
                                  + theX);
                    float theY = this->mPoints[i].GetY();
                    thePoint.SetY((sRandom.Generate() * 2.0f - 1.0f) * (1.0f - this->mGrowth) * theFactor * theAmplitude
                                  + theY);
                    float theZ = this->mPoints[i].GetZ();
                    thePoint.SetZ((1.0f - this->mGrowth) * theZ + this->mHeights[i] * this->mGrowth);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
                }
            } else {
                for (i = 0; i < this->mModel->GetVertexArray(0)->GetSize(); i++) {
                    this->mModel->GetVertexArray(0)->GetPoint(thePoint, i);
                    float theZ = this->mPoints[i].GetZ();
                    thePoint.SetZ((1.0f - this->mGrowth) * theZ + this->mGrowth * this->mHeights[i]);
                    this->mModel->GetVertexArray(0)->SetPoint(thePoint, i);
                }
            }
            this->mModel->ComputeBoundingVolumes();
            this->mModel->ComputeModelNormals();
            if (this->mGrowth == 1.0f)
                this->mDone = true;
        }
        GETexture* theTexture;
        theTexture = this->mGraphicEffect->mGraphicObject ? this->mGraphicEffect->mGraphicObject->mTextures[gScreenIndex] : 0;
        inQueues[2][theTexture].push_back(this);
    }
    return theResult;
}

// FUNCTION: EXE 0x500a24
// Destroys Queue's static random generator at exit (empty)

// FUNCTION: EXE 0x500a25
// Loads the volcano's media and builds its model; returns the first error
long EEE3DVolcano::SetUp()
{
    this->mGraphicObject = this->GetGraphicObject();
    long theResult = this->mGraphicObject->LoadMedia();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x500a47
// Draws the volcano mesh with vertex colours and z writes at depth range 0..1, and leaves the shared viewport at its
// normal depth range 0..1
long EEE3DVolcano::DrawPass4()
{
    unsigned long theScreen = gScreenIndex;
    if (this->mGraphicEffect->mGraphicObject)
        gRasterizer->SetAlphaTestingRefValue(this->mGraphicEffect->mGraphicObject->mAlphaReference, theScreen);
    else
        gRasterizer->SetAlphaTestingRefValue(0.0f, theScreen);
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    gRasterizer->SetBlending(kBlendSrcAlpha, kBlendInvSrcAlpha, theScreen);
    gRasterizer->SetShadingColorSource(1, theScreen);
    Fn10F833();
    bool theSorted = gOrthographicView;
    long theResult = gRasterizer->Draw(this->mModel, theScreen, 1.0f, theSorted);
    // Leave the shared viewport at its normal depth range 0..1 for the draws that follow.
    gU4E8F08.SetDepthRange(0.0f, 1.0f);
    gRasterizer->SetViewport(gU4E8F08, theScreen);
    gRasterizer->SetZBuffer(kZBufferTestAndWrite, theScreen);
    return theResult;
}

long EEE3DVolcano::DrawPass2()
{
    return 0;
}

long EEE3DVolcano::DrawPass5()
{
    return 0;
}

long EEE3DVolcano::DrawPass6()
{
    return 0;
}

// the empty Virtual2C: ICF-folded into 0xe123d
void EEE3DVolcano::PrepareQueue()
{
}

// FUNCTION: EXE 0x500b46
// Reads the volcano's area, centre, growth, time left, rise timer and grown flag (marking the mesh for reshaping);
// returns the first read error
long EEE3DVolcano::Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile)
{
    EEEffect3D::Load(inMajorVersion, inMinorVersion, inFile);
    this->mGraphicObject = this->GetGraphicObject();
    this->mGraphicObject->LoadMedia();
    this->mDone = false;
    long theResult = this->mArea.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mCenter.Load(inFile);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mGrowth);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTimeLeft);
    if (theResult != 0)
        return theResult;
    theResult = inFile.Read(this->mTime);
    if (theResult == 0)
        theResult = inFile.Read(this->mGrown);
    return theResult;
}

// FUNCTION: EXE 0x500be2
// After loading, repairs the base effect's pointers and rebuilds the volcano model
long EEE3DVolcano::RepairPointers()
{
    long theResult = EEEffect::RepairPointers();
    if (theResult == 0)
        theResult = this->BuildModel();
    return theResult;
}

// FUNCTION: EXE 0x500bf7
// Writes the volcano's area, centre, growth, time left, rise timer and grown flag; returns the first write error
long EEE3DVolcano::Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile)
{
    EEEffect3D::Save(inMajorVersion, inMinorVersion, outFile);
    long theResult = this->mArea.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = this->mCenter.Save(outFile);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mGrowth);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTimeLeft);
    if (theResult != 0)
        return theResult;
    theResult = outFile.Write(this->mTime);
    if (theResult == 0)
        theResult = outFile.Write(this->mGrown);
    return theResult;
}

// FUNCTION: EXE 0x500c7e
// Raises the volcano over effect time 1, holds it for the effect's duration, then lowers it again over time 1;
// returns 7 once its time runs out
long EEE3DVolcano::Step()
{
    long theResult = 0;
    long theElapsed = UMFloatToLong(gPhysics->GetTickTime());
    if (theElapsed <= 0)
        theElapsed = 1;
    if (!this->mGrown) {
        this->mGrowth += gPhysics->GetTickTime() / this->mGraphicEffect->mTime1;
        this->mTime += theElapsed;
        if (this->mGrowth >= 1.0f) {
            this->mGrowth = 1.0f;
            this->mTimeLeft = this->mGraphicEffect->mDuration + this->mGraphicEffect->mTime1;
            this->mGrown = true;
        }
    } else {
        this->mTimeLeft -= theElapsed;
        if (this->mTimeLeft < (long)this->mGraphicEffect->mTime1) {
            this->mDone = false;
            this->mGrowth = (float)this->mTimeLeft / this->mGraphicEffect->mTime1;
        }
        if (this->mTimeLeft < 0)
            theResult = 7;
    }
    return theResult;
}

// the template COMDATs of BuildModel's vectors (vector<GEColor>, vector<vector<GEColor> >, vector<vector<float> >)

// FUNCTION: EXE 0x500d4d
// Resizes the vector of colour rows (vector<vector<GEColor> >::resize(n))

// FUNCTION: EXE 0x500d73
// Resizes the vector of height rows (vector<vector<float> >::resize(n))

// FUNCTION: EXE 0x500d99
// Resizes a point vector, filling with a given point (vector<GE3DPoint>::resize)

// FUNCTION: EXE 0x500e4d
// Resizes the vector of colour rows, filling with a given row (vector<vector<GEColor> >::resize)

// FUNCTION: EXE 0x500f00
// Destroys and frees the colour rows (vector<vector<GEColor> >::_Tidy)

// FUNCTION: EXE 0x500f38
// Resizes the vector of height rows, filling with a given row (vector<vector<float> >::resize)

// FUNCTION: EXE 0x500feb
// Destroys and frees the height rows (vector<vector<float> >::_Tidy)

// FUNCTION: EXE 0x501023
// Inserts n copies of a point (vector<GE3DPoint>::insert)

// FUNCTION: EXE 0x5012d8
// Erases a range of 12-byte elements (bound to vector<EECPASGLandPathPoint>::erase, which shares this code)

// FUNCTION: EXE 0x501322
// Inserts n copies of a colour row (vector<vector<GEColor> >::insert)

// FUNCTION: EXE 0x5015d6
// Erases a range of colour rows (vector<vector<GEColor> >::erase)

// FUNCTION: EXE 0x50163f
// Inserts n copies of an inner vector (bound to vector<vector<long> >::insert, which shares this code)

// FUNCTION: EXE 0x5018f3
// Copy-constructs a colour vector (vector<GEColor> copy constructor)

// FUNCTION: EXE 0x50193d
// Copy-constructs n colour rows into raw storage (vector<vector<GEColor> >::_Ufill)

// FUNCTION: EXE 0x501968
// Copy-constructs n height rows into raw storage (vector<vector<float> >::_Ufill)

// FUNCTION: EXE 0x501993
// Copy-constructs a range of points into raw storage (vector<GE3DPoint>::_Ucopy)

// FUNCTION: EXE 0x5019c3
// Copy-constructs a range of colour rows into raw storage (vector<vector<GEColor> >::_Ucopy)

// FUNCTION: EXE 0x5019ef
// Assigns a colour row to every element of a range (std::fill)

// FUNCTION: EXE 0x501a0c
// Copies a range of colour rows backwards (std::copy_backward)

// FUNCTION: EXE 0x501a38
// Copies a range of colour rows (std::copy)

// FUNCTION: EXE 0x501a5c
// Copies a range of height rows (std::copy)

// FUNCTION: EXE 0x501a80
// Copy-constructs one colour row in place (std::_Construct)

// FUNCTION: EXE 0x501a92
// Destroys a colour vector and optionally frees it (vector<GEColor> scalar deleting destructor)

// FUNCTION: EXE 0x501aae
// Destroys a float vector and optionally frees it (vector<float> scalar deleting destructor)

// FUNCTION: EXE 0x501aca
// Assigns one colour vector to another (vector<GEColor>::operator=)
