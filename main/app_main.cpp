#include "App.hpp"

extern "C" void app_main() // NOLINT(readability-identifier-naming): IDF entry point
{
    static banana::App app;
    app.run();
}
