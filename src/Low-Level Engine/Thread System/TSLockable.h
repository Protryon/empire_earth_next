// C:\EEX\Low-Level Engine\Thread System\TSLockable.h: a base class owning a TSReadWriteLock. Every member inline;
// emitted with File System.cpp's inlines (0x4499..0x4574). The original was included through the drive-less include
// path "\eex\Low-Level Engine\Thread System" (ee.EEX_INCLUDE_DIRS), so its ESVERIFY reports __FILE__ as
// "\eex\Low-Level Engine\Thread System\TSLockable.h" (assert text 0x100d8acc): include it by bare name.
// The ctor's ESVERIFY is on line 30 (push 0x1e); the member definitions follow the class. Requires lle/TSScheduler.h.
#ifndef LLE_TSLOCKABLE_H
#define LLE_TSLOCKABLE_H

class LLE_API TSLockable {                  // vtable 0x10097624 (dtor only); sizeof 8
public:
    TSLockable();
    virtual ~TSLockable();
    void ReadLock() const;
    void ReadUnlock() const;
    void WriteLock();
    void WriteUnlock();
    // implicit copy ctor (0x44f8) and operator= (folded 0x43ce)

protected:
    TSReadWriteLock* mLock;                 // +0x04
};

// ----------------------------------------------------------------------------------------------------------------
// Inline members (exported COMDATs: the first includer in link order emits them)
//
// ----------------------------------------------------------------------------------------------------------------

inline TSLockable::TSLockable()             // 0x4499
{
    ESVERIFY_AT(TSScheduler::CreateReadWriteLock(this->mLock) == keseOK, 30);
}

inline TSLockable::~TSLockable()            // 0x44c8
{
    TSScheduler::DestroyReadWriteLock(this->mLock);
}

inline void TSLockable::ReadLock() const { this->mLock->ReadLock(); }    // 0x44d8
inline void TSLockable::ReadUnlock() const { this->mLock->ReadUnlock(); }  // 0x44e0
inline void TSLockable::WriteLock() { this->mLock->WriteLock(); }        // 0x44e8
inline void TSLockable::WriteUnlock() { this->mLock->WriteUnlock(); }    // 0x44f0

#endif
