#include <cstdlib>

#include "App.h"

int main()
{
    App app;

    try {
        if (!app.init())
            return EXIT_FAILURE;

        return app.run();
    }
    catch (...) {
        return EXIT_FAILURE;
    }
}
