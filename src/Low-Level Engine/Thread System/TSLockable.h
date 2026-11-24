// C:\EEX\Low-Level Engine\Thread System\TSLockable.h: a base class owning a TSReadWriteLock. Every member inline;
// emitted with File System.cpp's inlines (0x4499..0x4574). The original was included through the drive-less include
// path "\eex\Low-Level Engine\Thread System" (ee.EEX_INCLUDE_DIRS), so its ESVERIFY reports __FILE__ as
// "\eex\Low-Level Engine\Thread System\TSLockable.h" (assert text 0x100d8acc): include it by bare name.
// The ctor's ESVERIFY is on line 30 (push 0x1e); the member definitions follow the class. Requires lle/TSScheduler.h.
#ifndef LLE_TSLOCKABLE_H
#define LLE_TSLOCKABLE_H

class LLE_API TSLockable {                  // vtable 0x10097624 (dtor only); sizeof 8
public:
    TSLockable() { ESVERIFY_AT(TSScheduler::CreateReadWriteLock(this->mLock) == keseOK, 30); }
    virtual ~TSLockable() { TSScheduler::DestroyReadWriteLock(this->mLock); }
    void ReadLock() const { this->mLock->ReadLock(); }
    void ReadUnlock() const { this->mLock->ReadUnlock(); }
    void WriteLock() { this->mLock->WriteLock(); }
    void WriteUnlock() { this->mLock->WriteUnlock(); }
    // implicit copy ctor (0x44f8) and operator= (folded 0x43ce)

    TSReadWriteLock* mLock;                 // +0x04
};

#endif
