// C:\EEX\Low-Level Engine\PIMModule.cpp (folder and file name inferred: sorts after Network Engine.cpp, before
// the Sound Engine's SECD.cpp)
// The plug-in registry. The original searched a directory for *.dll files exporting PIMGetName/PIMGetType/
// PIMIsValid and loaded the first (or next) one of the requested type; every plug-in is linked into the game now,
// so the search walks the table of linked plug-ins instead, in the same first/next manner.
#include <string.h>
#include <windows.h>

#include "lle/LowLevelEngine.h"
#include "lle/PIMModule.h"

// The plug-ins linked into the game (each defined next to its entry points)
extern const PIMPlugin gPIMPluginTCPProtocol;          // 'ntwk' TCP Protocol
extern const PIMPlugin gPIMPluginMilesMixer;           // ' snd' Miles Sound System Mixer
extern const PIMPlugin gPIMPluginDefaultRandomMap;     // 'rmap' Default random map generator
extern const PIMPlugin gPIMPluginDX7HRTnLDisplay;      // 'rast' DX7 hardware T&L renderer
static const PIMPlugin* const sPlugins[] = {
    &gPIMPluginTCPProtocol, &gPIMPluginMilesMixer, &gPIMPluginDefaultRandomMap, &gPIMPluginDX7HRTnLDisplay,
};

FSFileSpec PIMModule::sDirectory;
unsigned long PIMModule::sType;
unsigned long PIMModule::sNext;
static bool sInitialized;                   // set by PIMInitialize (provisional name)

// Creates a plug-in module from a linked plug-in and its PIMGetName/PIMGetType/PIMIsValid entry points
PIMModule::PIMModule(const PIMPlugin* inPlugin, const UString& (*inGetName)(), unsigned long (*inGetType)(),
                     bool (*inIsValid)())
{
    this->mPlugin = inPlugin;
    this->mGetName = inGetName;
    this->mGetType = inGetType;
    this->mIsValid = inIsValid;
}

PIMModule::~PIMModule()
{
}

// The named entry point of a plug-in (NULL when it has none)
void* PIMModule::FindEntry(const PIMPlugin* inPlugin, const char* inName)
{
    for (const PIMEntry* theEntry = inPlugin->mEntries; theEntry->mName != NULL; theEntry++)
        if (strcmp(theEntry->mName, inName) == 0)
            return theEntry->mProc;
    return NULL;
}

// Finds the first (or next) linked plug-in of the requested type; kErrorNoMoreFiles when none is left
long PIMModule::GetModule(bool inFirst, PIMModule*& outModule)
{
    outModule = NULL;
    if (inFirst)
        PIMModule::sNext = 0;
    while (PIMModule::sNext < sizeof(sPlugins) / sizeof(sPlugins[0])) {
        const PIMPlugin* thePlugin = sPlugins[PIMModule::sNext++];
        const UString& (*theGetName)() = (const UString& (*)())PIMModule::FindEntry(thePlugin, "PIMGetName");
        unsigned long (*theGetType)() = (unsigned long (*)())PIMModule::FindEntry(thePlugin, "PIMGetType");
        bool (*theIsValid)() = (bool (*)())PIMModule::FindEntry(thePlugin, "PIMIsValid");
        if (theGetName != NULL && theGetType != NULL && theIsValid != NULL && PIMModule::sType == (*theGetType)()) {
            outModule = new PIMModule(thePlugin, theGetName, theGetType, theIsValid);
            return kErrorOK;
        }
    }
    return kErrorNoMoreFiles;
}

static void PIMShutdown(bool inForced)
{
}

// Marks the plug-in system initialised and registers its (empty) shutdown handler
long PIMInitialize()
{
    sInitialized = true;
    return URegisterShutdown(PIMShutdown);
}
