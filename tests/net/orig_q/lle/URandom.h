// C:\EEX\Low-Level Engine\Utilities\URandom.h (RECONSTRUCTION). Emission order in the unit at 0x1000dd9e:
// ctor, operator=, Generate(), Generate(min, max), NextInSeries, default-ctor closure, URandomIntegers::Generate,
// URandomIntegers ctor. With operator= second, only the ctor is in-class inline in the engine's own view; the
// rest follow the class (see docs/CALIBRATION.md "lle" for the plug-in inline evidence).
#ifndef LLE_URANDOM_H
#define LLE_URANDOM_H

#include <stdint.h>
#include "rust/base/random.h"   // the draws (ee_base random)

class LLE_API URandom {                     // proven sizeof 0x10 (TCP: operator new(0x10))
public:
    // proven inline (TCP NPIMGetStatus/NPIMActivate): LCG constants stored before the seed; seed 0 -> 1
    // default argument 0: the exported `default constructor closure' 0xde43 inlines this ctor (seed 0 -> store 1);
    // exe EERandomMapData's implicit ctor 0x283e06 calls the import for its +0x10 member with `push 0`
    URandom(unsigned long inSeed = 0)       // proven if/else: two stores (TCP 0x1345, 0x1df6)
        : mSeed(inSeed == 0 ? 1 : (uint32_t)inSeed), mMultiplier(0x343fd), mModulus(0x80000000), mIncrement(0x269ec3) {}
    // user-defined in the original (field moves, exported ICF pair 0x1000ddca): the same four copies
    URandom& operator=(const URandom& inRandom) = default;
    float Generate() { return ee_urandom_generate(reinterpret_cast<EeURandom*>(this)); }
    // proven inline (LLE 0x1000de1a, exported in the inline region): Default.dll 0xc0db expands it around a
    // call to the Generate() import
    float Generate(float inMinimum, float inMaximum) { return this->Generate() * (inMaximum - inMinimum) + inMinimum; }
    // exported, ICF-folded (0x10b8d, 0x54ae4): plain field accessors (no seed 0 -> 1 fix-up, unlike the ctor)
    unsigned long GetSeed() { return this->mSeed; }
    void SetSeed(unsigned long inSeed) { this->mSeed = inSeed; }
private:
    unsigned long NextInSeries() { return ee_urandom_next(reinterpret_cast<EeURandom*>(this)); }           // exported private ?NextInSeries@URandom@@AAEKXZ
public:

    // 32-bit state on every data model: the series is the same on Windows, Linux and macOS (lockstep games and
    // seeded maps depend on it). The functions keep their unsigned long signatures (the harness finds
    // NextInSeries/Generate by their mangled names); a seed is taken as its low 32 bits.
    uint32_t mSeed;                         // +0x00
    uint32_t mMultiplier;                   // +0x04
    uint32_t mModulus;                      // +0x08
    uint32_t mIncrement;                    // +0x0c
};
static_assert(sizeof(URandom) == 16, "URandom: four 32-bit words");
// port, TEST ONLY: a hook called after every out-of-line Generate() (NULL in the game; EE-SIM's --rng-trace sets it to
// write the physics stream's draws with their callers, the save/load fidelity hunt). It reads only.
extern "C" void (*gURandomTraceHook)(URandom* inRandom);

class LLE_API URandomIntegers : public URandom {   // proven base: LLE Generate(long,long) 0xde5f calls
public:                                             // URandom::Generate on the same this
    URandomIntegers() {}
    // EETreeClumps ctor 0xa53e expands LLE's body inline (URandom::Generate import, fimul (max + 1 - min),
    // fld/fistp, `== max + 1` clamp) and GEModel Caching.cpp expands it (MutateTriGenes 0x17735, MutateVertexGenes
    // 0x1784f) as well as the ctor (OptimizeMemoryCoherency 0x17912, OptimizeVertexCaching 0x180e1): both are inline
    // after the class in the newer-header view (emitted by GEBitmap.cpp, the first includer, at 0xde5f/0xde95).
    long Generate(long inMinimum, long inMaximum) { return ee_urandom_generate_int(reinterpret_cast<EeURandom*>(this), inMinimum, inMaximum); }
};

#endif
