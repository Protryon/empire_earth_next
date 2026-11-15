// C:\EEX\Empire Earth\Calamities\EECalamityManager.h (RECONSTRUCTION; file name inferred). The calamity classes
// Layout proven: EECalamityManager sizeof 0x24 = its new-site size, ctor 0xadf97 exact in EECalamityManager.cpp. Member
// names are provisional.
// by enum (one per dbcalamity.dat record) and the active calamities. EECalamityManager.cpp is 0xadf97..0xaebb7
// (__FILE__ 0x4b9850); vtable 0x43c714, RTTI EECalamityManager > USingleton<EECalamityManager>; sizeof 0x24 (the
// GetInstance COMDAT's new). Method names are provisional (by behaviour) except where an assert names a local.
// Requires lle/LowLevelEngine.h, lle/USingleton.h, <stdio.h> (FILE).
#ifndef EE_EECALAMITYMANAGER_H
#define EE_EECALAMITYMANAGER_H

class EECalamity;
class EECalamityClass;
class EEGlobalCalamityClass;
class EELocationCalamityClass;
class EEPlayer;
class EEUnitCalamityClass;
class FSFileSpec;

// A calamity class's enum (dbcalamity.dat +0x84; EECalamityClass::mClassEnum). Enum and enumerator names are
// provisional: the
// game's callers of GetCalamityClass(const&) pass converted temporaries (`x - 6`, the literal 7), so the parameter
// is a distinct type, and EECalamity::Load's long picks the unsigned long overload. Values from the class switch in
// LoadCalamityClasses (1, 2, 5, 6, 11, 12 have no class).
enum EECalamityEnum {
    kCalamityEarthquake = 3, kCalamityHurricane = 4, kCalamityPlague = 7, kCalamityLeprosy = 8,
    kCalamityVolcano = 9, kCalamityFire = 10, kCalamityMorale = 13, kCalamityTimeWarp = 14,
    kCalamityTeleport = 15, kCalamityAreaEffect = 16, kCalamityAEExtraDamage = 17, kCalamitySpecialAttack = 18,
    kCalamityAEMindControl = 19, kCalamityConscription = 20, kCalamityLogicBomb = 21, kCalamityDropObject = 22,
    kCalamityCloaking = 23, kCalamityMeteorStorm = 24, kCalamityLineOfSight = 25, kCalamitySpecialLocation = 26
};

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

class EEUnit;
class GETransformation;

// 0xaeb83 (EECalamityManager.cpp, after its template COMDATs): a new unit of class inUnitClass at inTransformation for
// inPlayerNumber (-1: inCalamity's player); 0 if the class does not exist (provisional name)
EEUnit* EECreateCalamityUnit(EECalamity* inCalamity, long inUnitClass, GETransformation& inTransformation,
                             long inPlayerNumber);

class EECalamityManager : public USingleton<EECalamityManager> {
public:
    EECalamityManager();                    // 0xadf97
    // +0x00: the implicit virtual destructor (0xadfe9, after the ??_G 0xadfcd)
    virtual void DumpOutOfSynch(FILE* inFile);   // +0x04 0xae6df
    virtual long GetOutOfSynchChecksum();   // +0x08 0xae7eb

    void DestroyCalamities();               // 0xae00b
    long LoadCalamityClasses(FSFileSpec* inFile);   // 0xae070 (db\dbcalamity.dat; EEWorld 0x141d95)
    // 0xae526/0xae5ac/0xae66b: a new calamity of class inClassEnum (false if the class or a unit is missing)
    bool CreateGlobalCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, unsigned long inPlayerNumber);
    bool CreateLocationCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, long inX, long inY,
                                unsigned long inPlayerNumber);
    bool CreateUnitCalamity(unsigned long inClassEnum, unsigned long inCasterUnitID, unsigned long inTargetUnitID,
                            unsigned long inPlayerNumber);
    void GetCalamities(std::vector<EECalamity*>*& outCalamities);   // 0xae711
    EECalamityClass* GetCalamityClass(const EECalamityEnum& inClassEnum) const;   // 0xae71d: 0 past the end
    bool GetCalamityIndex(EECalamity* inCalamity, long& outIndex);  // 0xae749
    EECalamityClass* GetCalamityClass(unsigned long inClassEnum) const;   // 0xae774: 0 past the end
    unsigned long GetCalamityClassCount() const { return this->mCalamityClasses.size(); }  // inline (out-of-line COMDAT 0x138e4b, 33 callers; EESETSETGameCalamity::Virtual14; provisional name)
    bool GetCalamityClassIndex(EECalamityClass* inClass, long& outIndex);   // 0xae79a
    bool GetCalamityType(EECalamityEnum inClassEnum, long& outType);  // 0xae7c5 (the class's Virtual14)
    bool IsCalamityActive(long inClassEnum);   // 0xae80e
    void LoadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum);   // 0xae834 (the class's Virtual28)
    // 0xae869: takes over loaded calamities (games older than 0.30 drop their plagues, leprosy and fire)
    void Restore(unsigned long inMajorVersion, unsigned long inMinorVersion, std::vector<EECalamity*>& inCalamities);
    void AddCalamity(EECalamity* inCalamity);   // 0xae8bc (kept only if its class starts it)
    std::vector<EECalamity*>::iterator DestroyCalamity(std::vector<EECalamity*>::iterator inCalamity);   // 0xae8e1
    void DestroyCalamityClasses(bool inShutdown);  // 0xae921
    void UnloadMedia(EEPlayer* inPlayer, const EECalamityEnum& inClassEnum);   // 0xae966 (the class's Virtual34)
    void Update(float inElapsedTime);       // 0xae999

    // the class of inClassEnum as a T (dynamic_cast; false if it is not one); COMDATs 0xaea85 (global), 0xaeaec
    // (location), 0x66a89 (unit)
    template<class T> bool GetCalamityClass(unsigned long inClassEnum, T*& outClass)
    {
        std::vector<long> theUnused;
        bool theResult = false;
        if (inClassEnum < this->mCalamityClasses.size()) {
            EECalamityClass* theClass = this->mCalamityClasses[inClassEnum];
            outClass = dynamic_cast<T*>(theClass);
            if (outClass != NULL)
                theResult = true;
        }
        return theResult;
    }

    std::vector<EECalamity*> mCalamities;   // +0x04
    std::vector<EECalamityClass*> mCalamityClasses;   // +0x14 by enum
};

#endif
