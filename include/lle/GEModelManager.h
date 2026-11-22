// C:\EEX\Low-Level Engine\Graphics Engine\GEModelManager.h (RECONSTRUCTION). Creates, loads (glTF files through an
// FSPath) and reference-counts GEModels by name. GEModelManager.cpp: 0x100206c7..0x10020d20.
// port (R4.3, seventh part): a whole Rust object (ee_ge manobj.rs); this class is its shell.
#ifndef LLE_GEMODELMANAGER_H
#define LLE_GEMODELMANAGER_H

#include <stdint.h>
#include "rust/render/ge.h"   // the manager (ee_ge manobj.rs), its host's EeGeManOp
#include "lle/UString.h"

class FSFileSpec;
class FSPath;
class GEModel;

class LLE_API GEModelManager {              // sizeof 0x18 in the original (mModels, mPath, mCriticalSection,
                                            // mMemoryFootprint: now the Rust object's)
public:
    // the manager on a model search path, with its lock
    GEModelManager(FSPath* inPath) { this->mRust = ee_ge_modman_new(&GEModelManager::Host, this, inPath); }
    // removes every unreferenced model, releases the path and the lock
    ~GEModelManager() { ee_ge_modman_free(this->mRust); }
    GEModelManager(const GEModelManager&) = delete;   // port: one Rust object per manager
    GEModelManager& operator=(const GEModelManager&) = delete;
    // a new model registered under a numbered name ("GEModelManager::CreateModel(name_N)")
    GEModel* CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                         unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags)
        { return (GEModel*)ee_ge_modman_create(this->mRust, inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount, inSubModelCount, inName, inFlags); }
    // a model attached to a parent model as the sub-model at an index (the parent holds it)
    GEModel* CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                            unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags, unsigned long inIndex,
                            GEModel* inParent)
        { return (GEModel*)ee_ge_modman_create_sub(this->mRust, inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount, inSubModelCount, inFlags, inIndex, inParent); }
    // Writes the list of loaded models and their reference counts to a file
    void DumpAllLoadedMedia(FSFileSpec& inFile) { ee_ge_modman_dump(this->mRust, &inFile); }
    // exported, ICF-folded (0x10040123); the sum of the cached models' SizeOf()
    unsigned long GetMemoryFootprint() { return ee_ge_modman_footprint(this->mRust); }
    // The loaded model of that name, else its glTF (.glb, .gltf) found, stored and registered under the name; NULL when
    // there is none or it does not load. The cache's key is the name as given ("models\x"; the original's was
    // "<name>.cem"). inShared: the frames the model shares (the original's std::map<unsigned long, long>:
    // EEGraphicObject's frame maps) as ee_ge's slice, in frame order
    GEModel* LoadModel(const UString& inName, const EeGeSharedFrames& inShared = EeGeSharedFrames{NULL, 0})
        { return (GEModel*)ee_ge_modman_load(this->mRust, inName, &inShared); }
    // Releases every model nobody else references; returns 5 when some are still in use
    long RemoveAllModels() { return ee_ge_modman_remove_all(this->mRust); }
    // Releases a model if nobody else references it; 3 when it is unknown, 5 when it is still in use
    long RemoveModel(GEModel* inModel) { return ee_ge_modman_remove(this->mRust, inModel); }
    // Replaces the model search path, keeping a reference to the new one
    void SetModelPath(FSPath* inPath) { ee_ge_modman_set_path(this->mRust, inPath); }
private:
    // the calls the Rust object makes on other objects (GEModel.cpp)
    static intptr_t Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c);

    EeGeModelManager* mRust;                // the cache (name -> model), the path, the lock and the footprint
};

#endif
