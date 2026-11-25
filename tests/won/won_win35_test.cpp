// WONGUI's cursors as one Rust object (R6 WON GUI round 35: cursorobj.rs). The original classes and statements as they
// were at 71c097464 (Orig: Cursor, CursorRef, ComponentCursor and Cursor.cpp's table; PlatCursor over platcursor.rs;
// PlatGUIWindow's cursor change; PlatWindowManager's constructor's standard cursors) against the one Cursor class
// over cursorobj.rs (Port: the shell and its host as WONLobbyGUI.h and Cursor.cpp have them), both over the same
// world: reference-counted components that log their end, and platform cursor calls that log (set a system cursor,
// create, set, destroy, show).
//
// Sequences of random steps over twelve cursor slots: a system cursor, a cursor of pixels (its creation failing now
// and then), a reference to another cursor or to none, a component cursor with a component or none, a plain cursor;
// a slot dropped; GetNative and GetComponent (never through a reference to none: the original read through NULL);
// SetStandardCursor with a cursor or none, GetStandardCursor and its answers; the window's cursor change with any
// cursor or none; the manager's installation of the six standard cursors. After each step the two sides' platform
// logs, answers (as slot, table and component numbers) and the reference counts of every slot's cursor, every table
// cursor and every component must be the same. A lobby game reaches only the installation, the arrow and the hand.
//
// The clipboard (clipobj.rs): 40,000 cases of Clipboard::Get over a platform text (any bytes, or none) and
// Clipboard::Set of a text (zero units and lone surrogate halves among them) against PlatClipboard.cpp's bodies.
//
// Then every host op of the port is mutated in turn (skipped, or its answer changed, or run twice) and the same
// sequences must differ. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <map>
#include <set>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "platform/PlatWindow.h"
#include "rust/won_gui/platcursor.h"
#include "rust/won_gui/platimg.h"
#include "rust/won_gui/cursorobj.h"
#include "rust/won_gui/clipobj.h"
#include "platform/PlatString.h"
#include "ee/wonapi/WONBuffer.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "rust/won_gui/guienum.h"

using namespace WONAPI;
typedef EeWonGuiStandardCursor StandardCursor;
wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

// ---- the world both sides share
static std::string* sLog;
static void L(const char* f, int a = 0, int b = 0, int c = 0, int d = 0)
{
    char s[96];
    snprintf(s, sizeof s, f, a, b, c, d);
    *sLog += s;
}
static bool sFailCreate;
static int sNextHandle;
static void WSetSystem(uint32_t c) { L(" sys%d", (int)c); }
static void* WCreate(const uint32_t* px, int w, int h, int x, int y)
{
    L(" create%dx%d@%d,%d", w, h, x, y);
    L(":%08x", px != NULL && w > 0 && h > 0 ? (int)px[0] : -1); // (a cursor of no pixels hands an address that holds none)
    return sFailCreate ? NULL : (void*)(intptr_t)(++sNextHandle);
}
static void WSet(void* c) { L(" set%d", (int)(intptr_t)c); }
static void WDestroy(void* c) { L(" destroy%d", (int)(intptr_t)c); }
static void WShow(int s) { L(" show%d", s); }
static const EeWonPlatCursorFns kFns = {WSetSystem, WCreate, WSet, WDestroy};
#define PlatCursorShow WShow

struct Comp : public RefCount {
    int id;
    explicit Comp(int i) : id(i) {}
    virtual ~Comp() { L(" ~comp%d", this->id); }
};

// ---- the original (WONLobbyGUI.h, WONLobbyPlatform.h, WONLobbyWONSkinConfig.h, Cursor.cpp, PlatGUIWindow.cpp and
// PlatWindowManager.cpp at 71c097464)
namespace Orig {
typedef Comp Component;
class Cursor : public RefCount {
protected:
    virtual ~Cursor(); // out of line
public:
    Cursor();
    virtual Cursor* GetNative(); // vtable +0x04, out of line
    virtual Component* GetComponent(int&,int&); // vtable +0x08, out of line
    static Cursor* GetStandardCursor(StandardCursor); // 0x326550: returns a shared cursor (not owned by the caller)
    static void SetStandardCursor(StandardCursor,Cursor*); //326560 mutable sharedreference
};
class CursorRef : public Cursor {
protected:
    SmartPtr<Cursor> mCursor; //08
public:
    CursorRef(Cursor* cursor) : mCursor(cursor) {}
    virtual Cursor* GetNative() { return this->mCursor->GetNative(); }
    virtual Component* GetComponent(int& x,int& y) { return this->mCursor->GetComponent(x,y); }
    void SetCursor(Cursor* cursor) { this->mCursor=cursor; }
};
class ComponentCursor : public Cursor {
protected:
    SmartPtr<Component> mComponent; //08
    int mMouseX,mMouseY; //0c,10
public:
    ComponentCursor(Component*,int=0,int=0); // at 0x00726710
    virtual Component* GetComponent(int&,int&); // slot 0x08, at 0x007267d0
};
class PlatCursor : public Cursor {
protected:
    virtual ~PlatCursor() { ee_won_plat_cursor_free(this->mRust); }
public:
    explicit PlatCursor(PlatSystemCursor inSystem) : mRust(ee_won_plat_cursor_system(&kFns, (uint32_t)inSystem)) {}
    PlatCursor(const uint32_t* inPixels, int inWidth, int inHeight, int inHotX, int inHotY)
        : mRust(ee_won_plat_cursor_pixels(&kFns, inPixels, inWidth, inHeight, inHotX, inHotY)) {}
    virtual Cursor* GetNative() { return this; }
    void Apply() { ee_won_plat_cursor_apply(this->mRust); }   // makes it the pointer's shape (UI thread)
    EeWonPlatCursor* mRust;
};
static SmartPtr<CursorRef> gStandardCursors[StandardCursor_Max+1];
Cursor::Cursor() {}
Cursor::~Cursor() {}
Cursor* Cursor::GetNative() { return NULL; }
Component* Cursor::GetComponent(int&,int&) { return NULL; }
Cursor* Cursor::GetStandardCursor(StandardCursor type) { return gStandardCursors[type].get(); }
void Cursor::SetStandardCursor(StandardCursor type,Cursor* cursor) {
    if(gStandardCursors[type].get()==NULL)
        gStandardCursors[type]=new CursorRef(cursor);
    else
        gStandardCursors[type]->SetCursor(cursor);
}
ComponentCursor::ComponentCursor(Component* component,int mouseX,int mouseY)
    : mComponent(component)
{
    this->mMouseX=mouseX;
    this->mMouseY=mouseY;
}
Component* ComponentCursor::GetComponent(int& mouseX,int& mouseY)
{
    mouseX=this->mMouseX;
    mouseY=this->mMouseY;
    return this->mComponent.get();
}
// PlatWinHost's case Op::ApplyCursor
static void ApplyCursor(intptr_t a)
{
        if (PlatCursor* theCursor = dynamic_cast<PlatCursor*>((Cursor*)a)) {
            PlatCursorShow(1);
            theCursor->Apply();
        }
}
// PlatWindowManager::PlatWindowManager's body
static void Install()
{
    std::vector<uint32_t> theHand(32 * 32);
    ee_won_plat_hand_cursor(theHand.data());   // the WON hand (32x32, hot spot 12, 4; platimg.rs)
    Cursor::SetStandardCursor(StandardCursor_Hand, new PlatCursor(theHand.data(), 32, 32, 12, 4));
    Cursor::SetStandardCursor(StandardCursor_Arrow, new PlatCursor(PLAT_CURSOR_ARROW));
    Cursor::SetStandardCursor(StandardCursor_IBeam, new PlatCursor(PLAT_CURSOR_IBEAM));
    Cursor::SetStandardCursor(StandardCursor_EastWest, new PlatCursor(PLAT_CURSOR_SIZE_WE));
    Cursor::SetStandardCursor(StandardCursor_NorthSouth, new PlatCursor(PLAT_CURSOR_SIZE_NS));
    Cursor::SetStandardCursor(StandardCursor_NorthSouthEastWest, new PlatCursor(PLAT_CURSOR_SIZE_ALL));
}
static Cursor* NewSystem(int c) { return new PlatCursor((PlatSystemCursor)c); }
static Cursor* NewPixels(const uint32_t* p, int w, int h, int x, int y) { return new PlatCursor(p, w, h, x, y); }
static Cursor* NewRef(Cursor* c) { return new CursorRef(c); }
static Cursor* NewComponent(Component* c, int x, int y) { return new ComponentCursor(c, x, y); }
static Cursor* NewPlain() { return new Cursor; }
}

// ---- the port: the shell as WONLobbyGUI.h and WONLobbyWONSkinConfig.h have it and Cursor.cpp's host (won_win35_port.h,
// copied from the working tree: its host is WONCursorHostReal there), the mutation hook in front as the host
static int sMutate = -1;            // the host op to mutate (-1: none)
static long sHostCalls[16];
namespace Port {
typedef Comp Component;
#include "won_win35_port.h"
static intptr_t WONCursorHost(EeWonCursorOp op, intptr_t a, intptr_t b, intptr_t c)
{
    sHostCalls[(int)op]++;
    if ((int)op != sMutate)
        return WONCursorHostReal(op, a, b, c);
    switch (op) {
    // a reference held twice (a skipped one frees an object its holder still reads)
    case EeWonCursorOp::Hold: case EeWonCursorOp::CompHold: WONCursorHostReal(op, a, b, c); return WONCursorHostReal(op, a, b, c);
    // a reference never given back
    case EeWonCursorOp::Release: case EeWonCursorOp::CompRelease: return 0;
    case EeWonCursorOp::Native: return 0;
    case EeWonCursorOp::Component: { intptr_t r = WONCursorHostReal(op, a, b, c); (*(int*)b)++; return r; }
    // the handle made with a reference too many
    case EeWonCursorOp::Wrap: { intptr_t r = WONCursorHostReal(op, a, b, c); ((Cursor*)r)->AddRef(); return r; }
    // (RustOf answers the table's own cursor: any other answer reads a wrong object; the one mutation that keeps the
    // driver alive is the reference's cursor left as it was)
    case EeWonCursorOp::RustOf: { static EeWonCursor* sPlain = ee_won_cursor_new(); return (intptr_t)sPlain; }
    }
    return 0;
}
static void ApplyCursor(intptr_t a) { ee_won_cursor_apply(a != 0 ? ((Cursor*)a)->mRust : NULL, (void (*)(int32_t))PlatCursorShow); }
static void Install() { ee_won_cursor_install_standard(WONCursorHost, &kFns); }
static Cursor* NewSystem(int c) { return new Cursor(ee_won_cursor_new_system(&kFns, (uint32_t)c)); }
static Cursor* NewPixels(const uint32_t* p, int w, int h, int x, int y) { return new Cursor(ee_won_cursor_new_pixels(&kFns, p, w, h, x, y)); }
static Cursor* NewRef(Cursor* c) { return new Cursor(ee_won_cursor_new_ref(WONCursorHost, (intptr_t)c)); }
static Cursor* NewComponent(Component* c, int x, int y) { return new Cursor(ee_won_cursor_new_component(WONCursorHost, (intptr_t)c, x, y)); }
static Cursor* NewPlain() { return new Cursor; }
}

// ---- one side's run of a sequence
static uint32_t sSeed;
static uint32_t Next() { sSeed = sSeed * 1664525u + 1013904223u; return sSeed >> 8; }
static int Rand(int n) { return (int)(Next() % (uint32_t)n); }

enum { kSlots = 12, kComps = 4, kTable = 6 };
enum Kind { kNone, kPlat, kRefNull, kRef, kComp, kPlain };

#define EE_WIN35_SIDE(NS) \
namespace NS { \
struct Side { \
    SmartPtr<Cursor> slot[kSlots]; \
    Kind kind[kSlots]; \
    SmartPtr<Comp> comp[kComps]; \
    Side() { for (int i = 0; i < kSlots; i++) kind[i] = kNone; } \
    int IdOf(Cursor* c) { \
        if (c == NULL) return -1; \
        for (int i = 0; i < kSlots; i++) if (slot[i].get() == c) return i; \
        for (int t = 0; t < kTable; t++) if (Cursor::GetStandardCursor((StandardCursor)t) == c) return 100 + t; \
        return 999; /* a table cursor's own cursor that left its slot */ \
    } \
    void State() { \
        for (int i = 0; i < kSlots; i++) L(" %d", slot[i].get() ? (int)slot[i]->GetRefCount() : -1); \
        L(" |"); \
        for (int t = 0; t < kTable; t++) { Cursor* c = Cursor::GetStandardCursor((StandardCursor)t); L(" %d", c ? (int)c->GetRefCount() : -1); } \
        L(" |"); \
        for (int k = 0; k < kComps; k++) L(" %d", comp[k].get() ? (int)comp[k]->GetRefCount() : -1); \
        L("\n"); \
    } \
}; \
/* a table cursor is safe to ask once it was set with a cursor (a reference to none reads through NULL) */ \
static bool sTableSet[kTable]; \
static void Run(std::string& out, uint32_t seed, int steps) \
{ \
    sLog = &out; sSeed = seed; sFailCreate = false; \
    Side s; \
    for (int k = 0; k < kComps; k++) s.comp[k] = new Comp(k); \
    for (int n = 0; n < steps; n++) { \
        int op = Rand(14), i = Rand(kSlots), j = Rand(kSlots), t = Rand(kTable), a = Rand(40), b = Rand(40); \
        sFailCreate = Rand(6) == 0; \
        L("%d:%d,%d,%d", op, i, j, t); \
        switch (op) { \
        case 0: s.slot[i] = NewSystem(a % 6); s.kind[i] = kPlat; break; \
        case 1: { uint32_t px[16]; for (int q = 0; q < 16; q++) px[q] = 0x01010101u * (uint32_t)(a + q); \
                  s.slot[i] = NewPixels(a % 5 == 0 ? NULL : px, a % 5 == 0 ? 0 : 4, 4, a % 4, b % 4); s.kind[i] = kPlat; break; } \
        case 2: { /* a reference to a cursor that is no reference, or to none */ \
            Cursor* c = a % 5 != 0 && (s.kind[j] == kPlat || s.kind[j] == kComp || s.kind[j] == kPlain) && j != i ? s.slot[j].get() : NULL; \
            L(" of%d", s.IdOf(c)); \
            Cursor* r = NewRef(c); s.slot[i] = r; s.kind[i] = c ? kRef : kRefNull; break; } \
        case 3: { Comp* c = a % 4 == 0 ? NULL : s.comp[a % kComps].get(); s.slot[i] = NewComponent(c, a - 20, b); s.kind[i] = kComp; break; } \
        case 4: s.slot[i] = NewPlain(); s.kind[i] = kPlain; break; \
        case 5: s.slot[i] = NULL; s.kind[i] = kNone; break; \
        case 6: case 7: if (s.kind[i] != kNone && s.kind[i] != kRefNull) { \
            int x = -7, y = -9; \
            Cursor* nat = s.slot[i]->GetNative(); Comp* c = s.slot[i]->GetComponent(x, y); \
            L(" nat%d comp%d", s.IdOf(nat), c ? c->id : -1); L(" %d,%d", x, y); } break; \
        case 8: case 9: { /* never a reference: the table's cursors answer through theirs */ \
            Cursor* c = a % 6 == 0 ? NULL : (s.kind[j] == kPlat || s.kind[j] == kComp || s.kind[j] == kPlain) ? s.slot[j].get() : NULL; \
            L(" std%d<-%d", t, s.IdOf(c)); \
            Cursor::SetStandardCursor((StandardCursor)t, c); sTableSet[t] = c != NULL; break; } \
        case 10: { Cursor* c = Cursor::GetStandardCursor((StandardCursor)t); \
            L(" got%d", c ? 1 : 0); \
            if (c != NULL && sTableSet[t]) { int x = -7, y = -9; Cursor* nat = c->GetNative(); Comp* k = c->GetComponent(x, y); \
                L(" nat%d comp%d", s.IdOf(nat), k ? k->id : -1); L(" %d,%d", x, y); ApplyCursor((intptr_t)nat); } break; } \
        case 11: case 12: ApplyCursor(a % 7 == 0 ? 0 : (intptr_t)s.slot[i].get()); break; \
        case 13: if (a % 8 == 0) { Install(); for (int q = 0; q < kTable; q++) sTableSet[q] = true; } break; \
        } \
        s.State(); \
    } \
    L("end"); \
} \
}
EE_WIN35_SIDE(Orig)
EE_WIN35_SIDE(Port)

// ---- the clipboard (clipobj.rs): PlatClipboard.cpp's two methods as they were at 71c097464 against the two
// statements over clipobj.rs, both over a platform clipboard that logs: its text handed out (or none), the text set
// (its bytes up to the terminator), each free
static bool sClipNone;
static std::string sClipHave;
static char* CGet()
{
    L(" get");
    if (sClipNone) return NULL;
    char* p = (char*)malloc(sClipHave.size() + 1);
    memcpy(p, sClipHave.c_str(), sClipHave.size() + 1);
    return p;
}
static int CSet(const char* t)
{
    L(" set");
    for (const unsigned char* p = (const unsigned char*)t; *p; p++) L(" %02x", *p);
    return 1;
}
static void CFree(void* p) { L(" free"); free(p); }
namespace OrigClip {
struct Clipboard { static GUIString Get(); static void Set(const GUIString&); };
#define PlatClipboardGetText CGet
#define PlatClipboardSetText CSet
#define free CFree
// Returns the clipboard's text; empty when it holds none
GUIString Clipboard::Get()
{
    GUIString aStr;
    char* aText = PlatClipboardGetText();
    if (aText == NULL)
        return aStr;
    uint32_t aCount = PlatUtf8ToWide(aText, PLAT_TEXT_TERMINATED, NULL, 0);
    std::wstring aWide(aCount, L'\0');
    if (aCount != 0)
        PlatUtf8ToWide(aText, PLAT_TEXT_TERMINATED, &aWide[0], aCount);
    free(aText);
    aStr = GUIString(aWide);
    return aStr;
}

// Replaces the clipboard's contents with the string
void Clipboard::Set(const GUIString& theStr)
{
    std::wstring aWide = theStr;
    std::string aText;
    uint32_t aCount = PlatWideToUtf8(aWide.c_str(), (uint32_t)aWide.size(), NULL, 0);
    aText.resize(aCount);
    if (aCount != 0)
        PlatWideToUtf8(aWide.c_str(), (uint32_t)aWide.size(), &aText[0], aCount);
    PlatClipboardSetText(aText.c_str());
}

#undef free
#undef PlatClipboardSetText
#undef PlatClipboardGetText
}
namespace PortClip {
struct Clipboard { static GUIString Get(); static void Set(const GUIString&); };
static const EeWonClipFns kClipFns = {CGet, CSet, CFree};
GUIString Clipboard::Get() { return GUIString(std::wstring((const wchar_t*)ee_won_clip_get(&kClipFns))); }
void Clipboard::Set(const GUIString& theStr) { ee_won_clip_set(&kClipFns, theStr.mData->mText, theStr.length()); }
}
static wchar_t ClipUnit()
{
    switch (Rand(12)) {
    case 0: return 0;                                   // cuts the text handed to the platform
    case 1: return (wchar_t)(0xD800 + Rand(0x800));     // a lone surrogate half
    case 2: return (wchar_t)(0x80 + Rand(0x780));
    case 3: return (wchar_t)(0x800 + Rand(0xC000));
    default: return (wchar_t)(0x20 + Rand(0x5f));
    }
}
// One clipboard case on both sides: the logs and the texts must be the same.
static int ClipCase(uint32_t seed)
{
    sSeed = seed;
    std::string lo, lp;
    int bad = 0;
    if (Rand(2)) {
        sClipNone = Rand(5) == 0;
        sClipHave.clear();
        for (int n = Rand(20); n > 0; n--) sClipHave.push_back((char)(Rand(3) == 0 ? 1 + Rand(255) : 0x20 + Rand(0x5f))); // (bytes that are no UTF-8 among them)
        sLog = &lo; GUIString o = OrigClip::Clipboard::Get();
        sLog = &lp; GUIString p = PortClip::Clipboard::Get();
        bad += (std::wstring)o != (std::wstring)p;
    } else {
        std::wstring w;
        for (int n = Rand(20); n > 0; n--) w.push_back(ClipUnit());
        GUIString s(w);
        sLog = &lo; OrigClip::Clipboard::Set(s);
        sLog = &lp; PortClip::Clipboard::Set(s);
    }
    return bad + (lo != lp);
}

// The platform module marks the cursor last made the pointer's shape (one mark for the process), so the two sides
// never interleave: the original runs a batch of sequences, then the port runs the same batch. A batch starts with no
// cursor marked (Unmark) and the port starts with the original's last one marked, which is none of its own.
static void Unmark()
{
    std::string sink;
    sLog = &sink;
    Orig::Cursor* c = Orig::NewSystem(0);
    c->AddRef();
    Orig::ApplyCursor((intptr_t)c);
    c->Release();
}

// The batch on both sides (the port under the mutation `mutate`, -1: none): the sequences that differ.
static int Batch(uint32_t salt, int count, int steps, int mutate, bool report)
{
    std::vector<std::string> ref(count);
    Unmark();
    for (int q = 0; q < count; q++) { sNextHandle = 0; Orig::Run(ref[q], salt * (uint32_t)(q + 1), steps); }
    int differ = 0;
    sMutate = mutate;
    for (int q = 0; q < count; q++) {
        std::string p;
        sNextHandle = 0; Port::Run(p, salt * (uint32_t)(q + 1), steps);
        const std::string& o = ref[q];
        if (o == p) continue;
        if (differ++ < 5 && report) {
            size_t k = 0;
            while (k < o.size() && k < p.size() && o[k] == p[k]) k++;
            size_t from = o.rfind('\n', k); from = from == std::string::npos ? 0 : from + 1;
            printf("FAIL sequence %d\n  orig %s\n  port %s\n", q, o.substr(from, o.find('\n', k) - from).c_str(),
                   p.substr(from, p.find('\n', k) - from).c_str());
        }
    }
    sMutate = -1;
    // every table cursor set to none on both sides: the next batch starts from equal tables
    std::string sink;
    sLog = &sink;
    for (int t = 0; t < kTable; t++) {
        Orig::Cursor::SetStandardCursor((StandardCursor)t, NULL); Orig::sTableSet[t] = false;
        Port::Cursor::SetStandardCursor((StandardCursor)t, NULL); Port::sTableSet[t] = false;
    }
    return differ;
}

int main()
{
    const int kSeqs = 3000, kSteps = 60, kMut = 300;
    int failures = Batch(0x9e3779b9u, kSeqs, kSteps, -1, true);
    long calls = 0;
    int reached = 0;
    for (int op = 1; op <= 8; op++) { calls += sHostCalls[op]; reached += sHostCalls[op] != 0; }
    printf("won_win35_test: %d sequences, %ld steps, %ld host calls, %d of 8 host ops reached, %d mismatches\n", kSeqs,
           (long)kSeqs * kSteps, calls, reached, failures);
    // every host op mutated in turn: the same sequences must differ from the original's (a table cursor's handle is
    // made once per process, in the batch above, so no mutation reaches the tables' own counts); then a batch without
    // a mutation, which must not differ: the mutated batches left the two sides in step
    int caught = 0;
    for (int op = 1; op <= 8; op++) {
        int differ = Batch(0x51ed270bu + 2u * (uint32_t)op, kMut, kSteps, op, false);
        printf("  mutation of host op %d: %d of %d sequences differ\n", op, differ, kMut);
        if (differ != 0) caught++; else failures++;
    }
    int after = Batch(0x2545f491u, kMut, kSteps, -1, true);
    failures += after;
    printf("won_win35_test: %d of 8 host-op mutations caught, %d mismatches in the batch after them, %d failures\n", caught, after, failures);
    // the clipboard
    int clip = 0;
    const int kClip = 40000;
    for (int q = 0; q < kClip; q++) clip += ClipCase(0x7f4a7c15u * (uint32_t)(q + 1)) != 0;
    failures += clip;
    printf("won_win35_test: %d clipboard cases, %d mismatches\n", kClip, clip);
    static std::string sExit; // (the tables' cursors end with the process and still log)
    sLog = &sExit;
    return failures;
}
