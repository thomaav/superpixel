#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <tuple>
#include <climits>

#define RMASK 4278190080
#define GMASK 16711680
#define BMASK 65280

struct Color {
	int32_t r;
	int32_t g;
	int32_t b;

	Color(int32_t r, int32_t g, int32_t b)
		: r(r), g(g), b(b) {}

	double l2norm() const
	{
		return sqrt(pow(r, 2) + pow(g, 2) + pow(b, 2));
	}

	Color operator+(const Color &rhs) const
	{
		return Color(r + rhs.r, g + rhs.g, b + rhs.b);
	}

	Color operator-(const Color &rhs) const
	{
		return Color(r - rhs.r, g - rhs.g, b - rhs.b);
	}
};

struct Pixel {
	Color color = Color(0, 0, 0);
	int32_t x;
	int32_t y;
	int32_t l = -1;
	int32_t d = INT_MAX;

	Pixel(int32_t x, int32_t y)
		: x(x), y(y) {};

	bool operator==(const Pixel &rhs) const
	{
		return (x == rhs.x && y == rhs.y);
	}

	double colorDist(Pixel &rhs) const
	{
		return sqrt(pow(color.r - rhs.color.r, 2) +
			    pow(color.g - rhs.color.g, 2) +
			    pow(color.b - rhs.color.b, 2));
	}

	double euclidDist(Pixel &rhs) const
	{
		return sqrt(pow(x - rhs.x, 2) +
			    pow(y - rhs.y, 2));
	}

	double dist(Pixel &rhs, double c, double s) const
	{
		double dc = colorDist(rhs);
		double ds = euclidDist(rhs);

		return sqrt(pow(dc, 2) + pow(ds/2, 2)*pow(c, 2));
	}
};

struct Image {
	const char *fp;
	std::vector<unsigned char> data;
	unsigned width, height;

	Image(const char *fp);
	~Image();

	void show() const;
	Color color(int x, int y) const;
	double gradient(int x, int y) const;
	Pixel minGradNeigh(int x, int y, int width) const;
	std::vector<std::vector<Pixel>> initClusters(int s) const;
	void SLIC();
};


