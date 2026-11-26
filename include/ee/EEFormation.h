// C:\EEX\Empire Earth\Formations\EEFormation.h (RECONSTRUCTION; file name inferred). A group formation (RTTI
// EEFormation > UReferenceCounter; vtable 0x43f838). Names are provisional. Requires lle/LowLevelEngine.h.
//
// New (the units' lane, round 2): one concrete class. The original's eight subclasses (EEFShortLine, EEFLongLine,
// EEFLooseLine, EEFStaggeredLine > EEFLine; EEFDeepWedge, EEFShallowWedge > EEFWedge; EEFInwardCircle,
// EEFOutwardCircle > EEFCircle) only gave a type its constructor's members (a line's offsets, a wedge's arms, a
// circle's mode and arc): those are Rust's (rust/crates/ee_sim/src/formations, capture.rs shape_of), and Place is one
// call there by type. What a caller sets stays here: the frame and a circle's minimum radius.
#ifndef EE_EEFORMATION_H
#define EE_EEFORMATION_H

class EEComplexUnit;
class EEGroup;

// one unit's place in a formation (0x14 bytes, the element of Arrange's output; the implicit operator= is the
// out-of-line 0x1a95d6, the copy is bitwise; provisional names)
struct EEFormationSlot {
    // default ctor (EEFLine::Place 0x4dd9a3 zeroes mUnit/mHeading after the GE3DPoint ctor)
    EEFormationSlot() : mUnit(NULL), mHeading(0.0f) {}
    GE3DPoint mPosition;                    // +0x00
    EEComplexUnit* mUnit;                   // +0x0c
    float mHeading;                         // +0x10
};

class EEFormation : public UReferenceCounter {
public:
    // 0-3 short, long, loose, staggered line; 4-5 deep, shallow wedge; 6-7 inward, outward circle; NULL otherwise
    static EEFormation* Create(long inType) { return inType >= 0 && inType < 8 ? new EEFormation(inType) : NULL; }

    explicit EEFormation(long inType)
        : mHeading(0.0f), mType(inType), mB5C(true), mMinRadius(0.5f)
    {
    }
    virtual ~EEFormation() {}
    // the group's units into a vector, then Place
    void Arrange(EEGroup* inGroup, std::vector<EEFormationSlot>& outSlots);
    // the formation's slots for inUnits: lines and wedges replace outSlots, a circle appends to it
    void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots);

    bool IsCircle() const { return this->mType == 6 || this->mType == 7; }
    void SetCenter(const GE3DPoint& inCenter) { this->mCenter = inCenter; }
    void SetMinRadius(float inRadius) { this->mMinRadius = inRadius; }
    long GetType() const { return this->mType; }

    GETransformation mOrientation;
    GE3DPoint mCenter;
    float mHeading;
    long mType;
    bool mB5C;
    float mMinRadius;                       // a circle's lower bound of the radius (0.5; EEHerd sets its own)
};

#endif
