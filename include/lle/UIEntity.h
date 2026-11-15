// C:\EEX\Low-Level Engine\User Interface\UIEntity.h (RECONSTRUCTION). Base of every UI element (controls and
// the UIEntityGroup members). No UIEntity.cpp in the stretch: the members are inline, emitted first by
// UIBackground.cpp (0x10058e86..0x10058f03). sizeof 8 (??_E stride, 0x10058eb4).
#ifndef LLE_UIENTITY_H
#define LLE_UIENTITY_H

class LLE_API UIEntity {
public:
    unsigned long GetType() const { return this->mType; }  // exported 0x10004380 (ICF)
protected:
    // 0x10058e86: the vptr is stored before mType (assigned in the body)
    UIEntity(unsigned long inType) { this->mType = inType; }
    virtual ~UIEntity() {}                  // 0x10058e98

    unsigned long mType;                    // +0x04
};

#endif
