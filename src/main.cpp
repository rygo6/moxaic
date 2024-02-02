#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"

#include <iostream>

using namespace Moxaic;

int main(int argc, char* argv[])
{
    SetConsoleTextDefault();

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-node") == 0) {
            Role = Role::Node;
        }
    }

    Core::Run();

    return 0;
}