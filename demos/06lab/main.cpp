#include <cstdlib>

#include "App.h"

int main()
{
    App app;

    if (!app.init())
        return EXIT_FAILURE;

    return app.run();
}