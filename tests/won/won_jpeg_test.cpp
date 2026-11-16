// JPGDecoder (R6 WON round 41): the original's statements on libjpeg 6b (tests/won/won_jpeg_orig.h, copied from
// JPGDecoder.cpp before the round) against the port (WONGUI/ImageDecoder.cpp's JPGDecoder over ee_won's jpeg.rs),
// each through the class's own steps (DoGetAttrib as GetImageAttributes calls it, DoDecodeRaw as DecodeRaw calls
// it) over a memory FileReader. The pictures: the lobby's JPEGs (the open tree's "Data/WONLobby Resources", when it
// is there: EE_DATA_ROOT or the main worktree's build/open_data/tree) and JPEGs libjpeg's own compressor writes
// here: grey, YCbCr, RGB (Adobe or by component ids), CMYK and YCCK; every sampling factor libjpeg can write;
// qualities 1..100 (16-bit tables below 25 without the baseline limit); restart intervals; optimized tables;
// progressive scans and non-interleaved sequential scans; sizes 1..300; then each one cut short, with bytes
// flipped, with markers dropped in, with a segment longer than the file (the reader's throw), with its scans' data
// garbled. Every case compares the attributes' answer and size, the decode's NULL or size and pixels, and a thrown
// FileReaderException's text.
//
// --golden PATH writes the small cases (the inputs in hex and both answers) for ee_won's cargo tests
// (rust/crates/ee_won/src/golden_jpeg.txt). Prints one line per failed check; the exit code is the failure count.
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyPersistence.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyPlatform.h"
using namespace WONAPI;

namespace WONAPI {
static void NotHere() { printf("won_jpeg_test: a display function was called\n"); exit(99); }
SmartPtr<NativeImage> DisplayContext::CreateImage(int, int) { NotHere(); return NULL; }
void Graphics::DrawImage(Image*, int, int) { NotHere(); }
void Graphics::Translate(int, int) { NotHere(); }
void Graphics::PushClipRect(int, int, int, int) { NotHere(); }
void Graphics::PopClipRect() { NotHere(); }
void RawImage8::CalcBytesPerRow() { NotHere(); }
void RawImage1::CalcBytesPerRow() { NotHere(); }
SmartPtr<NativeImage> RawImage32::CreateNativeImage(DisplayContext*) { NotHere(); return NULL; }
SmartPtr<NativeImage> RawImage8::CreateNativeImage(DisplayContext*) { NotHere(); return NULL; }
SmartPtr<NativeMask> RawImage1::GetMask(DisplayContext*) { NotHere(); return NULL; }
SmartPtr<NativeImage> RawImage1::GetNative(DisplayContext*) { NotHere(); return NULL; }
SmartPtr<NativeIcon> NativeImage::GetIcon() { NotHere(); return NULL; }
SmartPtr<RawImage32> NativeImage::GetRaw() { NotHere(); return NULL; }
SmartPtr<NativeImage> PlatGUIPackImage(DisplayContext*, NativeImage*, const char*) { NotHere(); return NULL; }
}

#include "won_jpeg_orig.h"

static int sFailures, sCases;
#define CHECK(c) do { if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

struct TestJPG : public JPGDecoder {
    bool Attrib(FileReader& r, ImageAttributes& a) { mFileReader = &r; return DoGetAttrib(a); }
    SmartPtr<RawImage> Raw(FileReader& r) { mFileReader = &r; return DoDecodeRaw(); }
};

// one side's answers: "A 1 w h" / "A 0" / "A throw <text>", then "D w h fnv" / "D NULL" / "D throw <text>"
static unsigned long long Fnv(const RawImage32* img)
{
    unsigned long long h = 1469598103934665603ull;
    const DWORD* px = const_cast<RawImage32*>(img)->GetImageData();
    size_t count = (size_t)const_cast<RawImage32*>(img)->GetWidth() * const_cast<RawImage32*>(img)->GetHeight();
    for (size_t i = 0; i < count; i++)
        for (int b = 0; b < 32; b += 8) { h ^= (px[i] >> b) & 0xff; h *= 1099511628211ull; }
    return h;
}
static std::string Answers(const std::vector<unsigned char>& in, bool orig)
{
    char line[600];
    std::string out;
    {
        FileReader r(in.empty() ? "" : (const void*)in.data(), (unsigned long)in.size());
        ImageAttributes a;
        try {
            bool ok = orig ? OrigGetAttrib(&r, a) : TestJPG().Attrib(r, a);
            snprintf(line, sizeof line, ok ? "A 1 %d %d" : "A 0", a.mWidth, a.mHeight);
        } catch (FileReaderException& e) { snprintf(line, sizeof line, "A throw %s", e.mDescription.c_str()); }
        out = line;
    }
    {
        FileReader r(in.empty() ? "" : (const void*)in.data(), (unsigned long)in.size());
        try {
            SmartPtr<RawImage> raw = orig ? OrigDecodeRaw(&r) : TestJPG().Raw(r);
            if (raw.get() == NULL) snprintf(line, sizeof line, " D NULL");
            else {
                RawImage32* img = static_cast<RawImage32*>(raw.get());
                snprintf(line, sizeof line, " D %d %d %016llx", img->GetWidth(), img->GetHeight(), Fnv(img));
            }
        } catch (FileReaderException& e) { snprintf(line, sizeof line, " D throw %s", e.mDescription.c_str()); }
        out += line;
    }
    return out;
}

static FILE* sGolden;
static int sGoldenCount;
static void Compare(const char* what, const std::vector<unsigned char>& in)
{
    std::string want = Answers(in, true), got = Answers(in, false);
    sCases++;
    if (want != got) {
        sFailures++;
        if (sFailures < 40) printf("FAIL %s (%u bytes)\n  orig %s\n  port %s\n", what, (unsigned)in.size(), want.c_str(), got.c_str());
    }
    if (sGolden != NULL && in.size() <= 900 && sGoldenCount < 400) {
        for (unsigned char b : in) fprintf(sGolden, "%02x", b);
        fprintf(sGolden, "%s %s\n", in.empty() ? "-" : "", want.c_str());
        sGoldenCount++;
    }
}

// ---- the encoder: libjpeg's compressor into memory

struct MemDest { jpeg_destination_mgr pub; std::vector<unsigned char>* out; JOCTET buf[1024]; };
static void DestInit(j_compress_ptr c) { MemDest* d = (MemDest*)c->dest; d->pub.next_output_byte = d->buf; d->pub.free_in_buffer = sizeof d->buf; }
static boolean DestEmpty(j_compress_ptr c)
{
    MemDest* d = (MemDest*)c->dest;
    d->out->insert(d->out->end(), d->buf, d->buf + sizeof d->buf);
    d->pub.next_output_byte = d->buf; d->pub.free_in_buffer = sizeof d->buf;
    return TRUE;
}
static void DestTerm(j_compress_ptr c)
{
    MemDest* d = (MemDest*)c->dest;
    d->out->insert(d->out->end(), d->buf, d->buf + (sizeof d->buf - d->pub.free_in_buffer));
}
static void EncError(j_common_ptr c) { longjmp(((my_error_mgr*)c->err)->setjmp_buffer, 1); }

static unsigned sSeed = 12345;
static unsigned Rand() { sSeed = sSeed * 1103515245u + 12345u; return (sSeed >> 8) & 0xffffff; }
static int RandIn(int lo, int hi) { return lo + (int)(Rand() % (unsigned)(hi - lo + 1)); }

struct Settings {
    int w, h, kind;          // kind: 0 grey, 1 YCbCr (JFIF), 2 RGB (Adobe), 3 RGB by ids, 4 CMYK, 5 YCCK, 6 YCbCr (Adobe only)
    int quality; bool baseline, optimize, progressive, separate; int restart, restartRows;
    int samp[4][2];
};

static bool Encode(const Settings& s, std::vector<unsigned char>& out)
{
    jpeg_compress_struct c;
    my_error_mgr err;
    c.err = jpeg_std_error(&err.pub);
    err.pub.error_exit = EncError;
    err.pub.output_message = [](j_common_ptr) {};
    MemDest dest;
    if (setjmp(err.setjmp_buffer)) { jpeg_destroy_compress(&c); return false; }
    jpeg_create_compress(&c);
    dest.out = &out;
    dest.pub.init_destination = DestInit; dest.pub.empty_output_buffer = DestEmpty; dest.pub.term_destination = DestTerm;
    c.dest = &dest.pub;
    c.image_width = s.w; c.image_height = s.h;
    int comps = s.kind == 0 ? 1 : s.kind >= 4 && s.kind <= 5 ? 4 : 3;
    c.input_components = comps;
    c.in_color_space = comps == 1 ? JCS_GRAYSCALE : comps == 4 ? JCS_CMYK : JCS_RGB;
    jpeg_set_defaults(&c);
    switch (s.kind) {
    case 2: jpeg_set_colorspace(&c, JCS_RGB); break;
    case 3: jpeg_set_colorspace(&c, JCS_RGB); c.write_Adobe_marker = FALSE; break;
    case 4: jpeg_set_colorspace(&c, JCS_CMYK); break;
    case 5: jpeg_set_colorspace(&c, JCS_YCCK); break;
    case 6: c.write_JFIF_header = FALSE; c.write_Adobe_marker = TRUE; break;
    }
    for (int i = 0; i < c.num_components; i++) { c.comp_info[i].h_samp_factor = s.samp[i][0]; c.comp_info[i].v_samp_factor = s.samp[i][1]; }
    jpeg_set_quality(&c, s.quality, s.baseline ? TRUE : FALSE);
    c.optimize_coding = s.optimize ? TRUE : FALSE;
    c.restart_interval = s.restart; c.restart_in_rows = s.restartRows;
    static jpeg_scan_info sScans[4];
    if (s.progressive) jpeg_simple_progression(&c);
    else if (s.separate && c.num_components > 1) {
        for (int i = 0; i < c.num_components; i++) {
            sScans[i].comps_in_scan = 1; sScans[i].component_index[0] = i; sScans[i].Ss = 0; sScans[i].Se = 63; sScans[i].Ah = sScans[i].Al = 0;
        }
        c.scan_info = sScans; c.num_scans = c.num_components;
    }
    jpeg_start_compress(&c, TRUE);
    std::vector<unsigned char> row(s.w * comps);
    int pattern = Rand() % 3;
    while (c.next_scanline < c.image_height) {
        int y = c.next_scanline;
        for (int x = 0; x < s.w; x++)
            for (int k = 0; k < comps; k++)
                row[x * comps + k] = pattern == 0 ? (unsigned char)Rand() : pattern == 1 ? (unsigned char)(x * 7 + y * 3 + k * 50)
                                   : (unsigned char)(((x / 4 + y / 4 + k) & 1) ? 230 : 20);
        JSAMPROW r = row.data();
        jpeg_write_scanlines(&c, &r, 1);
    }
    jpeg_finish_compress(&c);
    jpeg_destroy_compress(&c);
    return true;
}

static Settings RandomSettings(bool little)
{
    Settings s;
    s.w = little ? RandIn(1, 24) : (Rand() % 4 == 0 ? RandIn(60, 300) : RandIn(1, 70));
    s.h = little ? RandIn(1, 24) : (Rand() % 4 == 0 ? RandIn(60, 200) : RandIn(1, 70));
    s.kind = RandIn(0, 6);
    s.quality = Rand() % 5 == 0 ? RandIn(1, 25) : RandIn(25, 100);
    s.baseline = Rand() % 3 != 0;
    s.optimize = Rand() % 2 == 0;
    s.progressive = Rand() % 4 == 0;
    s.separate = !s.progressive && Rand() % 4 == 0;
    s.restart = Rand() % 4 == 0 ? RandIn(1, 5) : 0;
    s.restartRows = !s.restart && Rand() % 6 == 0 ? RandIn(1, 2) : 0;
    // sampling: the classic ones mostly, any 1..4 factors (at most 10 blocks in an MCU) sometimes
    static const int classic[][2] = {{1, 1}, {2, 1}, {2, 2}, {1, 2}, {4, 1}, {4, 2}, {1, 4}};
    for (int i = 0; i < 4; i++) { s.samp[i][0] = 1; s.samp[i][1] = 1; }
    if (Rand() % 3 == 0) {
        int b = 0;
        for (int i = 0; i < 4; i++) {
            do { s.samp[i][0] = RandIn(1, 4); s.samp[i][1] = RandIn(1, 4); } while (b + s.samp[i][0] * s.samp[i][1] > 10 - (3 - i));
            b += s.samp[i][0] * s.samp[i][1];
        }
    } else {
        const int* c = classic[Rand() % 7];
        s.samp[0][0] = c[0]; s.samp[0][1] = c[1];
    }
    return s;
}

static void Corruptions(const char* what, const std::vector<unsigned char>& in, int n)
{
    char name[200];
    for (int i = 0; i < n && !in.empty(); i++) {
        std::vector<unsigned char> c = in;
        int kind = Rand() % 6;
        size_t at = Rand() % c.size();
        if (kind == 0) c.resize(at);
        else if (kind == 1) for (int k = RandIn(1, 4); k > 0; k--) c[Rand() % c.size()] ^= (unsigned char)RandIn(1, 255);
        else if (kind == 2) { unsigned char m[2] = {0xff, (unsigned char)RandIn(0xc0, 0xfe)}; c.insert(c.begin() + at, m, m + 2); }
        else if (kind == 3) { unsigned char app[4] = {0xff, 0xe3, 0xff, 0xf0}; c.insert(c.begin() + 2, app, app + 4); c.resize((std::min)(c.size(), (size_t)RandIn(8, 3000))); }
        else if (kind == 4) { size_t from = in.size() / 2 + Rand() % (in.size() / 2 + 1); for (size_t k = from; k + 2 < c.size(); k += RandIn(1, 9)) c[k] = (unsigned char)Rand(); }
        else { unsigned char rst[2] = {0xff, (unsigned char)(0xd0 + RandIn(0, 7))}; c.insert(c.begin() + at, rst, rst + 2); }
        snprintf(name, sizeof name, "%s corrupt %d kind %d at %u", what, i, kind, (unsigned)at);
        Compare(name, c);
    }
}

static void LobbyPictures()
{
    namespace fs = std::filesystem;
    const char* root = getenv("EE_DATA_ROOT");
    fs::path dir = std::string(root ? root : "C:/Users/max/Desktop/empire_earth_next/build/open_data/tree") + "/Data/WONLobby Resources";
    if (!fs::is_directory(dir)) { printf("won_jpeg_test: no lobby pictures at %s\n", dir.string().c_str()); return; }
    int count = 0;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".jpg") continue;
        FILE* f = fopen(e.path().string().c_str(), "rb");
        std::vector<unsigned char> bytes;
        int ch;
        while (f && (ch = fgetc(f)) != EOF) bytes.push_back((unsigned char)ch);
        if (f) fclose(f);
        Compare(e.path().string().c_str(), bytes);
        Corruptions(e.path().string().c_str(), bytes, 20);
        count++;
    }
    printf("won_jpeg_test: %d lobby JPEGs\n", count);
}

int main(int argc, char** argv)
{
    int n = 3000;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--golden") == 0 && i + 1 < argc) sGolden = fopen(argv[++i], "w");
        else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) n = atoi(argv[++i]);
    }
    // the golden's cases first (small ones), then the rest
    char name[200];
    for (int i = 0; i < n; i++) {
        Settings s = RandomSettings(i < 300);
        std::vector<unsigned char> jpg;
        if (!Encode(s, jpg)) continue;
        snprintf(name, sizeof name, "case %d (%dx%d kind %d q%d samp %d%d prog %d sep %d rst %d/%d)", i, s.w, s.h, s.kind,
                 s.quality, s.samp[0][0], s.samp[0][1], s.progressive, s.separate, s.restart, s.restartRows);
        Compare(name, jpg);
        Corruptions(name, jpg, i < 300 ? 1 : 3);
    }
    std::vector<unsigned char> none, junk = {'n', 'o', 't', ' ', 'a', ' ', 'j', 'p', 'g'}, soi = {0xff, 0xd8};
    Compare("empty", none);
    Compare("junk", junk);
    Compare("soi only", soi);
    LobbyPictures();
    if (sGolden) fclose(sGolden);
    printf("won_jpeg_test: %d cases, %d golden, %d failures\n", sCases, sGoldenCount, sFailures);
    return sFailures;
}
