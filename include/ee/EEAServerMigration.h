// C:\EEX\Empire Earth\Actions\EEAServerMigration.h (RECONSTRUCTION; file name inferred). The host
// migration action (RTTI EEAServerMigration > EEAction; vtable 0x439090; no members): EEClient::ClientSlot4C 0xb7fa5
// posts it when the server is lost. ctor 0x4a0b, ??_G 0x4764 (ICF: EEAScroll's), Virtual4 0x4a1f (picks the new host:
// the local player becomes a "Migrated Server" EEServer, the others reconnect to it). Names are provisional.
// Requires ee/EEAction.h.
#ifndef EE_EEASERVERMIGRATION_H
#define EE_EEASERVERMIGRATION_H

#include "ee/EEAction.h"

class EEAServerMigration : public EEAction {
public:
    EEAServerMigration();                   // 0x4a0b
    virtual void Virtual4();                // +0x04 0x4a1f
    virtual void Virtual8();                // +0x08 (the shared empty slot 0x1720c6)
};

#endif
