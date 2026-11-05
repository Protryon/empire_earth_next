// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7StandardVertexArray.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000e793..0x1000e96c. No init chains. GetSize is ICF-folded with DX7StdColorVertexArray's (0x1000eadf), and
// SetColor with SetOpacity (0x1000e89e, `ret 8`).
#include <windows.h>
#include "gx/GX.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "dx7/DX7StandardVertexArray.h"

// FUNCTION: DX7HR 0x1000e793
// Creates a vertex array of the given number of untinted vertices (position, normal, one texture coordinate)
DX7StandardVertexArray::DX7StandardVertexArray(unsigned long inSize)
{
    this->mVertices = new GXVERTEX[inSize];
    this->mSize = inSize;
}

// FUNCTION: DX7HR 0x1000e7c0
// Destroys the vertex array, freeing its vertices
DX7StandardVertexArray::~DX7StandardVertexArray()
{
    delete[] this->mVertices;
}

// FUNCTION: DX7HR 0x1000e7dc
// Copies the vertices of another standard vertex array, as many as both arrays hold
void DX7StandardVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    const DX7StandardVertexArray& theArray = (const DX7StandardVertexArray&)inArray;
    memcpy(this->mVertices, theArray.mVertices, min(this->mSize, theArray.mSize) * sizeof(GXVERTEX));
}

// FUNCTION: DX7HR 0x1000e803
// Returns white for any vertex: this array keeps no colours (red, green and blue set to 1)
void DX7StandardVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const
{
    outColor.SetRed(1.0f);
    outColor.SetGreen(1.0f);
    outColor.SetBlue(1.0f);
}

// FUNCTION: DX7HR 0x1000e818
// Returns the normal of the given vertex
void DX7StandardVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const
{
    outNormal.SetX(this->mVertices[inIndex].nx);
    outNormal.SetY(this->mVertices[inIndex].ny);
    outNormal.SetZ(this->mVertices[inIndex].nz);
}

// FUNCTION: DX7HR 0x1000e845
// Returns full opacity for any vertex: this array keeps no opacity
float DX7StandardVertexArray::GetOpacity(unsigned long inIndex) const
{
    return 1.0f;
}

// FUNCTION: DX7HR 0x1000e84e
// Returns the position of the given vertex
void DX7StandardVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const
{
    outPoint.SetX(this->mVertices[inIndex].x);
    outPoint.SetY(this->mVertices[inIndex].y);
    outPoint.SetZ(this->mVertices[inIndex].z);
}

// FUNCTION: DX7HR 0x1000eadf
// Returns the number of vertices (one body shared with the coloured vertex array)
unsigned long DX7StandardVertexArray::GetSize() const
{
    return this->mSize;
}

// FUNCTION: DX7HR 0x1000e87a
// Returns the texture coordinate of the given vertex
void DX7StandardVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    outCoordinate.SetU(this->mVertices[inIndex].tu);
    outCoordinate.SetV(this->mVertices[inIndex].tv);
}

// FUNCTION: DX7HR 0x1000e89b
// Returns vertex array type 0 (standard, no colour)
unsigned long DX7StandardVertexArray::GetType() const
{
    return 0;
}

// FUNCTION: DX7HR 0x1000e89e
// Ignores the colour: this array keeps no colours (one body shared with SetOpacity)
void DX7StandardVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex)
{
}

// FUNCTION: DX7HR 0x1000e8a1
// Sets the normal of the given vertex
void DX7StandardVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex)
{
    this->mVertices[inIndex].nx = inNormal.GetX();
    this->mVertices[inIndex].ny = inNormal.GetY();
    this->mVertices[inIndex].nz = inNormal.GetZ();
}

void DX7StandardVertexArray::SetOpacity(float inOpacity, unsigned long inIndex)
{
}

// FUNCTION: DX7HR 0x1000e8ce
// Sets the position of the given vertex
void DX7StandardVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex)
{
    this->mVertices[inIndex].x = inPoint.GetX();
    this->mVertices[inIndex].y = inPoint.GetY();
    this->mVertices[inIndex].z = inPoint.GetZ();
}

// FUNCTION: DX7HR 0x1000e8fa
// Sets the texture coordinate of the given vertex
void DX7StandardVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    this->mVertices[inIndex].tu = inCoordinate.GetU();
    this->mVertices[inIndex].tv = inCoordinate.GetV();
}

// FUNCTION: DX7HR 0x1000e91d
// Sets the position, normal and texture coordinate of the given vertex; colour and opacity are ignored
void DX7StandardVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float inOpacity,
                                       unsigned long inIndex)
{
    GXVERTEX& theVertex = this->mVertices[inIndex];
    theVertex.x = inPoint.GetX();
    theVertex.y = inPoint.GetY();
    theVertex.z = inPoint.GetZ();
    theVertex.nx = inNormal.GetX();
    theVertex.ny = inNormal.GetY();
    theVertex.nz = inNormal.GetZ();
    theVertex.tu = inCoordinate.GetU();
    theVertex.tv = inCoordinate.GetV();
}

// FUNCTION: DX7HR 0x1000e962
// Returns the array's size in bytes: the vertex data plus 8
unsigned long DX7StandardVertexArray::SizeOf() const
{
    return this->mSize * sizeof(GXVERTEX) + 8;
}

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// FUNCTION: DX7HRTNL 0x1000e818
// FUNCTION: DX7HRTNL 0x1000e845
// FUNCTION: DX7HRTNL 0x1000e861
// FUNCTION: DX7HRTNL 0x1000e888
// FUNCTION: DX7HRTNL 0x1000e89d
// FUNCTION: DX7HRTNL 0x1000e8ca
// FUNCTION: DX7HRTNL 0x1000e8d3
// FUNCTION: DX7HRTNL 0x1000eb64
// FUNCTION: DX7HRTNL 0x1000e8ff
// FUNCTION: DX7HRTNL 0x1000e920
// FUNCTION: DX7HRTNL 0x1000e923
// FUNCTION: DX7HRTNL 0x1000e926
// FUNCTION: DX7HRTNL 0x1000e953
// FUNCTION: DX7HRTNL 0x1000e97f
// FUNCTION: DX7HRTNL 0x1000e9a2
// FUNCTION: DX7HRTNL 0x1000e9e7
