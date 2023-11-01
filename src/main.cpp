#include "main.hpp"
#include "moxaic_core.hpp"
#include "moxaic_logging.hpp"

bool Moxaic::g_ApplicationRunning = true;
Moxaic::Role Moxaic::g_Role = Compositor;

int main(int argc, char *argv[])
{
    if (!Moxaic::CoreInit()) {
        MXC_LOG_ERROR("Core Init Fail!");;
    }

    Moxaic::CoreLoop();

    return 0;
}
