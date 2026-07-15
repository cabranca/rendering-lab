#include "Window.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_vulkan.h"
#include <print>

namespace lab {

    void Window::init(std::string_view name, int width, int height, SDL_WindowFlags flags) {
        SDL_Init(SDL_INIT_VIDEO);
        if ((flags | SDL_WINDOW_VULKAN) == SDL_WINDOW_VULKAN) {
            SDL_Vulkan_LoadLibrary(nullptr);
        }

        m_Window = SDL_CreateWindow(name.data(), width, height, flags);
        if (!m_Window) {
            std::println("Window::init - Failed to create SDL Window");
            return;
        }

        m_Width = width;
        m_Height = height;
    }

    void Window::shutdown() {
        SDL_DestroyWindow(m_Window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    SDL_Window* Window::getWindowHandle() const {
        return m_Window;
    }

    std::tuple<int, int> Window::getWindowSize() const {
        return {m_Width, m_Height};
    }
}