// GEOpenFormats: glTF -> .cem and PNG -> .sst (new unit, not in the original game). The rules are those of
// tools/eemodel (gltf.py import_, sst.py from_png; docs/MODEL_FORMATS.md 4.3 and 5): every float operation is done in
// double precision and rounded to float where the Python side rounds (f32), so both give the same bytes; the C++ gate
// (tools/eemodel/cppgate) checks that over every model and texture of the game. Portable: the standard library,
// cgltf and stb_image only.
// UNIT-FLAGS: -EHsc
// (a conversion problem is thrown as a Failure inside GltfToCem, which returns it as false + message)
#include <math.h>
#include <stdint.h>
#include <string.h>
#include <charconv>
#include "lle/UFromChars.h"
#include <map>
#include <string>
#include <vector>

#include "cgltf/cgltf.h"
#include "stb_image/stb_image.h"

#include "GEOpenFormats.h"

#ifdef __clang__
#pragma clang fp contract(off)   // no fused multiply-add: the products and sums round as the Python side's
#endif

namespace {

// ------------------------------------------------------------------------------------------------- JSON values --
// The EE_model extension and material extras arrive as JSON text (cgltf keeps unknown extensions and extras raw).

struct Failure {
    std::string mMessage;
};

struct JValue {
    enum Kind { kNull, kBool, kInt, kNumber, kString, kArray, kObject } mKind = kNull;
    bool mBool = false;
    bool mIntFits = false;   // a JSON integer token that fits in 0 .. 2^63-1 (mInt)
    long long mInt = 0;
    double mNumber = 0;
    std::string mString;
    std::vector<JValue> mItems;
    std::vector<std::pair<std::string, JValue> > mMembers;

    const JValue* Get(const char* inKey) const
    {
        const JValue* theFound = NULL;   // the last one wins, as in Python's json
        for (size_t i = 0; i < mMembers.size(); i++)
            if (mMembers[i].first == inKey)
                theFound = &mMembers[i].second;
        return theFound;
    }
    bool IsNumber() const { return mKind == kInt || mKind == kNumber; }
    // a JSON integer in 0 .. inLimit-1 (the Python side's _is_int: not a bool, not a float)
    bool IsIndex(unsigned long long inLimit = 4294967296ull) const
    {
        return mKind == kInt && mIntFits && (unsigned long long)mInt < inLimit;
    }
};

class JParser {
public:
    JParser(const char* inText) : mP(inText) {}
    bool Parse(JValue& outValue)
    {
        if (!Value(outValue, 0))
            return false;
        Space();
        return *mP == 0;
    }

private:
    const char* mP;
    void Space()
    {
        while (*mP == ' ' || *mP == '\t' || *mP == '\n' || *mP == '\r')
            mP++;
    }
    bool Literal(const char* inWord)
    {
        size_t n = strlen(inWord);
        if (strncmp(mP, inWord, n) != 0)
            return false;
        mP += n;
        return true;
    }
    static void Utf8(std::string& ioOut, unsigned long inCode)
    {
        if (inCode < 0x80)
            ioOut += (char)inCode;
        else if (inCode < 0x800) {
            ioOut += (char)(0xc0 | (inCode >> 6));
            ioOut += (char)(0x80 | (inCode & 0x3f));
        } else if (inCode < 0x10000) {
            ioOut += (char)(0xe0 | (inCode >> 12));
            ioOut += (char)(0x80 | ((inCode >> 6) & 0x3f));
            ioOut += (char)(0x80 | (inCode & 0x3f));
        } else {
            ioOut += (char)(0xf0 | (inCode >> 18));
            ioOut += (char)(0x80 | ((inCode >> 12) & 0x3f));
            ioOut += (char)(0x80 | ((inCode >> 6) & 0x3f));
            ioOut += (char)(0x80 | (inCode & 0x3f));
        }
    }
    bool Hex4(unsigned long& outCode)
    {
        outCode = 0;
        for (int i = 0; i < 4; i++) {
            char c = *mP++;
            outCode <<= 4;
            if (c >= '0' && c <= '9')
                outCode |= c - '0';
            else if (c >= 'a' && c <= 'f')
                outCode |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                outCode |= c - 'A' + 10;
            else
                return false;
        }
        return true;
    }
    bool String(std::string& outText)
    {
        if (*mP++ != '"')
            return false;
        while (*mP != '"') {
            unsigned char c = (unsigned char)*mP;
            if (c == 0 || c < 0x20)
                return false;
            if (c != '\\') {
                outText += (char)c;
                mP++;
                continue;
            }
            mP++;
            char e = *mP++;
            switch (e) {
            case '"': outText += '"'; break;
            case '\\': outText += '\\'; break;
            case '/': outText += '/'; break;
            case 'b': outText += '\b'; break;
            case 'f': outText += '\f'; break;
            case 'n': outText += '\n'; break;
            case 'r': outText += '\r'; break;
            case 't': outText += '\t'; break;
            case 'u': {
                unsigned long theCode;
                if (!Hex4(theCode))
                    return false;
                if (theCode >= 0xd800 && theCode < 0xdc00 && mP[0] == '\\' && mP[1] == 'u') {
                    const char* theSave = mP;
                    mP += 2;
                    unsigned long theLow;
                    if (Hex4(theLow) && theLow >= 0xdc00 && theLow < 0xe000)
                        theCode = 0x10000 + ((theCode - 0xd800) << 10) + (theLow - 0xdc00);
                    else
                        mP = theSave;
                }
                Utf8(outText, theCode);
                break;
            }
            default:
                return false;
            }
        }
        mP++;
        return true;
    }
    bool Number(JValue& outValue)
    {
        const char* theStart = mP;
        bool theIsInt = true;
        if (*mP == '-')
            mP++;
        if (!(*mP >= '0' && *mP <= '9'))
            return false;
        while (*mP >= '0' && *mP <= '9')
            mP++;
        if (*mP == '.') {
            theIsInt = false;
            mP++;
            if (!(*mP >= '0' && *mP <= '9'))
                return false;
            while (*mP >= '0' && *mP <= '9')
                mP++;
        }
        if (*mP == 'e' || *mP == 'E') {
            theIsInt = false;
            mP++;
            if (*mP == '+' || *mP == '-')
                mP++;
            if (!(*mP >= '0' && *mP <= '9'))
                return false;
            while (*mP >= '0' && *mP <= '9')
                mP++;
        }
        std::from_chars_result r = UFromChars(theStart, mP, outValue.mNumber);   // correctly rounded
        if (r.ec != std::errc() && r.ec != std::errc::result_out_of_range)
            return false;
        outValue.mKind = theIsInt ? JValue::kInt : JValue::kNumber;
        if (theIsInt) {
            long long v;
            std::from_chars_result ri = std::from_chars(theStart, mP, v);
            outValue.mIntFits = ri.ec == std::errc() && v >= 0;
            outValue.mInt = outValue.mIntFits ? v : 0;
        }
        return true;
    }
    bool Value(JValue& outValue, int inDepth)
    {
        if (inDepth > 64)
            return false;
        Space();
        char c = *mP;
        if (c == '{') {
            mP++;
            outValue.mKind = JValue::kObject;
            Space();
            if (*mP == '}') {
                mP++;
                return true;
            }
            for (;;) {
                Space();
                std::pair<std::string, JValue> theMember;
                if (!String(theMember.first))
                    return false;
                Space();
                if (*mP++ != ':')
                    return false;
                if (!Value(theMember.second, inDepth + 1))
                    return false;
                outValue.mMembers.push_back(theMember);
                Space();
                if (*mP == ',') {
                    mP++;
                    continue;
                }
                if (*mP++ != '}')
                    return false;
                return true;
            }
        }
        if (c == '[') {
            mP++;
            outValue.mKind = JValue::kArray;
            Space();
            if (*mP == ']') {
                mP++;
                return true;
            }
            for (;;) {
                outValue.mItems.push_back(JValue());
                if (!Value(outValue.mItems.back(), inDepth + 1))
                    return false;
                Space();
                if (*mP == ',') {
                    mP++;
                    continue;
                }
                if (*mP++ != ']')
                    return false;
                return true;
            }
        }
        if (c == '"') {
            outValue.mKind = JValue::kString;
            return String(outValue.mString);
        }
        if (Literal("true")) {
            outValue.mKind = JValue::kBool;
            outValue.mBool = true;
            return true;
        }
        if (Literal("false")) {
            outValue.mKind = JValue::kBool;
            return true;
        }
        if (Literal("null")) {
            outValue.mKind = JValue::kNull;
            return true;
        }
        return Number(outValue);
    }

public:
    // a JSON string's escapes resolved (cgltf hands strings over raw, escapes included)
    static std::string Unescape(const char* inRaw)
    {
        std::string theQuoted = std::string("\"") + (inRaw ? inRaw : "") + "\"";
        JParser p(theQuoted.c_str());
        std::string theOut;
        if (!p.String(theOut))
            throw Failure{std::string("bad JSON string \"") + (inRaw ? inRaw : "") + "\""};
        return theOut;
    }
};

// ------------------------------------------------------------------------------------------------------ helpers --

// float32 rounding through memory: on the i686 build's x87 a cast alone may stay in a register at double precision
inline float F32(double inValue)
{
    volatile float theValue = (float)inValue;
    return theValue;
}

const double kIdentity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
// the Z-up -> Y-up wrapper node eemodel writes above the model (gltf.py AXES)
const double kAxes[16] = {1, 0, 0, 0, 0, 0, -1, 0, 0, 1, 0, 0, 0, 0, 0, 1};
const double kFramesPerSecond = 10.0;   // gltf.py FPS: frame i is keyframe time i / 10

typedef std::vector<double> Matrix;   // 16, column-major

Matrix MatMul(const Matrix& a, const Matrix& b)
{
    Matrix theOut(16);
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            double s = 0;
            for (int k = 0; k < 4; k++)
                s += a[k * 4 + r] * b[c * 4 + k];
            theOut[c * 4 + r] = s;
        }
    return theOut;
}

// gltf.py compose: (translation, quaternion xyzw, scale) -> column-major matrix of float32 values
Matrix Compose(const double t[3], const double q[4], const double s[3])
{
    double x = q[0], y = q[1], z = q[2], w = q[3];
    double r[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
                      {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
                      {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    Matrix g(16, 0.0);
    for (int c = 0; c < 3; c++)
        for (int i = 0; i < 3; i++)
            g[c * 4 + i] = F32(r[i][c] * s[c]);
    g[12] = F32(t[0]);
    g[13] = F32(t[1]);
    g[14] = F32(t[2]);
    g[15] = 1.0;
    return g;
}

void Trs(const cgltf_node* n, double t[3], double r[4], double s[3])
{
    for (int i = 0; i < 3; i++) {
        t[i] = n->has_translation ? n->translation[i] : 0.0;
        s[i] = n->has_scale ? n->scale[i] : 1.0;
    }
    for (int i = 0; i < 4; i++)
        r[i] = n->has_rotation ? n->rotation[i] : (i == 3 ? 1.0 : 0.0);
}

// gltf.py _local
Matrix Local(const cgltf_node* n)
{
    if (n->has_matrix)
        return Matrix(n->matrix, n->matrix + 16);
    double t[3], r[4], s[3];
    Trs(n, t, r, s);
    if (t[0] == 0 && t[1] == 0 && t[2] == 0 && r[0] == 0 && r[1] == 0 && r[2] == 0 && r[3] == 1 && s[0] == 1 &&
        s[1] == 1 && s[2] == 1)
        return Matrix(kIdentity, kIdentity + 16);
    return Compose(t, r, s);
}

bool IsSignedPermutation(const Matrix& g)
{
    for (int i = 0; i < 16; i++)
        if (!(g[i] == 0.0 || g[i] == 1.0 || g[i] == -1.0))
            return false;
    return g[12] == 0 && g[13] == 0 && g[14] == 0 && g[15] == 1;
}

// CP-1252 for the bytes 0x80..0x9f (eedata's mapping: the five undefined ones are their C1 code points)
const unsigned short kCp1252High[32] = {0x20ac, 0x81, 0x201a, 0x192, 0x201e, 0x2026, 0x2020, 0x2021, 0x2c6, 0x2030,
                                        0x160, 0x2039, 0x152, 0x8d, 0x17d, 0x8f, 0x90, 0x2018, 0x2019, 0x201c,
                                        0x201d, 0x2022, 0x2013, 0x2014, 0x2dc, 0x2122, 0x161, 0x203a, 0x153, 0x9d,
                                        0x17e, 0x178};

// gltf.py _name: Blender's ".NNN" duplicate suffix dropped, UTF-8 -> CP-1252
std::string GameName(const char* inName)
{
    std::string s = JParser::Unescape(inName);
    if (s.size() >= 4 && s[s.size() - 4] == '.' && isdigit((unsigned char)s[s.size() - 3]) &&
        isdigit((unsigned char)s[s.size() - 2]) && isdigit((unsigned char)s[s.size() - 1]))
        s.resize(s.size() - 4);
    std::string theOut;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        unsigned long theCode;
        int n;
        if (c < 0x80) {
            theCode = c;
            n = 1;
        } else if ((c & 0xe0) == 0xc0) {
            theCode = c & 0x1f;
            n = 2;
        } else if ((c & 0xf0) == 0xe0) {
            theCode = c & 0x0f;
            n = 3;
        } else if ((c & 0xf8) == 0xf0) {
            theCode = c & 0x07;
            n = 4;
        } else
            throw Failure{"name \"" + s + "\" is not UTF-8"};
        if (i + n > s.size())
            throw Failure{"name \"" + s + "\" is not UTF-8"};
        for (int k = 1; k < n; k++) {
            if (((unsigned char)s[i + k] & 0xc0) != 0x80)
                throw Failure{"name \"" + s + "\" is not UTF-8"};
            theCode = (theCode << 6) | ((unsigned char)s[i + k] & 0x3f);
        }
        i += n;
        int theByte = -1;
        if (theCode < 0x80 || (theCode >= 0xa0 && theCode <= 0xff))
            theByte = (int)theCode;
        else
            for (int k = 0; k < 32; k++)
                if (kCp1252High[k] == theCode)
                    theByte = 0x80 + k;
        if (theByte < 0)
            throw Failure{"name \"" + s + "\" has characters outside CP-1252"};
        theOut += (char)theByte;
    }
    return theOut;
}

std::vector<float> Floats(const cgltf_accessor* a)
{
    size_t n = cgltf_accessor_unpack_floats(a, NULL, 0);
    std::vector<float> theOut(n);
    if (n && cgltf_accessor_unpack_floats(a, &theOut[0], n) != n)
        throw Failure{"an accessor cannot be read"};
    return theOut;
}

std::vector<unsigned long long> Ints(const cgltf_accessor* a)
{
    std::vector<float> f = Floats(a);   // integers below 2^24 are exact; gltf.py reads them the same way
    std::vector<unsigned long long> theOut(f.size());
    for (size_t i = 0; i < f.size(); i++)
        theOut[i] = (unsigned long long)f[i];
    return theOut;
}

std::vector<unsigned long long> IndexValues(const cgltf_accessor* a)
{
    std::vector<unsigned long long> theOut(a->count);
    for (cgltf_size i = 0; i < a->count; i++)
        theOut[i] = cgltf_accessor_read_index(a, i);
    return theOut;
}

struct Sampler {
    std::vector<float> mTimes;
    std::vector<float> mValues;
    cgltf_interpolation_type mMode;

    // gltf.py _Sampler.at
    std::vector<double> At(double inTime, size_t nc) const
    {
        bool theCubic = mMode == cgltf_interpolation_type_cubic_spline;
        size_t theStep = theCubic ? 3 * nc : nc;
        size_t theOffset = theCubic ? nc : 0;
        const std::vector<float>& t = mTimes;
        if (t.empty() || mValues.size() < t.size() * theStep)
            throw Failure{"animation sampler output shorter than its input"};
        auto Key = [&](size_t i) {
            std::vector<double> k(nc);
            for (size_t c = 0; c < nc; c++)
                k[c] = mValues[i * theStep + theOffset + c];
            return k;
        };
        if (inTime <= t[0] || t.size() == 1)
            return Key(0);
        if (inTime >= t.back())
            return Key(t.size() - 1);
        size_t i = 0;
        while (i + 1 < t.size() && t[i + 1] <= inTime)
            i++;
        if (t[i] == inTime || mMode == cgltf_interpolation_type_step)
            return Key(i);
        std::vector<double> a = Key(i), b = Key(i + 1);
        double u = (inTime - t[i]) / ((double)t[i + 1] - t[i]);
        for (size_t c = 0; c < nc; c++)
            a[c] = a[c] + (b[c] - a[c]) * u;
        return a;
    }
};

typedef std::map<std::pair<const cgltf_node*, int>, Sampler> Channels;

// ------------------------------------------------------------------------------------------------ the cem model --

struct Material {
    std::string mName, mTexture;
    unsigned long mTextureIndex = 0;
    std::vector<std::pair<unsigned long, unsigned long> > mRanges;
    unsigned long mVertexStart = 0, mVertexCount = 0;
};

struct Model {
    std::string mName;
    float mCenter[3];
    unsigned long mPolygonCount = 0;
    std::vector<std::vector<unsigned long> > mLods;
    std::vector<Material> mMaterials;
    std::vector<std::string> mTagNames;
    std::vector<float> mRadii;
    std::vector<std::vector<float> > mVertices, mTags, mTransforms, mBoxes;   // per frame (transform: mMatrix rows)
    std::vector<Model> mChildren;
};

// gltf.py compute_bbox
std::vector<float> BoundingBox(const std::vector<float>& v, size_t vc)
{
    std::vector<float> theBox(6);
    for (int c = 0; c < 3; c++)
        theBox[c] = theBox[c + 3] = v.size() >= 3 ? v[c] : 0.0f;
    for (size_t j = 0; j < vc; j++)
        for (int c = 0; c < 3; c++) {
            float x = v[8 * j + c];
            if (x < theBox[c])
                theBox[c] = x;
            else if (x > theBox[c + 3])
                theBox[c + 3] = x;
        }
    return theBox;
}

const cgltf_accessor* Attribute(const cgltf_attribute* inAttributes, size_t inCount, cgltf_attribute_type inType)
{
    for (size_t i = 0; i < inCount; i++)
        if (inAttributes[i].type == inType && inAttributes[i].index == 0)
            return inAttributes[i].data;
    return NULL;
}

std::vector<double> FloatList(const JValue* v, size_t n, bool& outOk)
{
    std::vector<double> theOut;
    outOk = v && v->mKind == JValue::kArray && v->mItems.size() == n;
    if (outOk)
        for (size_t i = 0; i < n && outOk; i++) {
            outOk = v->mItems[i].IsNumber();
            theOut.push_back(F32(v->mItems[i].mNumber));
        }
    return theOut;
}

bool PairList(const JValue* r, size_t n)
{
    if (!r || r->mKind != JValue::kArray || r->mItems.size() != n)
        return false;
    for (size_t i = 0; i < n; i++) {
        const JValue& p = r->mItems[i];
        if (p.mKind != JValue::kArray || p.mItems.size() != 2 || !p.mItems[0].IsIndex() || !p.mItems[1].IsIndex())
            return false;
    }
    return true;
}

std::string TextureName(const cgltf_material* m)
{
    if (!m || !m->has_pbr_metallic_roughness || !m->pbr_metallic_roughness.base_color_texture.texture)
        return "";
    const cgltf_image* img = m->pbr_metallic_roughness.base_color_texture.texture->image;
    if (!img)
        return "";
    if (img->name && img->name[0])
        return GameName(img->name);
    if (img->uri && img->uri[0] && strncmp(img->uri, "data:", 5) != 0) {
        std::string theRaw = JParser::Unescape(img->uri);
        std::vector<char> theUri(theRaw.c_str(), theRaw.c_str() + theRaw.size() + 1);
        cgltf_decode_uri(&theUri[0]);
        std::string s = &theUri[0];
        size_t theSlash = s.find_last_of("/\\");
        if (theSlash != std::string::npos)
            s = s.substr(theSlash + 1);
        size_t theDot = s.rfind('.');
        if (theDot != std::string::npos)
            s.resize(theDot);
        return GameName(s.c_str());
    }
    return "";
}

class Importer {
public:
    Importer(const cgltf_data* inData) : mData(inData) {}
    const cgltf_data* mData;
    Channels mChannels;

    Model ImportModel(const cgltf_node* inNode, const Matrix* inCorrection);
};

const JValue* ExtensionData(const cgltf_node* n, JValue& outStore)
{
    for (size_t i = 0; i < n->extensions_count; i++)
        if (n->extensions[i].name && strcmp(n->extensions[i].name, "EE_model") == 0 && n->extensions[i].data) {
            JParser p(n->extensions[i].data);
            outStore = JValue();
            if (!p.Parse(outStore))
                throw Failure{"EE_model is not valid JSON"};
            return outStore.mKind == JValue::kObject ? &outStore : NULL;
        }
    return NULL;
}

Model Importer::ImportModel(const cgltf_node* inNode, const Matrix* inCorrection)
{
    JValue theStore;
    const JValue* ext = ExtensionData(inNode, theStore);
    static const JValue kNone;
    auto Ext = [&](const char* k) { return ext ? ext->Get(k) : NULL; };
    Model m;
    m.mName = GameName(inNode->name);
    const cgltf_mesh* mesh = inNode->mesh;
    if (mesh->primitives_count == 0)
        throw Failure{"a mesh without primitives"};
    size_t ntargets = mesh->primitives[0].targets_count;
    size_t fc = ntargets + 1;
    size_t nmat = mesh->primitives_count;
    // per primitive: [frame][8 x n floats] and the indices
    std::vector<size_t> theCounts(nmat);
    std::vector<std::vector<std::vector<float> > > pverts(nmat);
    std::vector<std::vector<unsigned long long> > pidx(nmat);
    for (size_t pi = 0; pi < nmat; pi++) {
        const cgltf_primitive& p = mesh->primitives[pi];
        if (p.type != cgltf_primitive_type_triangles)
            throw Failure{"only triangle lists are supported"};
        if (p.targets_count != ntargets)
            throw Failure{"primitives with different morph target counts"};
        const cgltf_accessor* pa = Attribute(p.attributes, p.attributes_count, cgltf_attribute_type_position);
        if (!pa)
            throw Failure{"a primitive without POSITION"};
        std::vector<float> pos = Floats(pa);
        size_t n = pos.size() / 3;
        const cgltf_accessor* na = Attribute(p.attributes, p.attributes_count, cgltf_attribute_type_normal);
        const cgltf_accessor* ta = Attribute(p.attributes, p.attributes_count, cgltf_attribute_type_texcoord);
        std::vector<float> nor = na ? Floats(na) : std::vector<float>(3 * n, 0.0f);
        std::vector<float> uv = ta ? Floats(ta) : std::vector<float>(2 * n, 0.0f);
        if (nor.size() != 3 * n || uv.size() != 2 * n)
            throw Failure{"attribute counts differ"};
        theCounts[pi] = n;
        pverts[pi].resize(fc);
        for (size_t f = 0; f < fc; f++) {
            std::vector<float> fp = pos, fn = nor;
            if (f > 0) {
                const cgltf_morph_target& t = p.targets[f - 1];
                const cgltf_accessor* dpa = Attribute(t.attributes, t.attributes_count, cgltf_attribute_type_position);
                const cgltf_accessor* dna = Attribute(t.attributes, t.attributes_count, cgltf_attribute_type_normal);
                std::vector<float> dp = dpa ? Floats(dpa) : std::vector<float>(3 * n, 0.0f);
                std::vector<float> dn = dna ? Floats(dna) : std::vector<float>(3 * n, 0.0f);
                if (dp.size() != 3 * n || dn.size() != 3 * n)
                    throw Failure{"morph target counts differ"};
                for (size_t i = 0; i < 3 * n; i++) {
                    fp[i] = F32((double)pos[i] + (double)dp[i]);
                    fn[i] = F32((double)nor[i] + (double)dn[i]);
                }
            }
            std::vector<float>& v = pverts[pi][f];
            v.resize(8 * n);
            for (size_t i = 0; i < n; i++) {
                memcpy(&v[8 * i], &fp[3 * i], 12);
                memcpy(&v[8 * i + 3], &fn[3 * i], 12);
                memcpy(&v[8 * i + 6], &uv[2 * i], 8);
            }
        }
        if (p.indices)
            pidx[pi] = IndexValues(p.indices);
        else
            for (size_t i = 0; i < n; i++)
                pidx[pi].push_back(i);
        if (pidx[pi].size() % 3)
            throw Failure{"an index count that is not a multiple of 3"};
        for (size_t i = 0; i < pidx[pi].size(); i++)
            if (pidx[pi][i] >= n)
                throw Failure{"an index out of range"};
    }
    // vertex layout: EE_model's vertex starts when valid and consistent, else one range after the other
    std::vector<unsigned long> starts;
    std::vector<int> theOwner;   // per vertex: (primitive, vertex) that defined it, -1 none
    std::vector<std::pair<size_t, size_t> > theLayout;
    bool theLaidOut = false;
    const JValue* vs = Ext("vertexStarts");
    if (vs && vs->mKind == JValue::kArray && vs->mItems.size() == nmat) {
        bool ok = true;
        for (size_t i = 0; i < nmat; i++)
            ok = ok && vs->mItems[i].IsIndex();
        if (ok) {
            size_t vc = 0;
            for (size_t i = 0; i < nmat; i++)
                vc = (std::max)(vc, (size_t)vs->mItems[i].mInt + theCounts[i]);
            std::vector<std::pair<size_t, size_t> > theSlots(vc, std::make_pair((size_t)-1, (size_t)0));
            for (size_t pi = 0; pi < nmat && ok; pi++) {
                size_t s = (size_t)vs->mItems[pi].mInt;
                for (size_t i = 0; i < theCounts[pi] && ok; i++) {
                    std::pair<size_t, size_t>& slot = theSlots[s + i];
                    if (slot.first == (size_t)-1)
                        slot = std::make_pair(pi, i);
                    else
                        for (size_t f = 0; f < fc && ok; f++)
                            ok = memcmp(&pverts[slot.first][f][8 * slot.second], &pverts[pi][f][8 * i], 32) == 0;
                }
            }
            for (size_t i = 0; i < vc && ok; i++)
                ok = theSlots[i].first != (size_t)-1;
            if (ok) {
                theLayout = theSlots;
                for (size_t i = 0; i < nmat; i++)
                    starts.push_back((unsigned long)vs->mItems[i].mInt);
                theLaidOut = true;
            }
        }
    }
    if (!theLaidOut)
        for (size_t pi = 0; pi < nmat; pi++) {
            starts.push_back((unsigned long)theLayout.size());
            for (size_t i = 0; i < theCounts[pi]; i++)
                theLayout.push_back(std::make_pair(pi, i));
        }
    size_t vc = theLayout.size();
    m.mVertices.resize(fc);
    for (size_t f = 0; f < fc; f++) {
        m.mVertices[f].resize(8 * vc);
        for (size_t i = 0; i < vc; i++)
            memcpy(&m.mVertices[f][8 * i], &pverts[theLayout[i].first][f][8 * theLayout[i].second], 32);
    }
    // frame fixes
    const JValue* fixes = Ext("frameFixes");
    if (fixes && fixes->mKind == JValue::kArray)
        for (size_t k = 0; k < fixes->mItems.size(); k++) {
            size_t fi = k + 1;
            const JValue& acc = fixes->mItems[k];
            if (acc.mKind == JValue::kNull || fi >= fc)
                continue;
            if (!acc.IsIndex(mData->accessors_count))
                throw Failure{"EE_model frameFixes does not match the vertices"};
            const cgltf_accessor* a = &mData->accessors[acc.mInt];
            if (!a->is_sparse || a->count != 6 * vc || a->type != cgltf_type_scalar ||
                a->component_type != cgltf_component_type_r_32f)
                throw Failure{"EE_model frameFixes does not match the vertices"};
            std::vector<float> theValues = Floats(a);
            const cgltf_accessor_sparse& sp = a->sparse;
            const unsigned char* theBase = (const unsigned char*)sp.indices_buffer_view->buffer->data +
                                           sp.indices_buffer_view->offset + sp.indices_byte_offset;
            size_t theSize = sp.indices_component_type == cgltf_component_type_r_8u ? 1 :
                             sp.indices_component_type == cgltf_component_type_r_16u ? 2 : 4;
            for (size_t j = 0; j < sp.count; j++) {
                unsigned long idx = 0;
                memcpy(&idx, theBase + j * theSize, theSize);   // little-endian
                m.mVertices[fi][8 * (idx / 6) + idx % 6] = theValues[idx];
            }
        }
    // LOD 0
    std::vector<std::pair<unsigned long, unsigned long> > r0;
    const JValue* jr0 = Ext("lod0Ranges");
    bool theR0 = PairList(jr0, nmat);
    for (size_t i = 0; i < nmat && theR0; i++)
        theR0 = (unsigned long long)jr0->mItems[i].mItems[1].mInt == pidx[i].size() / 3;
    if (theR0)
        for (size_t i = 0; i < nmat; i++)
            r0.push_back(std::make_pair((unsigned long)jr0->mItems[i].mItems[0].mInt,
                                        (unsigned long)jr0->mItems[i].mItems[1].mInt));
    else {
        unsigned long cum = 0;
        for (size_t i = 0; i < nmat; i++) {
            r0.push_back(std::make_pair(cum, (unsigned long)(pidx[i].size() / 3)));
            cum += (unsigned long)(pidx[i].size() / 3);
        }
    }
    unsigned long long total0 = 0;
    for (size_t i = 0; i < nmat; i++)
        total0 = (std::max)(total0, (unsigned long long)r0[i].first + r0[i].second);
    if (total0 > 0x10000000)
        throw Failure{"EE_model lod0Ranges too large"};
    m.mLods.push_back(std::vector<unsigned long>((size_t)(3 * total0), 0));
    for (size_t i = 0; i < nmat; i++)
        for (size_t j = 0; j < pidx[i].size(); j++)
            m.mLods[0][3 * r0[i].first + j] = (unsigned long)pidx[i][j];
    // LODs 1..: all of EE_model's or none
    std::vector<std::pair<std::vector<std::pair<unsigned long, unsigned long> >, std::vector<unsigned long> > > extra;
    const JValue* lods = Ext("lods");
    if (lods && lods->mKind == JValue::kArray) {
        bool ok = true;
        for (size_t l = 0; l < lods->mItems.size() && ok; l++) {
            const JValue& lod = lods->mItems[l];
            const JValue* rr = lod.mKind == JValue::kObject ? lod.Get("ranges") : NULL;
            const JValue* ia = lod.mKind == JValue::kObject ? lod.Get("indices") : NULL;
            if (ia && ia->mKind == JValue::kNull)
                ia = NULL;
            ok = PairList(rr, nmat);
            const cgltf_accessor* a = NULL;
            if (ok && ia) {
                ok = ia->IsIndex(mData->accessors_count);
                if (ok) {
                    a = &mData->accessors[ia->mInt];
                    ok = a->type == cgltf_type_scalar && (a->component_type == cgltf_component_type_r_8u ||
                                                          a->component_type == cgltf_component_type_r_16u ||
                                                          a->component_type == cgltf_component_type_r_32u);
                }
            }
            if (!ok)
                break;
            std::vector<unsigned long long> arr = a ? Ints(a) : std::vector<unsigned long long>();
            std::vector<std::pair<unsigned long, unsigned long> > ranges;
            for (size_t i = 0; i < nmat && ok; i++) {
                unsigned long long s = rr->mItems[i].mItems[0].mInt, c = rr->mItems[i].mItems[1].mInt;
                ok = 3 * (s + c) <= arr.size();
                for (unsigned long long j = 3 * s; j < 3 * (s + c) && ok; j++)
                    ok = arr[j] < theCounts[i];
                ranges.push_back(std::make_pair((unsigned long)s, (unsigned long)c));
            }
            if (ok)
                extra.push_back(std::make_pair(ranges, std::vector<unsigned long>(arr.begin(), arr.end())));
        }
        if (!ok)
            extra.clear();
    }
    for (size_t l = 0; l < extra.size(); l++)
        m.mLods.push_back(extra[l].second);
    for (size_t pi = 0; pi < nmat; pi++) {
        const cgltf_material* mat = mesh->primitives[pi].material;
        Material theMaterial;
        if (mat && mat->extras.data) {
            JValue ex;
            JParser p(mat->extras.data);
            if (!p.Parse(ex))
                throw Failure{"material extras are not valid JSON"};
            const JValue* ti = ex.mKind == JValue::kObject ? ex.Get("ee_textureIndex") : NULL;
            if (ti) {
                if (!ti->IsIndex())
                    throw Failure{"ee_textureIndex is not an integer 0..2^32-1"};
                theMaterial.mTextureIndex = (unsigned long)ti->mInt;
            }
        }
        theMaterial.mName = GameName(mat ? mat->name : NULL);
        theMaterial.mTexture = TextureName(mat);
        theMaterial.mRanges.push_back(r0[pi]);
        for (size_t l = 0; l < extra.size(); l++)
            theMaterial.mRanges.push_back(extra[l].first[pi]);
        theMaterial.mVertexStart = starts[pi];
        theMaterial.mVertexCount = (unsigned long)theCounts[pi];
        m.mMaterials.push_back(theMaterial);
    }
    const JValue* pc = Ext("polygonCount");
    m.mPolygonCount = pc && pc->IsIndex() ? (unsigned long)pc->mInt : (unsigned long)total0;
    // children: sub-models (with a mesh) and tag points (without)
    std::vector<const cgltf_node*> subs, tags;
    for (size_t i = 0; i < inNode->children_count; i++)
        (inNode->children[i]->mesh ? subs : tags).push_back(inNode->children[i]);
    std::vector<double> times;
    for (size_t f = 0; f < fc; f++)
        times.push_back(F32((double)f / kFramesPerSecond));
    m.mTags.assign(fc, std::vector<float>());
    for (size_t t = 0; t < tags.size(); t++) {
        m.mTagNames.push_back(GameName(tags[t]->name));
        Matrix theRest = Local(tags[t]);
        Channels::const_iterator smp = mChannels.find(std::make_pair(tags[t], (int)cgltf_animation_path_type_translation));
        for (size_t f = 0; f < fc; f++) {
            std::vector<double> p = smp != mChannels.end() ? smp->second.At(times[f], 3)
                                                            : std::vector<double>(theRest.begin() + 12, theRest.begin() + 15);
            for (int c = 0; c < 3; c++)
                m.mTags[f].push_back(F32(p[c]));
        }
    }
    // frame transforms
    std::vector<Matrix> tf;
    const JValue* jt = Ext("transforms");
    bool theExact = jt && jt->mKind == JValue::kArray && jt->mItems.size() == fc;
    for (size_t f = 0; f < fc && theExact; f++) {
        bool ok;
        tf.push_back(FloatList(&jt->mItems[f], 16, ok));
        theExact = ok;
    }
    if (!theExact) {
        tf.clear();
        Channels::const_iterator ts = mChannels.find(std::make_pair(inNode, (int)cgltf_animation_path_type_translation));
        Channels::const_iterator rs = mChannels.find(std::make_pair(inNode, (int)cgltf_animation_path_type_rotation));
        Channels::const_iterator ss = mChannels.find(std::make_pair(inNode, (int)cgltf_animation_path_type_scale));
        if (ts != mChannels.end() || rs != mChannels.end() || ss != mChannels.end()) {
            double t0[3], r0q[4], s0[3];
            Trs(inNode, t0, r0q, s0);
            for (size_t f = 0; f < fc; f++) {
                std::vector<double> t = ts != mChannels.end() ? ts->second.At(times[f], 3) : std::vector<double>(t0, t0 + 3);
                std::vector<double> r = rs != mChannels.end() ? rs->second.At(times[f], 4) : std::vector<double>(r0q, r0q + 4);
                std::vector<double> s = ss != mChannels.end() ? ss->second.At(times[f], 3) : std::vector<double>(s0, s0 + 3);
                tf.push_back(Compose(&t[0], &r[0], &s[0]));
            }
        } else
            tf.assign(fc, Local(inNode));
    }
    if (inCorrection)
        for (size_t f = 0; f < fc; f++) {
            tf[f] = MatMul(*inCorrection, tf[f]);
            for (int i = 0; i < 16; i++)
                tf[f][i] = F32(tf[f][i]);
        }
    for (size_t f = 0; f < fc; f++) {
        std::vector<float> theRows(16);
        for (int r = 0; r < 4; r++)
            for (int c = 0; c < 4; c++)
                theRows[r * 4 + c] = F32(tf[f][c * 4 + r]);
        m.mTransforms.push_back(theRows);
        m.mBoxes.push_back(BoundingBox(m.mVertices[f], vc));
    }
    bool ok;
    std::vector<double> theCenter = FloatList(Ext("center"), 3, ok);
    if (ok)
        for (int c = 0; c < 3; c++)
            m.mCenter[c] = (float)theCenter[c];
    else {
        // gltf.py compute_center
        float s = F32(1.0 / (double)F32((double)fc * 2.0));
        float acc[3] = {0, 0, 0};
        for (size_t f = 0; f < fc; f++)
            for (int c = 0; c < 3; c++)
                acc[c] = F32((double)acc[c] + (double)F32((double)m.mBoxes[f][c] + (double)m.mBoxes[f][c + 3]));
        for (int c = 0; c < 3; c++)
            m.mCenter[c] = F32((double)s * acc[c]);
    }
    std::vector<double> theRadii = FloatList(Ext("radii"), fc, ok);
    if (ok)
        for (size_t f = 0; f < fc; f++)
            m.mRadii.push_back((float)theRadii[f]);
    else
        for (size_t f = 0; f < fc; f++) {
            // gltf.py compute_radii
            double theBest = 0.0;
            const std::vector<float>& v = m.mVertices[f];
            for (size_t j = 0; j < vc; j++) {
                double d[3];
                for (int c = 0; c < 3; c++) {
                    double e = F32((double)v[8 * j + c] - m.mCenter[c]);
                    d[c] = F32(e * e);
                }
                double theDistance = F32((double)F32(d[0] + d[1]) + d[2]);
                if (theDistance > theBest)
                    theBest = theDistance;
            }
            m.mRadii.push_back(F32(sqrt(theBest)));
        }
    for (size_t i = 0; i < subs.size(); i++)
        m.mChildren.push_back(ImportModel(subs[i], NULL));
    return m;
}

void PutU32(std::vector<unsigned char>& out, unsigned long v)
{
    for (int i = 0; i < 4; i++)
        out.push_back((unsigned char)(v >> (8 * i)));
}

void PutFloats(std::vector<unsigned char>& out, const float* v, size_t n)
{
    if (n) {
        size_t at = out.size();
        out.resize(at + 4 * n);
        memcpy(&out[at], v, 4 * n);   // little-endian targets (x86, x86_64, arm64)
    }
}

void PutString(std::vector<unsigned char>& out, const std::string& s)
{
    std::string c = s.substr(0, strlen(s.c_str()));   // UString::Save: up to the NUL
    PutU32(out, (unsigned long)c.size() + 1);
    out.insert(out.end(), c.begin(), c.end());
    out.push_back(0);
}

// cem.py write (exact=False): GEModel::Save's layout, version 2.0
void WriteModel(std::vector<unsigned char>& out, const Model& m)
{
    const char* theTag = "SSMF";
    out.insert(out.end(), theTag, theTag + 4);
    out.push_back(2);
    out.push_back(0);
    out.push_back(0);
    out.push_back(0);
    size_t fc = m.mVertices.size();
    PutU32(out, m.mPolygonCount);
    PutU32(out, (unsigned long)(m.mVertices[0].size() / 8));
    PutU32(out, (unsigned long)m.mTagNames.size());
    PutU32(out, (unsigned long)m.mMaterials.size());
    PutU32(out, (unsigned long)fc);
    PutU32(out, (unsigned long)m.mChildren.size());
    PutU32(out, (unsigned long)m.mLods.size());
    PutString(out, m.mName);
    PutFloats(out, m.mCenter, 3);
    for (size_t l = 0; l < m.mLods.size(); l++) {
        PutU32(out, (unsigned long)(m.mLods[l].size() / 3));
        for (size_t i = 0; i < m.mLods[l].size(); i++)
            PutU32(out, m.mLods[l][i]);
    }
    for (size_t i = 0; i < m.mMaterials.size(); i++) {
        const Material& mat = m.mMaterials[i];
        PutString(out, mat.mName);
        PutU32(out, mat.mTextureIndex);
        for (size_t l = 0; l < mat.mRanges.size(); l++) {
            PutU32(out, mat.mRanges[l].first);
            PutU32(out, mat.mRanges[l].second);
        }
        PutU32(out, mat.mVertexStart);
        PutU32(out, mat.mVertexCount);
        PutString(out, mat.mTexture);
    }
    for (size_t i = 0; i < m.mTagNames.size(); i++)
        PutString(out, m.mTagNames[i]);
    for (size_t f = 0; f < fc; f++) {
        PutFloats(out, &m.mRadii[f], 1);
        PutFloats(out, m.mVertices[f].data(), m.mVertices[f].size());
        PutFloats(out, m.mTags[f].data(), m.mTags[f].size());
        PutFloats(out, m.mTransforms[f].data(), 16);
        PutFloats(out, m.mBoxes[f].data(), 6);
    }
    for (size_t i = 0; i < m.mChildren.size(); i++)
        WriteModel(out, m.mChildren[i]);
}

}  // namespace

namespace GEOpenFormats {

bool GltfToCem(const unsigned char* inData, unsigned long inLength, ResolveFunc inResolve, void* inUser,
               std::vector<unsigned char>& outCem, std::string& outError)
{
    cgltf_options theOptions;
    memset(&theOptions, 0, sizeof(theOptions));
    cgltf_data* theData = NULL;
    if (cgltf_parse(&theOptions, inData, inLength, &theData) != cgltf_result_success) {
        outError = "not a glTF 2.0 file";
        return false;
    }
    std::vector<std::vector<unsigned char> > theExternal(theData->buffers_count);
    bool ok = true;
    for (size_t i = 0; i < theData->buffers_count && ok; i++) {
        cgltf_buffer& b = theData->buffers[i];
        if (b.data || !b.uri || strncmp(b.uri, "data:", 5) == 0)
            continue;
        std::vector<char> theUri(b.uri, b.uri + strlen(b.uri) + 1);
        cgltf_decode_uri(&theUri[0]);
        if (!inResolve || !inResolve(inUser, &theUri[0], theExternal[i])) {
            outError = std::string("buffer \"") + &theUri[0] + "\" not found";
            ok = false;
        } else if (theExternal[i].size() < b.size) {
            outError = std::string("buffer \"") + &theUri[0] + "\" is shorter than its byteLength";
            ok = false;
        } else {
            b.data = theExternal[i].empty() ? NULL : &theExternal[i][0];
            b.data_free_method = cgltf_data_free_method_none;
        }
    }
    if (ok && cgltf_load_buffers(&theOptions, theData, NULL) != cgltf_result_success) {
        outError = "the glTF's buffers cannot be loaded";
        ok = false;
    }
    if (ok && cgltf_validate(theData) != cgltf_result_success) {
        outError = "the glTF is not valid (cgltf_validate)";
        ok = false;
    }
    if (ok) {
        try {
            Importer theImporter(theData);
            // the root model: the first node with a mesh, depth first from the scene's roots
            std::vector<const cgltf_node*> theStack;
            const cgltf_scene* theScene = theData->scene ? theData->scene :
                                          theData->scenes_count ? &theData->scenes[0] : NULL;
            if (theScene)
                for (size_t i = theScene->nodes_count; i-- > 0;)
                    theStack.push_back(theScene->nodes[i]);
            else
                for (size_t i = theData->nodes_count; i-- > 0;)
                    if (!theData->nodes[i].parent)
                        theStack.push_back(&theData->nodes[i]);
            const cgltf_node* theRoot = NULL;
            while (!theStack.empty()) {
                const cgltf_node* n = theStack.back();
                theStack.pop_back();
                if (n->mesh) {
                    theRoot = n;
                    break;
                }
                for (size_t i = n->children_count; i-- > 0;)
                    theStack.push_back(n->children[i]);
            }
            if (!theRoot)
                throw Failure{"no node with a mesh"};
            Matrix theAbove(kIdentity, kIdentity + 16);
            for (const cgltf_node* k = theRoot->parent; k; k = k->parent)
                theAbove = MatMul(Local(k), theAbove);
            Matrix theCorrection;
            bool theCorrect = theAbove != Matrix(kAxes, kAxes + 16);
            if (theCorrect) {
                Matrix theInverse(16);
                for (int r = 0; r < 4; r++)
                    for (int c = 0; c < 4; c++)
                        theInverse[r * 4 + c] = kAxes[c * 4 + r];
                theCorrection = MatMul(theInverse, theAbove);
                if (!IsSignedPermutation(theCorrection))
                    throw Failure{"the transform above the root model is not an axis change"};
            }
            if (theData->animations_count) {
                const cgltf_animation& an = theData->animations[0];
                for (size_t i = 0; i < an.channels_count; i++) {
                    const cgltf_animation_channel& ch = an.channels[i];
                    if (!ch.target_node || !ch.sampler)
                        continue;
                    Sampler s;
                    s.mTimes = Floats(ch.sampler->input);
                    s.mValues = Floats(ch.sampler->output);
                    s.mMode = ch.sampler->interpolation;
                    theImporter.mChannels[std::make_pair((const cgltf_node*)ch.target_node, (int)ch.target_path)] = s;
                }
            }
            Model theModel = theImporter.ImportModel(theRoot, theCorrect ? &theCorrection : NULL);
            outCem.clear();
            WriteModel(outCem, theModel);
        } catch (const Failure& e) {
            outError = e.mMessage;
            ok = false;
        }
    }
    cgltf_free(theData);
    return ok;
}

// ------------------------------------------------------------------------------------------------------- PNG --

namespace {

unsigned long BigU32(const unsigned char* p)
{
    return ((unsigned long)p[0] << 24) | ((unsigned long)p[1] << 16) | ((unsigned long)p[2] << 8) | p[3];
}

// The header facts stb_image does not report: the colour type, whether a tRNS chunk exists, the "ee:surface" text.
bool ScanPng(const std::vector<unsigned char>& d, int& outColorType, bool& outTrns, std::string& outSurface)
{
    static const unsigned char kSignature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    if (d.size() < 33 || memcmp(&d[0], kSignature, 8) != 0 || memcmp(&d[12], "IHDR", 4) != 0)
        return false;
    outColorType = d[25];
    outTrns = false;
    outSurface.clear();
    size_t p = 8;
    while (p + 8 <= d.size()) {
        unsigned long n = BigU32(&d[p]);
        if (n > d.size() - p - 8)
            break;
        const unsigned char* theKind = &d[p + 4];
        if (memcmp(theKind, "tRNS", 4) == 0)
            outTrns = true;
        else if (memcmp(theKind, "tEXt", 4) == 0) {
            std::string theBody((const char*)&d[p + 8], n);
            size_t z = theBody.find('\0');
            if (z != std::string::npos && theBody.substr(0, z) == "ee:surface")
                outSurface = theBody.substr(z + 1);   // Latin-1 bytes; only ASCII digits are accepted below
        } else if (memcmp(theKind, "IEND", 4) == 0)
            break;
        p += 12 + n;
    }
    return true;
}

bool ParseSize(const std::string& s, unsigned long& out)
{
    if (s.empty())
        return false;
    unsigned long long v = 0;
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] < '0' || s[i] > '9')
            return false;
        v = v * 10 + (s[i] - '0');
        if (v >= 4294967296ull)
            return false;
    }
    out = (unsigned long)v;
    return true;
}

}  // namespace

bool PngToSst(const std::vector<std::vector<unsigned char> >& inFrames, std::vector<unsigned char>& outSst,
              std::string& outError)
{
    if (inFrames.empty()) {
        outError = "no frames";
        return false;
    }
    std::vector<unsigned char> theFrames;
    unsigned long theWidth = 0, theHeight = 0;
    for (size_t f = 0; f < inFrames.size(); f++) {
        const std::vector<unsigned char>& d = inFrames[f];
        int theType;
        bool theTrns;
        std::string theSurface;
        if (!ScanPng(d, theType, theTrns, theSurface)) {
            outError = "not a PNG file";
            return false;
        }
        int ch = (theType == 4 || theType == 6 || theTrns) ? 4 : 3;
        int w = 0, h = 0, n = 0;
        unsigned char* px = stbi_load_from_memory(&d[0], (int)d.size(), &w, &h, &n, ch);
        if (!px) {
            outError = std::string("PNG cannot be decoded: ") + stbi_failure_reason();
            return false;
        }
        if (w > 65535 || h > 65535) {
            stbi_image_free(px);
            outError = "PNG larger than 65535 pixels (a Targa's limit)";
            return false;
        }
        if (f == 0) {
            theWidth = (unsigned long)w;
            theHeight = (unsigned long)h;
            if (!theSurface.empty()) {
                size_t x = theSurface.find('x');
                if (x == std::string::npos || !ParseSize(theSurface.substr(0, x), theWidth) ||
                    !ParseSize(theSurface.substr(x + 1), theHeight)) {
                    stbi_image_free(px);
                    outError = "bad ee:surface \"" + theSurface + "\"";
                    return false;
                }
            }
        }
        // sst.py _tga_bytes: type 0, an 18-byte Targa header, bottom-up BGR(A) rows
        theFrames.push_back(0);
        unsigned char theHeader[18] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, (unsigned char)w, (unsigned char)(w >> 8),
                                       (unsigned char)h, (unsigned char)(h >> 8), (unsigned char)(8 * ch),
                                       (unsigned char)(ch == 4 ? 8 : 0)};
        theFrames.insert(theFrames.end(), theHeader, theHeader + 18);
        for (int y = h - 1; y >= 0; y--) {
            const unsigned char* row = px + (size_t)y * w * ch;
            for (int x = 0; x < w; x++) {
                const unsigned char* p = row + (size_t)x * ch;
                theFrames.push_back(p[2]);
                theFrames.push_back(p[1]);
                theFrames.push_back(p[0]);
                if (ch == 4)
                    theFrames.push_back(p[3]);
            }
        }
        stbi_image_free(px);
    }
    outSst.clear();
    outSst.push_back(0);   // not compressed (unread)
    outSst.push_back(1);   // one mip level (unread)
    PutU32(outSst, (unsigned long)inFrames.size());
    PutU32(outSst, theWidth);
    PutU32(outSst, theHeight);
    outSst.insert(outSst.end(), theFrames.begin(), theFrames.end());
    return true;
}

}  // namespace GEOpenFormats
