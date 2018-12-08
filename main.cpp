#include <iostream>
#include <tuple>

#include "vendor/lodepng.h"
#include "image.h"

int main(int argc, char *argv[])
{
	Image img = Image("img/pingpong.png");
	img.SLIC();

	return 0;
}
