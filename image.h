#include <vector>

#pragma once

struct Image {
	const char *fp;
	std::vector<unsigned char> data;
	unsigned width, height;

	Image(const char *fp);
	~Image();

	void show();
};


