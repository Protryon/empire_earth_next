// The window's font cache: Window::GetFont's original statements (its std::map<FontDescriptor, SmartPtr<Font>> and
// FontDescriptor::operator<, Font.cpp) against ee_won fontmap.rs, over random descriptors (faces in mixed case,
// styles, sizes; the platform sometimes answering NULL). After each lookup: the font returned, the platform's
// calls and every font's reference count; at the end, the fonts each map releases (as a set: the std::map's
// destruction order is its tree's, the Rust map's the key order). `--print` prints the first sequences' traces.
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "rust/ee_won.h"

static int sFailures, sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 20) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static unsigned sSeed;
static unsigned R(unsigned n) { sSeed = sSeed * 69069u + 1u; return n ? (sSeed >> 16) % n : 0; }

// the stand-ins: a font is its id and reference count; the platform makes them in order
struct Font { int mId; int mRefs; };
static std::vector<Font*> sFonts;
static int sPlatformCalls;
static std::vector<int> sReleased;
static void Release(Font* f) { if (--f->mRefs == 0) sReleased.push_back(f->mId); }
template <class X> struct SmartPtr {
    X* p;
    SmartPtr(X* a = NULL) : p(a) { if (p) p->mRefs++; }
    SmartPtr(const SmartPtr& o) : p(o.p) { if (p) p->mRefs++; }
    ~SmartPtr() { if (p) Release(p); }
    SmartPtr& operator=(const SmartPtr& o) { if (o.p) o.p->mRefs++; if (p) Release(p); p = o.p; return *this; }
    X* get() const { return p; }
};
static SmartPtr<Font> PlatformGetFont()
{
    sPlatformCalls++;
    if (R(5) == 0) return NULL;
    Font* f = new Font;
    f->mId = (int)sFonts.size() + 1;
    f->mRefs = 0;
    sFonts.push_back(f);
    return f;
}

struct FontDescriptor {
    std::string mFace;
    int mStyle, mSize;
    // Font.cpp's operator< (StringCompareNoCase is ee_won_compare_no_case)
    bool operator<(const FontDescriptor& other) const {
        if(mSize<other.mSize) return true;
        if(mSize>other.mSize) return false;
        if(mStyle<other.mStyle) return true;
        if(mStyle>other.mStyle) return false;
        return ee_won_compare_no_case((const uint8_t*)mFace.data(), mFace.size(), (const uint8_t*)other.mFace.data(),
                                      other.mFace.size()) < 0;
    }
};

// the original (Window.cpp before the port)
struct OrigWindow {
    std::map<FontDescriptor,SmartPtr<Font> > mFontMap;
    SmartPtr<Font> GetFont(const FontDescriptor &theDescriptor)
    {
        std::map<FontDescriptor,SmartPtr<Font> >::iterator anItr = this->mFontMap.insert(std::map<FontDescriptor,SmartPtr<Font> >::value_type(theDescriptor, NULL)).first;
        SmartPtr<Font> aFont = anItr->second;
        if(aFont.get()==NULL)
        {
            aFont = PlatformGetFont();
            anItr->second = aFont;
        }
        return aFont;
    }
};

// the port (Window.cpp now)
struct PortWindow {
    EeWonFontMap* p;
    PortWindow() : p(ee_won_fontmap_new()) {}
    ~PortWindow()
    {
        for (intptr_t aFont; (aFont = ee_won_fontmap_pop(this->p)) >= 0; )
            if (aFont != 0) Release((Font*)aFont);
        ee_won_fontmap_free(this->p);
    }
    SmartPtr<Font> GetFont(const FontDescriptor &theDescriptor)
    {
        const std::string& f = theDescriptor.mFace;
        Font* aFont = (Font*)ee_won_fontmap_lookup(this->p, (const uint8_t*)f.data(), f.size(), theDescriptor.mStyle, theDescriptor.mSize);
        if (aFont == NULL) {
            SmartPtr<Font> aNew = PlatformGetFont();
            aFont = aNew.get();
            if (aFont != NULL) aFont->mRefs++;
            ee_won_fontmap_store(this->p, (const uint8_t*)f.data(), f.size(), theDescriptor.mStyle, theDescriptor.mSize, (uintptr_t)aFont);
        }
        return aFont;
    }
};

static const char* kFaces[] = {"Arial", "ARIAL", "arial", "Tahoma", "tahoma", "", "Times New Roman", "times new roman", "Ar"};

// one sequence on one side: its trace lines
template <class W> static std::vector<std::string> Run(unsigned seed)
{
    sSeed = seed;
    for (size_t i = 0; i < sFonts.size(); i++) delete sFonts[i];
    sFonts.clear();
    sPlatformCalls = 0;
    sReleased.clear();
    std::vector<std::string> t;
    char line[200];
    {
        W w;
        int n = 1 + (int)R(60);
        for (int i = 0; i < n; i++) {
            FontDescriptor d;
            d.mFace = kFaces[R(9)];
            d.mStyle = (int)R(4) - 1;
            d.mSize = 8 + (int)R(5);
            SmartPtr<Font> f = w.GetFont(d);
            snprintf(line, sizeof(line), "get %s %d %d -> %d calls %d", d.mFace.c_str(), d.mStyle, d.mSize,
                     f.get() ? f.get()->mId : 0, sPlatformCalls);
            std::string s = line;
            for (size_t k = 0; k < sFonts.size(); k++) s += " " + std::to_string(sFonts[k]->mRefs);
            t.push_back(s);
        }
    }
    std::set<int> rel(sReleased.begin(), sReleased.end());
    std::string s = "released";
    for (std::set<int>::iterator i = rel.begin(); i != rel.end(); ++i) s += " " + std::to_string(*i);
    snprintf(line, sizeof(line), " (%d of %d)", (int)sReleased.size(), (int)sFonts.size());
    t.push_back(s + line);
    return t;
}

int main(int argc, char** argv)
{
    bool aPrint = argc > 1 && strcmp(argv[1], "--print") == 0;
    int aDiffer = 0;
    size_t aLines = 0;
    for (unsigned seq = 0; seq < 20000; seq++) {
        std::vector<std::string> a = Run<OrigWindow>(seq * 7919u + 3u);
        std::vector<std::string> b = Run<PortWindow>(seq * 7919u + 3u);
        aLines += a.size();
        CHECK(a.size() == b.size());
        for (size_t i = 0; i < a.size() && i < b.size(); i++) {
            CHECK(a[i] == b[i]);
            if (a[i] != b[i] && aDiffer < 5) printf("seq %u line %u\n  orig: %s\n  port: %s\n", seq, (unsigned)i, a[i].c_str(), b[i].c_str());
        }
        if (a != b) aDiffer++;
        if (aPrint && seq < 3)
            for (size_t i = 0; i < a.size(); i++) printf("%s\n", a[i].c_str());
    }
    printf("won_fontmap_test: 20000 sequences, %u lines, %d differing, %d checks, %d failures\n", (unsigned)aLines, aDiffer,
           sChecks, sFailures);
    return sFailures == 0 ? 0 : 1;
}
