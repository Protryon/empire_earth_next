// C:\EEX\Empire Earth\AI Thread\EECPASGTransport.h (RECONSTRUCTION; file name inferred).
// What a transport attack sub group carries (RTTI EECPASGTransport, no base; vtable 0x43bbdc, 10 slots, four
// _purecall; sizeof 0x10: the transport kinds put it at +0x1e8, after their EECPAttackSubGroup part). Ctor 0x51805
// (+4/+8/+0xc = 0, then the vptr), ??_G 0x51819, dtor 0x51835 (the vptr store only: an empty out-of-line dtor).
// Names are provisional unless a comment names the evidence.
#ifndef EE_EECPASGTRANSPORT_H
#define EE_EECPASGTRANSPORT_H

class EECPAttackSubGroup;
class EECPASGLand;
class EECPILocation;

class EECPASGTransport {
public:
    EECPASGTransport();                     // 0x51805
    virtual ~EECPASGTransport();            // +0x00 (??_G 0x51819; dtor 0x51835: the vptr store only)
    // +0x04 _purecall (EECPASGAirTransport 0x44188: returns a count kept in ebx)
    virtual long CountCargo() = 0;
    // +0x08 _purecall (EECPASGAirTransport 0x50ab8: a sum over its units' +0x24c)
    virtual long GetCargoCapacity() = 0;
    virtual bool Virtual0C();               // +0x0c 0x18aa9 (`mov al, 1; ret`)
    // +0x10 0x5183c: mLand as its saved id (unsigned long), then mL0C
    virtual long Load(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& inFile);
    virtual void StartLoading() = 0;        // +0x14 _purecall (EECPASGAirTransport 0x44a58)
    // +0x18 0x51871 (returns 0): mLand from its id through this sub group's commander (dynamic_cast<EECPASGLand*>)
    virtual long Restore(unsigned long inMajorVersion, unsigned long inMinorVersion);
    // +0x1c 0x518ec: mLand's mID (0 without one), then mL0C
    virtual long Save(unsigned long inMajorVersion, unsigned long inMinorVersion, FSFile& outFile);
    // +0x20 0x51931: replaces mLand (ReleaseLand 0x518c7 first; references the new one)
    virtual void SetLand(EECPASGLand* inLand);
    // +0x24 _purecall (EECPASGAirTransport 0x44c2f, ret 4: reads inLocation's tile at +4/+8)
    virtual void UnloadAt(const EECPILocation& inLocation) = 0;

    void ReleaseLand();                     // 0x518c7 (clears mLand->mTransport when it is this; Dereference)

    EECPASGLand* mLand;                     // +0x04 (referenced; saved as its mID; EECPASGLand::mTransport points back)
    EECPAttackSubGroup* mOwner;             // +0x08 (the kinds' ctors: their own EECPAttackSubGroup part)
    long mCargoStage;                       // +0x0c (saved)
};

#endif
