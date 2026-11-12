// WONGUI's image decoders (PNGDecoder, JPGDecoder; WONGUI/PNGDecoder.cpp and JPGDecoder.cpp, the real units): every
// picture the lobby reads (the open tree's "Data/WONLobby Resources", when it is there: EE_DATA_ROOT or the main
// worktree's build/open_data/tree) and PNGs the driver writes itself in every colour type and bit depth (palette,
// gray 1/2/4/8/16, gray-alpha 8/16, RGB 8/16, RGBA 8/16, a tRNS chunk on palette, gray and RGB, an interlaced one, a
// truncated one, not a PNG at all). One line per picture: the attributes call's answer and size, the raw decode's
// size, transparency and a hash of its ARGB pixels. Golden: tests/won/won_image_golden.txt, recorded over the
// original libpng 1.0 decoder (round 18) and compared line by line; a missing data folder skips its lines.
// Prints one line per failed check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
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
using namespace WONAPI;

// what Image.cpp and ImageDecoder.cpp reference beyond the decoders (the display, the platform images): never
// called here
#include "ee/wonapi/WONLobbyPlatform.h"
namespace WONAPI {
static void NotHere() { printf("won_image_test: a display function was called\n"); exit(99); }
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

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

// the decoders' protected steps, as ImageDecoder's GetImageAttributes / DecodeRaw call them
struct TestPNG : public PNGDecoder {
    bool Attrib(FileReader& r, ImageAttributes& a) { mFileReader = &r; try { return DoGetAttrib(a); } catch (FileReaderException&) { return false; } }
    SmartPtr<RawImage> Raw(FileReader& r) { mFileReader = &r; return DoDecodeRaw(); }
};
struct TestJPG : public JPGDecoder {
    bool Attrib(FileReader& r, ImageAttributes& a) { mFileReader = &r; try { return DoGetAttrib(a); } catch (FileReaderException&) { return false; } }
    SmartPtr<RawImage> Raw(FileReader& r) { mFileReader = &r; return DoDecodeRaw(); }
};

static std::string Describe(const std::string& inName, const std::vector<unsigned char>& inBytes, bool inJpeg)
{
    char line[512];
    ImageAttributes attr;
    bool ok;
    SmartPtr<RawImage> raw;
    {
        FileReader r(inBytes.empty() ? "" : (const void*)inBytes.data(), (unsigned long)inBytes.size());
        ok = inJpeg ? TestJPG().Attrib(r, attr) : TestPNG().Attrib(r, attr);
    }
    {
        FileReader r(inBytes.empty() ? "" : (const void*)inBytes.data(), (unsigned long)inBytes.size());
        raw = inJpeg ? TestJPG().Raw(r) : TestPNG().Raw(r);
    }
    int n = snprintf(line, sizeof line, "%s attr=%d %dx%d", inName.c_str(), ok ? 1 : 0, ok ? attr.mWidth : 0,
                     ok ? attr.mHeight : 0);
    if (raw.get() == NULL) {
        snprintf(line + n, sizeof line - n, " raw=NULL");
        return line;
    }
    RawImage32* img = static_cast<RawImage32*>(raw.get());
    unsigned long long h = 1469598103934665603ull;
    const DWORD* px = img->GetImageData();
    size_t count = (size_t)img->GetWidth() * img->GetHeight();
    for (size_t i = 0; i < count; i++)
        for (int b = 0; b < 32; b += 8) { h ^= (px[i] >> b) & 0xff; h *= 1099511628211ull; }
    snprintf(line + n, sizeof line - n, " raw=%dx%d trans=%d first=%08lx last=%08lx fnv=%016llx", img->GetWidth(),
             img->GetHeight(), img->HasTransparency() ? 1 : 0, count ? (unsigned long)px[0] : 0ul,
             count ? (unsigned long)px[count - 1] : 0ul, h);
    return line;
}

// ---- a small PNG writer: stored zlib blocks, filter 0 (and 1 on odd rows), the CRC and Adler sums

static unsigned Crc32(const unsigned char* p, size_t n)
{
    unsigned c = 0xffffffffu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
    }
    return ~c;
}
static void Be32(std::vector<unsigned char>& o, unsigned v)
{
    o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v);
}
static void Chunk(std::vector<unsigned char>& o, const char* kind, const std::vector<unsigned char>& d)
{
    Be32(o, (unsigned)d.size());
    std::vector<unsigned char> c(kind, kind + 4);
    c.insert(c.end(), d.begin(), d.end());
    o.insert(o.end(), c.begin(), c.end());
    Be32(o, Crc32(c.data(), c.size()));
}
static std::vector<unsigned char> Zlib(const std::vector<unsigned char>& raw)
{
    std::vector<unsigned char> o;
    o.push_back(0x78); o.push_back(0x01);
    size_t at = 0;
    do {
        size_t len = std::min<size_t>(raw.size() - at, 65535);
        o.push_back(at + len == raw.size() ? 1 : 0);
        o.push_back(len & 0xff); o.push_back(len >> 8); o.push_back(~len & 0xff); o.push_back((~len >> 8) & 0xff);
        o.insert(o.end(), raw.begin() + at, raw.begin() + at + len);
        at += len;
    } while (at < raw.size());
    unsigned a = 1, b = 0;
    for (unsigned char v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    Be32(o, (b << 16) | a);
    return o;
}
static int Channels(int inColor) { return inColor == 0 ? 1 : inColor == 2 ? 3 : inColor == 3 ? 1 : inColor == 4 ? 2 : 4; }

// a w x h picture of colour type `color` and bit depth `depth`, its samples a pattern of x, y and the channel;
// `trns`: a tRNS chunk; `interlace`: the Adam7 flag (the rows are then written per pass)
static std::vector<unsigned char> MakePng(int w, int h, int color, int depth, bool trns, bool interlace)
{
    std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<unsigned char> ihdr;
    Be32(ihdr, w); Be32(ihdr, h);
    ihdr.push_back(depth); ihdr.push_back(color); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(interlace ? 1 : 0);
    Chunk(png, "IHDR", ihdr);
    int maxv = (1 << depth) - 1;
    if (color == 3) {
        std::vector<unsigned char> plte;
        for (int i = 0; i <= maxv; i++) { plte.push_back(i * 37); plte.push_back(255 - i * 11); plte.push_back(i * 5 + 9); }
        Chunk(png, "PLTE", plte);
        if (trns) {
            std::vector<unsigned char> t;
            for (int i = 0; i <= maxv && i < 3; i++) t.push_back(i * 100);
            Chunk(png, "tRNS", t);
        }
    } else if (trns && color == 0) {
        std::vector<unsigned char> t = {0, (unsigned char)(1 & maxv)};
        Chunk(png, "tRNS", t);
    } else if (trns && color == 2) {
        std::vector<unsigned char> t = {0, (unsigned char)(depth == 16 ? 0 : 3), 0, 5, 0, 7};
        Chunk(png, "tRNS", t);
    }
    int ch = Channels(color);
    std::vector<unsigned char> raw;
    // the passes' origins and steps (one pass of the whole picture when not interlaced)
    static const int kAdam[7][4] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
    int passes = interlace ? 7 : 1;
    for (int p = 0; p < passes; p++) {
        int x0 = interlace ? kAdam[p][0] : 0, y0 = interlace ? kAdam[p][1] : 0;
        int dx = interlace ? kAdam[p][2] : 1, dy = interlace ? kAdam[p][3] : 1;
        if (x0 >= w || y0 >= h) continue;
        for (int y = y0; y < h; y += dy) {
            raw.push_back(0);
            std::vector<int> samples;
            for (int x = x0; x < w; x += dx)
                for (int c = 0; c < ch; c++) samples.push_back((x * 7 + y * 13 + c * 29 + p) & maxv);
            if (depth == 16) {
                for (int s : samples) { int v = (s * 257 + 0x1234) & 0xffff; raw.push_back(v >> 8); raw.push_back(v & 0xff); }
            } else if (depth == 8) {
                for (int s : samples) raw.push_back(s);
            } else {
                int acc = 0, bits = 0;
                for (int s : samples) {
                    acc = (acc << depth) | s; bits += depth;
                    if (bits == 8) { raw.push_back(acc); acc = 0; bits = 0; }
                }
                if (bits) raw.push_back(acc << (8 - bits));
            }
        }
    }
    Chunk(png, "IDAT", Zlib(raw));
    Chunk(png, "IEND", std::vector<unsigned char>());
    return png;
}

static std::vector<std::string> sLines;

static void Synthetic()
{
    struct Case { int color, depth; bool trns, interlace; };
    static const Case kCases[] = {
        {0, 1, false, false}, {0, 2, false, false}, {0, 4, false, false}, {0, 8, false, false}, {0, 16, false, false},
        {0, 8, true, false}, {0, 2, true, false}, {0, 16, true, false},
        {2, 8, false, false}, {2, 16, false, false}, {2, 8, true, false}, {2, 16, true, false},
        {3, 1, false, false}, {3, 2, false, false}, {3, 4, false, false}, {3, 8, false, false}, {3, 8, true, false},
        {3, 4, true, false},
        {4, 8, false, false}, {4, 16, false, false}, {6, 8, false, false}, {6, 16, false, false},
        {6, 8, false, true}, {3, 4, false, true}, {0, 1, false, true},
    };
    const int sizes[][2] = {{1, 1}, {5, 3}, {17, 9}};
    for (const Case& c : kCases)
        for (const auto& s : sizes) {
            char name[96];
            snprintf(name, sizeof name, "synthetic/c%d-d%d%s%s-%dx%d", c.color, c.depth, c.trns ? "-trns" : "",
                     c.interlace ? "-adam7" : "", s[0], s[1]);
            sLines.push_back(Describe(name, MakePng(s[0], s[1], c.color, c.depth, c.trns, c.interlace), false));
        }
    std::vector<unsigned char> whole = MakePng(17, 9, 6, 8, false, false);
    std::vector<unsigned char> cut(whole.begin(), whole.begin() + whole.size() / 2);
    sLines.push_back(Describe("synthetic/truncated", cut, false));
    std::vector<unsigned char> header(whole.begin(), whole.begin() + 33);
    sLines.push_back(Describe("synthetic/header-only", header, false));
    std::vector<unsigned char> junk = {'n', 'o', 't', ' ', 'a', ' ', 'p', 'n', 'g'};
    sLines.push_back(Describe("synthetic/not-png", junk, false));
    sLines.push_back(Describe("synthetic/not-jpeg", junk, true));
}

static std::string DataFolder()
{
    const char* root = getenv("EE_DATA_ROOT");
    std::string base = root ? root : "C:/Users/max/Desktop/empire_earth_next/build/open_data/tree";
    return base + "/Data/WONLobby Resources";
}

static void LobbyPictures()
{
    namespace fs = std::filesystem;
    fs::path dir = DataFolder();
    if (!fs::is_directory(dir)) return;
    std::vector<std::string> names;
    for (const auto& e : fs::recursive_directory_iterator(dir)) {
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".png" || ext == ".jpg")
            names.push_back(fs::relative(e.path(), dir).generic_string());
    }
    std::sort(names.begin(), names.end());
    for (const std::string& n : names) {
        FILE* f = fopen((dir / n).string().c_str(), "rb");
        std::vector<unsigned char> bytes;
        if (f) {
            int c;
            while ((c = fgetc(f)) != EOF) bytes.push_back((unsigned char)c);
            fclose(f);
        }
        bool jpeg = n.size() > 4 && _stricmp(n.c_str() + n.size() - 4, ".jpg") == 0;
        sLines.push_back(Describe("lobby/" + n, bytes, jpeg));
    }
}

int main(int argc, char** argv)
{
    Synthetic();
    LobbyPictures();
    if (argc > 1 && strcmp(argv[1], "--print") == 0) {
        for (const std::string& l : sLines) printf("%s\n", l.c_str());
        return 0;
    }
    std::string golden = std::filesystem::path(__FILE__).parent_path().string() + "/won_image_golden.txt";
    FILE* g = fopen(golden.c_str(), "r");
    CHECK(g != NULL);
    std::map<std::string, std::string> want;
    if (g) {
        char buf[1024];
        while (fgets(buf, sizeof buf, g)) {
            std::string l(buf);
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            want[l.substr(0, l.find(" attr="))] = l;
        }
        fclose(g);
    }
    int compared = 0, lobby = 0;
    for (const std::string& l : sLines) {
        std::string key = l.substr(0, l.find(" attr="));
        std::map<std::string, std::string>::iterator i = want.find(key);
        CHECK(i != want.end());
        if (i == want.end()) { printf("  new: %s\n", l.c_str()); continue; }
        if (i->second != l) printf("  want %s\n  got  %s\n", i->second.c_str(), l.c_str());
        CHECK(i->second == l);
        compared++;
        if (key.compare(0, 6, "lobby/") == 0) lobby++;
    }
    printf("won_image_test: %d pictures compared (%d of the lobby's), %d failures\n", compared, lobby, sFailures);
    return sFailures;
}
