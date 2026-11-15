// C:\EEX\Low-Level Engine\File System\FSPersistent.h (file name inferred): an FSRestorable with a type tag that
// FSPersistentList can create from a registered prototype. Vtable 0x10097604. Every member inline, emitted with
// File System.cpp's inlines (0x4409..0x4495). Requires lle/FSRestorable.h.
#ifndef LLE_FSPERSISTENT_H
#define LLE_FSPERSISTENT_H

class LLE_API FSPersistent : public FSRestorable {
public:
    FSPersistent(long inTagID) { this->mTagID = inTagID; }                      // 0x4409
    virtual ~FSPersistent() {}                                                  // +0x10 0x441b
    virtual FSPersistent* CreateDefault() const = 0;                            // +0x14 (provisional name)
    virtual long GetTagID() const { return this->mTagID; }                      // +0x18 (folded 0x4380)
    // implicit copy ctor (0x442b)

private:
    FSPersistent() {}                                                           // 0x4422

    long mTagID;                            // +0x04
};

#endif
