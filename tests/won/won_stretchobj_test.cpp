// StretchImage's settings as a Rust object (R6 WON round 38): the original StretchImage.cpp's statements (the C++ before
// the port: the constructor, SetSize, CalcImage's filter settings, Paint, SetImage, SetBorders, CopyAttributes,
// Duplicate, GetNative, SetTile, SetSkipCenter) over stand-ins for the images (a cached image is the filter settings it
// was built from), against the C++ shell's statements over ee_won's stretchobj.rs.
//
// Each of 20,000 sequences keeps three images per side (made with random borders, some duplicates of others) and runs
// 60 random calls on them: sizes -3..80 (and 70,000, past the Image's unsigned short), borders -2..30, tiling,
// skip-centre, copies between them, duplicates replacing one, GetNative and Paint. After each call every image's
// state (size, borders, flags, the rebuild flag, the cached image's settings) and the call's trace (the filters built,
// the paint's operations) are compared. The first 300 sequences are rust/crates/ee_won/src/golden_stretchobj.txt,
// which the Rust tests replay; `--print` prints them.
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>
#include "rust/ee_won.h"

static unsigned sSeed;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 16) & 0x7fff; }
static unsigned R(unsigned n) { return n ? Rand() % n : 0; }
static int RI(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }
static std::string sTrace;
static void T(const std::string& s) { if (!sTrace.empty()) sTrace += "; "; sTrace += s; }

struct Filter {
    int mWidth, mHeight, mLeftWidth, mRightWidth, mTopHeight, mBottomHeight;
    bool mTileHorz, mTileVert, mSkipCenter;
    void SetSize(int w, int h) { mWidth = w; mHeight = h; }
    void SetBorders(int l, int t, int r, int b) { mLeftWidth = l; mRightWidth = r; mTopHeight = t; mBottomHeight = b; }
    void SetTile(bool h, bool v) { mTileHorz = h; mTileVert = v; }
    void SetSkipCenter(bool b) { mSkipCenter = b; }
    std::string Text() const
    {
        char a[120];
        snprintf(a, sizeof(a), "%d,%d,%d,%d,%d,%d,%d,%d,%d", mWidth, mHeight, mLeftWidth, mTopHeight, mRightWidth, mBottomHeight,
                 mTileHorz, mTileVert, mSkipCenter);
        return a;
    }
};
static std::string Plan(size_t (*f)(EeWonDrawOp*, size_t, const void*), const void* ctx)
{
    EeWonDrawOp ops[32];
    size_t n = f(ops, 32, ctx);
    std::string r;
    for (size_t i = 0; i < n && i < 32; i++) {
        char a[100];
        snprintf(a, sizeof(a), "%s%d:%d,%d,%d,%d,%d", i ? " " : "", ops[i].kind, ops[i].a, ops[i].b, ops[i].c, ops[i].d, ops[i].e);
        r += a;
    }
    return r;
}

// ---- the original statements
namespace orig {
struct StretchImage {
    unsigned short mWidth, mHeight;
    int mImage; // the source raw image's number
    std::string mCachedImage;
    bool mNeedRecalc;
    int mLeftWidth, mRightWidth, mTopHeight, mBottomHeight;
    bool mTileHorz, mTileVert, mSkipCenter;
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
StretchImage(int theImage, int leftWidth, int topHeight, int rightWidth, int bottomHeight)
{
	this->mImage = theImage;
	this->mWidth = this->mHeight = 0;
	this->mLeftWidth = leftWidth;
	this->mRightWidth = rightWidth;
	this->mTopHeight = topHeight;
	this->mBottomHeight = bottomHeight;
	this->mNeedRecalc = true;
	this->mTileHorz = this->mTileVert = true;
	this->mSkipCenter = false;
}
void SetSize(int theWidth, int theHeight)
{
	if(theWidth==this->mWidth && theHeight==this->mHeight)
		return;

	this->mWidth = theWidth;
	this->mHeight = theHeight;
	this->mNeedRecalc = true;
}
void CalcImage()
{
	Filter aFilter;
	aFilter.SetBorders(this->mLeftWidth,this->mTopHeight,this->mRightWidth,this->mBottomHeight);
	aFilter.SetSize(this->GetWidth(),this->GetHeight());
	aFilter.SetTile(this->mTileHorz,this->mTileVert);
	aFilter.SetSkipCenter(this->mSkipCenter);
	this->mCachedImage = std::to_string(this->mImage) + "/" + aFilter.Text();
	T("calc " + this->mCachedImage);
	this->mNeedRecalc = false;
}
static size_t PaintPlan(EeWonDrawOp* o, size_t n, const void* c)
{
	const StretchImage* t = (const StretchImage*)c;
	return ee_won_gui_draw_stretch_paint(t->mSkipCenter,t->mLeftWidth,t->mRightWidth,t->mTopHeight,t->mBottomHeight,t->mWidth,t->mHeight,o,n);
}
void Paint()
{
	if(this->mNeedRecalc)
		this->CalcImage();
	T("paint " + this->mCachedImage + " " + Plan(PaintPlan, this));
}
void SetImage(int theImage)
{
	this->mImage = theImage;
}
void SetBorders(int left, int top, int right, int bottom)
{
	this->mLeftWidth = left;
	this->mTopHeight = top;
	this->mRightWidth = right;
	this->mBottomHeight = bottom;
}
void CopyAttributes(StretchImage *from)
{
	this->mLeftWidth = from->mLeftWidth;
	this->mTopHeight = from->mTopHeight;
	this->mRightWidth = from->mRightWidth;
	this->mBottomHeight = from->mBottomHeight;
	this->mTileHorz = from->mTileHorz;
	this->mTileVert = from->mTileVert;
	this->mSkipCenter = from->mSkipCenter;
	this->mWidth = from->mWidth;
	this->mHeight = from->mHeight;
}
void SetTile(bool h,bool v) { this->mTileHorz=h; this->mTileVert=v; }
void SetSkipCenter(bool b) { this->mSkipCenter=b; }
StretchImage* Duplicate()
{
	StretchImage* aCopy = new StretchImage(this->mImage,this->mLeftWidth,this->mTopHeight,this->mRightWidth,this->mBottomHeight);
	aCopy->SetTile(this->mTileHorz,this->mTileVert);
	aCopy->SetSkipCenter(this->mSkipCenter);
	return aCopy;
}
std::string GetNative()
{
	if(this->mNeedRecalc)
		this->CalcImage();

	return this->mCachedImage;
}
std::string State()
{
	char a[160];
	snprintf(a, sizeof(a), "%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s", mWidth, mHeight, mImage, mLeftWidth, mTopHeight, mRightWidth,
	         mBottomHeight, mTileHorz, mTileVert, mSkipCenter, mNeedRecalc, mCachedImage.c_str());
	return a;
}
};
}

// ---- the port: the C++ shell's statements (WONLobbyStretchImage.h, GuiHost.cpp) over the Rust object
namespace port {
struct StretchImage {
    unsigned short mWidth, mHeight;
    int mImage;
    std::string mCachedImage;
    EeWonStretchImg* mObj;
    StretchImage(int i, EeWonStretchImg* o) : mImage(i), mObj(o) { this->mWidth = this->mHeight = 0; }
    StretchImage(int i, int l, int t, int r, int b) : mImage(i), mObj(ee_won_stretch_new(l, t, r, b)) { this->mWidth = this->mHeight = 0; }
    ~StretchImage() { ee_won_stretch_free(mObj); }
    int Get(EeWonStretchField f) { return ee_won_stretch_get(this->mObj, f); }
    int GetWidth() { return mWidth; }
    int GetHeight() { return mHeight; }
    int GetLeft() { return this->Get(EeWonStretchField::Left); }
    int GetRight() { return this->Get(EeWonStretchField::Right); }
    int GetTop() { return this->Get(EeWonStretchField::Top); }
    int GetBottom() { return this->Get(EeWonStretchField::Bottom); }
    StretchImage* Duplicate() { return new StretchImage(this->mImage, ee_won_stretch_duplicate(this->mObj)); }
    std::string GetNative() { if (this->Get(EeWonStretchField::NeedRecalc)) this->CalcImage(); return this->mCachedImage; }
    void SetSize(int w, int h) { if (ee_won_stretch_set_size(this->mObj, this->mWidth, this->mHeight, w, h)) { this->mWidth = w; this->mHeight = h; } }
    void SetImage(int i) { this->mImage = i; }
    void SetBorders(int l, int t, int r, int b) { ee_won_stretch_set_borders(this->mObj, l, t, r, b); }
    void CopyAttributes(StretchImage* f) { ee_won_stretch_copy(this->mObj, f->mObj); this->mWidth = f->mWidth; this->mHeight = f->mHeight; }
    void SetTile(bool h, bool v) { ee_won_stretch_set_tile(this->mObj, h, v); }
    void SetSkipCenter(bool b) { ee_won_stretch_set_skip(this->mObj, b); }
    void CalcImage()
    {
        Filter aFilter;
        aFilter.SetBorders(this->GetLeft(), this->GetTop(), this->GetRight(), this->GetBottom());
        aFilter.SetSize(this->GetWidth(), this->GetHeight());
        aFilter.SetTile(this->Get(EeWonStretchField::TileHorz) != 0, this->Get(EeWonStretchField::TileVert) != 0);
        aFilter.SetSkipCenter(this->Get(EeWonStretchField::SkipCenter) != 0);
        this->mCachedImage = std::to_string(this->mImage) + "/" + aFilter.Text();
        T("calc " + this->mCachedImage);
        ee_won_stretch_built(this->mObj);
    }
    static size_t PaintPlan(EeWonDrawOp* o, size_t n, const void* c)
    {
        const StretchImage* t = (const StretchImage*)c;
        return ee_won_stretch_paint(t->mObj, t->mWidth, t->mHeight, o, n);
    }
    void Paint()
    {
        if (this->Get(EeWonStretchField::NeedRecalc)) this->CalcImage();
        T("paint " + this->mCachedImage + " " + Plan(PaintPlan, this));
    }
    std::string State()
    {
        char a[160];
        snprintf(a, sizeof(a), "%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%s", mWidth, mHeight, mImage, GetLeft(), GetTop(), GetRight(), GetBottom(),
                 Get(EeWonStretchField::TileHorz), Get(EeWonStretchField::TileVert), Get(EeWonStretchField::SkipCenter),
                 Get(EeWonStretchField::NeedRecalc), mCachedImage.c_str());
        return a;
    }
};
}

template <class S> static std::string Run(S** im, int op, int i, int j, const int* v)
{
    sTrace.clear();
    S* x = im[i];
    switch (op) {
    case 0: x->SetSize(v[0], v[1]); break;
    case 1: x->SetBorders(v[0], v[1], v[2], v[3]); break;
    case 2: x->SetTile(v[0] & 1, v[1] & 1); break;
    case 3: x->SetSkipCenter(v[0] & 1); break;
    case 4: x->CopyAttributes(im[j]); break;
    case 5: { S* d = im[j]->Duplicate(); delete im[i]; im[i] = d; break; }
    case 6: T("native " + x->GetNative()); break;
    case 7: x->Paint(); break;
    case 8: x->SetImage(v[0]); break;
    default: { S* n = new S(v[0], v[1], v[2], v[3], v[4]); delete im[i]; im[i] = n; break; }
    }
    std::string s = sTrace + " |";
    for (int k = 0; k < 3; k++) s += " " + im[k]->State();
    return s;
}

int main(int argc, char** argv)
{
    bool print = argc > 1 && strcmp(argv[1], "--print") == 0;
    int sequences = print ? 300 : 20000;
    long checks = 0, failures = 0;
    for (int s = 0; s < sequences; s++) {
        sSeed = 777u + (unsigned)s;
        orig::StretchImage* o[3];
        port::StretchImage* p[3];
        if (print) printf("S %d\n", s);
        for (int k = 0; k < 3; k++) {
            int v[5] = {k + 1, RI(-2, 30), RI(-2, 30), RI(-2, 30), RI(-2, 30)};
            o[k] = new orig::StretchImage(v[0], v[1], v[2], v[3], v[4]);
            p[k] = new port::StretchImage(v[0], v[1], v[2], v[3], v[4]);
            if (print) printf("N %d %d %d %d %d\n", v[0], v[1], v[2], v[3], v[4]);
        }
        for (int c = 0; c < 60; c++) {
            int op = (int)R(10), i = (int)R(3), j = (int)R(3);
            int v[5];
            for (int k = 0; k < 5; k++) v[k] = RI(-2, 30);
            if (op == 0) { v[0] = R(8) == 0 ? 70000 : RI(-3, 80); v[1] = R(4) == 0 ? o[i]->mHeight : RI(-3, 80); if (R(3) == 0) v[0] = o[i]->mWidth; }
            if (op == 8 || op == 9) v[0] = RI(1, 9);
            std::string a = Run(o, op, i, j, v), b = Run(p, op, i, j, v);
            ++checks;
            if (a != b && ++failures <= 10) printf("MISMATCH seq %d call %d op %d\n  orig %s\n  port %s\n", s, c, op, a.c_str(), b.c_str());
            if (print) printf("O %d %d %d %d %d %d %d %d\nR %s\n", op, i, j, v[0], v[1], v[2], v[3], v[4], b.c_str());
        }
        for (int k = 0; k < 3; k++) { delete o[k]; delete p[k]; }
    }
    if (!print) printf("won_stretchobj_test: %d sequences, %ld checks, %ld failures\n", sequences, checks, failures);
    return failures ? 1 : 0;
}
