// EEDbFindFile: the game's seam for the db tables (new unit, not in the original game). The database loaders look
// their files up through this instead of FSPath::FindFile. A table is read only in its text form (tools/eedata: the
// .dat's name with .csv or .yaml): it is compiled (the Rust compiler, rust/crates/ee_data) into the table's record
// stream and served as an FSMemoryFile, so the loaders read the records they always read. There is no binary db file
// at run time (RUST_PLAN D9): a table without its text, or one with no text form, stops the game with its name.
// The path's own order applies: a Data\db\<table>.csv earlier on the path wins.
// The text tables reference each other's rows by id: one compiler content serves every table, reading the other
// tables' texts through the same path when a reference needs their ids (each text is parsed once).
// EE_DUMP_CONTENT=<file> (tests): every db table served is appended to <file> as the content dump
// (docs/MODDING_PLAN.md gate G1).
// port (game core lane, round 10): the bodies are Rust (ee_game gamecpp db_text.rs); the functions forward, their C++
// calls are EECore10OpB5's cases.
#include <windows.h>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "platform/PlatMessage.h"
#include "lle/GEColor.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"

#include "ee/EEDatabaseTables.h"
#include "ee/EECPBehavior.h"
#include "ee/EETechTreeRecord.h"
#include "ee/EERules.h"

#include "EEDataTextSpecs.h"
#include <stdio.h>
#include <string.h>
#include <utility>
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "ee/EEDbRecords.h"
#include <EEDatabase.h>
#include "lle/GETextureSet.h"
#include "ee/EEUnit.h"
#include "ee/EEButtonObject.h"
#include "lle/UIHotKey.h"
#include "lle/GEScreen.h"
#include "lle/GERasterizer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEAnimal.h"
#include "ee/EEMedia.h"
#include <stddef.h>
#include <stdint.h>
#include "ee/EETerrainColorTable.h"

// The generated specs against the record structs the loaders read (the rest are checked by eedata.py check (c)
// against their sources' static_asserts)
static_assert(sizeof(EEWorldTableEntry) == kEEDTSize_EEWorldTableEntry, "dbworld record layout");
static_assert(sizeof(EECPBehavior) == kEEDTSize_EECPBehavior, "dbcpbehavior record layout");
static_assert(sizeof(EETechTreeRecord) == kEEDTSize_EETechTreeRecord, "dbtechtree record layout");
static_assert(sizeof(EEUnitClassRecord) == kEEDTSize_EEUnitClassRecord, "dbobjects record layout");

long EEDbFindFile(FSPath* inPath, const UString& inName, FSFileSpec*& outFile)
{
    return ee_gamecpp_db_find_file(inPath, (const char*)inName, (void**)&outFile);
}

void EELoadRules(FSPath* inPath)
{
    ee_gamecpp_db_load_rules(inPath);
}

// ------------------------------------------------------------------------- round 10, lane B: the text tables' C++

int64_t EECore10OpD(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f);

#define P64(x) ((int64_t)(intptr_t)(x))
#define XSPEC ((FSFileSpec*)p)

#define TT ((EETerrainTable*)p)
extern long gDefaultTerrain, gFirstCliffTerrain;   // EETerrainTable.cpp
#define CLIFF ((EECliffTerrainTable*)p)
extern unsigned long& gTexturePixelDepth;
extern unsigned long& gTerrainMIPLevelsToDrop;
extern bool& gTrilinearFilter;               // 0x5308da (mip filtering on)
extern GETextureManager* gTextureManager;  // 0x531918 (ee/EmpireEarth.h)
#define BTN ((EEButtonObject*)p)
#define BTEX ((GETexture*)p)
#define BMP ((GEBitmap*)p)
#define ANIMAL ((EEAnimal*)p)
#define SURF ((GESurface*)p)
// the text tables' C++ calls (gamecpp db_text.rs; p the path, the file or the id map): one statement each
int64_t EECore10OpB5(EeGameCpp inOp, uintptr_t p, const int64_t* a, const float* f)
{
    typedef EeGameCpp O;
    switch (inOp) {
    case O::DxFindFile: return ((FSPath*)p)->FindFile(UString((const char*)a[0]), *(FSFileSpec**)a[1]);
    case O::DxOpen: return XSPEC->Open(kFileAccessRead);
    case O::DxLength: return XSPEC->GetLength(*(unsigned long*)a[0]);
    case O::DxDelete: delete XSPEC;
        break;
    case O::DxMessage: PlatMessageShow((const char*)a[0], (const char*)a[1], PLAT_MESSAGE_ERROR);
        break;
    case O::DxVerify: ESVerify(__FILE__, __LINE__, (const char*)a[0]);
        break;
    case O::DxMemoryFile: return P64(static_cast<FSFileSpec*>(new FSMemoryFile((unsigned char*)memcpy(new unsigned char[a[1] != 0 ? a[1] : 1], (const void*)a[0], (size_t)a[1]), (unsigned long)a[1], 1000, true)));
    case O::DxRulesHandle: return P64(const_cast<EeDataRules*>(EEGetRules().RustRules()));
    case O::DxRulesRefresh: EEGetRules().Refresh();
        break;
    // (EECore10OpB2) the terrain tables' C++ calls and members (gamecpp db_media.rs; p the terrain or the cliff terrain): one statement
    // each
    case O::DtTerrainFields: return P64(static_cast<EeDataTerrainFields*>(TT));
    case O::DtTerrainUse: return P64(static_cast<EeDataMediaUse*>(TT));
    case O::DtTerrainLock: return P64(sDatabaseLock.mLock);
    case O::DtTerrainNew: return P64(new EETerrainTable);
    case O::DtTerrainDelete: delete TT;
        break;
    case O::DtTerrainResize: EETerrainTable::sTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DtTerrainAt: return P64(EETerrainTable::sTable[a[0]]);
    case O::DtTerrainSet: EETerrainTable::sTable[a[0]] = (EETerrainTable*)a[1];
        break;
    case O::DtDefaultTerrain: return P64(&gDefaultTerrain);
    case O::DtFirstCliffTerrain: return P64(&gFirstCliffTerrain);
    case O::DtTerrainFirstTexture: return P64(&TT->mFirstTexture);
    case O::DtTerrainTextures: return P64(&TT->mTextures);
    case O::DtTerrainPreview: return P64(&TT->mPreviewTexture);
    case O::DtTerrainLoadTextureName: TT->mTextureName.Load(*(FSFile*)a[0]);
        break;
    case O::DtTerrainTextureName: return P64((const char*)TT->mTextureName);
    case O::DtTerrainCopyName: EEDbCopyName(TT->mName, *(const EeDataName*)a[0]);
        break;
    case O::DtTerrainName: return P64((const char*)TT->mName);
    case O::DtTerrainNameLength: return (int64_t)TT->mName.GetLength();
    case O::DtTerrainPushClass: TT->mUnitClasses.push_back((long)a[0]);
        break;
    case O::DtTerrainClassCount: return (int64_t)TT->mUnitClasses.size();
    case O::DtTerrainClass0: return TT->mUnitClasses[0];
    case O::DtTerrainColor: TT->mColor = GEColor(f[0], f[1], f[2]);
        break;
    case O::DtTerrainNewTextures: TT->mTextures = new GETexture*[(unsigned long)a[0]];
        break;
    case O::DtTerrainDeleteTextures: delete[] std::exchange(TT->mTextures, nullptr);
        break;
    case O::DtTerrainLoadTexture: return P64(gTextureManager->LoadTexture(UString((const char*)a[0]), GETextureDescription{1, 0, gTexturePixelDepth, gTerrainMIPLevelsToDrop, false, false, TT->mHasAlpha, true}));
    case O::DtTexSetFrame: ((GETexture*)p)->SetFrame((unsigned long)a[0]);
        break;
    case O::DtTexDereference: ((GETexture*)p)->Dereference();
        break;
    case O::DtMeshTextureSet: return P64(((GETerrainMesh*)p)->GetRegisteredTextureSet((unsigned long)a[0]));
    case O::DtSetTexture: ((GETextureSet*)p)->SetTexture((unsigned long)a[0], (unsigned long)a[1], (GETexture*)a[2]);
        break;
    case O::DtClassButton: return P64(((EEUnitClass*)p)->mButton);
    case O::DtCliffNew: return P64(new EECliffTerrainTable);
    case O::DtCliffDelete: delete CLIFF;
        break;
    case O::DtCliffFields: return P64(static_cast<EeDataCliffTerrainFields*>(CLIFF));
    case O::DtCliffCopyName: EEDbCopyName(CLIFF->mName, *(const EeDataName*)a[0]);
        break;
    case O::DtCliffResize: EETerrainTable::sCliffTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DtCliffCount: return (int64_t)EETerrainTable::sCliffTable.size();
    case O::DtCliffAt: return P64(EETerrainTable::sCliffTable[a[0]]);
    case O::DtCliffSet: EETerrainTable::sCliffTable[a[0]] = (EECliffTerrainTable*)a[1];
        break;
    case O::DtCliffClear: EETerrainTable::sCliffTable.clear();
        break;
    case O::DtFileReadRecord: return ((FSFileSpec*)p)->Read((unsigned char*)a[0], (unsigned long)a[1], *(unsigned long*)a[2]);
    case O::DtFilePrint: fprintf(*(FSFileSpec*)p, "%s", (const char*)a[0]);
        break;
    // (EECore10OpB3) the buttons' C++ calls and members (gamecpp db_media.rs; p the button, the texture or the bitmap): one statement each
    case O::DbtFields: return P64(static_cast<EeDataButtonFields*>(BTN));
    case O::DbtUse: return P64(static_cast<EeDataMediaUse*>(BTN));
    case O::DbtNew: return P64(new EEButtonObject);
    case O::DbtDelete: delete BTN;
        break;
    case O::DbtResize: EEButtonObject::sTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DbtCount: return (int64_t)EEButtonObject::sTable.size();
    case O::DbtAt: return P64(EEButtonObject::sTable[a[0]]);
    case O::DbtSet: EEButtonObject::sTable[a[0]] = (EEButtonObject*)a[1];
        break;
    case O::DbtCopyName: EEDbCopyName(BTN->mName, *(const EeDataName*)a[0]);
        break;
    case O::DbtName: return P64((const char*)BTN->mName);
    case O::DbtTexture: return P64(&BTN->mTexture);
    case O::DbtCellU: return P64(&BTN->mCellU);
    case O::DbtCellV: return P64(&BTN->mCellV);
    case O::DbtLoadTexture: return P64(gTextureManager->LoadTexture(BTN->mName, *(const GETextureDescription*)a[0]));
    case O::DbtReloadTexture: return gTextureManager->ReloadTexture(BTN->mName, *(const GETextureDescription*)a[0]);
    case O::DbtPixelDepth: return (int64_t)gTexturePixelDepth;
    case O::DbtScreenWidth: return (int64_t)gRasterizer->GetPrimarySurface(gScreenIndex)->GetWidth();
    case O::DbtTexWidth: return (int64_t)BTEX->GetWidth();
    case O::DbtTexHeight: return (int64_t)BTEX->GetHeight();
    case O::DbtTexMipDrop: return (int64_t)BTEX->GetNumMipLevelsToDrop();
    case O::DbtTexSurface: return P64(BTEX->GetSurface());
    case O::DbtTexSurfaceHeight: return (int64_t)BTEX->GetSurface()->GetHeight();
    case O::DbtBitmapTexture: BMP->SetTexture((GETexture*)a[0], (unsigned long)a[1]);
        break;
    case O::DbtBitmapCoordinates: BMP->SetTextureCoordinates(GETextureCoordinate(f[0], f[1]), GETextureCoordinate(f[2], f[1]), GETextureCoordinate(f[0], f[3]), GETextureCoordinate(f[2], f[3]));
        break;
    case O::DbtBitmapScale: BMP->SetXYScale(f[0], f[1]);
        break;
    case O::DbtHotKey: return P64(UIHotKey::GetHotKey((unsigned long)a[0]));
    // (EECore10OpB4) the animals' C++ calls and members (gamecpp db_media.rs; p the animal or the medium): one statement each
    case O::DanFields: return P64(static_cast<EeDataAnimalFields*>(ANIMAL));
    case O::DanNew: return P64(new EEAnimal);
    case O::DanDelete: delete ANIMAL;
        break;
    case O::DanResize: EEAnimal::sTable.resize((unsigned long)a[0], NULL);
        break;
    case O::DanCount: return (int64_t)EEAnimal::sTable.size();
    case O::DanAt: return P64(EEAnimal::sTable[a[0]]);
    case O::DanSet: EEAnimal::sTable[a[0]] = (EEAnimal*)a[1];
        break;
    case O::DanClear: EEAnimal::sTable.clear();
        break;
    case O::DanMedia: return P64(&ANIMAL->mMedia);
    case O::DanSoundCount: return (int64_t)ANIMAL->mSounds.size();
    case O::DanSoundAt: return P64(ANIMAL->mSounds[a[0]]);
    case O::DanSoundPush: ANIMAL->mSounds.push_back((EEMedia*)a[0]);
        break;
    case O::DanSoundsClear: ANIMAL->mSounds.clear();
        break;
    case O::DanListPush: ANIMAL->mL04.push_back((long)a[0]);
        break;
    case O::DanMediaGet: return P64(EEMedia::Get((unsigned long)a[0]));
    case O::DanMediaLoad: ((EEMedia*)p)->LoadMedia();
        break;
    case O::DanMediaUnload: ((EEMedia*)p)->UnloadMedia();
        break;
    // (EECore10OpB9) the colour table's C++ calls (gamecpp db_media.rs; p the surface or the colour): one statement each; the end of
    // lane B's chain
    case O::DcVerify: ESVERIFY_AT(a[0] != 0, a[1]);
        break;
    case O::DcSurfaceLoad: return SURF->Load(*(FSFileSpec*)a[0], kImageFilePNG);
    case O::DcBytesPerRow: return (int64_t)SURF->GetBytesPerRow();
    case O::DcWidth: return (int64_t)SURF->GetWidth();
    case O::DcHeight: return (int64_t)SURF->GetHeight();
    case O::DcNew: return P64(new EETerrainColorTable);
    case O::DcDelete: delete (EETerrainColorTable*)p;
        break;
    case O::DcSetColor: ((EETerrainColorTable*)p)->mColor = GEColor(f[0], f[1], f[2]);
        break;
    case O::DcResize: EETerrainColorTable::sTable.resize((unsigned long)a[0]);
        break;
    case O::DcCount: return (int64_t)EETerrainColorTable::sTable.size();
    case O::DcAt: return P64(EETerrainColorTable::sTable[a[0]]);
    case O::DcSet: EETerrainColorTable::sTable[a[0]] = (EETerrainColorTable*)a[1];
        break;
    default: return EECore10OpD(inOp, p, a, f);
    }
    return 0;
}
