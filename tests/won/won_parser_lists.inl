// StringParser's list reads and writes, which only the tests/won drivers call (R6 WON lobby and network round 38:
// moved out of ee/wonapi/WONLobbyParser.h unchanged). Included inside the class when EE_WON_STRINGPARSER_DRIVER is
// defined before the game headers.
public:
    // comma-separated lists; on a failed element the ones read so far stay
    bool ReadValue(std::list<int>& theList) { return ReadList(this, theList); }
    bool ReadValue(std::list<std::string>& theList) { return ReadStrings(theList); }
    bool ReadValue(std::list<std::wstring>& theList) { return ReadStrings(theList); }
    // "(1, 2), (3)": on a failed element or a missing ')' the groups read so far stay
    bool ReadValue(std::list<std::list<int> >& theList)
    {
        while (this->CheckNextChar('(')) {
            theList.push_back(std::list<int>());
            if (!ReadList(this, theList.back()) || !this->CheckNextChar(')')) return false;
            if (!this->CheckNextChar(',')) break;
        }
        return true;
    }
    template<class T> static void ValueToString(const std::list<T>& theList, std::string& theStr) // ", " between
    {
        for (typename std::list<T>::const_iterator i = theList.begin(); i != theList.end();) {
            StringParser::ValueToString(*i, theStr);
            if (++i != theList.end()) theStr += ", ";
        }
    }
    static void ValueToString(const std::list<std::list<int> >& theList, std::string& theStr) // "(a, b), (c)"
    {
        for (std::list<std::list<int> >::const_iterator i = theList.begin(); i != theList.end();) {
            theStr += "(";
            ValueToString(*i, theStr);
            theStr += ")";
            if (++i != theList.end()) theStr += ", ";
        }
    }
private:
    template<class T> static bool ReadList(StringParser* inParser, std::list<T>& theList)
    {
        while (true) {
            theList.push_back(T());
            if (!inParser->ReadValue(theList.back())) { theList.pop_back(); return false; }
            if (!inParser->CheckNextChar(',')) return true;
        }
    }
    template<class S> bool ReadStrings(std::list<S>& theList) // a string read never fails
    {
        do { theList.push_back(S()); this->ReadString(theList.back(), false); } while (this->CheckNextChar(','));
        return true;
    }
