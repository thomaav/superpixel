#include <iostream>

#include "vendor/lodepng.h"
#include "image.h"

int main(int argc, char *argv[])
{
	Image img = Image("img/pingpong.png");
	img.show();

	return 0;
}
