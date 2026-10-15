#pragma once
#include <cstdlib>
#include <iostream>
inline void TestCheck(bool passed, char const* expression, char const* file, int line)
{
    if (!passed)
    {
        std::cerr << file << ':' << line << ": FAIL: " << expression << '\n';
        std::exit(EXIT_FAILURE);
    }
}
#define CHECK(expression) TestCheck(static_cast<bool>(expression), #expression, __FILE__, __LINE__)
