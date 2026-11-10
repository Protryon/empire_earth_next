// C:\EEX\Empire Earth\Actions\EEAAddChatMessage.cpp (file name inferred: no __FILE__; RTTI class
// EEAAddChatMessage, vtable 0x43e340). The first unit of the link: 0x1000..0x16c0 (EEABootClientFromCanceledGame's
// Virtual4 starts at 0x16c0). A `??_I` helper COMDAT, the U2D header keys' `$E` chains, five unit keys (only this
// object has them), the UList node pools' template-static chain and their UMemoryPool COMDATs, then Virtual4.
// UNIT-FLAGS: -O1 -G6 -MD -GR -DLLE_CURRENT_HEADERS
#include <windows.h>
#include <mbstring.h>
#include <algorithm>

#include "lle/LowLevelEngine.h"
#include "lle/U2DPoint.h"
#include "lle/U2DRectangle.h"
#include "lle/U2DPolygon.h"
#include "lle/GEColor.h"
#include "lle/GESurface.h"
#include "lle/GETextureCoordinate.h"
#include "lle/GETextureManager.h"
#include "lle/GETexture.h"
#include "GEWindow.h"
#include "lle/GEScreen.h"
#include "lle/GEViewport.h"
#include "lle/GEVertexArray.h"
#include "lle/GERasterizer.h"
#include "lle/GERhombus.h"
#include "lle/GEBitmap.h"
#include "lle/FSFileSpec.h"
#include "lle/FSArchive.h"
#include "lle/FSPath.h"
#include "lle/TMPoll.h"
#include "lle/TSCriticalSection.h"
#include "lle/TSScheduler.h"
#include "lle/Input System.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/UITextLabel.h"
#include "lle/UISlider.h"
#include "lle/UIScrollBar.h"
#include "lle/UMemoryPool.h"
#include "lle/UList.h"
#include "lle/U2DSparseArray.h"

#include "ee/EmpireEarth.h"
#include "ee/EEUIManager.h"
#include "ee/EEAction.h"
#include "ee/EEAAddChatMessage.h"

// unit keys (provisional names; unreferenced in the exe: only their chains use them)
const UString kDefaultName("default");
const UString kCivExtension(".civ");
const UString kCivilizationsFolder("Civilizations");
const UString kDefaultFolder("Default");
const UString kUsersFolder("Users");

// FUNCTION: EXE 0x401000
// Calls a destructor on each element of an array, last to first (compiler vector-destructor helper)
// (??_I@YGXPAXIHP6EX0@Z@Z)

// SYNTHETIC: EXE 0x40102a
// (_$E7)

// SYNTHETIC: EXE 0x401034
// (_$E4)

// SYNTHETIC: EXE 0x401045
// (_$E6)

// SYNTHETIC: EXE 0x401051
// (_$E5)

// SYNTHETIC: EXE 0x40105c
// (_$E12)

// SYNTHETIC: EXE 0x401066
// (_$E9)

// SYNTHETIC: EXE 0x401077
// (_$E11)

// SYNTHETIC: EXE 0x401083
// (_$E10)

// SYNTHETIC: EXE 0x40108e
// (_$E17)

// SYNTHETIC: EXE 0x401098
// (_$E14)

// SYNTHETIC: EXE 0x4010a9
// (_$E16)

// SYNTHETIC: EXE 0x4010b5
// (_$E15)

// SYNTHETIC: EXE 0x4010c0
// (_$E22)

// SYNTHETIC: EXE 0x4010ca
// (_$E19)

// SYNTHETIC: EXE 0x4010db
// (_$E21)

// SYNTHETIC: EXE 0x4010e7
// (_$E20)

// SYNTHETIC: EXE 0x4010f2
// (_$E27)

// SYNTHETIC: EXE 0x4010fc
// (_$E24)

// SYNTHETIC: EXE 0x40110d
// (_$E26)

// SYNTHETIC: EXE 0x401119
// (_$E25)

// SYNTHETIC: EXE 0x401124
// (_$E32)

// SYNTHETIC: EXE 0x40112e
// (_$E29)

// SYNTHETIC: EXE 0x40113f
// (_$E31)

// SYNTHETIC: EXE 0x40114b
// (_$E30)

// SYNTHETIC: EXE 0x401156
// (_$E37)

// SYNTHETIC: EXE 0x401160
// (_$E34)

// SYNTHETIC: EXE 0x401171
// (_$E36)

// SYNTHETIC: EXE 0x40117d
// (_$E35)

// SYNTHETIC: EXE 0x401188
// (_$E83)

// SYNTHETIC: EXE 0x401192
// (_$E80)

// SYNTHETIC: EXE 0x4011a3
// (_$E82)

// SYNTHETIC: EXE 0x4011af
// (_$E81)

// SYNTHETIC: EXE 0x4011ba
// (_$E88)

// SYNTHETIC: EXE 0x4011c4
// (_$E85)

// SYNTHETIC: EXE 0x4011d5
// (_$E87)

// SYNTHETIC: EXE 0x4011e1
// (_$E86)

// SYNTHETIC: EXE 0x4011ec
// (_$E93)

// SYNTHETIC: EXE 0x4011f6
// (_$E90)

// SYNTHETIC: EXE 0x401207
// (_$E92)

// SYNTHETIC: EXE 0x401213
// (_$E91)

// SYNTHETIC: EXE 0x40121e
// (_$E98)

// SYNTHETIC: EXE 0x401228
// (_$E95)

// SYNTHETIC: EXE 0x401239
// (_$E97)

// SYNTHETIC: EXE 0x401245
// (_$E96)

// SYNTHETIC: EXE 0x401250
// (_$E103)

// SYNTHETIC: EXE 0x40125a
// (_$E100)

// SYNTHETIC: EXE 0x40126b
// (_$E102)

// SYNTHETIC: EXE 0x401277
// (_$E101)

// SYNTHETIC: EXE 0x401282
// (_$E121)

// SYNTHETIC: EXE 0x4012aa
// (?sNodePool@?$UList@PAVGELODVertex@@@@$D)

// SYNTHETIC: EXE 0x4012c5
// (_$E107)

// SYNTHETIC: EXE 0x4012d1
// (?sNodePool@?$UList@PAVGELODPolygon@@@@$D)

// SYNTHETIC: EXE 0x4012ec
// (_$E110)

// SYNTHETIC: EXE 0x4012f8
// (?sNodePool@?$UList@PAVGELODMetaVertex@@@@$D)

// SYNTHETIC: EXE 0x401313
// (_$E113)

// SYNTHETIC: EXE 0x40131f
// (?sNodePool@?$UList@VU2DSparseArrayPointContainerNode@@@@$D)

// SYNTHETIC: EXE 0x40133a
// (_$E116)

// SYNTHETIC: EXE 0x401346
// (?sNodePool@?$UList@VU2DSparseArrayPointContainerNode@@@@$E)

// FUNCTION: EXE 0x40136b
// Destroys the sparse-array node pool and frees it when asked (scalar deleting destructor)
// (??_G?$UMemoryPool@U?$UListNode@VU2DSparseArrayPointContainerNode@@@@@@UAEPAXI@Z)

// FUNCTION: EXE 0x40138d
// Frees the sparse-array node pool's chunks once none of their nodes is in use; returns whether it released them
// (?Release@?$UMemoryPool@U?$UListNode@VU2DSparseArrayPointContainerNode@@@@@@QAE_NXZ)

// FUNCTION: EXE 0x4013f1
// Destroys a chunk of the sparse-array node pool, freeing its node block
// (??1?$UMemoryPoolChunk@U?$UListNode@VU2DSparseArrayPointContainerNode@@@@@@QAE@XZ)

// SYNTHETIC: EXE 0x40140c
// (?sNodePool@?$UList@PAVGELODMetaVertex@@@@$E)

// FUNCTION: EXE 0x401431
// Destroys the LOD meta-vertex node pool and frees it when asked (scalar deleting destructor)
// (??_G?$UMemoryPool@U?$UListNode@PAVGELODMetaVertex@@@@@@UAEPAXI@Z)

// SYNTHETIC: EXE 0x401453
// (?sNodePool@?$UList@PAVGELODPolygon@@@@$E)

// FUNCTION: EXE 0x401478
// Destroys the LOD polygon node pool and frees it when asked (scalar deleting destructor)
// (??_G?$UMemoryPool@U?$UListNode@PAVGELODPolygon@@@@@@UAEPAXI@Z)

// SYNTHETIC: EXE 0x40149a
// (?sNodePool@?$UList@PAVGELODVertex@@@@$E)

// FUNCTION: EXE 0x4014bf
// Destroys the LOD vertex node pool and frees it when asked (scalar deleting destructor)
// (??_G?$UMemoryPool@U?$UListNode@PAVGELODVertex@@@@@@UAEPAXI@Z)

// FUNCTION: EXE 0x4014e1
// Creates the empty LOD vertex list-node pool
// (??0?$UMemoryPool@U?$UListNode@PAVGELODVertex@@@@@@QAE@XZ)

// FUNCTION: EXE 0x40151b
// Creates the empty LOD polygon list-node pool
// (??0?$UMemoryPool@U?$UListNode@PAVGELODPolygon@@@@@@QAE@XZ)

// FUNCTION: EXE 0x401555
// Creates the empty LOD meta-vertex list-node pool
// (??0?$UMemoryPool@U?$UListNode@PAVGELODMetaVertex@@@@@@QAE@XZ)

// FUNCTION: EXE 0x40158f
// Creates the empty sparse-array list-node pool
// (??0?$UMemoryPool@U?$UListNode@VU2DSparseArrayPointContainerNode@@@@@@QAE@XZ)

// FUNCTION: EXE 0x4015c9
// Appends a chat line to the lobby's chat box, keeping it scrolled to the end when it was already there or it is ours
void EEAAddChatMessage::Virtual4()
{
    if (gUIManager->GetCurrentForm() != gUIManager->GetFormByID(kFormLobby))
        return;
    UITextLabel* theChat = static_cast<UITextLabel*>(gUIManager->GetFormByID(kFormLobby)->GetControl(kLobbyChatPreviousMessages));
    bool theAtEnd;
    if (!this->mFromLocalPlayer && theChat->GetScrollBar() &&
        theChat->GetScrollBar()->GetSlider()->GetMaxRange() !=
            theChat->GetScrollBar()->GetSlider()->GetCurrentRangeValue())
        theAtEnd = false;
    else
        theAtEnd = true;
    theChat->AppendText(this->mText);
    if (theAtEnd == true && theChat->GetScrollBar())
        theChat->SetStartingDisplayLine(theChat->GetScrollBar()->GetSlider()->GetMaxRange());
}

// ICF-FOLDED: EXE 0x5720c6
// (no address of its own: folded into the shared `ret` body the vtable slot points at)
void EEAAddChatMessage::Virtual8()
{
}
