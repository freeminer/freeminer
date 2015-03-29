#ifndef BASE64_HEADER
#define BASE64_HEADER

#include <string>

bool base64_is_valid(std::string const& s);
std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

#endif // BASE64_HEADER
