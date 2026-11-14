// C:\EEX\Empire Earth\Calamities\EECCMeteorStorm.h (RECONSTRUCTION; file name inferred). The meteor storm
// Layout proven: EECCMeteorStorm sizeof 0x3c = its new-site size, ctor 0xb2634 exact in EECCMeteorStorm.cpp. Member
// names are provisional.
// calamity class: meteors of one unit class fall around the target location at random intervals (vtable 0x43cb90;
// RTTI EECCMeteorStorm > EELocationCalamityClass > EECalamityClass). EECCMeteorStorm.cpp is 0xb2634..0xb2ba3. sizeof
// 0x3c (EECalamityManager's new). Field names are provisional. Requires ee/EELocationCalamityClass.h.
#ifndef EE_EECCMETEORSTORM_H
#define EE_EECCMETEORSTORM_H

class EESoundObject;

struct EECalamityUpdateHost;
struct EeCalamityCall;
struct EeCalamityReply;
class EECCMeteorStorm : public EELocationCalamityClass {
public:
    // the class's own world steps of its Rust update (ee/EECalamityUpdate.h)
    static bool UpdateStep(EECalamityUpdateHost& ioHost, const EeCalamityCall& inCall, EeCalamityReply& outReply);

protected:
    bool LaunchMeteor(EECalamity* inCalamity);   // 0xb2795 (provisional name)
};

#endif
