#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"

using namespace Moxaic;

int main(int argc, char *argv[])
{
    for (int i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "-node") == 0) {
            g_Role = Role::Node;
        }
    }

    if (!CoreInit()) {
        MXC_LOG_ERROR("Core Init Fail!");;
    }

    CoreLoop();

    return 0;
}
