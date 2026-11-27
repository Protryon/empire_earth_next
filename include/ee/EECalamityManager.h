// C:\EEX\Empire Earth\Calamities\EECalamityManager.h (RECONSTRUCTION; file name inferred). The calamity classes
// Layout proven: EECalamityManager sizeof 0x24 = its new-site size, ctor 0xadf97 exact in EECalamityManager.cpp. Member
// names are provisional.
// by enum (one per dbcalamity.dat record) and the active calamities. EECalamityManager.cpp is 0xadf97..0xaebb7
// (__FILE__ 0x4b9850); vtable 0x43c714, RTTI EECalamityManager > USingleton<EECalamityManager>; sizeof 0x24 (the
// GetInstance COMDAT's new). Method names are provisional (by behaviour) except where an assert names a local.
// Requires lle/LowLevelEngine.h, lle/USingleton.h, <stdio.h> (FILE).
#ifndef EE_EECALAMITYMANAGER_H
#define EE_EECALAMITYMANAGER_H
#include "lle/USingleton.h"

class EECalamity;
class EECalamityClass;
class EEPlayer;
class FSFileSpec;

// A calamity class's enum (EECalamityEnum; dbcalamity.dat +0x84, EECalamityClass::mClassEnum): Rust's enums (ee_sim game_ids.rs), emitted back as plain C++ enums with their evidence comments.
#include "rust/sim/game_ids.h"
#include "rust/game/gamecpp.h"

// Deletes every element of ioList and empties it; the number deleted (cdecl template COMDATs 0xaea54, 0xaeb53 at
// the end of EECalamityManager.cpp; name and file name inferred: not in the unit, whose first assert is on line 77)
template<class T> unsigned long DeleteVectorContents(T& ioList)
{
    unsigned long theCount = 0;
    for (T::iterator theIterator = ioList.begin(); theIterator != ioList.end(); ++theIterator) {
        delete *theIterator;
        theCount++;
    }
    ioList.clear();
    return theCount;
}

class EECalamityManager : public USingleton<EECalamityManager> {
public:
    EECalamityManager() {  }
    // +0x00: the implicit virtual destructor (0xadfe9, after the ??_G 0xadfcd)
    // +0x04 0xae6df
    virtual void DumpOutOfSynch(FILE* inFile) { ee_gamecpp_calamity_dump_all(this, inFile); }
    // +0x08 0xae7eb
    virtual long GetOutOfSynchChecksum() { return ee_gamecpp_calamity_checksum_all(this); }

    void DestroyCalamities() { ee_gamecpp_calamity_destroy_all(this); }
    // 0xae526/0xae5ac/0xae66b: a new calamity of class inClassEnum (false if the class or a unit is missing)
    bool CreateGlobalCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, unsigned long inPlayerNumber) { return ee_gamecpp_calamity_create(this, 0, inClassEnum, inCasterUnitID, 0, 0, 0, inPlayerNumber); }
    bool CreateLocationCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, long inX, long inY, unsigned long inPlayerNumber) { return ee_gamecpp_calamity_create(this, 1, inClassEnum, inCasterUnitID, 0, inX, inY, inPlayerNumber); }
    bool CreateUnitCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, unsigned long inTargetUnitID, unsigned long inPlayerNumber) { return ee_gamecpp_calamity_create(this, 2, inClassEnum, inCasterUnitID, inTargetUnitID, 0, 0, inPlayerNumber); }
    void GetCalamities(std::vector<EECalamity*>*& outCalamities) { outCalamities = &this->mCalamities; }
    // 0xae71d: 0 past the end
    EECalamityClass* GetCalamityClass(const EECalamityEnum& inClassEnum) const { return (EECalamityClass*)ee_gamecpp_calamity_class_of(const_cast<EECalamityManager*>(this), inClassEnum); }
    // 0xae774: 0 past the end
    EECalamityClass* GetCalamityClass(unsigned long inClassEnum) const { return (EECalamityClass*)ee_gamecpp_calamity_class_of(const_cast<EECalamityManager*>(this), inClassEnum); }
    unsigned long GetCalamityClassCount() const { return this->mCalamityClasses.size(); }  // inline (out-of-line COMDAT 0x138e4b, 33 callers; EESETSETGameCalamity::Virtual14; provisional name)
    bool GetCalamityClassIndex(EECalamityClass* inClass, long& outIndex) { return ee_gamecpp_calamity_index(this, inClass, true, &outIndex); }
    // 0xae7c5 (the class's Virtual14)
    bool GetCalamityType(EECalamityEnum inClassEnum, long& outType) { return ee_gamecpp_calamity_type(this, inClassEnum, &outType); }
    bool IsCalamityActive(long inClassEnum) { return ee_gamecpp_calamity_is_active(this, inClassEnum); }
    // 0xae834 (the class's Virtual28)
    void LoadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum) { ee_gamecpp_calamity_class_media(this, inPlayer, inClassEnum, true); }
    // 0xae869: takes over loaded calamities (games older than 0.30 drop their plagues, leprosy and fire)
    void Restore(unsigned long inMajorVersion, unsigned long inMinorVersion, std::vector<EECalamity*>& inCalamities) { ee_gamecpp_calamity_restore(this, inMajorVersion, inMinorVersion, &inCalamities); }
    // 0xae966 (the class's Virtual34)
    void UnloadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum) { ee_gamecpp_calamity_class_media(this, inPlayer, inClassEnum, false); }
    void Update(float inElapsedTime) { ee_gamecpp_calamity_update_all(this, inElapsedTime); }

    // the class of inClassEnum (false if there is none; the original's dynamic_cast to the volcano's base)
    bool GetCalamityClass(unsigned long inClassEnum, EECalamityClass*& outClass)
    {
        outClass = this->GetCalamityClass(inClassEnum);
        return outClass != NULL;
    }

    std::vector<EECalamity*> mCalamities;   // +0x04
    std::vector<EECalamityClass*> mCalamityClasses;   // +0x14 by enum
};

#endif
