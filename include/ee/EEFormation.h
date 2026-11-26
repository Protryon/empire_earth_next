// C:\EEX\Empire Earth\Formations\EEFormation.h (RECONSTRUCTION; file name inferred). A group formation (RTTI
// EEFormation > UReferenceCounter; vtable 0x43f838). Names are provisional. Requires lle/LowLevelEngine.h.
//
// New (the units' lane, round 2): one concrete class. The original's eight subclasses (EEFShortLine, EEFLongLine,
// EEFLooseLine, EEFStaggeredLine > EEFLine; EEFDeepWedge, EEFShallowWedge > EEFWedge; EEFInwardCircle,
// EEFOutwardCircle > EEFCircle) only gave a type its constructor's members (a line's offsets, a wedge's arms, a
// circle's mode and arc): those are Rust's (rust/crates/ee_sim/src/formations, capture.rs shape_of), and so are Place
// and Arrange. What a caller sets stays here: the frame and a circle's minimum radius.
#ifndef EE_EEFORMATION_H
#define EE_EEFORMATION_H

class EEFormation : public UReferenceCounter {
public:
    // 0-3 short, long, loose, staggered line; 4-5 deep, shallow wedge; 6-7 inward, outward circle; NULL otherwise
    static EEFormation* Create(long inType) { return inType >= 0 && inType < 8 ? new EEFormation(inType) : NULL; }

    explicit EEFormation(long inType)
        : mHeading(0.0f), mType(inType), mB5C(true), mMinRadius(0.5f)
    {
    }
    virtual ~EEFormation() {}
    // (Arrange and Place, the slots of a group's or a list's units: ee_sim formations units.rs over the members from
    // mOrientation on, its EeFormationFields; EEWorldLayout.cpp registers and checks their place)

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
