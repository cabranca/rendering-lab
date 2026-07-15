#pragma once

#include <string_view>

#include <SDL3/SDL.h>

namespace lab {

	class Window {
	  public:
		void init(std::string_view name, int width, int height, SDL_WindowFlags flags);
		void shutdown();

		[[nodiscard]] SDL_Window* getWindowHandle() const;
		[[nodiscard]] std::tuple<int, int> getWindowSize() const;

	  private:
		SDL_Window* m_Window{ nullptr };
		int m_Width{ 0 };
		int m_Height{ 0 };
	};
} // namespace lab