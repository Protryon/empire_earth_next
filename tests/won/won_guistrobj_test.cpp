// WONGUI's GUIStringData buffer (R6 WONGUI B round 1): the original's statements (GUIStringData.cpp,
// GUIStringDataMutations.cpp and the header's inline Reserve / resize / append, copied below as OrigData) against the
// port (WONLobbyResource.h's GUIStringData over ee_won's guistrobj.rs) over random edit sequences on three strings,
// the length, the capacity and every unit to the terminator compared after each edit. The original's `new wchar_t[n]`
// is value-initialised here (`()`) so its uninitialised units read as the port's zeroed ones; the edits stay inside
// the original's defined behaviour (no wrapped length or capacity, no negative insert or setAt position, no erase of a
// negative length, no insert into itself).
// `--print FILE` writes the first sequences as rust/crates/ee_won_gui/src/golden_guistrobj.txt, which cargo replays
// (guistrobj_tests.rs); without it the golden is compared too. The exit code is the number of failures.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <string>
#include <list>
#include <map>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"

using namespace WONAPI;

wchar_t gEmptyWideString[2]; // EEWonNet.cpp's (GetEmptyGUIStringData)

// ---- the original statements (HEAD 6d0c36ca's GUIStringDataMutations.cpp, GUIStringData.cpp, WONLobbyResource.h)
struct OrigData {
    wchar_t* mText;
    unsigned short mLength;
    unsigned short mCapacity;
    void Init() { this->mLength=this->mCapacity=0; this->mText=NULL; }
    ~OrigData() { delete [] this->mText; }
    void Reserve(int inLength, bool inGrow)
    {
        inLength++;
        if (this->mCapacity < inLength) {
            wchar_t* theOld = this->mText;
            if (inGrow)
                inLength += inLength;
            this->mText = new wchar_t[inLength](); // (value-initialised: see the head comment)
            if (theOld != NULL) {
                memcpy(this->mText, theOld, (this->mLength + 1) * sizeof(wchar_t));
                delete[] theOld;
            }
            this->mCapacity = inLength;
        }
    }
    void resize(int inLength, bool inGrow) { this->Reserve(inLength,inGrow); this->mLength=inLength; this->mText[this->mLength]=0; }
    void append(int inChar) { this->resize(this->mLength + 1, true); this->mText[this->mLength - 1] = (wchar_t)inChar; }
    void append(const OrigData* inText, unsigned short inStart, unsigned short inLength)
    { int theLength = this->mLength; this->resize(theLength + inLength, true); memcpy(this->mText + theLength, inText->mText + inStart, inLength * sizeof(wchar_t)); }
    OrigData(unsigned short capacity) { this->Init(); this->resize(capacity,false); this->resize(0,false); }
    OrigData(const OrigData& other)
    { this->Init(); this->resize(other.mLength,false); memcpy(this->mText,other.mText,sizeof(wchar_t)*(this->mLength+1)); }
    OrigData(const OrigData* other,unsigned short offset,unsigned short length)
    { this->Init(); this->resize(length,false); memcpy(this->mText,other->mText+offset,sizeof(wchar_t)*length); }
    OrigData(const char* text) { this->Init(); this->AssignStr(text,strlen(text)); }
    OrigData(const wchar_t* text) { this->Init(); this->AssignWStr(text,wcslen(text)); }
    static void AssignUnits(void* context, const void* units, size_t count)
    {
        OrigData* data=static_cast<OrigData*>(context);
        data->resize((int)count,false);
        if(count!=0) memcpy(data->mText,units,count*sizeof(wchar_t));
    }
    void AssignStr(const char* text,int length)
    {
        this->resize(0,false);
        if(length>0)
            ee_won_guistring_widen(reinterpret_cast<const uint8_t*>(text),(size_t)length,sizeof(wchar_t),
                                   AssignUnits,this);
    }
    void AssignWStr(const wchar_t* text,int length)
    {
        this->resize(length,false);
        this->mLength=length;
        for(int i=0;i<length;++i) this->mText[i]=text[i];
    }
    void Fill(int character,unsigned short offset,unsigned short length)
    {
        int end=offset+length;
        this->resize(end,false);
        for(int i=offset;i<end;++i) this->mText[i]=character;
    }
    void assign(const OrigData* text,unsigned short offset,unsigned short length)
    {
        int aLen=length;
        this->Reserve(aLen,false);
        this->resize(0,false);
        int aLength=this->mLength;
        this->resize(aLength+aLen,true);
        memcpy(this->mText+aLength,text->mText+offset,aLen*sizeof(wchar_t));
    }
    void erase(int theStartChar, int theLength)
    {
        if (theStartChar>this->mLength || theStartChar < 0)
            return;
        int aEndChar = theStartChar+theLength;
        if(theLength==0 || aEndChar>this->mLength)
            aEndChar = this->mLength;
        if(aEndChar!=this->mLength)
            memmove(this->mText+theStartChar,this->mText+aEndChar,sizeof(wchar_t)*(this->mLength-aEndChar));
        this->resize(this->mLength-(aEndChar-theStartChar),false);
    }
    void insert(int theStartChar, const OrigData *theStr)
    {
        if(theStartChar>this->mLength)
            return;
        int aOldLen = this->mLength;
        this->resize(this->mLength+theStr->mLength,false);
        if(theStartChar!=aOldLen)
            memmove(this->mText+theStartChar+theStr->mLength,this->mText+theStartChar,sizeof(wchar_t)*(aOldLen-theStartChar));
        memcpy(this->mText+theStartChar,theStr->mText,sizeof(wchar_t)*theStr->mLength);
    }
    int setAt(int thePos, int theChar)
    {
        if (thePos < this->mLength)
            this->mText[thePos] = (unsigned short)theChar;
        return theChar;
    }
};

// ---- the driver

static unsigned sSeed = 1;
static int R(int n) { sSeed = sSeed * 1103515245u + 12345u; return n <= 0 ? 0 : (int)((sSeed >> 8) % (unsigned)n); }

static const wchar_t* const kWide[] = {L"", L"a", L"hello", L"\x00e9t\x00e9", L"\x263a smile", L"0123456789abcdef"};
static const char* const kNarrow[] = {"", "x", "abc", "\xc3\xa9t", "\xe2\x98\xba!", "\xff" "bad", "a longer narrow text"};

static std::string State(const wchar_t* t, unsigned short len, unsigned short cap)
{
    char b[32];
    snprintf(b, sizeof(b), "%u %u", (unsigned)len, (unsigned)cap);
    std::string s = b;
    if (t == NULL) return s + " -";
    s += " ";
    for (int i = 0; i <= len; i++) {
        snprintf(b, sizeof(b), "%s%x", i ? "," : "", (unsigned)t[i]);
        s += b;
    }
    return s;
}

int main(int argc, char** argv)
{
    const char* aPrint = argc > 2 && strcmp(argv[1], "--print") == 0 ? argv[2] : NULL;
    const int kSeqs = 20000, kSteps = 40, kGolden = 300;
    std::vector<std::string> aGolden;
    int aFailures = 0;
    long long aChecks = 0;
    for (int q = 0; q < kSeqs; q++) {
        sSeed = 0x9e3779b9u ^ (unsigned)(q * 2654435761u);
        OrigData* o[3];
        GUIStringData* p[3];
        for (int i = 0; i < 3; i++) {
            int c = R(5);
            o[i] = new OrigData((unsigned short)c);
            p[i] = new GUIStringData((unsigned short)c);
            char l[64];
            snprintf(l, sizeof(l), "s%d start %d %d", q, i, c);
            if (q < kGolden) aGolden.push_back(l);
        }
        for (int k = 0; k < kSteps; k++) {
            int i = R(3), j = R(3), op = R(15);
            int a = 0, b = 0, c = 0;
            std::string aName;
            OrigData& O = *o[i];
            GUIStringData& P = *p[i];
            int len = O.mLength;
            switch (op) {
            case 0: { // a new string: capacity, copy, substring, wide, narrow
                int kind = R(5);
                OrigData* no; GUIStringData* np;
                if (kind == 0) { a = R(9); no = new OrigData((unsigned short)a); np = new GUIStringData((unsigned short)a); aName = "newcap"; }
                else if (kind == 1) { no = new OrigData(*o[j]); np = new GUIStringData(*p[j]); aName = "newcopy"; }
                else if (kind == 2) { int jl = o[j]->mLength; a = R(jl + 1); b = R(jl - a + 1);
                    no = new OrigData(o[j], (unsigned short)a, (unsigned short)b); np = new GUIStringData(p[j], (unsigned short)a, (unsigned short)b); aName = "newsub"; }
                else if (kind == 3) { a = R(6); no = new OrigData(kWide[a]); np = new GUIStringData(kWide[a]); aName = "newwide"; }
                else { a = R(7); no = new OrigData(kNarrow[a]); np = new GUIStringData(kNarrow[a]); aName = "newnarrow"; }
                delete o[i]; delete p[i]; o[i] = no; p[i] = np;
                break;
            }
            case 1: a = R(40); b = R(2); O.Reserve(a, b != 0); P.Reserve(a, b != 0); aName = "reserve"; break;
            case 2: a = R(40); b = R(2); O.resize(a, b != 0); P.resize(a, b != 0); aName = "resize"; break;
            case 3: a = R(6); b = (int)wcslen(kWide[a]); b = R(b + 1); O.AssignWStr(kWide[a], b); P.AssignWStr(kWide[a], b); aName = "wstr"; break;
            case 4: a = R(7); b = (int)strlen(kNarrow[a]); b = R(b + 1); O.AssignStr(kNarrow[a], b); P.AssignStr(kNarrow[a], b); aName = "str"; break;
            case 5: a = 'A' + R(26); b = R(12); c = R(12); O.Fill(a, (unsigned short)b, (unsigned short)c); P.Fill(a, (unsigned short)b, (unsigned short)c); aName = "fill"; break;
            case 6: { int jl = o[j]->mLength; a = R(jl + 1); b = R(jl - a + 1);
                O.assign(o[j], (unsigned short)a, (unsigned short)b); P.assign(p[j], (unsigned short)a, (unsigned short)b); aName = "assign"; break; }
            case 7: a = 'a' + R(26); O.append(a); P.append(a); aName = "appendc"; break;
            case 8: { int jl = o[j]->mLength; a = R(jl + 1); b = R(jl - a + 1);
                O.append(o[j], (unsigned short)a, (unsigned short)b); P.append(p[j], (unsigned short)a, (unsigned short)b); aName = "append"; break; }
            case 9: case 10: a = R(len + 3) - 1; b = R(len + 3); // (a negative length writes past the end: UB) O.erase(a, b); P.erase(a, b); aName = "erase"; break;
            case 11: case 12: // (into itself: the original's memmove by the other's new length writes past the end, UB)
                j = (i + 1 + R(2)) % 3; a = R(len + 2); O.insert(a, o[j]); P.insert(a, p[j]); aName = "insert"; break;
            case 13: { a = R(len + 2); b = 0x20 + R(0x300); int ro = O.setAt(a, b), rp = P.setAt(a, b); c = ro;
                if (ro != rp) { if (aFailures++ < 20) printf("FAIL s%d k%d setAt answered %d / %d\n", q, k, ro, rp); }
                aName = "setat"; break; }
            default: { // the substring through GUIString (WONLobbyResource.h's inline substr)
                GUIString aWhole(new GUIStringData(*p[i]));
                a = R(len + 1); b = R(len - a + 1);
                GUIString aSub = aWhole.substr((unsigned short)a, (unsigned short)b);
                OrigData aWant(o[i], (unsigned short)a, (unsigned short)b);
                aChecks++;
                if (State(aWant.mText, aWant.mLength, aWant.mCapacity) !=
                        State(aSub.mData->mText, aSub.mData->mLength, aSub.mData->mCapacity) && aFailures++ < 20)
                    printf("FAIL s%d k%d substr %d %d\n", q, k, a, b);
                aName = "substr"; break; }
            }
            for (int t = 0; t < 3; t++) {
                std::string so = State(o[t]->mText, o[t]->mLength, o[t]->mCapacity);
                std::string sp = State(p[t]->mText, p[t]->mLength, p[t]->mCapacity);
                aChecks++;
                if (so != sp && aFailures++ < 20)
                    printf("FAIL s%d k%d %s %d %d %d %d %d: string %d\n  orig %s\n  port %s\n", q, k, aName.c_str(), i, j, a, b, c, t,
                           so.c_str(), sp.c_str());
            }
            if (q < kGolden) {
                char l[96];
                snprintf(l, sizeof(l), "s%d %s %d %d %d %d %d :", q, aName.c_str(), i, j, a, b, c);
                aGolden.push_back(std::string(l) + " " + State(o[i]->mText, o[i]->mLength, o[i]->mCapacity));
            }
        }
        for (int i = 0; i < 3; i++) { delete o[i]; delete p[i]; }
    }
    if (aPrint) {
        FILE* f = fopen(aPrint, "wb");
        for (size_t i = 0; i < aGolden.size(); i++) fprintf(f, "%s\n", aGolden[i].c_str());
        fclose(f);
        printf("wrote %d lines to %s\n", (int)aGolden.size(), aPrint);
    } else {
        std::string aPath = __FILE__;
        size_t aSlash = aPath.find_last_of("/\\");
        aPath = aPath.substr(0, aSlash + 1) + "../../rust/crates/ee_won_gui/src/golden_guistrobj.txt";
        FILE* f = fopen(aPath.c_str(), "rb");
        if (f == NULL) { printf("FAIL: no %s\n", aPath.c_str()); aFailures++; }
        else {
            static char aLine[65536];
            size_t n = 0;
            while (fgets(aLine, sizeof(aLine), f)) {
                size_t m = strlen(aLine);
                while (m && (aLine[m - 1] == '\n' || aLine[m - 1] == '\r')) aLine[--m] = 0;
                if (n >= aGolden.size() || aGolden[n] != aLine) { if (aFailures++ < 20) printf("FAIL golden line %d\n", (int)n + 1); }
                n++;
            }
            fclose(f);
            if (n != aGolden.size()) { printf("FAIL golden has %d lines, the run %d\n", (int)n, (int)aGolden.size()); aFailures++; }
        }
    }
    printf("won_guistrobj_test: %d sequences, %lld checks, %d failures\n", kSeqs, aChecks, aFailures);
    return aFailures;
}
