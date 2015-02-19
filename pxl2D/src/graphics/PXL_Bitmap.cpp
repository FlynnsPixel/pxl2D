#include "PXL_Bitmap.h"
#include <iostream>
#include "PXL_ImageUtils.h"

PXL_Bitmap::PXL_Bitmap(std::string path) {
	buffer_loaded = false;
	load_bitmap(path);
	has_transparency = check_has_transparency();
}

PXL_Bitmap::PXL_Bitmap(int bitmap_width, int bitmap_height, PXL_Colour fill_colour) {
	buffer_loaded = true;
	width = bitmap_width;
	height = bitmap_height;
	pixels = new PXL_ubyte[(width * height) * 4];
	size = (width * height) * 4;
	fill(fill_colour);
	has_transparency = check_has_transparency();
}

PXL_Bitmap::PXL_Bitmap(int bitmap_width, int bitmap_height, PXL_ubyte* pixel_buffer) {
	buffer_loaded = true;
	width = bitmap_width;
	height = bitmap_height;
	pixels = pixel_buffer;
	size = (width * height) * 4;
	has_transparency = check_has_transparency();
}

void PXL_Bitmap::fill(PXL_Colour colour) {
	PXL_ubyte r = colour.r * 255;
	PXL_ubyte g = colour.g * 255;
	PXL_ubyte b = colour.b * 255;
	PXL_ubyte a = colour.a * 255;

	int row_size = width * 4;
	for (int y = 0; y < height; ++y) {
		int row_y = y * row_size;
		for (int x = 0; x < row_size; x += 4) {
			pixels[x + row_y] = r;
			pixels[x + 1 + row_y] = g;
			pixels[x + 2 + row_y] = b;
			pixels[x + 3 + row_y] = a;
		}
	}
}

bool PXL_Bitmap::check_has_transparency() {
	//checks whether the specified pixels contain any transparency
	if (pixels == NULL) {
		return false;
	}else {
		for (int n = 0; n < (width * height) * 4; n += 4) {
			/*if (pixels[n + 3] == 0) {
				pixels[n] = 255;
				pixels[n + 1] = 255;
				pixels[n + 2] = 255;
				pixels[n + 3] = 0;
				has_transparency = true;
			}*/
		}
	}
}

void PXL_Bitmap::load_bitmap(std::string path) {
	free();

	PXL_load_png(path, this);

	buffer_loaded = true;
}

void PXL_Bitmap::free() {
	if (buffer_loaded) {
		buffer_loaded = false;
		delete[] pixels;
	}
}

PXL_Bitmap::~PXL_Bitmap() {
	free();
}