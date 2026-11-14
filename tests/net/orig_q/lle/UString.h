// ne_queues_test stand-in: a thread's name
#pragma once
#include <string>
class UString {
public:
    UString(const char* inText) : mText(inText) {}
    std::string mText;
};
