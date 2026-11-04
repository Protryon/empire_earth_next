// C:\EEX\Low-Level Engine\Graphics Engine\GEVector.h (RECONSTRUCTION). GEVector.cpp (__FILE__ anchor, ~0x10036755)
// holds the quantized-vector code; the basic operations are emitted in the GE2DPoint.cpp block (0x1000c240..).
#ifndef LLE_GEVECTOR_H
#define LLE_GEVECTOR_H

class UString;
class FSFile;

class LLE_API GEVector {                    // proven sizeof 0xc (EEClumps: +0x08..+0x14, three fldz)
public:
#ifdef LLE_EXE_HEADERS
    GEVector() : mX(0.0f), mY(0.0f), mZ(0.0f) {}
#else
    // through the inline setters (not a free inline): QuantizedVectorArray's member-array ctor 0x10035ab0 calls the
    // out-of-line GEVector() 0x2766 times; the init-list form expands there (the exe view keeps the init list: the
    // setter form costs exe 0x4b0502)
    GEVector() { this->SetX(0.0f); this->SetY(0.0f); this->SetZ(0.0f); }
#endif
    GEVector(float inX, float inY, float inZ) : mX(inX), mY(inY), mZ(inZ) {}
    // user-defined (exported ICF pair 0x1000d588): GE3DLine's copy ctor 0xc725 copies its GEVector with three
    // movs where its trivially-copied GE3DPoint gets movsd
    GEVector(const GEVector& inVector) : mX(inVector.mX), mY(inVector.mY), mZ(inVector.mZ) {}
    ~GEVector() {}
    GEVector& operator=(const GEVector& inVector) { this->mX = inVector.mX; this->mY = inVector.mY; this->mZ = inVector.mZ; return *this; }
    GEVector operator-() const;
#ifdef LLE_EXE_HEADERS
    // inline in the exe's view: the exe never imports ??YGEVector@@QAEAAV0@ABV0@@Z and expands it (fadd x, y, z:
    // EEMap::Fn11D5E8 0x51d831)
    GEVector& operator+=(const GEVector& inVector) { this->mX += inVector.mX; this->mY += inVector.mY; this->mZ += inVector.mZ; return *this; }
#else
    GEVector& operator+=(const GEVector& inVector);
#endif
    GEVector& operator-=(const GEVector& inVector);
    bool operator==(const GEVector& inVector) const;
    bool operator!=(const GEVector& inVector) const;
    void Add(const GEVector& inVector, GEVector& outVector) const;
    void CrossProduct(const GEVector& inVector, GEVector& outVector) const;
    // inline (GE3DPlane::IsPointOnPlane 0xd4c3 expands it; the plug-ins never call it)
    float DotProduct(const GEVector& inVector) const { return inVector.mZ * this->mZ + inVector.mY * this->mY + inVector.mX * this->mX; }
    float GetMagnitude() const;
    float GetMagnitudeSquared() const;
    float GetX() const { return this->mX; }  // inline (exported, ICF-folded with GE2DPoint's)
    float GetY() const { return this->mY; }
    float GetZ() const { return this->mZ; }
    // GEVector.cpp (0x35f0e..0x367fc): the vector's cube-face quantization (sQuantizedVectorArray)
    GEVector* GetQuantizedVersion() const;
    static long LoadQuantizedIndex(FSFile& inFile, GEVector*& outVector);
    void Normalize()
    {
        float theMagnitude = this->GetMagnitude();
        if (theMagnitude >= -0.0001f && theMagnitude <= 0.0001f) {
            this->mX = 0.0f;
            this->mY = 0.0f;
            this->mZ = 0.0f;
        } else {
            float theScale = 1.0f / theMagnitude;
            this->mX *= theScale;
            this->mY *= theScale;
            this->mZ *= theScale;
        }
    }
    // inline (no import in Default.dll; LLE's exported body 0xc3d4 is this expression; 0xc29f keeps the scale in
    // st0 across the three products, its tail-merged branches load only the scale)
    void Scale(float inScale, GEVector& outVector) const
    {
        outVector.mX = inScale * this->mX;
        outVector.mY = inScale * this->mY;
        outVector.mZ = inScale * this->mZ;
    }
    void SetX(float inX) { this->mX = inX; }  // inline (exported 0x54ae4 / 0xc56c / 0xc576)
    void SetY(float inY) { this->mY = inY; }
    void SetZ(float inZ) { this->mZ = inZ; }
    long SaveQuantizedIndex(FSFile& inFile) const;
    void Subtract(const GEVector& inVector, GEVector& outVector) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;

    float mX;                               // +0x00
    float mY;                               // +0x04
    float mZ;                               // +0x08

    // Inferred GEVector.cpp-only visibility (same scheme as GEWindow.h): every static data member a unit sees
    // takes a VC6 $E number, and every engine unit includes this header (their $E binds were recorded without these).
    // Exported ?sVectorXStringID@GEVector@@0VUString@@B etc.; QuantizedVectorArray is defined in GEVector.cpp.
#ifdef LOWLEVELENGINE_EXPORTS
private:
    struct QuantizedVectorArray;

    static const UString sVectorXStringID;              // 0x1012d888
    static const UString sVectorYStringID;              // 0x1012d870
    static const UString sVectorZStringID;              // 0x1012d860
    static const UString sQuantizedVectorIndexStringID; // 0x1012d850
    static QuantizedVectorArray sQuantizedVectorArray;  // 0x1010ff70
#endif
};

#endif
