#include "image.h"
#include "util.h"

#include <iostream>
#include <cstring>
#include <cmath>
#include <tuple>
#include <climits>
#include <algorithm>
#include <map>

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

Image::Image(unsigned width, unsigned height)
	: width(width), height(height)
{
	data.assign(height*width * 4, 0);
}

Image::~Image()
{
	;
}

void Image::show() const
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

void Image::setPixels(std::vector<Pixel> &pixels)
{
	for (auto &pixel: pixels) {
		int32_t x = (int) pixel.x;
		int32_t y = (int) pixel.y;
		data[4*y*width + 4*x]     = pixel.color.r;
		data[4*y*width + 4*x + 1] = pixel.color.g;
		data[4*y*width + 4*x + 2] = pixel.color.b;
	}
}

void Image::setPixelsWhite(std::vector<Pixel> &pixels)
{
	for (auto &pixel: pixels) {
		int32_t x = (int) pixel.x;
		int32_t y = (int) pixel.y;
		data[4*y*width + 4*x]     = 0xFF;
		data[4*y*width + 4*x + 1] = 0xFF;
		data[4*y*width + 4*x + 2] = 0xFF;
	}
}

Color Image::color(int x, int y) const
{
	// Extend the image with the edge values.
	y = y >= height ? height - 1 : y;
	x = x >= width ? width - 1 : x;

	int32_t r = data[4*y*width + 4*x];
	int32_t g = data[4*y*width + 4*x + 1];
	int32_t b = data[4*y*width + 4*x + 2];

	return Color(r, g, b);
}

double Image::gradient(int x, int y) const
{
	// Values beyond edges are extended to be edge values.
	double fd_x = (color(x+1, y) - color(x-1, y)).l2norm();
	double fd_y = (color(x, y+1) - color(x, y-1)).l2norm();

	return fd_x + fd_y;
}

Pixel Image::minGradNeigh(int x, int y, int kernelSize) const
{
	int half = kernelSize / 2;
	int minGradX, minGradY;
	double minGrad = FLT_MAX;

	for (int i = y - half; i <= y + half; ++i) {
		for (int j = x - half; j <= x + half; ++j) {
			if (i < 0 || j < 0 || i >= height || j >= width)
				continue;

			int grad = gradient(j, i);
			if (grad < minGrad) {
				minGrad = grad;
				minGradX = j;
				minGradY = i;
			}
		}
	}

	return Pixel(minGradX, minGradY);
}

std::vector<Pixel> Image::initCenters(int s) const
{
	std::vector<Pixel> centers;

	for (double y = 0; y < height; y += s) {
		for (double x = 0; x < width; x += s) {
			Pixel center = minGradNeigh((int) x, (int) y, 3);
			center.color = color(center.x, center.y);
			centers.push_back(center);
		}
	}

	return centers;
}

void Image::SLIC()
{
	double n_sp = 200.0f;
	double n_tp = height * width;
	int s = (int) sqrt(n_tp / n_sp);

	// Initialize samples for all pixels with label L(p) = -1 and
	// distance d(p) = -inf.
	std::vector<Pixel> pixels;
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			Pixel pixel = Pixel(x, y);
			pixel.color = color(x, y);
			pixels.push_back(pixel);
		}
	}

	// Centers should be according to the distance <s> between them.
	std::vector<Pixel> centers = initCenters(s);
	std::vector<int> centerCounts(centers.size());

	for (int i = 0; i < ITERATIONS; ++i) {
		println("Iteration " << i+1 << "/" << ITERATIONS);

		// Reset all distance values.
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				pixels[y*width + x].d = FLT_MAX;
			}
		}

		// Assign pixels to centers.
		int half = s-1;
		size_t icenter = 0;
		for (auto &center : centers) {
			for (int y = center.y - half; y <= center.y + half; ++y) {
				for (int x = center.x - half; x <= center.x + half; ++x) {
					if (x < 0 || y < 0 || x >= width || y >= height)
						continue;

					Pixel &pixel = pixels[y*width + x];
					double dist = center.dist(pixel, 15.0, s);

					if (dist < pixel.d) {
						pixel.d = dist;
						pixel.l = icenter;
					}
				}
			}

			++icenter;
		}

		// Recalculate centers.
		for (size_t c = 0; c < centers.size(); ++c) {
			centers[c].color = Color(0, 0, 0);
			centers[c].x = 0.0;
			centers[c].y = 0.0;
			centerCounts[c] = 0;
		}

		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				Pixel &pixel = pixels[y*width + x];

				if (pixel.l != -1) {
					centers[pixel.l].color = centers[pixel.l].color + pixel.color;
					centers[pixel.l].x += pixel.x;
					centers[pixel.l].y += pixel.y;
					centerCounts[pixel.l] += 1;
				}
			}
		}

		for (size_t c = 0; c < centers.size(); ++c) {
			centers[c].color.r /= centerCounts[c];
			centers[c].color.g /= centerCounts[c];
			centers[c].color.b /= centerCounts[c];
			centers[c].x /= centerCounts[c];
			centers[c].y /= centerCounts[c];
		}
	}

	// Superpixelate the image.
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			Pixel center = centers[pixels[y*width + x].l];
			data[4*y*width + 4*x] = (unsigned char) center.color.r;
			data[4*y*width + 4*x + 1] = (unsigned char) center.color.g;
			data[4*y*width + 4*x + 2] = (unsigned char) center.color.b;
		}
	}

	show();
}

void visualizePixels(std::vector<Pixel> &pixels, unsigned width, unsigned height)
{
	Image img = Image(width, height);
	img.setPixelsWhite(pixels);
	img.show();
}

void visualizeAssignedCenters(std::vector<Pixel> &pixels, unsigned width, unsigned height)
{
	for (auto &pixel : pixels) {
		pixel.color = Color(pixel.l, pixel.l, pixel.l);
	}

	Image img = Image(width, height);
	img.setPixels(pixels);
	img.show();
	exit(0);
}
