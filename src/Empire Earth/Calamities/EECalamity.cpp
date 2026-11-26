// C:\EEX\Empire Earth\Calamities\EECalamity.cpp (file name inferred: no __FILE__; the class's save keys, ctors, members
// and the vector load/save templates it instantiates first). One active calamity: 0xacb75..0xadcd9 (init chains of
// the 21 save keys 0xacb75..0xad0f4). Unit flags: the exe's module set.
// Data (.rdata, in order): -10000.0f 0x43c690 (not referenced by the unit's code), the vtable 0x43c698 (COL
// 0x43c694). (.data): the key strings 0x4b90d8.., type descriptors, the dump formats 0x4b9324..; (.bss): the keys
// 0x4e5f58..0x4e5fa0 and their guard bytes.
#include <windows.h>
#include <stdint.h>
#include <stdio.h>

#include "lle/LowLevelEngine.h"

#include "ee/EEEffectManager.h"
#include "ee/EECalamity.h"

const UString EECalamity::mEffectGenerationStringID("EffectGenerationStringID");

// (the 21 keys' init chains 0x4acb75..0x4ad0f4: registrar, ctor, atexit registrar, guarded dtor each)

