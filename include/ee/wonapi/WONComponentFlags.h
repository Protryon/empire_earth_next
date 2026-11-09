// ee/wonapi/WONComponentFlags.h. Named integer constants; names PROVISIONAL unless the comment says T1.
#ifndef EE_WONCOMPONENTFLAGS_H
#define EE_WONCOMPONENTFLAGS_H

// Component::mComponentBits bits; names = the resource files' COMPONENTFLAGS keywords (HasFocus: the accessor Component::HasFocus)
enum ComponentFlag {
    ComponentFlag_Invisible = 0x1,      // T1 ComponentConfig COMPONENTFLAGS keyword "Invisible" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_WantFocus = 0x8,      // T1 ComponentConfig COMPONENTFLAGS keyword "WantFocus" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_HasFocus = 0x10,      // T1 Component::HasFocus() tests it
    ComponentFlag_ParentDrag = 0x20,    // T1 ComponentConfig COMPONENTFLAGS keyword "ParentDrag" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_NoParentCursor = 0x40,  // T1 ComponentConfig COMPONENTFLAGS keyword "NoParentCursor" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_Disabled = 0x80,      // T1 ComponentConfig COMPONENTFLAGS keyword "Disabled" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_GrabBG = 0x100,       // T1 ComponentConfig COMPONENTFLAGS keyword "GrabBG" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_Translucent = 0x200,  // T1 ComponentConfig COMPONENTFLAGS keyword "Translucent" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_NoInput = 0x800,      // T1 ComponentConfig COMPONENTFLAGS keyword "NoInput" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_AlwaysOnTop = 0x1000,  // T1 ComponentConfig COMPONENTFLAGS keyword "AlwaysOnTop" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_Clip = 0x2000,        // T1 ComponentConfig COMPONENTFLAGS keyword "Clip" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_WantTabFocus = 0x4000,  // T1 ComponentConfig COMPONENTFLAGS keyword "WantTabFocus" (ComponentConfigResource.cpp HandleInstruction)
    ComponentFlag_DebugBG = 0x20000,    // T1 ComponentConfig COMPONENTFLAGS keyword "DebugBG" (ComponentConfigResource.cpp HandleInstruction)
};

#endif // EE_WONCOMPONENTFLAGS_H
