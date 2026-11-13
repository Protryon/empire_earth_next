// Low-Level Engine.dll API as seen by its importers (inferred header).
// Declarations are proven from the importers' mangled names (exact signatures); class layouts are not proven
// and only declared where an importer's codegen shows them. Split into real per-subsystem headers once the
// engine's own source layout is known (C:\EEX\Low-Level Engine\...).
//
// Include <windows.h> first: the original headers were seen through its A/W macros
// (TSScheduler::CreateEvent is exported as CreateEventA).
#ifndef LLE_LOWLEVELENGINE_H
#define LLE_LOWLEVELENGINE_H

#include "lle/LowLevelEngineCore.h"

// ---- Strings: the engine's per-class headers
#include "lle/UString.h"

#include "lle/UWideString.h"

// ---- Error system checksums (ESChecksum.cpp)
#include "lle/ESChecksum.h"

// ---- Random
#include "lle/URandom.h"

// ---- Geometry engine (GE) basic types
// the engine's per-class headers (GE2DPoint.cpp block 0x1000c129: UMFastMath, GE2DPoint, GEVector, GE3DPoint,
// GE3DLine; then GE3DPlane)
#include "lle/UMFastMath.h"
#include "lle/GE2DPoint.h"
#include "lle/GEVector.h"
#include "lle/GE3DPoint.h"
#include "lle/GE3DLine.h"
#include "lle/GE3DPlane.h"

#include "lle/GERectangle.h"

#include "lle/GEOrientation.h"
#include "lle/GETransformation.h"

// ---- Threading / scheduling (TS): the engine's per-class headers
#include "lle/TSCriticalSection.h"
#include "lle/TSEvent.h"
#include "lle/TSReadWriteLock.h"
#include "lle/TSThread.h"
#include "lle/TSScheduler.h"

LLE_API UString operator+(const UString& inLeft, const UString& inRight);
LLE_API UString operator+(const UString& inLeft, const char* inRight);

// ---- Reference counting
#include "lle/UReferenceCounter.h"

// ---- File system (FS): the engine's per-class headers (vtables from RTTI, layouts from the engine's own code)
#include "lle/FSFile.h"
#include "lle/FSBasicFile.h"
#include "lle/FSFileSpec.h"
#include "lle/FSMemoryFile.h"
LLE_API void FSInitialize(const UString& inPath);     // proven ?FSInitialize@@YAXABVUString@@@Z (exe import)

// ---- Sound engine (SE): the engine's per-class headers. The plug-ins see SESample (and through it SEMixer and
// SEConductor). Engine units include the SE headers they use themselves: the SE classes' static data members would
// otherwise shift every engine unit's `$E` numbering (one number per static member a unit sees, see TSScheduler.h).
#ifndef LOWLEVELENGINE_EXPORTS
#include "lle/SESample.h"
#endif
class GEWindow;
LLE_API long SEInitialize(const FSFileSpec& inDirectory, GEWindow* inWindow);   // Sound Engine.cpp

// ---- Network engine (NE)
#include "lle/NEProtocolConfiguration.h"

#endif
