#pragma once

#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <glm/glm.hpp>
#include <functional>
#include <algorithm>
#include <any>

namespace Moxaic
{
    enum class Phase : uint8_t
    {
        Pressed = 0,
        Released = 1,
    };

    enum class Button : uint8_t
    {
        None = 0,
        Left = 1,
        Middle = 2,
        Right = 3,
    };

    struct MouseMotionEvent
    {
        glm::vec2 delta;
    };

    struct MouseEvent
    {
        Phase phase;
        Button button;
    };

    struct KeyEvent
    {
        Phase phase;
        SDL_Keycode key;
    };

    bool WindowInit();
    void WindowPoll();
    void WindowShutdown();

    extern VkExtent2D g_WindowDimensions;
    extern SDL_Window *g_pSDLWindow;

    using MouseMotionCallback = std::function<void(const MouseMotionEvent &)>;
    using MouseCallback = std::function<void(const MouseEvent &)>;
    using KeyCallback = std::function<void(const KeyEvent &)>;

    extern std::vector<MouseMotionCallback *> g_MouseMotionSubscribers;
    extern std::vector<MouseCallback *> g_MouseSubscribers;
    extern std::vector<KeyCallback *> g_KeySubscribers;

    template<typename T>
    void Unsubscribe(std::vector<T *> &vector, const T &item)
    {
        vector.erase(std::remove(vector.begin(),
                                 vector.end(),
                                 &item),
                     vector.end());
    }

// I might hate this... why did they give us templates
    template<typename T>
    class MouseMotionReceiver
    {
    protected:
        MouseMotionReceiver() { g_MouseMotionSubscribers.push_back(&m_MouseMotionBinding); }
        ~MouseMotionReceiver() { Unsubscribe(g_MouseMotionSubscribers, m_MouseMotionBinding); }
        MouseMotionCallback m_MouseMotionBinding{[this](const MouseMotionEvent &event) {
            static_cast<T *>(this)->OnMouseMove(event);
        }};
    };

    template<typename T>
    class MouseReceiver
    {
    protected:
        MouseReceiver() { g_MouseSubscribers.push_back(&m_MouseBinding); }
        ~MouseReceiver() { Unsubscribe(g_MouseSubscribers, m_MouseBinding); }
        MouseCallback m_MouseBinding{[this](const MouseEvent &event) {
            static_cast<T *>(this)->OnMouse(event);
        }};
    };

    template<typename T>
    class KeyReceiver
    {
    protected:
        KeyReceiver() { g_KeySubscribers.push_back(&m_KeyBinding); }
        ~KeyReceiver() { Unsubscribe(g_KeySubscribers, m_KeyBinding); }
        KeyCallback m_KeyBinding{[this](const KeyEvent &event) {
            static_cast<T *>(this)->OnKey(event);
        }};
    };
}