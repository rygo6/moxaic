#include "main.hpp"
#include "moxaic_core.hpp"

bool Moxaic::g_ApplicationRunning = true;
Moxaic::Role Moxaic::g_Role = Compositor;

int main(int argc, char *argv[])
{
    Moxiac::CoreInit();
    Moxiac::CoreLoop();

    return 0;
}
