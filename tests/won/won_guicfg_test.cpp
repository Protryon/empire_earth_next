// WONGUI's config readers' integer expressions (ComponentConfigRead.cpp): the original's ParseExpression and
// GetIntValue's name scan and classification (copied from the C++ before the port, the StringParser replaced by a
// byte cursor with the same SkipWhitespace / GetChar / IncrementPos, GetIntValue's lookups by a decimal number or a
// thrown error) against ee_won's guicfg module through its C ABI, the port's wrapper (the operand's exception caught
// in the callback and rethrown) included, over generated expressions (a fixed LCG). Prints one line per failed
// check; the exit code is the number of failed checks.
#include <stdio.h>
#include <ctype.h>
#include <exception>
#include <stdexcept>
#include <string>
#include "rust/ee_rust.h"

static int sFailures;
static int sChecks;
#define CHECK(c) do { sChecks++; if (!(c)) { sFailures++; if (sFailures < 40) printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static unsigned int sSeed = 99991;
static unsigned int Next() { sSeed = sSeed * 1103515245u + 12345u; return sSeed >> 8; }

struct ConfigObjectException : std::runtime_error { ConfigObjectException(const std::string& s) : std::runtime_error(s) {} };

// the StringParser's cursor over a narrow text
struct Parser {
    std::string s;
    int pos;
    unsigned char GetChar(int o) { int p = pos + o; return p >= 0 && p < (int)s.size() ? (unsigned char)s[p] : 0; }
    void IncrementPos(int n) { pos += n; }
    void SkipWhitespace() { while (GetChar(0) == ' ' || GetChar(0) == '\t') pos++; }
};

struct Config {
    Parser* mStringParser;
    int mDepth;
    // GetIntValue: a decimal number, else a thrown error (a missing named value)
    int GetIntValue()
    {
        int start = mStringParser->pos;
        bool neg = mStringParser->GetChar(0) == '-';
        if (neg) mStringParser->IncrementPos(1);
        unsigned int v = 0;
        bool any = false;
        while (isdigit(mStringParser->GetChar(0))) { v = v * 10 + (mStringParser->GetChar(0) - '0'); mStringParser->IncrementPos(1); any = true; }
        if (!any) { mStringParser->pos = start; throw ConfigObjectException("Invalid named value: " + std::to_string(start)); }
        return neg ? (int)(0u - v) : (int)v;
    }
    int OrigParse(int inLevel);
    int PortParse(int inLevel);
};

int Config::OrigParse(int inLevel)
{
    int value;
    this->mStringParser->SkipWhitespace();
    if(this->mStringParser->GetChar(0)=='(')
    {
        this->mStringParser->IncrementPos(1);
        value=this->OrigParse(0);
        this->mStringParser->SkipWhitespace();
        if(this->mStringParser->GetChar(0)!=')')
            throw ConfigObjectException("Expecting ')'");
        this->mStringParser->IncrementPos(1);
    }
    else
        value=this->GetIntValue();
    for(;;)
    {
        this->mStringParser->SkipWhitespace();
        unsigned char op=this->mStringParser->GetChar(0);
        this->mStringParser->IncrementPos(1);
        if(op=='+' && inLevel<1)
            value+=this->OrigParse(1);
        else if(op=='-' && inLevel<1)
            value-=this->OrigParse(1);
        else if(op=='*' && inLevel<2)
            value*=this->OrigParse(2);
        else if(op=='/' && inLevel<2)
        {
            int divisor=this->OrigParse(2);
            if(divisor==0)
                throw ConfigObjectException("Division by 0.");
            // the original faulted on INT_MIN / -1; the port gives INT_MIN
            if(divisor==-1) value=(int)(0u-(unsigned int)value);
            else value/=divisor;
        }
        else
        {
            this->mStringParser->IncrementPos(-1);
            return value;
        }
    }
}

// the port's wrapper (ComponentConfigRead.cpp after the port)
struct ExprHostCtx { Config* mConfig; std::exception_ptr mError; };
static void ExprSkip(void* h) { static_cast<ExprHostCtx*>(h)->mConfig->mStringParser->SkipWhitespace(); }
static uint8_t ExprPeek(void* h) { return static_cast<ExprHostCtx*>(h)->mConfig->mStringParser->GetChar(0); }
static void ExprAdvance(void* h, int32_t n) { static_cast<ExprHostCtx*>(h)->mConfig->mStringParser->IncrementPos(n); }
static bool ExprInt(void* h, int32_t* out)
{
    ExprHostCtx* c = static_cast<ExprHostCtx*>(h);
    try { *out = c->mConfig->GetIntValue(); return true; }
    catch (...) { c->mError = std::current_exception(); return false; }
}
int Config::PortParse(int inLevel)
{
    ExprHostCtx aCtx = { this, std::exception_ptr() };
    EeWonExprHost aHost = { &aCtx, ExprSkip, ExprPeek, ExprAdvance, ExprInt };
    int32_t aValue = 0;
    switch (ee_won_cfg_expr(&aHost, inLevel, &aValue)) {
    case 0: return aValue;
    case 1: throw ConfigObjectException("Expecting ')'");
    case 2: throw ConfigObjectException("Division by 0.");
    default: std::rethrow_exception(aCtx.mError);
    }
}

static std::string Gen(int depth)
{
    static const char* kAtoms[] = { "0", "1", "2", "7", "13", "-3", "100", "2147483647", "-2147483648", "x", "", "65535" };
    std::string s;
    int terms = 1 + (int)(Next() % 4);
    for (int i = 0; i < terms; i++) {
        if (i) {
            static const char* kOps[] = { "+", "-", "*", "/", " + ", " / ", ",", ")", "%" };
            s += kOps[Next() % 9];
        }
        if (Next() % 5 == 0) s += " ";
        if (depth < 3 && Next() % 4 == 0) s += "(" + Gen(depth + 1) + (Next() % 6 ? ")" : "");
        else s += kAtoms[Next() % 12];
    }
    return s;
}

int main()
{
    for (int n = 0; n < 300000; n++) {
        std::string text = Gen(0);
        if (Next() % 7 == 0) text += " , 5";
        int level = (int)(Next() % 3);
        Parser p1 = { text, 0 }, p2 = { text, 0 };
        Config c1 = { &p1, 0 }, c2 = { &p2, 0 };
        std::string e1, e2;
        int v1 = 0, v2 = 0;
        try { v1 = c1.OrigParse(level); } catch (const std::exception& e) { e1 = e.what(); }
        try { v2 = c2.PortParse(level); } catch (const std::exception& e) { e2 = e.what(); }
        CHECK(e1 == e2 && p1.pos == p2.pos && (!e1.empty() || v1 == v2));
    }
    // GetIntValue's name scan and kinds
    for (int c = -128; c < 128; c++) {
        char next = (char)c;
        bool o = true;
        if ((unsigned long)next < 256 ? isspace((unsigned long)next) : 0) o = false;
        else if (next!='@' && next!='_' && next!='.' && !((unsigned long)next < 256 ? isalnum((unsigned long)next) : 0)) o = false;
        CHECK(o == ee_won_cfg_name_char(next));
    }
    static const char* kNames[] = { "", "@", "@I", "@ID", "@ID_", "@ID_OK", "@id_OK", "@IDx", "@Font", "a.b", "a.b.c", ".x", "x.", "WIDTH", "@a.b" };
    for (size_t i = 0; i < sizeof(kNames) / sizeof(kNames[0]); i++) {
        std::string name = kNames[i];
        int kind; size_t at;
        if(name[0]=='@') {
            if(name.length()>3 && toupper(name[1]=='I') && toupper(name[2]=='D') && name[3]=='_') { kind = 0; at = 4; }
            else { kind = 1; at = 1; }
        } else {
            std::string::size_type dot=name.rfind('.');
            if(dot!=std::string::npos) { kind = 2; at = dot; } else { kind = 3; at = 0; }
        }
        size_t rat = 99;
        int rk = ee_won_cfg_name_kind((const uint8_t*)name.data(), name.size(), &rat);
        CHECK(rk == kind && rat == at);
    }
    printf("won_guicfg_test: %d checks, %d failures\n", sChecks, sFailures);
    return sFailures;
}
