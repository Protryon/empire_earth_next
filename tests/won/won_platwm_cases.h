// won_platwm_test.cpp's windows' methods (PlatGUIWindow's, as the real ones behave) and its runner, included once in
// each namespace after the manager's statements.
static PlatWindowManager* sManager;
PlatWindowManager* PlatGUIWindow::GetPlatManager() { return sManager; }
void PlatGUIWindow::FollowSurface()
{
    tr("w" + I(mId) + " FollowSurface");
    if (!this->FillsSurface() || sManager == NULL || this->GetWindowState() != WindowState_Open) return;
    int w = sManager->GetSurfaceWidth(), h = sManager->GetSurfaceHeight();
    if (w == this->mWidth && h == this->mHeight) return;
    this->mX = this->mY = 0; this->mWidth = w; this->mHeight = h;
    this->QueueEvent(new SizeEvent(-1, -1, w, h));
    sManager->SetDirty();
}
void PlatGUIWindow::FollowPixelScale() { tr("w" + I(mId) + " FollowPixelScale " + I((long long)(sManager->GetPixelScale() * 100))); sManager->SetDirty(); }
void PlatGUIWindow::OnPaint()
{
    tr("w" + I(mId) + " OnPaint");
    if (!this->mInvalid) return;
    this->mInvalid = false;
    sManager->SetDirty();
}
void PlatGUIWindow::QueueMouse(int inType, int inSurfaceX, int inSurfaceY, int inMod, int inButtonOrAmount)
{
    WONRectangle theClient;
    this->GetClientRect(theClient);
    this->QueueEvent(new MouseEvent(inType, inSurfaceX - theClient.x, inSurfaceY - theClient.y, inMod, inButtonOrAmount));
}
void PlatGUIWindow::SetPosSize(int x, int y, int width, int height)
{
    tr("w" + I(mId) + " SetPosSize " + I(x) + "," + I(y) + "," + I(width) + "," + I(height));
    if (this->GetWindowState() != WindowState_Open || this->FillsSurface()) return;
    if (width == -1) width = this->mWidth;
    if (height == -1) height = this->mHeight;
    WONRectangle o; this->GetClientRect(o);
    this->mX = x; this->mY = y; this->mWidth = width; this->mHeight = height;
    WONRectangle n; this->GetClientRect(n);
    if (n.width != o.width || n.height != o.height) this->QueueEvent(new SizeEvent(-1, -1, n.width, n.height));
    if (n.x != o.x || n.y != o.y) this->QueueEvent(new SizeEvent(n.x, n.y, -1, -1));
    sManager->SetDirty();
}
void PlatGUIWindow::PlatformSetFocus()
{
    tr("w" + I(mId) + " PlatformSetFocus");
    if (sManager == NULL || this->GetWindowState() != WindowState_Open) return;
    if (!this->IsPopup()) sManager->BringToFront(this);
    sManager->SetFocusWindow(this);
}
void PlatGUIWindow::Close()
{
    tr("w" + I(mId) + " Close");
    this->mState = WindowState_Closed;
    this->mVisible = false;
    if (this->mRegistered) sManager->RemovePlatWindow(this);
}

static unsigned long long sRng;
static unsigned R(unsigned n) { sRng = sRng * 6364136223846793005ull + 1442695040888963407ull; return n ? (unsigned)((sRng >> 33) % n) : 0; }
static int Rg(int lo, int hi) { return lo + (int)R((unsigned)(hi - lo + 1)); }
static uint32_t Next(uint32_t& s) { s = s * 1103515245u + 12345u; return s ^ (s >> 15); }
static uint32_t Fnv(const void* d, size_t n) { uint32_t h = 2166136261u; const unsigned char* p = (const unsigned char*)d; for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; } return h; }
static const float kScales[] = {1.0f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 4.0f, 0.5f, 7.0f};

static std::string Snapshot(PlatWindowManager* m, std::vector<SmartPtr<PlatGUIWindow> >& ws)
{
    std::string s = "focus " + I(m->GetFocusWindow() ? m->GetFocusWindow()->mId : -1) + " capture " +
                    I(m->GetCaptureWindow() ? m->GetCaptureWindow()->mId : -1) + " surface " + I(m->GetSurfaceWidth()) + "x" +
                    I(m->GetSurfaceHeight()) + " scale " + I((long long)(m->GetPixelScale() * 100));
    int x = 0, y = 0;
    bool have = m->GetMousePosition(x, y);
    s += " mouse " + I(have) + " " + I(x) + "," + I(y);
    for (size_t i = 0; i < ws.size(); i++) {
        PlatGUIWindow* w = ws[i].get();
        s += " [" + I(w->mX) + "," + I(w->mY) + "," + I(w->mWidth) + "," + I(w->mHeight) + " v" + I(w->mVisible) + " r" + I(w->mRegistered) + " s" + I(w->mState) + "]";
    }
    const uint32_t* p; int fw, fh, pitch, lw, lh; uint32_t serial;
    if (m->LockFrame(p, fw, fh, pitch, lw, lh, serial)) {
        s += " frame " + I(fw) + "x" + I(fh) + " " + I(pitch) + " " + I(lw) + "x" + I(lh) + " #" + I(serial) + " " + I(Fnv(p, (size_t)pitch * fh));
        m->UnlockFrame();
    } else
        s += " noframe";
    return s;
}

static void Forward(const PlatInputEvent& e) { tr("forward " + I(e.mType)); }

static PlatInputEvent RandomEvent(std::vector<SmartPtr<PlatGUIWindow> >&)
{
    static const uint32_t kKeys[] = {0x08, 0x09, 0x0D, 0x1B, 0x12, 0x79, 'A', 'M', 'Z', '0', 0x25, 0, 0x70};
    static const char* kTexts[] = {"a", "hello", "\xc3\xa9t\xc3\xa9", "\xf0\x9f\x98\x80", "", "\xff\xfe", "0123456789012345678901234567890123456789012345678901234567890123"};
    PlatInputEvent e;
    memset(&e, 0, sizeof e);
    e.mType = (uint32_t)Rg(0, 18);
    e.mWindow = R(5) ? (PlatWindow*)(intptr_t)Rg(1, 2) : NULL;
    if (e.mType == PLAT_EVENT_KEY_DOWN || e.mType == PLAT_EVENT_KEY_UP) {
        e.mKey.mVirtualKey = kKeys[R(13)];
        e.mKey.mModifiers = R(3) ? 0 : (uint32_t)Rg(0, 15);
    } else if (e.mType == PLAT_EVENT_TEXT) {
        const char* t = kTexts[R(7)];
        memcpy(e.mText.mText, t, strlen(t) < 63 ? strlen(t) : 63);
    } else {
        e.mMouse.mX = Rg(-20, 400); e.mMouse.mY = Rg(-20, 300);
        e.mMouse.mButton = (uint32_t)Rg(0, 5); e.mMouse.mClicks = (uint32_t)Rg(0, 4);
        e.mMouse.mWheel = (float)Rg(-30, 30) / 10.0f + (R(4) == 0 ? 0.05f : 0.0f);
        e.mMouse.mModifiers = (uint32_t)Rg(0, 15);
    }
    return e;
}

// One world from a seed: windows, the manager, 60 random operations; the trace with a snapshot after each
static std::string Run(unsigned long long seed, int& outOps)
{
    sRng = seed; sTrace.clear(); sEvents.clear(); sCurrentThread = 1; WindowManager::ThreadId() = 1;
    sClientW = Rg(0, 500); sClientH = Rg(0, 400); if (R(8) == 0) sClientW = 0;
    SmartPtr<PlatWindowManager> m = new PlatWindowManager;
    sManager = m.get();
    m->SetEventForward(R(4) ? Forward : NULL);
    // a small surface (the frame is hashed after every operation)
    m->SetSurfaceSize(Rg(20, 240), Rg(20, 180));
    std::vector<SmartPtr<PlatGUIWindow> > ws;
    int n = Rg(1, 6);
    for (int i = 0; i < n; i++) {
        PlatGUIWindow* w = new PlatGUIWindow;
        w->mId = i;
        w->mParent = i > 0 && R(4) ? ws[R((unsigned)i)].get() : NULL;
        w->mStyle = R(3) == 0 ? 0x20 : 0;
        w->mPopup = R(5) == 0; w->mTopMost = R(6) == 0; w->mNotModal = R(3) == 0; w->mInvalid = R(2) != 0;
        w->mVisible = R(4) != 0; w->mMinimizeOnAltTab = R(2) != 0; w->mForwardKeyMsgToParent = R(3) == 0;
        w->mX = Rg(-30, 220); w->mY = Rg(-30, 160); w->mWidth = Rg(0, 160); w->mHeight = Rg(0, 120);
        int tl = (int)R(12);
        for (int k = 0; k < tl; k++) w->mTitle += (wchar_t)(R(5) ? Rg('A', 'z') : Rg(0xA0, 0x400));
        if (R(5)) {
            PlatNativeImage* img = new PlatNativeImage;
            img->mSurface = SurfaceBase::GetNewSurface(WONAPI::SurfaceFormat_32);
            int iw = Rg(0, 170), ih = Rg(0, 130);
            img->mSurface->Create(iw, ih);
            uint32_t s = (uint32_t)R(0x7fffffff);
            uint32_t* px = (uint32_t*)img->mSurface->GetPixelsBase();
            for (int k = 0; k < img->mSurface->GetPixelsPerRow() * ih; k++) px[k] = Next(s);
            w->mOffscreenImage = img;
        }
        ws.push_back(w);
        m->mAll.push_back(w);
        if (R(5)) m->AddPlatWindow(w);
    }
    std::string out = Snapshot(m, ws) + "\n";
    outOps = 60;
    for (int op = 0; op < 60; op++) {
        PlatGUIWindow* w = ws[R((unsigned)ws.size())].get();
        int k = (int)R(22);
        if (getenv("PLATWM_DEBUG")) fprintf(stderr, "op %d kind %d w%d\n", op, k, w->mId);
        sTrace.clear();
        std::string what;
        switch (k) {
        case 0: m->AddPlatWindow(w); what = "add"; break;
        case 1: if (w->mRegistered) { m->RemovePlatWindow(w); } what = "remove"; break;
        case 2: m->BringToFront(w); what = "front"; break;
        case 3: m->SetFocusWindow(R(4) ? w : NULL); what = "focus"; break;
        case 4: m->SetCaptureWindow(R(2) ? w : NULL); what = "capture"; break;
        case 5: { PlatGUIWindow* h = m->WindowFromPoint(Rg(-20, 300), Rg(-20, 220)); what = "hit " + I(h ? h->mId : -1); break; }
        case 6: case 7: case 8: case 9: case 10: { PlatInputEvent e = RandomEvent(ws); sModifiers = (uint32_t)Rg(0, 15); m->HandlePlatEvent(e); what = "event " + I(e.mType); break; }
        case 11: { if (R(3) == 0) sCurrentThread = 2; m->SetSurfaceSize(Rg(-10, 300), Rg(-10, 220)); sCurrentThread = 1; what = "size"; break; }
        case 12: { if (R(3) == 0) sCurrentThread = 2; m->SetPixelScale(kScales[R(9)]); sCurrentThread = 1; what = "scale"; break; }
        case 13: m->SetPresentRect(R(3) ? (float)Rg(0, 50) / 100.0f : 0.0f, (float)Rg(0, 50) / 100.0f, R(4) ? (float)Rg(0, 100) / 100.0f : 0.0f, (float)Rg(1, 100) / 100.0f); what = "present"; break;
        case 14: case 15: { if (R(2)) sEvents.push_back(RandomEvent(ws)); m->GetNextEvent(); what = "next"; break; }
        case 16: m->PaintAndComposite(); what = "paint"; break;
        case 17: m->GetSystemColorScheme(); what = "scheme"; break;
        case 18: { WONRectangle r; m->GetScreenRect(r); int a, b; m->GetScreenResolutionHook(a, b); what = "screen " + I(r.width) + "x" + I(r.height) + " " + I(a) + "x" + I(b); break; }
        case 19: if (R(2)) sCurrentThread = 2; m->Signal(); sCurrentThread = 1; what = "signal"; break;
        case 20: w->mVisible = !w->mVisible; w->mInvalid = true; what = "toggle"; break;
        case 21: if (R(4) == 0 && w->mState == Window::WindowState_Open) { w->Close(); } else { w->mTitle += L"x"; w->mInvalid = true; } what = "close/title"; break;
        }
        out += "op " + what + "\n" + sTrace + Snapshot(m, ws) + "\n";
    }
    sTrace.clear();
    m = NULL;   // the destructor closes the windows: each leaves the manager
    sManager = NULL;
    out += "end\n" + sTrace;
    return out;
}
