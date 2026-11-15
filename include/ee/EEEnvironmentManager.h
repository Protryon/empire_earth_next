// C:\EEX\Empire Earth\Empire Earth\EEEnvironmentManager.h (RECONSTRUCTION; file name inferred; provisional member
// names). The weather / day-night state of a game (RTTI EEEnvironmentManager > FSPersistent > FSRestorable, vtable
// 0x441f70, sizeof 0x5c, ctor 0x12ed4e; EEEnvironmentManager.cpp 0x12ed4e..0x12f4b2). Layout from the ctor
// (member ctors, then the body's stores); unnamed members by offset. Requires lle/FSPersistent.h, lle/GEColor.h,
// <vector>.
#ifndef EE_EEENVIRONMENTMANAGER_H
#define EE_EEENVIRONMENTMANAGER_H

class EEEMStorm;
template <class T> class U2DPoint;

class EEEnvironmentManager : public FSPersistent {
public:
    EEEnvironmentManager();                 // 0x12ed4e (tag 0xc)
    void FnE123D();                         // (ICF-folded into 0xe123d: an empty body)
    void SetEnabled(bool inEnabled);       // 0x12f1fb (mB58; loads or unloads its effects; provisional name)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x12ef75
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x08 0x12f083
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x12f0ff
    virtual ~EEEnvironmentManager();        // +0x10 (??_G 0x12ede0)
    virtual FSPersistent* CreateDefault() const;   // +0x14 0x12ef03

    std::vector<EEEMStorm*> mStorms;        // +0x08 (Load news EEEMStorm(0, this) per element)
    GEColor mC18;                           // +0x18
    GEColor mC24;                           // +0x24
    long mState;                            // +0x30 (= 0)
    float mWind;                            // +0x34
    float mWindTarget;                      // +0x38
    float mWindRate;                        // +0x3c
    float mF40;                             // +0x40 (= 0.25)
    float mF44;                             // +0x44 (= 0.75)
    unsigned long mNextStep;                // +0x48 (= 0; read/written as unsigned long)
    long mMaxStorms;                        // +0x4c (= 3)
    unsigned long mL50;                     // +0x50 (not set by the ctor; read/written as unsigned long)
    long mInterval;                         // +0x54 (= 5000)
    bool mIsEnabled;                        // +0x58 (= false)
    void Step(unsigned long inTime);        // 0x12ef1a (provisional name)
    // EEEnvironmentManager.cpp's other members (provisional names)
    bool IsInStorm(const U2DPoint<long>& inPoint, bool inTypes13, bool inTypes24);   // 0x12ee1b
    void DeleteStorms();                    // 0x12f25b
    void UpdateStorms(float inTimeOfDay, unsigned long inTime);   // 0x12f2a6
    void UpdateWind(float inTimeOfDay, unsigned long inTime);   // 0x12f3d6 (ret 8; both unused)
    // inline (out-of-line COMDAT 0x15439f at the end of EEPhysics.cpp, its first user UpdateWeather passes mLD0; unsigned: ja)
    // Provisional name
    void Tick(unsigned long inTime);
};

#endif
