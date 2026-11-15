#ifdef _WIN32
#include <Windows.h>
extern "C" {
	__declspec(dllexport) DWORD NvOptimusEnablement = 1;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
#include <cstdlib>
#include "app.hpp"

int main()
{
    App app;

    if (!app.init())
        return EXIT_FAILURE;

    return app.run();
}

