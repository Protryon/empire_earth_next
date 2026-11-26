// ConfigParser's methods only the tests/won drivers call (R6 WON lobby and network round 38: moved out of
// ee/wonapi/WONLobbyConfig.h unchanged). Included inside the class when EE_WON_CFGPARSER_DRIVER is defined; the
// saved parser position (ConfigParserState) is won_cfgparser_driver.h's.
public:
    static void TakeWide(void* c, const uint32_t* u, size_t n)
    { std::wstring* w = static_cast<std::wstring*>(c); w->erase(); for (size_t i = 0; i < n; i++) *w += (wchar_t)u[i]; }
    static void TakeState(void* c, const EeWonConfigState* s)
    {
        ConfigParserState* o = static_cast<ConfigParserState*>(c);
        o->mFilePos = s->pos; o->mFileNameAndPath.assign((const char*)s->path, s->path_len); o->mLineNum = s->line;
        o->mBackStr.erase(); for (size_t i = 0; i < s->back_len; i++) o->mBackStr += (wchar_t)s->back[i];
    }
    std::string Text(EeWonCfgpText w) const { std::string s; ee_won_cfgp_text(this->mObj, w, Take, &s); return s; }
    int Field(EeWonCfgpField f) const { return ee_won_cfgp_field(this->mObj, f); }
    bool Read(std::string* n, std::wstring* w, bool e, bool k)
    { return ee_won_cfgp_read_token(this->mObj, n ? 0 : (uint32_t)sizeof(wchar_t), e, k, Take, TakeWide, n ? (void*)n : (void*)w); }
    bool ReadToken(std::string& t, bool inToEndLine=false, bool inSkip=false) { return this->Read(&t, NULL, inToEndLine, inSkip); }
    bool ReadToken(std::wstring& t, bool inToEndLine=false, bool inSkip=false) { return this->Read(NULL, &t, inToEndLine, inSkip); }
    void SetParent(ConfigParser* p) { ee_won_cfgp_set_parent(this->mObj, p ? p->mObj : NULL); }
    void CopyWarnings(ConfigParser& s) { ee_won_cfgp_copy_warnings(this->mObj, s.mObj); }
    bool IsStopped() { return this->Field(EeWonCfgpField::Stopped) != 0; }
    void Stop() { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::Stopped, 1); }
    bool GetIgnorePathOnIncludeFile() { return this->Field(EeWonCfgpField::IgnorePath) != 0; }
    // the members the original's callers read: mAbortReason.empty(), mIsUnicode, mFilePath, mLineNum, ...
    bool HasNoError() const { return this->Field(EeWonCfgpField::NoError) != 0; }
    bool IsUnicode() const { return this->Field(EeWonCfgpField::Unicode) != 0; }
    int GetLineNum() const { return this->Field(EeWonCfgpField::Line); }
    bool IsFileOpen() const { return this->Field(EeWonCfgpField::Open) != 0; }
    bool GetWarningsAsErrors() const { return this->Field(EeWonCfgpField::WarningsAsErrors) != 0; }
    void SetIgnorePathOnIncludeFile(bool on) { ee_won_cfgp_set_field(this->mObj, EeWonCfgpField::IgnorePath, on); }
    std::string GetAbortText() const { return this->Text(EeWonCfgpText::Abort); } // as it is (no "Parse aborted.")
    std::string GetWarnings() const { return this->Text(EeWonCfgpText::Warnings); }
    std::string GetFilePath() const { return this->Text(EeWonCfgpText::FilePath); }
    std::string GetFileNameAndPath() const { return this->Text(EeWonCfgpText::FileName); }
    bool EndOfFile() { return ee_won_cfgp_end_of_file(this->mObj); }
    int GetChar() { return ee_won_cfgp_get_char(this->mObj); }
    int PeekChar() { return ee_won_cfgp_peek_char(this->mObj); }
    void UngetChar(int c) { ee_won_cfgp_unget_char(this->mObj, c); }
    void UngetStr(const std::string& s) { ee_won_cfgp_unget_narrow(this->mObj, B(s), s.length()); }
    void UngetStr(const std::wstring& s)
    { std::vector<uint32_t> u(s.begin(), s.end()); ee_won_cfgp_unget_wide(this->mObj, u.empty() ? NULL : &u[0], u.size()); }
    void SkipWhitespace() { ee_won_cfgp_skip_whitespace(this->mObj); }
    void Reset() { ee_won_cfgp_reset(this->mObj); }
    void PushDefineStack() { ee_won_cfgp_push_define_stack(this->mObj); }
    void PopDefineStack() { ee_won_cfgp_pop_define_stack(this->mObj); }
    bool GetDefine(const std::string& n, std::string& v)
    { std::string t; if (!ee_won_cfgp_get_define(this->mObj, B(n), n.length(), Take, &t)) return false; v = t; return true; }
    bool GetDefine(const std::string& n, std::wstring& v)
    { std::string t; if (!this->GetDefine(n, t)) return false; v = StringToWString(t); return true; }
    bool AddGroupDefine(const std::string& n, ConfigParserState* s)
    {
        if (s == NULL) return ee_won_cfgp_add_group_define(this->mObj, B(n), n.length(), NULL);
        std::vector<uint16_t> b(s->mBackStr.begin(), s->mBackStr.end());
        EeWonConfigState e = { s->mFilePos, B(s->mFileNameAndPath), s->mFileNameAndPath.length(), s->mLineNum,
                               b.empty() ? NULL : &b[0], b.size() };
        return ee_won_cfgp_add_group_define(this->mObj, B(n), n.length(), &e);
    }
    bool GetGroupDefine(const std::string& n, ConfigParserState& s)
    { return ee_won_cfgp_get_group_define(this->mObj, B(n), n.length(), TakeState, &s); }
    void GetState(ConfigParserState& s) { ee_won_cfgp_get_state(this->mObj, TakeState, &s); }
    bool SetState(const ConfigParserState& s)
    {
        std::vector<uint16_t> b(s.mBackStr.begin(), s.mBackStr.end());
        EeWonConfigState e = { s.mFilePos, B(s.mFileNameAndPath), s.mFileNameAndPath.length(), s.mLineNum,
                               b.empty() ? NULL : &b[0], b.size() };
        int32_t st = 0;
        bool r = ee_won_cfgp_set_state(this->mObj, &e, &st);
        FileReaderCheck(st);
        return r;
    }
