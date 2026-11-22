// C:\EEX\Empire Earth\Civilization Builder\EECBCivilizationFiles.cpp (file name inferred: no __FILE__ or RTTI of its
// own). 0x27fdfe..0x2813f4, after EECivilizationBuilder.cpp and before EECivilizationManager.cpp. The builder's half
// (its state, the file being saved, the builder's open and close, buys, returns, deletes, accept: CivBuilder*) is
// Rust's (ee_screens civ_whole.rs); this unit keeps the simulation's: the civilization timer's limits, the
// civilizations' directory, a civilization file's load (EELoadCivilization: found and read here, its bytes read by
// ee_tech civ_file.rs), its application to a player (EEApplyCivilizationFile: the AI's, the campaign's, the player
// resets') and the server's grant of a player's upgrade request (EECBRequestUpgrades), both Rust's (civ_file.rs)
// over this unit's host (EECivFileCall). Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include <list>

#include "lle/LowLevelEngine.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/NEMessage.h"
#include "lle/NESynchronousCommunications.h"
#include "lle/UResourceFile.h"

#include "ee/EmpireEarth.h"
#include "ee/EECommunications.h"
#include "ee/EEUIManager.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEMessage.h"


#include "ee/EECivilizationManager.h"
#include "platform/PlatMessage.h"
#include "ee/EEPlayer.h"
#include "ee/EEPhysics.h"
#include "ee/EECivilizationBuilder.h"
#include "ee/EEStringIDs.h"
#include "ee/EECivCapture.h"
#include "rust/sim/tech.h"
#include "rust/game/screens.h"   // the host (civ_file_calls.rs)
#include "ee/EEScreensShims.h"
extern char gEmptyString[];                 // 0x4e5b98 (pooled "")

// chain at 0x67fe94 (wrapper)
//   object 0x965518  ctor __imp_??0FSFileSpec@@QAE@XZ  dtor __imp_??1FSFileSpec@@UAE@XZ
// the civilization file being saved or loaded (the builder's host, EECivilizationBuilder.cpp, saves through it)
FSFileSpec gU565518;

// Returns the civilization choice time limit (300 s)
unsigned long EEGetCivTimerLimit()
{
    return 0x493e0;
}

// Returns the time of the civilization timer's first warning (240 s)
unsigned long EEGetCivTimerFirstWarning()
{
    return 0x3a980;
}

// Returns the time of the civilization timer's second warning (270 s)
unsigned long EEGetCivTimerSecondWarning()
{
    return 0x41eb0;
}

// Returns the civilization folder: the user's full path (Users\default\Civilizations\) or the relative one
UString EEGetCivilizationDirectory(bool inX)
{
    UString theDirectory;
    if (inX)
        theDirectory = gStartupDirectory + "Users\\default\\" + kCivilizationsKey + "\\";
    else
        theDirectory = kCivilizationsKey + "\\";
    return theDirectory;
}

// Reads a civilization file's name and upgrade ids from the data path or the given directory, then the capture's
// record: Rust's (civ_load.rs over the shims below; ee_tech civ_file.rs reads the bytes: the legacy binary form or
// the keyed text)
long EELoadCivilization(const UString& inName, const UString& inDirectory, UWideString& outName,
                        std::vector<EECivUpgrade>& outUpgrades)
{
    return ee_screens_civl_load((uintptr_t)&inName, (uintptr_t)&inDirectory, (uintptr_t)&outName, (uintptr_t)&outUpgrades);
}

// The C++ side of a file's application or a request (ee_tech civ_file.rs): the file's or the request's name, the
// upgrades, the player
struct EECivFileContext {
    UString* mFile;
    EEPlayer* mPlayer;
    unsigned char mPlayerIndex;
    UWideString mName;
    std::vector<EECivUpgrade> mUpgrades;
};

// the host: Rust's (civ_file_calls.rs) over the one-statement shims below (UI lane round 11)
static void EECivFileCall(void* inCtx, const EeTechCivFileCall* c, EeTechCivFileReply* r)
{
    ee_screens_civ_file_host(inCtx, c, r);
}

#define CF(x) static_cast<EECivFileContext*>((void*)(x))
static unsigned long sCFCost = 0;       // an upgrade's cost (CanAfford's)
static UWideString sCFFormat;           // the refusal's format and line
static UWideString sCFText;
extern "C" int64_t ee_screenscpp_civf_load(uintptr_t inCtx, bool inUser)
{ return EELoadCivilization(*CF(inCtx)->mFile, EEGetCivilizationDirectory(inUser), CF(inCtx)->mName, CF(inCtx)->mUpgrades); }
extern "C" uintptr_t ee_screenscpp_civf_name(uintptr_t inCtx) { return (uintptr_t)(const char*)CF(inCtx)->mName; }
extern "C" uint32_t ee_screenscpp_civf_name_characters(uintptr_t inCtx)
{ return (uint32_t)CF(inCtx)->mName.GetLength(); }
extern "C" uintptr_t ee_screenscpp_civf_upgrades(uintptr_t inCtx) { return (uintptr_t)CF(inCtx)->mUpgrades.data(); }
extern "C" uintptr_t ee_screenscpp_civf_upgrade_count(uintptr_t inCtx) { return CF(inCtx)->mUpgrades.size(); }
extern "C" void ee_screenscpp_civf_name_string(uintptr_t inCtx, int64_t inId)
{ gResource->GetString((long)inId, CF(inCtx)->mName); }
extern "C" void ee_screenscpp_civf_civ_clear(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.mPurchasedUpgrades.clear(); }
extern "C" void ee_screenscpp_civf_civ_points_to_max(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.mAvailablePoints = CF(inCtx)->mPlayer->mCivilization.mMaxPoints; }
extern "C" void ee_screenscpp_civf_civ_set_points_and_max(uintptr_t inCtx, uint64_t inPoints)
{ CF(inCtx)->mPlayer->mCivilization.SetPointsAndMax((unsigned long)inPoints); }
extern "C" void ee_screenscpp_civf_civ_set_name(uintptr_t inCtx)
{ CF(inCtx)->mPlayer->mCivilization.SetName(CF(inCtx)->mName); }
extern "C" void ee_screenscpp_civf_civ_can_afford(uintptr_t inCtx, uint32_t inUpgrade)
{ CF(inCtx)->mPlayer->mCivilization.CanAfford((EECivUpgrade)inUpgrade, &sCFCost, NULL); }
extern "C" uint64_t ee_screenscpp_civf_cost() { return (uint64_t)sCFCost; }
extern "C" uint64_t ee_screenscpp_civf_civ_max_points(uintptr_t inCtx)
{ return (uint64_t)CF(inCtx)->mPlayer->mCivilization.GetMaxPoints(); }
extern "C" void ee_screenscpp_civf_civ_buy(uintptr_t inCtx, uint32_t inUpgrade)
{ CF(inCtx)->mPlayer->mCivilization.Buy((EECivUpgrade)inUpgrade, CF(inCtx)->mPlayer->mPlayerID, true); }
extern "C" void ee_screenscpp_civf_civ_set_points(uintptr_t inCtx) { CF(inCtx)->mPlayer->mCivilization.SetPoints(0); }
extern "C" bool ee_screenscpp_civf_can_buy() { return ::gWorld.mGameOptions.CanBuyCivilization(); }
extern "C" bool ee_screenscpp_civf_purchased(uintptr_t inCtx) { return CF(inCtx)->mPlayer->mCivUpgradesPurchased; }
extern "C" bool ee_screenscpp_civf_custom_civs() { return gAllowCustomCivs; }
extern "C" bool ee_screenscpp_civf_scenario_game() { return gIsScenarioGame; }
extern "C" bool ee_screenscpp_civf_in_scenario_editor() { return gUIManager->GetInScenarioEditor(); }
extern "C" bool ee_screenscpp_civf_can_afford_all(uintptr_t inCtx)
{ return CF(inCtx)->mPlayer->mCivilization.CanAffordAll(CF(inCtx)->mUpgrades); }
extern "C" uintptr_t ee_screenscpp_civf_grant_new(uintptr_t inCtx, int64_t inPoints)
{ return (uintptr_t) new EEMRecord(kMessageCivUpgradesPurchased, {CF(inCtx)->mPlayerIndex, (long)inPoints, CF(inCtx)->mUpgrades, CF(inCtx)->mName}); }
extern "C" bool ee_screenscpp_civf_multiplayer() { return gCommunications->GetIsMultiplayerSession(); }
extern "C" void ee_screenscpp_civf_send(uintptr_t inMessage)
{ gCommunications->ProcessLocalUserInput((NEMessage*)inMessage, kSlotNone); }
extern "C" void ee_screenscpp_civf_execute(uintptr_t inMessage) { ((NEMessage*)inMessage)->Execute(); }
extern "C" void ee_screenscpp_civf_release(uintptr_t inMessage) { ((NEMessage*)inMessage)->Dereference(); }
extern "C" void ee_screenscpp_civf_refuse_format()
{ gResource->GetString(IDS_PLAYER_HAS_CIVILIZATION_NAMED_WHICH, sCFFormat); }
extern "C" void ee_screenscpp_civf_refuse_print(uintptr_t inCtx)
{ sprintf(sCFText, sCFFormat, (const char*)CF(inCtx)->mPlayer->mName, (const char*)CF(inCtx)->mName, (const char*)::gWorld.GetCurrentPlayer()->mName); }
extern "C" void ee_screenscpp_civf_chat_type()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageSelectPlayerChatType, {0L}), kSlotNone); }
extern "C" void ee_screenscpp_civf_chat()
{ gCommunications->ProcessLocalUserInput(new EEMRecord(kMessageInGameChat, {sCFText}), kSlotNone); }
extern "C" void ee_screenscpp_civf_random() { gPhysics->GetRandom(0L, 1L); }
extern "C" void ee_screenscpp_civf_strings_clear() { sCFFormat = UWideString(); }
extern "C" void ee_screenscpp_civf_text_clear() { sCFText = UWideString(); }

// Gives a player the civilization from a file (the game's, else the user's), buying its upgrades at no cost;
// a game file's premade civilization keeps its localized name (ee_tech civ_file.rs apply_file)
long EEApplyCivilizationFile(UString& ioName, EEPlayer* inPlayer)
{
    UString theName = ioName;
    EECivFileContext theContext{&ioName, inPlayer, 0, UWideString(), {}};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    long theResult = ee_tech_civ_apply_file(&theHost, (const char*)ioName);
    EECivCaptureApplyFile(theName, inPlayer, theResult);   // TEST ONLY: CVAF
    return theResult;
}

// Grants a player's civilization upgrade request when allowed, valid and affordable (broadcast in multiplayer);
// otherwise posts a chat line naming the refused civilization (ee_tech civ_file.rs request_upgrades)
void EECBRequestUpgrades(unsigned char inPlayer, UWideString& inName, std::vector<EECivUpgrade>& inUpgrades)
{
    EECivFileContext theContext{NULL, ::gWorld.mPlayers[inPlayer], inPlayer, inName, inUpgrades};
    EeTechCivFileHost theHost{&theContext, &EECivFileCall};
    ee_tech_civ_request_upgrades(&theHost);
}

// EELoadCivilization's file, read and results (civ_load.rs, UI lane round 12)
static std::vector<unsigned char> sCivlBytes;   // the file's bytes (the original's local)
static unsigned long sCivlGot = 0;
extern "C" void ee_screenscpp_civl_string(uintptr_t inString, void* inOut)
{ EEShimOut(inOut, *(const UString*)inString); }
extern "C" void ee_screenscpp_civl_global_close() { gU565518.Close(); }
extern "C" int32_t ee_screenscpp_civl_find(EeScreensShimText inPath, uintptr_t* inOut)
{ return gDataPath->FindFile(EEShimString(inPath), *reinterpret_cast<FSFileSpec**>(inOut)); }
extern "C" uintptr_t ee_screenscpp_civl_spec_new() { return (uintptr_t)new FSFileSpec; }
extern "C" void ee_screenscpp_civl_global_volume() { gU565518.SetVolume(gStartupVolume); }
extern "C" void ee_screenscpp_civl_global_directory(EeScreensShimText inDirectory)
{ gU565518.SetDirectory(EEShimString(inDirectory)); }
extern "C" void ee_screenscpp_civl_global_file(EeScreensShimText inFile) { gU565518.SetFile(EEShimString(inFile)); }
extern "C" void ee_screenscpp_civl_spec_assign_global(uintptr_t inSpec) { *(FSFileSpec*)inSpec = gU565518; }
extern "C" int32_t ee_screenscpp_civl_open(uintptr_t inSpec) { return ((FSFileSpec*)inSpec)->Open(kFileAccessRead); }
extern "C" int32_t ee_screenscpp_civl_length(uintptr_t inSpec, unsigned long* inOut)
{ return ((FSFileSpec*)inSpec)->GetLength(*inOut); }
extern "C" void ee_screenscpp_civl_buffer_resize(uint64_t inSize) { sCivlBytes.resize((size_t)inSize); }
extern "C" int32_t ee_screenscpp_civl_read(uintptr_t inSpec, uint64_t inSize)
{ return ((FSFileSpec*)inSpec)->Read(sCivlBytes.data(), (unsigned long)inSize, sCivlGot); }
extern "C" void ee_screenscpp_civl_buffer(void* inOut)
{ ee_screens_shim_out(inOut, sCivlBytes.data(), sCivlBytes.size()); }
extern "C" void ee_screenscpp_civl_set_name(uintptr_t inName, EeScreensShimText inText)
{ *(UWideString*)inName = EEShimText(inText); }
extern "C" void ee_screenscpp_civl_upgrade_push(uintptr_t inList, uint32_t inUpgrade)
{ ((std::vector<EECivUpgrade>*)inList)->push_back(inUpgrade); }
extern "C" void ee_screenscpp_civl_message(EeScreensShimText inText)
{ PlatMessageShow("Empire Earth: civilization", (const char*)EEShimString(inText), PLAT_MESSAGE_ERROR); }
extern "C" void ee_screenscpp_civl_close(uintptr_t inSpec) { ((FSFileSpec*)inSpec)->Close(); }
extern "C" void ee_screenscpp_civl_delete(uintptr_t inSpec) { delete (FSFileSpec*)inSpec; }
extern "C" void ee_screenscpp_civl_capture(uintptr_t inName, uintptr_t inDirectory, int64_t inResult, uintptr_t inOutName, uintptr_t inOutUpgrades)
{ EECivCaptureLoad(*(const UString*)inName, *(const UString*)inDirectory, (long)inResult, *(const UWideString*)inOutName, *(const std::vector<EECivUpgrade>*)inOutUpgrades); }
