// C:\EEX\Empire Earth\Empire Earth\EEVoting.h (RECONSTRUCTION; file name inferred). RTTI
// EEVoting (vtable 0x441fdc). EEVoting.cpp 0x137b3f..0x137b74: ctor 0x137b3f, ??_G 0x137b58, dtor 0x137b74. The
// world holds one at +0xb5c. sizeof 0xc. Member names are provisional.
#ifndef EE_EEVOTING_H
#define EE_EEVOTING_H

class EEVoting {
public:
    EEVoting();                             // 0x137b3f: +4/+6 = 0 (words), +0xa/+8 = 0 (bytes)
    virtual ~EEVoting();                    // 0x137b74
    long Propose(unsigned short inEdict, long inArg2);  // 0x137b7b (1: a proposal is already open) (EEMEdictProposal 0xbde48; provisional name)

    short mEdict;                           // +0x04
    short mU06;                             // +0x06
    bool mB08;                              // +0x08
    bool mB09;                              // +0x09 (not set by the ctor)
    bool mB0A;                              // +0x0a
};

#endif
