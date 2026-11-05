// C:\EEX\Empire Earth\Data Pre-Loading Thread\EEGraphicEffect.cpp (name from RTTI EEGraphicEffect; no __FILE__).
// 0xd6bed..0xd72c1: the effect table of db\dbgfxeffects.dat (EEGraphicEffect : UReferenceCounter, EEMedia). The
// three statics' init chains (sEffects 0x4e8420, the 0x48 per-kind media vectors 0x4e7f98, the id set 0x4e7f88;
// guard byte 0x4e8418) open the unit. Unit flags: the exe's module set.
#include <windows.h>
#include <stdio.h>
#include <set>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/FSFileSpec.h"
#include "lle/TSScheduler.h"
#include "lle/URandom.h"

#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEMedia.h"
#include "ee/EEGraphicObject.h"
#include "ee/EEGraphicEffect.h"

// The unit's three dynamic initializers open it (0xd6bed..0xd6ceb), in definition order; one guard byte 0x4e8418
// (bits 1/2/4) for the three atexit destructors. .bss layout is reverse definition order.
// chain at 0x4d6bed: sEffects 0x4e8420 (allocator byte, _Buy(0)), atexit dtor 0x4d6c1b (_Tidy)
std::vector<EEGraphicEffect*> EEGraphicEffect::sEffects;
// SYNTHETIC: EXE 0x4d6bed   (wrapper)
// SYNTHETIC: EXE 0x4d6bf7   (ctor)
// SYNTHETIC: EXE 0x4d6c0f   (registrar)
// SYNTHETIC: EXE 0x4d6c1b   (dtor)
// chain at 0x4d6c36: 0x4e7f98, the media of each kind (0x48 kinds; provisional name)
std::vector<EEMedia*> EEGraphicEffect::sMediaByKind[0x48];
// SYNTHETIC: EXE 0x4d6c36   (wrapper)
// SYNTHETIC: EXE 0x4d6c40   (ctor)
// SYNTHETIC: EXE 0x4d6c64   (registrar)
// SYNTHETIC: EXE 0x4d6c70   (dtor)
// chain at 0x4d6c9a: 0x4e7f88, the effect ids (provisional name)
std::set<unsigned long> EEGraphicEffect::sIDs;
// SYNTHETIC: EXE 0x4d6c9a   (wrapper)
// SYNTHETIC: EXE 0x4d6ca4   (ctor)
// SYNTHETIC: EXE 0x4d6cc4   (registrar)
// SYNTHETIC: EXE 0x4d6cd0   (dtor)
extern std::vector<EEMedia*> gMediaTable;   // 0x4e8448 (EEAnimal.cpp)

// One record of db\dbgfxeffects.dat (0xe8 bytes, read whole). Layout inferred (only the fields the ctor copies).
struct EEGraphicEffectRecord {
    char mLabel[100];                       // +0x000 the row name (the game never reads it; the text's label)
    int32_t mUnusedDatabaseID;              // +0x064 the database row id (not read)
    int32_t mID;                            // +0x68 -> +0x60 (> 0: a real record)
    int32_t mKind;                          // +0x6c -> +0x58
    int32_t mTagPointKind;                  // +0x70 -> +0x5c
    int32_t mMediaIndex;                    // +0x74 -> +0x0c (EEMedia::Get)
    float mStartSize;                       // +0x78 -> +0x20
    float mHeight;                          // +0x7c -> +0x1c
    float mAlpha;                           // +0x80 -> +0x24
    int32_t mTime1;                         // +0x84 -> +0x70
    int32_t mTime2;                         // +0x88 -> +0x74
    int32_t mDuration;                      // +0x8c -> +0x78
    int32_t mIntParam1;                     // +0x90 -> +0x64
    int32_t mIntParam2;                     // +0x94 -> +0x68
    int32_t mIntParam3;                     // +0x98 -> +0x6c
    int32_t mLinkedEffectIndex1;            // +0x9c -> +0x10 (an index until Load links it)
    int32_t mLinkedEffectIndex2;            // +0xa0 -> +0x14
    int32_t mLinkedEffectIndex3;            // +0xa4 -> +0x18
    int32_t mNameStringId;                  // +0xa8 -> +0x7c
    float mParam1;                          // +0xac -> +0x28
    float mParam2;                          // +0xb0 -> +0x2c
    float mParam3;                          // +0xb4 -> +0x30
    float mParam4;                          // +0xb8 -> +0x34
    float mParam5;                          // +0xbc -> +0x38
    float mParam6;                          // +0xc0 -> +0x3c
    float mParam7;                          // +0xc4 -> +0x40
    float mParam8;                          // +0xc8 -> +0x44
    float mParam9;                          // +0xcc -> +0x48
    float mParam10;                         // +0xd0 -> +0x4c
    float mParam11;                         // +0xd4 -> +0x50
    float mEmitPositionSpread;                          // +0xd8 -> +0x54
    bool mFlag1;                            // +0xdc -> +0x88
    bool mFlag2;                            // +0xdd -> +0x89
    bool mIsScenarioListed;                 // +0xde -> +0x8b
    bool mIsEmitFlat;                       // +0xdf -> +0x8c
    bool mEndsWithUnitOrDuration;           // +0xe0 -> +0x8a
    int32_t mDetailLevel;                   // +0xe4 -> +0x84
};
static_assert(sizeof(EEGraphicEffectRecord) == 232, "on-disk record: 232 bytes on every target");

// FUNCTION: EXE 0x4d6ceb
// Creates a graphic effect from its table record; linked effects stay table indices until LoadTable links them
EEGraphicEffect::EEGraphicEffect(EEGraphicEffectRecord* inRecord)
{
    this->mGraphicObject = static_cast<EEGraphicObject*>(EEMedia::Get(inRecord->mMediaIndex));
    this->mAlpha = inRecord->mAlpha;
    this->mSize = inRecord->mHeight;
    this->mStartSize = inRecord->mStartSize;
    this->mKind = inRecord->mKind;
    this->mTagPointKind = inRecord->mTagPointKind;
    this->mTime1 = inRecord->mTime1;
    this->mTime2 = inRecord->mTime2;
    this->mDuration = inRecord->mDuration;
    this->mID = inRecord->mID;
    this->mLinkedEffect1 = reinterpret_cast<EEGraphicEffect*>(inRecord->mLinkedEffectIndex1);
    this->mLinkedEffect2 = reinterpret_cast<EEGraphicEffect*>(inRecord->mLinkedEffectIndex2);
    this->mLinkedEffect3 = reinterpret_cast<EEGraphicEffect*>(inRecord->mLinkedEffectIndex3);
    this->mIntParam1 = inRecord->mIntParam1;
    this->mIntParam2 = inRecord->mIntParam2;
    this->mIntParam3 = inRecord->mIntParam3;
    this->mParam1 = inRecord->mParam1;
    this->mParam2 = inRecord->mParam2;
    this->mParam3 = inRecord->mParam3;
    this->mParam4 = inRecord->mParam4;
    this->mParam5 = inRecord->mParam5;
    this->mParam6 = inRecord->mParam6;
    this->mParam7 = inRecord->mParam7;
    this->mParam8 = inRecord->mParam8;
    this->mParam9 = inRecord->mParam9;
    this->mParam10 = inRecord->mParam10;
    this->mParam11 = inRecord->mParam11;
    this->mEmitPositionSpread = inRecord->mEmitPositionSpread;
    this->mFlag1 = inRecord->mFlag1;
    this->mFlag2 = inRecord->mFlag2;
    this->mEndsWithUnitOrDuration = inRecord->mEndsWithUnitOrDuration;
    this->mIsEmitFlat = inRecord->mIsEmitFlat;
    this->mIsScenarioListed = inRecord->mIsScenarioListed;
    this->mNameStringId = inRecord->mNameStringId;
    this->mDetailLevel = inRecord->mDetailLevel;
    this->mNumLoads = 0;
}

// Compiler-generated: the ??_G of the virtual dtor (calls it out of line; EEMedia's thunk 0xd726c adjusts this):
// TEMPLATE: EXE 0x4d6e5d

// FUNCTION: EXE 0x4d6e79
// Destroys the graphic effect (nothing of its own to free)
EEGraphicEffect::~EEGraphicEffect()
{
}

// FUNCTION: EXE 0x4d6e9e
// Reads the graphic effect table from the file and links each effect's three follow-on effects by index;
// returns the last read result
long EEGraphicEffect::LoadTable(FSFile* inFile)
{
    EEGraphicEffectRecord theRecord;
    inFile->Open(kFileAccessRead);
    unsigned long theCount = 0;
    unsigned long theRead = 0;
    long theResult = inFile->Read(theCount);
    if (theResult == kErrorOK) {
        EEGraphicEffect::sEffects.resize(theCount, NULL);
        for (unsigned long i = 0; i < theCount; i++) {
            theResult = inFile->Read(reinterpret_cast<unsigned char*>(&theRecord), sizeof(theRecord), theRead);
            EEGraphicEffect::sEffects[i] = theRecord.mID > 0 ? new EEGraphicEffect(&theRecord) : NULL;
        }
    }
    for (unsigned long j = 0; j < EEGraphicEffect::sEffects.size(); j++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::sEffects[j];
        if (theEffect == NULL)
            continue;
        unsigned long theIndex = USavedID(theEffect->mLinkedEffect1);
        if (theIndex < EEGraphicEffect::sEffects.size())
            theEffect->mLinkedEffect1 = EEGraphicEffect::sEffects[theIndex];
        else
            theEffect->mLinkedEffect1 = NULL;
        theIndex = USavedID(theEffect->mLinkedEffect2);
        if (theIndex < EEGraphicEffect::sEffects.size())
            theEffect->mLinkedEffect2 = EEGraphicEffect::sEffects[theIndex];
        else
            theEffect->mLinkedEffect2 = NULL;
        theIndex = USavedID(theEffect->mLinkedEffect3);
        if (theIndex < EEGraphicEffect::sEffects.size())
            theEffect->mLinkedEffect3 = EEGraphicEffect::sEffects[theIndex];
        else
            theEffect->mLinkedEffect3 = NULL;
    }
    inFile->Close();
    return theResult;
}

// FUNCTION: EXE 0x4d6fe6
// Writes to the file every graphic effect that was used without being preloaded
void EEGraphicEffect::DumpIDs(FSFileSpec& inFile)
{
    std::set<unsigned long>::iterator theIterator;
    fprintf(inFile, "\n\nMEDIA DUMP OF ALL GRAPHIC EFFECTS THAT WERE NOT LOADED CORRECTLY\n\n");
    for (theIterator = EEGraphicEffect::sIDs.begin(); theIterator != EEGraphicEffect::sIDs.end(); ++theIterator)
        fprintf(inFile, "The Graphic Effect %d (name not available in release) was used without being preloaded, notify a programmer about this.\n",
                EEGraphicEffect::sEffects[*theIterator]->mID);
}

// FUNCTION: EXE 0x4d7048
// Releases every graphic effect in the table and empties it
void EEGraphicEffect::UnloadTable()
{
    for (std::vector<EEGraphicEffect*>::iterator theIterator = EEGraphicEffect::sEffects.begin(); theIterator != EEGraphicEffect::sEffects.end(); ++theIterator) {
        if (*theIterator != NULL)
            (*theIterator)->Dereference();
    }
    EEGraphicEffect::sEffects.clear();
}

// FUNCTION: EXE 0x4d7074
// Returns the effect's graphic, or a random graphic of its kind when it has none of its own
EEMedia* EEGraphicEffect::GetMedia()
{
    if (this->mGraphicObject == NULL) {
        long theKind = this->mKind;
        return EEGraphicEffect::sMediaByKind[theKind][gPhysics->GetRandom(0L, (long)(EEGraphicEffect::sMediaByKind[theKind].size() - 1))];
    }
    return static_cast<EEMedia*>(this->mGraphicObject);
}

// FUNCTION: EXE 0x4d70b7
// Files every loaded graphic object under its kind (kinds below 0x48) for kind-only effects
void EEGraphicEffect::SortMediaByKind()
{
    unsigned long theCount = EEMedia::GetCount();
    for (unsigned long i = 0; i < theCount; i++) {
        EEMedia* theMedia = EEMedia::Get(i);
        EEGraphicObject* theObject = static_cast<EEGraphicObject*>(theMedia);
        if (theMedia != NULL && theObject->mKind < 0x48)
            EEGraphicEffect::sMediaByKind[theObject->mKind].push_back(theMedia);
    }
}

// FUNCTION: EXE 0x4d7100
// Loads the effect's graphic (or every graphic of its kind) and its linked effects; on demand, records it as
// not preloaded
void EEGraphicEffect::Load(bool inIsOnDemand)
{
    unsigned long theStart = TMGetTime();
    this->mNumLoads++;
    if (this->mGraphicObject != NULL) {
        if (inIsOnDemand == true)
            this->RecordNotPreloaded();
        this->mGraphicObject->LoadMedia();
    } else {
        for (unsigned long i = 0; i < EEGraphicEffect::sMediaByKind[this->mKind].size(); i++)
            EEGraphicEffect::sMediaByKind[this->mKind][i]->LoadMedia();
    }
    if (this->mLinkedEffect1 != NULL)
        this->mLinkedEffect1->Load(inIsOnDemand);
    if (this->mLinkedEffect2 != NULL)
        this->mLinkedEffect2->Load(inIsOnDemand);
    if (this->mLinkedEffect3 != NULL)
        this->mLinkedEffect3->Load(inIsOnDemand);
    if (TMGetTime() - theStart > 1) {
        TSScheduler::GetCurrentThreadIndex();
        TMGetTime();
    }
}

// FUNCTION: EXE 0x4d71ac
// Unloads the effect's graphic (or every graphic of its kind) and its linked effects
void EEGraphicEffect::Unload()
{
    unsigned long theStart = TMGetTime();
    this->mNumLoads--;
    if (this->mGraphicObject != NULL) {
        this->mGraphicObject->UnloadMedia();
    } else {
        for (unsigned long i = 0; i < EEGraphicEffect::sMediaByKind[this->mKind].size(); i++)
            EEGraphicEffect::sMediaByKind[this->mKind][i]->UnloadMedia();
    }
    if (this->mLinkedEffect1 != NULL)
        this->mLinkedEffect1->Unload();
    if (this->mLinkedEffect2 != NULL)
        this->mLinkedEffect2->Unload();
    if (this->mLinkedEffect3 != NULL)
        this->mLinkedEffect3->Unload();
    if (TMGetTime() - theStart > 1) {
        TSScheduler::GetCurrentThreadIndex();
        TMGetTime();
    }
}

// FUNCTION: EXE 0x4d723c
// Records the effect's id for the dump when its graphic has not been loaded yet
void EEGraphicEffect::RecordNotPreloaded()
{
    if (this->mGraphicObject != NULL && this->mGraphicObject->mMediaCount == 0) {
        unsigned long theID = this->mID;
        EEGraphicEffect::sIDs.insert(theID);
    }
}

// SYNTHETIC: EXE 0x4d726c
// (EEMedia::~EEMedia's adjusting thunk into 0x4d6e5d: this - 8)

// SYNTHETIC: EXE 0x4d7274
// (EEMedia's ??_G, inline)

// FUNCTION: EXE 0x4d7291
// Returns the number of entries in the media table
unsigned long EEMedia::GetCount()
{
    return gMediaTable.size();
}

// FUNCTION: EXE 0x4d72a9
// Loads the effect as preloaded media; always returns 0
long EEGraphicEffect::LoadMedia()
{
    this->Load(false);
    return 0;
}

// FUNCTION: EXE 0x4d72b6
// Unloads the effect's media; always returns 0
long EEGraphicEffect::UnloadMedia()
{
    this->Unload();
    return 0;
}
