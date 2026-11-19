// New (port): the game log's host (ee/EEGameLog.h; its bodies are Rust: rust/crates/ee_game/src/game_log.rs). The
// file through the platform layer, the lobby options, a player's texts and colour row, a gather goal's resource, a
// unit's killer, the physics' clock.
#include <windows.h>
#include <vector>
#include <deque>
#include <map>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "lle/LowLevelEngine.h"
#include "lle/GE2DPoint.h"
#include "lle/GE3DPoint.h"
#include "lle/GEVector.h"
#include "lle/GEColor.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GE3DPlane.h"
#include "lle/GE3DLine.h"
#include "lle/GEViewport.h"
#include "lle/GETransformation.h"
#include "lle/GEPhysicalModel.h"
#include "lle/GETerrainMesh.h"
#include "lle/FSFile.h"
#include "lle/U2DSparseArray.h"
#include "lle/U2DPolygon.h"
#include "lle/USingleton.h"
#include "ee/EmpireEarth.h"
#include "ee/EEPhysics.h"
#include "ee/EEPlayer.h"
#include "ee/EEComputerPlayer.h"
#include "ee/EEUnit.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EECPResourceCenter.h"
#include "ee/EECPCombatManager.h"
#include "ee/EEUnitGoal.h"
#include "ee/EEUGRust.h"
#include "ee/EEUnitAction.h"
#include <math.h>
#include <mutex>
#include <ctime>
#include "platform/PlatPath.h"
#include "platform/PlatFile.h"
#include "platform/PlatSim.h"
#include "ee/EEGamePreferences.h"
#include "ee/EECPTrace.h"
#include "ee/EECPEngine.h"
#include "ee/EEGameLog.h"
#include "ee/EEColorTable.h"
#include "rust/game/game_log.h"

namespace {

FILE* sFile = NULL;

void Put(EeGameLogCall* c, const std::string& inText)
{
    snprintf(c->text, sizeof(c->text), "%s", inText.c_str());
}

}  // namespace

extern "C" int64_t ee_game_log_host(EeGameLogCall* c)
{
    EEPlayer* p = reinterpret_cast<EEPlayer*>((uintptr_t)c->handle);
    switch (c->op) {
    case EeGameLogOp::Wanted: {
        if (!gGamePreferences.GetSettings().mGameLog || gPlatSimMode)
            return 0;
        return PlatPathGetDataRoot(c->text, sizeof(c->text)) != 0;
    }
    case EeGameLogOp::MakeDirectory:
        PlatCreateDirectory(c->text);
        return 0;
    case EeGameLogOp::Open:   // the data root is in the game's C:\ form on POSIX too
        sFile = PlatFOpen(c->text, "w");
        if (sFile == NULL)
            Put(c, strerror(errno));
        return sFile != NULL;
    case EeGameLogOp::Write:
        if (sFile != NULL) {
            fputs(c->text, sFile);
            fflush(sFile);
        }
        return 0;
    case EeGameLogOp::Close:
        if (sFile != NULL)
            fclose(sFile);
        sFile = NULL;
        return 0;
    case EeGameLogOp::Options: {
        const EEGameOptions& theOptions = gWorld.mLobbyOptions;
        Put(c, (const char*)theOptions.GetMapType());
        c->i[0] = (unsigned long)theOptions.GetMapSize();
        c->i[1] = (unsigned long)theOptions.GetDifficulty();
        return 0;
    }
    case EeGameLogOp::Player: {
        EEComputerPlayer* ai = p->mComputerPlayer;
        c->i[0] = p->IsHuman() ? 0 : ai != NULL ? 1 : 2;
        c->i[1] = (unsigned long)EEColorTable::FindIndex(p->mColor);
        c->f[0] = p->mColor.mRed;
        c->f[1] = p->mColor.mGreen;
        c->f[2] = p->mColor.mBlue;
        Put(c, std::string((const char*)p->mName) + "\x1f" + (const char*)p->mCivilization.GetName() + "\x1f"
            + (ai != NULL ? EECPPersonality(ai).mID : std::string("-")) + "\x1f" + (ai != NULL ? EECPCheatNames(ai) : "-")
            + "\x1f" + (ai != NULL ? EECPEngineName(ai) : std::string("-")));
        return 0;
    }
    case EeGameLogOp::Civilization:
        Put(c, (const char*)p->mCivilization.GetName());
        return 0;
    case EeGameLogOp::GatherType: {
        EEUnit* r = static_cast<EEUGRust*>(reinterpret_cast<EEUnitGoal*>((uintptr_t)c->handle))->GetGatherResource();
        return r != NULL ? (int64_t)r->mResourceType : 0;
    }
    case EeGameLogOp::Killer: {   // all losses by the killer: a human's unit, a computer player's, none or wild
        EEComplexUnit* theKiller = reinterpret_cast<EEUnit*>((uintptr_t)c->handle)->mComplexUnit->mLastAttacker;
        EEPlayer* theOwner = theKiller != NULL ? theKiller->mPlayer : NULL;
        return theOwner == NULL || theOwner->mPlayerID == 0 ? 2 : theOwner->IsHuman() ? 0 : 1;
    }
    case EeGameLogOp::Physics:
        c->i[0] = gPhysics != NULL ? (unsigned long)gPhysics->mGameTime : 0;
        c->i[1] = gPhysics != NULL && gPhysics->mGameOver;
        return 0;
    case EeGameLogOp::TraceOn:   // the computer players' decision trace into the log too (EECPTraceOut)
        gCPTraceOn = true;
        return 0;
    case EeGameLogOp::FileName: {
        time_t theNow = time(NULL);
        struct tm theLocal;
#ifdef _WIN32
        localtime_s(&theLocal, &theNow);
#else
        localtime_r(&theNow, &theLocal);
#endif
        strftime(c->text, sizeof(c->text), "/game-%Y%m%d-%H%M%S.log", &theLocal);
        return 0;
    }
    }
    return 0;
}

bool EEGameLogOn() { return ee_game_log_on(); }
void EEGameLogTick(unsigned long inGameTime) { ee_game_log_tick(inGameTime); }
void EEGameLogLine(const char* inText) { ee_game_log_line(inText); }
void EEGameLogChat(unsigned long inPlayer, const char* inText) { ee_game_log_chat(inPlayer, inText); }

void EEGameLogIncome(EEPlayer* inPlayer, long inResource, long inAmount)
{
    if (inPlayer != NULL)
        ee_game_log_income(inPlayer->mPlayerID, inResource, inAmount);
}

void EEGameLogUnitLost(EEPlayer* inPlayer, EEUnit* inUnit)
{
    if (inUnit != NULL && inUnit->mUnitClass != NULL)
        ee_game_log_unit_lost((uint64_t)(uintptr_t)inPlayer, (uint64_t)(uintptr_t)inUnit);
}
