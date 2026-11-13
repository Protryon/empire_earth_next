// C:\EEX\Empire Earth\User Interface Thread\EEUIPGSLineGraph.cpp (file name inferred: no __FILE__; RTTI class
// EEUIPGSLineGraph, vtable 0x44b95c). The post-game summary's line graph: 0x276442..0x276631 (ctor, the inline
// GetGraphAt, ??_G, dtor, the slots in address order). Its own unit: alphabetical link order puts EEUIPGSLineGraph.cpp
// between EEUIMiniMap.cpp (ends 0x276442) and EEUserInterface.cpp (its ctor, the first function after this range,
// carries that unit's __FILE__ 0x276631); none of these members is called from EEUserInterface.cpp, and the class has
// one .cpp like every other EEUI* control. Unit flags: the exe module set.
#include <windows.h>
#include <mbstring.h>
#include <algorithm>
#include "lle/LowLevelEngine.h"
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
#include "lle/SESample.h"
#include "lle/SESMInstanceNode.h"
#include "lle/SESoundManager.h"
#include "lle/Input System.h"
#include "lle/ISKeyboard.h"
#include "lle/ISMouse.h"
#include "lle/UIControl.h"
#include "lle/UIEvent.h"
#include "lle/UIEntityGroup.h"
#include "lle/UIBackground.h"
#include "lle/UIFontTable.h"
#include "lle/UIHotKey.h"
#include "lle/UIIMECandidate.h"
#include "lle/UIForm.h"
#include "lle/UIManager.h"
#include "lle/U2DPoint.h"
#include "lle/UGraph.h"
#include "lle/UIGraph.h"
#include "lle/UILineGraph.h"
#include "ee/RTSWorld.h"
#include "ee/EEWorld.h"
#include "ee/EEPhysics.h"
#include "ee/EEAreaUtilities.h"
#include "ee/EEPlayer.h"
#include "ee/EmpireEarth.h"
#include "ee/EEUIPGSLineGraph.h"
#include "rust/ee_screens.h"   // ee_screens_pgs_format_x/_y (ee_screens post_game.rs)
#include <string>

// FUNCTION: EXE 0x676442
// Creates the post-game summary's line graph
EEUIPGSLineGraph::EEUIPGSLineGraph(unsigned long inId, const GERectangle& inRectangle, UIForm* inForm,
                                   unsigned long inIndex, unsigned long inBorderThickness)
    : UILineGraph(inId, inRectangle, inForm, inIndex, inBorderThickness)
{
}

// FUNCTION: EXE 0x676485
// Returns one of the stored graphs by index
// (GetGraphAt, inline in the class: emitted with the vtable after the ctor)

// SYNTHETIC: EXE 0x676496
// (the scalar deleting destructor, emitted with the vtable; slot EEUIPGSLineGraph+0x0)

// FUNCTION: EXE 0x6764b2
// Destroys the line graph
EEUIPGSLineGraph::~EEUIPGSLineGraph()
{
}

// FUNCTION: EXE 0x6764e0
// Adds a statistics series (time, value points) in a colour to one of the stored graphs
// NEAR (owner-waived): exact only with long point fields, which Load/Save 0x16d76c/0x16d86b rule out (a long local
// copy and member-wise stores were probed and are further off). The original loads the fields in store order
void EEUIPGSLineGraph::AddGraphSeries(const std::vector<EEStatisticsPoint>& inPoints, unsigned long inGraph,
                                      GEColor inColor)
{
    UGraphSeries theSeries;
    for (unsigned long i = 0; i < inPoints.size(); i++) {
        U2DPoint<long> thePoint(inPoints[i].mTime, inPoints[i].mValue);
        theSeries.AddDataPoint(thePoint);
    }
    theSeries.SetColor(inColor);
    this->mGraphs[inGraph].AddSeries(theSeries);
}

// FUNCTION: EXE 0x676565
// Draws the line graph
long EEUIPGSLineGraph::DrawPolygons()
{
    return UILineGraph::DrawPolygons();
}

// FUNCTION: EXE 0x67656b
// Draws one series of the graph
long EEUIPGSLineGraph::DrawSeries(const UGraphSeries& inSeries)
{
    return UILineGraph::DrawSeries(inSeries);
}

// FUNCTION: EXE 0x676578
// Formats a y axis value as a rounded integer (ee_screens post_game.rs format_y)
void EEUIPGSLineGraph::FormatYValues(float inValue, UWideString& outString)
{
    char theText[32];
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    size_t theLength = ee_screens_pgs_format_y(theBits, (uint8_t*)theText, sizeof(theText));
    outString = std::string(theText, theLength).c_str();
}

// FUNCTION: EXE 0x67659d
// Formats an x axis value (game time) as minutes:seconds, at least 0:01 (ee_screens post_game.rs format_x)
void EEUIPGSLineGraph::FormatXValues(float inValue, UWideString& outString)
{
    char theText[32];
    uint32_t theBits;
    memcpy(&theBits, &inValue, 4);
    size_t theLength = ee_screens_pgs_format_x(theBits, (uint32_t)gPhysics->GetGameTime(), (uint8_t*)theText,
                                               sizeof(theText));
    outString = std::string(theText, theLength).c_str();
}

// FUNCTION: EXE 0x6765eb
// Shows one of the stored graphs
void EEUIPGSLineGraph::ShowGraph(unsigned long inGraph)
{
    this->mGraph = this->mGraphs[inGraph];
    this->SetDirty(true);
}

// FUNCTION: EXE 0x676616
// Stores a graph at an index
void EEUIPGSLineGraph::SetGraphAt(const UGraph& inGraph, unsigned long inIndex)
{
    this->mGraphs[inIndex] = inGraph;
}
