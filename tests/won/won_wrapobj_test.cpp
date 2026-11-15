// WrappedText as a Rust object (R6 WON round 38): the original WrappedText.cpp's statements (the C++ before the port:
// Init, CalcWrap over guiwrap's wrap, SetText, SetWrap, SetCentered, SetFont, SetMaxWidth and Paint's plan), copied
// over stand-ins for the text and the font, against ee_won's wrapobj.rs driven as the C++ shell drives it.
//
// Each of 20,000 sequences makes two fonts (each character's width 0-12 from the sequence, a height 0-20) and runs 40
// random calls: texts of 0-60 units from letters, spaces, CR, LF and units past 0xff; wrap on and off; centring;
// a font, the other or none; maximum widths -5..120; paints at random points. After each call the width, the
// height (the member, not GetHeight), the break list and the paint's operations (with a font) are compared. The first
// 300 sequences are rust/crates/ee_won/src/golden_wrapobj.txt, which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include "rust/ee_won.h"

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }

// ---- the stand-ins
struct GUIStringData { std::vector<wchar_t> mBuf; const wchar_t* mText; };
struct GUIString {
    GUIStringData* mData;
    GUIString() : mData(new GUIStringData) { mData->mBuf.push_back(0); mData->mText = &mData->mBuf[0]; }
    GUIString(const GUIString& o) : mData(new GUIStringData(*o.mData)) { mData->mText = &mData->mBuf[0]; }
    GUIString& operator=(const GUIString& o) { *mData = *o.mData; mData->mText = &mData->mBuf[0]; return *this; }
    int length() const { return (int)mData->mBuf.size() - 1; }
    wchar_t operator[](int i) const { return mData->mBuf[(size_t)i]; }
};
struct Font {
    int mId, mHeight, mW[16];
    int Width(int ch) const { return mW[(unsigned)ch % 16]; }
    int GetCharacterWidth(int ch) const { return Width(ch); }
    int GetHeight() const { return mHeight; }
    int GetStringWidth(const GUIString& s, unsigned short off, unsigned short len) const
    {
        int w = 0;
        for (int i = off; i < off + len && i < s.length(); i++) w += Width(s[i]);
        return w;
    }
};
template <class X> struct SmartPtr {
    X* p;
    SmartPtr() : p(NULL) {}
    SmartPtr& operator=(X* a) { p = a; return *this; }
    X* get() const { return p; }
    X* operator->() const { return p; }
};
struct WONDrawMeasure {
    Font* mFont;
    const GUIString* mText;
    static int32_t StringWidth(void* ctx, int32_t offset, int32_t length)
    {
        WONDrawMeasure* m = (WONDrawMeasure*)ctx;
        return m->mFont->GetStringWidth(*m->mText, (unsigned short)offset, (unsigned short)length);
    }
    static int32_t CharWidth(void* ctx, int32_t index)
    {
        WONDrawMeasure* m = (WONDrawMeasure*)ctx;
        return m->mFont->GetCharacterWidth(index < 0 ? 'M' : (*m->mText)[index]);
    }
    EeWonTextMeasure Get() { EeWonTextMeasure r = {this, StringWidth, CharWidth}; return r; }
};
struct Ops { std::vector<EeWonDrawOp> v; };

// ---- the original statements
namespace orig {
class WrappedText {
public:
    GUIString mText;
    SmartPtr<Font> mFont;
    struct BreakInfo {
        unsigned short mBreakPos, mNextStartPos, mWidth;
        BreakInfo(int p, int n, int w) : mBreakPos((unsigned short)p), mNextStartPos((unsigned short)n), mWidth((unsigned short)w) {}
    };
    typedef std::list<BreakInfo> BreakList;
    BreakList mBreakList;
    int mWidth, mHeight, mMaxWidth;
    bool mWrap, mIsCentered;
void Init()
{
	this->mWrap = true;
	this->mIsCentered = false;
	this->mWidth = this->mHeight = this->mMaxWidth = 0;
}
WrappedText()
{
	this->Init();
}
void CalcWrap()
{
	if(this->mFont.get()==NULL)
		return;

	if(!this->mWrap)
	{
		this->mWidth = this->mFont->GetStringWidth(this->mText,0,this->mText.length());
		this->mHeight = this->mFont->GetHeight();
		return;
	}

	if(this->mMaxWidth==0)
		return;

	this->mBreakList.clear();

	// the font measures each unit; the breaks are ee_won's guiwrap module
	int aLength = this->mText.length();
	std::vector<int> aWidths(aLength);
	for(int i = 0; i < aLength; i++)
		aWidths[i] = this->mFont->GetCharacterWidth(this->mText[i]);
	std::vector<EeWonBreak> aBreaks(2*aLength+1);
	int aWidth = 0;
	size_t aCount = ee_won_gui_wrap(this->mText.mData->mText, (uint16_t)aLength, sizeof(wchar_t),
		aLength ? &aWidths[0] : NULL, this->mMaxWidth, &aBreaks[0], aBreaks.size(), &aWidth);
	if(aCount > aBreaks.size())
	{
		aBreaks.resize(aCount);
		aCount = ee_won_gui_wrap(this->mText.mData->mText, (uint16_t)aLength, sizeof(wchar_t),
			aLength ? &aWidths[0] : NULL, this->mMaxWidth, &aBreaks[0], aBreaks.size(), &aWidth);
	}
	for(size_t k = 0; k < aCount; k++)
		this->mBreakList.push_back(BreakInfo(aBreaks[k].pos,aBreaks[k].next,aBreaks[k].width));
	this->mWidth = aWidth;

	this->mHeight = (this->mBreakList.size() + 1)*this->mFont->GetHeight();
}
void SetText(const GUIString &theText)
{
	this->mText = theText;
	this->CalcWrap();
}
void SetWrap(bool wrap)
{
	if(wrap!=this->mWrap)
	{
		this->mWrap = wrap;
		this->CalcWrap();
	}
}
void SetCentered(bool isCentered)
{
	this->mIsCentered = isCentered;
}
void SetFont(Font *theFont)
{
	this->mFont = theFont;
	this->CalcWrap();
}
void SetMaxWidth(int theMaxWidth)
{
	int aWidth = 0;
	bool aRewrap = false;
	if(!ee_won_gui_max_width(theMaxWidth, this->mMaxWidth, this->mWrap, !this->mBreakList.empty(), &aWidth, &aRewrap))
		return;
	this->mMaxWidth = aWidth;
	if(aRewrap)
		this->CalcWrap();
}
// Paint's plan (its WONDrawRun executes these operations on the Graphics)
void Paint(Ops& out, int x, int y)
{
	if(this->mFont.get()==NULL)
		return;

	std::vector<EeWonLineBreak> aBreaks;
	for(BreakList::iterator anItr = this->mBreakList.begin(); anItr!=this->mBreakList.end(); ++anItr)
	{
		EeWonLineBreak b;
		b.break_pos = anItr->mBreakPos;
		b.next_start = anItr->mNextStartPos;
		b.width = anItr->mWidth;
		aBreaks.push_back(b);
	}
	WONDrawMeasure aMeasure = { this->mFont.get(), &this->mText };
	EeWonTextMeasure m = aMeasure.Get();
	int aFontHeight = this->mFont->GetHeight();
	out.v.resize(512);
	size_t n = ee_won_gui_draw_wrapped_text(true,this->mText.length(),this->mIsCentered,this->mWidth,aFontHeight,
			aBreaks.empty() ? NULL : &aBreaks[0],aBreaks.size(),x,y,&m,&out.v[0],out.v.size());
	out.v.resize(n);
}
};
}

// ---- the port: the C++ shell's statements over the Rust object
namespace port {
class WrappedText {
public:
    GUIString mText;
    SmartPtr<Font> mFont;
    EeWonWrapText* mObj;
    WrappedText() : mObj(ee_won_wrap_new()) {}
    ~WrappedText() { ee_won_wrap_free(mObj); }
    void CalcWrap()
    {
        WONDrawMeasure aMeasure = { this->mFont.get(), &this->mText };
        EeWonTextMeasure m = aMeasure.Get();
        ee_won_wrap_calc(this->mObj, this->mFont.get() != NULL, this->mText.mData->mText, this->mText.length(), sizeof(wchar_t), &m,
                         this->mFont.get() != NULL ? this->mFont->GetHeight() : 0);
    }
    void SetText(const GUIString& t) { this->mText = t; this->CalcWrap(); }
    void SetWrap(bool w) { if (ee_won_wrap_set_wrap(this->mObj, w)) this->CalcWrap(); }
    void SetCentered(bool c) { ee_won_wrap_set_centered(this->mObj, c); }
    void SetFont(Font* f) { this->mFont = f; this->CalcWrap(); }
    void SetMaxWidth(int w) { if (ee_won_wrap_set_max_width(this->mObj, w)) this->CalcWrap(); }
    void Paint(Ops& out, int x, int y)
    {
        if (this->mFont.get() == NULL) return;
        WONDrawMeasure aMeasure = { this->mFont.get(), &this->mText };
        EeWonTextMeasure m = aMeasure.Get();
        out.v.resize(512);
        size_t n = ee_won_wrap_paint(this->mObj, this->mText.length(), this->mFont->GetHeight(), x, y, &m, &out.v[0], out.v.size());
        out.v.resize(n);
    }
};
}

static GUIString RandomText()
{
    static const wchar_t kU[] = {L'a', L'b', L'M', L' ', L' ', L'\r', L'\n', L'x', 0x3a9, 0x4e2d};
    GUIString s;
    int n = R(5) == 0 ? 0 : RI(1, 60);
    s.mData->mBuf.clear();
    for (int i = 0; i < n; i++) s.mData->mBuf.push_back(kU[R(10)]);
    s.mData->mBuf.push_back(0);
    s.mData->mText = &s.mData->mBuf[0];
    return s;
}

static std::string Text(const GUIString& s)
{
    std::string r;
    for (int i = 0; i < s.length(); i++) { char a[12]; snprintf(a, 12, "%s%x", i ? "," : "", (unsigned)s[i]); r += a; }
    return r.empty() ? "-" : r;
}

static std::string OpsText(const Ops& o)
{
    std::string r;
    for (size_t i = 0; i < o.v.size(); i++) {
        const EeWonDrawOp& p = o.v[i];
        char a[120];
        snprintf(a, sizeof(a), "%s%d:%d,%d,%d,%d,%d,%d,%d,%d", i ? " " : "", p.kind, p.a, p.b, p.c, p.d, p.e, p.f, p.g, p.h);
        r += a;
    }
    return r.empty() ? "-" : r;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;
    int sequences = print ? 300 : 20000;
    long checks = 0, failures = 0;
    for (int s = 0; s < sequences; s++) {
        sSeed = 5000u + (unsigned)s;
        Font fonts[2];
        for (int f = 0; f < 2; f++) {
            fonts[f].mId = f + 1;
            fonts[f].mHeight = RI(0, 20);
            for (int i = 0; i < 16; i++) fonts[f].mW[i] = RI(0, 12);
        }
        if (print) {
            printf("S %d\n", s);
            for (int f = 0; f < 2; f++) {
                printf("F %d", fonts[f].mHeight);
                for (int i = 0; i < 16; i++) printf(" %d", fonts[f].mW[i]);
                printf("\n");
            }
        }
        orig::WrappedText o;
        port::WrappedText p;
        for (int k = 0; k < 40; k++) {
            int op = (int)R(6);
            GUIString t;
            int a = 0, b = 0;
            Ops oo, po;
            switch (op) {
            case 0: t = RandomText(); o.SetText(t); p.SetText(t); break;
            case 1: a = (int)R(2); o.SetWrap(a != 0); p.SetWrap(a != 0); break;
            case 2: a = (int)R(2); o.SetCentered(a != 0); p.SetCentered(a != 0); break;
            case 3: a = (int)R(3); o.SetFont(a ? &fonts[a - 1] : NULL); p.SetFont(a ? &fonts[a - 1] : NULL); break;
            case 4: a = R(6) == 0 ? 0 : RI(-5, 120); o.SetMaxWidth(a); p.SetMaxWidth(a); break;
            default: a = RI(-20, 50); b = RI(-20, 50); o.Paint(oo, a, b); p.Paint(po, a, b); break;
            }
            std::string ob, pb;
            for (orig::WrappedText::BreakList::iterator i = o.mBreakList.begin(); i != o.mBreakList.end(); ++i) {
                char x[40]; snprintf(x, 40, " %u/%u/%u", i->mBreakPos, i->mNextStartPos, i->mWidth); ob += x;
            }
            std::vector<EeWonLineBreak> bb(256);
            bb.resize(ee_won_wrap_breaks(p.mObj, &bb[0], bb.size()));
            for (size_t i = 0; i < bb.size(); i++) {
                char x[40]; snprintf(x, 40, " %u/%u/%u", bb[i].break_pos, bb[i].next_start, bb[i].width); pb += x;
            }
            char line[200];
            snprintf(line, sizeof(line), "%d %d", o.mWidth, o.mHeight);
            std::string os = line + ob + " | " + OpsText(oo);
            snprintf(line, sizeof(line), "%d %d", ee_won_wrap_width(p.mObj), ee_won_wrap_height(p.mObj));
            std::string ps = line + pb + " | " + OpsText(po);
            ++checks;
            bool bad = os != ps;
            if (bad && ++failures <= 10) printf("MISMATCH seq %d call %d op %d %d %d\n  orig %s\n  port %s\n", s, k, op, a, b, os.c_str(), ps.c_str());
            if (print) {
                printf("O %d %d %d %s\n", op, a, b, op == 0 ? Text(t).c_str() : "-");
                printf("R %s\n", os.c_str());
            }
        }
    }
    if (!print) printf("won_wrapobj_test: %d sequences, %ld checks, %ld failures\n", sequences, checks, failures);
    return failures ? 1 : 0;
}
