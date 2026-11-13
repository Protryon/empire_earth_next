// C:\EEX\Low-Level Engine\Graphics Engine\GEModelManager.h (RECONSTRUCTION). Creates, loads (glTF files through an
// FSPath) and reference-counts GEModels by name. GEModelManager.cpp: 0x100206c7..0x10020d20.
// port (R4.3, seventh part): a whole Rust object (ee_ge manobj.rs); this class is its shell.
#ifndef LLE_GEMODELMANAGER_H
#define LLE_GEMODELMANAGER_H

#include <map>
#include <stdint.h>
#include "rust/ee_ge.h"   // the manager (ee_ge manobj.rs), its host's EeGeManOp

class FSFileSpec;
class FSPath;
class GEModel;
class UString;

class LLE_API GEModelManager {              // sizeof 0x18 in the original (mModels, mPath, mCriticalSection,
                                            // mMemoryFootprint: now the Rust object's)
public:
    GEModelManager(FSPath* inPath);
    ~GEModelManager();
    GEModelManager(const GEModelManager&) = delete;   // port: one Rust object per manager
    GEModelManager& operator=(const GEModelManager&) = delete;
    GEModel* CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                         unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags);
    GEModel* CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                            unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags, unsigned long inIndex,
                            GEModel* inParent);
    void DumpAllLoadedMedia(FSFileSpec& inFile);
    unsigned long GetMemoryFootprint();     // exported, ICF-folded (0x10040123); the sum of the cached models' SizeOf()
    unsigned long GetModelCount();          // exported, ICF-folded (0x10044c71)
    GEModel* LoadModel(const UString& inName, std::map<unsigned long, long>& ioTextures);
    long RemoveAllModels();
    long RemoveModel(GEModel* inModel);
    void SetModelPath(FSPath* inPath);
    void VerifyModels();
private:
    // the calls the Rust object makes on other objects (GEModelManager.cpp)
    static intptr_t Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeModelManager* mRust;                // the cache (name -> model), the path, the lock and the footprint
};

#endif
