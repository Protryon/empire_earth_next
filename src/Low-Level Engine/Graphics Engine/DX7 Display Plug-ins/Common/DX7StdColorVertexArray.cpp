// C:\EEX\Low-Level Engine\Graphics Engine\DX7 Display Plug-ins\Common\DX7StdColorVertexArray.cpp
// Unit flags (hypothesis, docs/CALIBRATION.md "dx7hr"): -O1 -G6 -MD -GR
//
// 0x1000e96c..0x1000eced. No init chains. GetSize at 0x1000eadf is the ICF-kept body DX7StandardVertexArray's
// marker claims. Colors are packed with d3dtypes.h's RGBA_* macros; float -> int is UMFloatToLong (fld/fistp).
#include <windows.h>
#include "gx/GX.h"

#include "lle/LowLevelEngine.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "dx7/DX7StdColorVertexArray.h"

// unit-local (.rdata 0x1001067c, after the shared constant pool; GetColor multiplies by it from memory, which a
// literal would hoist into st0) (provisional name).
static const float kScale = 1.0f / 255.0f;

// FUNCTION: DX7HR 0x1000e96c
// Creates a vertex array of the given number of coloured vertices (position, normal, packed colour, texture coordinate)
DX7StdColorVertexArray::DX7StdColorVertexArray(unsigned long inSize)
{
    this->mVertices = new GXVERTEX_STD_COLOR[inSize];
    this->mSize = inSize;
}

// FUNCTION: DX7HR 0x1000e99a
// Destroys the vertex array, freeing its vertices
DX7StdColorVertexArray::~DX7StdColorVertexArray()
{
    delete[] this->mVertices;
}

// FUNCTION: DX7HR 0x1000e9b6
// Copies the vertices of another coloured vertex array, as many as both arrays hold
void DX7StdColorVertexArray::CopyFrom(const GEVertexArray& inArray)
{
    const DX7StdColorVertexArray& theArray = (const DX7StdColorVertexArray&)inArray;
    memcpy(this->mVertices, theArray.mVertices, min(this->mSize, theArray.mSize) * sizeof(GXVERTEX_STD_COLOR));
}

// FUNCTION: DX7HR 0x1000e9e0
// Returns the colour of the given vertex, unpacking its red, green and blue bytes to 0..1
void DX7StdColorVertexArray::GetColor(GEColor& outColor, unsigned long inIndex) const
{
    outColor.SetRed((float)GXRGBA_GETRED(this->mVertices[inIndex].color) * kScale);
    outColor.SetGreen((float)GXRGBA_GETGREEN(this->mVertices[inIndex].color) * kScale);
    outColor.SetBlue((float)GXRGBA_GETBLUE(this->mVertices[inIndex].color) * kScale);
}

// FUNCTION: DX7HR 0x1000ea57
// Returns the normal of the given vertex
void DX7StdColorVertexArray::GetNormal(GEVector& outNormal, unsigned long inIndex) const
{
    outNormal.SetX(this->mVertices[inIndex].nx);
    outNormal.SetY(this->mVertices[inIndex].ny);
    outNormal.SetZ(this->mVertices[inIndex].nz);
}

// FUNCTION: DX7HR 0x1000ea87
// Returns the opacity of the given vertex, from its alpha byte (0..1)
float DX7StdColorVertexArray::GetOpacity(unsigned long inIndex) const
{
    return (float)GXRGBA_GETALPHA(this->mVertices[inIndex].color) * kScale;
}

// FUNCTION: DX7HR 0x1000eab0
// Returns the position of the given vertex
void DX7StdColorVertexArray::GetPoint(GE3DPoint& outPoint, unsigned long inIndex) const
{
    outPoint.SetX(this->mVertices[inIndex].x);
    outPoint.SetY(this->mVertices[inIndex].y);
    outPoint.SetZ(this->mVertices[inIndex].z);
}

unsigned long DX7StdColorVertexArray::GetSize() const
{
    return this->mSize;
}

// FUNCTION: DX7HR 0x1000eae3
// Returns the texture coordinate of the given vertex
void DX7StdColorVertexArray::GetTextureCoordinate(GETextureCoordinate& outCoordinate, unsigned long inIndex) const
{
    outCoordinate.SetU(this->mVertices[inIndex].tu);
    outCoordinate.SetV(this->mVertices[inIndex].tv);
}

// FUNCTION: DX7HR 0x1000eb07
// Returns vertex array type 1 (with colour)
unsigned long DX7StdColorVertexArray::GetType() const
{
    return 1;
}

// FUNCTION: DX7HR 0x1000eb0b
// Sets the red, green and blue of the given vertex (0..1 scaled to bytes, unclamped), keeping its alpha
void DX7StdColorVertexArray::SetColor(const GEColor& inColor, unsigned long inIndex)
{
    long theRed = UMFloatToLong(inColor.GetRed() * 255.0f);
    long theGreen = UMFloatToLong(inColor.GetGreen() * 255.0f);
    long theBlue = UMFloatToLong(inColor.GetBlue() * 255.0f);
    this->mVertices[inIndex].color = (this->mVertices[inIndex].color & 0xff000000) | GXRGB_MAKE(theRed, theGreen, theBlue);
}

// FUNCTION: DX7HR 0x1000eb77
// Sets the normal of the given vertex
void DX7StdColorVertexArray::SetNormal(const GEVector& inNormal, unsigned long inIndex)
{
    this->mVertices[inIndex].nx = inNormal.GetX();
    this->mVertices[inIndex].ny = inNormal.GetY();
    this->mVertices[inIndex].nz = inNormal.GetZ();
}

// FUNCTION: DX7HR 0x1000eba7
// Sets the alpha byte of the given vertex from an opacity of 0..1 (unclamped), keeping its colour
void DX7StdColorVertexArray::SetOpacity(float inOpacity, unsigned long inIndex)
{
    long theAlpha = UMFloatToLong(inOpacity * 255.0f);
    this->mVertices[inIndex].color = GXRGBA_SETALPHA(this->mVertices[inIndex].color, theAlpha);
}

// FUNCTION: DX7HR 0x1000ebe0
// Sets the position of the given vertex
void DX7StdColorVertexArray::SetPoint(const GE3DPoint& inPoint, unsigned long inIndex)
{
    this->mVertices[inIndex].x = inPoint.GetX();
    this->mVertices[inIndex].y = inPoint.GetY();
    this->mVertices[inIndex].z = inPoint.GetZ();
}

// FUNCTION: DX7HR 0x1000ec0f
// Sets the texture coordinate of the given vertex
void DX7StdColorVertexArray::SetTextureCoordinate(const GETextureCoordinate& inCoordinate, unsigned long inIndex)
{
    this->mVertices[inIndex].tu = inCoordinate.GetU();
    this->mVertices[inIndex].tv = inCoordinate.GetV();
}

// FUNCTION: DX7HR 0x1000ec35
// Sets every field of the given vertex: position, normal, colour with opacity as alpha, texture coordinate
void DX7StdColorVertexArray::SetVertex(const GEColor& inColor, const GE3DPoint& inPoint, const GEVector& inNormal,
                                       const GETextureCoordinate& inCoordinate, float inOpacity,
                                       unsigned long inIndex)
{
    GXVERTEX_STD_COLOR& theVertex = this->mVertices[inIndex];
    theVertex.x = inPoint.GetX();
    theVertex.y = inPoint.GetY();
    theVertex.z = inPoint.GetZ();
    theVertex.nx = inNormal.GetX();
    theVertex.ny = inNormal.GetY();
    theVertex.nz = inNormal.GetZ();
    theVertex.color = GXRGBA(UMFloatToLong(inColor.GetRed() * 255.0f), UMFloatToLong(inColor.GetGreen() * 255.0f),
                                UMFloatToLong(inColor.GetBlue() * 255.0f), UMFloatToLong(inOpacity * 255.0f));
    theVertex.tu = inCoordinate.GetU();
    theVertex.tv = inCoordinate.GetV();
}

// FUNCTION: DX7HR 0x1000ecdf
// Returns the array's size in bytes: the vertex data plus 8
unsigned long DX7StdColorVertexArray::SizeOf() const
{
    return this->mSize * sizeof(GXVERTEX_STD_COLOR) + 8;
}

// DX7HRTnLDisplay.dll (the T&L twin builds this unit unchanged): the same functions at their twin addresses.
// FUNCTION: DX7HRTNL 0x1000e9f1
// FUNCTION: DX7HRTNL 0x1000ea1f
// FUNCTION: DX7HRTNL 0x1000ea3b
// FUNCTION: DX7HRTNL 0x1000ea65
// FUNCTION: DX7HRTNL 0x1000eadc
// FUNCTION: DX7HRTNL 0x1000eb0c
// FUNCTION: DX7HRTNL 0x1000eb35
// FUNCTION: DX7HRTNL 0x1000eb68
// FUNCTION: DX7HRTNL 0x1000eb8c
// FUNCTION: DX7HRTNL 0x1000eb90
// FUNCTION: DX7HRTNL 0x1000ebfc
// FUNCTION: DX7HRTNL 0x1000ec2c
// FUNCTION: DX7HRTNL 0x1000ec65
// FUNCTION: DX7HRTNL 0x1000ec94
// FUNCTION: DX7HRTNL 0x1000ecba
// FUNCTION: DX7HRTNL 0x1000ed64
