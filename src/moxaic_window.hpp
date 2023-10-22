#pragma once

#include <SDL2/SDL.h>

namespace Moxaic {

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    extern SDL_Window *g_pSDLWindow;
}