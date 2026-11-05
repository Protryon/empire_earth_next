// EECivilizationManager.h (RECONSTRUCTION; file name inferred). RTTI EECivilizationManager (vtable 0x44bfd4, one slot:
// the deleting dtor); Civilization Builder/EECivilizationManager.cpp. The game's instance is gCivilizationManager
// (0x52a3a0; Empire Earth.cpp's init chain constructs it; provisional name). Only what the decompiled units use.
#ifndef EE_EECIVILIZATIONMANAGER_H
#define EE_EECIVILIZATIONMANAGER_H
class FSFile;

class EECBUIListBox;
class EEPlayer;
class EEUnitClassUpgrade;
class EECMUpgradeFunctor;                   // EECivilizationManager.cpp (vtable 0x44bfec)
class UString;
class UWideString;

// One civilization of the list (0x78 bytes: the manager's vector +0x5210, copied by GetCivilizations; operator+
// 0x17a95e scales by 0x78). Names are provisional. The body is visible to every
// includer (array members cost no `$E` number; the old guard changed nothing).
struct EECivilizationEntry;
struct EECivilizationEntry {
    // inline (EEUIEPDCS's file helper 0x1e61db: with this expansion VC6 refuses the two earlier vector operator[]s as
    // the original does; provisional name)
    const char* GetFileName() const { return this->mFileName; }

    long mUnusedDatabaseID;                 // +0x00
    long mIndex;                            // +0x04
    char mFileName[100];                   // +0x08 the civilization's file (EEUIEPDCS: the selected entry's file)
    long mChecksum;                        // +0x6c actual281bc5 filename/checksum record
    long mNameID;                           // +0x70 string id of the name (gResource->GetString)
    long mEraNameID;                        // +0x74
};
static_assert(sizeof(EECivilizationEntry) == 120, "on-disk record: 120 bytes on every target");

// Actual serialized600-byte category row,35 records at manager+4. Names provisional.
struct EECivilizationUpgradeValue { float mValue; float mBonus; };
struct EECivilizationCategoryEntry {
    long mDatabaseID;                      // +00, loader preserves/increments
    long mIndex;                           // +04, loader preserves/increments
    unsigned char mUndecoded08[100];       // +08..6b actual persisted interval; untouched
    long mNameID;                          // +6c, actual name reader281c92
    long mCost;                            // +70, actual long281873
    long mUnitSet;                         // +74, actual loader writes102
    EECivilizationUpgradeValue mLevels[60];// +78, actual1..<61 float-pair enumeration
};
static_assert(sizeof(EECivilizationCategoryEntry) == 600, "on-disk record: 600 bytes on every target");

class EECivilizationManager {
public:
    long Load(FSFile* inFile); // original-backed 0x281514; caller1414a3, FSFile IO proof
    long LoadPremadeCivilizations(FSFile* inFile); // original-backed 0x281672; caller1414a3, FSFile IO proof
    EECivilizationManager();            // 0x281458
    virtual ~EECivilizationManager();   // 0x2814c4
    // (EEUIEPDCS*; provisional names): 0x2818f9 clears outCivs and push_backs every entry of
    // the vector at +0x5210; 0x28199b DeleteAllEntries, then fills inList from the string ids at +0x5220
    void GetCivilizations(std::vector<EECivilizationEntry>& outCivs);
    void FillCivilizationList(EECBUIListBox* inList);
    // an upgrade id is (category << 6) | index; the category table has 0x23 entries of 0x258 bytes (the list box's
    // callers; provisional names)
    long GetExtraCost(unsigned short inUpgrade);    // 0x281873 (the category entry's +0x74: the upgrade's cost)
    bool GetCategoryNameID(unsigned long inCategory, long* outNameID);  // 0x281c92 (false when inCategory >= 0x23)
    bool LookupLevel(unsigned short inUpgrade, float* outValue, float* outBonus);  // 0x281d18
    long CountInCategory(unsigned short inUpgrade, std::vector<unsigned short>* inList);  // 0x28189e (how many of inList share inUpgrade's category)
    void SplitAttribute(unsigned short inUpgrade, unsigned long* outCategory, unsigned long* outIndex);  // 0x281dc7
    // (provisional names)
    void AddCategory(const EECivilizationCategoryEntry& inEntry);   // 0x2814e5 (memcpy into mCategories[mCategoryCount++])
    void GetCivilizationsOfType(std::vector<EECivilizationEntry>& outCivs, long inType);   // 0x281945 (mU74 == inType)
    bool GetCategoryLevel(unsigned long inCategory, unsigned long inIndex, float* outValue, float* outBonus);  // 0x281d37
    bool GetCategory(unsigned long inCategory, EECivilizationCategoryEntry** outEntry);   // 0x281d79
    unsigned long MakeUpgrade(unsigned long inCategory, unsigned char inIndex);   // 0x281d9c (ecx unused)
    unsigned long MakePowerUpgrade(unsigned char inIndex);   // 0x281db1 (the powers' category Load appends; ecx unused)
    bool ForEachUpgrade(EECMUpgradeFunctor& inFunctor);   // 0x281cb2 (every category while the functor agrees)
    bool ForEachUpgradeOfCategory(unsigned long inCategory, EECMUpgradeFunctor& inFunctor);   // 0x281cd5 (indices 1..60)
    std::vector<unsigned short> GetUpgrades();      // 0x281ded (every upgrade, through the EECMGetUpgrades functor)
    // (provisional names)
    bool MatchesPremadeCivilization(const UWideString& inFile, const UWideString& inName, std::vector<unsigned short>& inUpgrades,
                  long* outNameID);   // 0x281bc5 (premade civilization checksum test; reads gCivilizationManager)
    bool ApplyUpgrade(unsigned short inUpgrade, long inPlayer);  // 0x281e4d (an EECBProcessUpgrade over the category's unit set)
    void ApplyUpgradeLevel(EEUnitClassUpgrade* inUpgrade, unsigned long inIndex, long inUnitType, float inValue, EEPlayer* inPlayer);  // 0x281ed3
    unsigned long GetNumCivilizations() { return this->mCivilizations.size(); } // actual17a522
    EECivilizationCategoryEntry mCategories[35]; // +0004, ctor5208-byte region
    std::vector<EECivilizationEntry> mCivilizations; // +520c
    std::vector<long> mEraNameIDs;                // +521c
    unsigned long mCategoryCount;               // +522c
};

extern EECivilizationManager gCivilizationManager;          // 0x52a3a0
// EECivilizationManager.cpp's two init-chain strings, read by the civilization file unit (0x27fdfe..; extern, so the
// definitions have external linkage)
extern const UString kCivKey;               // 0x5654d0 ".civ"
extern const UString kCivilizationsKey;     // 0x5654c0 "Civilizations"

// Civilization Builder helpers (cdecl; provisional names): 0x27ffc0 the civilizations'
// directory (gStartupDirectory-based when inX); 0x280840 loads civilization inName from inDirectory (FSPath::FindFile
// in gDataPath), returns keseOK on success
UString EEGetCivilizationDirectory(bool inX);
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<unsigned short>& outUpgrades);

#endif
