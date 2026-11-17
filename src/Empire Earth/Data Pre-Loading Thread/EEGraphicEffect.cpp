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
#include "ee/EEDbRecords.h"

// The unit's three dynamic initializers open it (0xd6bed..0xd6ceb), in definition order; one guard byte 0x4e8418
// (bits 1/2/4) for the three atexit destructors. .bss layout is reverse definition order.
// chain at 0x4d6bed: sEffects 0x4e8420 (allocator byte, _Buy(0)), atexit dtor 0x4d6c1b (_Tidy)
std::vector<EEGraphicEffect*> EEGraphicEffect::sEffects;
// chain at 0x4d6c36: 0x4e7f98, the media of each kind (0x48 kinds; provisional name)
std::vector<EEMedia*> EEGraphicEffect::sMediaByKind[0x48];
// chain at 0x4d6c9a: 0x4e7f88, the effect ids (provisional name)
std::set<unsigned long> EEGraphicEffect::sIDs;
extern std::vector<EEMedia*> gMediaTable;   // 0x4e8448 (EEAnimal.cpp)

// Creates a graphic effect from its table record's members (ee_data tables.rs) and its graphic; LoadTable links the
// follow-on effects
EEGraphicEffect::EEGraphicEffect(const EeDataGraphicEffectFields& inFields, EEGraphicObject* inGraphicObject)
    : EeDataGraphicEffectFields(inFields)
{
    this->mGraphicObject = inGraphicObject;
    this->mLinkedEffect1 = NULL;
    this->mLinkedEffect2 = NULL;
    this->mLinkedEffect3 = NULL;
    this->mNumLoads = 0;
}

// Destroys the graphic effect (nothing of its own to free)
EEGraphicEffect::~EEGraphicEffect()
{
}

// Reads the graphic effect table from the file (the record and its copy are ee_data tables.rs'; rows whose id is not
// above 0 stay NULL) and links each effect's three follow-on effects by index; returns the last read result
long EEGraphicEffect::LoadTable(FSFile* inFile)
{
    std::vector<EeDataGraphicEffectLinks> theLinks;
    long theResult = EEDbReadRecords(inFile, ee_data_graphic_effect_record_size(),
        [&](unsigned long inCount) {
            EEGraphicEffect::sEffects.resize(inCount, NULL);
            theLinks.resize(inCount);
            return true;
        },
        [&](unsigned long inIndex, const unsigned char* inRecord) {
            EeDataGraphicEffectFields theFields;
            EEGraphicEffect* theEffect = NULL;
            if (ee_data_graphic_effect_from_record(inRecord, &theFields, &theLinks[inIndex]))
                theEffect = new EEGraphicEffect(theFields,
                    static_cast<EEGraphicObject*>(EEMedia::Get(theLinks[inIndex].media)));
            EEGraphicEffect::sEffects[inIndex] = theEffect;
        });
    for (unsigned long j = 0; j < EEGraphicEffect::sEffects.size() && j < theLinks.size(); j++) {
        EEGraphicEffect* theEffect = EEGraphicEffect::sEffects[j];
        if (theEffect == NULL)
            continue;
        EEGraphicEffect** theLinked[3] = {&theEffect->mLinkedEffect1, &theEffect->mLinkedEffect2, &theEffect->mLinkedEffect3};
        for (int k = 0; k < 3; k++) {
            unsigned long theIndex = theLinks[j].linked[k];
            *theLinked[k] = theIndex < EEGraphicEffect::sEffects.size() ? EEGraphicEffect::sEffects[theIndex] : NULL;
        }
    }
    return theResult;
}

// Writes to the file every graphic effect that was used without being preloaded
void EEGraphicEffect::DumpIDs(FSFileSpec& inFile)
{
    std::set<unsigned long>::iterator theIterator;
    fprintf(inFile, "\n\nMEDIA DUMP OF ALL GRAPHIC EFFECTS THAT WERE NOT LOADED CORRECTLY\n\n");
    for (theIterator = EEGraphicEffect::sIDs.begin(); theIterator != EEGraphicEffect::sIDs.end(); ++theIterator)
        fprintf(inFile, "The Graphic Effect %ld (name not available in release) was used without being preloaded, notify a programmer about this.\n",
                EEGraphicEffect::sEffects[*theIterator]->mID);
}

// Releases every graphic effect in the table and empties it
void EEGraphicEffect::UnloadTable()
{
    for (std::vector<EEGraphicEffect*>::iterator theIterator = EEGraphicEffect::sEffects.begin(); theIterator != EEGraphicEffect::sEffects.end(); ++theIterator) {
        if (*theIterator != NULL)
            (*theIterator)->Dereference();
    }
    EEGraphicEffect::sEffects.clear();
}

// Returns the effect's graphic, or a random graphic of its kind when it has none of its own
EEMedia* EEGraphicEffect::GetMedia()
{
    if (this->mGraphicObject == NULL) {
        long theKind = this->mKind;
        return EEGraphicEffect::sMediaByKind[theKind][gPhysics->GetRandom(0L, (long)(EEGraphicEffect::sMediaByKind[theKind].size() - 1))];
    }
    return static_cast<EEMedia*>(this->mGraphicObject);
}

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

// Records the effect's id for the dump when its graphic has not been loaded yet
void EEGraphicEffect::RecordNotPreloaded()
{
    if (this->mGraphicObject != NULL && this->mGraphicObject->mMediaCount == 0) {
        unsigned long theID = this->mID;
        EEGraphicEffect::sIDs.insert(theID);
    }
}

// Returns the number of entries in the media table
unsigned long EEMedia::GetCount()
{
    return gMediaTable.size();
}

// Loads the effect as preloaded media; always returns 0
long EEGraphicEffect::LoadMedia()
{
    this->Load(false);
    return 0;
}

// Unloads the effect's media; always returns 0
long EEGraphicEffect::UnloadMedia()
{
    this->Unload();
    return 0;
}
