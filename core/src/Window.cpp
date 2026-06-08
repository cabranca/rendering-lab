#include "Window.h"

#include <SDL3/SDL.h>

namespace lab {

	void Window::init(const char* title, int width, int height) {
		m_Window = SDL_CreateWindow(title, width, height, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	}

	glm::ivec2 Window::getSize() const {
		glm::ivec2 size{};
		SDL_GetWindowSize(m_Window, &size.x, &size.y);
		return size;
	}

	void Window::shutdown() {
		SDL_DestroyWindow(m_Window);
	}
} // namespace lab
