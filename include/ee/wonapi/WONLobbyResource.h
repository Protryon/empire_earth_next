// WONAPI GUI/resource classes used by the game's lobby glue (C:\EEX\Empire Earth\WONAPI\WONLobby\GameCode\EmpireEarth\)
// (provisional names; layouts are undecoded unless a member is listed).
// Requires <string>, ee/EEWonNet.h (RefCount, SmartPtr).
#ifndef EE_WONAPI_WONLOBBYRESOURCE_H
#define EE_WONAPI_WONLOBBYRESOURCE_H
#include "rust/won_gui/compcfg.h"
#include "rust/won_gui/guistrobj.h"
#include "rust/won_gui/localize.h"
#include "rust/won_net/wonfile.h"
#include "rust/won_lobby/eegame.h"     // LobbyGame's EE game (WONLobbyModelObj.h, included inside the namespace)
#include "ee/wonapi/WONLobbyGuard.h"    // and its WonRethrow
#include "ee/wonapi/WONDriver.h"        // EE_WON_DRV
#include <stdexcept>
#include <set>
#include <map>
#include <string.h>
#include <wchar.h>
#include "ee/wonapi/WONConfigObject.h"
#include "ee/wonapi/WONLobbyBackground.h"

namespace WONAPI {
class Component;
class ComponentConfig;
class ResourceConfigTable;
void WONGUIConfig_Init(ResourceConfigTable*); // at 0x0079a1a0
class Font; // owned font results in resource readers; GUI definition follows this header
class Background;
class StringParser;

class Sound;
class Image;

// the shared, reference-counted text of a GUIString (R6 WON GUI round 39): ee_won's guistrobj.rs EeWonGstr makes,
// counts, edits and frees it (the reads are guistring.rs's); this is its view, read in place by 100+ readers: a
// RefCount without a vtable, which RefCount::Release gives to Rust
struct GUIStringData : public RefCount {
    GUIStringData() = delete;
    wchar_t* mText;
    unsigned short mLength;
    unsigned short mCapacity;
};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
static_assert(sizeof(GUIStringData) == sizeof(EeWonGstr) && offsetof(GUIStringData, mText) == offsetof(EeWonGstr, buf),
              "guistrobj.rs EeWonGstr");
#pragma clang diagnostic pop

// 0x328450 (WONAPI, /GX): the shared empty string (made on first use)
inline GUIStringData* GetEmptyGUIStringData() { return (GUIStringData*)ee_won_gstr_empty(); }

// a lobby text: one reference to its shared data (4 bytes: the resource unit's 73 globals); the edits copy shared
// data first (CopyOnWrite, in ee_won_gstr_edit)
class GUIString {
public:
    GUIString() : mData(GetEmptyGUIStringData()) {}
    void erase() { this->mData=GetEmptyGUIStringData(); } // inline, no copy-on-write: replaces the data with the shared empty string
    int ToInt(int start=0) const { return this->R(EeWonGstrRead::ToInt, NULL, start); }
    void reserve(int length) { this->E(EeWonGstrOp::Reserve, NULL, length); } // inline, deliberately without copy-on-write (used at 0x0073ff52 and 0x0074053e)
    GUIString(const char* inText) : mData(N(EeWonGstrOp::AssignNarrow, inText, (int)strlen(inText))) {} // UTF-8 (D6)
    GUIString(const wchar_t* inText) : mData(N(EeWonGstrOp::AssignUnits, inText, (int)wcslen(inText))) {}
    GUIString(const char* text,unsigned short length) : mData(N(EeWonGstrOp::AssignNarrow, text, length)) {} // bounded constructor (not identified in the original exe)
    GUIString(const wchar_t* text,unsigned short length) : mData(N(EeWonGstrOp::AssignUnits, text, length)) {} // bounded constructor (not identified in the original exe)
    GUIString(const std::string& inText) : mData(N(EeWonGstrOp::AssignNarrow, inText.c_str(), (int)inText.length())) {}
    GUIString(const std::wstring& inText) : mData(N(EeWonGstrOp::AssignUnits, inText.c_str(), (int)inText.length())) {}
    // (EELobbyGame 0x292e9f: GetEmptyGUIStringData() into the SmartPtr ctor COMDAT 0x293cd4)
    GUIString(GUIStringData* inData) : mData(inData) {}
    GUIString& operator=(const GUIString& theCopy) { this->mData = theCopy.mData; return *this; }
    GUIString& operator=(GUIStringData* theData) { this->mData = theData; return *this; }
    int CompareNoCase(const GUIString& inOther) const { return this->R(EeWonGstrRead::CompareNoCase, inOther.mData.mPtr); }
    // the narrow text is UTF-8 (decoded first): 1 when it ends first, the first difference, else 0 or -1
    int CompareNoCase(const char* inText, int inOffset = 0) const { return this->R(EeWonGstrRead::CompareNarrowNoCase, inText, inOffset); }
    void resize(int inLength) { this->E(EeWonGstrOp::Resize, NULL, inLength); }
    // 0x7276f0: offset and length are not range-checked
    void assign(const GUIString& s,unsigned short o,unsigned short n) { this->E(EeWonGstrOp::Assign, &s.mData.mPtr, o, n); }
    void assign(const GUIString& s,unsigned short o) { unsigned short off=o; unsigned short l=s.length(); this->assign(s,off,l-off); }
    unsigned short length() const { return this->mData->mLength; }
    bool empty() const { return this->mData->mLength==0; }
    int operator[](int inPos) const { return this->mData->mText[inPos]; }
    unsigned short at(int inPos) const { return this->mData->mText[inPos]; }
    GUIString substr(unsigned short inStart, unsigned short inLength) const { return GUIString(N(EeWonGstrOp::NewSub, this->mData.mPtr, inStart, inLength)); }
    GUIString substr(unsigned short inStart) const { return this->substr(inStart, this->length() - inStart); }
    int find(int inChar, int inStart) const { return this->R(EeWonGstrRead::FindChar, NULL, inChar, inStart); }
    int find(const GUIString& inText, int inStart) const { return this->R(EeWonGstrRead::Find, inText.mData.mPtr, inStart); }
    int findNoCase(const GUIString& inText, int inStart) const { return this->R(EeWonGstrRead::FindNoCase, inText.mData.mPtr, inStart); }
    void erase(int start,int length=0) { this->E(EeWonGstrOp::Erase, NULL, start, length); }
    void insert(int start,const GUIString& text) { this->E(EeWonGstrOp::Insert, &text.mData.mPtr, start); }
    void append(int inChar) { this->E(EeWonGstrOp::AppendUnit, NULL, inChar); } // inline (as used at 0x007022d0)
    void append(const GUIString& inText) { this->append(inText, 0, inText.length()); }
    void append(const GUIString& inText, unsigned short inStart) { this->append(inText, inStart, inText.length()-inStart); }
    //: the lobby's saved-games directory 0x2988e9 expands append(const GUIString&) (the length is read before the
    // copy-on-write call)
    void append(const GUIString& inText, unsigned short inStart, unsigned short inLength) { this->E(EeWonGstrOp::Append, &inText.mData.mPtr, inStart, inLength); }
    // a std::set<GUIString> key (0x298c87: `key.mData->Compare(node.mData) < 0`)
    bool operator<(const GUIString& inOther) const { return this->R(EeWonGstrRead::Compare, inOther.mData.mPtr) < 0; }
    // (EELobbyGame 0x292e9f: the version and saved-player name checks, Compare 0x327fa0 against 0)
    bool operator==(const GUIString& inOther) const { return this->R(EeWonGstrRead::Compare, inOther.mData.mPtr) == 0; }
    bool operator!=(const GUIString& inOther) const { return !(*this == inOther); }
    // 0x327d90: the game's narrow text, UTF-8 (D6; the original narrowed to Latin-1 and '?'); EELobbyGameOptionsCtrl
    // 0x2948f5 passes one to a std::string parameter
    operator std::string() const { std::string s; ee_won_gstr_narrow(this->G(), &GUIString::NarrowSink, &s); return s; }
    //: 0x296c32 narrows a GUIString through a std::wstring temporary
    operator std::wstring() const { return std::wstring(this->mData->mText, this->mData->mLength); }

    SmartPtr<GUIStringData> mData;
private:
    const EeWonGstr* G() const { return (const EeWonGstr*)this->mData.mPtr; }
    static GUIStringData* N(EeWonGstrOp inOp, const void* inOther, int a, int b = 0) { return (GUIStringData*)ee_won_gstr_new(inOp, inOther, a, b); }
    int E(EeWonGstrOp inOp, const void* inOther = NULL, int a = 0, int b = 0, int c = 0)
    { return ee_won_gstr_edit(inOp, (EeWonGstr**)&this->mData.mPtr, inOther, a, b, c); }
    int R(EeWonGstrRead inOp, const void* inOther, int a = 0, int b = 0) const { return ee_won_gstr_read(inOp, this->G(), inOther, a, b); }
    static void NarrowSink(void* inResult, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inResult)->assign((const char*)inBytes, inLength); }
};
// the empty GUIString (_$E31; GUIStringSingleton.cpp's until R6 WON GUI round 39)
inline GUIString gEmptyGUIString;
// the lobby's formatted texts (0x34a100, cdecl; StringLocalize::GetStr: localize.rs): the format with <s1>, <s2> and
// <s3> replaced by the given strings
inline GUIString LobbyFormatString(const GUIString& f, const GUIString& a) { return (GUIStringData*)ee_won_gui_localize(&f, &a, NULL, NULL, 1); }
inline GUIString LobbyFormatString2(const GUIString& f, const GUIString& a, const GUIString& b) { return (GUIStringData*)ee_won_gui_localize(&f, &a, &b, NULL, 2); }
inline GUIString LobbyFormatString3(const GUIString& f, const GUIString& a, const GUIString& b, const GUIString& c)
{ return (GUIStringData*)ee_won_gui_localize(&f, &a, &b, &c, 3); }
// a new text with no reference yet (the original's `new GUIStringData(...)`)
inline GUIStringData* NewGUIStringData(const GUIString& inText) { return (GUIStringData*)ee_won_gstr_new(EeWonGstrOp::NewCopy, inText.mData.mPtr, 0, 0); }

// Clipboard utility (static calls only).
class Clipboard {
public:
    static GUIString Get(); //3cea00 cdecl hidden return
    static void Set(const GUIString&); //3ceb90 cdecl constref
};

// 0x377f40: static resource string by 32-bit index (borrowed).

// A resource group (a ConfigObject: STRING, IMAGE, SOUND, FONT, COLOR and BACKGROUND lines) and the table of groups.
// Since R6 WONLobby round 25 both are ee_won_gui compcfg.rs objects (the maps, the handlers, the reads, the look-ups,
// the glue functions; the control ids and allocators its globals); these are the forwards (WONCompCfg.inl).
class ResourceConfig : public ConfigObject {
protected:
    virtual ~ResourceConfig();
public:
    ResourceConfig();
    virtual bool HandleValueInstruction(ConfigParser*,const std::string&,StringParser&);
    virtual bool PreParse(ConfigParser*); // the parse count
    GUIString SafeGetString(const std::string&); // these three throw when the group lacks the key
    Image* SafeGetImage(const std::string&); // borrowed
    Sound* SafeGetSound(const std::string&); // borrowed
    void AddSound(const std::string&,Sound*); // added or replaced
    EeWonResCfg* mRes; // owned
};
class ResourceConfigTable : public RefCount {
public:
    typedef void (*GlueResourceFunc)(ResourceConfigTable*);
    typedef ComponentConfig* (*ComponentConfigFactory)();
    typedef Component* (*ComponentFactory)();
    ResourceConfigTable();
    ResourceConfig* AddConfig(const std::string&,bool* = NULL); // made when missing; whether it existed
    ResourceConfig* SafeGetConfig(const std::string&); // throws when missing
    void RegisterGlueResourceFunc(GlueResourceFunc);
    void SafeGlueResources(); // every glue function with the table, in address order
    void RegisterControlId(const std::string&,int); // (the control ids and allocators are global)
    void AddAllocator(const std::string&,ComponentConfigFactory,ComponentFactory); // the first one stays
    EeWonResTable* mTable; // owned
protected:
    virtual ~ResourceConfigTable();
};
typedef SmartPtr<ResourceConfigTable> ResourceConfigTablePtr; // SDK type

// CriticalSection: ee/EEWonNet.h


// a file name split into its parts (0x70 bytes: EELobbyFileDialog 0x29136a has it at [ebp-0x80] below a string
// temporary at [ebp-0x10]; six strings, then the file's details read on demand; class name and
// members provisional). Its implicit dtor is out of line (0x29141e, a COMDAT of the lobby glue).
class WONFile {
public:
    // the path, its parts, the stat details and the cache flags are ee_won's (wonfile.rs EeWonFile; WONFile.cpp is
    // deleted: net round 27); the getters return copies
    WONFile(const std::string& inPath) : mFile(ee_won_file_new(B(inPath), inPath.size())) {}
    WONFile(const WONFile& inFile) : mFile(ee_won_file_clone(inFile.mFile)) {}
    WONFile& operator=(const WONFile& inFile) { ee_won_file_assign(this->mFile, inFile.mFile); return *this; }
    ~WONFile() { ee_won_file_free(this->mFile); }
#include EE_WON_DRV(Resource_WONFile) // the drivers' members
    static const char PATH_SEP = '\\';
    void Reset() { this->Num(EE_WON_FILE_RESET); }
    bool Exists() { return this->Num(EE_WON_FILE_EXISTS) != 0; }
    std::string GetFilePath() { return this->Text(EE_WON_FILE_PATH); }
    void SetFilePath(const std::string& inPath) { ee_won_file_op(this->mFile, EE_WON_FILE_SET_PATH, 0, B(inPath), inPath.size()); }
    std::string GetFileName() { return this->Text(EE_WON_FILE_NAME); }
    long GetModifyTime() { return static_cast<long>(this->Num(EE_WON_FILE_MODIFY_TIME)); }
    unsigned long GetSize() { return static_cast<unsigned long>(this->Num(EE_WON_FILE_SIZE)); }
    bool Delete(bool inEvenIfReadOnly) { return this->Num(EE_WON_FILE_DELETE, inEvenIfReadOnly) != 0; }
    EeWonFile* mFile;
private:
    explicit WONFile(EeWonFile* inFile) : mFile(inFile) {}
    static const uint8_t* B(const std::string& inText) { return reinterpret_cast<const uint8_t*>(inText.data()); }
    static void Sink(void* inText, const uint8_t* inBytes, size_t inLength)
    { static_cast<std::string*>(inText)->assign(reinterpret_cast<const char*>(inBytes), inLength); }
    // where the original's substr threw (2: a full path or NormalizePath of ".")
    static void Check(long long inResult) { if (inResult == 2) throw std::out_of_range("invalid string position"); }
    long long Num(int inWhich, long long inValue = 0)
    { long long r = ee_won_file_op(this->mFile, inWhich, inValue, NULL, 0); if (inWhich == EE_WON_FILE_CALC_FULL_PATH || inWhich == EE_WON_FILE_NORMALIZE) Check(r); return r; }
    std::string Text(int inWhich, const EeWonFile* inOther = NULL)
    { std::string s; Check(ee_won_file_text(this->mFile, inWhich, inOther, Sink, &s)); return s; }
};








// Server resources.

extern unsigned long gLobbyAcceptTimeout; // DWORD, 20000 (provisional name)

// Popup texts (declarations only).


}

#include "ee/wonapi/WONLobbyResourceGlobals.h"

#endif
