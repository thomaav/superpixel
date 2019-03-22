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

void Image::save(const char *fp) const
{
	unsigned err = lodepng::encode(fp, data, width, height);
	if (err) {
		std::cout << "[lodepng::decode error]: " << lodepng_error_text(err) << std::endl;
	}
}

void Image::setPixelColors(std::vector<Pixel> &pixels)
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

Color Image::getPixelColor(int x, int y) const
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
	double fd_x = (getPixelColor(x+1, y) - getPixelColor(x-1, y)).l2norm();
	double fd_y = (getPixelColor(x, y+1) - getPixelColor(x, y-1)).l2norm();

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
			center.color = getPixelColor(center.x, center.y);
			centers.push_back(center);
		}
	}

	return centers;
}

void Image::SLIC()
{
	double n_sp = 800.0f;
	double n_tp = height * width;
	int s = (int) sqrt(n_tp / n_sp);

	// Initialize samples for all pixels with label L(p) = -1 and
	// distance d(p) = -inf.
	std::vector<std::vector<Pixel>> pixels;
	for (size_t y = 0; y < height; ++y) {
		std::vector<Pixel> row;

		for (size_t x = 0; x < width; ++x) {
			Pixel pixel = Pixel(x, y);
			pixel.color = getPixelColor(x, y);
			row.push_back(pixel);
		}

		pixels.push_back(row);
	}

	// Centers should be according to the distance <s> between them.
	std::vector<Pixel> centers = initCenters(s);
	std::vector<int> centerCounts(centers.size());

	// SLIC lives in here.
	for (int i = 0; i < ITERATIONS; ++i) {
		println("Iteration " << i+1 << "/" << ITERATIONS);

		// Reset all distance values.
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				pixels[y][x].d = FLT_MAX;
			}
		}

		// Assign pixels to the center that is found to be the closest
		// (using color distance and euclidean distance).
		for (size_t icenter = 0; icenter < centers.size(); ++icenter) {
			Pixel &center = centers[icenter];
			for (int y = center.y - s; y <= center.y + s; ++y) {
				for (int x = center.x - s; x <= center.x + s; ++x) {
					if (x < 0 || y < 0 || x >= width || y >= height)
						continue;

					Pixel &pixel = pixels[y][x];
					double dist = center.dist(pixel, 40.0, s);

					if (dist < pixel.d) {
						pixel.d = dist;
						pixel.l = icenter;
					}
				}
			}
		}

		// Reset center values (as the value of the centers are only
		// determined by the clusters that were found above).
		for (size_t c = 0; c < centers.size(); ++c) {
			centers[c].color = Color(0, 0, 0);
			centers[c].x = 0.0;
			centers[c].y = 0.0;
			centerCounts[c] = 0;
		}

		// Find new center values by summing the rgb and xy values of
		// the clusters we found above.
		for (size_t y = 0; y < height; ++y) {
			for (size_t x = 0; x < width; ++x) {
				Pixel &pixel = pixels[y][x];
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

	// Enforcement of connectivity.
	const int dx[] = {-1, 0, 1, 0};
	const int dy[] = {0, -1, 0, 1};

	int newCenters[height][width];
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			newCenters[y][x] = -1;
		}
	}

	int label = 0;
	int neighborLabel = 0;
	const int limit = (height*width) / ((int) centers.size());

	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			if (newCenters[y][x] == -1) {
				std::vector<Pixel> segment;
				segment.push_back(Pixel(x, y));

				// Check 4-connected neighborhood to find the label
				// (cluster) of an adjacent pixel.
				for (size_t i = 0; i < 4; ++i) {
					int nx = segment[0].x + dx[i];
					int ny = segment[0].y + dy[i];

					if (nx < 0 || ny < 0 || nx >= width || ny >= height)
						continue;

					if (newCenters[ny][nx] >= 0) {
						neighborLabel = newCenters[ny][nx];
					}
				}

				int count = 1;
				for (int c = 0; c < count; ++c) {
					for (size_t i = 0; i < 4; ++i) {
						int nx = segment[c].x + dx[i];
						int ny = segment[c].y + dy[i];

						if (nx < 0 || ny < 0 || nx >= width || ny >= height)
							continue;

						if (newCenters[ny][nx] == -1 && pixels[ny][nx].l == pixels[y][x].l) {
							segment.push_back(Pixel(nx, ny));
							newCenters[ny][nx] = label;
							++count;
						}
					}
				}

				if (count <= limit >> 2) {
					--label;
					for (int c = 0; c < count; ++c) {
						newCenters[(int) segment[c].y][(int) segment[c].x] = neighborLabel;
					}
				}

				++label;
			}
		}
	}

	// for (size_t y = 0; y < height; ++y) {
	// 	for (size_t x = 0; x < height; ++x) {
	// 		pixels[y*width + x].l = newCenters[y][x];
	// 	}
	// }

	// visualizeAssignedCenters(pixels, width, height);

	// Superpixelate the image.
	for (size_t y = 0; y < height; ++y) {
		for (size_t x = 0; x < width; ++x) {
			// Pixel &center = centers.at(newCenters[y][x]);
			Pixel &center = centers.at(pixels[y][x].l);
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
	img.setPixelColors(pixels);
	img.show();
	exit(0);
}
