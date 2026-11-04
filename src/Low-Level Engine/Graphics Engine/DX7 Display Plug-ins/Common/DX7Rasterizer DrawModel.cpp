// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer DrawModel.cpp (file name
// inferred: the third of the four DX7Rasterizer units in link order; it holds the 3D model draws)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000383a..0x10004d1a: the U2D key init chains, DX7StandardVertexArray's implicit copy ctor and vector deleting
// dtor, the model vertex copies, Draw(GEPolygonArray), Draw(GEModel), Draw3DLine, DrawModelHierarchy and DrawSky
// (export order), the vector<GERasterizer::TransformStack> and std::exception COMDATs, then the template-static init
// chain head and its atexit registrars.
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "lle/GEMaterial.h"

// The U2D key init chains:
// SYNTHETIC: DX7HR 0x1000383a
// SYNTHETIC: DX7HR 0x10003844
// SYNTHETIC: DX7HR 0x10003855
// SYNTHETIC: DX7HR 0x10003861
// SYNTHETIC: DX7HR 0x1000386c
// SYNTHETIC: DX7HR 0x10003876
// SYNTHETIC: DX7HR 0x10003887
// SYNTHETIC: DX7HR 0x10003893
// SYNTHETIC: DX7HR 0x1000389e
// SYNTHETIC: DX7HR 0x100038a8
// SYNTHETIC: DX7HR 0x100038b9
// SYNTHETIC: DX7HR 0x100038c5
// SYNTHETIC: DX7HR 0x100038d0
// SYNTHETIC: DX7HR 0x100038da
// SYNTHETIC: DX7HR 0x100038eb
// SYNTHETIC: DX7HR 0x100038f7
// SYNTHETIC: DX7HR 0x10003902
// SYNTHETIC: DX7HR 0x1000390c
// SYNTHETIC: DX7HR 0x1000391d
// SYNTHETIC: DX7HR 0x10003929
// SYNTHETIC: DX7HR 0x10003934
// SYNTHETIC: DX7HR 0x1000393e
// SYNTHETIC: DX7HR 0x1000394f
// SYNTHETIC: DX7HR 0x1000395b
// SYNTHETIC: DX7HR 0x10003966
// SYNTHETIC: DX7HR 0x10003970
// SYNTHETIC: DX7HR 0x10003981
// SYNTHETIC: DX7HR 0x1000398d

// DX7StandardVertexArray's implicit copy ctor and vector deleting dtor:
// TEMPLATE: DX7HR 0x10003998
// TEMPLATE: DX7HR 0x100039bb

// FUNCTION: DX7HR 0x10003a15
// Copies a material's vertex range of the model's current frame (coloured vertices) into the vertex buffer
void DX7Rasterizer::CopyStdColorVertices(GEModel* inModel, GEMaterial* inMaterial, D3DVERTEX_STD_COLOR* outVertices)
{
    memcpy(outVertices,
           static_cast<DX7StdColorVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsCurrentFrame()))->mVertices +
               inMaterial->GetVertexStartingIndex(),
           inMaterial->GetVertexRangeLength() * sizeof(D3DVERTEX_STD_COLOR));
}

// FUNCTION: DX7HR 0x10003a52
// Copies a material's vertex range of the model's current frame into the vertex buffer as white coloured vertices
void DX7Rasterizer::CopyStdVertices(GEModel* inModel, GEMaterial* inMaterial, D3DVERTEX_STD_COLOR* outVertices)
{
    D3DVERTEX* theVertex =
        static_cast<DX7StandardVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsCurrentFrame()))->mVertices +
        inMaterial->GetVertexStartingIndex();
    D3DVERTEX* theEnd = theVertex + inMaterial->GetVertexRangeLength();
    for (; theVertex < theEnd; theVertex++, outVertices++) {
        outVertices->x = theVertex->x;
        outVertices->y = theVertex->y;
        outVertices->z = theVertex->z;
        outVertices->nx = theVertex->nx;
        outVertices->ny = theVertex->ny;
        outVertices->nz = theVertex->nz;
        outVertices->color = 0xffffffff;
        outVertices->tu = theVertex->tu;
        outVertices->tv = theVertex->tv;
    }
}

// FUNCTION: DX7HR 0x10003ac5
// Writes a material's coloured vertices blended between the current and next frame (by the fractional frame position)
void DX7Rasterizer::CopyAndSmoothStdColorVertices(GEModel* inModel, GEMaterial* inMaterial,
                                                  D3DVERTEX_STD_COLOR* outVertices)
{
    float theFraction = inModel->GetFractionalFramePosition();
    float theInverse = 1.0f - theFraction;
    // The range length is read into a local first: its load precedes the frame loads, and the x87 operand order
    // of the first blend follows from it (94.8 -> exact).
    unsigned long theLength = inMaterial->GetVertexRangeLength();
    D3DVERTEX_STD_COLOR* theCurrent =
        static_cast<DX7StdColorVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsCurrentFrame()))->mVertices +
        inMaterial->GetVertexStartingIndex();
    D3DVERTEX_STD_COLOR* theEnd = theCurrent + theLength;
    D3DVERTEX_STD_COLOR* theNext =
        static_cast<DX7StdColorVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsNextFrame()))->mVertices +
        inMaterial->GetVertexStartingIndex();
    for (; theCurrent < theEnd; theCurrent++, theNext++, outVertices++) {
        outVertices->x = theNext->x * theFraction + theCurrent->x * theInverse;
        outVertices->y = theCurrent->y * theInverse + theNext->y * theFraction;
        outVertices->z = theCurrent->z * theInverse + theNext->z * theFraction;
        outVertices->nx = theCurrent->nx * theInverse + theNext->nx * theFraction;
        outVertices->ny = theCurrent->ny * theInverse + theNext->ny * theFraction;
        outVertices->nz = theCurrent->nz * theInverse + theNext->nz * theFraction;
        outVertices->color = theCurrent->color;
        outVertices->tu = theCurrent->tu;
        outVertices->tv = theCurrent->tv;
    }
}

// FUNCTION: DX7HR 0x10003b94
// Writes a material's vertices blended between the current and next frame as white coloured vertices
void DX7Rasterizer::CopyAndSmoothStdVertices(GEModel* inModel, GEMaterial* inMaterial,
                                             D3DVERTEX_STD_COLOR* outVertices)
{
    float theFraction = inModel->GetFractionalFramePosition();
    float theInverse = 1.0f - theFraction;
    // The range length is read into a local first: its load precedes the frame loads, and the x87 operand order
    // of the first blend follows from it (94.8 -> exact).
    unsigned long theLength = inMaterial->GetVertexRangeLength();
    D3DVERTEX* theCurrent =
        static_cast<DX7StandardVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsCurrentFrame()))->mVertices +
        inMaterial->GetVertexStartingIndex();
    D3DVERTEX* theEnd = theCurrent + theLength;
    D3DVERTEX* theNext =
        static_cast<DX7StandardVertexArray*>(inModel->GetVertexArray(inModel->GetGraphicsNextFrame()))->mVertices +
        inMaterial->GetVertexStartingIndex();
    for (; theCurrent < theEnd; theCurrent++, theNext++, outVertices++) {
        outVertices->x = theNext->x * theFraction + theCurrent->x * theInverse;
        outVertices->y = theCurrent->y * theInverse + theNext->y * theFraction;
        outVertices->z = theCurrent->z * theInverse + theNext->z * theFraction;
        outVertices->nx = theCurrent->nx * theInverse + theNext->nx * theFraction;
        outVertices->ny = theCurrent->ny * theInverse + theNext->ny * theFraction;
        outVertices->nz = theCurrent->nz * theInverse + theNext->nz * theFraction;
        outVertices->color = 0xffffffff;
        outVertices->tu = theCurrent->tu;
        outVertices->tv = theCurrent->tv;
    }
}

// FUNCTION: DX7HR 0x10003c5d
// Draws an indexed triangle list with an identity world transform and D3D clipping on; counts the triangles drawn
long DX7Rasterizer::Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                         unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
{
    GETransformation theIdentity;
    D3DMATRIX theMatrix;
    long theResult;
    this->mDrawingAccess->Enter();
    this->Set3DMode(inScreen);
    this->ConvertToD3DMatrix(theIdentity, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
    this->SetTexture(inTexture, inScreen);
    D3DVERTEX_STD_COLOR* theBuffer;
    this->LockVB(inVertexCount, inScreen, theBuffer);
    theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
    if (inVertices.GetType() == 1)
        memcpy(theBuffer, ((DX7StdColorVertexArray&)inVertices).mVertices,
               inVertexCount * sizeof(D3DVERTEX_STD_COLOR));
    else {
        D3DVERTEX* theVertex = ((DX7StandardVertexArray&)inVertices).mVertices;
        D3DVERTEX* theEnd = theVertex + inVertexCount;
        for (; theVertex < theEnd; theVertex++, theBuffer++) {
            theBuffer->x = theVertex->x;
            theBuffer->y = theVertex->y;
            theBuffer->z = theVertex->z;
            theBuffer->nx = theVertex->nx;
            theBuffer->ny = theVertex->ny;
            theBuffer->nz = theVertex->nz;
            theBuffer->color = 0xffffffff;
            theBuffer->tu = theVertex->tu;
            theBuffer->tv = theVertex->tv;
        }
    }
    theResult = this->UnlockVB(inScreen);
    while (true) {
        if (this->mDX7ScreenStates[inScreen]->mDevice->DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST,
                this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition,
                inVertexCount, ((DX7PolygonArray&)inPolygons).mIndices, inCount * 3, 0) != DDERR_WASSTILLDRAWING)
            break;
        Sleep(0);
    }
    this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += inVertexCount;
    this->mScreenStates[inScreen]->m32C[1] += inCount;
    this->mDrawingAccess->Leave();
    return theResult;
}

// FUNCTION: DX7HR 0x10003e60
// Draws a model and its sub-models under the drawing lock (the float argument becomes the level-of-detail divisor)
long DX7Rasterizer::Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
{
    this->mDrawingAccess->Enter();
    this->Set3DMode(inScreen);
    long theResult = this->DrawModelHierarchy(inModel, inScreen, inOpacity, inOrthographic);
    this->mDrawingAccess->Leave();
    return theResult;
}

// FUNCTION: DX7HR 0x10003ea7
// Draws a textured polyline (at most 1200 segments) as a red line list in camera space, advancing the texture V
// along it (without the drawing lock)
long DX7Rasterizer::Draw3DLine(std::vector<std::pair<GE3DPoint, float> >& inPoints, GETexture* inTexture,
                               float inV, unsigned long inScreen, bool inOrthographic)
{
    GE3DPoint thePoint;
    GETransformation theTransformation;
    D3DMATRIX theMatrix;
    float theV = inV;
    GEViewport& theViewport = this->mScreenStates[inScreen]->mViewport;
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &theMatrix);
    theTransformation.SetElement(1, 1, 0.0f);
    theTransformation.SetElement(1, 2, 1.0f);
    theTransformation.SetElement(2, 1, 1.0f);
    theTransformation.SetElement(2, 2, 0.0f);
    this->ConvertToD3DMatrix(theTransformation, theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(D3DTRANSFORMSTATE_VIEW, &theMatrix);
    if (!inOrthographic)
        this->ConvertToD3DMatrix(theViewport.GetPerspectiveProjection(), theMatrix);
    else
        this->ConvertToD3DMatrix(theViewport.GetOrthographicProjection(), theMatrix);
    this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(D3DTRANSFORMSTATE_PROJECTION, &theMatrix);
    this->SetTexture(inTexture, inScreen);
    D3DMATERIAL7 theMaterial;
    theMaterial.dcvAmbient.r = 1.0f;
    theMaterial.dcvAmbient.g = 1.0f;
    theMaterial.dcvAmbient.b = 1.0f;
    theMaterial.dcvAmbient.a = 1.0f;
    theMaterial.dcvDiffuse.r = 1.0f;
    theMaterial.dcvDiffuse.g = 1.0f;
    theMaterial.dcvDiffuse.b = 1.0f;
    theMaterial.dcvDiffuse.a = 1.0f;
    theMaterial.dcvEmissive.r = 0.0f;
    theMaterial.dcvEmissive.g = 0.0f;
    theMaterial.dcvEmissive.b = 0.0f;
    theMaterial.dcvEmissive.a = 0.0f;
    theMaterial.dcvSpecular.r = 0.0f;
    theMaterial.dcvSpecular.g = 0.0f;
    theMaterial.dcvSpecular.b = 0.0f;
    theMaterial.dcvSpecular.a = 0.0f;
    theMaterial.dvPower = 0.0f;
    this->mDX7ScreenStates[inScreen]->mDevice->SetMaterial(&theMaterial);
    this->SetShadingColorSource(0, inScreen);
    unsigned long theCount = inPoints.size();
    // A polyline needs two points (an empty one would wrap the unsigned vertex count and read past the vector).
    if (theCount < 2)
        return 0;
    unsigned long theVertexCount = theCount * 2 - 2;
    if (theVertexCount > 0x960) {
        theVertexCount = 0x960;
        theCount = 0x4b1;
    }
    D3DVERTEX_STD_COLOR* theBuffer;
    this->LockVB(theVertexCount, inScreen, theBuffer);
    theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
    for (unsigned long i = 1; i < theCount; i++) {
        theViewport.GetCameraInverse().Apply(inPoints[i - 1].first, thePoint);
        theBuffer->x = thePoint.GetX();
        theBuffer->y = thePoint.GetY();
        theBuffer->z = thePoint.GetZ();
        theBuffer->nx = 0.0f;
        theBuffer->ny = 0.0f;
        theBuffer->nz = 0.0f;
        theBuffer->color = 0xffff0000;
        theBuffer->tu = 0.0f;
        theBuffer->tv = theV;
        theV += inPoints[i - 1].second;
        theBuffer++;
        theViewport.GetCameraInverse().Apply(inPoints[i].first, thePoint);
        theBuffer->x = thePoint.GetX();
        theBuffer->y = thePoint.GetY();
        theBuffer->z = thePoint.GetZ();
        theBuffer->nx = 0.0f;
        theBuffer->ny = 0.0f;
        theBuffer->nz = 0.0f;
        theBuffer->color = 0xffff0000;
        theBuffer->tu = 0.0f;
        theBuffer->tv = theV;
        theBuffer++;
        if (theV <= -1.0f)
            theV = UMCeil(theV) - theV;
        else if (theV > 1.0f)
            theV = theV - UMFloor(theV);
    }
    long theResult = this->UnlockVB(inScreen);
    while (true) {
        if (this->mDX7ScreenStates[inScreen]->mDevice->DrawPrimitiveVB(D3DPT_LINELIST,
                this->mDX7ScreenStates[inScreen]->mVertexBuffer, this->mDX7ScreenStates[inScreen]->mVertexBufferPosition,
                theVertexCount, 0) != DDERR_WASSTILLDRAWING)
            break;
        Sleep(0);
    }
    this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += theVertexCount;
    return theResult;
}

// FUNCTION: DX7HR 0x100042c1
// Draws a model and recursively its sub-models: composes the transforms, culls, picks the root's level of detail from
// its on-screen size and draws each material's triangles; stops at the first sub-model error
long DX7Rasterizer::DrawModelHierarchy(GEModel* inModel, unsigned long inScreen, float inDetail, bool inOrthographic)
{
    long theResult = kErrorOK;
    D3DVERTEX_STD_COLOR* theBuffer;
    GEViewport* theViewport = &this->mScreenStates[inScreen]->mViewport;
    unsigned long theMaterialCount = inModel->GetMaterialCount();
    GETransformation theTransformation(false);
    float theScale;
    bool theHasParent;
    if (GERasterizer::sTransformStack.empty()) {
        theTransformation = inModel->GetGraphicsTransform();
        theScale = inModel->GetGraphicsScale();
        theHasParent = false;
    } else {
        GERasterizer::sTransformStack.back().mTransformation->Apply(inModel->GetGraphicsTransform(), theTransformation);
        theScale = inModel->GetGraphicsScale() * GERasterizer::sTransformStack.back().mScale;
        theHasParent = true;
    }
    GERasterizer::sTransformStack.push_back(TransformStack(&theTransformation, theScale));
    bool theIsVisible;
    bool theIsClipped;
    if (!inOrthographic)
        this->IsPerspectiveModelVisible(inModel, theScale, theViewport, theTransformation, theIsVisible, theIsClipped);
    else
        this->IsOrthographicModelVisible(inModel, theViewport, theTransformation, theIsVisible, theIsClipped);
    if (theIsVisible) {
        unsigned long theLOD;
        if (!theHasParent) {
            float theWidth = theViewport->GetViewRectangle().GetWidth();
            float theDetail = this->GetPixelSize(inModel, theViewport, theTransformation, theScale, inOrthographic) *
                              (800.0f / theWidth) / inDetail;
            if (1.0f < theDetail)
                theDetail = 1.0f;
            theLOD = UMFloatToLong((1.0f - theDetail) * inModel->GetLODCount());
            if (theLOD >= inModel->GetLODCount() - 1)
                theLOD = inModel->GetLODCount() - 1;
        } else
            theLOD = 0;
        D3DMATRIX theMatrix;
        this->ConvertToD3DMatrix(theTransformation, theMatrix);
        this->mDX7ScreenStates[inScreen]->mDevice->SetTransform(D3DTRANSFORMSTATE_WORLD, &theMatrix);
        if (!theIsClipped)
            theResult = this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(D3DRENDERSTATE_CLIPPING, FALSE);
        else
            theResult = this->mDX7ScreenStates[inScreen]->mDevice->SetRenderState(D3DRENDERSTATE_CLIPPING, TRUE);
        GEPolygonArray* thePolygons = inModel->GetPolygonArray(theLOD);
        D3DMATERIAL7 theD3DMaterial;
        theD3DMaterial.dcvAmbient.r = 1.0f;
        theD3DMaterial.dcvAmbient.g = 1.0f;
        theD3DMaterial.dcvAmbient.b = 1.0f;
        theD3DMaterial.dcvAmbient.a = 1.0f;
        theD3DMaterial.dcvDiffuse.r = 1.0f;
        theD3DMaterial.dcvDiffuse.g = 1.0f;
        theD3DMaterial.dcvDiffuse.b = 1.0f;
        theD3DMaterial.dcvDiffuse.a = 1.0f;
        theD3DMaterial.dcvEmissive.r = 0.0f;
        theD3DMaterial.dcvEmissive.g = 0.0f;
        theD3DMaterial.dcvEmissive.b = 0.0f;
        theD3DMaterial.dcvEmissive.a = 0.0f;
        theD3DMaterial.dcvSpecular.r = 0.0f;
        theD3DMaterial.dcvSpecular.g = 0.0f;
        theD3DMaterial.dcvSpecular.b = 0.0f;
        theD3DMaterial.dcvSpecular.a = 0.0f;
        theD3DMaterial.dvPower = 0.0f;
        for (unsigned long i = 0; i < theMaterialCount; i++) {
            GEMaterial* theMaterial = inModel->GetMaterial(i);
            if (!(theMaterial->GetPolygonRangeLength(theLOD) > 0))
                continue;
            this->mScreenStates[inScreen]->m32C[1] += theMaterial->GetPolygonRangeLength(theLOD);
            this->SetTexture(theMaterial->GetTexture(inScreen), inScreen);
            theD3DMaterial.dcvAmbient.r = theMaterial->GetColor().GetRed();
            theD3DMaterial.dcvAmbient.g = theMaterial->GetColor().GetGreen();
            theD3DMaterial.dcvAmbient.b = theMaterial->GetColor().GetBlue();
            theD3DMaterial.dcvDiffuse.r = theMaterial->GetColor().GetRed();
            theD3DMaterial.dcvDiffuse.g = theMaterial->GetColor().GetGreen();
            theD3DMaterial.dcvDiffuse.b = theMaterial->GetColor().GetBlue();
            theD3DMaterial.dcvDiffuse.a = theMaterial->GetOpacity() * inModel->GetModelOpacity();
            this->mDX7ScreenStates[inScreen]->mDevice->SetMaterial(&theD3DMaterial);
            this->LockVB(theMaterial->GetVertexRangeLength(), inScreen, theBuffer);
            theBuffer += this->mDX7ScreenStates[inScreen]->mVertexBufferPosition;
            if (inModel->GetVertexArray(0)->GetType() == 0) {
                if (inModel->GetFractionalFramePosition() != 0.0f)
                    this->CopyAndSmoothStdVertices(inModel, theMaterial, theBuffer);
                else
                    this->CopyStdVertices(inModel, theMaterial, theBuffer);
            } else {
                if (inModel->GetFractionalFramePosition() != 0.0f)
                    this->CopyAndSmoothStdColorVertices(inModel, theMaterial, theBuffer);
                else
                    this->CopyStdColorVertices(inModel, theMaterial, theBuffer);
            }
            theResult = this->UnlockVB(inScreen);
            while (true) {
                if (this->mDX7ScreenStates[inScreen]->mDevice->DrawIndexedPrimitiveVB(D3DPT_TRIANGLELIST,
                        this->mDX7ScreenStates[inScreen]->mVertexBuffer,
                        this->mDX7ScreenStates[inScreen]->mVertexBufferPosition, theMaterial->GetVertexRangeLength(),
                        static_cast<DX7PolygonArray*>(thePolygons)->mIndices +
                            theMaterial->GetPolygonStartingIndex(theLOD) * 3,
                        theMaterial->GetPolygonRangeLength(theLOD) * 3, 0) != DDERR_WASSTILLDRAWING)
                    break;
                Sleep(0);
            }
            this->mDX7ScreenStates[inScreen]->mVertexBufferPosition += theMaterial->GetVertexRangeLength();
        }
    }
    for (unsigned long j = 0; j < inModel->GetSubModelCount() && theResult == kErrorOK; j++)
        theResult = this->DrawModelHierarchy(inModel->GetSubModel(j), inScreen, inDetail, inOrthographic);
    GERasterizer::sTransformStack.pop_back();
    return theResult;
}

// FUNCTION: DX7HR 0x10004741
// Draws the sky model like any model; always returns kErrorOK
long DX7Rasterizer::DrawSky(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
{
    this->Draw(inModel, inScreen, inOpacity, inOrthographic);
    return kErrorOK;
}

// COMDATs this unit is the first (link order) to instantiate (vector<GERasterizer::TransformStack>, the std
// exceptions its _Xlen throws), then the template-static init chain head and atexit registrars:
// TEMPLATE: DX7HR 0x1000475f
// TEMPLATE: DX7HR 0x10004766
// TEMPLATE: DX7HR 0x10004792
// TEMPLATE: DX7HR 0x100047c6
// TEMPLATE: DX7HR 0x10004816
// TEMPLATE: DX7HR 0x10004877
// TEMPLATE: DX7HR 0x1000488b
// TEMPLATE: DX7HR 0x10004b28
// TEMPLATE: DX7HR 0x10004b3a
// TEMPLATE: DX7HR 0x10004b3e
// TEMPLATE: DX7HR 0x10004b61
// TEMPLATE: DX7HR 0x10004b7e
// TEMPLATE: DX7HR 0x10004bc5
// TEMPLATE: DX7HR 0x10004bd2
// TEMPLATE: DX7HR 0x10004c00
// TEMPLATE: DX7HR 0x10004c09
// TEMPLATE: DX7HR 0x10004c33
// TEMPLATE: DX7HR 0x10004c5e
// TEMPLATE: DX7HR 0x10004c80
// TEMPLATE: DX7HR 0x10004cab
// SYNTHETIC: DX7HR 0x10004cc2
// SYNTHETIC: DX7HR 0x10004cea
// SYNTHETIC: DX7HR 0x10004cf6
// SYNTHETIC: DX7HR 0x10004d02
// SYNTHETIC: DX7HR 0x10004d0e

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// SYNTHETIC: DX7HRTNL 0x100038a7
// SYNTHETIC: DX7HRTNL 0x100038b1
// SYNTHETIC: DX7HRTNL 0x100038c2
// SYNTHETIC: DX7HRTNL 0x100038ce
// SYNTHETIC: DX7HRTNL 0x100038d9
// SYNTHETIC: DX7HRTNL 0x100038e3
// SYNTHETIC: DX7HRTNL 0x100038f4
// SYNTHETIC: DX7HRTNL 0x10003900
// SYNTHETIC: DX7HRTNL 0x1000390b
// SYNTHETIC: DX7HRTNL 0x10003915
// SYNTHETIC: DX7HRTNL 0x10003926
// SYNTHETIC: DX7HRTNL 0x10003932
// SYNTHETIC: DX7HRTNL 0x1000393d
// SYNTHETIC: DX7HRTNL 0x10003947
// SYNTHETIC: DX7HRTNL 0x10003958
// SYNTHETIC: DX7HRTNL 0x10003964
// SYNTHETIC: DX7HRTNL 0x1000396f
// SYNTHETIC: DX7HRTNL 0x10003979
// SYNTHETIC: DX7HRTNL 0x1000398a
// SYNTHETIC: DX7HRTNL 0x10003996
// SYNTHETIC: DX7HRTNL 0x100039a1
// SYNTHETIC: DX7HRTNL 0x100039ab
// SYNTHETIC: DX7HRTNL 0x100039bc
// SYNTHETIC: DX7HRTNL 0x100039c8
// SYNTHETIC: DX7HRTNL 0x100039d3
// SYNTHETIC: DX7HRTNL 0x100039dd
// SYNTHETIC: DX7HRTNL 0x100039ee
// SYNTHETIC: DX7HRTNL 0x100039fa
// TEMPLATE: DX7HRTNL 0x10003a05
// TEMPLATE: DX7HRTNL 0x10003a28
// FUNCTION: DX7HRTNL 0x10003a82
// FUNCTION: DX7HRTNL 0x10003abf
// FUNCTION: DX7HRTNL 0x10003b32
// FUNCTION: DX7HRTNL 0x10003c01
// FUNCTION: DX7HRTNL 0x10003cca
// FUNCTION: DX7HRTNL 0x10003ecd
// FUNCTION: DX7HRTNL 0x10003f14
// FUNCTION: DX7HRTNL 0x1000432e
// FUNCTION: DX7HRTNL 0x100047ae
// TEMPLATE: DX7HRTNL 0x100047cc
// TEMPLATE: DX7HRTNL 0x100047d3
// TEMPLATE: DX7HRTNL 0x100047ff
// TEMPLATE: DX7HRTNL 0x10004833
// TEMPLATE: DX7HRTNL 0x10004883
// TEMPLATE: DX7HRTNL 0x100048f1
// TEMPLATE: DX7HRTNL 0x10004917
// TEMPLATE: DX7HRTNL 0x10004bb4
// TEMPLATE: DX7HRTNL 0x10004bc6
// TEMPLATE: DX7HRTNL 0x10004bca
// TEMPLATE: DX7HRTNL 0x10004bed
// TEMPLATE: DX7HRTNL 0x10004c0a
// TEMPLATE: DX7HRTNL 0x10004c51
// TEMPLATE: DX7HRTNL 0x10004c5e
// TEMPLATE: DX7HRTNL 0x10004c8c
// TEMPLATE: DX7HRTNL 0x10004c95
// TEMPLATE: DX7HRTNL 0x10004cbf
// TEMPLATE: DX7HRTNL 0x10004cea
// TEMPLATE: DX7HRTNL 0x10004d0c
// TEMPLATE: DX7HRTNL 0x10004d37
// SYNTHETIC: DX7HRTNL 0x10004d4e
// SYNTHETIC: DX7HRTNL 0x10004d76
// SYNTHETIC: DX7HRTNL 0x10004d82
// SYNTHETIC: DX7HRTNL 0x10004d8e
// SYNTHETIC: DX7HRTNL 0x10004d9a
// TEMPLATE: DX7HRTNL 0x100048e4
// TEMPLATE: DX7HRTNL 0x10004905
