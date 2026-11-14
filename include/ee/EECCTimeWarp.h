// C:\EEX\Empire Earth\Calamities\EECCTimeWarp.h (RECONSTRUCTION; file name inferred). The time warp calamity
// Layout proven: EECCTimeWarp sizeof 0x4c = its new-site size, ctor 0xb3b42 exact in EECCTimeWarp.cpp. Member names are
// provisional.
// class: the target unit leaves the map for a while and returns where it was (vtable 0x43cea4; RTTI EECCTimeWarp >
// EEUnitCalamityClass > EECalamityClass). EECCTimeWarp.cpp is 0xb3ade..0xb4634 (its two colour globals' init
// chains first). sizeof 0x4c. Field names are provisional. Requires ee/EEUnitCalamityClass.h.
#ifndef EE_EECCTIMEWARP_H
#define EE_EECCTIMEWARP_H

class EEGraphicEffect;
class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCTimeWarp : public EEUnitCalamityClass {
public:
    // the class's own steps of its Rust Start and Stop (ee/EECalamityUpdate.h: EeCalamityOp::ClassStep)
    static bool StartStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    // 0xb3ede: the departure (inDeparting) or arrival effect at inLocation and the secondary sound
    void PlayEffect(GE3DPoint& inLocation, EECalamity* inCalamity, bool inDeparting);
    void Return(EECalamity* inCalamity);    // 0xb3fc9: the unit comes back at its tile (provisional name)
    void Depart(EECalamity* inCalamity);    // 0xb41dd: the unit leaves (provisional name)
};

#endif
