// EEPhysicsModel.h (RECONSTRUCTION; file name inferred). A unit
// class's physics model (RTTI EEPhysicsModel, vtable 0x442848; the EEPM* classes derive from it; they are empty
// singletons: gPM* (0x531d58..0x531d84) hold one `new` of each, EEUnitClass +0x110 points at one). The code is in
// Physics Thread/EEPMLand.cpp, range 0x1543b7..0x16066d (unit inferred: no __FILE__).
// Slot names are the vtable offsets; the arguments are proven by `ret N` and the callers. The per-unit "state"
// (EEComplexUnit +0x268) is dispatched by Step: 0 -> V04, 1 -> V10, 2 -> V34, 3 -> V38, 4 -> V50, 5 -> V54, then V4C.
#ifndef EE_EEPHYSICSMODEL_H
#define EE_EEPHYSICSMODEL_H

class EEComplexUnit;
class GE2DPoint;
class GE3DPoint;
class GETransformation;
class EEPMLand;

// TEST ONLY (USST, R5b.4 leaf 6, the twelfth round; EEPhysicsModel.cpp): a float's bits; the push arithmetic's note
// (kUnitStatePush: the members' push and location, the collision period, inSpeed the speed it is capped at; a move's
// sizes and point, zeros for none)
uint32_t EEPMBits(float inValue);
void EEPMNotePush(EEComplexUnit* inUnit, float inSpeed, bool inMoving, float inDX, float inDY, float inX, float inY);
// a speed mode's note (kUnitStateSpeedMode: the remaining distance and the terrain pitch as the mode read them)
void EEPMNoteSpeedMode(EEComplexUnit* inUnit, bool inHelicopter, float inRemaining, float inPitch, float inDistance,
                       float inTurn, long inMode);
// New (R5b.4 leaf 6, the twelfth round): the models' shared arithmetic, Rust (ee_units physics.rs; each noted in USST)
// over the unit's values: the pitch mode and approach (inNaval: EEPMNaval's and EEPMSpaceship's, over the class's
// rates; else the unit's, its active group's when it is in one), the speed after a mode (inSpeed, inMaxSpeed,
// inAccel: the model's), the speed mode (inHelicopter: EEPMHelicopter's), the collision push's arithmetic (inSpeed:
// the speed it is capped at; true when the unit moves, to outX, outY; outPast: past the move distance)
long EEPMPitchMode(EEComplexUnit* inUnit, bool inNaval, float inRate, float inAngle);
void EEPMApproachPitch(EEComplexUnit* inUnit, bool inNaval, long inMode, float inCurrent, float inTarget, float* outRate,
                       float* outNew);
float EEPMAccelerate(EEComplexUnit* inUnit, float inSpeed, float inMaxSpeed, float inAccel, float inDistance, long inMode);
long EEPMSpeedMode(EEComplexUnit* inUnit, bool inHelicopter, float inDistance, float inTurn);
bool EEPMPush(EEComplexUnit* inUnit, float inSpeed, float& outX, float& outY, bool& outPast);

class EEPhysicsModel {
public:
    virtual ~EEPhysicsModel() {}                                                        // +0x00 (??_G 0x156ddb)
    virtual void Step0(EEComplexUnit* inUnit) {}                                        // +0x04 (state 0)
    virtual void ApplyBob(EEComplexUnit* inUnit) {}                                     // +0x08
    virtual void Move(EEComplexUnit* inUnit);                                           // +0x0c 0x156213 (Update the unit's motion)
    virtual void Step1(EEComplexUnit* inUnit) {}                                        // +0x10 (state 1)
    // +0x14 0x154678: the heading/pitch the unit needs to face inTo from inFrom; the second output is the pitch
    virtual void PitchTowardSlope(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float inDistance, float* outYaw, float* outPitch);
    virtual void ApproachPitch(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outRate, float* outNew);  // +0x18 0x15453a
    // +0x1c 0x154a25
    virtual void TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset);
    virtual void SteadyTurn(EEComplexUnit* inUnit, float* outA, float* outB, float* outC);  // +0x20 0x154ada
    virtual float Accelerate(EEComplexUnit* inUnit, float inDistance, float inTurn);    // +0x24 0x154bde
    virtual long SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn);      // +0x28 0x154cb2 (2 hold, 3 accelerate, 4 brake)
    virtual long PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle);         // +0x2c 0x154e98
    virtual long YawMode(EEComplexUnit* inUnit, float inRate, float inAngle);           // +0x30 0x154f56
    virtual void Step2(EEComplexUnit* inUnit) {}                                        // +0x34 (state 2)
    virtual void Step3(EEComplexUnit* inUnit);                                          // +0x38 0x1550dc (state 3)
    virtual void Collide(EEComplexUnit* inUnit);                                        // +0x3c 0x1560cf
    virtual bool Push(EEComplexUnit* inUnit, EEComplexUnit* inOther);                   // +0x40 0x155984
    virtual bool PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther);             // +0x44 0x155247
    virtual void RemoveBob(EEComplexUnit* inUnit) {}                                    // +0x48
    virtual void AfterStep(EEComplexUnit* inUnit);                                      // +0x4c 0x156710
    virtual void Step4(EEComplexUnit* inUnit) {}                                        // +0x50 (state 4)
    virtual void Step5(EEComplexUnit* inUnit) {}                                        // +0x54 (state 5)
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);         // +0x58 0x156908
    // New (port): Move's body, shared by EEPMNaval and EEPMSpaceship (their copies were the base's without the ground
    // placement: inOnGround false)
    void MoveByPush(EEComplexUnit* inUnit, bool inOnGround);

    // 0x156897: by inUnit's +0x268 state (EEUCSpaceBuilding/EEUCNavalBuilding::Update when the unit is placed)
    void Step(EEComplexUnit* inUnit);
    // 0x554738 (the turn-rate step of Virtual1C): inCurrent/inTarget headings, outputs the angle left, the new rate and the new heading
    void TurnTowardAngle(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outA, float* outB, float* outC, bool inFlag);
    // 0x55512b (brake the turn rate: outRate, outNew heading)
    void SpinDown(EEComplexUnit* inUnit, float* outRate, float* outNew);
};

class EEPMLand : public EEPhysicsModel {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15a0db
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x15a785
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x15a3c6
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x15a75b
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15e754
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15be0e
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15a791
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x15aa64
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x15abe8
};

class EEPMHovercraft : public EEPMLand {
public:
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15a0cf
};

class EEPMAirplane : public EEPhysicsModel {
public:
    // 0x557999: the unit follows its carrier's (mCounterKind1) tag point (the launch run): OrientOnCarrier moves it there
    void FollowCarrier(EEComplexUnit* inUnit);
    // 0x5581a0: true when the unit sits on its carrier (kind 2/4, commands 0x3ea/0x3f6, mIsClearanceGranted) away from the carrier's tag point
    bool IsDocked(EEComplexUnit* inUnit);
    // 0x5582d6: the unit at inPoint + (0, inOffset, 0) under inCarrier, oriented as its carrier; then the class's 0x613db5
    void OrientOnCarrier(const GETransformation& inCarrier, const GE3DPoint& inPoint, EEComplexUnit* inUnit, float inOffset);
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15726c
    virtual void ApplyBob(EEComplexUnit* inUnit) {}    // +0x08 (empty: 0x157269)
    virtual void Move(EEComplexUnit* inUnit) {}        // +0x0c (empty: 0x157269)
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x1576a9
    virtual void TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset);  // +0x1c 0x15e718
    virtual long SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn);  // +0x28 0x157b1e
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x157c83
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c (empty: 0x157269)
    virtual void RemoveBob(EEComplexUnit* inUnit) {}   // +0x48 (empty: 0x157269)
    virtual void AfterStep(EEComplexUnit* inUnit) {}   // +0x4c (empty: 0x157269)
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x1583ad
    virtual void Step5(EEComplexUnit* inUnit) {}       // +0x54 (empty: 0x157269)
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x1586d2
    virtual float ComputePitch(EEComplexUnit* inUnit, float inValue);  // +0x5c 0x157c2d
    virtual float ComputeRoll(EEComplexUnit* inUnit, float inValue);  // +0x60 0x15da2c
};

class EEPMAirAnimal : public EEPMAirplane {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x156df8
    virtual void TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset);  // +0x1c 0x15d8de
    virtual long SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn);  // +0x28 0x157140
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x157251
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x11c531
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15725d
    virtual float ComputePitch(EEComplexUnit* inUnit, float inValue);  // +0x5c 0x157229
    virtual float ComputeRoll(EEComplexUnit* inUnit, float inValue);  // +0x60 0x15723d
};

class EEPMHelicopter : public EEPhysicsModel {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x158a24
    virtual void ApplyBob(EEComplexUnit* inUnit);    // +0x08 0x158ce3
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x1590e8
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x158da4
    virtual long SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn);  // +0x28 0x1593ef
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x159507
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x159a34
    virtual bool Push(EEComplexUnit* inUnit, EEComplexUnit* inOther);        // +0x40 0x1600dc
    virtual bool PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther);  // +0x44 0x1600cc
    virtual void RemoveBob(EEComplexUnit* inUnit);   // +0x48 0x1599ae
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15be0e
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x159bc6
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x159e4d
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x159ffa
    virtual float ComputePitch(EEComplexUnit* inUnit, float inValue);  // +0x5c 0x159346
    virtual float ComputeRoll(EEComplexUnit* inUnit, float inValue);  // +0x60 0x15937a
};

class EEPMNaval : public EEPhysicsModel {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15b473
    virtual void ApplyBob(EEComplexUnit* inUnit);    // +0x08 0x15b668
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x15b9d1
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x15b786
    virtual void ApproachPitch(EEComplexUnit* inUnit, float inCurrent, float inTarget, float* outRate, float* outNew);  // +0x18 0x15c047
    virtual long PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle);  // +0x2c 0x15c146
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x15be1a
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15e754
    virtual bool PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther);  // +0x44 0x15c1cb
    virtual void RemoveBob(EEComplexUnit* inUnit);   // +0x48 0x15c8f5
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15be0e
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15c987
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x15cb5f
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x15cc7f
};

class EEPMSubmergedNaval : public EEPhysicsModel {
public:
    // 0x55f49d: EEAdjustDiveDepth (the depth clamp of inLocation for the unit; Virtual0C/Virtual58) (provisional name)
    void AdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation);
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15f4af
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x15f8e3
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x15f69c
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x15fbe6
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x1600ec
    virtual bool Push(EEComplexUnit* inUnit, EEComplexUnit* inOther);        // +0x40 0x1600dc
    virtual bool PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther);  // +0x44 0x1600cc
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15be0e
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15fdde
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x15ffb9
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x1602b8
};

class EEPMSeaAnimal : public EEPMSubmergedNaval {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15d44f
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x15d793
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x15d787
    virtual void TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset);  // +0x1c 0x15d8de
    virtual float Accelerate(EEComplexUnit* inUnit, float inDistance, float inTurn);  // +0x24 0x15d7ab
    virtual long SpeedMode(EEComplexUnit* inUnit, float inDistance, float inTurn);  // +0x28 0x15dac4
    virtual long PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle);  // +0x2c 0x15da8c
    virtual long YawMode(EEComplexUnit* inUnit, float inRate, float inAngle);  // +0x30 0x15daa8
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x15dd12
    virtual void Step3(EEComplexUnit* inUnit);       // +0x38 0x15dd1e
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15dd2a
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15d79f
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15dd36
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x15dd42
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x15dd4e
    virtual float ComputePitch(EEComplexUnit* inUnit, float inValue);  // +0x5c 0x15d9d6
    virtual float ComputeRoll(EEComplexUnit* inUnit, float inValue);  // +0x60 0x15da2c
};

class EEPMProjectile : public EEPhysicsModel {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15d015
    virtual void Move(EEComplexUnit* inUnit) {}        // +0x0c (empty: 0x157269)
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x15d259
    virtual void Collide(EEComplexUnit* inUnit) {}     // +0x3c (empty: 0x157269)
    virtual void AfterStep(EEComplexUnit* inUnit) {}   // +0x4c (empty: 0x157269)
};

class EEPMMissile : public EEPhysicsModel {
public:
    void Accelerate(EEComplexUnit* inUnit);  // 0x55b041 (accelerate along the heading, SetTransformation)
    void Home(EEComplexUnit* inUnit);       // 0x55b103
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15aff0
    virtual void Move(EEComplexUnit* inUnit) {}        // +0x0c (empty: 0x157269)
    virtual void Collide(EEComplexUnit* inUnit) {}     // +0x3c (empty: 0x157269)
    virtual void AfterStep(EEComplexUnit* inUnit) {}   // +0x4c (empty: 0x157269)
};

class EEPMBuilding : public EEPhysicsModel {
public:
    virtual void Move(EEComplexUnit* inUnit) {}        // +0x0c (empty: 0x157269)
    virtual void Step2(EEComplexUnit* inUnit);       // +0x34 0x1587c7
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15e754
    virtual void AfterStep(EEComplexUnit* inUnit) {}   // +0x4c (empty: 0x157269)
};

class EEPMSpaceship : public EEPhysicsModel {
public:
    virtual void Step0(EEComplexUnit* inUnit);       // +0x04 0x15dd66
    virtual void ApplyBob(EEComplexUnit* inUnit) {}    // +0x08 (empty: 0x157269)
    virtual void Move(EEComplexUnit* inUnit);        // +0x0c 0x15e2db
    virtual void Step1(EEComplexUnit* inUnit);       // +0x10 0x15e047
    virtual void TurnToward(EEComplexUnit* inUnit, const GE3DPoint& inFrom, const GE3DPoint& inTo, float* outA, float* outB, float* outC, bool inUseFacingOffset);  // +0x1c 0x15e718
    virtual long PitchMode(EEComplexUnit* inUnit, float inRate, float inAngle);  // +0x2c 0x15c146
    virtual void Step2(EEComplexUnit* inUnit) {}       // +0x34 (empty: 0x157269)
    virtual void Collide(EEComplexUnit* inUnit);     // +0x3c 0x15e754
    virtual bool PushMoving(EEComplexUnit* inUnit, EEComplexUnit* inOther);  // +0x44 0x15e760
    virtual void RemoveBob(EEComplexUnit* inUnit);   // +0x48 0x15ee8a
    virtual void AfterStep(EEComplexUnit* inUnit);   // +0x4c 0x15be0e
    virtual void Step4(EEComplexUnit* inUnit);       // +0x50 0x15ef1a
    virtual void Step5(EEComplexUnit* inUnit);       // +0x54 0x15f0fe
    virtual bool ClipStep(EEComplexUnit* inUnit, const GEVector& inMove, GE3DPoint& ioLocation, float* outDistance);    // +0x58 0x15f21d
};

// 0x4de374 (cdecl; the crashing airplane's class +0xcc effect at inLocation) (provisional name; argument types
// inferred)
void EEPlaceImpactDecal(EEComplexUnit* inUnit, GE3DPoint& inLocation);
// a trace compiled empty in the shipping build (calls land on the shared empty `ret` 0x5720c6). Typed, not variadic: all
// six "Collision: Turning Unit" sites build the GEOrientation directly in the argument slot through its inline copy ctor
// (3x fldz into [esp], `mov eax, esp`; 0x5564c6.., 0x1592c3.., 0x15bc84.., 0x15e58e..), which a variadic call cannot
// produce (VC6 makes a frame temporary and copies it with movsd) (provisional name; argument types inferred)
void EEPhysicsTrace(const char* inFormat, unsigned long inUnitID, GEOrientation inOrientation);
// 0x63f2c0 (cdecl; clamps ioLocation's z between the unit class's -mCruiseAltitude and the ground;
// EEPMSubmergedNaval::AdjustDiveDepth) (provisional name)
void EEAdjustDiveDepth(EEComplexUnit* inUnit, GE3DPoint& ioLocation);
void EEInitPhysicsModels();     // 0x1543b7 (creates the 12 singletons gPhysicsModels[], reads world table entry 79)
void EEFreePhysicsModels();     // 0x1550ba

#endif
