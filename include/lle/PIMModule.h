// C:\EEX\Low-Level Engine\PIMModule.h (RECONSTRUCTION; folder inferred). A plug-in module: one of the plug-ins
// linked into the game (renderer 'rast', network protocol 'ntwk', sound mixer ' snd', random map generator 'rmap') whose
// PIMGetType matches the requested type. The original loaded each plug-in from a DLL in a directory and looked its
// entry points up with GetProcAddress; here every plug-in is linked in and lists its entry points in a PIMPlugin
// table (registered in PIMModule.cpp).
#ifndef LLE_PIMMODULE_H
#define LLE_PIMMODULE_H

// One named entry point of a plug-in
struct PIMEntry {
    const char* mName;
    void* mProc;
};
#define PIM_ENTRY(f) { #f, reinterpret_cast<void*>(&f) }

// A plug-in linked into the game: its original file name and its entry points (ending with { NULL, NULL }).
// Every plug-in lists PIMGetName, PIMGetType and PIMIsValid.
struct PIMPlugin {
    const char* mFile;
    const PIMEntry* mEntries;
};

class LLE_API PIMModule {
public:
    void* GetEntry(const UString& inName) const { return PIMModule::FindEntry(this->mPlugin, inName); }
    static long GetFirstModule(const FSFileSpec& inDirectory, unsigned long inType, PIMModule*& outModule)
    {
        PIMModule::sDirectory = inDirectory;
        PIMModule::sType = inType;
        return PIMModule::GetModule(true, outModule);
    }
    const UString& GetName() const { return (*this->mGetName)(); }
    static long GetNextModule(PIMModule*& outModule) { return PIMModule::GetModule(false, outModule); }
    unsigned long GetType() const { return (*this->mGetType)(); }
    bool IsValid() const { return (*this->mIsValid)(); }
    void Unload() { delete this; }

private:
    PIMModule(const PIMPlugin* inPlugin, const UString& (*inGetName)(), unsigned long (*inGetType)(),
              bool (*inIsValid)());
    ~PIMModule();
    static long GetModule(bool inFirst, PIMModule*& outModule);
    static void* FindEntry(const PIMPlugin* inPlugin, const char* inName);

    static FSFileSpec sDirectory;           // the directory of the last GetFirstModule (the original's search path)
    static unsigned long sType;             // the requested type
    static unsigned long sNext;             // the next registry index GetModule looks at

    const UString& (*mGetName)();           // PIMGetName
    unsigned long (*mGetType)();            // PIMGetType
    bool (*mIsValid)();                     // PIMIsValid
    const PIMPlugin* mPlugin;
};

LLE_API long PIMInitialize();

#endif
