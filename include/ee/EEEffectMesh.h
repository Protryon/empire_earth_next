// New (R4.4): the effects' meshes built in Rust (rust/crates/ee_render/src/gfx mesh.rs: the farm's, the volcano's) stored into a
// model's vertex and polygon arrays. Each record says which values its builder set; only those are stored (the
// builders' later writes to a slot already won in Rust, as they did in the arrays). Requires lle/GEModel.h,
// lle/GEVertexArray.h and lle/GEPolygonArray.h.
#ifndef EE_EEEFFECTMESH_H
#define EE_EEEFFECTMESH_H

#include <vector>
#include "rust/render/gfx.h"

// A built mesh's set records stored into inModel's first vertex and polygon arrays (EEEffectMesh::Store; the raster
// table's store_mesh)
inline void EEStoreMesh(GEModel* inModel, const EeGfxMeshVertex* inVertices, size_t inVertexCount,
                        const EeGfxMeshTriangle* inTriangles, size_t inTriangleCount)
{
    GEVertexArray* theVertices = inModel->GetVertexArray(0);
    for (unsigned long i = 0; i < inVertexCount; i++) {
        const EeGfxMeshVertex& v = inVertices[i];
        if (v.set & EE_GFX_MESH_POINT)
            theVertices->SetPoint(GE3DPoint(v.point.x, v.point.y, v.point.z), i);
        if (v.set & EE_GFX_MESH_NORMAL)
            theVertices->SetNormal(GEVector(v.normal.x, v.normal.y, v.normal.z), i);
        if (v.set & EE_GFX_MESH_COORD)
            theVertices->SetTextureCoordinate(GETextureCoordinate(v.coord.u, v.coord.v), i);
        if (v.set & EE_GFX_MESH_COLOR)
            theVertices->SetColor(GEColor(v.color.red, v.color.green, v.color.blue), i);
        if (v.set & EE_GFX_MESH_OPACITY)
            theVertices->SetOpacity(v.opacity, i);
    }
    GEPolygonArray* thePolygons = inModel->GetPolygonArray(0);
    for (unsigned long i = 0; i < inTriangleCount; i++) {
        const EeGfxMeshTriangle& t = inTriangles[i];
        if (t.set) {
            thePolygons->SetVertexIndex1(i, t.v[0]);
            thePolygons->SetVertexIndex2(i, t.v[1]);
            thePolygons->SetVertexIndex3(i, t.v[2]);
        }
    }
}

// A mesh of inVertices vertices and inTriangles triangles, every record unset.
struct EEEffectMesh {
    EEEffectMesh(unsigned long inVertices, unsigned long inTriangles)
        : mVertices(inVertices, EeGfxMeshVertex()), mTriangles(inTriangles, EeGfxMeshTriangle()) {}
    // the records set into inModel's first vertex and polygon arrays
    void Store(GEModel* inModel) const
    {
        EEStoreMesh(inModel, this->mVertices.empty() ? NULL : &this->mVertices[0], this->mVertices.size(),
                    this->mTriangles.empty() ? NULL : &this->mTriangles[0], this->mTriangles.size());
    }
    EeGfxMeshVertex* Vertices() { return this->mVertices.empty() ? NULL : &this->mVertices[0]; }
    EeGfxMeshTriangle* Triangles() { return this->mTriangles.empty() ? NULL : &this->mTriangles[0]; }

    std::vector<EeGfxMeshVertex> mVertices;
    std::vector<EeGfxMeshTriangle> mTriangles;
};

#endif
