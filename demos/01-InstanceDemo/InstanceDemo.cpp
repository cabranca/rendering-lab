#include <iostream>

#include <SDL3/SDL_init.h>

#include <Core.h>

int main() {
	SDL_Init(SDL_INIT_VIDEO);
	std::cout << "SDL initialized\n";

	lab::Core core;
	core.init("Instance Demo");

	core.shutdown();
	SDL_Quit();
	std::cout << "SDL quitted\n";

	return 0;
}