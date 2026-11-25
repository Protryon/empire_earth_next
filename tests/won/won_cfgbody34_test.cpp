// The config subclasses' bodies in Rust (R6 WON round 34: ee_won_gui cfgbody.rs over the one-statement ops of
// include/ee/wonapi/WONCfgBody.inl): ButtonConfig's FITTEXT, ContainerConfig's PERCENTLAYOUT and COLORSCHEME groups
// and its base instructions, PercentLayoutConfig's and ColorSchemeConfig's PreParse, FontConfig's GetFont, SetFont
// and GetNamedValue, ImageConfig's GetNamedValue and ImageFilterConfig's GetImage, against the bodies as they were
// (won_cfgbody34_orig.h) over one stand-in world (won_cfgbody34_world.h). The lobby's files reach these with their
// usual values; the branches they never take (a failed parse, an exception in a read, a font without a font, a
// parent chain without a scheme) are this golden's.
//
// Every run draws a body and a script (the texts and the ends of the value, the reads and commas that throw, the
// child parse's answer and what it sets, the base's answer, the look-up, the parents and their schemes, the font
// state, the names); the two traces (every call, every object made and deleted, the texts held), the answer or
// exception and the state after must be identical. Then each op of the host in turn is mutated (its statement
// skipped, its answer changed, or a holder made twice): every mutation must make a mismatch. Exit code: the
// mismatches plus the mutations not caught.
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>
#include <exception>
#include "rust/won_gui/guiconf.h"
#include "rust/won_gui/cfgbody.h"
#include "rust/won_gui/cfgkind.h" // the configs' kinds (WONCfgBody.inl makes them by kind)

// what the bodies meet (both sides alike)
struct BodyScript {
    std::vector<std::string> texts; int textThrowAt; int commaThrowAt; int setTextThrows; std::vector<int> eos;
    int parse;            // the child parse: 0 false, 1 true, 2 throws ConfigObjectException, 3 OtherException
    int refs[4]; int params[8]; int colors[4];
    int base;             // the base HandleInstruction: 0 false, 1 true, 2 throws
    int found;            // Fn386860 finds the component
    int parents; int parentComp[3]; int parentScheme[3]; int wmColor;
    int fontThrows;
    int width, height;
};

namespace Orig {
#include "won_cfgbody34_world.h"
#include "won_cfgbody34_orig.h"
}
namespace Port {
#include "won_cfgbody34_world.h"
intptr_t CfgBodyCall(ComponentConfig* inConfig, EeWonCfgBody inBody, intptr_t a = 0, intptr_t b = 0, intptr_t c = 0);
#include "ee/wonapi/WONCfgBody.inl"
// the game's forwards (ConfigTableHost.cpp, ContainerConfigGroups.cpp, ComponentLayoutConfigs.cpp, FontConfig.cpp,
// ImageConfig.cpp)
void ComponentConfig::HandleFitText() { CfgBodyCall(this, EeWonCfgBody::FitText); }
bool ContainerConfig::HandleInstruction(ConfigParser* p, const std::string& t) { return CfgBodyCall(this, EeWonCfgBody::ContainerInstruction, (intptr_t)p, (intptr_t)t.data(), (intptr_t)t.size()) != 0; }
bool PercentLayoutConfig::PreParse(ConfigParser* p) { return CfgBodyCall(this, EeWonCfgBody::PercentPreParse, (intptr_t)p) != 0; }
bool ColorSchemeConfig::PreParse(ConfigParser* p) { return CfgBodyCall(this, EeWonCfgBody::SchemePreParse, (intptr_t)p) != 0; }
Font* FontConfig::GetFont() { return (Font*)CfgBodyCall(this, EeWonCfgBody::FontGet); }
void FontConfig::SetFont(Font* f) { CfgBodyCall(this, EeWonCfgBody::FontSet, (intptr_t)f); }
bool FontConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::FontNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }
bool ImageConfig::GetNamedValue(const std::string& n, int& v) { return CfgBodyCall(this, EeWonCfgBody::ImageNamed, (intptr_t)n.data(), (intptr_t)n.size(), (intptr_t)&v) != 0; }
Image* ImageFilterConfig::GetImage() { return (Image*)CfgBodyCall(this, EeWonCfgBody::FilterImage); }
// the mutation pass: op sMutate changed
static int sMutate = -1;
static intptr_t Mutated(void* f, EeWonCfgBodyOp op, void* c, intptr_t x, intptr_t y, intptr_t z, bool* ok)
{
    if ((int)op != sMutate) return CfgBodyHost(f, op, c, x, y, z, ok);
    typedef EeWonCfgBodyOp O;
    switch (op) {
    case O::BtnTextHold: case O::PercentNew: case O::SchemeNew: CfgBodyHost(f, op, c, x, y, z, ok); return CfgBodyHost(f, op, c, x, y, z, ok);
    case O::ReadTextHold: { intptr_t r = CfgBodyHost(f, op, c, x, y, z, ok); if (*ok) ((GUIString*)r)->s += "!"; return r; }
    case O::BtnWidth: case O::BtnHeight: case O::DescSize: case O::FontHeight: case O::ImgWidth: case O::ImgHeight: return CfgBodyHost(f, op, c, x, y, z, ok) + 1;
    case O::EndOfString: case O::PercentParse: case O::SchemeParse: case O::BaseInstruction: case O::NeedRecalc: case O::FontStale:
    case O::FilterIsRaw: return !CfgBodyHost(f, op, c, x, y, z, ok);
    case O::PercentRef: case O::CfgParent: case O::CfgComponent: case O::OwnScheme: case O::FontPtr: case O::ImagePtr: case O::PercentFind:
        CfgBodyHost(f, op, c, x, y, z, ok); return 0;
    case O::DefaultScheme: return (intptr_t)new ColorScheme(77); // another scheme
    default: return 0; // the statement skipped
    }
}
intptr_t MutatedCall(ComponentConfig* k, EeWonCfgBody b, intptr_t x, intptr_t y, intptr_t z)
{
    CfgBodyFrame aFrame;
    intptr_t aAnswer = 0;
    if (ee_won_cfg_body(Mutated, &aFrame, k, b, x, y, z, &aAnswer) == 2) std::rethrow_exception(aFrame.mErr);
    return aAnswer;
}
}

static unsigned sSeed = 20261124;
static unsigned Rnd(unsigned n) { sSeed = sSeed * 1103515245u + 12345u; return n ? (sSeed >> 8) % n : 0; }
struct Run { int body; BodyScript s; int state[8]; std::string name; };
static Run MakeRun()
{
    Run r;
    r.body = (int)Rnd(9);
    static const char* const kTexts[] = {"a", "longer text", "", "boom", "mid size", "x"};
    for (unsigned i = Rnd(5); i > 0; i--) r.s.texts.push_back(kTexts[Rnd(6)]);
    r.s.textThrowAt = Rnd(5) == 0 ? (int)Rnd(4) : -1;
    r.s.commaThrowAt = Rnd(5) == 0 ? (int)Rnd(3) : -1;
    r.s.setTextThrows = Rnd(4) == 0;
    for (unsigned i = Rnd(5); i > 0; i--) r.s.eos.push_back(Rnd(3) == 0);
    r.s.parse = Rnd(10) < 6 ? 1 : (int)Rnd(4);
    for (int i = 0; i < 4; i++) r.s.refs[i] = Rnd(3) == 0;
    for (int i = 0; i < 8; i++) r.s.params[i] = Rnd(2) ? -1 : (int)Rnd(200) - 50;
    for (int i = 0; i < 4; i++) r.s.colors[i] = (int)Rnd(100000);
    r.s.base = (int)Rnd(3);
    r.s.found = Rnd(3) != 0;
    r.s.parents = (int)Rnd(4);
    for (int i = 0; i < 3; i++) { r.s.parentComp[i] = Rnd(3) != 0; r.s.parentScheme[i] = Rnd(3) == 0; }
    r.s.wmColor = (int)Rnd(1000);
    r.s.fontThrows = Rnd(8) == 0 ? 1 + (int)Rnd(2) : 0;
    r.s.width = (int)Rnd(100); r.s.height = (int)Rnd(50);
    for (int i = 0; i < 8; i++) r.state[i] = (int)Rnd(4);
    static const char* const kNames[] = {"PERCENTLAYOUT", "COLORSCHEME", "percentlayout", "OTHER", "SIZE", "HEIGHT", "WIDTH", "size", ""};
    r.name = kNames[Rnd(9)];
    return r;
}
#define SIDE(NS)                                                                                                      \
    static std::string Play##NS(const Run& r, bool mutated)                                                           \
    {                                                                                                                 \
        using namespace NS;                                                                                           \
        sTrace.clear(); Live().clear(); sNextId = 0; sTextsRead = sCommas = sEos = 0;                                 \
        gScript = r.s;                                                                                                 \
        WindowManager::GetDefaultWindowManager()->mColorScheme = new ColorScheme(r.s.wmColor);                       \
        sRefComps.clear(); for (int i = 0; i < 4; i++) sRefComps.push_back(new Component);                            \
        std::string out;                                                                                              \
        ConfigParser parser;                                                                                          \
        {                                                                                                             \
            SmartPtr<ContainerConfig> root = new ContainerConfig;                                                     \
            root->mComponent = new Container;                                                                         \
            root->mContainer = static_cast<Container*>(root->mComponent.get());                                       \
            root->mCurLayoutId = r.state[0];                                                                          \
            /* the parent chain: configs with or without a component, with or without its own scheme */            \
            std::vector<SmartPtr<ComponentConfig> > chain;                                                            \
            ConfigObject* up = NULL;                                                                                  \
            for (int i = 0; i < r.s.parents; i++) {                                                                   \
                SmartPtr<ComponentConfig> p = new ComponentConfig;                                                    \
                if (r.s.parentComp[i]) { p->mComponent = new Component; if (r.s.parentScheme[i]) p->mComponent->mOwnScheme = new ColorScheme(1000 * (i + 1)); } \
                p->mParent = up; up = p.get(); chain.push_back(p);                                                    \
            }                                                                                                         \
            tr("-- body " + P(r.body));                                                                               \
            try {                                                                                                     \
                Port::ComponentConfig* pk = NULL; intptr_t px = 0, py = 0, pz = 0; EeWonCfgBody pb = EeWonCfgBody::FitText; \
                int v = -5;                                                                                           \
                switch (r.body) {                                                                                     \
                case 0: {                                                                                             \
                    SmartPtr<ComponentConfig> k = new ComponentConfig; k->mComponent = new Component;                 \
                    if (mutated) { pk = (Port::ComponentConfig*)(void*)k.get(); pb = EeWonCfgBody::FitText; Port::MutatedCall(pk, pb, px, py, pz); } \
                    else k->HandleFitText();                                                                          \
                    out = "fit " + k->mComponent->mText.s + " " + P(k->mComponent->mW) + "x" + P(k->mComponent->mH); \
                    break;                                                                                            \
                }                                                                                                     \
                case 1: {                                                                                             \
                    bool b = mutated ? Port::MutatedCall((Port::ComponentConfig*)(void*)root.get(), EeWonCfgBody::ContainerInstruction, (intptr_t)&parser, (intptr_t)r.name.data(), (intptr_t)r.name.size()) != 0 \
                                     : root->HandleInstruction(&parser, r.name);                                      \
                    out = "instruction " + P(b);                                                                      \
                    break;                                                                                            \
                }                                                                                                     \
                case 2: {                                                                                             \
                    SmartPtr<PercentLayoutConfig> k = new PercentLayoutConfig; k->mParent = root.get(); k->mName = r.name; \
                    bool b = mutated ? Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::PercentPreParse, (intptr_t)&parser, 0, 0) != 0 : k->PreParse(&parser); \
                    out = "percent " + P(b) + " " + PP(k->target);                                                    \
                    break;                                                                                            \
                }                                                                                                     \
                case 3: {                                                                                             \
                    SmartPtr<ColorSchemeConfig> k = new ColorSchemeConfig; k->mParent = up;                           \
                    bool b = mutated ? Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::SchemePreParse, (intptr_t)&parser, 0, 0) != 0 : k->PreParse(&parser); \
                    out = "scheme " + P(b) + " " + PS(k->mColorScheme.get());                                         \
                    break;                                                                                            \
                }                                                                                                     \
                case 4: case 5: case 6: {                                                                             \
                    SmartPtr<FontConfig> k = new FontConfig;                                                          \
                    k->mDescriptor = FontDescriptor{"Arial", r.state[1], 8 + r.state[2]};                             \
                    k->mNeedRecalcFont = r.state[3] != 0;                                                             \
                    if (r.state[4]) k->mFont = new Font(r.state[4] == 1 ? k->mDescriptor : FontDescriptor{"Other", 1, 5}); \
                    SmartPtr<Font> other = r.state[5] ? new Font(FontDescriptor{"Set", 2, 11}) : NULL;                 \
                    if (r.body == 4) {                                                                                \
                        Font* f = mutated ? (Font*)Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::FontGet, 0, 0, 0) : k->GetFont(); \
                        out = "font " + PP(f);                                                                        \
                    } else if (r.body == 5) {                                                                         \
                        if (mutated) Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::FontSet, (intptr_t)other.get(), 0, 0); \
                        else k->SetFont(other.get());                                                                 \
                        out = "set";                                                                                  \
                    } else {                                                                                          \
                        bool b = mutated ? Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::FontNamed, (intptr_t)r.name.data(), (intptr_t)r.name.size(), (intptr_t)&v) != 0 : k->GetNamedValue(r.name, v); \
                        out = "named " + P(b) + " " + P(v);                                                           \
                    }                                                                                                 \
                    out += " " + PP(k->mFont.get()) + " " + k->mDescriptor.mFace + P(k->mDescriptor.mStyle) + P(k->mDescriptor.mSize) + P(k->mNeedRecalcFont); \
                    break;                                                                                            \
                }                                                                                                     \
                case 7: {                                                                                             \
                    SmartPtr<ImageConfig> k = new ImageConfig; k->mImage = new Image(10 + r.state[1]);                \
                    bool b = mutated ? Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::ImageNamed, (intptr_t)r.name.data(), (intptr_t)r.name.size(), (intptr_t)&v) != 0 : k->GetNamedValue(r.name, v); \
                    out = "image named " + P(b) + " " + P(v);                                                         \
                    break;                                                                                            \
                }                                                                                                     \
                default: {                                                                                            \
                    SmartPtr<ImageFilterConfig> k = new ImageFilterConfig;                                            \
                    k->mRawImage = new RawImage(5 + r.state[1]);                                                      \
                    if (r.state[2]) k->mImage = new Image(1);                                                         \
                    k->mImageIsRawImage = r.state[3] != 0;                                                            \
                    Image* m = mutated ? (Image*)Port::MutatedCall((Port::ComponentConfig*)(void*)k.get(), EeWonCfgBody::FilterImage, 0, 0, 0) : k->GetImage(); \
                    out = "filter " + PP(m) + " " + P(k->mImageIsRawImage);                                           \
                    break;                                                                                            \
                }                                                                                                     \
                }                                                                                                     \
            } catch (ConfigObjectException& e) { out = "threw config " + e.mText; }                                   \
            catch (OtherException& e) { out = "threw other " + P(e.mCode); }                                          \
            tr(out + " live" + Census());                                                                             \
        }                                                                                                             \
        sRefComps.clear();                                                                                            \
        WindowManager::GetDefaultWindowManager()->mColorScheme = NULL;                                                \
        tr("after" + Census());                                                                                       \
        return sTrace;                                                                                                \
    }
SIDE(Orig)
SIDE(Port)

int main(int argc, char** argv)
{
    const int runs = argc > 1 ? atoi(argv[1]) : 30000;
    int bad = 0;
    size_t bytes = 0;
    std::vector<Run> all;
    for (int i = 0; i < runs; i++) all.push_back(MakeRun());
    for (int i = 0; i < runs; i++) {
        std::string a = PlayOrig(all[i], false), b = PlayPort(all[i], false);
        bytes += a.size();
        if (a != b && bad++ < 3) printf("run %d differs:\n--- orig\n%s--- port\n%s", i, a.c_str(), b.c_str());
    }
    printf("won_cfgbody34_test: %d runs, %u trace bytes, %d mismatching\n", runs, (unsigned)bytes, bad);
    int missed = 0, ops = (int)EeWonCfgBodyOp::ImagePtr + 1;
    for (int op = 0; op < ops; op++) {
        Port::sMutate = op;
        bool caught = false;
        for (int i = 0; i < runs && i < 5000 && !caught; i++) caught = PlayOrig(all[i], false) != PlayPort(all[i], true);
        if (!caught) { missed++; printf("mutation of op %d not caught\n", op); }
    }
    Port::sMutate = -1;
    printf("won_cfgbody34_test: %d mutations, %d caught\n", ops, ops - missed);
    return bad + missed;
}
