// ComponentConfig::ReadConfig_Unsafe in Rust (R6 WON round 34: ee_won_gui cfgload.rs over the one-statement ops of
// include/ee/wonapi/WONCfgLoad.inl) against the original body as it was (won_cfgload34_orig.h), both over one
// stand-in world that traces every call (won_cfgload34_world.h). A LAN game's lobby loads its files without failing
// and never asks for the parse time, so the failure paths, the exceptions and the parse-time report are this golden's.
//
// Every run draws a script (the binding, the parse's answer or exception and whether it sets SHOWPARSETIME, the
// glue's and the init's exceptions, the message box's, the timer's values with ties and large ones), a table given
// or not, a parser given or not, a current table or none, the parse-time flag's value and the resource folder; the
// two traces (every call, every reference count, every object made and deleted), the answer or the exception and
// the state after (the error text, the current table and its count, the flag, the live objects) must be identical.
// Then each op of the host in turn is mutated (its statement skipped, its answer changed, or a pointer op run twice):
// every mutation must make a mismatch. Exit code: the mismatches plus the mutations not caught.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <exception>
#include "rust/won_gui/compcfg.h"
#include "rust/won_gui/cfgload.h"

namespace Orig {
#include "won_cfgload34_world.h"
#include "won_cfgload34_orig.h"
}
namespace Port {
#include "won_cfgload34_world.h"
#include "ee/wonapi/WONCfgLoad.inl"
// the mutation pass: the host behind a wrapper that changes op sMutate (the entry's own statements, as
// ReadConfig_Unsafe has them, around the call)
static int sMutate = -1;
static intptr_t Mutated(void* f, EeWonCfgLoadOp op, void* c, intptr_t a, intptr_t b, intptr_t d, bool* ok)
{
    if ((int)op != sMutate) return CfgLoadHost(f, op, c, a, b, d, ok);
    typedef EeWonCfgLoadOp O;
    switch (op) {
    case O::TableHold: case O::TableNew: case O::ParserNew: CfgLoadHost(f, op, c, a, b, d, ok); return CfgLoadHost(f, op, c, a, b, d, ok);
    case O::ShowParseTime: case O::ParseTopLevel: return !CfgLoadHost(f, op, c, a, b, d, ok);
    case O::LinesParsed: return CfgLoadHost(f, op, c, a, b, d, ok) + 1;
    case O::Glue: case O::InitComponents: return CfgLoadHost(f, op, c, a, b, d, ok) == 0 ? EE_WON_CFG_LOAD_CAUGHT : 0;
    case O::TableSet: return CfgLoadHost(f, op, c, (intptr_t)new ResourceConfigTable, b, d, ok); // another table
    case O::ElapsedTime: case O::CountedTime: case O::PercentTime: CfgLoadHost(f, op, c, a, b, d, ok); *(double*)a += 1; return 0;
    default: return 0; // the statement skipped
    }
}
static bool ReadMutated(ComponentConfig* k, Component* c, const char* f, ResourceConfigTable* t, ConfigParser* p)
{
    CfgLoadFrame aFrame;
    const int r = ee_won_cfg_load(Mutated, &aFrame, k, c, f, t, p);
    if (r == 2) std::rethrow_exception(aFrame.mErr);
    return r == 1;
}
}

static unsigned sSeed = 20261123;
static unsigned Rnd(unsigned n) { sSeed = sSeed * 1103515245u + 12345u; return n ? (sSeed >> 8) % n : 0; }
static double Time()
{
    static const double kTies[] = {0.125, 0.375, 2.675, 1.005, 0.5, 0.0, 1e9, 12345.6789, 0.004999, 0.995};
    return Rnd(3) == 0 ? kTies[Rnd(10)] : (double)Rnd(1000000) / (double)(1 + Rnd(999));
}
struct Run {
    Orig::Script s; bool giveTable, giveParser, hasCurrent, show; std::string folder, file;
};
static Run MakeRun()
{
    Run r;
    r.s.setComponent = Rnd(12) == 0 ? 1 + (int)Rnd(2) : 0;
    r.s.parse = Rnd(10) < 6 ? 1 : (int)Rnd(4);
    r.s.parseShows = Rnd(3) == 0;
    r.s.lines = (int)Rnd(5000) - 10;
    r.s.glue = Rnd(4) == 0 ? 1 + (int)Rnd(2) : 0;
    r.s.init = Rnd(4) == 0 ? 1 + (int)Rnd(2) : 0;
    r.s.box = Rnd(6) == 0;
    r.s.elapsed = Time(); r.s.counted = Time(); r.s.percent = Rnd(4) == 0 ? Time() : (double)Rnd(1000) / 997.0;
    r.giveTable = Rnd(2); r.giveParser = Rnd(2); r.hasCurrent = Rnd(2); r.show = Rnd(2);
    static const char* const kFolders[] = {"Data/WONLobby Resources/", "res\\", "", "x/"};
    static const char* const kFiles[] = {"_LobbyMaster.cfg", "./a.cfg", "c:abs.cfg", "Lobby.cfg", ""};
    r.folder = kFolders[Rnd(4)]; r.file = kFiles[Rnd(5)];
    return r;
}
#define SIDE(NS)                                                                                                      \
    static std::string Play##NS(const Run& r, bool mutated)                                                           \
    {                                                                                                                 \
        using namespace NS;                                                                                           \
        sTrace.clear(); Live().clear(); sNextId = 0;                                                                  \
        memcpy(&NS::gScript, &r.s, sizeof r.s);                                                                       \
        gShowParseTime = r.show; gLinesParsed = 3;                                                                    \
        ComponentConfig::mResourceConfigTable = r.hasCurrent ? new ResourceConfigTable : NULL;                        \
        std::string out;                                                                                              \
        {                                                                                                             \
            SmartPtr<ComponentConfig> k = new ComponentConfig;                                                        \
            SmartPtr<Component> c = new Component;                                                                    \
            SmartPtr<ResourceConfigTable> t = r.giveTable ? new ResourceConfigTable : NULL;                          \
            ConfigParser* p = r.giveParser ? new ConfigParser : NULL;                                                 \
            tr("-- load");                                                                                            \
            try {                                                                                                     \
                bool ok = mutated ? Port::ReadMutated((Port::ComponentConfig*)(void*)k.get(), (Port::Component*)(void*)c.get(), r.file.c_str(), (Port::ResourceConfigTable*)(void*)t.get(), (Port::ConfigParser*)(void*)p) \
                                  : k->ReadConfig_Unsafe(c.get(), r.file.c_str(), t.get(), p);                        \
                out = "returned " + P(ok);                                                                            \
            } catch (ConfigObjectException& e) { out = "threw config " + e.mText; }                                   \
            catch (OtherException& e) { out = "threw other " + P(e.mCode); }                                          \
            tr(out + " error [" + k->mConfigError + "] table " + PP(ComponentConfig::mResourceConfigTable.get()) +   \
               " show " + P(gShowParseTime) + " lines " + P(gLinesParsed) + " component " + PP(k->mComponent.get()) + \
               " live" + Census());                                                                                   \
            delete p;                                                                                                 \
        }                                                                                                             \
        ComponentConfig::mResourceConfigTable = NULL;                                                                 \
        tr("after" + Census());                                                                                       \
        return sTrace;                                                                                                \
    }
SIDE(Orig)
SIDE(Port)

static void SetFolder(const std::string& f)
{
    ee_won_cc_call(NULL, NULL, NULL, EeWonCcCall::SetResourceFolder, 0, (intptr_t)f.data(), (intptr_t)f.size(), 0);
}

int main(int argc, char** argv)
{
    const int runs = argc > 1 ? atoi(argv[1]) : 20000;
    int bad = 0;
    size_t bytes = 0;
    std::vector<Run> all;
    for (int i = 0; i < runs; i++) all.push_back(MakeRun());
    for (int i = 0; i < runs; i++) {
        SetFolder(all[i].folder);
        std::string a = PlayOrig(all[i], false), b = PlayPort(all[i], false);
        bytes += a.size();
        if (a != b && bad++ < 3) printf("run %d differs:\n--- orig\n%s--- port\n%s", i, a.c_str(), b.c_str());
    }
    printf("won_cfgload34_test: %d runs, %u trace bytes, %d mismatching\n", runs, (unsigned)bytes, bad);
    // the mutations: each op changed in turn, over the first 2,000 runs
    int missed = 0, ops = (int)EeWonCfgLoadOp::ParseTimeBox + 1;
    for (int op = 0; op < ops; op++) {
        Port::sMutate = op;
        int caught = 0;
        for (int i = 0; i < runs && i < 2000 && !caught; i++) {
            SetFolder(all[i].folder);
            if (PlayOrig(all[i], false) != PlayPort(all[i], true)) caught++;
        }
        if (!caught) { missed++; printf("mutation of op %d not caught\n", op); }
    }
    Port::sMutate = -1;
    printf("won_cfgload34_test: %d mutations, %d caught\n", ops, ops - missed);
    return bad + missed;
}
