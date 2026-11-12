// The WONAPI GUI's platform backend offscreen (WONLobbyPlatform.h): a PlatWindowManager and a window on a 640x480
// lobby surface, a dialog background, labels, buttons, a check box, a radio button, an input box, a list area and a
// text area laid out in it, painted and composited; the frame is written as a BMP (argument 1, default
// won_gui_test.bmp) to look at. Also checks the GDI rules of the graphics hooks (end points, rectangle edges, mask
// blits, blending) on a small image. Links with the won module and the platform layer. Prints one line per failed
// check; the exit code is the number of failed checks.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>
#include "lle/LowLevelEngine.h"
#include "ee/EEWonNet.h"
#include "ee/wonapi/WONLobbyResource.h"
#include "ee/wonapi/WONLobbyGUI.h"
#include "ee/wonapi/WONLobbyMSBorderPainters.h"
#include "ee/wonapi/WONLobbyMSPainters.h"
#include "ee/wonapi/WONLobbyTextAreaConfig.h"
#include "ee/wonapi/WONLobbyImageDecoder.h"
#include "ee/wonapi/WONLobbyPlatform.h"
#include "platform/PlatFont.h"

using namespace WONAPI;

// the game defines it (EEWonNet.cpp): the empty wide string GUIString shares
wchar_t gEmptyWideString[2];

static int sFailures;
#define CHECK(c) do { if (!(c)) { sFailures++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static bool WriteBMP(const char* inPath, const uint32_t* inPixels, int inWidth, int inHeight, int inPitch)
{
    FILE* f = fopen(inPath, "wb");
    if (f == NULL)
        return false;
    uint32_t theImage = (uint32_t)inWidth * inHeight * 4;
    unsigned char h[54] = {'B', 'M'};
    uint32_t theFile = 54 + theImage;
    memcpy(h + 2, &theFile, 4);
    uint32_t v = 54; memcpy(h + 10, &v, 4);
    v = 40; memcpy(h + 14, &v, 4);
    int32_t w = inWidth, hh = -inHeight; memcpy(h + 18, &w, 4); memcpy(h + 22, &hh, 4);
    h[26] = 1; h[28] = 32;
    memcpy(h + 34, &theImage, 4);
    fwrite(h, 1, 54, f);
    for (int y = 0; y < inHeight; y++)
        fwrite((const char*)inPixels + (size_t)y * inPitch, 4, inWidth, f);
    fclose(f);
    return true;
}

static uint32_t Pixel(NativeImage* inImage, int x, int y)
{
    PlatNativeImage* p = static_cast<PlatNativeImage*>(inImage);
    return p->GetPixels()[y * p->GetPitchPixels() + x] & 0xFFFFFF;
}

static void CheckGraphicsRules(Window* inWindow)
{
    SmartPtr<NativeImage> theImage = inWindow->CreateImage(16, 16);
    Graphics& g = theImage->GetGraphics();
    CHECK(Pixel(theImage.get(), 3, 3) == 0);                 // a new bitmap is black
    g.SetColor(0xFF0000);
    g.DrawLine(1, 1, 5, 1);                                  // both end points
    CHECK(Pixel(theImage.get(), 1, 1) == 0xFF0000 && Pixel(theImage.get(), 5, 1) == 0xFF0000);
    CHECK(Pixel(theImage.get(), 6, 1) == 0);
    g.SetColor(0x00FF00);
    g.DrawRect(2, 4, 4, 3);                                  // x..x+width-1
    CHECK(Pixel(theImage.get(), 2, 4) == 0x00FF00 && Pixel(theImage.get(), 5, 6) == 0x00FF00);
    CHECK(Pixel(theImage.get(), 6, 4) == 0 && Pixel(theImage.get(), 3, 5) == 0);
    g.SetColor(0x0000FF);
    g.FillRect(10, 10, 2, 2);
    CHECK(Pixel(theImage.get(), 11, 11) == 0x0000FF && Pixel(theImage.get(), 12, 11) == 0);
    g.SetColor(0xFFFFFF);
    g.FillRectBlend(10, 10, 1, 1, 128);                      // 255*128/255 + 0 on blue's 0 channels, 255 on blue
    CHECK(Pixel(theImage.get(), 10, 10) == 0x8080FF);
    g.PushClipRect(0, 0, 4, 4);
    g.SetColor(0x123456);
    g.FillRect(0, 0, 16, 16);
    g.PopClipRect();
    CHECK(Pixel(theImage.get(), 3, 3) == 0x123456 && Pixel(theImage.get(), 4, 0) == 0 && Pixel(theImage.get(), 13, 13) == 0);
    // a transparent raw image: alpha 0 pixels keep the destination
    SmartPtr<RawImage32> theRaw = new RawImage32(2, 1);
    theRaw->mImageData[0] = 0xFF00FF00;
    theRaw->mImageData[1] = 0x00FF0000;
    theRaw->SetDoTransparency(true);
    g.DrawImage(theRaw.get(), 8, 0);
    CHECK(Pixel(theImage.get(), 8, 0) == 0x00FF00 && Pixel(theImage.get(), 9, 0) == 0);
    SmartPtr<RawImage32> theBack = static_cast<PlatNativeImage*>(theImage.get())->GetRaw();
    CHECK((theBack->mImageData[16 + 1] >> 24) == 0);       // GetDIBits: the alpha byte reads 0
}

int main(int argc, char** argv)
{
    const char* thePath = argc > 1 ? argv[1] : "won_gui_test.bmp";
    CHECK(PlatFontRegisterBuiltIn() != 0);
    SmartPtr<PlatWindowManager> theManager = new PlatWindowManager;
    theManager->SetSurfaceSize(640, 480);
    theManager->GetSystemColorScheme();

    SmartPtr<Window> theWindow = CreateNativeWindow();
    theWindow->Create(WONRectangle(0, 0, 100, 100));
    RootContainer* theRoot = theWindow->mRoot.get();
    CHECK(theRoot->Width() == 640 && theRoot->Height() == 480);
    theRoot->SetColorScheme(theManager->mColorScheme.get());
    CheckGraphicsRules(theWindow.get());

    MSDlgBack* theBack = new MSDlgBack;
    theBack->SetPosSize(0, 0, 640, 480);
    theRoot->AddChild(theBack);

    Font* theFont = theManager->GetDefaultFont();
    Font* theBold = theManager->GetDefaultFontMod(FontStyle_Bold, 10);

    Label* theTitle = new Label;
    theTitle->SetFont(theBold);
    theTitle->SetText("WONAPI GUI on the platform layer");
    theTitle->SetPosSize(16, 12, 400, 20);
    theRoot->AddChild(theTitle);

    Label* theLabel = new Label;
    theLabel->SetText("Player name:");
    theLabel->SetPosSize(16, 44, 90, 16);
    theRoot->AddChild(theLabel);

    MSInputBox* theInput = new MSInputBox;
    theInput->SetPosSize(110, 40, 200, 21);
    theInput->SetText("Caesar", false);
    theRoot->AddChild(theInput);

    MSButton* theOK = new MSButton("OK");
    theOK->SetPosSize(470, 440, 75, 23);
    theRoot->AddChild(theOK);
    MSButton* theCancel = new MSButton("Cancel");
    theCancel->SetPosSize(552, 440, 75, 23);
    theRoot->AddChild(theCancel);

    MSCheckbox* theCheck = new MSCheckbox;
    theCheck->SetText("Remember me", false);
    theCheck->SetPosSize(330, 42, 120, 16);
    theCheck->SetCheck(true);
    theRoot->AddChild(theCheck);
    MSRadioButton* theRadio = new MSRadioButton;
    theRadio->SetText("Direct connect", false);
    theRadio->SetPosSize(460, 42, 140, 16);
    theRadio->SetCheck(true);
    theRoot->AddChild(theRadio);

    ListArea* theList = new ListArea;
    theList->SetPosSize(16, 80, 220, 200);
    const char* theNames[] = {"Alexander", "Boudicca", "Charlemagne", "Darius", "Eleanor", "Frederick",
                              "Genghis", "Hannibal", "Isabella", "Julius", "Khufu", "Leonidas", "Montezuma",
                              "Nefertiti", "Octavian", "Pericles", "Qin Shi Huang", "Ramesses"};
    for (size_t i = 0; i < sizeof(theNames) / sizeof(theNames[0]); i++)
        theList->InsertString(theNames[i]);
    theRoot->AddChild(theList);

    TextArea* theText = new TextArea;
    theText->SetPosSize(250, 80, 374, 200);
    theText->SetTextFlags(TextFlag_WordWrap, true);
    theText->AddSegment("Welcome to the Empire Earth lobby. ");
    theText->AddSegment("This line is red, ", 0xC00000);
    theText->AddSegment("and this one wraps across the width of the text area to show word wrapping at work.");
    theText->LineBreak();
    theText->AddSegment("Underlined: ", 0);
    theText->AddSegment("www.example.com", 0x0000FF, theManager->GetDefaultFontMod(FontStyle_Underline));
    theText->LineBreak();
    theText->AddSegment("Accents: \xC3\xA9t\xC3\xA9, na\xC3\xAFve, \xE2\x82\xAC 100");   // UTF-8, the game's text (decision D6)
    theText->LineBreak();
    theRoot->AddChild(theText);

    MS3DFrame* theFrame = new MS3DFrame(2, false);
    theFrame->SetPosSize(16, 296, 608, 128);
    theRoot->AddChild(theFrame);
    Label* theInside = new Label;
    theInside->SetText("A sunken 3D frame (MS3DFrame) with a label inside it.");
    theInside->SetPosSize(28, 310, 500, 16);
    theRoot->AddChild(theInside);

    // a framed child window (a dialog in its own window), centred on the surface
    SmartPtr<Window> theDialog = CreateNativeWindow();
    theDialog->SetParent(theWindow.get());
    theDialog->SetTitle("Connect");
    theDialog->mStyle = 0x41;
    theDialog->Create(WONRectangle(0, 0, 220, 70));
    theDialog->mRoot->SetColorScheme(theManager->mColorScheme.get());
    MSDlgBack* theDialogBack = new MSDlgBack;
    theDialogBack->SetPosSize(0, 0, 220, 70);
    theDialog->AddComponent(theDialogBack);
    Label* theDialogText = new Label;
    theDialogText->SetText("Connecting to the host...");
    theDialogText->SetPosSize(12, 10, 200, 16);
    theDialog->AddComponent(theDialogText);
    MSButton* theAbort = new MSButton("Abort");
    theAbort->SetPosSize(72, 38, 75, 23);
    theDialog->AddComponent(theAbort);
    WONRectangle theDialogRect;
    theDialog->GetRect(theDialogRect, true);
    CHECK(theDialogRect.width == 220 && theDialogRect.height == 70);

    for (int i = 0; i < 3; i++) {
        theManager->ProcessEvents();
        theManager->PaintAndComposite();
    }
    const uint32_t* thePixels = NULL;
    int theWidth = 0, theHeight = 0, thePitch = 0;
    uint32_t theSerial = 0;
    bool theLocked = theManager->LockFrame(thePixels, theWidth, theHeight, thePitch, theSerial);
    CHECK(theLocked);
    if (theLocked) {
        CHECK(theWidth == 640 && theHeight == 480 && theSerial != 0);
        CHECK(WriteBMP(thePath, thePixels, theWidth, theHeight, thePitch));
        theManager->UnlockFrame();
        printf("wrote %s (%dx%d, serial %u)\n", thePath, theWidth, theHeight, theSerial);
    }
    theDialog->Close();
    theManager->ProcessEvents();
    theWindow->Close();
    theManager->ProcessEvents();
    printf("%d failed\n", sFailures);
    return sFailures;
}
