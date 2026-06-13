#include <iostream>

#include <SDL3/SDL_init.h>

#include <Core.h>

int main() {
	SDL_Init(SDL_INIT_VIDEO);
	std::cout << "SDL initialized\n";

	auto* window = SDL_CreateWindow("InstanceDemo", 1600, 1900, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

	lab::Core core;
	core.init("Instance Demo", window);

	core.shutdown();
	SDL_DestroyWindow(window); 
	SDL_Quit();
	std::cout << "SDL quitted\n";

	return 0;
}