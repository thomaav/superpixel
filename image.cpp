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
		data[4*pixel.y*width + 4*pixel.x]     = 0xFF;
		data[4*pixel.y*width + 4*pixel.x + 1] = 0xFF;
		data[4*pixel.y*width + 4*pixel.x + 2] = 0xFF;
		data[4*pixel.y*width + 4*pixel.x + 3] = 0xFF;
	}
}

Color Image::color(int x, int y) const
{
	// Extend the image with the edge values.
	y = y >= height ? height - 1 : y;
	x = x >= width ? width - 1 : x;

	int32_t rgb = ((int32_t *) &data[0])[y*x + x];
	int32_t r = (rgb & RMASK) >> 24;
	int32_t g = (rgb & GMASK) >> 16;
	int32_t b = (rgb & BMASK) >> 8;

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
	int minGrad = INT_MAX;
	int minGradX, minGradY;

	for (int i = y - half; i <= y + half; ++i) {
		for (int j = x - half; j <= x + half; ++j) {
			if (i < 0 || j < 0 || i >= height || j >= width) {
				continue;
			}

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

std::vector<std::map<int32_t, Pixel>> Image::initClusters(int s) const
{
	std::vector<std::map<int32_t, Pixel>> clusters;

	for (double y = 0; y < height; y += s) {
		for (double x = 0; x < width; x += s) {
			std::map<int32_t, Pixel> cluster;
			Pixel center = minGradNeigh((int) x, (int) y, 3);
			cluster.insert(std::pair<int32_t, Pixel>(center.y*width + center.x,
													 center));
			clusters.push_back(cluster);
		}
	}

	return clusters;
}

void Image::SLIC()
{
	double n_sp = 300.0f;
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

	std::vector<std::map<int32_t, Pixel>> clusters = initClusters(s);
	std::vector<Pixel> centers;
	for (auto &cluster : clusters) {
		centers.push_back(cluster.begin()->second);
	}

	int half = s-1;
	size_t icenter = 0;
	for (auto &center : centers) {
		for (int y = center.y - half; y <= center.y + half; ++y) {
			for (int x = center.x - half; x <= center.x + half; ++x) {
				if (x < 0 || y < 0 || x >= height || y >= width)
					continue;

				if (x == center.x && y == center.y)
					continue;

				Pixel &pixel = pixels[y*width + x];
				int32_t prevl = pixel.l;
				double dist = center.dist(pixel, 1.0, s);

				if (dist < pixel.d) {
					pixel.d = dist;
					pixel.l = icenter;

					// Remove the pixel from its previous cluster.
					if (prevl != -1) {
						clusters[prevl].erase(pixel.y*width + pixel.x);
					}

					// Add the pixel to its new cluster.
					clusters[pixel.l].insert(std::pair<int32_t, Pixel>
											 (pixel.y*width + pixel.x, pixel));
				}
			}
		}

		++icenter;
	}

	std::vector<Pixel> visPix;
	for (auto &pixel : clusters[151]) {
		visPix.push_back(pixel.second);
	}
	visualizePixels(visPix, width, height);
}

void visualizePixels(std::vector<Pixel> &pixels, unsigned width, unsigned height)
{
	Image img = Image(width, height);
	img.setPixels(pixels);
	img.show();
}

void visualizePixels(std::vector<std::vector<Pixel>> &clusters,
					 unsigned width, unsigned height)
{
	Image img = Image(width, height);
	for (auto &pixels : clusters) {
		img.setPixels(pixels);
	}
	img.show();
}
