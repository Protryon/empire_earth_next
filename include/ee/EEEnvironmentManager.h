// C:\EEX\Empire Earth\Empire Earth\EEEnvironmentManager.h (RECONSTRUCTION; file name inferred; provisional member
// names). The weather / day-night state of a game (RTTI EEEnvironmentManager > FSPersistent > FSRestorable, vtable
// 0x441f70, sizeof 0x5c, ctor 0x12ed4e; EEEnvironmentManager.cpp 0x12ed4e..0x12f4b2). Layout from the ctor
// (member ctors, then the body's stores); unnamed members by offset. Requires lle/FSPersistent.h, lle/GEColor.h,
// <vector>.
#ifndef EE_EEENVIRONMENTMANAGER_H
#define EE_EEENVIRONMENTMANAGER_H

#include "rust/game/gamecpp.h"
#include "lle/FSPersistent.h"
#include "lle/GEColor.h"
class EEEMStorm;
template <class T> class U2DPoint;

class EEEnvironmentManager : public FSPersistent {
public:
    EEEnvironmentManager();                 // 0x12ed4e (tag 0xc)
    // (ICF-folded into 0xe123d: an empty body)
    void FnE123D() {  }
    // Turns the weather on or off: off deletes every storm and unloads the rain and snow effects, on loads them
    // 0x12f1fb (mB58; loads or unloads its effects; provisional name)
    void SetEnabled(bool inEnabled) { ee_gamecpp_env_set_enabled(this, inEnabled); }
    // Reads the storms, state, wind and timing from a saved game; returns the first read error
    // +0x04 0x12ef75
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile) { return ee_gamecpp_env_load(this, &inFile); }
    // Reconnects the loaded storms after a load, deleting the ones that fail to restore; returns 0
    // +0x08 0x12f083
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond) { return ee_gamecpp_env_repair(this); }
    // Writes the storms, state, wind and timing to a saved game; returns the first write error
    // +0x0c 0x12f0ff
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile) { return ee_gamecpp_env_save(this, &outFile); }
    // Destroys the weather manager, releasing and deleting every storm
    // +0x10 (??_G 0x12ede0)
    virtual ~EEEnvironmentManager() { ee_gamecpp_env_delete_storms(this); }
    // Creates a new default weather manager (the persistent-object factory)
    // +0x14 0x12ef03
    virtual FSPersistent* CreateDefault() const { return new EEEnvironmentManager; }

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
    // Advances the storms and the wind, clears a finished state and schedules the next weather step by game speed
    void Step(unsigned long inTime) { ee_gamecpp_env_step(this, inTime); }
    // (DeleteStorms, UpdateStorms, UpdateWind: ee_game gamecpp weather.rs)
    // inline (out-of-line COMDAT 0x15439f at the end of EEPhysics.cpp, its first user UpdateWeather passes mLD0; unsigned: ja)
    // Provisional name
    void Tick(unsigned long inTime) { if (this->mNextStep <= inTime && this->mIsEnabled == true) this->Step(inTime); }
};

#endif
