// C:\EEX\Low-Level Engine\Graphics Engine\GEModelManager.h (RECONSTRUCTION). Creates, loads (.cem files through an
// FSPath) and reference-counts GEModels by name. GEModelManager.cpp: 0x100206c7..0x10020d20.
#ifndef LLE_GEMODELMANAGER_H
#define LLE_GEMODELMANAGER_H

#include <map>

class FSFileSpec;
class FSPath;
class GEModel;
class TSCriticalSection;

class LLE_API GEModelManager {              // sizeof 0x18
public:
    GEModelManager(FSPath* inPath);
    ~GEModelManager();
    GEModel* CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                         unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags);
    GEModel* CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                            unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags, unsigned long inIndex,
                            GEModel* inParent);
    void DumpAllLoadedMedia(FSFileSpec& inFile);
    unsigned long GetMemoryFootprint() { return this->mMemoryFootprint; }  // exported, ICF-folded (0x10040123)
    unsigned long GetModelCount() { return this->mModels.size(); }    // exported, ICF-folded (0x10044c71)
    GEModel* LoadModel(const UString& inName, std::map<unsigned long, long>& ioTextures);
    long RemoveAllModels();
    long RemoveModel(GEModel* inModel);
    void SetModelPath(FSPath* inPath);
    void VerifyModels();
private:
    std::map<UString, GEModel*> mModels;   // +0x00 name -> model
    FSPath* mPath;                          // +0x0c (referenced)
    TSCriticalSection* mCriticalSection;    // +0x10
    unsigned long mMemoryFootprint;         // +0x14 sum of the loaded models' SizeOf()
};

#endif
