#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define VULKAN_MEDIUM_IMPLEMENTATION
#include "vulkan_medium.h"

#include <iostream>

using namespace Moxaic;

int main(int argc, char* argv[])
{
    SetConsoleTextDefault();

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-node") == 0) {
            role = Role::Node;
        }
    }

    Core::Run();

    return 0;
}