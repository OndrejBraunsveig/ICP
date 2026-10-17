#include <cstdlib>

#include "app.hpp"

int main()
{
    App app;

    if (!app.init())
        return EXIT_FAILURE;

    return app.run();
}