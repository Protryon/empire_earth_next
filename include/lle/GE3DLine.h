// C:\EEX\Low-Level Engine\Graphics Engine\GE3DLine.h (RECONSTRUCTION). Requires GE3DPoint.h, GEVector.h.
#ifndef LLE_GE3DLINE_H
#define LLE_GE3DLINE_H

class FSFile;
class GE3DPlane;

class LLE_API GE3DLine {                    // sizeof 0x18
public:
    GE3DLine() : mDirection(1.0f, 0.0f, 0.0f) {}   // inline (refused at every -O1 call site, 0x1000c5ba; a no-/Og caller still spends an inline-`this` slot on it: GEVector::GetQuantizedVersion)
    GE3DLine(const GE3DPoint& inStart, const GE3DPoint& inEnd) { this->mStart = inStart; this->ComputeDirection(inEnd); }
    GE3DLine(const GE3DPoint& inStart, const GEVector& inDirection) { this->mStart = inStart; this->mDirection = inDirection; }
    ~GE3DLine() {}
    void Define(const GE3DPoint& inStart, const GE3DPoint& inEnd) { this->mStart = inStart; this->ComputeDirection(inEnd); }
    void Define(const GE3DPoint& inStart, const GEVector& inDirection) { this->mStart = inStart; this->mDirection = inDirection; }
    const GEVector& GetDirection() const { return this->mDirection; }
    void GetPointAt(float inT, GE3DPoint& outPoint) const
    {
        outPoint.mX = this->mDirection.GetX() * inT + this->mStart.GetX();
        outPoint.mY = this->mDirection.GetY() * inT + this->mStart.GetY();
        outPoint.mZ = this->mDirection.GetZ() * inT + this->mStart.GetZ();
    }
    const GE3DPoint& GetStart() const { return this->mStart; }
    bool Intersects(const GE3DPoint& inPoint, float inTolerance, float& outT) const;
    bool Intersects(const GE3DLine& inLine, float& outT, float& outLineT) const;
    bool Intersects(const GE3DPlane& inPlane, float& outT) const;
    bool IntersectsIn2D(const GE3DLine& inLine, float& outT, float& outLineT) const;
    long Load(FSFile& inFile);
    long Save(FSFile& inFile) const;
    void SetDirection(const GEVector& inDirection) { this->mDirection = inDirection; }
    void SetStart(const GE3DPoint& inStart) { this->mStart = inStart; }
    bool SolveForX(float inX, float& outT) const;
    bool SolveForX(float inX, float& outT, GE3DPoint& outPoint) const;
    bool SolveForY(float inY, float& outT) const;
    bool SolveForY(float inY, float& outT, GE3DPoint& outPoint) const;
    bool SolveForZ(float inZ, float& outT) const;
    bool SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const;
private:
    friend struct GERustLayout;             // the Rust mirror's layout check (Graphics Engine/GERustMath.h)
    void ComputeDirection(const GE3DPoint& inEnd)
    {
        this->mDirection.mX = inEnd.GetX() - this->mStart.GetX();
        this->mDirection.mY = inEnd.GetY() - this->mStart.GetY();
        this->mDirection.mZ = inEnd.GetZ() - this->mStart.GetZ();
    }

    GE3DPoint mStart;                       // +0x00
    GEVector mDirection;                    // +0x0c
};

#endif
