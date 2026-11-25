// C:\EEX\Empire Earth\Empire Earth\EECameraKey.h (RECONSTRUCTION; file name inferred; provisional class name). An
// 0xa4-byte camera key: EECameraControl keeps a vector of them (+0x04) and EEScenarioManager one at
// +0x08. Layout from its ctor 0x12883b (two GETransformation default ctors, then six fldz stores: two GE3DPoint
// default ctors at +0x80/+0x8c); +0x98..+0xa3 are not touched by the ctor. Requires lle/GETransformation.h,
// lle/GE3DPoint.h.
#ifndef EE_EECAMERAKEY_H
#define EE_EECAMERAKEY_H

class EECameraKey {
public:
    // 0x12883b (out of line: EEScenarioManager's ctor calls it)
    EECameraKey() {  }

    GETransformation mT00;                  // +0x00
    GETransformation mT40;                  // +0x40
    GE3DPoint mOffset;                      // +0x80
    GE3DPoint mFocus;                       // +0x8c
    float mWait;                            // +0x98 (untouched by the ctor; EECameraControl::mWait)
    float mRate;                            // +0x9c (the speed: 1 / duration)
    bool mChanged;                          // +0xa0 (EECameraControl::mChanged; the implicit operator= copies a byte)
};

#endif
