// the members only the drivers use, cut from ee/wonapi/WONLobbyParser.h's StringParser (R6 WON GUI round 40):
// included in its class body when EE_WON_DRIVER is defined (tools/xbuild.py build_won_test)
    bool StartsWith(const char* theStr) { return ee_won_parser_starts_with(this->Text(), this->Unit(), &this->mPos, theStr); }
    void SkipNonWhitespace() { ee_won_parser_skip_non_whitespace(this->Text(), this->Unit(), &this->mPos); }
    const wchar_t* GetWStr() { return mWideText; }
    int GetPos() { return this->mPos; }
    void SetIsWString(bool isWString) { this->mWide=isWString; }
    bool GetIsWString() { return this->mWide; }
    // wide characters above 255 read as 0xff
    unsigned char GetChar(int inOffset) { return ee_won_parser_get_char(this->Text(), this->Unit(), this->mPos, inOffset); } //2a48c0
    wchar_t GetWChar(int inOffset) { return (wchar_t)ee_won_parser_get_wchar(this->Text(), this->Unit(), this->mPos, inOffset); } //2a4910
    void IncrementPos(int inAmount) { this->mPos += inAmount; } //2a4900
    void SkipWhitespace() { ee_won_parser_skip_whitespace(this->Text(), this->Unit(), &this->mPos); } //2a4970
    // whitespace stays consumed on a mismatch
    bool CheckNextChar(int inChar) { return ee_won_parser_check_next_char(this->Text(), this->Unit(), &this->mPos, inChar); } //2a49b0
    static void ValueToString(const bool& v, std::string& s) { ee_won_parser_format_bool(v, AppendBytes, &s); } //2a5820 True/False
    static void ValueToString(const int& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); } //2a5a70
    static void ValueToString(const unsigned __int64& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const __int64& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned long& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const long& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const short& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const char& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned char& v, std::string& s) { ee_won_parser_format_u64(v, AppendBytes, &s); }
    static void ValueToString(const unsigned short& v, std::string& s) { ee_won_parser_format_i64(v, AppendBytes, &s); } //2a5c40
    static void ValueToString(const std::string& v, std::string& s) { s += v; } //2a5c60
    static void ValueToString(const std::wstring& v, std::string& s) { s += WStringToString(v); } //2a5d70
