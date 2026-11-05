// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer DrawTerrain.cpp (file name
// inferred: the fourth of the four DX7Rasterizer units in link order; it holds the terrain draws)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x10004d1a..0x100091aa: the U2D key init chains, Draw(GETerrainMesh), DrawGridLines, DrawTerrainGrid,
// DrawTerrainHighlight, DrawTerrainMaterial, DrawWater, Light and TransformToCamera (export order), the vector
// COMDATs they pull in, then the template-static init chain head and its atexit registrars. The out-of-line
// vector<T*>::operator[] COMDAT 0x10008be2 is not emitted: our candidates expand every operator[] (inline budget).
// Its only caller is DrawWater's last statement (mDX7ScreenStates[inScreen]->mViewportChanged = true, 0x10007226),
// where the original's budget is exhausted; GETerrainMesh::GetNumVisibleTileRows is likewise an inline the budget
// refuses (called through the import in six functions, expanded with size() out of line in Light).
#include <windows.h>
#include "gx/GX.h"

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"

// A material's visible-polygon lists end with their polygon count, stored in the last (pointer) slot
static inline unsigned long DX7ListCount(const std::vector<GETerrainMesh::GETMPolygon*>& inList)
{
    return (unsigned long)reinterpret_cast<uintptr_t>(inList.back());
}
static inline void DX7SetListCount(std::vector<GETerrainMesh::GETMPolygon*>& ioList, unsigned long inCount)
{
    ioList.back() = reinterpret_cast<GETerrainMesh::GETMPolygon*>((uintptr_t)inCount);
}

// The D3D light indices in use (DX7Rasterizer.cpp).
extern unsigned long gLightsInUse[8];

// One directional light as Light gathers it from the device: its colour and its direction pointing at the light.
// sizeof 0x18 (Light's vector arithmetic) (provisional names).
struct DX7RDirectionalLight {
    GEColor mColor;                         // +0x00
    GEVector mDirection;                    // +0x0c
};

// The U2D key init chains:
// SYNTHETIC: DX7HR 0x10004d1a
// SYNTHETIC: DX7HR 0x10004d24
// SYNTHETIC: DX7HR 0x10004d35
// SYNTHETIC: DX7HR 0x10004d41
// SYNTHETIC: DX7HR 0x10004d4c
// SYNTHETIC: DX7HR 0x10004d56
// SYNTHETIC: DX7HR 0x10004d67
// SYNTHETIC: DX7HR 0x10004d73
// SYNTHETIC: DX7HR 0x10004d7e
// SYNTHETIC: DX7HR 0x10004d88
// SYNTHETIC: DX7HR 0x10004d99
// SYNTHETIC: DX7HR 0x10004da5
// SYNTHETIC: DX7HR 0x10004db0
// SYNTHETIC: DX7HR 0x10004dba
// SYNTHETIC: DX7HR 0x10004dcb
// SYNTHETIC: DX7HR 0x10004dd7
// SYNTHETIC: DX7HR 0x10004de2
// SYNTHETIC: DX7HR 0x10004dec
// SYNTHETIC: DX7HR 0x10004dfd
// SYNTHETIC: DX7HR 0x10004e09
// SYNTHETIC: DX7HR 0x10004e14
// SYNTHETIC: DX7HR 0x10004e1e
// SYNTHETIC: DX7HR 0x10004e2f
// SYNTHETIC: DX7HR 0x10004e3b
// SYNTHETIC: DX7HR 0x10004e46
// SYNTHETIC: DX7HR 0x10004e50
// SYNTHETIC: DX7HR 0x10004e61
// SYNTHETIC: DX7HR 0x10004e6d

// FUNCTION: DX7HR 0x10004e78
// Draws the visible terrain texture by texture, unclipped then clipped polygons (space maps draw materials 1..8 first);
// with no visible tile row fills the view with two black quads instead
long DX7Rasterizer::Draw(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    GETransformation theTransformation;
    GXMATRIX theMatrix;
    std::map<GETexture*, unsigned long>::iterator theIterator;
    unsigned long theColor = GXRGBA(UMFloatToLong(0.0f), UMFloatToLong(0.0f), UMFloatToLong(0.0f),
                                       UMFloatToLong(255.0f));
    long theResult;
    GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_WORLD, &theMatrix);
    theTransformation.SetElement(1, 1, 0.0f);
    theTransformation.SetElement(1, 2, 1.0f);
    theTransformation.SetElement(2, 1, 1.0f);
    theTransformation.SetElement(2, 2, 0.0f);
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    theResult = this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_VIEW, &theMatrix);
    if (!inOrthographic)
        this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
    else
        this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_PROJECTION, &theMatrix);
    if (inMesh.GetNumVisibleTileRows() != 0) {
        unsigned long theIndex;
        if (inMesh.GetIsSpaceMap()) {
            for (theIterator = inMesh.GetTerrainMaterialMap().begin();
                 theIterator != inMesh.GetTerrainMaterialMap().end(); theIterator++) {
                theIndex = theIterator->second;
                if (theIndex < 9 && theIndex > 0) {
                    this->mScreenStates[inScreen]->m32C[1] +=
                        DX7ListCount(inMesh.GetTerrainMaterialList()[theIndex].mIndices);
                    this->mScreenStates[inScreen]->m32C[1] +=
                        DX7ListCount(inMesh.GetTerrainMaterialList()[theIndex].mStripIndices);
                    this->SetTexture(theIterator->first, inScreen);
                    this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, FALSE);
                    this->DrawTerrainMaterial(&theViewport, inMesh.GetTerrainMaterialList()[theIndex].mIndices, inScreen);
                    this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, TRUE);
                    theResult = this->DrawTerrainMaterial(&theViewport,
                                                    inMesh.GetTerrainMaterialList()[theIndex].mStripIndices,
                                                    inScreen);
                }
            }
        }
        for (theIterator = inMesh.GetTerrainMaterialMap().begin(); theIterator != inMesh.GetTerrainMaterialMap().end();
             theIterator++) {
            theIndex = (*theIterator).second;
            if (!inMesh.GetIsSpaceMap() || theIndex >= 9 || theIndex <= 0) {
                this->mScreenStates[inScreen]->m32C[1] +=
                    DX7ListCount(inMesh.GetTerrainMaterialList()[theIndex].mIndices);
                this->mScreenStates[inScreen]->m32C[1] +=
                    DX7ListCount(inMesh.GetTerrainMaterialList()[theIndex].mStripIndices);
                this->SetTexture((*theIterator).first, inScreen);
                this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, FALSE);
                this->DrawTerrainMaterial(&theViewport, inMesh.GetTerrainMaterialList()[theIndex].mIndices, inScreen);
                this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, TRUE);
                theResult = this->DrawTerrainMaterial(&theViewport, inMesh.GetTerrainMaterialList()[theIndex].mStripIndices,
                                                inScreen);
            }
        }
        this->mDX7ScreenStates[inScreen]->mViewportChanged = true;
    } else {
        // No terrain row is visible: fill the view with two black quads in front of the camera.
        GE3DPoint theWorldPoints[6];
        GE3DPoint theCameraPoints[6];
        GEVector theForward;
        GEVector theRight;
        GEVector theOffset;
        float theX;
        float theY;
        float theCenterX;
        float theCenterY;
        float theCenterZ;
        GXVERTEX_STD_COLOR* theBuffer;
        theViewport.GetCamera().Apply(GEVector(0.0f, 1.0f, 0.0f), theForward);
        theForward.SetZ(0.0f);
        theForward.Normalize();
        theViewport.GetCamera().Apply(GEVector(1.0f, 0.0f, 0.0f), theRight);
        theRight.SetZ(0.0f);
        theRight.Normalize();
        theX = theViewport.GetCamera().GetX();
        theY = theViewport.GetCamera().GetY();
        theRight.Scale(-1000.0f, theOffset);
        theWorldPoints[0].SetX(theOffset.GetX() + theX);
        theWorldPoints[0].SetY(theOffset.GetY() + theY);
        theWorldPoints[0].SetZ(theOffset.GetZ());
        theRight.Scale(1000.0f, theOffset);
        theWorldPoints[1].SetX(theOffset.GetX() + theX);
        theWorldPoints[1].SetY(theOffset.GetY() + theY);
        theWorldPoints[1].SetZ(theOffset.GetZ());
        theForward.Scale(7.071067f, theOffset);
        theCenterX = theOffset.GetX() + theX;
        theCenterY = theOffset.GetY() + theY;
        theCenterZ = theOffset.GetZ();
        theRight.Scale(-1000.0f, theOffset);
        theWorldPoints[2].SetX(theOffset.GetX() + theCenterX);
        theWorldPoints[2].SetY(theOffset.GetY() + theCenterY);
        theWorldPoints[2].SetZ(theOffset.GetZ() + theCenterZ);
        theRight.Scale(1000.0f, theOffset);
        theWorldPoints[3].SetX(theOffset.GetX() + theCenterX);
        theWorldPoints[3].SetY(theOffset.GetY() + theCenterY);
        theWorldPoints[3].SetZ(theOffset.GetZ() + theCenterZ);
        theForward.Scale((float)sqrt(theViewport.GetRearClippingDistance() * theViewport.GetRearClippingDistance() +
                                     theViewport.GetRearClippingDistance() * theViewport.GetRearClippingDistance()),
                         theOffset);
        theCenterX = theOffset.GetX() + theX;
        theCenterY = theOffset.GetY() + theY;
        theCenterZ = theOffset.GetZ();
        theRight.Scale(-1000.0f, theOffset);
        theWorldPoints[4].SetX(theOffset.GetX() + theCenterX);
        theWorldPoints[4].SetY(theOffset.GetY() + theCenterY);
        theWorldPoints[4].SetZ(theOffset.GetZ() + theCenterZ);
        theRight.Scale(1000.0f, theOffset);
        theWorldPoints[5].SetX(theOffset.GetX() + theCenterX);
        theWorldPoints[5].SetY(theOffset.GetY() + theCenterY);
        theWorldPoints[5].SetZ(theOffset.GetZ() + theCenterZ);
        theViewport.GetCameraInverse().Apply(theWorldPoints[0], theCameraPoints[0]);
        theViewport.GetCameraInverse().Apply(theWorldPoints[1], theCameraPoints[1]);
        theViewport.GetCameraInverse().Apply(theWorldPoints[2], theCameraPoints[2]);
        theViewport.GetCameraInverse().Apply(theWorldPoints[3], theCameraPoints[3]);
        theViewport.GetCameraInverse().Apply(theWorldPoints[4], theCameraPoints[4]);
        theViewport.GetCameraInverse().Apply(theWorldPoints[5], theCameraPoints[5]);
        this->mScreenStates[inScreen]->m32C[1] += 4;
        this->SetTexture(NULL, inScreen);
        this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, TRUE);
        this->LockVB(12, inScreen, theBuffer);
        theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
#define DX7R_BLACK_VERTEX(n)                            \
        theBuffer->x = theCameraPoints[n].GetX();       \
        theBuffer->y = theCameraPoints[n].GetY();       \
        theBuffer->z = theCameraPoints[n].GetZ();       \
        theBuffer->nx = 0.0f;                           \
        theBuffer->ny = 0.0f;                           \
        theBuffer->nz = 0.0f;                           \
        theBuffer->color = theColor;                    \
        theBuffer->tu = 0.0f;                           \
        theBuffer->tv = 0.0f;                           \
        theBuffer++;
        DX7R_BLACK_VERTEX(2)
        DX7R_BLACK_VERTEX(3)
        DX7R_BLACK_VERTEX(0)
        DX7R_BLACK_VERTEX(3)
        DX7R_BLACK_VERTEX(1)
        DX7R_BLACK_VERTEX(0)
        DX7R_BLACK_VERTEX(4)
        DX7R_BLACK_VERTEX(5)
        DX7R_BLACK_VERTEX(2)
        DX7R_BLACK_VERTEX(5)
        DX7R_BLACK_VERTEX(3)
        DX7R_BLACK_VERTEX(2)
#undef DX7R_BLACK_VERTEX
        this->UnlockVB(inScreen);
        while (true) {
            if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_TRIANGLELIST,
                    this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition, 12,
                    0) != GXERR_WASSTILLDRAWING)
                break;
            PlatSleep(0);
        }
        this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += 12;
        theResult = kErrorOK;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x10005b93
// Draws grid lines between tile corners whose both ends are visible, as one red line list of at most 1200 lines
long DX7Rasterizer::DrawGridLines(GETerrainMesh& inMesh,
                                  std::vector<std::pair<U2DPoint<long>, U2DPoint<long> > >& inLines,
                                  GETexture* inTexture, float inWidth, unsigned long inScreen, bool inOrthographic)
{
    long theResult;
    GE3DPoint theWorldPoint;
    GE3DPoint theCameraPoint;
    GETransformation theTransformation;
    GXMATRIX theMatrix;
    GXMATERIAL theMaterial;
    GXVERTEX_STD_COLOR* theBuffer;
    theResult = kErrorOK;
    unsigned long theLineCount = inLines.size();
    unsigned long theVertexCount = inLines.size() * 2;
    float theEndV = inWidth + 1.0f;
    unsigned long theDrawnLines = 0;
    if (inMesh.GetNumVisibleTileRows() == 0)
        return theResult;
    GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_WORLD, &theMatrix);
    theTransformation.SetElement(1, 1, 0.0f);
    theTransformation.SetElement(1, 2, 1.0f);
    theTransformation.SetElement(2, 1, 1.0f);
    theTransformation.SetElement(2, 2, 0.0f);
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_VIEW, &theMatrix);
    if (!inOrthographic)
        this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
    else
        this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_PROJECTION, &theMatrix);
    this->SetTexture(inTexture, inScreen);
    theMaterial.ambient.r = 1.0f;
    theMaterial.ambient.g = 1.0f;
    theMaterial.ambient.b = 1.0f;
    theMaterial.ambient.a = 1.0f;
    theMaterial.diffuse.r = 1.0f;
    theMaterial.diffuse.g = 1.0f;
    theMaterial.diffuse.b = 1.0f;
    theMaterial.diffuse.a = 1.0f;
    theMaterial.emissive.r = 0.0f;
    theMaterial.emissive.g = 0.0f;
    theMaterial.emissive.b = 0.0f;
    theMaterial.emissive.a = 0.0f;
    theMaterial.specular.r = 0.0f;
    theMaterial.specular.g = 0.0f;
    theMaterial.specular.b = 0.0f;
    theMaterial.specular.a = 0.0f;
    theMaterial.power = 0.0f;
    this->mDX7ScreenStates[inScreen]->mDevice->SetMaterial(&theMaterial);
    this->SetShadingColorSource(0, inScreen);
    unsigned long theStartX;
    unsigned long theStartY;
    unsigned long theWidth;
    unsigned long theHeight;
    inMesh.GetTerrainVisibleBottomLeftIndices(theStartX, theStartY);
    inMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
    unsigned long theEndX = theWidth + theStartX + 1;
    unsigned long theEndY = theHeight + theStartY + 1;
    if (theVertexCount > 0x960) {
        theVertexCount = 0x960;
        theLineCount = 0x4b0;
    }
    this->LockVB(theVertexCount, inScreen, theBuffer);
    theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
    for (unsigned long i = 0; i < theLineCount; i++) {
        unsigned long theX1 = inLines[i].first.GetX();
        unsigned long theY1 = inLines[i].first.GetY();
        unsigned long theX2 = inLines[i].second.GetX();
        unsigned long theY2 = inLines[i].second.GetY();
        if (theX1 < theStartX || theX1 >= theEndX || theX2 < theStartX || theX2 >= theEndX ||
            theY1 < theStartY || theY1 >= theEndY || theY2 < theStartY || theY2 >= theEndY ||
            !inMesh.GetTerrainPointVisibilityArray()[theX1 - theStartX][theY1 - theStartY] ||
            !inMesh.GetTerrainPointVisibilityArray()[theX2 - theStartX][theY2 - theStartY])
            continue;
        theWorldPoint.SetX((float)theX1);
        theWorldPoint.SetY((float)theY1);
        theWorldPoint.SetZ(inMesh.GetZ(theX1, theY1) >= 0.0f ? inMesh.GetZ(theX1, theY1) : 0.0f);
        theViewport.GetCameraInverse().Apply(theWorldPoint, theCameraPoint);
        theBuffer->x = theCameraPoint.GetX();
        theBuffer->y = theCameraPoint.GetY();
        theBuffer->z = theCameraPoint.GetZ();
        theBuffer->nx = 0.0f;
        theBuffer->ny = 0.0f;
        theBuffer->nz = 0.0f;
        theBuffer->color = GXRGBA(255, 0, 0, 255);
        theBuffer->tu = 0.0f;
        theBuffer->tv = inWidth;
        theBuffer++;
        theWorldPoint.SetX((float)theX2);
        theWorldPoint.SetY((float)theY2);
        theWorldPoint.SetZ(inMesh.GetZ(theX2, theY2) >= 0.0f ? inMesh.GetZ(theX2, theY2) : 0.0f);
        theViewport.GetCameraInverse().Apply(theWorldPoint, theCameraPoint);
        theBuffer->x = theCameraPoint.GetX();
        theBuffer->y = theCameraPoint.GetY();
        theBuffer->z = theCameraPoint.GetZ();
        theBuffer->nx = 0.0f;
        theBuffer->ny = 0.0f;
        theBuffer->nz = 0.0f;
        theBuffer->color = GXRGBA(255, 0, 0, 255);
        theBuffer->tu = 0.0f;
        theBuffer->tv = theEndV;
        theBuffer++;
        theDrawnLines++;
    }
    theResult = this->UnlockVB(inScreen);
    if (theDrawnLines > 0) {
        while (true) {
            if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_LINELIST,
                    this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition,
                    theDrawnLines * 2, 0) != GXERR_WASSTILLDRAWING)
                break;
            PlatSleep(0);
        }
    }
    this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += theVertexCount;
    return theResult;
}

// FUNCTION: DX7HR 0x10006159
// Draws two red edge lines for every visible tile, one draw call per tile
long DX7Rasterizer::DrawTerrainGrid(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    GETransformation theTransformation;
    GXMATRIX theMatrix;
    GXVERTEX_STD_COLOR* theBuffer;
    long theResult = kErrorOK;
    if (inMesh.GetNumVisibleTileRows() != 0) {
        GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_WORLD, &theMatrix);
        theTransformation.SetElement(1, 1, 0.0f);
        theTransformation.SetElement(1, 2, 1.0f);
        theTransformation.SetElement(2, 1, 1.0f);
        theTransformation.SetElement(2, 2, 0.0f);
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        theResult = this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_VIEW, &theMatrix);
        if (!inOrthographic)
            this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
        else
            this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_PROJECTION, &theMatrix);
        unsigned long theStartX;
        unsigned long theStartY;
        unsigned long theWidth;
        unsigned long theHeight;
        inMesh.GetTerrainVisibleBottomLeftIndices(theStartX, theStartY);
        inMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
        unsigned long theEndX = theWidth + theStartX + 1;
        unsigned long theEndY = theHeight + theStartY + 1;
        for (unsigned long y = theStartY; y < theEndY; y++) {
            for (unsigned long x = theStartX; x < theEndX; x++) {
                if (inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY] &&
                    inMesh.GetTerrainPointVisibilityArray()[x + 1 - theStartX][y - theStartY] &&
                    inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y + 1 - theStartY]) {
                    this->LockVB(4, inScreen, theBuffer);
                    theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
#define DX7R_GRID_VERTEX(dx, dy)                                                                                    \
                    theBuffer->x =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[x + dx - theStartX][y + dy - theStartY].mPoint.GetX(); \
                    theBuffer->y =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[x + dx - theStartX][y + dy - theStartY].mPoint.GetY(); \
                    theBuffer->z =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[x + dx - theStartX][y + dy - theStartY].mPoint.GetZ(); \
                    theBuffer->nx = 0.0f;                                                                           \
                    theBuffer->ny = 0.0f;                                                                           \
                    theBuffer->nz = 0.0f;                                                                           \
                    theBuffer->color = GXRGBA(255, 0, 0, 255);                                                   \
                    theBuffer->tu = 0.0f;                                                                           \
                    theBuffer->tv = 0.0f;                                                                           \
                    theBuffer++;
                    DX7R_GRID_VERTEX(0, 0)
                    DX7R_GRID_VERTEX(0, 1)
                    DX7R_GRID_VERTEX(0, 0)
                    DX7R_GRID_VERTEX(1, 0)
#undef DX7R_GRID_VERTEX
                    theResult = this->UnlockVB(inScreen);
                    while (true) {
                        if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_LINELIST,
                                this->mDX7ScreenStates[inScreen]->mVertexBuffer,
                                this->mDX7ScreenStates[inScreen]->mVertexBufferPosition, 4, 0) != GXERR_WASSTILLDRAWING)
                            break;
                        PlatSleep(0);
                    }
                    this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += 4;
                }
            }
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x100065d1
// Shades a tile rectangle with a translucent colour over every valid visible tile, drawn in batches of 400 quads
long DX7Rasterizer::DrawTerrainHighlight(GETerrainMesh& inMesh, const U2DRectangle<long>& inRectangle,
                                         unsigned long inScreen, bool inOrthographic, const GEColor& inColor,
                                         float inOpacity)
{
    long theRemaining;
    GETransformation theTransformation;
    GXMATRIX theMatrix;
    GXVERTEX_STD_COLOR* theBuffer;
    unsigned long theRight = inRectangle.mRight;
    unsigned long theLeft = inRectangle.mLeft;
    unsigned long theBottom = inRectangle.mBottom;
    long theResult = kErrorOK;
    unsigned long theTop = inRectangle.mTop;
    long theQuadCount = 0;
    long theWidth = theRight - theLeft;
    if (theWidth < 0)
        theWidth = -theWidth;
    long theHeight = theBottom - theTop;
    if (theHeight < 0)
        theHeight = -theHeight;
    theRemaining = (theHeight + 1) * (theWidth + 1);
    if (inMesh.GetNumVisibleTileRows() != 0) {
        unsigned long theColor = GXRGBA(UMFloatToLong(inColor.GetRed() * 255.0f),
                                           UMFloatToLong(inColor.GetGreen() * 255.0f),
                                           UMFloatToLong(inColor.GetBlue() * 255.0f),
                                           UMFloatToLong(inOpacity * 255.0f));
        GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
        this->SetTexture(NULL, inScreen);
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_WORLD, &theMatrix);
        theTransformation.SetElement(1, 1, 0.0f);
        theTransformation.SetElement(1, 2, 1.0f);
        theTransformation.SetElement(2, 1, 1.0f);
        theTransformation.SetElement(2, 2, 0.0f);
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_VIEW, &theMatrix);
        if (!inOrthographic)
            this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
        else
            this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_PROJECTION, &theMatrix);
        unsigned long theStartX;
        unsigned long theStartY;
        unsigned long theVisibleWidth;
        unsigned long theVisibleHeight;
        inMesh.GetTerrainVisibleBottomLeftIndices(theStartX, theStartY);
        inMesh.GetTerrainVisibleTileDimensions(theVisibleWidth, theVisibleHeight);
        unsigned long theEndX = theVisibleWidth + theStartX + 1;
        unsigned long theEndY = theVisibleHeight + theStartY + 1;
        this->LockVB(min(400, theRemaining) * 6, inScreen, theBuffer);
        theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
        for (unsigned long y = theTop; y <= theBottom; y++) {
            unsigned long theRow = y - theStartY;
            for (unsigned long x = theLeft; x <= theRight; x++) {
                unsigned long theColumn = x - theStartX;
                theRemaining--;
                if (inMesh.GetIsValidTile(x, y) && x >= theStartX && x <= theEndX && y >= theStartY &&
                    y <= theEndY &&
                    inMesh.GetTerrainPointVisibilityArray()[theColumn][theRow] &&
                    inMesh.GetTerrainPointVisibilityArray()[theColumn + 1][theRow] &&
                    inMesh.GetTerrainPointVisibilityArray()[theColumn][theRow + 1] &&
                    inMesh.GetTerrainPointVisibilityArray()[theColumn + 1][theRow + 1]) {
#define DX7R_HIGHLIGHT_VERTEX(dx, dy)                                                                               \
                    theBuffer->x =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[theColumn + dx][theRow + dy].mPoint.GetX(); \
                    theBuffer->y =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[theColumn + dx][theRow + dy].mPoint.GetY(); \
                    theBuffer->z =                                                                                  \
                        inMesh.GetTerrainRenderVertexArray()[theColumn + dx][theRow + dy].mPoint.GetZ(); \
                    theBuffer->nx = 0.0f;                                                                           \
                    theBuffer->ny = 0.0f;                                                                           \
                    theBuffer->nz = 0.0f;                                                                           \
                    theBuffer->color = theColor;                                                                    \
                    theBuffer->tu = 0.0f;                                                                           \
                    theBuffer->tv = 0.0f;                                                                           \
                    theBuffer++;
                    DX7R_HIGHLIGHT_VERTEX(0, 0)
                    DX7R_HIGHLIGHT_VERTEX(0, 1)
                    DX7R_HIGHLIGHT_VERTEX(1, 0)
                    DX7R_HIGHLIGHT_VERTEX(0, 1)
                    DX7R_HIGHLIGHT_VERTEX(1, 1)
                    DX7R_HIGHLIGHT_VERTEX(1, 0)
#undef DX7R_HIGHLIGHT_VERTEX
                    theQuadCount++;
                    if (theQuadCount == 400 && theRemaining > 0) {
                        this->UnlockVB(inScreen);
                        while (true) {
                            if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_TRIANGLELIST,
                                    this->mDX7ScreenStates[inScreen]->mVertexBuffer,
                                    this->mDX7ScreenStates[inScreen]->mVertexBufferPosition, 0x960, 0) !=
                                GXERR_WASSTILLDRAWING)
                                break;
                            PlatSleep(0);
                        }
                        theQuadCount = 0;
                        this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += 0x960;
                        this->LockVB(min(400, theRemaining) * 6, inScreen, theBuffer);
                        theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
                    }
                }
            }
        }
        theResult = this->UnlockVB(inScreen);
        if (theQuadCount > 0) {
            while (true) {
                if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_TRIANGLELIST,
                        this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition,
                        theQuadCount * 6, 0) != GXERR_WASSTILLDRAWING)
                    break;
                PlatSleep(0);
            }
            this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += theQuadCount * 6;
        }
    }
    return theResult;
}

// FUNCTION: DX7HR 0x10006c53
// Draws a material's NULL-terminated polygon list (count in its last element) in batches, with fog-darkened vertex
// colours; skips triangles whose vertices are all fully visible with no alpha
long DX7Rasterizer::DrawTerrainMaterial(GEViewport* inViewport, std::vector<GETerrainMesh::GETMPolygon*>& inPolygons,
                                        unsigned long inScreen)
{
    long theResult = kErrorOK;
    GEColor theColors[3];
    GETerrainMesh::GETMRenderVertex* theVertices[3];
    float theVisibilities[3];
    float theAlphas[3];
    std::vector<GETerrainMesh::GETMPolygon*>::iterator thePolygon = inPolygons.begin();
    unsigned long theRemaining = DX7ListCount(inPolygons) * 3;
    while (theRemaining > 0) {
        unsigned long theBatch = theRemaining;
        if (theBatch >= 0x960)
            theBatch = 0x960;
        theRemaining -= theBatch;
        unsigned long theVertexCount = 0;
        GXVERTEX_STD_COLOR* theBuffer;
        this->LockVB(theBatch, inScreen, theBuffer);
        theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
        while (*thePolygon != NULL && theBatch > 0) {
            GETerrainMesh::GETMPolygon* theTriangle = *thePolygon;
            bool theIsVisible = false;
            int i;
            for (i = 0; i < 3; i++) {
                theVertices[i] = theTriangle->mVertices[i];
                if (theVertices[i]->mVisibility >= 0.0f)
                    theVisibilities[i] = theVertices[i]->mVisibility;
                else
                    theVisibilities[i] = *theVertices[i]->mLinkedVisibility;
                if (theTriangle->mAlpha[i] >= 0.0f)
                    theAlphas[i] = theTriangle->mAlpha[i];
                else
                    theAlphas[i] = 1.0f - theVisibilities[i];
                if (theVisibilities[i] < 1.0f || theAlphas[i] > 0.0f)
                    theIsVisible = true;
            }
            if (theIsVisible) {
                for (i = 0; i < 3; i++) {
                    GETerrainMesh::GETMRenderVertex* theVertex = theVertices[i];
                    theBuffer->x = theVertex->mPoint.GetX();
                    theBuffer->y = theVertex->mPoint.GetY();
                    theBuffer->z = theVertex->mPoint.GetZ();
                    theBuffer->nx = 0.0f;
                    theBuffer->ny = 0.0f;
                    theBuffer->nz = 0.0f;
                    if (theTriangle->mAlpha[i] >= 0.0f)
                        theBuffer->color =
                            GXRGBA(UMFloatToLong(theVertex->mColor.GetRed() * theVisibilities[i] * 255.0f),
                                      UMFloatToLong(theVertex->mColor.GetGreen() * theVisibilities[i] * 255.0f),
                                      UMFloatToLong(theVertex->mColor.GetBlue() * theVisibilities[i] * 255.0f),
                                      UMFloatToLong(theAlphas[i] * 255.0f));
                    else
                        theBuffer->color = GXRGBA(UMFloatToLong(0.0f), UMFloatToLong(0.0f), UMFloatToLong(0.0f),
                                                     UMFloatToLong(theAlphas[i] * 255.0f));
                    theBuffer->tu = theTriangle->mUV[i].GetU();
                    theBuffer->tv = theTriangle->mUV[i].GetV();
                    theBuffer++;
                }
                theVertexCount += 3;
            }
            thePolygon++;
            theBatch -= 3;
        }
        theResult = this->UnlockVB(inScreen);
        if (theVertexCount > 0) {
            while (true) {
                if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(GXPT_TRIANGLELIST,
                        this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition,
                        theVertexCount, 0) != GXERR_WASSTILLDRAWING)
                    break;
                PlatSleep(0);
            }
        }
        this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += theVertexCount;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x10006f40
// Draws every water material, unclipped then clipped polygons, and counts its triangles
long DX7Rasterizer::DrawWater(GETerrainMesh& inMesh, unsigned long inScreen, bool inOrthographic)
{
    GETransformation theTransformation;
    GXMATRIX theMatrix;
    long theResult = kErrorOK;
    std::vector<GETerrainMesh::GETMMaterial>& theMaterials = inMesh.GetWaterMaterialList();
    if (inMesh.GetNumVisibleTileRows() != 0) {
        GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_WORLD, &theMatrix);
        theTransformation.SetElement(1, 1, 0.0f);
        theTransformation.SetElement(1, 2, 1.0f);
        theTransformation.SetElement(2, 1, 1.0f);
        theTransformation.SetElement(2, 2, 0.0f);
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        theResult = this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_VIEW, &theMatrix);
        if (!inOrthographic)
            this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
        else
            this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(GXTS_PROJECTION, &theMatrix);
        unsigned long theCount = theMaterials.size();
        for (unsigned long i = 0; i < theCount; i++) {
            this->mScreenStates[inScreen]->m32C[1] += DX7ListCount(theMaterials[i].mIndices);
            this->mScreenStates[inScreen]->m32C[1] += DX7ListCount(theMaterials[i].mStripIndices);
            this->SetTexture(theMaterials[i].mTexture, inScreen);
            this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, FALSE);
            theResult = this->DrawTerrainMaterial(&theViewport, theMaterials[i].mIndices, inScreen);
            this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(GXRS_CLIPPING, TRUE);
            theResult = this->DrawTerrainMaterial(&theViewport, theMaterials[i].mStripIndices, inScreen);
        }
        this->mDX7ScreenStates[inScreen]->mViewportChanged = true;
    }
    return theResult;
}

// FUNCTION: DX7HR 0x1000723b
// Lights the visible terrain and water vertices: ambient plus each active directional light, normalised, times
// the vertex colour
void DX7Rasterizer::Light(GETerrainMesh& inMesh, unsigned long inScreen)
{
    std::vector<DX7RDirectionalLight> theLights;
    DX7RDirectionalLight theDirectional;
    GXLIGHT theLight;
    GEColor theTerrainLight;
    GEColor theWaterLight;
    if (inMesh.GetNumVisibleTileRows() == 0)
        return;
    for (int i = 0; i < 8; i++) {
        if (gLightsInUse[i] == 0)
            continue;
        this->mDX7ScreenStates[inScreen]->mDevice->GetLight(i, &theLight);
        if (theLight.dltType != GXLIGHT_DIRECTIONAL ||
            (theLight.dcvDiffuse.r == 0.0f && theLight.dcvDiffuse.g == 0.0f &&
             theLight.dcvDiffuse.b == 0.0f))
            continue;
        theDirectional.mColor.SetRed(theLight.dcvDiffuse.r);
        theDirectional.mColor.SetGreen(theLight.dcvDiffuse.g);
        theDirectional.mColor.SetBlue(theLight.dcvDiffuse.b);
        theDirectional.mDirection.SetX(-theLight.dvDirection.x);
        theDirectional.mDirection.SetY(-theLight.dvDirection.y);
        theDirectional.mDirection.SetZ(-theLight.dvDirection.z);
        theLights.push_back(theDirectional);
    }
    theWaterLight = this->mComputedAmbientColor;
    unsigned long j;
    for (j = 0; j < theLights.size(); j++) {
        float theIntensity = theLights[j].mDirection.GetZ();
        if (!(theIntensity > 0.0f))
            continue;
        theWaterLight.SetRed(theLights[j].mColor.GetRed() * theIntensity + theWaterLight.GetRed());
        theWaterLight.SetGreen(theLights[j].mColor.GetGreen() * theIntensity + theWaterLight.GetGreen());
        theWaterLight.SetBlue(theLights[j].mColor.GetBlue() * theIntensity + theWaterLight.GetBlue());
    }
    theWaterLight.Normalize();
    unsigned long theStartX;
    unsigned long theStartY;
    unsigned long theWidth;
    unsigned long theHeight;
    inMesh.GetTerrainVisibleBottomLeftIndices(theStartX, theStartY);
    inMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
    unsigned long theEndX = theWidth + theStartX + 2;
    unsigned long theEndY = theHeight + theStartY + 2;
    for (unsigned long y = theStartY; y < theEndY; y++) {
        for (unsigned long x = theStartX; x < theEndX; x++) {
            if (!inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY])
                continue;
            GEVector* theNormal = inMesh.GetVertexNormal(x, y);
            theTerrainLight = this->mComputedAmbientColor;
            for (j = 0; j < theLights.size(); j++) {
                float theIntensity = theLights[j].mDirection.DotProduct(*theNormal);
                if (!(theIntensity > 0.0f))
                    continue;
                theTerrainLight.SetRed(theLights[j].mColor.GetRed() * theIntensity +
                                       theTerrainLight.GetRed());
                theTerrainLight.SetGreen(theLights[j].mColor.GetGreen() * theIntensity +
                                         theTerrainLight.GetGreen());
                theTerrainLight.SetBlue(theLights[j].mColor.GetBlue() * theIntensity +
                                        theTerrainLight.GetBlue());
            }
            theTerrainLight.Normalize();
            const GEColor& theColor = inMesh.GetVertexColor(x, y);
            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mColor =
                GEColor(theColor.GetRed() * theTerrainLight.GetRed(),
                        theColor.GetGreen() * theTerrainLight.GetGreen(),
                        theColor.GetBlue() * theTerrainLight.GetBlue());
            if (!inMesh.GetWaterPointVisibilityArray()[x - theStartX][y - theStartY])
                continue;
            const GEColor& theWater = inMesh.GetWaterColor(x, y);
            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mColor =
                GEColor(theWater.GetRed() * theWaterLight.GetRed(),
                        theWater.GetGreen() * theWaterLight.GetGreen(),
                        theWater.GetBlue() * theWaterLight.GetBlue());
        }
    }
}

// The water animation clock: advanced by every TransformToCamera that animates the water.
static float sWaterTime = 0.0f;

// FUNCTION: DX7HR 0x10007763
// Moves the visible terrain and water vertices to camera space (animating the water waves) and sets their clip flags,
// then sorts each material's polygons into unclipped and clipped lists; water-only mode skips dry vertices
void DX7Rasterizer::TransformToCamera(GETerrainMesh& inMesh, GEViewport& inViewport, float inScale, bool inWaterOnly,
                                      bool inOrthographic)
{
    GE3DPoint thePoint;
    GE3DPoint theWaterPoint;
    GEVector theNormal;
    GE3DPlane theLeftPlane = inViewport.GetLeftPlane();
    GE3DPlane theRightPlane = inViewport.GetRightPlane();
    GE3DPlane theTopPlane = inViewport.GetTopPlane();
    GE3DPlane theBottomPlane = inViewport.GetBottomPlane();
    float theNear = inViewport.GetFrontClippingDistance();
    float theFar = inViewport.GetRearClippingDistance();
    float theAngle;
    unsigned long theStartX;
    unsigned long theStartY;
    unsigned long theWidth;
    unsigned long theHeight;
    unsigned long theEndX;
    unsigned long theEndY;
    unsigned long x;
    unsigned long y;
    unsigned long i;
    unsigned long j;
    float theZ;
    float theWave;
    unsigned long theFlags;
    bool theIsWaterVisible;
    unsigned long theMaterialCount;
    unsigned long thePolygonCount;
    unsigned long theIndexCount;
    unsigned long theStripCount;
    unsigned long theClippedCount;
    GETerrainMesh::GETMPolygon* thePolygon;
    if (inMesh.GetNumVisibleTileRows() == 0)
        return;
    theAngle = UMArcTan(inViewport.GetHalfHeight() * UMTan(inViewport.GetFieldOfView() * 0.5f) /
                        (inViewport.GetHalfWidth() - 1.0f));
    theNormal.SetX(0.0f);
    theNormal.SetY(UMSin(theAngle));
    theNormal.SetZ(UMCos(theAngle));
    theNormal.Normalize();
    theBottomPlane.Define(thePoint, theNormal);
    theNormal.SetZ(-theNormal.GetZ());
    theTopPlane.Define(thePoint, theNormal);
    if ((this->mWaterRipple0 == 0.0f || inMesh.GetVisibleWaterTilePercentage() == 0.0f) && inWaterOnly)
        return;
    sWaterTime += inScale * this->mWaterRipple0;
    inMesh.GetTerrainVisibleBottomLeftIndices(theStartX, theStartY);
    inMesh.GetTerrainVisibleTileDimensions(theWidth, theHeight);
    theEndX = theWidth + theStartX + 2;
    theEndY = theHeight + theStartY + 2;
    if (!inOrthographic) {
        if (inWaterOnly) {
            for (y = theStartY; y < theEndY; y++) {
                for (x = theStartX; x < theEndX; x++) {
                    if (!inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    theZ = inMesh.GetZ(x, y);
                    theIsWaterVisible =
                        inMesh.GetWaterPointVisibilityArray()[x - theStartX][y - theStartY];
                    if (!(theZ < 0.0f) && !theIsWaterVisible)
                        continue;
                    thePoint.SetX((float)x);
                    thePoint.SetY((float)y);
                    theWave = (float)sin(thePoint.GetX() * thePoint.GetY() + sWaterTime);
                    if (theZ < -0.5f) {
                        thePoint.SetZ(theZ - theWave * this->mWaterRipple2);
                        inViewport.GetCameraInverse().Apply(thePoint,
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                        theFlags = 0;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetY() <= theNear)
                            theFlags |= kVertexClipNear;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetY() >= theFar)
                            theFlags |= kVertexClipFar;
                        if (theLeftPlane.IsPointOnOrBelowPlane(
                                inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                            theFlags |= kVertexClipLeft;
                        if (theRightPlane.IsPointOnOrBelowPlane(
                                inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                            theFlags |= kVertexClipRight;
                        if (theTopPlane.IsPointOnOrBelowPlane(
                                inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                            theFlags |= kVertexClipTop;
                        if (theBottomPlane.IsPointOnOrBelowPlane(
                                inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                            theFlags |= kVertexClipBottom;
                        inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                            theFlags;
                    }
                    if (!theIsWaterVisible)
                        continue;
                    theWaterPoint.SetX((float)x);
                    theWaterPoint.SetY((float)y);
                    if (theZ < 0.0f)
                        theWaterPoint.SetZ(theWave * this->mWaterRipple1);
                    else
                        theWaterPoint.SetZ((theWave - 1.0f) * this->mWaterRipple1 * 0.5f);
                    inViewport.GetCameraInverse().Apply(theWaterPoint,
                        inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() <= theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() >= theFar)
                        theFlags |= kVertexClipFar;
                    if (theLeftPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipLeft;
                    if (theRightPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipRight;
                    if (theTopPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipTop;
                    if (theBottomPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                        theFlags;
                }
            }
        } else {
            for (y = theStartY; y < theEndY; y++) {
                for (x = theStartX; x < theEndX; x++) {
                    if (!inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    thePoint.SetX((float)x);
                    thePoint.SetY((float)y);
                    theZ = inMesh.GetZ(x, y);
                    theWave = (float)sin(thePoint.GetX() * thePoint.GetY() + sWaterTime);
                    if (theZ >= -0.5f)
                        thePoint.SetZ(theZ);
                    else
                        thePoint.SetZ(theZ - theWave * this->mWaterRipple2);
                    inViewport.GetCameraInverse().Apply(thePoint,
                        inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetY() <=
                        theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetY() >=
                        theFar)
                        theFlags |= kVertexClipFar;
                    if (theLeftPlane.IsPointOnOrBelowPlane(
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipLeft;
                    if (theRightPlane.IsPointOnOrBelowPlane(
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipRight;
                    if (theTopPlane.IsPointOnOrBelowPlane(
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipTop;
                    if (theBottomPlane.IsPointOnOrBelowPlane(
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags = theFlags;
                    if (!inMesh.GetWaterPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    theWaterPoint.SetX((float)x);
                    theWaterPoint.SetY((float)y);
                    if (theZ < 0.0f)
                        theWaterPoint.SetZ(theWave * this->mWaterRipple1);
                    else
                        theWaterPoint.SetZ((theWave - 1.0f) * this->mWaterRipple1 * 0.5f);
                    inViewport.GetCameraInverse().Apply(theWaterPoint,
                        inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() <= theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() >= theFar)
                        theFlags |= kVertexClipFar;
                    if (theLeftPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipLeft;
                    if (theRightPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipRight;
                    if (theTopPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipTop;
                    if (theBottomPlane.IsPointOnOrBelowPlane(
                            inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                        theFlags;
                }
            }
            for (x = 0; x < inMesh.GetEdgeWorldPoints().size(); x++)
                inViewport.GetCameraInverse().Apply(inMesh.GetEdgeWorldPoints()[x],
                                                    inMesh.GetEdgeRenderVertices()[x]->mPoint);
            for (x = 0; x < inMesh.GetCliffWorldPoints().size(); x++)
                inViewport.GetCameraInverse().Apply(inMesh.GetCliffWorldPoints()[x],
                                                    inMesh.GetCliffRenderVertices()[x]->mPoint);
        }
    } else {
        if (inWaterOnly) {
            for (y = theStartY; y < theEndY; y++) {
                for (x = theStartX; x < theEndX; x++) {
                    if (!inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    theZ = inMesh.GetZ(x, y);
                    theIsWaterVisible =
                        inMesh.GetWaterPointVisibilityArray()[x - theStartX][y - theStartY];
                    if (!(theZ < 0.0f) && !theIsWaterVisible)
                        continue;
                    thePoint.SetX((float)x);
                    thePoint.SetY((float)y);
                    theWave = (float)sin(thePoint.GetX() * thePoint.GetY() + sWaterTime);
                    if (theZ < -0.5f) {
                        thePoint.SetZ(theZ - theWave * this->mWaterRipple2);
                        inViewport.GetCameraInverse().Apply(thePoint,
                            inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                        theFlags = 0;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetY() <= theNear)
                            theFlags |= kVertexClipNear;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetY() >= theFar)
                            theFlags |= kVertexClipFar;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetX() <= inViewport.GetOrthographicRectangle().GetLeft())
                            theFlags |= kVertexClipLeft;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetX() >= inViewport.GetOrthographicRectangle().GetRight())
                            theFlags |= kVertexClipRight;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetZ() >= -(inViewport.GetOrthographicRectangle().GetTop() *
                                                    inViewport.GetYClippingPlane()))
                            theFlags |= kVertexClipTop;
                        if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY]
                                .mPoint.GetZ() <= -(inViewport.GetYClippingPlane() *
                                                    inViewport.GetOrthographicRectangle().GetBottom()))
                            theFlags |= kVertexClipBottom;
                        inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                            theFlags;
                    }
                    if (!theIsWaterVisible)
                        continue;
                    theWaterPoint.SetX((float)x);
                    theWaterPoint.SetY((float)y);
                    if (theZ < 0.0f)
                        theWaterPoint.SetZ(theWave * this->mWaterRipple1);
                    else
                        theWaterPoint.SetZ((theWave - 1.0f) * this->mWaterRipple1 * 0.5f);
                    inViewport.GetCameraInverse().Apply(theWaterPoint,
                        inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() <= theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() >= theFar)
                        theFlags |= kVertexClipFar;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetX() <= inViewport.GetOrthographicRectangle().GetLeft())
                        theFlags |= kVertexClipLeft;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetX() >= inViewport.GetOrthographicRectangle().GetRight())
                        theFlags |= kVertexClipRight;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetZ() >= -(inViewport.GetOrthographicRectangle().GetTop() *
                                                inViewport.GetYClippingPlane()))
                        theFlags |= kVertexClipTop;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetZ() <= -(inViewport.GetYClippingPlane() *
                                                inViewport.GetOrthographicRectangle().GetBottom()))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                        theFlags;
                }
            }
        } else {
            for (y = theStartY; y < theEndY; y++) {
                for (x = theStartX; x < theEndX; x++) {
                    if (!inMesh.GetTerrainPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    thePoint.SetX((float)x);
                    thePoint.SetY((float)y);
                    theZ = inMesh.GetZ(x, y);
                    theWave = (float)sin(thePoint.GetX() * thePoint.GetY() + sWaterTime);
                    if (theZ >= -0.5f)
                        thePoint.SetZ(theZ);
                    else
                        thePoint.SetZ(theZ - theWave * this->mWaterRipple2);
                    inViewport.GetCameraInverse().Apply(thePoint,
                        inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetY() <=
                        theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetY() >=
                        theFar)
                        theFlags |= kVertexClipFar;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetX() <=
                        inViewport.GetOrthographicRectangle().GetLeft())
                        theFlags |= kVertexClipLeft;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetX() >=
                        inViewport.GetOrthographicRectangle().GetRight())
                        theFlags |= kVertexClipRight;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetZ() >=
                        -(inViewport.GetYClippingPlane() * inViewport.GetOrthographicRectangle().GetTop()))
                        theFlags |= kVertexClipTop;
                    if (inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mPoint.GetZ() <=
                        -(inViewport.GetYClippingPlane() * inViewport.GetOrthographicRectangle().GetBottom()))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetTerrainRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags = theFlags;
                    if (!inMesh.GetWaterPointVisibilityArray()[x - theStartX][y - theStartY])
                        continue;
                    theWaterPoint.SetX((float)x);
                    theWaterPoint.SetY((float)y);
                    if (theZ < 0.0f)
                        theWaterPoint.SetZ(theWave * this->mWaterRipple1);
                    else
                        theWaterPoint.SetZ((theWave - 1.0f) * this->mWaterRipple1 * 0.5f);
                    inViewport.GetCameraInverse().Apply(theWaterPoint,
                        inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mPoint);
                    theFlags = 0;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() <= theNear)
                        theFlags |= kVertexClipNear;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetY() >= theFar)
                        theFlags |= kVertexClipFar;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetX() <= inViewport.GetOrthographicRectangle().GetLeft())
                        theFlags |= kVertexClipLeft;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetX() >= inViewport.GetOrthographicRectangle().GetRight())
                        theFlags |= kVertexClipRight;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetZ() >= -(inViewport.GetYClippingPlane() *
                                                inViewport.GetOrthographicRectangle().GetTop()))
                        theFlags |= kVertexClipTop;
                    if (inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY]
                            .mPoint.GetZ() <= -(inViewport.GetYClippingPlane() *
                                                inViewport.GetOrthographicRectangle().GetBottom()))
                        theFlags |= kVertexClipBottom;
                    inMesh.GetWaterRenderVertexArray()[x - theStartX][y - theStartY].mClipFlags =
                        theFlags;
                }
            }
            for (x = 0; x < inMesh.GetEdgeWorldPoints().size(); x++)
                inViewport.GetCameraInverse().Apply(inMesh.GetEdgeWorldPoints()[x],
                                                    inMesh.GetEdgeRenderVertices()[x]->mPoint);
            for (x = 0; x < inMesh.GetCliffWorldPoints().size(); x++)
                inViewport.GetCameraInverse().Apply(inMesh.GetCliffWorldPoints()[x],
                                                    inMesh.GetCliffRenderVertices()[x]->mPoint);
        }
    }
    // Sort each material's polygons: none clipped -> mIndices (drawn with D3D clipping off), partly clipped
    // -> mStripIndices (clipping on); all three outside one common plane -> dropped. Both lists end in NULL
    // and carry their count in their last element.
    theMaterialCount = inMesh.GetTerrainMaterialList().size();
    for (i = 0; i < theMaterialCount; i++) {
        theStripCount = 0;
        theIndexCount = 0;
        thePolygonCount = inMesh.GetTerrainMaterialList()[i].mPolygons.size();
        for (j = 0; j < thePolygonCount; j++) {
            thePolygon = inMesh.GetTerrainMaterialList()[i].mPolygons[j];
            theClippedCount = (thePolygon->mVertices[0]->mClipFlags != 0) +
                              (thePolygon->mVertices[1]->mClipFlags != 0) +
                              (thePolygon->mVertices[2]->mClipFlags != 0);
            if (theClippedCount == 0) {
                inMesh.GetTerrainMaterialList()[i].mIndices[theIndexCount] = thePolygon;
                theIndexCount++;
            } else if (theClippedCount != 3 ||
                       (thePolygon->mVertices[0]->mClipFlags & thePolygon->mVertices[1]->mClipFlags &
                        thePolygon->mVertices[2]->mClipFlags) == 0 ||
                       (thePolygon->mVertices[0]->mClipFlags & thePolygon->mVertices[1]->mClipFlags &
                        thePolygon->mVertices[2]->mClipFlags) == 0x3f) {
                inMesh.GetTerrainMaterialList()[i].mStripIndices[theStripCount] = thePolygon;
                theStripCount++;
            }
        }
        inMesh.GetTerrainMaterialList()[i].mStripIndices[theStripCount] = NULL;
        inMesh.GetTerrainMaterialList()[i].mIndices[theIndexCount] = NULL;
        DX7SetListCount(inMesh.GetTerrainMaterialList()[i].mStripIndices, theStripCount);
        DX7SetListCount(inMesh.GetTerrainMaterialList()[i].mIndices, theIndexCount);
    }
    theMaterialCount = inMesh.GetWaterMaterialList().size();
    for (i = 0; i < theMaterialCount; i++) {
        theStripCount = 0;
        theIndexCount = 0;
        thePolygonCount = inMesh.GetWaterMaterialList()[i].mPolygons.size();
        for (j = 0; j < thePolygonCount; j++) {
            thePolygon = inMesh.GetWaterMaterialList()[i].mPolygons[j];
            theClippedCount = (thePolygon->mVertices[0]->mClipFlags != 0) +
                              (thePolygon->mVertices[1]->mClipFlags != 0) +
                              (thePolygon->mVertices[2]->mClipFlags != 0);
            if (theClippedCount == 0) {
                inMesh.GetWaterMaterialList()[i].mIndices[theIndexCount] = thePolygon;
                theIndexCount++;
            } else if (theClippedCount != 3 ||
                       (thePolygon->mVertices[0]->mClipFlags & thePolygon->mVertices[1]->mClipFlags &
                        thePolygon->mVertices[2]->mClipFlags) == 0 ||
                       (thePolygon->mVertices[0]->mClipFlags & thePolygon->mVertices[1]->mClipFlags &
                        thePolygon->mVertices[2]->mClipFlags) == 0x3f) {
                inMesh.GetWaterMaterialList()[i].mStripIndices[theStripCount] = thePolygon;
                theStripCount++;
            }
        }
        inMesh.GetWaterMaterialList()[i].mStripIndices[theStripCount] = NULL;
        inMesh.GetWaterMaterialList()[i].mIndices[theIndexCount] = NULL;
        DX7SetListCount(inMesh.GetWaterMaterialList()[i].mStripIndices, theStripCount);
        DX7SetListCount(inMesh.GetWaterMaterialList()[i].mIndices, theIndexCount);
    }
}


// COMDATs this unit is the first (link order) to instantiate, then the template-static init chain head and atexit
// registrars:
// TEMPLATE: DX7HR 0x10008bc5
// TEMPLATE: DX7HR 0x10008bcc
// TEMPLATE: DX7HR 0x10008bef
// TEMPLATE: DX7HR 0x10008c1b
// TEMPLATE: DX7HR 0x10008c2d
// TEMPLATE: DX7HR 0x10008c42
// TEMPLATE: DX7HR 0x10008c54
// TEMPLATE: DX7HR 0x10008cae
// TEMPLATE: DX7HR 0x10008ce8
// TEMPLATE: DX7HR 0x10008d15
// TEMPLATE: DX7HR 0x10008d22
// TEMPLATE: DX7HR 0x10008d37
// TEMPLATE: DX7HR 0x10008d6f
// TEMPLATE: DX7HR 0x1000904d
// TEMPLATE: DX7HR 0x1000904e
// TEMPLATE: DX7HR 0x10009075
// TEMPLATE: DX7HR 0x1000908c
// TEMPLATE: DX7HR 0x100090b4
// TEMPLATE: DX7HR 0x100090d1
// TEMPLATE: DX7HR 0x100090fd
// TEMPLATE: DX7HR 0x10009127
// SYNTHETIC: DX7HR 0x10009152
// SYNTHETIC: DX7HR 0x1000917a
// SYNTHETIC: DX7HR 0x10009186
// SYNTHETIC: DX7HR 0x10009192
// SYNTHETIC: DX7HR 0x1000919e

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x10004da6
// SYNTHETIC: DX7HRTNL 0x10004db0
// SYNTHETIC: DX7HRTNL 0x10004dc1
// SYNTHETIC: DX7HRTNL 0x10004dcd
// SYNTHETIC: DX7HRTNL 0x10004dd8
// SYNTHETIC: DX7HRTNL 0x10004de2
// SYNTHETIC: DX7HRTNL 0x10004df3
// SYNTHETIC: DX7HRTNL 0x10004dff
// SYNTHETIC: DX7HRTNL 0x10004e0a
// SYNTHETIC: DX7HRTNL 0x10004e14
// SYNTHETIC: DX7HRTNL 0x10004e25
// SYNTHETIC: DX7HRTNL 0x10004e31
// SYNTHETIC: DX7HRTNL 0x10004e3c
// SYNTHETIC: DX7HRTNL 0x10004e46
// SYNTHETIC: DX7HRTNL 0x10004e57
// SYNTHETIC: DX7HRTNL 0x10004e63
// SYNTHETIC: DX7HRTNL 0x10004e6e
// SYNTHETIC: DX7HRTNL 0x10004e78
// SYNTHETIC: DX7HRTNL 0x10004e89
// SYNTHETIC: DX7HRTNL 0x10004e95
// SYNTHETIC: DX7HRTNL 0x10004ea0
// SYNTHETIC: DX7HRTNL 0x10004eaa
// SYNTHETIC: DX7HRTNL 0x10004ebb
// SYNTHETIC: DX7HRTNL 0x10004ec7
// SYNTHETIC: DX7HRTNL 0x10004ed2
// SYNTHETIC: DX7HRTNL 0x10004edc
// SYNTHETIC: DX7HRTNL 0x10004eed
// SYNTHETIC: DX7HRTNL 0x10004ef9
// FUNCTION: DX7HRTNL 0x10004f04
// FUNCTION: DX7HRTNL 0x10005c1f
// FUNCTION: DX7HRTNL 0x100061e5
// FUNCTION: DX7HRTNL 0x1000665d
// FUNCTION: DX7HRTNL 0x10006cdf
// FUNCTION: DX7HRTNL 0x10006fcc
// FUNCTION: DX7HRTNL 0x100072c7
// FUNCTION: DX7HRTNL 0x100077ef
// TEMPLATE: DX7HRTNL 0x10008c51
// TEMPLATE: DX7HRTNL 0x10008c58
// TEMPLATE: DX7HRTNL 0x10008c7b
// TEMPLATE: DX7HRTNL 0x10008ca7
// TEMPLATE: DX7HRTNL 0x10008cb9
// TEMPLATE: DX7HRTNL 0x10008cce
// TEMPLATE: DX7HRTNL 0x10008d28
// TEMPLATE: DX7HRTNL 0x10008d62
// TEMPLATE: DX7HRTNL 0x10008d8f
// TEMPLATE: DX7HRTNL 0x10008da4
// TEMPLATE: DX7HRTNL 0x10008ddc
// TEMPLATE: DX7HRTNL 0x100090ba
// TEMPLATE: DX7HRTNL 0x100090bb
// TEMPLATE: DX7HRTNL 0x100090e2
// TEMPLATE: DX7HRTNL 0x100090f9
// TEMPLATE: DX7HRTNL 0x10009121
// TEMPLATE: DX7HRTNL 0x1000913e
// TEMPLATE: DX7HRTNL 0x1000916a
// TEMPLATE: DX7HRTNL 0x10009194
// SYNTHETIC: DX7HRTNL 0x100091bf
// SYNTHETIC: DX7HRTNL 0x100091e7
// SYNTHETIC: DX7HRTNL 0x100091f3
// SYNTHETIC: DX7HRTNL 0x100091ff
// SYNTHETIC: DX7HRTNL 0x1000920b
