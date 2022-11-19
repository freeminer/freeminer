#pragma once
#include <string>

// A C++ function that will produce a stack trace with demangled function and method names.
std::string stacktrace(int skip = 1);
