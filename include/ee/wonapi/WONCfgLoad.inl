#include "rust/won_gui/cfgload.h"
// The load of a config file (R6 WON round 34): ComponentConfig::ReadConfig_Unsafe is ee_won_gui's cfgload.rs over
// this host's one-statement ops. Included inside a namespace by ComponentConfigHost.cpp (WONAPI) and by
// tests/won/won_cfgload34_test.cpp (over its stand-ins), so both run these very statements. An op that can throw runs
// under the guard; the glue and the init catch a ConfigObjectException as the original did.
struct CfgLoadFrame { std::exception_ptr mErr; std::string mCaught; };
// f() as the op's answer; any exception kept in the frame (the op answers 0 with *ok cleared)
template<class F> static intptr_t CfgLoadGuard(void* inFrame, bool* ok, F f)
{
    try { return (intptr_t)f(); } catch (...) { static_cast<CfgLoadFrame*>(inFrame)->mErr = std::current_exception(); *ok = false; return 0; }
}
// f() run; a ConfigObjectException caught with its text kept (EE_WON_CFG_LOAD_CAUGHT), any other kept as the guard's
template<class F> static intptr_t CfgLoadCatch(void* inFrame, bool* ok, F f)
{
    try { f(); return 0; }
    catch (ConfigObjectException& e) { static_cast<CfgLoadFrame*>(inFrame)->mCaught = e.mText; return EE_WON_CFG_LOAD_CAUGHT; }
    catch (...) { static_cast<CfgLoadFrame*>(inFrame)->mErr = std::current_exception(); *ok = false; return 0; }
}
#define CFG_LOAD_G(...) CfgLoadGuard(inFrame, ok, [&] { return (intptr_t)(__VA_ARGS__); })
#define CFG_LOAD_GV(...) CfgLoadGuard(inFrame, ok, [&] { __VA_ARGS__; return (intptr_t)0; })
#define CFG_LOAD_CATCH(...) CfgLoadCatch(inFrame, ok, [&] { __VA_ARGS__; })
static intptr_t CfgLoadHost(void* inFrame, EeWonCfgLoadOp op, void* c, intptr_t a, intptr_t b, intptr_t d, bool* ok)
{
    typedef EeWonCfgLoadOp O;
    ComponentConfig* k = (ComponentConfig*)c;
    switch (op) {
    case O::SetComponent: return CFG_LOAD_GV(k->SetComponent((Component*)a));
    case O::SetCacheImages: WindowManager::GetDefaultWindowManager()->SetCacheImages(a != 0); return 0;
    case O::TableHold: return (intptr_t)new SmartPtr<ResourceConfigTable>(ComponentConfig::mResourceConfigTable);
    case O::TableNew: return (intptr_t)new ResourceConfigTable;
    case O::TableSet: ComponentConfig::mResourceConfigTable = (ResourceConfigTable*)a; return 0;
    case O::TableRestore: ComponentConfig::mResourceConfigTable = *(SmartPtr<ResourceConfigTable>*)c; return 0;
    case O::TableDrop: delete (SmartPtr<ResourceConfigTable>*)c; return 0;
    case O::ShowParseTime: return gShowParseTime;
    case O::SetShowParseTime: gShowParseTime = a != 0; return 0;
    case O::ParserNew: return (intptr_t)new ConfigParser;
    case O::ParserFree: delete (ConfigParser*)c; return 0;
    case O::WarningsAsErrors: ((ConfigParser*)c)->SetWarningsAsErrors(true); return 0;
    case O::ParseTopLevel: return CFG_LOAD_G(k->ParseTopLevel(*(ConfigParser*)a, std::string((const char*)b, (size_t)d).c_str()));
    case O::CloseFile: ((ConfigParser*)c)->CloseFile(); return 0;
    case O::AbortReason: k->mConfigError = ((ConfigParser*)a)->GetAbortReason(); return 0;
    case O::Glue: return CFG_LOAD_CATCH(ComponentConfig::mResourceConfigTable->SafeGlueResources());
    case O::InitComponents: return CFG_LOAD_CATCH(k->InitComponents());
    case O::CaughtError: k->mConfigError = GUIString(static_cast<CfgLoadFrame*>(inFrame)->mCaught); return 0;
    case O::LinesParsed: return gLinesParsed;
    case O::ElapsedTime: *(double*)a = PerfTimer::GetElapsedTime(); return 0;
    case O::CountedTime: *(double*)a = PerfTimer::GetCountedTime(); return 0;
    case O::PercentTime: *(double*)a = PerfTimer::GetPercentTime(); return 0;
    case O::ParseTimeBox: return CFG_LOAD_GV(MSMessageBoxMsg(Window::GetDefaultWindow(), GUIString("Config Parse Time"), GUIString(std::string((const char*)a, (size_t)b)), gEmptyGUIString, gEmptyGUIString, gEmptyGUIString));
    default: return 0;
    }
}
#undef CFG_LOAD_G
#undef CFG_LOAD_GV
#undef CFG_LOAD_CATCH
// Parses the configuration file, glues its resources and initialises every configured component; on failure
// returns false with the error text, leaving the resource table, parse-time flag and image caching as set
bool ComponentConfig::ReadConfig_Unsafe(Component* component, const char* fileName, ResourceConfigTable* table, ConfigParser* parser)
{
    CfgLoadFrame aFrame;
    const int r = ee_won_cfg_load(CfgLoadHost, &aFrame, this, component, fileName, table, parser);
    if (r == 2) std::rethrow_exception(aFrame.mErr);
    return r == 1;
}
