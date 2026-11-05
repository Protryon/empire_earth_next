// C:\EEX\Empire Earth\Persistent\EEPCalamityHelper.h (RECONSTRUCTION; minimal; provisional names). The
// saved calamity state (RTTI EEPCalamityHelper > FSPersistent, vtable 0x44220c, tag 0xb, sizeof 0x10, ctor 0x1497f4;
// EEPCalamityHelper.cpp 0x1497f4..0x149c3c, __FILE__ 0x149b30). Only what Empire Earth.cpp's registration uses.
// Requires lle/FSPersistent.h and <vector>.
#ifndef EE_EEPCALAMITYHELPER_H
#define EE_EEPCALAMITYHELPER_H

class EECalamity;
class EEContagionManager;

class EEPCalamityHelper : public FSPersistent {
public:
    EEPCalamityHelper();                    // 0x1497f4 (+0x08 = 0, +0x0c = new EEContagionManager)
    virtual void GetChecksum(ESChecksum& ioChecksum) const;   // +0x00 (ICF-folded `ret 4`)
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);   // +0x04 0x14998e
    virtual long RepairPointers(unsigned long inFirst, unsigned long inSecond);   // +0x08 0x149a33
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);  // +0x0c 0x149b30
    virtual ~EEPCalamityHelper();           // +0x10 (??_G 0x149826)
    virtual FSPersistent* CreateDefault() const;   // +0x14 0x149977
    virtual void Virtual1C(long inArg);     // +0x1c (ICF-folded `ret 4`)
    // +0x20 0x1498c2 (ret 4, returns this): the vtable slot holds operator= (was declared Virtual20())
    virtual EEPCalamityHelper& operator=(const EEPCalamityHelper& inOther);
    void ClearCalamities();                 // 0x149860 (deletes mL08; provisional name)
    void ClearContagionManager();           // 0x149880 (deletes mContagionManager; provisional name)
    void Reset();                           // 0x149897 (both, then a new EEContagionManager; EEGameState 0x13181d)

    std::vector<EECalamity*>* mCalamities;  // +0x08 (EEWorld::StartGame passes *mL08 to EECalamityManager::Restore when non-zero)
    EEContagionManager* mContagionManager;  // +0x0c (provisional name)
};

#endif
