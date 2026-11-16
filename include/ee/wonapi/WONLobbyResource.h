// WONAPI GUI/resource classes used by the game's lobby glue (C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\)
// (provisional names; layouts are undecoded unless a member is listed).
// Requires <string>, ee/EEWonNet.h (RefCount, SmartPtr).
#ifndef EE_WONAPI_WONLOBBYRESOURCE_H
#define EE_WONAPI_WONLOBBYRESOURCE_H
#include <set>
#include <map>
#include <string.h>
#include <wchar.h>
#include "ee/wonapi/WONConfigObject.h"
#include "rust/ee_won.h" // guistrobj.rs, guistring.rs
#include "ee/wonapi/WONLobbyBackground.h"

namespace WONAPI {
class Component;
class ComponentConfig;
class ResourceConfigTable;
void WONGUIConfig_Init(ResourceConfigTable*); // at 0x0079a1a0
void ImageConfig_Init(ResourceConfigTable*); // at 0x0078adf0
void FontConfig_Init(ResourceConfigTable*); // at 0x0078a080
class Font; // owned font results in resource readers; GUI definition follows this header
class Background;
class StringParser;
void StringToUpperCaseInPlace(std::string&); // utility, at 0x006a69a0

class Sound;
class Image;

// the shared, reference-counted text of a GUIString (0x10 bytes: `new` in 0x328450; ctor 0x327840 from a wide string).
// R6 WONGUI B round 1: the buffer's edits are ee_won's guistrobj.rs (the constructors' fills, Reserve, resize, the
// wide and narrow assignments, Fill, assign, append, erase, insert, setAt, the free; GUIStringData.cpp,
// GUIStringDataMutations.cpp, GUIStringDataResize.cpp and GUIStringDataSearch.cpp are gone), its reads guistring.rs.
// The class keeps its three members: the block (EeWonGuiStrBuf) the Rust edits in place and 100+ readers read.
class GUIStringData : public RefCount {
public:
    explicit GUIStringData(unsigned short inCapacity) { this->S(EeWonGstrOp::NewCapacity, NULL, inCapacity); }
    virtual ~GUIStringData() { this->S(EeWonGstrOp::Free); }
    GUIStringData(const wchar_t* inText) { this->Init(); this->AssignWStr(inText, (int)wcslen(inText)); } // 0x327840
    GUIStringData(const char* inText) { this->Init(); this->AssignStr(inText, (int)strlen(inText)); } // 0x3277e0
    GUIStringData(const char* inText, unsigned short inLength) { this->Init(); this->AssignStr(inText, inLength); }
    GUIStringData(const wchar_t* inText, unsigned short inLength) { this->Init(); this->AssignWStr(inText, inLength); }
    GUIStringData(const std::string& inText) { this->Init(); this->AssignStr(inText.c_str(), (int)inText.length()); }
    GUIStringData(const std::wstring& inText) { this->Init(); this->AssignWStr(inText.c_str(), (int)inText.length()); }
    GUIStringData(const GUIStringData& inCopy) : RefCount() { this->S(EeWonGstrOp::NewCopy, inCopy.Buf()); } // 0x727600
    // 0x7276f0: offset and length are not range-checked
    GUIStringData(const GUIStringData* inOther, unsigned short inStart, unsigned short inLength)
    { this->S(EeWonGstrOp::NewSub, inOther->Buf(), inStart, inLength); }
    void Init() { this->mLength = this->mCapacity = 0; this->mText = NULL; } // at 0x007274b0
    void AssignStr(const char* inText, int inLength) { this->S(EeWonGstrOp::AssignNarrow, inText, inLength); } // UTF-8 (D6)
    void AssignWStr(const wchar_t* inText, int inLength) { this->S(EeWonGstrOp::AssignUnits, inText, inLength); }
    void assign(const GUIStringData* inText, unsigned short inStart, unsigned short inLength)
    { this->S(EeWonGstrOp::Assign, inText->Buf(), inStart, inLength); }
    void resize(int inLength, bool inGrow) { this->S(EeWonGstrOp::Resize, NULL, inLength, inGrow); }
    void Fill(int inChar, unsigned short inStart, unsigned short inLength)
    { this->S(EeWonGstrOp::Fill, NULL, inChar, inStart, inLength); }
    void erase(int inStart, int inLength) { this->S(EeWonGstrOp::Erase, NULL, inStart, inLength); }
    void insert(int inStart, const GUIStringData* inText) { this->S(EeWonGstrOp::Insert, inText->Buf(), inStart); }
    int setAt(int inPos, int inChar) { return this->S(EeWonGstrOp::SetAt, NULL, inPos, inChar); }
    void append(int inChar) { this->S(EeWonGstrOp::AppendUnit, NULL, inChar); } // inline (as used at 0x007022d0)
    // inline in the WON library (ChatCommandParser::AddHelpEntry 0x757800)
    void append(const GUIStringData* inText, unsigned short inStart, unsigned short inLength)
    { this->S(EeWonGstrOp::Append, inText->Buf(), inStart, inLength); }
    // 0x2911c9 (a COMDAT of EELobbyFileDialog.cpp): room for inLength characters and the terminator
    void Reserve(int inLength, bool inGrow) { this->S(EeWonGstrOp::Reserve, NULL, inLength, inGrow); }
    // the reads (guistring.rs)
    int ToInt(unsigned short inStart) const
    { return ee_won_guistring_to_int(this->mText, this->mLength, inStart, sizeof(wchar_t)); }
    int find(int inChar, int inStart) const
    { return ee_won_guistring_find_char(this->mText, this->mLength, inChar, inStart, false, sizeof(wchar_t)); }
    int rFind(int inChar, int inStart) const
    { return ee_won_guistring_find_char(this->mText, this->mLength, inChar, inStart, true, sizeof(wchar_t)); }
    int find(const GUIStringData* inText, int inStart) const { return this->Op(EeWonGuiStringOp::Find, inText, inStart); }
    int findNoCase(const GUIStringData* inText, int inStart) const { return this->Op(EeWonGuiStringOp::FindNoCase, inText, inStart); }
    int rFind(const GUIStringData* inText, int inStart) const { return this->Op(EeWonGuiStringOp::RFind, inText, inStart); }
    int findOneOf(const GUIStringData* inText, int inStart) const { return this->Op(EeWonGuiStringOp::FindOneOf, inText, inStart); }
    int Compare(const GUIStringData* inOther) const { return this->Op(EeWonGuiStringOp::Compare, inOther, 0); }
    int CompareNoCase(const GUIStringData* inOther) const { return this->Op(EeWonGuiStringOp::CompareNoCase, inOther, 0); }
    // the narrow text is UTF-8 (decoded first): 1 when it ends first, the first difference, else 0 (compare: only when
    // it continues with '0', the original's bug) or -1
    int compare(const char* inText, int inOffset) const
    { return ee_won_guistring_compare_narrow(this->mText, this->mLength, inOffset, (const uint8_t*)inText, false, sizeof(wchar_t)); }
    int CompareNoCase(const char* inText, int inOffset) const
    { return ee_won_guistring_compare_narrow(this->mText, this->mLength, inOffset, (const uint8_t*)inText, true, sizeof(wchar_t)); }
    void toUpper() { ee_won_guistring_change_case(this->mText, this->mLength, true, sizeof(wchar_t)); } // below 256 only
    void toLowerCase() { ee_won_guistring_change_case(this->mText, this->mLength, false, sizeof(wchar_t)); }
    // 0x327d90: the game's narrow text, UTF-8 (D6; the original narrowed to Latin-1 and '?')
    std::string GetString() const
    {
        std::string aResult;
        if (this->mLength != 0)
            ee_won_guistring_narrow(this->mText, this->mLength, sizeof(wchar_t), &GUIStringData::NarrowSink, &aResult);
        return aResult;
    }
    std::wstring GetWString() const { return std::wstring(this->mText, this->mLength); }

    wchar_t* mText;                         // +0x08
    unsigned short mLength;                 // +0x0c
    unsigned short mCapacity;               // +0x0e
private:
    // the three members (their layout checked in GUIStringSingleton.cpp)
    EeWonGuiStrBuf* Buf() const { return (EeWonGuiStrBuf*)&this->mText; }
    int S(EeWonGstrOp inOp, const void* inOther = NULL, int a = 0, int b = 0, int c = 0)
    { return ee_won_gstr(inOp, this->Buf(), inOther, a, b, c, sizeof(wchar_t)); }
    int Op(EeWonGuiStringOp inOp, const GUIStringData* inOther, int inStart) const
    { return ee_won_guistring_op(inOp, this->mText, this->mLength, inOther->mText, inOther->mLength, inStart, sizeof(wchar_t)); }
    static void NarrowSink(void* inResult, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inResult)->assign((const char*)inBytes, inLength); }
};

// 0x328450 (WONAPI, /GX): the shared empty string (a function-local static, created on first use)
GUIStringData* GetEmptyGUIStringData();

// a lobby text: one reference to its shared data (4 bytes: the resource unit's 73 globals)
class GUIString {
public:
    GUIString() : mData(GetEmptyGUIStringData()) {}
    explicit GUIString(unsigned short capacity) : mData(new GUIStringData(capacity)) {} // inline (used at 0x007398f7)
    void erase() { this->mData=GetEmptyGUIStringData(); } // inline, no copy-on-write: replaces the data with the shared empty string
    int ToInt(int start=0) const { return this->mData->ToInt(start); }
    void reserve(int length) { this->mData->Reserve(length,false); } // inline, deliberately without copy-on-write (used at 0x0073ff52 and 0x0074053e)
    GUIString(const char* inText) : mData(new GUIStringData(inText)) {}
    GUIString(const wchar_t* inText) : mData(new GUIStringData(inText)) {}
    GUIString(const char* text,unsigned short length) :mData(new GUIStringData(text,length)) {} // bounded constructor (not identified in the original exe)
    GUIString(const wchar_t* text,unsigned short length) :mData(new GUIStringData(text,length)) {} // bounded constructor (not identified in the original exe)
    GUIString(const std::string& inText) : mData(new GUIStringData(inText)) {}
    GUIString(const std::wstring& inText) : mData(new GUIStringData(inText)) {}
    // (EELobbyGame 0x292e9f: GetEmptyGUIStringData() into the SmartPtr ctor COMDAT 0x293cd4)
    GUIString(GUIStringData* inData) : mData(inData) {}
    GUIString& operator=(const GUIString& theCopy) { this->mData = theCopy.mData; return *this; }
    GUIString& operator=(GUIStringData* theData) { this->mData = theData; return *this; }
    int compare(const char* theStr, int theOffset=0) const { return this->mData->compare(theStr,theOffset); } // inline, read-only
    int CompareNoCase(const GUIString& inOther) const { return this->mData->CompareNoCase(inOther.mData.get()); }
    int CompareNoCase(const char* inText, int inOffset = 0) const { return this->mData->CompareNoCase(inText, inOffset); }
    //: the lobby's saved-games directory 0x2988e9 expands append(const GUIString&) (the length is read
    // before the copy-on-write call) and calls CopyOnWrite 0x29121b / GUIStringData::append 0x292419 out of line
    void CopyOnWrite() { if (this->mData->GetRefCount() > 1) this->mData = new GUIStringData(*this->mData.mPtr); }
    void Fill(int ch,unsigned short offset,unsigned short length) { this->CopyOnWrite(); this->mData->Fill(ch,offset,length); } // inline
    void resize(int inLength) { this->CopyOnWrite(); this->mData->resize(inLength,false); }
    void assign(const GUIString& s,unsigned short o,unsigned short n) { this->CopyOnWrite(); this->mData->assign(s.mData,o,n); }
    void assign(const GUIString& s,unsigned short o) { unsigned short off=o; unsigned short l=s.length(); this->assign(s,off,l-off); }
    unsigned short length() const { return this->mData->mLength; }
    bool empty() const { return this->mData->mLength==0; }
    int operator[](int inPos) const { return this->mData->mText[inPos]; }
    unsigned short at(int inPos) const { return this->mData->mText[inPos]; }
    GUIString substr(unsigned short inStart, unsigned short inLength) const
    { return GUIString(new GUIStringData(this->mData.get(), inStart, inLength)); }
    GUIString substr(unsigned short inStart) const
    { return GUIString(new GUIStringData(this->mData.get(), inStart, this->length() - inStart)); }
    int find(int inChar, int inStart) const { return this->mData->find(inChar, inStart); }
    int find(const GUIString& inText, int inStart) const { return this->mData->find(inText.mData.get(), inStart); }
    int findNoCase(const GUIString& inText, int inStart) const { return this->mData->findNoCase(inText.mData.get(), inStart); }
    int rFind(int ch,int start=-1) const { return this->mData->rFind(ch,start); } // inline
    int rFind(const GUIString& text,int start=-1) const { return this->mData->rFind(text.mData.get(),start); }
    int findOneOf(const GUIString& text,int start=0) const { return this->mData->findOneOf(text.mData.get(),start); }
    int setAt(int pos,int ch) { this->CopyOnWrite(); return this->mData->setAt(pos,ch); }
    void erase(int start,int length=0) { this->CopyOnWrite(); this->mData->erase(start,length); }
    void insert(int start,const GUIString& text) { this->CopyOnWrite(); this->mData->insert(start,text.mData.get()); }
    void toUpper() { this->CopyOnWrite(); this->mData->toUpper(); }
    void toLowerCase() { this->CopyOnWrite(); this->mData->toLowerCase(); } // copy-on-write forwarder
    void append(int inChar) { this->CopyOnWrite(); this->mData->append(inChar); } // copy-on-write forwarder
    void append(const GUIString& inText) { this->append(inText, 0, inText.length()); }
    void append(const GUIString& inText, unsigned short inStart) { this->append(inText, inStart, inText.length()-inStart); }
    void append(const GUIString& inText, unsigned short inStart, unsigned short inLength)
    {
        this->CopyOnWrite();
        this->mData->append(inText.mData.get(), inStart, inLength);
    }
    // a std::set<GUIString> key (0x298c87: `key.mData->Compare(node.mData) < 0`)
    bool operator<(const GUIString& inOther) const { return mData->Compare(inOther.mData.get()) < 0; }
    // (EELobbyGame 0x292e9f: the version and saved-player name checks, Compare 0x327fa0 against 0)
    bool operator==(const GUIString& inOther) const { return this->mData->Compare(inOther.mData.get()) == 0; }
    bool operator!=(const GUIString& inOther) const { return this->mData->Compare(inOther.mData.get()) != 0; }
    // the narrowed text (EELobbyGameOptionsCtrl 0x2948f5 passes one to a std::string parameter; shape inferred)
    operator std::string() const { return mData->GetString(); }
    //: 0x296c32 narrows a GUIString through a std::wstring temporary
    operator std::wstring() const { return mData->GetWString(); }

    SmartPtr<GUIStringData> mData;
};

// Clipboard utility (static calls only).
class Clipboard {
public:
    static GUIString Get(); //3cea00 cdecl hidden return
    static void Set(const GUIString&); //3ceb90 cdecl constref
};

// 0x377f40: static resource string by 32-bit index (borrowed).

// ResourceConfig (0xb0 bytes), a ConfigObject. AddConfig (0x00793bb0) allocates 0xb0 bytes and
// calls the no-argument constructor at 0x00790740; constructor, destructor and vtable are out of
// line. Unlike the later SDK, there is no mConfigTable member at +0xb0.
class ResourceConfig : public ConfigObject {
protected:
    virtual ~ResourceConfig(); // complete destructor 0x00790d10, deleting destructor 0x00790870
    bool mAllowOverwrite; //54, natural55..57
    int mParseCount; // offset 0x58, incremented at 0x00793340 before the key is read
    template<class T> class ResMap : public std::map<std::string,std::pair<T,int>,StringLessNoCase> {};
    typedef ResMap<GUIString> StringMap;
    typedef ResMap<SmartPtr<Image> > ImageMap;
    typedef ResMap<SmartPtr<Font> > FontMap;
    typedef ResMap<DWORD> ColorMap;
    typedef ResMap<Background> BackgroundMap;
    typedef ResMap<int> IntMap;
    typedef ResMap<SmartPtr<Sound> > SoundMap;
    StringMap mStringMap; //5c..67
    ImageMap mImageMap; //68..73
    FontMap mFontMap; //74..7f
    ColorMap mColorMap; //80..8b
    BackgroundMap mBackgroundMap; // offsets 0x8c..0x97 (Background is 0x14 bytes plus a 4-byte generation)
    IntMap mIntMap; //98..a3
    SoundMap mSoundMap; // offsets 0xa4..0xaf; class ends at 0xb0 (no later-SDK mConfigTable)
    bool HandleString(StringParser&); // at 0x00792ac0
    bool HandleImage(StringParser&); //392d90
    bool HandleSound(StringParser&); //392820
    bool HandleFont(StringParser&); //392410
    bool HandleColor(StringParser&); //3923d0
    bool HandleBackground(StringParser&); //393020
public:
    ResourceConfig(); // at 0x00790740 (no arguments)
    virtual bool HandleValueInstruction(ConfigParser*,const std::string&,StringParser&); // at 0x00793350
    virtual bool PreParse(ConfigParser*);  // at 0x00793340
    void SetAllowOverwrite(bool value) { this->mAllowOverwrite=value; } // inline setter (member at +0x54)
    bool GetString(const std::string&,GUIString&); //3934c0
    bool GetImage(const std::string&,Image*&); //393570
    bool GetFont(const std::string&,Font*&); //3935d0
    bool GetSound(const std::string&,Sound*&); //393630
    bool GetColor(const std::string&,DWORD&); //3936a0
    bool GetBackground(const std::string&,Background&); //393710
    bool GetInt(const std::string&,int&); //393820
    const GUIString& SafeGetString(const std::string&); // at 0x00793890 (the resource owns the string)
    Image* SafeGetImage(const std::string&); // at 0x007938b0 (borrowed result)
    Sound* SafeGetSound(const std::string&); // at 0x007938d0 (borrowed result)
    void AddSound(const std::string&,Sound*); //3934a0
    Font* SafeGetFont(const std::string&);
    DWORD SafeGetColor(const std::string&);
    const Background& SafeGetBackground(const std::string&);
    int SafeGetInt(const std::string&);
    static void EnsureComma(StringParser&); //390f20
    static void EnsureCloseParen(StringParser&); //391060
    static int SafeReadColor(StringParser&); //3911a0
    static int SafeReadFontStyle(StringParser&); //3916c0
    static SmartPtr<Font> SafeReadFont(StringParser&); //391870
    static SmartPtr<Sound> SafeReadSound(StringParser&); //391ab0
    static SmartPtr<Image> SafeReadImage(StringParser&); //391ea0
    static Background SafeReadBackground(StringParser&); //392050
};
// ResourceConfigTable (0x20 bytes), a RefCount (8 bytes): constructor 0x007938f0, destructor
// 0x00793aa0; they, the parser and the factories are out of line.
class ResourceConfigTable : public RefCount {
public:
    typedef void (*GlueResourceFunc)(ResourceConfigTable*);
    typedef ComponentConfig* (*ComponentConfigFactory)();
    typedef Component* (*ComponentFactory)();
    ResourceConfigTable(); // at 0x007938f0 (0x20 bytes)
    ResourceConfig* AddConfig(const std::string&,bool* = NULL); // at 0x00793bb0; bool* output
    ResourceConfig* SafeGetConfig(const std::string&); // at 0x00793e50; throws when missing
    ResourceConfig* GetConfig(const std::string&);
    void ClearGlueResourceFuncs();
    void ClearAllocators();
    void ClearControlIds();
    void ClearResources();
    void ClearAll();
    void RegisterGlueResourceFunc(GlueResourceFunc); //393f40
    void SafeGlueResources(); // callback walk, at 0x00793fe0
    void RegisterControlId(const std::string&,int); // at 0x007941d0
    void AddAllocator(const std::string&,ComponentConfigFactory,ComponentFactory); //394040
    bool GetControlId(const std::string&,int&); //3942f0
    bool AllocateConfig(const std::string&,SmartPtr<ComponentConfig>&,SmartPtr<Component>&); //3941f0
protected:
    virtual ~ResourceConfigTable(); // slot 0; complete 0x00793aa0, deleting 0x007939c0
    typedef std::map<std::string,SmartPtr<ResourceConfig>,StringLessNoCase> ResourceMap;
    ResourceMap mResourceMap; // offsets 0x08..0x13 (owning VC6 map, 0x0c bytes)
    typedef std::set<GlueResourceFunc> GlueResourceFuncSet;
    GlueResourceFuncSet mGlueResourceFuncSet; // offsets 0x14..0x1f; class size 0x20
    typedef std::map<std::string,int,StringLessNoCase> ControlIdMap;
    static ControlIdMap mControlIdMap; // at 0x00966af8 (declaration only)
    struct ConfigAllocator {
        ComponentConfigFactory mConfigFactory;
        ComponentFactory mComponentFactory;
        ConfigAllocator(ComponentConfigFactory a,ComponentFactory b) : mConfigFactory(a),mComponentFactory(b) { }
    };
    typedef std::map<std::string,ConfigAllocator,StringLessNoCase> ConfigAllocatorMap;
    static ConfigAllocatorMap mConfigAllocatorMap; // at 0x00966b08 (declaration only)
};
typedef SmartPtr<ResourceConfigTable> ResourceConfigTablePtr; // SDK type

// CriticalSection: ee/EEWonNet.h

// 0x2a6590 (WONAPI string utilities, /GX): widens a string (inCodePage < 0: the default code page 0x2a6570)
std::wstring StringToWString(const std::string& inString, int inCodePage);
// 0x2a6410 (WONAPI string utilities, /GX): narrows a wide string
std::string WStringToString(const std::wstring& inString, int inCodePage);

// a file name split into its parts (0x70 bytes: EELobbyFileDialog 0x29136a has it at [ebp-0x80] below a string
// temporary at [ebp-0x10]; six strings, then the file's details read on demand; class name and
// members provisional). Its implicit dtor is out of line (0x29141e, a COMDAT of the lobby glue).
class WONFile {
public:
    WONFile(const std::string& inPath);     // 0x2a7610 (WONAPI, /GX)
    bool NormalizePath();
    std::string GetRelativePath(const WONFile&);
    static WONFile GetWorkingDirectory();
    bool SetWorkingDirectory();
    static const char PATH_SEP;
    void Reset();
    void CalcFileInfo();
    void CalcFileDirectory();
    void CalcFullPath();
    void CalcFileExtension();
    bool SetReadOnly(bool inReadOnly);      // file attributes
    bool Exists();
    bool IsDir();
    bool CreateAsDirectory(bool inCreateAll = true);
    const std::string& GetFilePath();
    const std::string& GetFileDirectory();
    const std::string& GetFileExtension();
    const std::string& GetFullPath();
    void SetFilePath(const std::string& inPath);
    const std::string& GetFileName();       // 0x2a7ff0 (+0x20, parsed on first use)
    long GetModifyTime();
    unsigned long GetSize();                // 0x2a8050 (+0x60, read on first use; inferred)
    bool Delete(bool inFlag);

    std::string mPath;                      // +0x00
    std::string mFileDirectory;             // +0x10
    std::string mFileName;                  // +0x20
    std::string mFileNameNoExtension;       // +0x30
    std::string mFileExtension;             // +0x40
    std::string mFullPath;                  // +0x50
    unsigned long mSize;                   // +0x60, _stat st_size
    long mModifyTime;                       // +0x64, _stat st_mtime
    bool mExists;                           // +0x68, _stat success
    bool mIsReadOnly;                       // +0x69, original mode-derived predicate
    bool mIsDir;                            // +0x6a, directory predicate
    bool mFileDirectoryValid;               // +0x6b, string10 cache flag
    bool mFileNameValid;                    // +0x6c, string20 cache flag
    bool mFileExtensionValid;               // +0x6d, string40 cache flag
    bool mStatValid;                        // +0x6e
    bool mFullPathValid;                    // +0x6f, string50 cache flag
};








// Server resources.

extern unsigned long gLobbyAcceptTimeout; // DWORD, 20000 (provisional name)
std::string StringToLowerCase(const std::string&); // at 0x006a66f0
std::string StringToUpperCase(const std::string&);
std::wstring WStringToLowerCase(const std::wstring&);
void StringToLowerCaseInPlace(std::string&);
void WStringToUpperCaseInPlace(std::wstring&);
void WStringToLowerCaseInPlace(std::wstring&);

// Popup texts (declarations only).


}

#include "ee/wonapi/WONLobbyResourceGlobals.h"

#endif
