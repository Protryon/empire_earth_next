// New (port): the attack, attack-ground and heal goals' trigger tests and steps, and the shots' trigger tests, as
// plain records (docs/RUST_PLAN.md R5b.2, sixth round; rust/crates/ee_combat/src/goals.rs). The C++ fills the plain
// reads of a test (EeCombatConditionIn, null-safe) or a step (EeCombatStepIn); the lazy reads and the step's
// operations are logged into the record in the order they are made.
//
// With EE_SIM_CAPTURE set (ee/EESimCapture.h, tools/sim_capture.py) every test is folded into a CBTC record (some ten
// million tests a calamity game: one record per 4096 tests, their count and the FNV-1a 64 of each test's record and
// EeCombatConditionOut in order, the last batch at the game's end; EE_SIM_CAPTURE_CBTC_FULL=1 writes every test's
// bytes instead, for finding where two builds part) and every step appends a CBGS record (the record with its
// operations). A test that calls another
// captured test (the torpedo's other triggers, a base class's test) records both, the inner one first.
#ifndef EE_EECOMBATGOALS_H
#define EE_EECOMBATGOALS_H

#include <stdint.h>
#include <string.h>

#include "rust/ee_rust.h"
#include "ee/EESimCapture.h"

class EEComplexUnit;
class EEUnit;

// The plain reads of a test of inUnit on inEvent with the target inTarget (NULL: none; the record's target fields
// stay 0). The attack goal's own fields (goal_tile, convert) are the caller's. Defined in EECombatGoals.cpp.
void EECombatFillCondition(EeCombatConditionIn& in, int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent,
                           EEUnit* inTarget);
// The plain reads of a step of inUnit (the target only gives its id)
void EECombatFillStep(EeCombatStepIn& in, int32_t inKind, EEComplexUnit* inUnit, EEUnit* inTarget);
// CBTC, CBGS (no record while the capture is off)
void EECombatCaptureCondition(const EeCombatConditionIn& in, const EeCombatConditionOut& inOut);
void EECombatCaptureStep(const EeCombatStepIn& in);
// writes the tests' last CBTC batch (EE-SIM at the game's end, before the capture closes)
void EECombatCaptureFlush();

// TEST ONLY: the golden instrumentation of the C++ bodies (recorded before the move to Rust). A scope fills its
// record while the capture is on and logs the reads made inside it (the thread's innermost scope).
class EECombatConditionScope {
public:
    EECombatConditionScope(int32_t inKind, EEComplexUnit* inUnit, unsigned long inEvent, EEUnit* inTarget)
        : mActive(EESimCaptureRecord::Active()), mPrevious(sCurrent)
    {
        memset(&this->mIn, 0, sizeof(this->mIn));
        if (!this->mActive)
            return;
        EECombatFillCondition(this->mIn, inKind, inUnit, inEvent, inTarget);
        sCurrent = this;
    }
    ~EECombatConditionScope()
    {
        if (this->mActive)
            sCurrent = this->mPrevious;
    }
    void Commit(long inResult, int32_t inAction)
    {
        if (!this->mActive)
            return;
        EeCombatConditionOut theOut = {(int32_t)inResult, inAction};
        EECombatCaptureCondition(this->mIn, theOut);
    }
    static void Log(int32_t inCode, uint32_t inValue)
    {
        if (sCurrent == NULL)
            return;
        EeCombatConditionIn& in = sCurrent->mIn;
        if (in.reads < 4) {
            in.log[in.reads].code = inCode;
            in.log[in.reads].value = inValue;
        }
        in.reads++;
    }

    EeCombatConditionIn mIn;

private:
    bool mActive;
    EECombatConditionScope* mPrevious;
    static thread_local EECombatConditionScope* sCurrent;
};

// a lazy read's answer, logged (bool or long)
template <class T> inline T EECombatRead(int32_t inCode, T inValue)
{
    EECombatConditionScope::Log(inCode, (uint32_t)(long)inValue);
    return inValue;
}

inline float EECombatReadF(int32_t inCode, float inValue)
{
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    EECombatConditionScope::Log(inCode, theBits);
    return inValue;
}

class EECombatStepScope {
public:
    EECombatStepScope(int32_t inKind, EEComplexUnit* inUnit, EEUnit* inTarget) : mActive(EESimCaptureRecord::Active())
    {
        memset(&this->mIn, 0, sizeof(this->mIn));
        if (this->mActive)
            EECombatFillStep(this->mIn, inKind, inUnit, inTarget);
    }
    ~EECombatStepScope()
    {
        if (this->mActive)
            EECombatCaptureStep(this->mIn);
    }
    void Op(int32_t inOp, int32_t inArg) { this->Log(inOp, inArg, 0); }
    template <class T> T Read(int32_t inOp, int32_t inArg, T inValue)
    {
        this->Log(inOp, inArg, (int32_t)inValue);
        return inValue;
    }

    EeCombatStepIn mIn;

private:
    void Log(int32_t inOp, int32_t inArg, int32_t inValue)
    {
        if (!this->mActive)
            return;
        if (this->mIn.count < 12) {
            this->mIn.log[this->mIn.count].op = inOp;
            this->mIn.log[this->mIn.count].arg = inArg;
            this->mIn.log[this->mIn.count].value = inValue;
        }
        this->mIn.count++;
    }
    bool mActive;
};

#endif
