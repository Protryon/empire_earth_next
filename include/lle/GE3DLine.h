// C:\EEX\Low-Level Engine\Graphics Engine\GE3DLine.h (RECONSTRUCTION). Requires GE3DPoint.h, GEVector.h.
#ifndef LLE_GE3DLINE_H
#define LLE_GE3DLINE_H

class FSFile;
class GE3DPlane;

class GE3DLine {                    // sizeof 0x18
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
    // within inTolerance of a point (outT: the nearer of the two crossing parameters); two lines meeting (within 0.05
    // in the third axis; the parameters on each); a plane (false when parallel)
    bool Intersects(const GE3DPoint& inPoint, float inTolerance, float& outT) const
        { return ee_ge_line_intersects_point(GERust(*this), GERust(inPoint), inTolerance, &outT); }
    bool Intersects(const GE3DLine& inLine, float& outT, float& outLineT) const
        { return ee_ge_line_intersects_line(GERust(*this), GERust(inLine), &outT, &outLineT); }
    bool Intersects(const GE3DPlane& inPlane, float& outT) const
        { return ee_ge_line_intersects_plane(GERust(*this), GERust(inPlane), &outT); }
    template <class F> long Load(F& inFile)
    {
        long theResult = this->mStart.Load(inFile);
        return theResult == kErrorOK ? this->mDirection.Load(inFile) : theResult;
    }
    template <class F> long Save(F& inFile) const
    {
        long theResult = this->mStart.Save(inFile);
        return theResult == kErrorOK ? this->mDirection.Save(inFile) : theResult;
    }
    void SetDirection(const GEVector& inDirection) { this->mDirection = inDirection; }
    void SetStart(const GE3DPoint& inStart) { this->mStart = inStart; }
    // the parameter (and the point) where the line reaches a given x, y or z; false when it runs parallel to it
    bool SolveForX(float inX, float& outT) const { return ee_ge_line_solve(GERust(*this), 0, inX, &outT, NULL); }
    bool SolveForX(float inX, float& outT, GE3DPoint& outPoint) const
        { return ee_ge_line_solve(GERust(*this), 0, inX, &outT, GERust(outPoint)); }
    bool SolveForY(float inY, float& outT) const { return ee_ge_line_solve(GERust(*this), 1, inY, &outT, NULL); }
    bool SolveForY(float inY, float& outT, GE3DPoint& outPoint) const
        { return ee_ge_line_solve(GERust(*this), 1, inY, &outT, GERust(outPoint)); }
    bool SolveForZ(float inZ, float& outT) const { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, NULL); }
    bool SolveForZ(float inZ, float& outT, GE3DPoint& outPoint) const
        { return ee_ge_line_solve(GERust(*this), 2, inZ, &outT, GERust(outPoint)); }
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
