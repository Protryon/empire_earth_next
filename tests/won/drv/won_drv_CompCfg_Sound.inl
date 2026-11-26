// WONCompCfg.inl's sound descriptor cases, for the drivers' world (the game reads its sounds in Rust: soundobj.rs,
// R6 WON GUI round 41)
    case O::SoundDescNew: return (intptr_t)new SoundDescriptor;
    case O::SoundDescFlags: ((SoundDescriptor*)c)->SetSoundFlags((int)a, true); return 0;
    case O::SoundDescPath: ((SoundDescriptor*)c)->mPath = CcStr(a, b); return 0;
    case O::SoundDecode: return CC_G(CcGive(WindowManager::GetDefaultWindowManager()->DecodeSound(*(SoundDescriptor*)c)));
    case O::SoundDescFree: delete (SoundDescriptor*)c; return 0;
