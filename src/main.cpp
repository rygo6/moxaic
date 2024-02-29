#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "moxaic_renderer.hpp"

using namespace Moxaic;

int main(int argc, char* argv[])
{
    Renderer renderer;
    renderer.Init();

    return 0;
    SetConsoleTextDefault();

    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-node") == 0) {
            role = Role::Node;
        }
    }

    Core::Run();

    return 0;
}