#pragma once

#include <glm/glm.hpp>

struct SDL_Window;

namespace lab {

    // Owns the SDL window. SDL itself is initialized by VulkanContext (it must be
    // up before instance creation), so the window is created afterwards.
    // Input polling stays in the demo since it is experiment-specific.
    class Window {
        public:
        void init(const char* title, int width, int height);
        void shutdown();

        SDL_Window* getHandle() const { return m_Window; }
        glm::ivec2 getSize() const;

        private:
        SDL_Window* m_Window{ nullptr };
    };
}
