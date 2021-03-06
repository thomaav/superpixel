#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <tuple>
#include <climits>
#include <ostream>
#include <map>
#include <cfloat>

#define ITERATIONS 10

struct Color {
	double r;
	double g;
	double b;

	Color(double r, double g, double b)
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

	friend std::ostream &operator<<(std::ostream &os, Color const &color)
	{
		return os << "(" << color.r << "," << color.g << "," << color.b << ")";
	}
};

struct Pixel {
	Color color = Color(0, 0, 0);
	double x, y;
	int32_t l = -1;
	double d = FLT_MAX;

	Pixel(double x, double y)
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

	friend std::ostream &operator<<(std::ostream &os, Pixel const &px)
	{
		return os << "(" << px.x << "," << px.y << ")";
	}
};

struct Image {
	const char *fp;
	std::vector<unsigned char> data;
	unsigned width, height;

	Image(const char *fp);
	Image(unsigned width, unsigned height);
	~Image();

	void show() const;
	void save(const char *fp) const;
	void setPixelColors(std::vector<Pixel> &pixels);
	void setPixelsWhite(std::vector<Pixel> &pixels);
	Color getPixelColor(int x, int y) const;

	double gradient(int x, int y) const;
	Pixel minGradNeigh(int x, int y, int width) const;

	std::vector<Pixel> initCenters(int s) const;
	void SLIC();
};

void visualizePixels(std::vector<Pixel> &pixels, unsigned width, unsigned height);
void visualizeAssignedCenters(std::vector<Pixel> &pixels, unsigned width, unsigned height);
