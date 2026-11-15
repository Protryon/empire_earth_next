// Golden driver for the Utilities moved in LLE lane round 7: UGraphSeries/UGraph, U2DFractal and Utilities.cpp's
// shutdown list. Random operations on pools of series and graphs (copies, assignments, points, colours, combining
// (into an input too), pruning, removals), each line the operation and its arguments, then the state of what it
// changed; fractal maps of every power-of-two size up to 64 from random corners, ranges and seeds (computed again on
// the same object); the shutdown list's order. Floats print as their bits.
//
//   python -I build/lle7/drvutil.py orig|new;   build/lle7/util_<orig|new>.exe <sequences> > golden.txt
// Linked over the original units (orig) and over the forwarders (new): both must print the same. ee_util's
// golden.rs replays the trace over the Rust objects.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"
#include "lle/GEColor.h"
#include "lle/UGraph.h"
#include "lle/U2DFractal.h"

static uint64_t gRand;
static uint32_t R(uint32_t n)
{
    gRand = gRand * 6364136223846793005ull + 1442695040888963407ull;
    return n ? (uint32_t)(gRand >> 33) % n : 0;
}

static uint32_t Bits(float inValue)
{
    uint32_t b;
    memcpy(&b, &inValue, 4);
    return b;
}

static float Float(uint32_t inBits)
{
    float f;
    memcpy(&f, &inBits, 4);
    return f;
}

static std::string SeriesState(const UGraphSeries& s)
{
    std::string o;
    char t[96];
    snprintf(t, sizeof t, "n%lu c%08x,%08x,%08x e%d r%ld,%ld,%ld,%ld p", s.GetDataPointCount(),
             Bits(s.GetColor().GetRed()), Bits(s.GetColor().GetGreen()), Bits(s.GetColor().GetBlue()),
             (int)s.GetIsEnabled(), s.GetMinX(), s.GetMaxX(), s.GetMinY(), s.GetMaxY());
    o = t;
    for (unsigned long i = 0; i < s.GetDataPointCount(); i++) {
        const U2DPoint<long>& p = s.GetDataPoint(i);
        snprintf(t, sizeof t, " %ld,%ld", p.GetX(), p.GetY());
        o += t;
    }
    return o;
}

static void PrintGraph(int inSlot, const UGraph& g)
{
    printf("G%d n%lu r%ld,%ld,%ld,%ld t%s|%s|%s\n", inSlot, g.GetSeriesCount(), g.GetMinX(), g.GetMaxX(), g.GetMinY(),
           g.GetMaxY(), (const char*)g.GetTitle(), (const char*)g.GetXLabel(), (const char*)g.GetYLabel());
    for (unsigned long i = 0; i < g.GetSeriesCount(); i++)
        printf(" %s\n", SeriesState(g.GetSeries(i)).c_str());
}

static const int kSeries = 6;
static const int kGraphs = 4;

static void GraphSequence(int inSequence)
{
    UGraphSeries* s[kSeries];
    UGraph* g[kGraphs];
    for (int i = 0; i < kSeries; i++)
        s[i] = new UGraphSeries();
    for (int i = 0; i < kGraphs; i++)
        g[i] = new UGraph();
    printf("seq %d\n", inSequence);
    int theOps = 10 + R(50);
    long theXRange = 1 + R(30);
    for (int op = 0; op < theOps; op++) {
        int i = R(kSeries), j = R(kSeries), a = R(kGraphs), b = R(kGraphs), c = R(kGraphs);
        switch (R(19)) {
        case 0:
            printf("snew %d\n", i);
            delete s[i];
            s[i] = new UGraphSeries();
            break;
        case 1:
            if (i == j)
                continue;
            printf("scopy %d %d\n", i, j);
            delete s[i];
            s[i] = new UGraphSeries(*s[j]);
            break;
        case 2:
            printf("sasg %d %d\n", i, j);
            *s[i] = *s[j];
            break;
        case 3: case 4: case 5: case 6: {
            long x = (long)R(theXRange) - (R(4) == 0 ? 5 : 0);
            long y = (long)R(200) - 50;
            printf("sadd %d %ld %ld\n", i, x, y);
            s[i]->AddDataPoint(U2DPoint<long>(x, y));
            break;
        }
        case 7:
            printf("sclr %d\n", i);
            s[i]->ClearAllDataPoints();
            break;
        case 8: {
            if (s[i]->GetDataPointCount() == 0)
                continue;
            unsigned long k = R(s[i]->GetDataPointCount());
            printf("srem %d %lu\n", i, k);
            s[i]->RemoveDataPoint(k);
            break;
        }
        case 9: {
            static const float kValues[] = {0.0f, 0.25f, 0.5f, 1.0f, -1.5f, 3.0e7f};
            float r = kValues[R(6)], gr = kValues[R(6)], bl = kValues[R(6)];
            printf("scol %d %08x %08x %08x\n", i, Bits(r), Bits(gr), Bits(bl));
            s[i]->SetColor(GEColor(r, gr, bl));
            break;
        }
        case 10: {
            int e = R(2);
            printf("sen %d %d\n", i, e);
            s[i]->SetIsEnabled(e != 0);
            break;
        }
        case 11: {
            if (g[a]->GetSeriesCount() == 0)
                continue;
            unsigned long k = R(g[a]->GetSeriesCount());
            printf("sfromg %d %d %lu\n", i, a, k);
            UGraphSeries* theCopy = new UGraphSeries(g[a]->GetSeries(k));
            delete s[i];
            s[i] = theCopy;
            break;
        }
        case 12:
            if (R(3) == 0) {
                printf("gnew %d\n", a);
                delete g[a];
                g[a] = new UGraph();
            } else {
                if (a == b)
                    continue;
                printf("gcopy %d %d\n", a, b);
                delete g[a];
                g[a] = new UGraph(*g[b]);
            }
            PrintGraph(a, *g[a]);
            continue;
        case 13:
            printf("gasg %d %d\n", a, b);
            *g[a] = *g[b];
            PrintGraph(a, *g[a]);
            continue;
        case 14: case 15:
            printf("gadd %d %d\n", a, i);
            g[a]->AddSeries(*s[i]);
            PrintGraph(a, *g[a]);
            continue;
        case 16:
            switch (R(5)) {
            case 0:
                printf("gclr %d\n", a);
                g[a]->ClearAllSeries();
                break;
            case 1:
                printf("gprune %d\n", a);
                g[a]->PruneGraph();
                break;
            case 2: {
                if (g[a]->GetSeriesCount() == 0)
                    continue;
                unsigned long k = R(g[a]->GetSeriesCount());
                printf("grem %d %lu\n", a, k);
                g[a]->RemoveSeries(k);
                break;
            }
            case 3: {
                if (g[a]->GetSeriesCount() == 0)
                    continue;
                unsigned long k = R(g[a]->GetSeriesCount());
                int e = R(2);
                printf("gen %d %lu %d\n", a, k, e);
                g[a]->SetIsSeriesEnabled(k, e != 0);
                break;
            }
            default: {
                int n = R(100);
                printf("gtitle %d %d\n", a, n);
                char t[32];
                snprintf(t, sizeof t, "title %d", n);
                g[a]->SetTitle(UWideString(t));
                snprintf(t, sizeof t, "x %d", n + 1);
                g[a]->SetXLabel(UWideString(t));
                snprintf(t, sizeof t, "y %d", n + 2);
                g[a]->SetYLabel(UWideString(t));
                break;
            }
            }
            PrintGraph(a, *g[a]);
            continue;
        default:
            printf("gcomb %d %d %d\n", a, b, c);
            g[a]->CombineGraph(*g[b], *g[c]);
            PrintGraph(c, *g[c]);
            continue;
        }
        printf("S%d %s\n", i, SeriesState(*s[i]).c_str());
    }
    for (int i = 0; i < kSeries; i++)
        delete s[i];
    for (int i = 0; i < kGraphs; i++)
        delete g[i];
}

static void PrintFractal(U2DFractal& f)
{
    unsigned long d = f.GetSquareDimension();
    for (unsigned long x = 0; x < d; x++) {
        printf(" ");
        for (unsigned long y = 0; y < d; y++)
            printf("%08x", Bits(f.GetValue(x, y)));
        printf("\n");
    }
}

static void FractalSequence(int inSequence)
{
    static const float kCorners[] = {0.0f, 0.5f, 1.0f, -2.0f, 0.125f};
    static const float kRanges[] = {0.0f, 0.1f, 0.5f, 1.0f, 10.5f, 0.25f};
    unsigned long d = 1ul << R(inSequence % 5 == 0 ? 7 : 5);
    float theCorner = kCorners[R(5)];
    float theRange = R(3) == 0 ? Float(0x3c000000u + R(0x4000000u)) : kRanges[R(6)];
    unsigned long theSeed = R(4) == 0 ? R(3) : R(0x7fffffff);
    printf("frac %lu %08x %08x %lu\n", d, Bits(theCorner), Bits(theRange), theSeed);
    U2DFractal* f;
    if (R(2) == 0) {
        f = new U2DFractal(d, theCorner, theRange, theSeed);
    } else {
        f = new U2DFractal(d);
        f->ComputeFractal(theCorner, theRange, theSeed);
    }
    PrintFractal(*f);
    theCorner = kCorners[R(5)];
    theRange = kRanges[R(6)];
    theSeed = R(0x7fffffff);
    printf("frecomp %08x %08x %lu\n", Bits(theCorner), Bits(theRange), theSeed);
    f->ComputeFractal(theCorner, theRange, theSeed);
    PrintFractal(*f);
    delete f;
}

static int gCalls;
static void Third(bool inForced)
{
    printf("shutdown third %d %d %d\n", (int)inForced, ++gCalls, (int)UShutdownInProgress());
}
static void Second(bool inForced)
{
    printf("shutdown second %d %d\n", (int)inForced, ++gCalls);
    printf("register %ld\n", URegisterShutdown(Third));
}
static void First(bool inForced)
{
    printf("shutdown first %d %d\n", (int)inForced, ++gCalls);
}

int main(int argc, char** argv)
{
    int theSequences = argc > 1 ? atoi(argv[1]) : 2000;
    if (argc > 2)
        setvbuf(stdout, NULL, _IONBF, 0);
    gRand = 7;
    for (int i = 0; i < theSequences; i++)
        GraphSequence(i);
    for (int i = 0; i < theSequences / 10; i++)
        FractalSequence(i);
    printf("progress %d\n", (int)UShutdownInProgress());
    printf("register %ld\n", URegisterShutdown(First));
    printf("register %ld\n", URegisterShutdown(Second));
    UShutdown(true, false);
    printf("progress %d\n", (int)UShutdownInProgress());
    UShutdown(false, false);
    printf("end %d\n", gCalls);
    return 0;
}
