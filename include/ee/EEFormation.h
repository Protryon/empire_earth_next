// C:\EEX\Empire Earth\Formations\EEFormation.h (RECONSTRUCTION; file name inferred). A group formation (RTTI
// EEFormation > UReferenceCounter; vtable 0x43f838). Names are provisional. Requires lle/LowLevelEngine.h.
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
    static EEFormation* Create(long inType);   // 0xddcc6: one of the 8 formation classes (0..7), 0 otherwise

    // the inline ctor/dtor (COMDATs 0xdd7f8 / ??_G 0xdd83d, emitted by EEFCircle.cpp) and the two
    // virtuals (vtable 0x43f838: ??_G, +0x04 0xdddd5, +0x08 pure). Names are provisional.
    EEFormation(long inType)
        : mHeading(0.0f), mType(inType)
    {
        this->mB5C = true;
    }
    virtual ~EEFormation() {}
    // +0x04 0xdddd5 (EEFormation.cpp): the group's units into a vector, then Place
    virtual void Arrange(EEGroup* inGroup, std::vector<EEFormationSlot>& outSlots);
    // +0x08: the formation's slots for inUnits (EEFCircle 0xdd1cc, EEFLine 0xdd9a3, EEFWedge 0xde019)
    virtual void Place(std::vector<EEComplexUnit*>& inUnits, std::vector<EEFormationSlot>& outSlots) = 0;

    // layout: the base ctor 0xdd7f8 (EEFLine's 0xdd8b2 calls it); sizeof 0x60
    GETransformation mOrientation;          // +0x08
    void SetCenter(const GE3DPoint& inCenter) { this->mCenter = inCenter; }   // inline: EEHerd::Surround 0xa4351 refuses the GE3DPoint::operator= inside (share)
    GE3DPoint mCenter;                      // +0x48 (EEActiveGroup::LayOutFormation 0x1ec5f3 assigns it through
                                            // GE3DPoint::operator=; the ctor 0xdd7f8 zeroes it) (provisional name)

    float mHeading;                         // +0x54
    long GetType() const { return this->mType; }
    long mType;                             // +0x58
    bool mB5C;                              // +0x5c (= true)
};

#endif
