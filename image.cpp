#include "image.h"

#include <iostream>
#include <cstring>

#include <SDL/SDL.h>

#include "vendor/lodepng.h"

Image::Image(const char *fp)
	: fp(fp)
{
	unsigned err = lodepng::decode(data, width, height, fp);
	if (err) {
		// Exception?
		std::cout << "[lodepng::decode error]: " << lodepng_error_text(err) << std::endl;
	}
}

Image::~Image()
{
	;
}

void Image::show()
{
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::cout << "[SDL_Init error]: Init video failed" << std::endl;
		return;
	}

	SDL_Surface *scr = SDL_SetVideoMode(width, height, 32, SDL_HWSURFACE);
	if (!scr) {
		std::cout << "[SDL_SetVideoMode error]: No SDL screen" << std::endl;
		return;
	}

	// Convert from RGBA of PNG to expected ARGB of SDL.
	unsigned int *pixel = (unsigned int *) scr->pixels;
	for (size_t i = 0; i < height * width; ++i) {
		unsigned int r = data[4*i + 0];
		unsigned int g = data[4*i + 1];
		unsigned int b = data[4*i + 2];

		*pixel = 65536*r + 256*g + b;
		++pixel;
	}

	SDL_Event event;
	int run = 1;
	while (run) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) run = 0;
			else if (SDL_GetKeyState(NULL)[SDLK_ESCAPE]) run = 0;
		}

		SDL_UpdateRect(scr, 0, 0, 0, 0);
		SDL_Delay(5);
	}

	SDL_Quit();
}
