// ArithmeticException (0x18 bytes): WON SDK class with the original's RTTI, throw and dtor
// Only the arithmetic/crypto implementation files include this type.
#ifndef EE_WONAPI_WONARITHMETICEXCEPTION_H
#define EE_WONAPI_WONARITHMETICEXCEPTION_H
#include <exception>
#include <string>
namespace WONAPI {
class ArithmeticException : public std::exception {
public:
    std::string what; //08..18, message; intentionally hides inherited what()
    ArithmeticException() : exception() {}
    ArithmeticException(const std::string& message) { this->what=message; }
};
}
#endif
