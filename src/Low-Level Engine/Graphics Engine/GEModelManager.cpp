// C:\EEX\Low-Level Engine\Graphics Engine\GEModelManager.cpp
// Unit flags (calibrated, docs/CALIBRATION.md "lle"): -O1 -G6 -MD -GR -DLOWLEVELENGINE_EXPORTS
//
// 0x100206c7..0x10020d20: GEModelManager's members, then the map<UString, GEModel*> COMDATs and the UList
// template-static init pairs. No U2D keys. The ESVERIFY is the original's line 39 (0x27).
#include <windows.h>
#include <stdio.h>
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
#include "GEOpenFormats.h"   // GEFindModelFile: a model's .glb/.gltf in place of its .cem

static unsigned long sModelNumber = 1;      // 0x100d953c: appended to created models' names
// Release-build trace of a model file that failed to load or close: compiled out, but its arguments (the file's
// volume, directory and name) are still evaluated (provisional name).
inline void GEReportModelFile(const UString& inVolume, const UString& inDirectory, const UString& inFile)
{
}

//
// FUNCTION: LLE 0x100206c7
// Creates the model manager with a model search path and its lock
GEModelManager::GEModelManager(FSPath* inPath)
{
    this->mMemoryFootprint = 0;
    this->mPath = inPath;
    if (inPath)
        inPath->Reference();
    ESVERIFY_AT(TSScheduler::CreateCriticalSection(this->mCriticalSection) == keseOK, 39);
}

// FUNCTION: LLE 0x10020717
// Destroys the model manager: removes every unreferenced model, releases the path and the lock
GEModelManager::~GEModelManager()
{
    this->RemoveAllModels();
    if (this->mPath)
        this->mPath->Dereference();
    TSScheduler::DestroyCriticalSection(this->mCriticalSection);
}

// FUNCTION: LLE 0x1002073d
// Creates a new model registered under a numbered name ("GEModelManager::CreateModel(name_N)")
GEModel* GEModelManager::CreateModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                     unsigned long inFrameCount, unsigned long inSubModelCount, const UString& inName, unsigned long inFlags)
{
    UString theName;
    GEModel* theModel;
    this->mCriticalSection->Enter();
    sprintf(theName, "GEModelManager::CreateModel(%s_%d)", (const char*)inName, sModelNumber++);
    theModel = new GEModel(inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount, inSubModelCount, theName, inFlags, this);
    this->mModels[theName] = theModel;
    this->mCriticalSection->Leave();
    return theModel;
}

// FUNCTION: LLE 0x100207d6
// Creates a model and attaches it to a parent model as the sub-model at an index (the parent holds it)
GEModel* GEModelManager::CreateSubModel(unsigned long inPolygonCount, unsigned long inVertexCount, unsigned long inMaterialCount, unsigned long inTagPointCount,
                                        unsigned long inFrameCount, unsigned long inSubModelCount, unsigned long inFlags,
                                        unsigned long inIndex, GEModel* inParent)
{
    GEModel* theModel;
    this->mCriticalSection->Enter();
    theModel = new GEModel(inPolygonCount, inVertexCount, inMaterialCount, inTagPointCount, inFrameCount, inSubModelCount, inParent->mName, inFlags, this);
    inParent->SetSubModel(theModel, inIndex);
    theModel->UReferenceCounter::Dereference();
    this->mCriticalSection->Leave();
    return theModel;
}

// FUNCTION: LLE 0x10020845
// Writes the list of loaded models and their reference counts to a file
void GEModelManager::DumpAllLoadedMedia(FSFileSpec& inFile)
{
    std::map<UString, GEModel*>::iterator theIter;
    this->mCriticalSection->Enter();
    fprintf(inFile, "\n\n*********Model Manager - Current Loaded Files In Memory*********\n\n");
    for (theIter = this->mModels.begin(); theIter != this->mModels.end(); theIter++)
        fprintf(inFile, "The %s model file is loaded in memory with a reference count of %d.\n",
                (const char*)theIter->first, theIter->second->mReferenceCount);
    this->mCriticalSection->Leave();
}

// FUNCTION: LLE 0x100208b0
// Returns the loaded model of that name (".cem" added), else finds, loads and registers it; NULL on failure
GEModel* GEModelManager::LoadModel(const UString& inName, std::map<unsigned long, long>& ioTextures)
{
    UString theFileName = inName + ".cem";
    FSFileSpec* theFile = NULL;
    GEModel* theModel = NULL;
    std::map<UString, GEModel*>::iterator theIter;
    TSScheduler::GetCurrentThreadIndex();
    this->mCriticalSection->Enter();
    theIter = this->mModels.find(theFileName);
    if (theIter != this->mModels.end())
        theModel = theIter->second;
    else if (GEFindModelFile(this->mPath, theFileName, theFile) == kErrorOK) {   // the open format first
        if (theFile->Open(kFileAccessRead) == kErrorOK) {
            theModel = new GEModel(this);
            if (theModel->Load(*theFile, ioTextures, true) == kErrorOK) {
                this->mModels[theFileName] = theModel;
                this->mMemoryFootprint += theModel->SizeOf();
            } else {
                GEReportModelFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
                theModel->UReferenceCounter::Dereference();
                theModel = NULL;
            }
            if (theFile->Close() != kErrorOK)
                GEReportModelFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
        } else
            GEReportModelFile(theFile->GetVolume(), theFile->GetDirectory(), theFile->GetFile());
        delete theFile;
    }
    this->mCriticalSection->Leave();
    TSScheduler::GetCurrentThreadIndex();
    return theModel;
}

// FUNCTION: LLE 0x100209dd
// Releases every model nobody else references; returns 5 when some are still in use
long GEModelManager::RemoveAllModels()
{
    long theResult = kErrorOK;
    std::map<UString, GEModel*>::iterator theIter;
    GEModel* theModel;
    this->mCriticalSection->Enter();
    for (theIter = this->mModels.begin(); theIter != this->mModels.end();) {
        theModel = theIter->second;
        if (theModel->mReferenceCount == 1) {
            this->mMemoryFootprint -= theModel->SizeOf();
            theModel->UReferenceCounter::Dereference();
            theIter = this->mModels.erase(theIter);
        } else {
            theResult = 5;
            theIter++;
        }
    }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10020a53
// Releases a model if nobody else references it; 3 when it is unknown, 5 when it is still in use
long GEModelManager::RemoveModel(GEModel* inModel)
{
    long theResult = 3;
    std::map<UString, GEModel*>::iterator theIter;
    this->mCriticalSection->Enter();
    for (theIter = this->mModels.begin(); theIter != this->mModels.end(); theIter++)
        if (theIter->second == inModel) {
            if (inModel->mReferenceCount == 1) {
                this->mMemoryFootprint -= inModel->SizeOf();
                inModel->UReferenceCounter::Dereference();
                this->mModels.erase(theIter);
                theResult = kErrorOK;
            } else
                theResult = 5;
            break;
        }
    this->mCriticalSection->Leave();
    return theResult;
}

// FUNCTION: LLE 0x10020ad1
// Replaces the model search path, keeping a reference to the new one
void GEModelManager::SetModelPath(FSPath* inPath)
{
    if (this->mPath)
        this->mPath->Dereference();
    this->mPath = inPath;
    if (inPath)
        inPath->Reference();
}

// FUNCTION: LLE 0x10020af2
// Runs the self check on every loaded model (results ignored)
void GEModelManager::VerifyModels()
{
    std::map<UString, GEModel*>::iterator theIter = this->mModels.begin();
    this->mCriticalSection->Enter();
    for (; theIter != this->mModels.end(); theIter++)
        theIter->second->Verify();
    this->mCriticalSection->Leave();
}

// Compiler-generated for this unit (names in analysis/lle/names.txt):

// Compiler-generated, continued:
// TEMPLATE: LLE 0x10020b31
// TEMPLATE: LLE 0x10020b6a
// TEMPLATE: LLE 0x10020b95
// TEMPLATE: LLE 0x10020bd8
// TEMPLATE: LLE 0x10020c9d

// UList<GELOD*>::sNodePool init wrapper and atexit registrars (GELOD.h through GEModel.h):
// SYNTHETIC: LLE 0x10020cde
// SYNTHETIC: LLE 0x10020cfc
// SYNTHETIC: LLE 0x10020d08
// SYNTHETIC: LLE 0x10020d14
