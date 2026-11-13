// C:\EEX\Empire Earth\AI Thread\EECPASGUProphet.h (RECONSTRUCTION; file name inferred). An
// attack sub group unit kind that casts calamities (RTTI EECPASGUProphet > EECPAttackSubGroupUnit; vtable 0x4397a8;
// sizeof 0x94; mType 8; EECPASGUProphet.cpp 0xb2be..0xd1b6): ctors 0xb2be/0xb311 (the base, mAnalyzers, the vptr, then
// mType), dtor 0xb345 (0xb367 deletes mAnalyzers' elements first). The base of EECPASGUHero and EECPASGUMechProphet.
// Four new slots (+0x7c..+0x88). Requires ee/EECPAttackSubGroupUnit.h, ee/EECPASGUProphetCalamityAnalyzerBase.h.
#ifndef EE_EECPASGUPROPHET_H
#define EE_EECPASGUPROPHET_H

class EEUnitGoal;
class EEUGRust;

class EECPASGUProphet : public EECPAttackSubGroupUnit {
public:
    EECPASGUProphet(EEComplexUnit* inUnit, EECPAttackSubGroup* inSubGroup); // 0xb2be
    EECPASGUProphet(EECPAttackSubGroup* inSubGroup);    // 0xb311
    virtual ~EECPASGUProphet();             // 0xb345
    virtual bool Retarget();                // +0x14 0xb757
    virtual void Flee();                    // +0x1c 0xba62 (EECPAttackSubGroupUnit::Flee() as a tail jump)
    virtual void SetState(long inState, unsigned long inTime);    // +0x28 0xba67
    virtual bool CanAttack(EECPASTargetUnit* inTarget);    // +0x2c 0xbcc1
    virtual bool CheckCondition(long inState, unsigned long inTime);  // +0x50 0xbb49
    virtual void Update(unsigned long inTime);      // +0x6c 0xbb69
    virtual bool ShouldRetreat();           // +0x74 0xbd2b
    // +0x7c 0xb68c: inGoal 0 -> dynamic_cast<EEUnitGoal*>(mUnit's +0x1ec); its +0x30 tech id names the calamity
    virtual bool CanStillCast(EEUGRust* inGoal);
    virtual void MoveAwayFromCast();        // +0x80 0xbaab (news a 0x38-byte goal)
    // +0x84 0xb375: a new analyzer for calamity kind inType (a switch), 0 for the others
    virtual EECPASGUProphetCalamityAnalyzerBase* CreateAnalyzer(long inTechID, unsigned long inType);
    virtual void MakeAnalyzers();           // +0x88 0xb605

    // 0xb367 (provisional name): deletes every analyzer through 0xc6cd and clears mAnalyzers
    long ReleaseAnalyzers();

    // +0x84 (_Buy(0) in both ctors; the dtor deletes every element through 0xc6cd, then _Tidy)
    std::vector<EECPASGUProphetCalamityAnalyzerBase*> mAnalyzers;
};

#endif
