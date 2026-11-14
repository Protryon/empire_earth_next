// C:\EEX\Low-Level Engine\Graphics Engine\GEModelManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100206c7..0x10020d20: GEModelManager's members, then the map<UString, GEModel*> COMDATs and the UList
// template-static init pairs. No U2D keys. The ESVERIFY is the original's line 39 (0x27).
//
// port (R4.3, seventh part): the manager is a whole Rust object (ee_ge manobj.rs): the cache, the path, the lock and
// the footprint with every method. This unit is its shell: the forwarders and the host, which makes each call the
// original made on another object (the lock, the path, the models, the file lookup, the dump's lines).
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <map>

#include "lle/LowLevelEngineCore.h"
#include "lle/UString.h"
#include "lle/UWideString.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"
#include "lle/TSScheduler.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/GEModel.h"
#include "lle/GEModelManager.h"
#include "GEOpenFormats.h"   // GEFindModelFile: a model's .glb/.gltf
#include "rust/ee_ge.h"       // the manager (ee_ge manobj.rs), ee_ge_cem_free

// CreateModel's and CreateSubModel's counts and flags, through the Rust object to the host
struct GEModelCreateArgs {
    unsigned long mPolygonCount, mVertexCount, mMaterialCount, mTagPointCount, mFrameCount, mSubModelCount, mFlags;
};

// The calls the manager makes on other objects (EeGeManOp), in the original's statements
intptr_t GEModelManager::Host(void* inOwner, EeGeManOp inOp, void* inObj, intptr_t a, intptr_t b, intptr_t c)
{
    GEModelManager* theManager = (GEModelManager*)inOwner;
    GEModel* theModel = (GEModel*)inObj;
    const GEModelCreateArgs* theArgs = (const GEModelCreateArgs*)a;
    switch (inOp) {
    case EeGeManOp::LockCreate: {
        TSCriticalSection* theLock = NULL;
        ESVERIFY_AT(TSScheduler::CreateCriticalSection(theLock) == keseOK, 39);
        return (intptr_t)theLock;
    }
    case EeGeManOp::LockDestroy:
        TSScheduler::DestroyCriticalSection((TSCriticalSection*)inObj);
        return 0;
    case EeGeManOp::LockEnter:
        ((TSCriticalSection*)inObj)->Enter();
        return 0;
    case EeGeManOp::LockLeave:
        ((TSCriticalSection*)inObj)->Leave();
        return 0;
    case EeGeManOp::PathReference:
        ((FSPath*)inObj)->Reference();
        return 0;
    case EeGeManOp::PathDereference:
        ((FSPath*)inObj)->Dereference();
        return 0;
    case EeGeManOp::RefCount:
        return (intptr_t)theModel->mReferenceCount;
    case EeGeManOp::BaseDereference:
        theModel->UReferenceCounter::Dereference();
        return 0;
    case EeGeManOp::DumpHeader:
        fprintf(*(FSFileSpec*)inObj, "\n\n*********Model Manager - Current Loaded Files In Memory*********\n\n");
        return 0;
    case EeGeManOp::DumpLine:
        fprintf(*(FSFileSpec*)b, "The %s model file is loaded in memory with a reference count of %ld.\n",
                (const char*)a, theModel->mReferenceCount);
        return 0;
    case EeGeManOp::ModelNew:
        return (intptr_t)new GEModel(theArgs->mPolygonCount, theArgs->mVertexCount, theArgs->mMaterialCount,
                                     theArgs->mTagPointCount, theArgs->mFrameCount, theArgs->mSubModelCount,
                                     UString((const char*)b), theArgs->mFlags, theManager);
    case EeGeManOp::ModelNewSub: {
        GEModel* theSubModel = new GEModel(theArgs->mPolygonCount, theArgs->mVertexCount, theArgs->mMaterialCount,
                                           theArgs->mTagPointCount, theArgs->mFrameCount, theArgs->mSubModelCount,
                                           theModel->GetName(), theArgs->mFlags, theManager);
        theModel->SetSubModel(theSubModel, (unsigned long)b);
        theSubModel->UReferenceCounter::Dereference();
        return (intptr_t)theSubModel;
    }
    case EeGeManOp::ModelFindFile:
        return GEFindModelFile((FSPath*)inObj, UString((const char*)a), *(std::map<unsigned long, long>*)b,
                               *(EeGeCemModel**)c);
    case EeGeManOp::ModelNewEmpty:
        return (intptr_t)new GEModel(theManager);
    case EeGeManOp::ModelStoreLoaded:
        return theModel->StoreLoaded((const EeGeCemModel*)a, *(std::map<unsigned long, long>*)b);
    case EeGeManOp::ModelSizeOf:
        return (intptr_t)theModel->SizeOf();
    case EeGeManOp::ModelVerify:
        theModel->Verify();
        return 0;
    case EeGeManOp::ModelFreeFile:
        ee_ge_cem_free((EeGeCemModel*)inObj);
        return 0;
    default:
        return 0;
    }
}

// FUNCTION: LLE 0x100206c7
// Creates the model manager with a model search path and its lock
GEModelManager::GEModelManager(FSPath* inPath)
{
    this->mRust = ee_ge_modman_new(&GEModelManager::Host, this, inPath);
}

// FUNCTION: LLE 0x10020717
// Destroys the model manager: removes every unreferenced model, releases the path and the lock
GEModelManager::~GEModelManager()
{
    ee_ge_modman_free(this->mRust);
}

// FUNCTION: LLE 0x1002073d
// Creates a new model registered under a numbered name ("GEModelManager::CreateModel(name_N)")
GEModel* GEModelManager::CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                     unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags)
{
    GEModelCreateArgs theArgs = {inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount,
                                 inSubModelCount, inFlags};
    return (GEModel*)ee_ge_modman_create(this->mRust, &theArgs, inName);
}

// FUNCTION: LLE 0x100207d6
// Creates a model and attaches it to a parent model as the sub-model at an index (the parent holds it)
GEModel* GEModelManager::CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                        unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags,
                                        unsigned long inIndex, GEModel* inParent)
{
    GEModelCreateArgs theArgs = {inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount,
                                 inSubModelCount, inFlags};
    return (GEModel*)ee_ge_modman_create_sub(this->mRust, &theArgs, inIndex, inParent);
}

// FUNCTION: LLE 0x10020845
// Writes the list of loaded models and their reference counts to a file
void GEModelManager::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    ee_ge_modman_dump(this->mRust, &inFile);
}

unsigned long GEModelManager::GetMemoryFootprint()
{
    return ee_ge_modman_footprint(this->mRust);
}

// FUNCTION: LLE 0x100208b0
// Returns the loaded model of that name, else finds its glTF (.glb, .gltf), stores it and registers it under the
// name; NULL when there is none or it does not load. The cache's key is the name as given ("models\x"; the
// original's was "<name>.cem")
GEModel* GEModelManager::LoadModel(const UString& inName, std::map<unsigned long, long>& ioTextures)
{
    return (GEModel*)ee_ge_modman_load(this->mRust, inName, &ioTextures);
}

// FUNCTION: LLE 0x100209dd
// Releases every model nobody else references; returns 5 when some are still in use
long GEModelManager::RemoveAllModels()
{
    return ee_ge_modman_remove_all(this->mRust);
}

// FUNCTION: LLE 0x10020a53
// Releases a model if nobody else references it; 3 when it is unknown, 5 when it is still in use
long GEModelManager::RemoveModel(GEModel* inModel)
{
    return ee_ge_modman_remove(this->mRust, inModel);
}

// FUNCTION: LLE 0x10020ad1
// Replaces the model search path, keeping a reference to the new one
void GEModelManager::SetModelPath(FSPath* inPath)
{
    ee_ge_modman_set_path(this->mRust, inPath);
}
