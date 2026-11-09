// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7Rasterizer DrawModel.cpp (file name
// inferred: the third of the four DX7Rasterizer units in link order; it holds the 3D model draws)
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000383a..0x10004d1a: the U2D key init chains, DX7StandardVertexArray's implicit copy ctor and vector deleting
// dtor, the model vertex copies, Draw(GEPolygonArray), Draw(GEModel), Draw3DLine, DrawModelHierarchy and DrawSky
// (export order), the vector<GERasterizer::TransformStack> and std::exception COMDATs, then the template-static init
// chain head and its atexit registrars.
#include <windows.h>
#include "gx/GX.h"

#include "dx7/DX7Rasterizer.h"
#include "dx7/DX7PolygonArray.h"
#include "dx7/DX7StandardVertexArray.h"
#include "dx7/DX7Surface.h"
#include "dx7/DX7Screen.h"
#include "lle/GEMaterial.h"
#include "dx7/DX7DrawSeam.h"

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

// The model draws moved to Rust (rust/crates/ee_dx7/src/draw_model.rs, docs/RUST_PLAN.md R4.2). The C++ here forwards;
// the ee_dx7cpp_* below read the GE objects for it. The original's CopyStd*/CopyAndSmooth* (0x10003a15..0x10003c5d) are
// the Rust blend_*/white_copy; the transform stack became a Rust call parameter (only DrawModelHierarchy used it).

extern "C" {
void ee_dx7cpp_model_info(void* inModel, EeDx7ModelInfo* outInfo)
{
    GEModel* theModel = static_cast<GEModel*>(inModel);
    outInfo->material_count = theModel->GetMaterialCount();
    outInfo->lod_count = theModel->GetLODCount();
    outInfo->sub_model_count = theModel->GetSubModelCount();
    outInfo->current_frame = theModel->GetGraphicsCurrentFrame();
    outInfo->next_frame = theModel->GetGraphicsNextFrame();
    outInfo->vertex_type = theModel->GetVertexArray(0)->GetType();
    outInfo->scale = theModel->GetGraphicsScale();
    outInfo->opacity = theModel->GetModelOpacity();
    outInfo->fraction = theModel->GetFractionalFramePosition();
    DX7CopyTransform(theModel->GetGraphicsTransform(), outInfo->transform);
}

void ee_dx7cpp_model_visible(void* inRasterizer, unsigned long inScreen, void* inModel, float inScale, const float inTransform[4][4], bool inOrthographic,
                             bool* outVisible, bool* outClipped)
{
    DX7DrawAccess* theRasterizer = DX7Access(inRasterizer);
    GEViewport* theViewport = &theRasterizer->mScreenStates[inScreen]->mViewport;
    GETransformation theTransformation(inTransform);
    bool theVisible;
    bool theClipped;
    if (!inOrthographic)
        theRasterizer->IsPerspectiveModelVisible(static_cast<GEModel*>(inModel), inScale, theViewport, theTransformation, theVisible, theClipped);
    else
        theRasterizer->IsOrthographicModelVisible(static_cast<GEModel*>(inModel), theViewport, theTransformation, theVisible, theClipped);
    *outVisible = theVisible;
    *outClipped = theClipped;
}

float ee_dx7cpp_pixel_size(void* inRasterizer, unsigned long inScreen, void* inModel, const float inTransform[4][4], float inScale, bool inOrthographic)
{
    DX7DrawAccess* theRasterizer = DX7Access(inRasterizer);
    GEViewport* theViewport = &theRasterizer->mScreenStates[inScreen]->mViewport;
    GETransformation theTransformation(inTransform);
    return theRasterizer->GetPixelSize(static_cast<GEModel*>(inModel), theViewport, theTransformation, inScale, inOrthographic);
}

float ee_dx7cpp_view_width(void* inRasterizer, unsigned long inScreen)
{
    return DX7Access(inRasterizer)->mScreenStates[inScreen]->mViewport.GetViewRectangle().GetWidth();
}

void ee_dx7cpp_material_info(void* inModel, unsigned long inScreen, unsigned long inIndex, unsigned long inLOD, EeDx7MaterialInfo* outInfo)
{
    GEMaterial* theMaterial = static_cast<GEModel*>(inModel)->GetMaterial(inIndex);
    outInfo->texture = theMaterial->GetTexture(inScreen);
    outInfo->polygon_start = theMaterial->GetPolygonStartingIndex(inLOD);
    outInfo->polygon_length = theMaterial->GetPolygonRangeLength(inLOD);
    outInfo->vertex_start = theMaterial->GetVertexStartingIndex();
    outInfo->vertex_length = theMaterial->GetVertexRangeLength();
    outInfo->opacity = theMaterial->GetOpacity();
    outInfo->color[0] = theMaterial->GetColor().GetRed();
    outInfo->color[1] = theMaterial->GetColor().GetGreen();
    outInfo->color[2] = theMaterial->GetColor().GetBlue();
}

const unsigned short* ee_dx7cpp_polygon_indices(void* inModel, unsigned long inLOD)
{
    return static_cast<DX7PolygonArray*>(static_cast<GEModel*>(inModel)->GetPolygonArray(inLOD))->mIndices;
}

const void* ee_dx7cpp_frame_vertices(void* inModel, unsigned long inFrame, unsigned long inVertexType)
{
    GEVertexArray* theArray = static_cast<GEModel*>(inModel)->GetVertexArray(inFrame);
    if (inVertexType == 0)
        return static_cast<DX7StandardVertexArray*>(theArray)->mVertices;
    return static_cast<DX7StdColorVertexArray*>(theArray)->mVertices;
}

void* ee_dx7cpp_sub_model(void* inModel, unsigned long inIndex) { return static_cast<GEModel*>(inModel)->GetSubModel(inIndex); }
}

// FUNCTION: DX7HR 0x10003c5d
// Draws an indexed triangle list with an identity world transform and D3D clipping on; counts the triangles drawn
long DX7Rasterizer::Draw(GEPolygonArray& inPolygons, unsigned long inCount, GEVertexArray& inVertices,
                         unsigned long inVertexCount, GETexture* inTexture, unsigned long inScreen, bool inSorted)
{
    unsigned long theType = inVertices.GetType();
    const void* theVertices = theType == 1 ? static_cast<const void*>(((DX7StdColorVertexArray&)inVertices).mVertices)
                                           : static_cast<const void*>(((DX7StandardVertexArray&)inVertices).mVertices);
    return ee_dx7r_draw_polygon_array(this, inScreen, DX7DrawState(this, inScreen), ((DX7PolygonArray&)inPolygons).mIndices,
                                      inCount, theType, theVertices, inVertexCount, inTexture);
}

// FUNCTION: DX7HR 0x10003e60
// Draws a model and its sub-models under the drawing lock (the float argument becomes the level-of-detail divisor)
long DX7Rasterizer::Draw(GEModel* inModel, unsigned long inScreen, float inOpacity, bool inOrthographic)
{
    return ee_dx7r_draw_model(this, inScreen, DX7DrawState(this, inScreen), inModel, inOpacity, inOrthographic);
}

// FUNCTION: DX7HR 0x10003ea7
// Draws a textured polyline (at most 1200 segments) as a red line list in camera space, advancing the texture V
// along it (without the drawing lock)
long DX7Rasterizer::Draw3DLine(std::vector<std::pair<GE3DPoint, float> >& inPoints, GETexture* inTexture,
                               float inV, unsigned long inScreen, bool inOrthographic)
{
    static_assert(sizeof(std::pair<GE3DPoint, float>) == 16, "ee_dx7/src/draw_model.rs draw_3d_line: (x, y, z, advance)");
    EeDx7ViewportInfo theViewport = DX7ViewportInfoOf(this->mScreenStates[inScreen]->mViewport);
    return ee_dx7r_draw_3d_line(this, inScreen, DX7DrawState(this, inScreen), reinterpret_cast<const float(*)[4]>(inPoints.data()),
                                inPoints.size(), inTexture, inV, inOrthographic, &theViewport);
}

// FUNCTION: DX7HR 0x100042c1
// Draws a model and recursively its sub-models (no parent: the top of a hierarchy); stops at the first sub-model error
long DX7Rasterizer::DrawModelHierarchy(GEModel* inModel, unsigned long inScreen, float inDetail, bool inOrthographic)
{
    return ee_dx7r_draw_model_hierarchy(this, inScreen, DX7DrawState(this, inScreen), inModel, inDetail, inOrthographic);
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
