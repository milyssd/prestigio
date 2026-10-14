#include <cassert>
#include <iostream>

void Addmod_prestigio_hollowScripts();
void AddprestigioScripts();
void Addprestigio_mainScripts();
void Addmod_prestigio_hollow_mainScripts();

namespace
{
unsigned registrations = 0;
}

// Stand in for runtime registration: this test links the real module loader.
void AddPrestigioHollowScripts()
{
    ++registrations;
}

int main()
{
    Addmod_prestigio_hollowScripts();
    assert(registrations == 1);
    AddprestigioScripts();
    assert(registrations == 2);
    Addprestigio_mainScripts();
    assert(registrations == 3);
    Addmod_prestigio_hollow_mainScripts();
    assert(registrations == 4);
    std::cout << "PASS: all four directory entry points link and register once\n";
}
